#pragma once

#include <afxcmn.h>
#include <afxwin.h>

#include <cstdint>
#include <string>
#include <vector>

#include "config/ini_manager.h"
#include "core/app_state.h"
#include "core/bridge_service.h"
#include "core/pin_usage_registry.h"
#include "log/logger.h"
#include "ui/fw_upload_tab.h"
#include "ui/uart_isp_tab.h"
#include "ui/xmodem_tab.h"

class CMainFrame : public CFrameWnd {
public:
    CMainFrame();

protected:
    BOOL PreCreateWindow(CREATESTRUCT& cs) override;
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnTabSelChange(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnBnClickedLoadImage();
    afx_msg void OnBnClickedSaveIni();
    afx_msg void OnBnClickedLoadIni();
    afx_msg void OnBnClickedResetIni();
    afx_msg void OnBnClickedSaveLog();
    afx_msg void OnBnClickedSaveLogCheck();
    afx_msg void OnBnClickedClearLog();
    afx_msg void OnClose();

    DECLARE_MESSAGE_MAP()

private:
    void LayoutControls(int cx, int cy);
    void InitializeTabs();
    void ShowActiveTabPage();
    void AppendLog(const std::wstring& text);
    void TrimVisibleLogIfNeeded();
    void UpdateIniPathUi();
    void SetFirmwareImagePathText();
    bool LoadFirmwareImageFromPath(const std::wstring& path, bool show_errors);
    void RefreshSharedImageState();
    void LoadIni();
    void SaveIni();
    void SaveIniTo(const std::wstring& path);
    bool TryLoadIniFrom(const std::wstring& path);
    void ResetIniToDefault();
    void SaveLogToFile(const std::wstring& path);
    std::wstring GetControlText(const CWnd& w) const;
    void SetControlText(CWnd& w, const std::wstring& text);
    static void ShowErrorBox(const std::wstring& title, const std::wstring& message);
    static std::wstring AnsiToWide(const char* text);

private:
    CFont ui_font_;

    CButton image_group_;
    CStatic image_path_label_;
    CEdit image_path_edit_;
    CButton load_image_btn_;
    CStatic image_info_label_;

    CButton save_ini_btn_;
    CButton load_ini_btn_;
    CButton reset_ini_btn_;
    CStatic ini_path_title_;
    CStatic ini_path_value_;
    CStatic build_info_title_;
    CStatic build_info_value_;

    CTabCtrl tab_ctrl_;
    CFwUploadTab fw_upload_tab_;
    CUartIspTab uart_isp_tab_;
    CXmodemTab xmodem_tab_;
    CButton save_log_check_;
    CButton save_log_btn_;
    CButton clear_log_btn_;
    CEdit log_edit_;

    std::vector<std::uint8_t> firmware_image_data_;
    mfc_tool::config::IniManager ini_;
    mfc_tool::core::AppState state_;
    mfc_tool::core::BridgeService service_;
    mfc_tool::core::PinUsageRegistry pin_usage_;
    mfc_tool::log::Logger logger_;
};
