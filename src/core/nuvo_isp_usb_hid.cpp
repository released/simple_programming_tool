#include "nuvo_isp_usb_hid.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <hidsdi.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <thread>

#include "../hid/hid_scan.h"

namespace mfc_tool::core {
namespace {

constexpr std::uint32_t CMD_UPDATE_APROM = 0x000000A0u;
constexpr std::uint32_t CMD_READ_CONFIG = 0x000000A2u;
constexpr std::uint32_t CMD_SYNC_PACKNO = 0x000000A4u;
constexpr std::uint32_t CMD_GET_FWVER = 0x000000A6u;
constexpr std::uint32_t CMD_RUN_APROM = 0x000000ABu;
constexpr std::uint32_t CMD_RESET = 0x000000ADu;
constexpr std::uint32_t CMD_CONNECT = 0x000000AEu;
constexpr std::uint32_t CMD_GET_DEVICEID = 0x000000B1u;
constexpr std::uint32_t CMD_RESEND_PACKET = 0x000000FFu;
constexpr std::uint16_t kNuvotonVid = 0x0416u;
constexpr std::uint16_t kNuvotonUsbIspPid = 0x3F00u;
constexpr std::uint16_t kNuvotonLegacyUsbIspPid = 0xA316u;
constexpr std::size_t kPacketSize = 64u;
constexpr std::size_t kHeaderSize = 8u;
constexpr std::size_t kPayloadSize = kPacketSize - kHeaderSize;
constexpr int kNuvoIspConnectAttemptMs = 40;

HANDLE AsHandle(void* h) {
    return reinterpret_cast<HANDLE>(h);
}

std::vector<std::uint8_t> IoReadWithTimeout(HANDLE handle, DWORD bytes_to_read, int timeout_ms) {
    std::vector<std::uint8_t> out(bytes_to_read, 0u);

    OVERLAPPED ov = {};
    ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!ov.hEvent) {
        throw std::runtime_error("CreateEvent failed for HID read");
    }

    DWORD read_bytes = 0;
    BOOL ok = ReadFile(handle, out.data(), bytes_to_read, nullptr, &ov);
    if (!ok && GetLastError() != ERROR_IO_PENDING) {
        CloseHandle(ov.hEvent);
        throw std::runtime_error("HID ReadFile failed");
    }

    DWORD wait = WaitForSingleObject(ov.hEvent, static_cast<DWORD>((std::max)(1, timeout_ms)));
    if (wait != WAIT_OBJECT_0) {
        CancelIoEx(handle, &ov);
        CloseHandle(ov.hEvent);
        throw std::runtime_error(wait == WAIT_TIMEOUT ? "HID read timeout" : "HID read wait failed");
    }

    if (!GetOverlappedResult(handle, &ov, &read_bytes, FALSE)) {
        CloseHandle(ov.hEvent);
        throw std::runtime_error("GetOverlappedResult(HID read) failed");
    }

    CloseHandle(ov.hEvent);
    out.resize(read_bytes);
    return out;
}

DWORD IoWriteWithTimeout(HANDLE handle, const std::vector<std::uint8_t>& data, int timeout_ms) {
    OVERLAPPED ov = {};
    ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!ov.hEvent) {
        throw std::runtime_error("CreateEvent failed for HID write");
    }

    DWORD written = 0;
    BOOL ok = WriteFile(handle, data.data(), static_cast<DWORD>(data.size()), nullptr, &ov);
    if (!ok && GetLastError() != ERROR_IO_PENDING) {
        CloseHandle(ov.hEvent);
        throw std::runtime_error("HID WriteFile failed");
    }

    DWORD wait = WaitForSingleObject(ov.hEvent, static_cast<DWORD>((std::max)(1, timeout_ms)));
    if (wait != WAIT_OBJECT_0) {
        CancelIoEx(handle, &ov);
        CloseHandle(ov.hEvent);
        throw std::runtime_error(wait == WAIT_TIMEOUT ? "HID write timeout" : "HID write wait failed");
    }

    if (!GetOverlappedResult(handle, &ov, &written, FALSE)) {
        CloseHandle(ov.hEvent);
        throw std::runtime_error("GetOverlappedResult(HID write) failed");
    }

    CloseHandle(ov.hEvent);
    return written;
}

