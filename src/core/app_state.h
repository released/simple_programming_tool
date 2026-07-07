#pragma once

#include <string>

#include "../config/ini_manager.h"
#include "fw_version.generated.h"

namespace mfc_tool::core {

struct UiState {
    std::wstring vid = L"0x0416";
    std::wstring pid = L"0x5020";
    std::wstring timeout_ms = L"2000";
    std::wstring device_label = L"Auto Select";
    std::wstring bridge_interface = L"HID";
    std::wstring firmware_image_path;
    std::wstring vcom_port;
    std::wstring vcom_baud = L"115200";
    bool save_log_checked = false;
    std::wstring expected_fw_version = M032_EXPECTED_FW_VERSION;
    std::wstring last_seen_fw_version = L"-";
};

struct FwUploadState {
    std::wstring master_i2c_port = L"0";
    std::wstring master_i2c_pins = L"I2C0_PB4_PB5";
    std::wstring speed_hz = L"100000";
    std::wstring target_addr = L"0x60";
    std::wstring response_delay_ms = L"2";
    std::wstring image_path;
};

struct UartIspState {
    std::wstring com_port;
    std::wstring baud = L"115200";
    std::wstring timeout_ms = L"2000";
    std::wstring image_path;
};

struct XmodemState {
    std::wstring com_port;
    std::wstring baud = L"115200";
    std::wstring handshake_timeout_ms = L"60000";
    std::wstring image_path;
};

struct AppState {
    UiState ui;
    FwUploadState fw_upload;
    UartIspState uart_isp;
    XmodemState xmodem;

    static AppState Default();

    config::IniData ToIniData(const std::wstring& ini_path) const;
    void ApplyIniData(const config::IniData& data);
};

} // namespace mfc_tool::core
