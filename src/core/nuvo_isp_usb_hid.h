#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include <array>

#include "../hid/hid_types.h"

namespace mfc_tool::core {

struct NuvoIspUsbHidOptions {
    int response_poll_ms = 20;
    int normal_response_timeout_ms = 5000;
    int long_response_timeout_ms = 25000;
    int program_retry_count = 10;
};

struct NuvoIspUsbHidResponse {
    std::vector<std::uint8_t> bytes;
    std::uint16_t expected_checksum = 0;
    std::uint16_t response_checksum = 0;
    std::uint32_t expected_index = 0;
    std::uint32_t response_index = 0;
    bool checksum_ok = false;
    bool index_ok = false;
};

struct NuvoIspUsbHidProgress {
    std::uint32_t bytes_done = 0;
    std::uint32_t total_bytes = 0;
    std::uint32_t packet_index = 0;
};

class NuvoIspUsbHidClient {
public:
    NuvoIspUsbHidClient() = default;
    ~NuvoIspUsbHidClient();

    NuvoIspUsbHidClient(const NuvoIspUsbHidClient&) = delete;
    NuvoIspUsbHidClient& operator=(const NuvoIspUsbHidClient&) = delete;

    void Connect(std::uint16_t vid, std::uint16_t pid, const std::wstring& path, int timeout_ms);
    void Disconnect();
    [[nodiscard]] bool IsConnected() const noexcept;
    [[nodiscard]] const mfc_tool::hid::DeviceInfo& CurrentDevice() const noexcept { return device_; }

    void SetOptions(const NuvoIspUsbHidOptions& options);
    [[nodiscard]] const NuvoIspUsbHidOptions& Options() const noexcept { return options_; }

    bool TryConnectTargetOnce(int response_timeout_ms,
                              const std::function<bool()>& should_cancel = {},
                              const std::function<void()>& on_poll = {},
                              std::string* error_text = nullptr);
    void ConnectTarget(const std::function<bool()>& should_cancel = {},
                       const std::function<void()>& on_poll = {});
    void SyncPacketNumber(const std::function<bool()>& should_cancel = {},
                          const std::function<void()>& on_poll = {});
    std::uint8_t GetFirmwareVersion();
    std::uint32_t GetDeviceId();
    std::array<std::uint32_t, 4> ReadConfig();

    void ProgramAprom(const std::vector<std::uint8_t>& image,
                      const std::function<void(const NuvoIspUsbHidProgress&)>& on_progress,
                      const std::function<bool()>& should_cancel,
                      const std::function<void()>& on_poll);

    void RunAprom();
    void ResetTarget();

private:
    void OpenHandle(const std::wstring& path);
    void PopulateDeviceMeta();

    NuvoIspUsbHidResponse Transact(std::uint32_t command,
                                   const std::vector<std::uint8_t>& payload,
                                   bool check_index,
                                   int response_timeout_ms,
                                   bool require_valid,
                                   const std::function<bool()>& should_cancel,
                                   const std::function<void()>& on_poll);
    void SendNoResponse(std::uint32_t command);
    std::vector<std::uint8_t> ReadPacket(std::uint16_t expected_checksum,
                                         std::uint32_t expected_index,
                                         bool check_index,
                                         int timeout_ms,
                                         const std::function<bool()>& should_cancel,
                                         const std::function<void()>& on_poll);
    std::vector<std::uint8_t> BuildWriteReport(const std::vector<std::uint8_t>& packet) const;
    void SendResendPacket(const std::function<bool()>& should_cancel,
                          const std::function<void()>& on_poll);

    static std::vector<std::uint8_t> BuildPacket(std::uint32_t command,
                                                 std::uint32_t packet_index,
                                                 const std::vector<std::uint8_t>& payload);
    static std::uint16_t Checksum(const std::vector<std::uint8_t>& bytes);
    static std::uint16_t ReadLe16(const std::uint8_t* bytes);
    static std::uint32_t ReadLe32(const std::vector<std::uint8_t>& bytes, std::size_t offset);
    static std::uint32_t ReadLe32Ptr(const std::uint8_t* bytes);
    static void AppendLe32(std::vector<std::uint8_t>* out, std::uint32_t value);
    static std::string ResponseErrorText(const NuvoIspUsbHidResponse& response);
    static std::string WideToNarrow(const std::wstring& text);
    static void ThrowIfCancelled(const std::function<bool()>& should_cancel);
    static bool IsLegacyNuvotonHid(std::uint16_t requested_vid,
                                   std::uint16_t requested_pid,
                                   std::uint16_t actual_pid);
    static std::string LastErrorMessage(const char* prefix);

private:
    void* handle_ = nullptr;
    mfc_tool::hid::DeviceInfo device_ = {};
    NuvoIspUsbHidOptions options_;
    int timeout_ms_ = 2000;
    std::uint32_t packet_index_ = 18u;
};

} // namespace mfc_tool::core