void QueryCaps(HANDLE handle, mfc_tool::hid::DeviceInfo* info) {
    if (info == nullptr) {
        return;
    }
    PHIDP_PREPARSED_DATA preparsed = nullptr;
    if (!HidD_GetPreparsedData(handle, &preparsed) || preparsed == nullptr) {
        return;
    }

    HIDP_CAPS caps = {};
    NTSTATUS status = HidP_GetCaps(preparsed, &caps);
    HidD_FreePreparsedData(preparsed);
    if (status != HIDP_STATUS_SUCCESS) {
        return;
    }

    info->usage_page = caps.UsagePage;
    info->usage = caps.Usage;
    info->input_report_len = caps.InputReportByteLength;
    info->output_report_len = caps.OutputReportByteLength;
    info->feature_report_len = caps.FeatureReportByteLength;
}

void QueryAttributes(HANDLE handle, mfc_tool::hid::DeviceInfo* info) {
    if (info == nullptr) {
        return;
    }
    HIDD_ATTRIBUTES attr = {};
    attr.Size = sizeof(attr);
    if (!HidD_GetAttributes(handle, &attr)) {
        return;
    }
    info->vendor_id = attr.VendorID;
    info->product_id = attr.ProductID;
}

std::wstring QueryWideString(HANDLE handle, BOOLEAN(__stdcall* query_fn)(HANDLE, PVOID, ULONG)) {
    wchar_t buffer[256] = {};
    if (!query_fn(handle, buffer, static_cast<ULONG>(sizeof(buffer)))) {
        return L"";
    }
    return std::wstring(buffer);
}

} // namespace

NuvoIspUsbHidClient::~NuvoIspUsbHidClient() {
    Disconnect();
}

void NuvoIspUsbHidClient::Connect(std::uint16_t vid,
                                  std::uint16_t pid,
                                  const std::wstring& path,
                                  int timeout_ms) {
    Disconnect();
    timeout_ms_ = (std::max)(100, timeout_ms);
    packet_index_ = 18u;

    if (!path.empty()) {
        device_.path = path;
        OpenHandle(path);
    } else {
        auto devices = mfc_tool::hid::EnumerateHidDevices(vid, pid, true);
        if (devices.empty() && vid == kNuvotonVid && pid == kNuvotonUsbIspPid) {
            devices = mfc_tool::hid::EnumerateHidDevices(vid, kNuvotonLegacyUsbIspPid, true);
        }
        auto preferred = mfc_tool::hid::SelectPreferredDevice(devices);
        if (!preferred.has_value()) {
            throw std::runtime_error("No matching USB HID ISP device found");
        }
        device_ = preferred.value();
        OpenHandle(device_.path);
    }

    PopulateDeviceMeta();

    if (device_.vendor_id != 0u && vid != 0u && device_.vendor_id != vid) {
        Disconnect();
        throw std::runtime_error("Connected USB HID VID does not match requested VID");
    }
    if (device_.product_id != 0u && pid != 0u &&
        device_.product_id != pid &&
        !IsLegacyNuvotonHid(vid, pid, device_.product_id)) {
        Disconnect();
        throw std::runtime_error("Connected USB HID PID does not match requested PID");
    }
}

void NuvoIspUsbHidClient::Disconnect() {
    HANDLE h = AsHandle(handle_);
    if (h != nullptr && h != INVALID_HANDLE_VALUE) {
        CancelIoEx(h, nullptr);
        CloseHandle(h);
    }
    handle_ = nullptr;
    device_ = {};
    packet_index_ = 18u;
}

bool NuvoIspUsbHidClient::IsConnected() const noexcept {
    HANDLE h = AsHandle(handle_);
    return h != nullptr && h != INVALID_HANDLE_VALUE;
}

void NuvoIspUsbHidClient::SetOptions(const NuvoIspUsbHidOptions& options) {
    options_ = options;
    options_.response_poll_ms = std::clamp(options_.response_poll_ms, 1, 1000);
    options_.normal_response_timeout_ms = std::clamp(options_.normal_response_timeout_ms, 100, 60000);
    options_.long_response_timeout_ms = std::clamp(options_.long_response_timeout_ms, 1000, 120000);
    options_.program_retry_count = std::clamp(options_.program_retry_count, 1, 50);
}

bool NuvoIspUsbHidClient::TryConnectTargetOnce(int response_timeout_ms,
                                               const std::function<bool()>& should_cancel,
                                               const std::function<void()>& on_poll,
                                               std::string* error_text) {
    try {
        (void)Transact(CMD_CONNECT, {}, false, response_timeout_ms, true, should_cancel, on_poll);
        SyncPacketNumber(should_cancel, on_poll);
        if (error_text != nullptr) {
            error_text->clear();
        }
        return true;
    } catch (const std::exception& e) {
        if (error_text != nullptr) {
            *error_text = e.what();
        }
        return false;
    }
}

