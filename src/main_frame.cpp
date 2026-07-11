#include "main_frame.h"

#include <afxdlgs.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "build_info.generated.h"
#include "core/text_utils.h"
#include "resource.h"
#include "ui/layout_utils.h"

namespace {

constexpr int kUiLogEditLimitChars = 512 * 1024;
constexpr int kUiLogTrimThresholdChars = 160 * 1024;
constexpr int kUiLogTrimTargetChars = 120 * 1024;

std::wstring NowTimeText() {
    SYSTEMTIME st = {};
    GetLocalTime(&st);

    wchar_t buf[32] = {};
    swprintf_s(buf, L"%02u:%02u:%02u", st.wHour, st.wMinute, st.wSecond);
    return std::wstring(buf);
}

} // namespace

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWnd)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_MESSAGE(WM_DPICHANGED, &CMainFrame::OnDpiChanged)
    ON_WM_GETMINMAXINFO()
    ON_NOTIFY(TCN_SELCHANGE, ID_TAB_CTRL, &CMainFrame::OnTabSelChange)
    ON_BN_CLICKED(ID_TOP_LOAD_IMAGE_BTN, &CMainFrame::OnBnClickedLoadImage)
    ON_BN_CLICKED(ID_TOP_SAVE_INI_BTN, &CMainFrame::OnBnClickedSaveIni)
    ON_BN_CLICKED(ID_TOP_LOAD_INI_BTN, &CMainFrame::OnBnClickedLoadIni)
    ON_BN_CLICKED(ID_TOP_RESET_INI_BTN, &CMainFrame::OnBnClickedResetIni)
    ON_BN_CLICKED(ID_LOG_SAVE_BTN, &CMainFrame::OnBnClickedSaveLog)
    ON_BN_CLICKED(ID_LOG_SAVE_CHECK, &CMainFrame::OnBnClickedSaveLogCheck)
    ON_BN_CLICKED(ID_LOG_CLEAR_BTN, &CMainFrame::OnBnClickedClearLog)
    ON_WM_CLOSE()
END_MESSAGE_MAP()

