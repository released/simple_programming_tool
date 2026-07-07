#pragma once

#include <afxcmn.h>
#include <afxwin.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "../core/app_state.h"
#include "../core/xmodem_sender.h"

class CXmodemTab : public CWnd {
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
    afx_msg void OnSendXmodem();
    afx_msg void OnCancel();
    DECLARE_MESSAGE_MAP()

private:
    void LayoutControls(const CRect& r);
    void RefreshComPorts();
    void PopulateBaudCombo();
    void UpdateEnableState();
    void SaveVisibleToState();
    void LoadVisibleFromState();
    void EnsureConnected();
    void LoadImageFromPath(const std::wstring& path);
    void SetImagePathText();
    void SetStatusText(const std::wstring& text);
    void AppendStatusText(const std::wstring& text);
    void SetProgress(int value, const std::wstring& text);
    void PumpUiMessages();

    std::wstring SelectedComPort() const;
    std::uint32_t SelectedBaudrate() const;
    int CurrentHandshakeTimeoutMs() const;
    std::wstring GetEditText(const CEdit& edit) const;
    void ApplyComboSelection(CComboBox& combo, const std::wstring& value);
    static std::wstring AnsiToWide(const char* text);

private:
    std::function<void(const std::wstring&)> log_;
    std::function<const std::vector<std::uint8_t>&()> firmware_image_data_;
    std::function<std::wstring()> firmware_image_path_;
    std::function<void()> persist_settings_;
    mfc_tool::core::XmodemSender sender_;
    mfc_tool::core::XmodemState state_;
    std::vector<std::uint8_t> image_data_;

    bool loading_ = false;
    bool busy_ = false;
    bool cancel_requested_ = false;

    CFont ui_font_;
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
    CButton send_xmodem_btn_;
    CButton cancel_btn_;
    CStatic transfer_state_label_;
    CProgressCtrl progress_;
    CStatic progress_label_;

    CButton status_group_;
    CEdit status_edit_;
};
