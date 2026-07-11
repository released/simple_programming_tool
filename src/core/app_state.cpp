#include "app_state.h"

#include <cwctype>

namespace mfc_tool::core {
namespace {

bool IsAbsolutePath(const std::wstring& path) {
    if (path.size() >= 2u && path[1] == L':') {
        return true;
    }
    return path.size() >= 2u &&
           ((path[0] == L'\\' && path[1] == L'\\') ||
            (path[0] == L'/' && path[1] == L'/'));
}

std::wstring LowerPath(std::wstring path) {
    for (wchar_t& ch : path) {
        if (ch == L'/') {
            ch = L'\\';
        } else {
            ch = static_cast<wchar_t>(towlower(ch));
        }
    }
    return path;
}

std::wstring ExeRootPath() {
    return config::IniManager::DefaultIniPath(L"");
}

std::wstring ToExeRelativePath(const std::wstring& path) {
    if (path.empty() || !IsAbsolutePath(path)) {
        return path;
    }

    const std::wstring root = ExeRootPath();
    const std::wstring lower_path = LowerPath(path);
    const std::wstring lower_root = LowerPath(root);
    if (lower_root.empty() || lower_path.find(lower_root) != 0u) {
        return path;
    }
    return path.substr(root.size());
}

std::wstring FromExeRelativePath(const std::wstring& path) {
    if (path.empty() || IsAbsolutePath(path)) {
        return path;
    }
    return config::IniManager::DefaultIniPath(path);
}

std::wstring GetValue(
    const config::IniData& data,
    const std::wstring& section,
    const std::wstring& key,
    const std::wstring& fallback
) {
    auto sec_it = data.find(section);
    if (sec_it == data.end()) {
        return fallback;
    }
    auto key_it = sec_it->second.find(key);
    if (key_it == sec_it->second.end()) {
        return fallback;
    }
    return key_it->second;
}

} // namespace

AppState AppState::Default() {
    AppState s;
    return s;
}

config::IniData AppState::ToIniData(const std::wstring& ini_path) const {
    config::IniData out;
    out[L"APP"] = {
        {L"ini_path", ToExeRelativePath(ini_path)}
    };

    out[L"UI"] = {
        {L"vid", ui.vid},
        {L"pid", ui.pid},
        {L"timeout_ms", ui.timeout_ms},
        {L"device_label", ui.device_label},
        {L"bridge_interface", ui.bridge_interface},
        {L"firmware_image_path", ToExeRelativePath(ui.firmware_image_path)},
        {L"vcom_port", ui.vcom_port},
        {L"vcom_baud", ui.vcom_baud},
        {L"save_log_checked", ui.save_log_checked ? L"1" : L"0"},
        {L"expected_fw_version", ui.expected_fw_version},
        {L"last_seen_fw_version", ui.last_seen_fw_version}
    };

    out[L"FW_UPLOAD"] = {
        {L"master_i2c_port", fw_upload.master_i2c_port},
        {L"master_i2c_pins", fw_upload.master_i2c_pins},
        {L"speed_hz", fw_upload.speed_hz},
        {L"target_addr", fw_upload.target_addr},
        {L"response_delay_ms", fw_upload.response_delay_ms}
    };

    out[L"UART_ISP"] = {
        {L"com_port", uart_isp.com_port},
        {L"baud", uart_isp.baud},
        {L"timeout_ms", uart_isp.timeout_ms}
    };

    out[L"XMODEM"] = {
        {L"com_port", xmodem.com_port},
        {L"baud", xmodem.baud},
        {L"handshake_timeout_ms", xmodem.handshake_timeout_ms}
    };

    out[L"USB_HID_ISP"] = {
        {L"vid", usb_hid_isp.vid},
        {L"pid", usb_hid_isp.pid},
        {L"timeout_ms", usb_hid_isp.timeout_ms},
        {L"device_label", usb_hid_isp.device_label}
    };

    return out;
}

void AppState::ApplyIniData(const config::IniData& data) {
    ui.vid = GetValue(data, L"UI", L"vid", ui.vid);
    ui.pid = GetValue(data, L"UI", L"pid", ui.pid);
    ui.timeout_ms = GetValue(data, L"UI", L"timeout_ms", ui.timeout_ms);
    ui.device_label = GetValue(data, L"UI", L"device_label", ui.device_label);
    ui.bridge_interface = GetValue(data, L"UI", L"bridge_interface", ui.bridge_interface);
    ui.firmware_image_path = FromExeRelativePath(GetValue(data, L"UI", L"firmware_image_path", ui.firmware_image_path));
    ui.vcom_port = GetValue(data, L"UI", L"vcom_port", ui.vcom_port);
    ui.vcom_baud = GetValue(data, L"UI", L"vcom_baud", ui.vcom_baud);
    ui.save_log_checked = GetValue(data, L"UI", L"save_log_checked", ui.save_log_checked ? L"1" : L"0") == L"1";
    ui.expected_fw_version = GetValue(data, L"UI", L"expected_fw_version", ui.expected_fw_version);
    ui.last_seen_fw_version = GetValue(data, L"UI", L"last_seen_fw_version", ui.last_seen_fw_version);

    fw_upload.master_i2c_port = GetValue(data, L"FW_UPLOAD", L"master_i2c_port", fw_upload.master_i2c_port);
    fw_upload.master_i2c_pins = GetValue(data, L"FW_UPLOAD", L"master_i2c_pins", fw_upload.master_i2c_pins);
    fw_upload.speed_hz = GetValue(data, L"FW_UPLOAD", L"speed_hz", fw_upload.speed_hz);
    fw_upload.target_addr = GetValue(data, L"FW_UPLOAD", L"target_addr", fw_upload.target_addr);
    fw_upload.response_delay_ms = GetValue(data, L"FW_UPLOAD", L"response_delay_ms", fw_upload.response_delay_ms);
    fw_upload.image_path = FromExeRelativePath(GetValue(data, L"FW_UPLOAD", L"image_path", fw_upload.image_path));
    if (ui.firmware_image_path.empty() && !fw_upload.image_path.empty()) {
        ui.firmware_image_path = fw_upload.image_path;
    }

    uart_isp.com_port = GetValue(data, L"UART_ISP", L"com_port", uart_isp.com_port);
    uart_isp.baud = GetValue(data, L"UART_ISP", L"baud", uart_isp.baud);
    uart_isp.timeout_ms = GetValue(data, L"UART_ISP", L"timeout_ms", uart_isp.timeout_ms);
    uart_isp.image_path = FromExeRelativePath(GetValue(data, L"UART_ISP", L"image_path", uart_isp.image_path));
    if (ui.firmware_image_path.empty() && !uart_isp.image_path.empty()) {
        ui.firmware_image_path = uart_isp.image_path;
    }

    xmodem.com_port = GetValue(data, L"XMODEM", L"com_port", xmodem.com_port);
    xmodem.baud = GetValue(data, L"XMODEM", L"baud", xmodem.baud);
    xmodem.handshake_timeout_ms = GetValue(data, L"XMODEM", L"handshake_timeout_ms", xmodem.handshake_timeout_ms);
    xmodem.image_path = FromExeRelativePath(GetValue(data, L"XMODEM", L"image_path", xmodem.image_path));
    if (ui.firmware_image_path.empty() && !xmodem.image_path.empty()) {
        ui.firmware_image_path = xmodem.image_path;
    }

    usb_hid_isp.vid = GetValue(data, L"USB_HID_ISP", L"vid", usb_hid_isp.vid);
    usb_hid_isp.pid = GetValue(data, L"USB_HID_ISP", L"pid", usb_hid_isp.pid);
    usb_hid_isp.timeout_ms = GetValue(data, L"USB_HID_ISP", L"timeout_ms", usb_hid_isp.timeout_ms);
    usb_hid_isp.device_label = GetValue(data, L"USB_HID_ISP", L"device_label", usb_hid_isp.device_label);
    usb_hid_isp.image_path = FromExeRelativePath(GetValue(data, L"USB_HID_ISP", L"image_path", usb_hid_isp.image_path));
    if (ui.firmware_image_path.empty() && !usb_hid_isp.image_path.empty()) {
        ui.firmware_image_path = usb_hid_isp.image_path;
    }
}

} // namespace mfc_tool::core