CMainFrame::CMainFrame()
    : ini_(mfc_tool::config::IniManager::DefaultIniPath(L"simple_programming_tool.ini")),
      state_(mfc_tool::core::AppState::Default()) {
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs) {
    if (!CFrameWnd::PreCreateWindow(cs)) {
        return FALSE;
    }
    cs.style |= WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
    HICON app_icon = AfxGetApp()->LoadIconW(IDR_MAINFRAME);
    cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW, ::LoadCursor(nullptr, IDC_ARROW),
                                       reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1), app_icon);
    return TRUE;
}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) {
    if (CFrameWnd::OnCreate(lpCreateStruct) == -1) {
        return -1;
    }

    current_dpi_ = mfc_tool::ui::GetDpiForWnd(*this);
    RecreateUiFont();
    HICON app_icon = AfxGetApp()->LoadIconW(IDR_MAINFRAME);
    SetIcon(app_icon, TRUE);
    SetIcon(app_icon, FALSE);

    auto create_or_fail = [this](BOOL ok, const wchar_t* name) -> bool {
        if (!ok) {
            AppendLog(std::wstring(L"Create control failed: ") + name);
            return false;
        }
        return true;
    };

    if (!create_or_fail(image_group_.Create(L"Firmware Image", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, CRect(), this, ID_TOP_IMAGE_GROUP), L"Firmware Image group")) return -1;
    if (!create_or_fail(image_path_label_.Create(L"Path", WS_CHILD | WS_VISIBLE, CRect(), this, ID_TOP_IMAGE_PATH_LABEL), L"Firmware Image path label")) return -1;
    if (!create_or_fail(image_path_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_READONLY, CRect(), this, ID_TOP_IMAGE_PATH_EDIT), L"Firmware Image path edit")) return -1;
    if (!create_or_fail(load_image_btn_.Create(L"Load BIN", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, ID_TOP_LOAD_IMAGE_BTN), L"Load BIN")) return -1;
    if (!create_or_fail(image_info_label_.Create(L"No image loaded", WS_CHILD | WS_VISIBLE, CRect(), this, ID_TOP_IMAGE_INFO_LABEL), L"Firmware Image info label")) return -1;

    if (!create_or_fail(save_ini_btn_.Create(L"Save INI", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, ID_TOP_SAVE_INI_BTN), L"Save INI")) return -1;
    if (!create_or_fail(load_ini_btn_.Create(L"Load INI", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, ID_TOP_LOAD_INI_BTN), L"Load INI")) return -1;
    if (!create_or_fail(reset_ini_btn_.Create(L"Reset INI", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, ID_TOP_RESET_INI_BTN), L"Reset INI")) return -1;
    if (!create_or_fail(ini_path_title_.Create(L"INI:", WS_CHILD | WS_VISIBLE, CRect(), this, ID_TOP_INI_PATH_TITLE), L"INI path title")) return -1;
    if (!create_or_fail(ini_path_value_.Create(L"-", WS_CHILD | WS_VISIBLE | SS_PATHELLIPSIS, CRect(), this, ID_TOP_INI_PATH_VALUE), L"INI path value")) return -1;
    if (!create_or_fail(build_info_title_.Create(L"Build:", WS_CHILD | WS_VISIBLE, CRect(), this, ID_TOP_BUILD_INFO_TITLE), L"Build info title")) return -1;
    if (!create_or_fail(build_info_value_.Create(L"-", WS_CHILD | WS_VISIBLE | SS_PATHELLIPSIS, CRect(), this, ID_TOP_BUILD_INFO_VALUE), L"Build info value")) return -1;
    if (!create_or_fail(tab_ctrl_.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | TCS_FIXEDWIDTH, CRect(), this, ID_TAB_CTRL), L"Tab ctrl")) return -1;
    if (!create_or_fail(save_log_check_.Create(L"Save Log", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, CRect(), this, ID_LOG_SAVE_CHECK), L"Save Log checkbox")) return -1;
    if (!create_or_fail(save_log_btn_.Create(L"Save Log", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, ID_LOG_SAVE_BTN), L"Save Log button")) return -1;
    if (!create_or_fail(clear_log_btn_.Create(L"Clear Log", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, ID_LOG_CLEAR_BTN), L"Clear Log button")) return -1;
    if (!create_or_fail(log_edit_.Create(WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_BORDER,
                                         CRect(), this, ID_LOG_EDIT), L"Log edit")) return -1;
    log_edit_.LimitText(kUiLogEditLimitChars);

    CRect tab_dummy(0, 0, 100, 100);
    if (!fw_upload_tab_.Create(&tab_ctrl_, tab_dummy, ID_TAB_FW_UPLOAD)) return -1;
    if (!uart_isp_tab_.Create(&tab_ctrl_, tab_dummy, ID_TAB_UART_ISP)) return -1;
    if (!xmodem_tab_.Create(&tab_ctrl_, tab_dummy, ID_TAB_XMODEM)) return -1;
    if (!usb_hid_isp_tab_.Create(&tab_ctrl_, tab_dummy, ID_TAB_USB_HID_ISP)) return -1;

    auto tab_log = [this](const std::wstring& msg) { AppendLog(msg); };
    auto firmware_image_data = [this]() -> const std::vector<std::uint8_t>& { return firmware_image_data_; };
    auto firmware_image_path = [this]() -> std::wstring { return state_.ui.firmware_image_path; };
    fw_upload_tab_.Bind(&service_, tab_log, &pin_usage_, firmware_image_data, firmware_image_path, [this]() { SaveIni(); });
    uart_isp_tab_.Bind(tab_log, firmware_image_data, firmware_image_path, [this]() { SaveIni(); });
    xmodem_tab_.Bind(tab_log, firmware_image_data, firmware_image_path, [this]() { SaveIni(); });
    usb_hid_isp_tab_.Bind(tab_log, firmware_image_data, firmware_image_path, [this]() { SaveIni(); });

    ApplyTopControlFont();

    InitializeTabs();
    save_log_check_.SetCheck(state_.ui.save_log_checked ? BST_CHECKED : BST_UNCHECKED);

    LoadIni();

    AppendLog(L"Simple programming HID/VCOM tool initialized.");
    AppendLog(L"I2C bridge controls are available inside the FW Upload (I2C) tab.");
    UpdateIniPathUi();
    SetControlText(build_info_value_, std::wstring(L"v") + SIMPLE_PROGRAMMING_TOOL_BUILD_VERSION + L" | " + SIMPLE_PROGRAMMING_TOOL_BUILD_TIME);

    CRect rc;
    GetClientRect(&rc);
    LayoutControls(rc.Width(), rc.Height());
    RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
    return 0;
}

