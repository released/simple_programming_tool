#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "../vcom/serial_port.h"

namespace mfc_tool::core {

struct XmodemOptions {
    int handshake_timeout_ms = 60000;
    int response_timeout_ms = 10000;
    int byte_timeout_ms = 1000;
    int max_retries = 10;
};

struct XmodemProgress {
    std::uint32_t bytes_done = 0;
    std::uint32_t total_bytes = 0;
    std::uint32_t block_index = 0;
    bool crc_mode = true;
};

class XmodemSender {
public:
    XmodemSender() = default;

    void Connect(const std::wstring& com_port, std::uint32_t baudrate);
    void Disconnect();
    [[nodiscard]] bool IsConnected() const noexcept;
    [[nodiscard]] const std::wstring& CurrentPort() const noexcept { return current_port_; }
    [[nodiscard]] std::uint32_t CurrentBaudrate() const noexcept { return baudrate_; }

    void SetOptions(const XmodemOptions& options);
    [[nodiscard]] const XmodemOptions& Options() const noexcept { return options_; }

    void CancelTransfer();
    void Send(const std::vector<std::uint8_t>& image,
              const std::function<void(const XmodemProgress&)>& on_progress,
              const std::function<bool()>& should_cancel,
              const std::function<void()>& on_poll);

private:
    int ReadByte(int timeout_ms,
                 const std::function<bool()>& should_cancel,
                 const std::function<void()>& on_poll);
    void WriteByte(std::uint8_t byte);
    void WriteAll(const std::vector<std::uint8_t>& bytes);
    int WaitForReceiver(const std::function<bool()>& should_cancel,
                        const std::function<void()>& on_poll);

    static std::uint16_t Crc16Ccitt(const std::uint8_t* data, std::size_t len);
    static std::uint8_t Checksum8(const std::uint8_t* data, std::size_t len);
    static std::string WideToNarrow(const std::wstring& text);
    static void ThrowIfCancelled(const std::function<bool()>& should_cancel);

private:
    mfc_tool::vcom::SerialPort port_;
    XmodemOptions options_;
    std::wstring current_port_;
    std::uint32_t baudrate_ = 115200;
};

} // namespace mfc_tool::core
