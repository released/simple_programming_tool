#pragma once

#include <afxcmn.h>
#include <afxwin.h>

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "../core/app_state.h"
#include "../core/nuvo_isp_usb_hid.h"
#include "../hid/hid_types.h"

class CUsbHidIspTab : public CWnd {
public:
    BOOL Create(CWnd* parent, const RECT& rect, UINT id);

    void Bind(std::function<void(const std::wstring&)> logger,
              std::function<const std::vector<std::uint8_t>&()> firmware_image_data,
              std::function<std::wstring()> firmware_image_path,
              std::function<void()> persist_settings = {});
    void LoadState(const mfc_tool::core::AppState& state);
    void SaveState(mfc_tool::core::AppState* state) const;
    void OnShutdown();
    void RefreshSharedImageState();
    void RefreshDpiLayout();

protected:
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnRefreshHid();
    afx_msg void OnConnectHid();
    afx_msg void OnDisconnectHid();
    afx_msg void OnConnectTarget();
    afx_msg void OnGetDeviceId();
    afx_msg void OnProgram();
    afx_msg void OnRunAprom();
    afx_msg void OnResetTarget();
    afx_msg void OnStop();
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
    DECLARE_MESSAGE_MAP()

private:
    void LayoutControls(const CRect& r);
    void RefreshDevices();
    void UpdateEnableState();
    void SaveVisibleToState();
    void LoadVisibleFromState();
    void EnsureHidConnected();
    struct BootloaderConnectResult {
        std::uint8_t fw_version = 0;
        std::uint32_t device_id = 0;
        std::array<std::uint32_t, 4> config = {};
        bool config_valid = false;
    };
    BootloaderConnectResult ConnectBootloaderFlow();
    void SetStatusText(const std::wstring& text);
    void AppendStatusText(const std::wstring& text);
    void SetProgress(int value, const std::wstring& text);
    void SetTargetReady(bool ready, const std::wstring& text = L"");
    void PumpUiMessages();

    std::uint16_t CurrentVid() const;
    std::uint16_t CurrentPid() const;
    int CurrentTimeoutMs() const;
    std::wstring SelectedDevicePath() const;
    std::wstring GetEditText(const CEdit& edit) const;
    void ApplyComboSelection(CComboBox& combo, const std::wstring& value);
    static std::wstring DeviceLabel(const mfc_tool::hid::DeviceInfo& d, int index);
    static std::wstring AnsiToWide(const char* text);
    static std::wstring FormatHex32(std::uint32_t value);

private:
    std::function<void(const std::wstring&)> log_;
    std::function<const std::vector<std::uint8_t>&()> firmware_image_data_;
    std::function<std::wstring()> firmware_image_path_;
    std::function<void()> persist_settings_;
    mfc_tool::core::NuvoIspUsbHidClient client_;
    mfc_tool::core::UsbHidIspState state_;
    std::vector<mfc_tool::hid::DeviceInfo> scanned_devices_;

    bool loading_ = false;
    bool busy_ = false;
    bool cancel_requested_ = false;
    bool target_ready_ = false;

    CFont ui_font_;
    CBrush target_ready_brush_;
    CBrush target_idle_brush_;
    CButton conn_group_;
    CStatic vid_label_;
    CEdit vid_edit_;
    CStatic pid_label_;
    CEdit pid_edit_;
    CStatic timeout_label_;
    CEdit timeout_edit_;
    CStatic device_label_;
    CComboBox device_combo_;
    CButton refresh_hid_btn_;
    CButton connect_hid_btn_;
    CButton disconnect_hid_btn_;

    CButton action_group_;
    CButton connect_target_btn_;
    CButton get_device_btn_;
    CButton program_btn_;
    CButton run_aprom_btn_;
    CButton reset_target_btn_;
    CButton stop_btn_;
    CStatic target_state_label_;
    CProgressCtrl progress_;
    CStatic progress_label_;

    CButton status_group_;
    CEdit status_edit_;
};