void CMainFrame::OnSize(UINT nType, int cx, int cy) {
    CFrameWnd::OnSize(nType, cx, cy);
    LayoutControls(cx, cy);
}

LRESULT CMainFrame::OnDpiChanged(WPARAM wParam, LPARAM lParam) {
    current_dpi_ = HIWORD(wParam);
    RecreateUiFont();
    ApplyTopControlFont();
    fw_upload_tab_.RefreshDpiLayout();
    uart_isp_tab_.RefreshDpiLayout();
    xmodem_tab_.RefreshDpiLayout();
    usb_hid_isp_tab_.RefreshDpiLayout();

    const RECT* suggested_rect = reinterpret_cast<const RECT*>(lParam);
    if (suggested_rect != nullptr) {
        SetWindowPos(nullptr,
                     suggested_rect->left,
                     suggested_rect->top,
                     suggested_rect->right - suggested_rect->left,
                     suggested_rect->bottom - suggested_rect->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }

    CRect rc;
    GetClientRect(&rc);
    LayoutControls(rc.Width(), rc.Height());
    RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN | RDW_UPDATENOW);
    return 0;
}

void CMainFrame::OnGetMinMaxInfo(MINMAXINFO* lpMMI) {
    CFrameWnd::OnGetMinMaxInfo(lpMMI);
    if (lpMMI == nullptr) {
        return;
    }

    const mfc_tool::ui::DpiScaler dpi(current_dpi_ != 0u ? current_dpi_ : mfc_tool::ui::GetDpiForWnd(*this));
    lpMMI->ptMinTrackSize.x = (std::max)(lpMMI->ptMinTrackSize.x, static_cast<LONG>(dpi.Scale(1180)));
    lpMMI->ptMinTrackSize.y = (std::max)(lpMMI->ptMinTrackSize.y, static_cast<LONG>(dpi.Scale(760)));
}

void CMainFrame::RecreateUiFont() {
    (void)mfc_tool::ui::CreatePointFontForWindow(ui_font_, *this, 90);
}

void CMainFrame::ApplyTopControlFont() {
    const std::array<CWnd*, 17> controls = {
        &image_group_, &image_path_label_, &image_path_edit_, &load_image_btn_, &image_info_label_,
        &save_ini_btn_, &load_ini_btn_, &reset_ini_btn_, &ini_path_title_, &ini_path_value_,
        &build_info_title_, &build_info_value_, &tab_ctrl_, &save_log_check_, &save_log_btn_,
        &clear_log_btn_, &log_edit_
    };
    for (CWnd* w : controls) {
        if (w != nullptr && ::IsWindow(w->GetSafeHwnd())) {
            w->SetFont(&ui_font_, FALSE);
        }
    }
}

