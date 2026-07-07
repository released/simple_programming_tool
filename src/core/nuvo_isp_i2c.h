#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "bridge_service.h"

namespace mfc_tool::core {

struct NuvoIspI2cOptions {
    int port = 0;
    int addr7 = 0x60;
    int response_delay_ms = 2;
    int response_poll_ms = 2;
    int normal_response_timeout_ms = 5000;
    int long_response_timeout_ms = 25000;
    int program_retry_count = 10;
};

struct NuvoIspI2cResponse {
    std::vector<std::uint8_t> bytes;
    std::uint16_t expected_checksum = 0;
    std::uint16_t response_checksum = 0;
    std::uint32_t expected_index = 0;
    std::uint32_t response_index = 0;
    bool checksum_ok = false;
    bool index_ok = false;
};

struct NuvoIspI2cProgress {
    std::uint32_t bytes_done = 0;
    std::uint32_t total_bytes = 0;
    std::uint32_t packet_index = 0;
};

class NuvoIspI2cClient {
public:
    explicit NuvoIspI2cClient(BridgeService* service);

    void SetOptions(const NuvoIspI2cOptions& options);
    [[nodiscard]] const NuvoIspI2cOptions& Options() const noexcept { return options_; }

    void ConnectTarget(const std::function<bool()>& should_cancel = {},
                       const std::function<void()>& on_poll = {});
    void SyncPacketNumber(const std::function<bool()>& should_cancel = {},
                          const std::function<void()>& on_poll = {});
    std::uint8_t GetFirmwareVersion();
    std::uint32_t GetDeviceId();

    void ProgramAprom(const std::vector<std::uint8_t>& image,
                      const std::function<void(const NuvoIspI2cProgress&)>& on_progress,
                      const std::function<bool()>& should_cancel,
                      const std::function<void()>& on_poll);

    void RunAprom();
    void ResetTarget();

private:
    NuvoIspI2cResponse Transact(std::uint32_t command,
                                const std::vector<std::uint8_t>& payload,
                                bool check_index,
                                int response_timeout_ms,
                                bool require_valid,
                                const std::function<bool()>& should_cancel,
                                const std::function<void()>& on_poll);
    void SendNoResponse(std::uint32_t command);
    NuvoIspI2cResponse ReadResponse(std::uint16_t expected_checksum,
                                    std::uint32_t expected_index,
                                    bool check_index,
                                    int response_timeout_ms,
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
    static bool IsBusyPattern(const std::vector<std::uint8_t>& bytes);
    static std::string ResponseErrorText(const NuvoIspI2cResponse& response);
    static void ThrowIfCancelled(const std::function<bool()>& should_cancel);

private:
    BridgeService* service_ = nullptr;
    NuvoIspI2cOptions options_;
    std::uint32_t packet_index_ = 18;
};

} // namespace mfc_tool::core