void NuvoIspUsbHidClient::ConnectTarget(const std::function<bool()>& should_cancel,
                                        const std::function<void()>& on_poll) {
    const auto start = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::milliseconds(options_.long_response_timeout_ms);
    const auto retry_sleep = std::chrono::milliseconds((std::max)(20, options_.response_poll_ms));
    const int attempt_timeout_ms = (std::min)(options_.normal_response_timeout_ms, 300);
    std::string last_error = "target did not respond";

    while (true) {
        ThrowIfCancelled(should_cancel);
        if (TryConnectTargetOnce(attempt_timeout_ms, should_cancel, on_poll, &last_error)) {
            return;
        }
        ThrowIfCancelled(should_cancel);

        if (on_poll) {
            on_poll();
        }
        if (std::chrono::steady_clock::now() - start >= timeout) {
            break;
        }
        std::this_thread::sleep_for(retry_sleep);
    }

    throw std::runtime_error("USB HID ISP target handshake timeout: " + last_error);
}

void NuvoIspUsbHidClient::SyncPacketNumber(const std::function<bool()>& should_cancel,
                                           const std::function<void()>& on_poll) {
    std::vector<std::uint8_t> payload;
    packet_index_ = 1u;
    AppendLe32(&payload, packet_index_);
    (void)Transact(CMD_SYNC_PACKNO, payload, false, options_.normal_response_timeout_ms, true, should_cancel, on_poll);
}

std::uint8_t NuvoIspUsbHidClient::GetFirmwareVersion() {
    const auto response = Transact(CMD_GET_FWVER, {}, true, options_.normal_response_timeout_ms, true, {}, {});
    if (response.bytes.size() <= 8u) {
        throw std::runtime_error("invalid USB HID ISP firmware-version response");
    }
    return response.bytes[8];
}

std::uint32_t NuvoIspUsbHidClient::GetDeviceId() {
    const auto response = Transact(CMD_GET_DEVICEID, {}, true, options_.normal_response_timeout_ms, true, {}, {});
    if (response.bytes.size() < 12u) {
        throw std::runtime_error("invalid USB HID ISP device-id response");
    }
    return ReadLe32(response.bytes, 8u);
}

std::array<std::uint32_t, 4> NuvoIspUsbHidClient::ReadConfig() {
    const auto response = Transact(CMD_READ_CONFIG, {}, true, options_.normal_response_timeout_ms, true, {}, {});
    if (response.bytes.size() < 24u) {
        throw std::runtime_error("invalid USB HID ISP read-config response");
    }

    std::array<std::uint32_t, 4> config = {};
    for (std::size_t i = 0; i < config.size(); ++i) {
        config[i] = ReadLe32(response.bytes, 8u + i * 4u);
    }
    return config;
}

void NuvoIspUsbHidClient::ProgramAprom(
    const std::vector<std::uint8_t>& image,
    const std::function<void(const NuvoIspUsbHidProgress&)>& on_progress,
    const std::function<bool()>& should_cancel,
    const std::function<void()>& on_poll
) {
    if (!IsConnected()) {
        throw std::runtime_error("USB HID is not connected");
    }
    if (image.empty()) {
        throw std::invalid_argument("binary image is empty");
    }
    if (image.size() > 0x00100000u) {
        throw std::invalid_argument("binary image is too large for the ISP packet flow");
    }

    const std::uint32_t total_len = static_cast<std::uint32_t>(image.size());
    const std::uint32_t packet_address_field = 0u;
    SyncPacketNumber(should_cancel, on_poll);

    std::uint32_t offset = 0u;
    std::uint32_t packet_count = 0u;
    while (offset < total_len) {
        ThrowIfCancelled(should_cancel);

        std::vector<std::uint8_t> payload;
        std::uint32_t command = 0u;
        std::uint32_t write_len = total_len - offset;
        int timeout_ms = options_.normal_response_timeout_ms;

        if (offset == 0u) {
            command = CMD_UPDATE_APROM;
            write_len = (std::min)(write_len, static_cast<std::uint32_t>(kPayloadSize - 8u));
            AppendLe32(&payload, packet_address_field);
            AppendLe32(&payload, total_len);
            payload.insert(payload.end(), image.begin(), image.begin() + write_len);
            timeout_ms = options_.long_response_timeout_ms;
        } else {
            write_len = (std::min)(write_len, static_cast<std::uint32_t>(kPayloadSize));
            payload.insert(payload.end(), image.begin() + offset, image.begin() + offset + write_len);
        }

        bool accepted = false;
        for (int attempt = 0; attempt < options_.program_retry_count && !accepted; ++attempt) {
            ThrowIfCancelled(should_cancel);
            NuvoIspUsbHidResponse response = Transact(command, payload, true, timeout_ms, false, should_cancel, on_poll);
            if (response.checksum_ok && response.index_ok) {
                accepted = true;
                break;
            }
            if (offset == 0u || attempt + 1 >= options_.program_retry_count) {
                throw std::runtime_error(ResponseErrorText(response));
            }
            SendResendPacket(should_cancel, on_poll);
        }

        offset += write_len;
        ++packet_count;
        if (on_progress) {
            NuvoIspUsbHidProgress progress;
            progress.bytes_done = (std::min)(offset, static_cast<std::uint32_t>(image.size()));
            progress.total_bytes = static_cast<std::uint32_t>(image.size());
            progress.packet_index = packet_count;
            on_progress(progress);
        }
        if (on_poll) {
            on_poll();
        }
    }
}