void CMainFrame::LayoutControls(int cx, int cy) {
    if (cx <= 0 || cy <= 0) {
        return;
    }

    const mfc_tool::ui::DpiScaler dpi(current_dpi_ != 0u ? current_dpi_ : mfc_tool::ui::GetDpiForWnd(*this));
    const mfc_tool::ui::LayoutMetrics metrics = mfc_tool::ui::MetricsForWindow(*this);
    const int margin = metrics.margin8;
    const int gap = metrics.gap;
    const int row_h = metrics.row26;
    const int label_h = metrics.label18;
    const int label_y_pad = dpi.Scale(4);
    const int label_pad = dpi.Scale(10);
    const int group_top_pad = metrics.groupTopPad;
    const bool compact_vertical = dpi.Dpi() >= 144u;
    const int log_min_h = compact_vertical ? dpi.Scale(64) : dpi.Scale(120);
    const int log_preferred_h = compact_vertical ? cy / 9 : cy / 5;
    const int log_h = (std::max)(log_min_h, log_preferred_h);
    int btn_w = dpi.Scale(86);
    btn_w = (std::max)(btn_w, mfc_tool::ui::MeasureButtonMinWidth(save_ini_btn_));
    btn_w = (std::max)(btn_w, mfc_tool::ui::MeasureButtonMinWidth(load_image_btn_));

    int y = margin;
    int x = margin;
    const int image_h = dpi.Scale(76);
    const int load_image_w = (std::max)(btn_w, mfc_tool::ui::MeasureButtonMinWidth(load_image_btn_));
    mfc_tool::ui::SafeMoveWindow(image_group_, margin, y, cx - margin * 2, image_h);
    x = margin + dpi.Scale(12);
    {
        const int yy = y + group_top_pad;
        x += mfc_tool::ui::PlaceLabel(image_path_label_, x, yy + label_y_pad, label_pad) + gap;
        mfc_tool::ui::SafeMoveWindow(load_image_btn_, cx - margin - load_image_w, yy, load_image_w, row_h);
        mfc_tool::ui::SafeMoveWindow(image_path_edit_, x, yy, (std::max)(dpi.Scale(160), cx - margin - load_image_w - gap - x), row_h);
        mfc_tool::ui::SafeMoveWindow(image_info_label_, margin + dpi.Scale(12), yy + row_h + dpi.Scale(4), cx - margin * 2 - dpi.Scale(24), label_h);
    }

    y += image_h + gap;
    x = margin;
    x += mfc_tool::ui::PlaceLabel(ini_path_title_, x, y + label_y_pad, label_pad) + gap;
    const int save_w = (std::max)(btn_w, mfc_tool::ui::MeasureButtonMinWidth(save_ini_btn_));
    const int load_w = (std::max)(btn_w, mfc_tool::ui::MeasureButtonMinWidth(load_ini_btn_));
    const int reset_w = (std::max)(btn_w, mfc_tool::ui::MeasureButtonMinWidth(reset_ini_btn_));
    const int ini_buttons_w = save_w + load_w + reset_w + gap * 2;
    const int ini_buttons_x = (std::max)(x + dpi.Scale(160), cx - margin - ini_buttons_w);
    const int build_block_w = dpi.Scale(360);
    const int build_x = (std::max)(x + dpi.Scale(160), ini_buttons_x - build_block_w - gap);
    mfc_tool::ui::SafeMoveWindow(ini_path_value_, x, y + label_y_pad, (std::max)(dpi.Scale(120), build_x - x - gap), label_h);
    x = build_x;
    x += mfc_tool::ui::PlaceLabel(build_info_title_, x, y + label_y_pad, label_pad) + gap;
    mfc_tool::ui::SafeMoveWindow(build_info_value_, x, y + label_y_pad, (std::max)(dpi.Scale(120), ini_buttons_x - x - gap), label_h);
    x = ini_buttons_x;
    mfc_tool::ui::SafeMoveWindow(save_ini_btn_, x, y, save_w, row_h);
    x += save_w + gap;
    mfc_tool::ui::SafeMoveWindow(load_ini_btn_, x, y, load_w, row_h);
    x += load_w + gap;
    mfc_tool::ui::SafeMoveWindow(reset_ini_btn_, x, y, reset_w, row_h);
    y += row_h + gap;

    const int top_h = y;
    const int log_y = cy - log_h - margin;
    const int log_btn_y = log_y - row_h - gap;
    mfc_tool::ui::SafeMoveWindow(save_log_check_, margin, log_btn_y + dpi.Scale(3), dpi.Scale(110), metrics.checkbox20);
    mfc_tool::ui::SafeMoveWindow(save_log_btn_, margin + dpi.Scale(116), log_btn_y, btn_w, row_h);
    mfc_tool::ui::SafeMoveWindow(clear_log_btn_, margin + dpi.Scale(116) + btn_w + gap, log_btn_y, btn_w, row_h);
    mfc_tool::ui::SafeMoveWindow(log_edit_, margin, log_y, cx - margin * 2, cy - log_y - margin);
    mfc_tool::ui::SafeMoveWindow(tab_ctrl_, margin, top_h, cx - margin * 2, (std::max)(120, log_btn_y - top_h - gap));
    ShowActiveTabPage();
}

void CMainFrame::InitializeTabs() {
    tab_ctrl_.DeleteAllItems();
    tab_ctrl_.InsertItem(0, L"FW Upload (I2C)");
    tab_ctrl_.InsertItem(1, L"FW Upload (UART ISP)");
    tab_ctrl_.InsertItem(2, L"FW Upload (XMODEM)");
    tab_ctrl_.InsertItem(3, L"FW Upload (USB HID)");
    {
        const mfc_tool::ui::DpiScaler dpi(current_dpi_ != 0u ? current_dpi_ : mfc_tool::ui::GetDpiForWnd(*this));
        tab_ctrl_.SetItemSize(CSize(dpi.Scale(190), dpi.Scale(24)));
    }
    tab_ctrl_.SetCurSel(0);
    ShowActiveTabPage();
}

