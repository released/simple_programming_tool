#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "serial_frame.h"
#include "serial_port.h"

namespace mfc_tool::vcom {

class VcomBridgeException : public std::runtime_error {
public:
    explicit VcomBridgeException(const std::string& msg) : std::runtime_error(msg) {}
};

class VcomBridgeStatusException : public VcomBridgeException {
public:
    explicit VcomBridgeStatusException(std::uint8_t status);
    [[nodiscard]] std::uint8_t status() const noexcept { return status_; }

private:
    std::uint8_t status_ = 0;
};

class VcomBridgeClient {
public:
    VcomBridgeClient() = default;
    ~VcomBridgeClient();

    VcomBridgeClient(const VcomBridgeClient&) = delete;
    VcomBridgeClient& operator=(const VcomBridgeClient&) = delete;

    void Connect(const std::wstring& com_port, std::uint32_t baudrate, int timeout_ms);
    void Disconnect();

    [[nodiscard]] bool IsConnected() const noexcept;
    [[nodiscard]] const std::wstring& CurrentPort() const noexcept { return current_port_; }
    [[nodiscard]] std::uint32_t CurrentBaudrate() const noexcept { return baudrate_; }

    std::vector<std::uint8_t> Transact(std::uint8_t cmd, const std::vector<std::uint8_t>& payload);

private:
    std::vector<std::uint8_t> ReadFrame(std::uint8_t cmd, std::uint16_t seq, int timeout_ms);
    static std::string WideToNarrow(const std::wstring& text);

private:
    SerialPort port_;
    FrameStreamParser parser_;
    std::wstring current_port_;
    std::uint32_t baudrate_ = 115200;
    int timeout_ms_ = 2000;
    std::uint16_t seq_ = 1;
};

} // namespace mfc_tool::vcom