void NuvoIspUsbHidClient::RunAprom() {
    SendNoResponse(CMD_RUN_APROM);
}

void NuvoIspUsbHidClient::ResetTarget() {
    SendNoResponse(CMD_RESET);
}

void NuvoIspUsbHidClient::OpenHandle(const std::wstring& path) {
    HANDLE h = CreateFileW(
        path.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED,
        nullptr
    );

    if (h == INVALID_HANDLE_VALUE) {
        throw std::runtime_error(LastErrorMessage("CreateFileW"));
    }

    handle_ = h;
    HidD_SetNumInputBuffers(h, 64);
}

void NuvoIspUsbHidClient::PopulateDeviceMeta() {
    HANDLE h = AsHandle(handle_);
    if (h == nullptr || h == INVALID_HANDLE_VALUE) {
        return;
    }

    QueryAttributes(h, &device_);
    QueryCaps(h, &device_);
    device_.manufacturer = QueryWideString(h, HidD_GetManufacturerString);
    device_.product = QueryWideString(h, HidD_GetProductString);
    device_.serial_number = QueryWideString(h, HidD_GetSerialNumberString);

    if (device_.input_report_len == 0u) {
        device_.input_report_len = 65u;
    }
    if (device_.output_report_len == 0u) {
        device_.output_report_len = 65u;
    }
}

NuvoIspUsbHidResponse NuvoIspUsbHidClient::Transact(
    std::uint32_t command,
    const std::vector<std::uint8_t>& payload,
    bool check_index,
    int response_timeout_ms,
    bool require_valid,
    const std::function<bool()>& should_cancel,
    const std::function<void()>& on_poll
) {
    if (!IsConnected()) {
        throw std::runtime_error("USB HID is not connected");
    }
    if (payload.size() > kPayloadSize) {
        throw std::invalid_argument("ISP payload exceeds 56 bytes");
    }

    ThrowIfCancelled(should_cancel);
    HANDLE h = AsHandle(handle_);
    const std::uint32_t sent_index = packet_index_;
    const std::vector<std::uint8_t> packet = BuildPacket(command, sent_index, payload);
    const std::uint16_t expected_checksum = Checksum(packet);

    const std::vector<std::uint8_t> report = BuildWriteReport(packet);
    const DWORD written = IoWriteWithTimeout(h, report, timeout_ms_);
    if (written == 0u) {
        throw std::runtime_error("failed to write USB HID report");
    }
    packet_index_ += 2u;

    std::vector<std::uint8_t> rx = ReadPacket(expected_checksum,
                                             sent_index + 1u,
                                             check_index,
                                             response_timeout_ms,
                                             should_cancel,
                                             on_poll);

    NuvoIspUsbHidResponse response;
    response.bytes = std::move(rx);
    response.expected_checksum = expected_checksum;
    response.response_checksum = static_cast<std::uint16_t>(response.bytes[0] | (response.bytes[1] << 8));
    response.expected_index = sent_index + 1u;
    response.response_index = ReadLe32(response.bytes, 4u);
    response.checksum_ok = (response.response_checksum == response.expected_checksum);
    response.index_ok = !check_index || (response.response_index == response.expected_index);
    if (require_valid && (!response.checksum_ok || !response.index_ok)) {
        throw std::runtime_error(ResponseErrorText(response));
    }
    return response;
}