void CMainFrame::ShowActiveTabPage() {
    if (!::IsWindow(tab_ctrl_.GetSafeHwnd())) {
        return;
    }
    CRect rc;
    tab_ctrl_.GetClientRect(&rc);
    tab_ctrl_.AdjustRect(FALSE, &rc);
    {
        const mfc_tool::ui::DpiScaler dpi(current_dpi_ != 0u ? current_dpi_ : mfc_tool::ui::GetDpiForWnd(*this));
        const int page_pad = dpi.Scale(4);
        rc.DeflateRect(page_pad, page_pad, page_pad, page_pad);
    }

    mfc_tool::ui::SafeMoveWindow(fw_upload_tab_, rc);
    mfc_tool::ui::SafeMoveWindow(uart_isp_tab_, rc);
    mfc_tool::ui::SafeMoveWindow(xmodem_tab_, rc);
    mfc_tool::ui::SafeMoveWindow(usb_hid_isp_tab_, rc);

    const int sel = tab_ctrl_.GetCurSel();
    fw_upload_tab_.ShowWindow(sel == 0 ? SW_SHOW : SW_HIDE);
    uart_isp_tab_.ShowWindow(sel == 1 ? SW_SHOW : SW_HIDE);
    xmodem_tab_.ShowWindow(sel == 2 ? SW_SHOW : SW_HIDE);
    usb_hid_isp_tab_.ShowWindow(sel == 3 ? SW_SHOW : SW_HIDE);
}

void CMainFrame::OnTabSelChange(NMHDR* pNMHDR, LRESULT* pResult) {
    (void)pNMHDR;
    CRect rc;
    GetClientRect(&rc);
    LayoutControls(rc.Width(), rc.Height());
    if (pResult != nullptr) {
        *pResult = 0;
    }
}

void CMainFrame::AppendLog(const std::wstring& text) {
    const std::wstring line = L"[" + NowTimeText() + L"] " + text;
    logger_.AddLine(line);

    if (!::IsWindow(log_edit_.GetSafeHwnd())) {
        return;
    }
    int len = log_edit_.GetWindowTextLengthW();
    log_edit_.SetSel(len, len);
    log_edit_.ReplaceSel((line + L"\r\n").c_str());
    TrimVisibleLogIfNeeded();
}

void CMainFrame::TrimVisibleLogIfNeeded() {
    const int len = log_edit_.GetWindowTextLengthW();
    if (len <= kUiLogTrimThresholdChars) {
        return;
    }

    CString visible_log;
    log_edit_.GetWindowTextW(visible_log);
    std::wstring text = visible_log.GetString();
    int trim_chars = len - kUiLogTrimTargetChars;
    if (trim_chars <= 0) {
        return;
    }
    std::wstring::size_type trim_end = text.find(L'\n', static_cast<std::wstring::size_type>(trim_chars));
    if (trim_end == std::wstring::npos) {
        trim_end = static_cast<std::wstring::size_type>(trim_chars);
    } else {
        ++trim_end;
    }
    if (trim_end == 0 || trim_end >= text.size()) {
        return;
    }

    log_edit_.SetRedraw(FALSE);
    log_edit_.SetSel(0, static_cast<int>(trim_end));
    log_edit_.ReplaceSel(L"");
    log_edit_.SetSel(log_edit_.GetWindowTextLengthW(), log_edit_.GetWindowTextLengthW());
    log_edit_.SetRedraw(TRUE);
    log_edit_.Invalidate(FALSE);
}

void CMainFrame::UpdateIniPathUi() {
    SetControlText(ini_path_value_, ini_.Path());
}

void CMainFrame::SetFirmwareImagePathText() {
    SetControlText(image_path_edit_, state_.ui.firmware_image_path);
    const std::wstring info = firmware_image_data_.empty()
        ? L"No image loaded"
        : (L"Image size: " + std::to_wstring(firmware_image_data_.size()) + L" bytes");
    SetControlText(image_info_label_, info);
}

