#include "vcom_bridge_client.h"

#include <algorithm>
#include <chrono>
#include <sstream>

namespace mfc_tool::vcom {
namespace {

std::string BridgeStatusTextNarrow(std::uint8_t status) {
    switch (status) {
    case 0x00: return "OK";
    case 0x01: return "BAD_MAGIC";
    case 0x02: return "BAD_COMMAND";
    case 0x03: return "BAD_PAYLOAD";
    case 0x04: return "IO_ERROR";
    case 0x05: return "TIMEOUT";
    case 0x06: return "NOT_READY";
    case 0x07: return "BUSY";
    case 0x08: return "UNSUPPORTED";
    default: return "UNKNOWN";
    }
}

} // namespace

VcomBridgeStatusException::VcomBridgeStatusException(std::uint8_t status)
    : VcomBridgeException("bridge status error: " + BridgeStatusTextNarrow(status) +
                          " (0x" + [&status]() {
                              std::ostringstream ss;
                              ss << std::uppercase << std::hex << static_cast<int>(status);
                              return ss.str();
                          }() + ")"),
      status_(status) {
}

VcomBridgeClient::~VcomBridgeClient() {
    Disconnect();
}

void VcomBridgeClient::Connect(const std::wstring& com_port, std::uint32_t baudrate, int timeout_ms) {
    Disconnect();

    if (com_port.empty()) {
        throw VcomBridgeException("No COM port selected");
    }

    std::wstring error;
    if (!port_.Open(com_port, baudrate, &error)) {
        throw VcomBridgeException("Open COM failed: " + WideToNarrow(error));
    }

    current_port_ = com_port;
    baudrate_ = baudrate;
    timeout_ms_ = (std::max)(100, timeout_ms);
    seq_ = 1;
    parser_.Clear();
}

void VcomBridgeClient::Disconnect() {
    port_.Close();
    parser_.Clear();
    current_port_.clear();
}

bool VcomBridgeClient::IsConnected() const noexcept {
    return port_.IsOpen();
}

std::vector<std::uint8_t> VcomBridgeClient::Transact(std::uint8_t cmd, const std::vector<std::uint8_t>& payload) {
    if (!port_.IsOpen()) {
        throw VcomBridgeException("Serial port is not open");
    }

    const std::uint16_t seq = seq_++;
    Frame req;
    req.type = FrameType::Request;
    req.seq = seq;
    req.cmd = cmd;
    req.status = 0u;
    req.payload = payload;

    const std::vector<std::uint8_t> raw = EncodeFrame(req);
    std::wstring error;
    if (!port_.WriteAll(raw.data(), raw.size(), static_cast<std::uint32_t>(timeout_ms_), &error)) {
        throw VcomBridgeException("Write COM failed: " + WideToNarrow(error));
    }

    return ReadFrame(cmd, seq, timeout_ms_);
}

std::vector<std::uint8_t> VcomBridgeClient::ReadFrame(std::uint8_t cmd, std::uint16_t seq, int timeout_ms) {
    const auto start = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::milliseconds((std::max)(100, timeout_ms));

    while (true) {
        Frame frame;
        if (parser_.TryPop(&frame)) {
            if (frame.type != FrameType::Response || frame.cmd != cmd || frame.seq != seq) {
                continue;
            }
            if (frame.status != 0x00u) {
                throw VcomBridgeStatusException(frame.status);
            }
            return frame.payload;
        }

        const auto now = std::chrono::steady_clock::now();
        if (now - start >= timeout) {
            throw VcomBridgeException("timeout waiting VCOM response");
        }

        int remain_ms = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(timeout - (now - start)).count());
        remain_ms = (std::max)(20, remain_ms);

        std::vector<std::uint8_t> chunk;
        std::wstring error;
        if (!port_.ReadSome(&chunk, 512u, static_cast<std::uint32_t>(remain_ms), &error)) {
            if (error == L"Read timeout.") {
                continue;
            }
            throw VcomBridgeException("Read COM failed: " + WideToNarrow(error));
        }
        parser_.Push(chunk.data(), chunk.size());
    }
}

std::string VcomBridgeClient::WideToNarrow(const std::wstring& text) {
    std::string out;
    out.reserve(text.size());
    for (wchar_t ch : text) {
        out.push_back(ch >= 0 && ch <= 0x7F ? static_cast<char>(ch) : '?');
    }
    return out;
}

} // namespace mfc_tool::vcom
