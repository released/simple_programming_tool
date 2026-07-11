#pragma once

#include <afxcmn.h>
#include <afxwin.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "../core/app_state.h"
#include "../core/board_i2c_catalog.h"
#include "../core/bridge_service.h"
#include "../core/pin_usage_registry.h"

class CFwUploadTab : public CWnd {
public:
    BOOL Create(CWnd* parent, const RECT& rect, UINT id);

    void Bind(mfc_tool::core::BridgeService* service,
              std::function<void(const std::wstring&)> logger,
              mfc_tool::core::PinUsageRegistry* pin_usage,
              std::function<const std::vector<std::uint8_t>&()> firmware_image_data,
              std::function<std::wstring()> firmware_image_path,
              std::function<void()> persist_settings = {});
    void SetConnected(bool connected);
    void OnDisconnected();
    void RefreshSharedImageState();
    void RefreshDpiLayout();

    void LoadState(const mfc_tool::core::AppState& state);
    void SaveState(mfc_tool::core::AppState* state) const;

protected:
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnPortChanged();
    afx_msg void OnPinsChanged();
    afx_msg void OnEnableHost();
    afx_msg void OnDisableHost();
    afx_msg void OnLoadImage();
    afx_msg void OnConnectTarget();
    afx_msg void OnGetDeviceId();
    afx_msg void OnProgram();
    afx_msg void OnRunAprom();
    afx_msg void OnResetTarget();
    afx_msg void OnStop();
    afx_msg void OnBridgeRefreshHid();
    afx_msg void OnBridgeConnectHid();
    afx_msg void OnBridgeDisconnectHid();
    afx_msg void OnBridgeGetInfo();
    afx_msg void OnBridgePing();
    afx_msg void OnBridgeResetMcu();
    afx_msg void OnBridgeRefreshVcom();
    afx_msg void OnBridgeConnectVcom();
    afx_msg void OnBridgeDisconnectVcom();
    afx_msg void OnBridgeSelectHid();
    afx_msg void OnBridgeSelectVcom();
    afx_msg void OnBridgeVcomChanged();
    afx_msg void OnBridgeVcomBaudChanged();
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
    DECLARE_MESSAGE_MAP()

private:
    void LayoutControls(const CRect& r);
    void RefreshDevices();
    void RefreshVcomPorts();
    void SetBridgeConnectionUi();
    void SetBridgeInterfaceSelection(bool hid_selected);
    bool IsHidInterfaceSelected() const;
    void UpdateBridgeStatusChipColor();
    void UpdateFirmwareInfoUi();
    void SaveBridgeVisibleToState();
    void LoadBridgeVisibleFromState();
    std::wstring SelectedDevicePath() const;
    std::wstring SelectedVcomPort() const;
    std::uint32_t SelectedVcomBaudrate() const;
    void ApplyComboSelection(CComboBox& combo, const std::wstring& value);
    static std::wstring DeviceLabel(const mfc_tool::hid::DeviceInfo& d, int index);
    void LogBridgeInfoPayload(const std::vector<std::uint8_t>& rx, bool from_connect);
    static std::wstring ResetReasonText(std::uint8_t reason);
    static std::wstring ExtractFirmwareVersion(const std::wstring& bridge_name);
    static bool FirmwareVersionMatch(const std::wstring& expected, const std::wstring& actual);
    void PopulatePortCombo();
    void PopulatePinCombo(const std::wstring& preferred_ini_name = L"");
    void UpdateEnableState();
    void RefreshPinUsage();
    void SaveVisibleToState();
    void LoadVisibleFromState();
    void EnsureHostEnabled();
    void SetHostEnabled(bool enabled);
    void LoadImageFromPath(const std::wstring& path);
    void SetImagePathText();
    void SetStatusText(const std::wstring& text);
    void AppendStatusText(const std::wstring& text);
    void SetProgress(int value, const std::wstring& text);
    void SetTargetReady(bool ready, const std::wstring& text = L"");
    void PumpUiMessages();

    int CurrentPort() const;
    int CurrentAddress() const;
    int CurrentSpeedHz() const;
    int CurrentResponseDelayMs() const;
    std::wstring GetEditText(const CEdit& edit) const;
    const mfc_tool::core::board_i2c::PinPair& CurrentPinPair() const;
    static std::wstring OwnerId();
    static std::vector<std::wstring> SharedOwnerIds();
    static std::wstring AnsiToWide(const char* text);
    static std::wstring FormatHex32(std::uint32_t value);

private:
    mfc_tool::core::BridgeService* service_ = nullptr;
    mfc_tool::core::PinUsageRegistry* pin_usage_ = nullptr;
    std::function<void(const std::wstring&)> log_;
    std::function<const std::vector<std::uint8_t>&()> firmware_image_data_;
    std::function<std::wstring()> firmware_image_path_;
    std::function<void()> persist_settings_;

    bool connected_ = false;
    bool loading_ = false;
    bool host_enabled_ = false;
    bool busy_ = false;
    bool cancel_requested_ = false;
    bool target_ready_ = false;

    mfc_tool::core::UiState ui_state_;
    mfc_tool::core::FwUploadState state_;
    std::vector<std::uint8_t> image_data_;
    std::vector<mfc_tool::hid::DeviceInfo> scanned_devices_;

    CFont ui_font_;
    CBrush bridge_status_brush_;
    COLORREF bridge_status_color_ = RGB(95, 104, 117);
    COLORREF bridge_status_text_color_ = RGB(255, 255, 255);
    std::wstring expected_fw_version_ = M032_EXPECTED_FW_VERSION;
    std::wstring current_fw_version_ = L"-";
    bool fw_version_match_ = true;

    CButton hid_group_;
    CButton hid_check_;
    CStatic vid_label_;
    CEdit vid_edit_;
    CStatic pid_label_;
    CEdit pid_edit_;
    CStatic timeout_label_;
    CEdit timeout_edit_;
    CButton refresh_btn_;
    CButton connect_btn_;
    CButton disconnect_btn_;
    CStatic device_label_;
    CComboBox device_combo_;

    CButton vcom_group_;
    CButton vcom_check_;
    CStatic vcom_label_;
    CComboBox vcom_combo_;
    CStatic vcom_baud_label_;
    CComboBox vcom_baud_combo_;
    CButton refresh_vcom_btn_;
    CButton connect_vcom_btn_;
    CButton disconnect_vcom_btn_;

    CStatic bridge_status_title_;
    CStatic bridge_status_chip_;
    CButton get_info_btn_;
    CButton ping_btn_;
    CButton reset_mcu_btn_;
    CStatic fw_info_title_;
    CStatic fw_info_value_;

    CButton config_group_;
    CStatic port_label_;
    CComboBox port_combo_;
    CStatic pins_label_;
    CComboBox pins_combo_;
    CStatic speed_label_;
    CEdit speed_edit_;
    CStatic addr_label_;
    CEdit addr_edit_;
    CStatic delay_label_;
    CEdit delay_edit_;
    CButton enable_host_btn_;
    CButton disable_host_btn_;

    CButton file_group_;
    CStatic path_label_;
    CEdit path_edit_;
    CButton load_image_btn_;
    CStatic file_info_label_;

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
    CBrush target_ready_brush_;
    CBrush target_idle_brush_;

    CButton status_group_;
    CEdit status_edit_;
};