bool CMainFrame::LoadFirmwareImageFromPath(const std::wstring& path, bool show_errors) {
    if (path.empty()) {
        if (show_errors) {
            ShowErrorBox(L"Load Binary", L"Select a binary image first.");
        }
        return false;
    }

    try {
        std::ifstream in(std::filesystem::path(path), std::ios::binary);
        if (!in) {
            throw std::runtime_error("failed to open binary image");
        }
        std::vector<std::uint8_t> image;
        image.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        if (image.empty()) {
            throw std::runtime_error("binary image is empty");
        }

        firmware_image_data_ = std::move(image);
        state_.ui.firmware_image_path = path;
        SetFirmwareImagePathText();
        RefreshSharedImageState();
        AppendLog(L"Firmware image loaded: " + path + L" (" + std::to_wstring(firmware_image_data_.size()) + L" bytes)");
        return true;
    } catch (const std::exception& e) {
        if (show_errors) {
            ShowErrorBox(L"Load Binary", AnsiToWide(e.what()));
        }
        AppendLog(L"Firmware image load failed: " + AnsiToWide(e.what()));
        SetFirmwareImagePathText();
        RefreshSharedImageState();
        return false;
    }
}

void CMainFrame::RefreshSharedImageState() {
    fw_upload_tab_.RefreshSharedImageState();
    uart_isp_tab_.RefreshSharedImageState();
    xmodem_tab_.RefreshSharedImageState();
    usb_hid_isp_tab_.RefreshSharedImageState();
}

void CMainFrame::LoadIni() {
    UpdateIniPathUi();
    mfc_tool::config::IniData data;
    std::wstring error;
    if (!ini_.Exists()) {
        firmware_image_data_.clear();
        SetFirmwareImagePathText();
        fw_upload_tab_.LoadState(state_);
        uart_isp_tab_.LoadState(state_);
        xmodem_tab_.LoadState(state_);
        usb_hid_isp_tab_.LoadState(state_);
        RefreshSharedImageState();
        SaveIni();
        AppendLog(L"INI created with defaults.");
        return;
    }
    if (!ini_.Load(&data, &error)) {
        AppendLog(L"INI load failed: " + error);
        return;
    }

    state_.ApplyIniData(data);
    save_log_check_.SetCheck(state_.ui.save_log_checked ? BST_CHECKED : BST_UNCHECKED);
    fw_upload_tab_.LoadState(state_);
    uart_isp_tab_.LoadState(state_);
    xmodem_tab_.LoadState(state_);
    usb_hid_isp_tab_.LoadState(state_);
    firmware_image_data_.clear();
    SetFirmwareImagePathText();
    if (!state_.ui.firmware_image_path.empty()) {
        LoadFirmwareImageFromPath(state_.ui.firmware_image_path, false);
    } else {
        RefreshSharedImageState();
    }
    AppendLog(L"INI loaded.");
}

void CMainFrame::SaveIni() {
    fw_upload_tab_.SaveState(&state_);
    uart_isp_tab_.SaveState(&state_);
    xmodem_tab_.SaveState(&state_);
    usb_hid_isp_tab_.SaveState(&state_);
    state_.ui.save_log_checked = (save_log_check_.GetCheck() == BST_CHECKED);

    std::wstring error;
    auto data = state_.ToIniData(ini_.Path());
    if (!ini_.Save(data, &error)) {
        AppendLog(L"INI save failed: " + error);
        return;
    }
    AppendLog(L"INI saved.");
}

void CMainFrame::SaveIniTo(const std::wstring& path) {
    ini_.SetPath(path);
    UpdateIniPathUi();
    SaveIni();
}

bool CMainFrame::TryLoadIniFrom(const std::wstring& path) {
    ini_.SetPath(path);
    UpdateIniPathUi();
    LoadIni();
    return true;
}

void CMainFrame::ResetIniToDefault() {
    state_ = mfc_tool::core::AppState::Default();
    save_log_check_.SetCheck(BST_UNCHECKED);
    firmware_image_data_.clear();
    SetFirmwareImagePathText();
    fw_upload_tab_.LoadState(state_);
    uart_isp_tab_.LoadState(state_);
    xmodem_tab_.LoadState(state_);
    usb_hid_isp_tab_.LoadState(state_);
    RefreshSharedImageState();
    SaveIni();
}