void NuvoIspUsbHidClient::SendNoResponse(std::uint32_t command) {
    if (!IsConnected()) {
        throw std::runtime_error("USB HID is not connected");
    }

    HANDLE h = AsHandle(handle_);
    const std::vector<std::uint8_t> packet = BuildPacket(command, packet_index_, {});
    const std::vector<std::uint8_t> report = BuildWriteReport(packet);
    const DWORD written = IoWriteWithTimeout(h, report, timeout_ms_);
    if (written == 0u) {
        throw std::runtime_error("failed to write USB HID report");
    }
    packet_index_ += 2u;
}

std::vector<std::uint8_t> NuvoIspUsbHidClient::ReadPacket(
    std::uint16_t expected_checksum,
    std::uint32_t expected_index,
    bool check_index,
    int timeout_ms,
    const std::function<bool()>& should_cancel,
    const std::function<void()>& on_poll
) {
    HANDLE h = AsHandle(handle_);
    if (h == nullptr || h == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("USB HID is not connected");
    }

    const std::uint16_t in_len = device_.input_report_len == 0u ? 65u : device_.input_report_len;
    const DWORD bytes_to_read = static_cast<DWORD>((std::max<std::uint16_t>)(in_len, 64u));
    const auto start = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::milliseconds((std::max)(100, timeout_ms));
    std::vector<std::uint8_t> fallback;

    while (true) {
        ThrowIfCancelled(should_cancel);
        const auto now = std::chrono::steady_clock::now();
        if (now - start >= timeout) {
            throw std::runtime_error("USB HID ISP response timeout");
        }

        int remain_ms = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(timeout - (now - start)).count());
        remain_ms = (std::max)(50, remain_ms);

        std::vector<std::uint8_t> read_buf = IoReadWithTimeout(h, bytes_to_read, remain_ms);
        if (on_poll) {
            on_poll();
        }
        if (read_buf.size() < kPacketSize) {
            continue;
        }

        std::vector<const std::uint8_t*> candidates;
        if (read_buf.size() >= kPacketSize + 1u) {
            candidates.push_back(read_buf.data() + 1);
        }
        candidates.push_back(read_buf.data());

        for (const std::uint8_t* candidate : candidates) {
            const std::uint16_t checksum = ReadLe16(candidate);
            const std::uint32_t response_index = ReadLe32Ptr(candidate + 4);
            if (checksum == expected_checksum && (!check_index || response_index == expected_index)) {
                return std::vector<std::uint8_t>(candidate, candidate + kPacketSize);
            }
        }

        if (device_.product_id == kNuvotonLegacyUsbIspPid) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        const std::uint8_t* best = candidates.front();
        if (read_buf.size() >= kPacketSize + 1u && read_buf[0] == 0u) {
            best = read_buf.data() + 1;
        }
        fallback.assign(best, best + kPacketSize);
        return fallback;
    }
}

std::vector<std::uint8_t> NuvoIspUsbHidClient::BuildWriteReport(const std::vector<std::uint8_t>& packet) const {
    if (packet.size() != kPacketSize) {
        throw std::runtime_error("USB HID ISP packet size must be 64 bytes");
    }

    const std::uint16_t out_len = device_.output_report_len == 0u ? 65u : device_.output_report_len;
    if (out_len < kPacketSize) {
        throw std::runtime_error("USB HID output report length too small");
    }
    if (out_len == kPacketSize) {
        return packet;
    }

    std::vector<std::uint8_t> report(out_len, 0u);
    report[0] = 0u;
    std::memcpy(report.data() + 1, packet.data(), packet.size());
    return report;
}

void NuvoIspUsbHidClient::SendResendPacket(const std::function<bool()>& should_cancel,
                                           const std::function<void()>& on_poll) {
    (void)Transact(CMD_RESEND_PACKET, {}, false, options_.long_response_timeout_ms, true, should_cancel, on_poll);
}

