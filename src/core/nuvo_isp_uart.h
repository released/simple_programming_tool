#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "../vcom/serial_port.h"

namespace mfc_tool::core {

struct NuvoIspUartOptions {
    int response_poll_ms = 20;
    int normal_response_timeout_ms = 5000;
    int long_response_timeout_ms = 25000;
    int program_retry_count = 10;
};

struct NuvoIspUartResponse {
    std::vector<std::uint8_t> bytes;
    std::uint16_t expected_checksum = 0;
    std::uint16_t response_checksum = 0;
    std::uint32_t expected_index = 0;
    std::uint32_t response_index = 0;
    bool checksum_ok = false;
    bool index_ok = false;
};

struct NuvoIspUartProgress {
    std::uint32_t bytes_done = 0;
    std::uint32_t total_bytes = 0;
    std::uint32_t packet_index = 0;
};

class NuvoIspUartClient {
public:
    NuvoIspUartClient() = default;

    void Connect(const std::wstring& com_port, std::uint32_t baudrate, int timeout_ms);
    void Disconnect();
    [[nodiscard]] bool IsConnected() const noexcept;
    [[nodiscard]] const std::wstring& CurrentPort() const noexcept { return current_port_; }
    [[nodiscard]] std::uint32_t CurrentBaudrate() const noexcept { return baudrate_; }

    void SetOptions(const NuvoIspUartOptions& options);
    [[nodiscard]] const NuvoIspUartOptions& Options() const noexcept { return options_; }

    void ConnectTarget(const std::function<bool()>& should_cancel = {},
                       const std::function<void()>& on_poll = {});
    void SyncPacketNumber(const std::function<bool()>& should_cancel = {},
                          const std::function<void()>& on_poll = {});
    std::uint8_t GetFirmwareVersion();
    std::uint32_t GetDeviceId();

    void ProgramAprom(const std::vector<std::uint8_t>& image,
                      const std::function<void(const NuvoIspUartProgress&)>& on_progress,
                      const std::function<bool()>& should_cancel,
                      const std::function<void()>& on_poll);

    void RunAprom();
    void ResetTarget();

private:
    NuvoIspUartResponse Transact(std::uint32_t command,
                                 const std::vector<std::uint8_t>& payload,
                                 bool check_index,
                                 int response_timeout_ms,
                                 bool require_valid,
                                 const std::function<bool()>& should_cancel,
                                 const std::function<void()>& on_poll);
    void SendNoResponse(std::uint32_t command);
    std::vector<std::uint8_t> ReadExact(std::size_t length,
                                        int timeout_ms,
                                        const std::function<bool()>& should_cancel,
                                        const std::function<void()>& on_poll);
    void SendResendPacket(const std::function<bool()>& should_cancel,
                          const std::function<void()>& on_poll);

    static std::vector<std::uint8_t> BuildPacket(std::uint32_t command,
                                                 std::uint32_t packet_index,
                                                 const std::vector<std::uint8_t>& payload);
    static std::uint16_t Checksum(const std::vector<std::uint8_t>& bytes);
    static std::uint32_t ReadLe32(const std::vector<std::uint8_t>& bytes, std::size_t offset);
    static void AppendLe32(std::vector<std::uint8_t>* out, std::uint32_t value);
    static std::string ResponseErrorText(const NuvoIspUartResponse& response);
    static std::string WideToNarrow(const std::wstring& text);
    static void ThrowIfCancelled(const std::function<bool()>& should_cancel);

private:
    mfc_tool::vcom::SerialPort port_;
    NuvoIspUartOptions options_;
    std::wstring current_port_;
    std::uint32_t baudrate_ = 115200;
    int timeout_ms_ = 2000;
    std::uint32_t packet_index_ = 18;
};

} // namespace mfc_tool::core
