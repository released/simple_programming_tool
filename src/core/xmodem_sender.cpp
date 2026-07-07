#include "xmodem_sender.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <stdexcept>
#include <thread>

namespace mfc_tool::core {
namespace {

constexpr std::uint8_t XMD_SOH = 0x01u;
constexpr std::uint8_t XMD_EOT = 0x04u;
constexpr std::uint8_t XMD_ACK = 0x06u;
constexpr std::uint8_t XMD_NAK = 0x15u;
constexpr std::uint8_t XMD_CAN = 0x18u;
constexpr std::uint8_t XMD_CTRLZ = 0x1Au;
constexpr std::size_t kBlockSize = 128u;
constexpr int kRequiredSyncCount = 2;

std::string ResponseName(int rsp) {
    if (rsp < 0) {
        return "timeout";
    }
    switch (rsp) {
    case XMD_ACK: return "ACK";
    case XMD_NAK: return "NAK";
    case XMD_CAN: return "CAN";
    case 'C': return "C";
    default:
        {
            const char hex[] = "0123456789ABCDEF";
            std::string out = "0x";
            out.push_back(hex[(rsp >> 4) & 0x0F]);
            out.push_back(hex[rsp & 0x0F]);
            return out;
        }
    }
}

} // namespace

void XmodemSender::Connect(const std::wstring& com_port, std::uint32_t baudrate) {
    Disconnect();
    if (com_port.empty()) {
        throw std::runtime_error("no COM port selected");
    }

    std::wstring error;
    if (!port_.Open(com_port, baudrate, &error)) {
        throw std::runtime_error("open XMODEM UART failed: " + WideToNarrow(error));
    }
    current_port_ = com_port;
    baudrate_ = baudrate;
}

void XmodemSender::Disconnect() {
    port_.Close();
    current_port_.clear();
}

bool XmodemSender::IsConnected() const noexcept {
    return port_.IsOpen();
}

void XmodemSender::SetOptions(const XmodemOptions& options) {
    options_ = options;
    options_.handshake_timeout_ms = std::clamp(options_.handshake_timeout_ms, 1000, 300000);
    options_.response_timeout_ms = std::clamp(options_.response_timeout_ms, 1000, 60000);
    options_.byte_timeout_ms = std::clamp(options_.byte_timeout_ms, 10, 10000);
    options_.max_retries = std::clamp(options_.max_retries, 1, 50);
}

void XmodemSender::CancelTransfer() {
    if (!IsConnected()) {
        return;
    }
    WriteByte(XMD_CAN);
    WriteByte(XMD_CAN);
    WriteByte(XMD_CAN);
}

void XmodemSender::Send(
    const std::vector<std::uint8_t>& image,
    const std::function<void(const XmodemProgress&)>& on_progress,
    const std::function<bool()>& should_cancel,
    const std::function<void()>& on_poll
) {
    if (!IsConnected()) {
        throw std::runtime_error("XMODEM UART is not connected");
    }
    if (image.empty()) {
        throw std::invalid_argument("binary image is empty");
    }

    std::wstring purge_error;
    if (!port_.Purge(&purge_error)) {
        throw std::runtime_error("XMODEM UART purge failed: " + WideToNarrow(purge_error));
    }

    const int sync = WaitForReceiver(should_cancel, on_poll);
    const bool crc_mode = (sync == 'C');
    std::uint8_t packet_no = 1u;
    std::size_t offset = 0u;
    std::uint32_t block_index = 0u;

    if (on_progress) {
        XmodemProgress p;
        p.total_bytes = static_cast<std::uint32_t>(image.size());
        p.crc_mode = crc_mode;
        on_progress(p);
    }

    while (offset < image.size()) {
        ThrowIfCancelled(should_cancel);

        const std::size_t remain = image.size() - offset;
        const std::size_t copy_len = (std::min)(remain, kBlockSize);
        std::vector<std::uint8_t> packet;
        packet.reserve(3u + kBlockSize + (crc_mode ? 2u : 1u));
        packet.push_back(XMD_SOH);
        packet.push_back(packet_no);
        packet.push_back(static_cast<std::uint8_t>(~packet_no));

        std::uint8_t payload[kBlockSize] = {};
        std::memcpy(payload, image.data() + offset, copy_len);
        if (copy_len < kBlockSize) {
            payload[copy_len] = XMD_CTRLZ;
        }
        packet.insert(packet.end(), payload, payload + kBlockSize);
        if (crc_mode) {
            const std::uint16_t crc = Crc16Ccitt(payload, kBlockSize);
            packet.push_back(static_cast<std::uint8_t>((crc >> 8u) & 0xFFu));
            packet.push_back(static_cast<std::uint8_t>(crc & 0xFFu));
        } else {
            packet.push_back(Checksum8(payload, kBlockSize));
        }

        bool acked = false;
        int last_rsp = -1;
        int nak_count = 0;
        int timeout_count = 0;
        for (int retry = 0; retry < options_.max_retries && !acked; ++retry) {
            ThrowIfCancelled(should_cancel);
            WriteAll(packet);
            const int rsp = ReadByte(options_.response_timeout_ms, should_cancel, on_poll);
            last_rsp = rsp;
            if (rsp == XMD_ACK) {
                acked = true;
                break;
            }
            if (rsp == XMD_NAK) {
                ++nak_count;
            } else if (rsp < 0) {
                ++timeout_count;
            }
            if (rsp == XMD_CAN) {
                const int rsp2 = ReadByte(1000, should_cancel, on_poll);
                if (rsp2 == XMD_CAN) {
                    WriteByte(XMD_ACK);
                    throw std::runtime_error("XMODEM transfer cancelled by target");
                }
            }
        }

        if (!acked) {
            CancelTransfer();
            throw std::runtime_error(
                "XMODEM block was not acknowledged; block=" +
                std::to_string(block_index + 1u) +
                ", last response=" + ResponseName(last_rsp) +
                ", NAK=" + std::to_string(nak_count) +
                ", timeout=" + std::to_string(timeout_count));
        }

        offset += copy_len;
        ++block_index;
        ++packet_no;
        if (on_progress) {
            XmodemProgress p;
            p.bytes_done = static_cast<std::uint32_t>((std::min)(offset, image.size()));
            p.total_bytes = static_cast<std::uint32_t>(image.size());
            p.block_index = block_index;
            p.crc_mode = crc_mode;
            on_progress(p);
        }
        if (on_poll) {
            on_poll();
        }
    }

    for (int retry = 0; retry < options_.max_retries; ++retry) {
        ThrowIfCancelled(should_cancel);
        WriteByte(XMD_EOT);
        const int rsp = ReadByte(options_.response_timeout_ms, should_cancel, on_poll);
        if (rsp == XMD_ACK) {
            return;
        }
    }

    throw std::runtime_error("XMODEM EOT was not acknowledged");
}

int XmodemSender::ReadByte(int timeout_ms,
                           const std::function<bool()>& should_cancel,
                           const std::function<void()>& on_poll) {
    const auto start = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::milliseconds(timeout_ms);
    while (true) {
        ThrowIfCancelled(should_cancel);
        std::vector<std::uint8_t> chunk;
        std::wstring error;
        if (port_.ReadSome(&chunk, 1u, static_cast<std::uint32_t>(options_.byte_timeout_ms), &error)) {
            if (!chunk.empty()) {
                return chunk.front();
            }
        } else if (error != L"Read timeout.") {
            throw std::runtime_error("XMODEM UART read failed: " + WideToNarrow(error));
        }

        if (on_poll) {
            on_poll();
        }
        if (std::chrono::steady_clock::now() - start >= timeout) {
            return -1;
        }
    }
}

void XmodemSender::WriteByte(std::uint8_t byte) {
    WriteAll(std::vector<std::uint8_t>{byte});
}

void XmodemSender::WriteAll(const std::vector<std::uint8_t>& bytes) {
    std::wstring error;
    if (!port_.WriteAll(bytes.data(), bytes.size(), static_cast<std::uint32_t>(options_.response_timeout_ms), &error)) {
        throw std::runtime_error("XMODEM UART write failed: " + WideToNarrow(error));
    }
}

int XmodemSender::WaitForReceiver(const std::function<bool()>& should_cancel,
                                  const std::function<void()>& on_poll) {
    const auto start = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::milliseconds(options_.handshake_timeout_ms);
    int last_sync = -1;
    int sync_count = 0;
    while (std::chrono::steady_clock::now() - start < timeout) {
        const int c = ReadByte(1000, should_cancel, on_poll);
        if (c == 'C' || c == XMD_NAK) {
            if (c == last_sync) {
                ++sync_count;
            } else {
                last_sync = c;
                sync_count = 1;
            }
            if (sync_count >= kRequiredSyncCount) {
                return c;
            }
            continue;
        }
        if (c >= 0) {
            last_sync = -1;
            sync_count = 0;
        }
        if (c == XMD_CAN) {
            const int c2 = ReadByte(1000, should_cancel, on_poll);
            if (c2 == XMD_CAN) {
                WriteByte(XMD_ACK);
                throw std::runtime_error("XMODEM transfer cancelled by target");
            }
        }
    }
    throw std::runtime_error("XMODEM receiver handshake timeout");
}

std::uint16_t XmodemSender::Crc16Ccitt(const std::uint8_t* data, std::size_t len) {
    std::uint16_t crc = 0u;
    for (std::size_t i = 0; i < len; ++i) {
        crc ^= static_cast<std::uint16_t>(data[i]) << 8u;
        for (int bit = 0; bit < 8; ++bit) {
            if ((crc & 0x8000u) != 0u) {
                crc = static_cast<std::uint16_t>((crc << 1u) ^ 0x1021u);
            } else {
                crc = static_cast<std::uint16_t>(crc << 1u);
            }
        }
    }
    return crc;
}

std::uint8_t XmodemSender::Checksum8(const std::uint8_t* data, std::size_t len) {
    std::uint32_t sum = 0u;
    for (std::size_t i = 0; i < len; ++i) {
        sum += data[i];
    }
    return static_cast<std::uint8_t>(sum & 0xFFu);
}

std::string XmodemSender::WideToNarrow(const std::wstring& text) {
    std::string out;
    out.reserve(text.size());
    for (wchar_t ch : text) {
        out.push_back(ch >= 0 && ch <= 0x7F ? static_cast<char>(ch) : '?');
    }
    return out;
}

void XmodemSender::ThrowIfCancelled(const std::function<bool()>& should_cancel) {
    if (should_cancel && should_cancel()) {
        throw std::runtime_error("operation cancelled by user");
    }
}

} // namespace mfc_tool::core