std::vector<std::uint8_t> NuvoIspUsbHidClient::BuildPacket(
    std::uint32_t command,
    std::uint32_t packet_index,
    const std::vector<std::uint8_t>& payload
) {
    std::vector<std::uint8_t> packet(kPacketSize, 0u);
    packet[0] = static_cast<std::uint8_t>(command & 0xFFu);
    packet[1] = static_cast<std::uint8_t>((command >> 8u) & 0xFFu);
    packet[2] = static_cast<std::uint8_t>((command >> 16u) & 0xFFu);
    packet[3] = static_cast<std::uint8_t>((command >> 24u) & 0xFFu);
    packet[4] = static_cast<std::uint8_t>(packet_index & 0xFFu);
    packet[5] = static_cast<std::uint8_t>((packet_index >> 8u) & 0xFFu);
    packet[6] = static_cast<std::uint8_t>((packet_index >> 16u) & 0xFFu);
    packet[7] = static_cast<std::uint8_t>((packet_index >> 24u) & 0xFFu);
    std::copy(payload.begin(), payload.end(), packet.begin() + static_cast<std::ptrdiff_t>(kHeaderSize));
    return packet;
}

std::uint16_t NuvoIspUsbHidClient::Checksum(const std::vector<std::uint8_t>& bytes) {
    std::uint32_t sum = 0u;
    for (std::uint8_t b : bytes) {
        sum += b;
    }
    return static_cast<std::uint16_t>(sum & 0xFFFFu);
}

std::uint16_t NuvoIspUsbHidClient::ReadLe16(const std::uint8_t* bytes) {
    return static_cast<std::uint16_t>(bytes[0] | (bytes[1] << 8u));
}

std::uint32_t NuvoIspUsbHidClient::ReadLe32(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    if (bytes.size() < offset + 4u) {
        throw std::runtime_error("short little-endian word");
    }
    return static_cast<std::uint32_t>(bytes[offset]) |
        (static_cast<std::uint32_t>(bytes[offset + 1u]) << 8u) |
        (static_cast<std::uint32_t>(bytes[offset + 2u]) << 16u) |
        (static_cast<std::uint32_t>(bytes[offset + 3u]) << 24u);
}

std::uint32_t NuvoIspUsbHidClient::ReadLe32Ptr(const std::uint8_t* bytes) {
    return static_cast<std::uint32_t>(bytes[0]) |
        (static_cast<std::uint32_t>(bytes[1]) << 8u) |
        (static_cast<std::uint32_t>(bytes[2]) << 16u) |
        (static_cast<std::uint32_t>(bytes[3]) << 24u);
}

void NuvoIspUsbHidClient::AppendLe32(std::vector<std::uint8_t>* out, std::uint32_t value) {
    out->push_back(static_cast<std::uint8_t>(value & 0xFFu));
    out->push_back(static_cast<std::uint8_t>((value >> 8u) & 0xFFu));
    out->push_back(static_cast<std::uint8_t>((value >> 16u) & 0xFFu));
    out->push_back(static_cast<std::uint8_t>((value >> 24u) & 0xFFu));
}

std::string NuvoIspUsbHidClient::ResponseErrorText(const NuvoIspUsbHidResponse& response) {
    std::ostringstream ss;
    ss << "USB HID ISP response validation failed";
    if (!response.checksum_ok) {
        ss << ": checksum expected=0x" << std::uppercase << std::hex << std::setw(4) << std::setfill('0')
           << response.expected_checksum << " actual=0x" << std::setw(4) << response.response_checksum;
    }
    if (!response.index_ok) {
        ss << ": packet index expected=" << std::dec << response.expected_index
           << " actual=" << response.response_index;
    }
    return ss.str();
}

std::string NuvoIspUsbHidClient::WideToNarrow(const std::wstring& text) {
    std::string out;
    out.reserve(text.size());
    for (wchar_t ch : text) {
        out.push_back(ch >= 0 && ch <= 0x7F ? static_cast<char>(ch) : '?');
    }
    return out;
}

void NuvoIspUsbHidClient::ThrowIfCancelled(const std::function<bool()>& should_cancel) {
    if (should_cancel && should_cancel()) {
        throw std::runtime_error("operation cancelled by user");
    }
}

bool NuvoIspUsbHidClient::IsLegacyNuvotonHid(std::uint16_t requested_vid,
                                             std::uint16_t requested_pid,
                                             std::uint16_t actual_pid) {
    return requested_vid == kNuvotonVid &&
           requested_pid == kNuvotonUsbIspPid &&
           actual_pid == kNuvotonLegacyUsbIspPid;
}

std::string NuvoIspUsbHidClient::LastErrorMessage(const char* prefix) {
    DWORD err = GetLastError();
    std::ostringstream oss;
    oss << prefix << " failed, win32=" << err;
    return oss.str();
}

} // namespace mfc_tool::core
