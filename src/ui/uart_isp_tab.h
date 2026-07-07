#pragma once

#include <afxcmn.h>
#include <afxwin.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "../core/app_state.h"
#include "../core/nuvo_isp_uart.h"

class CUartIspTab : public CWnd {
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

protected:
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnRefreshCom();
    afx_msg void OnOpenCom();
    afx_msg void OnCloseCom();
    afx_msg void OnLoadImage();
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
    void RefreshComPorts();
    void PopulateBaudCombo();
    void UpdateEnableState();
    void SaveVisibleToState();
    void LoadVisibleFromState();
    void EnsureUartConnected();
    void LoadImageFromPath(const std::wstring& path);
    void SetImagePathText();
    void SetStatusText(const std::wstring& text);
    void AppendStatusText(const std::wstring& text);
    void SetProgress(int value, const std::wstring& text);
    void SetTargetReady(bool ready, const std::wstring& text = L"");
    void PumpUiMessages();

    std::wstring SelectedComPort() const;
    std::uint32_t SelectedBaudrate() const;
    int CurrentTimeoutMs() const;
    std::wstring GetEditText(const CEdit& edit) const;
    void ApplyComboSelection(CComboBox& combo, const std::wstring& value);
    static std::wstring AnsiToWide(const char* text);
    static std::wstring FormatHex32(std::uint32_t value);

private:
    std::function<void(const std::wstring&)> log_;
    std::function<const std::vector<std::uint8_t>&()> firmware_image_data_;
    std::function<std::wstring()> firmware_image_path_;
    std::function<void()> persist_settings_;
    mfc_tool::core::NuvoIspUartClient client_;
    mfc_tool::core::UartIspState state_;
    std::vector<std::uint8_t> image_data_;

    bool loading_ = false;
    bool busy_ = false;
    bool cancel_requested_ = false;
    bool target_ready_ = false;

    CFont ui_font_;
    CBrush target_ready_brush_;
    CBrush target_idle_brush_;
    CButton conn_group_;
    CStatic com_label_;
    CComboBox com_combo_;
    CStatic baud_label_;
    CComboBox baud_combo_;
    CStatic timeout_label_;
    CEdit timeout_edit_;
    CButton refresh_com_btn_;
    CButton open_com_btn_;
    CButton close_com_btn_;

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

    CButton status_group_;
    CEdit status_edit_;
};