void CMainFrame::SaveLogToFile(const std::wstring& path) {
    std::wstring error;
    if (!logger_.SaveToFile(path, &error)) {
        ShowErrorBox(L"Save Log Error", error);
        return;
    }
    AppendLog(L"Log saved: " + path);
}

std::wstring CMainFrame::GetControlText(const CWnd& w) const {
    CString s;
    const_cast<CWnd&>(w).GetWindowTextW(s);
    return s.GetString();
}

void CMainFrame::SetControlText(CWnd& w, const std::wstring& text) {
    if (::IsWindow(w.GetSafeHwnd())) {
        w.SetWindowTextW(text.c_str());
    }
}

void CMainFrame::ShowErrorBox(const std::wstring& title, const std::wstring& message) {
    ::MessageBoxW(nullptr, message.c_str(), title.c_str(), MB_ICONERROR | MB_OK);
}

std::wstring CMainFrame::AnsiToWide(const char* text) {
    if (text == nullptr) {
        return L"";
    }
    int n = MultiByteToWideChar(CP_ACP, 0, text, -1, nullptr, 0);
    if (n <= 0) {
        return L"";
    }
    std::wstring out(static_cast<size_t>(n - 1), L'\0');
    MultiByteToWideChar(CP_ACP, 0, text, -1, out.data(), n);
    return out;
}

void CMainFrame::OnBnClickedLoadImage() {
    CFileDialog dlg(TRUE, L"bin", nullptr,
                    OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST,
                    L"Binary Files (*.bin)|*.bin|All Files (*.*)|*.*||", this);
    if (dlg.DoModal() != IDOK) {
        return;
    }
    if (LoadFirmwareImageFromPath(dlg.GetPathName().GetString(), true)) {
        SaveIni();
    }
}

void CMainFrame::OnBnClickedSaveIni() {
    CFileDialog dlg(FALSE, L"ini", L"simple_programming_tool.ini",
                    OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY,
                    L"INI Files (*.ini)|*.ini|All Files (*.*)|*.*||", this);
    if (dlg.DoModal() == IDOK) {
        SaveIniTo(dlg.GetPathName().GetString());
    }
}

void CMainFrame::OnBnClickedLoadIni() {
    if (service_.IsConnected()) {
        ShowErrorBox(L"Load INI", L"Disconnect first before loading INI settings.");
        AppendLog(L"Load INI blocked while connected.");
        return;
    }

    CFileDialog dlg(TRUE, L"ini", nullptr,
                    OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
                    L"INI Files (*.ini)|*.ini|All Files (*.*)|*.*||", this);
    if (dlg.DoModal() == IDOK) {
        TryLoadIniFrom(dlg.GetPathName().GetString());
    }
}

void CMainFrame::OnBnClickedResetIni() {
    if (service_.IsConnected()) {
        ShowErrorBox(L"Reset INI", L"Disconnect first before resetting INI settings.");
        AppendLog(L"Reset INI blocked while connected.");
        return;
    }

    if (MessageBoxW(L"Reset INI settings to defaults?", L"Reset INI", MB_ICONQUESTION | MB_YESNO) != IDYES) {
        return;
    }
    ResetIniToDefault();
    AppendLog(L"INI reset to defaults.");
}

void CMainFrame::OnBnClickedSaveLog() {
    if (save_log_check_.GetCheck() != BST_CHECKED) {
        return;
    }

    CFileDialog dlg(FALSE, L"log", L"simple_programming_tool.log",
                    OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY,
                    L"Log Files (*.log)|*.log|Text Files (*.txt)|*.txt|All Files (*.*)|*.*||", this);
    if (dlg.DoModal() == IDOK) {
        SaveLogToFile(dlg.GetPathName().GetString());
    }
}

void CMainFrame::OnBnClickedSaveLogCheck() {
    const bool checked = (save_log_check_.GetCheck() == BST_CHECKED);
    mfc_tool::ui::SafeEnableWindow(save_log_btn_, checked);
}

void CMainFrame::OnBnClickedClearLog() {
    logger_.Clear();
    log_edit_.SetWindowTextW(L"");
}

void CMainFrame::OnClose() {
    SaveIni();
    fw_upload_tab_.OnDisconnected();
    uart_isp_tab_.OnShutdown();
    xmodem_tab_.OnShutdown();
    usb_hid_isp_tab_.OnShutdown();
    service_.Disconnect();
    CFrameWnd::OnClose();
}
