#include "uart_isp_tab.h"

#include <afxdlgs.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "../core/text_utils.h"
#include "layout_utils.h"

namespace {

enum : UINT {
    IDC_UART_CONN_GROUP = 23000,
    IDC_UART_COM_LABEL,
    IDC_UART_COM_COMBO,
    IDC_UART_BAUD_LABEL,
    IDC_UART_BAUD_COMBO,
    IDC_UART_TIMEOUT_LABEL,
    IDC_UART_TIMEOUT_EDIT,
    IDC_UART_REFRESH_COM,
    IDC_UART_OPEN_COM,
    IDC_UART_CLOSE_COM,
    IDC_UART_FILE_GROUP,
    IDC_UART_PATH_LABEL,
    IDC_UART_PATH_EDIT,
    IDC_UART_LOAD_IMAGE,
    IDC_UART_FILE_INFO,
    IDC_UART_ACTION_GROUP,
    IDC_UART_CONNECT_TARGET,
    IDC_UART_GET_DEVICE,
    IDC_UART_PROGRAM,
    IDC_UART_RUN_APROM,
    IDC_UART_RESET_TARGET,
    IDC_UART_STOP,
    IDC_UART_TARGET_STATE,
    IDC_UART_PROGRESS,
    IDC_UART_PROGRESS_LABEL,
    IDC_UART_STATUS_GROUP,
    IDC_UART_STATUS_EDIT,
};

bool IsCancelError(const std::exception& e) {
    return std::string(e.what()).find("cancelled") != std::string::npos ||
        std::string(e.what()).find("canceled") != std::string::npos ||
        std::string(e.what()).find("cancelled") != std::string::npos;
}

} // namespace

BEGIN_MESSAGE_MAP(CUartIspTab, CWnd)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_BN_CLICKED(IDC_UART_REFRESH_COM, &CUartIspTab::OnRefreshCom)
    ON_BN_CLICKED(IDC_UART_OPEN_COM, &CUartIspTab::OnOpenCom)
    ON_BN_CLICKED(IDC_UART_CLOSE_COM, &CUartIspTab::OnCloseCom)
    ON_BN_CLICKED(IDC_UART_LOAD_IMAGE, &CUartIspTab::OnLoadImage)
    ON_BN_CLICKED(IDC_UART_CONNECT_TARGET, &CUartIspTab::OnConnectTarget)
    ON_BN_CLICKED(IDC_UART_GET_DEVICE, &CUartIspTab::OnGetDeviceId)
    ON_BN_CLICKED(IDC_UART_PROGRAM, &CUartIspTab::OnProgram)
    ON_BN_CLICKED(IDC_UART_RUN_APROM, &CUartIspTab::OnRunAprom)
    ON_BN_CLICKED(IDC_UART_RESET_TARGET, &CUartIspTab::OnResetTarget)
    ON_BN_CLICKED(IDC_UART_STOP, &CUartIspTab::OnStop)
    ON_WM_CTLCOLOR()
END_MESSAGE_MAP()

BOOL CUartIspTab::Create(CWnd* parent, const RECT& rect, UINT id) {
    CString cls = AfxRegisterWndClass(CS_DBLCLKS, ::LoadCursor(nullptr, IDC_ARROW),
                                      reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1), nullptr);
    return CWnd::CreateEx(WS_EX_CONTROLPARENT, cls, L"UartIspTab",
                          WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, rect, parent, id);
}

void CUartIspTab::Bind(std::function<void(const std::wstring&)> logger,
                       std::function<const std::vector<std::uint8_t>&()> firmware_image_data,
                       std::function<std::wstring()> firmware_image_path,
                       std::function<void()> persist_settings) {
    log_ = std::move(logger);
    firmware_image_data_ = std::move(firmware_image_data);
    firmware_image_path_ = std::move(firmware_image_path);
    persist_settings_ = std::move(persist_settings);
}

int CUartIspTab::OnCreate(LPCREATESTRUCT lpCreateStruct) {
    if (CWnd::OnCreate(lpCreateStruct) == -1) {
        return -1;
    }

    ui_font_.CreatePointFont(90, L"Segoe UI");
    target_ready_brush_.CreateSolidBrush(RGB(48, 158, 98));
    target_idle_brush_.CreateSolidBrush(RGB(226, 226, 226));
    auto fail = [this](BOOL ok, const wchar_t* name) -> bool {
        if (!ok && log_) {
            log_(std::wstring(L"Create UART ISP control failed: ") + name);
        }
        return ok != FALSE;
    };

    if (!fail(conn_group_.Create(L"UART ISP Connection", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, CRect(), this, IDC_UART_CONN_GROUP), L"connection group")) return -1;
    if (!fail(com_label_.Create(L"COM", WS_CHILD | WS_VISIBLE, CRect(), this, IDC_UART_COM_LABEL), L"COM label")) return -1;
    if (!fail(com_combo_.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, CRect(), this, IDC_UART_COM_COMBO), L"COM combo")) return -1;
    if (!fail(baud_label_.Create(L"Baud", WS_CHILD | WS_VISIBLE, CRect(), this, IDC_UART_BAUD_LABEL), L"baud label")) return -1;
    if (!fail(baud_combo_.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, CRect(), this, IDC_UART_BAUD_COMBO), L"baud combo")) return -1;
    if (!fail(timeout_label_.Create(L"Timeout ms", WS_CHILD | WS_VISIBLE, CRect(), this, IDC_UART_TIMEOUT_LABEL), L"timeout label")) return -1;
    if (!fail(timeout_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, CRect(), this, IDC_UART_TIMEOUT_EDIT), L"timeout edit")) return -1;
    if (!fail(refresh_com_btn_.Create(L"Refresh COM", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_UART_REFRESH_COM), L"refresh COM")) return -1;
    if (!fail(open_com_btn_.Create(L"Open UART", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_UART_OPEN_COM), L"open UART")) return -1;
    if (!fail(close_com_btn_.Create(L"Close UART", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_UART_CLOSE_COM), L"close UART")) return -1;

    if (!fail(action_group_.Create(L"Nuvoton UART ISP", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, CRect(), this, IDC_UART_ACTION_GROUP), L"action group")) return -1;
    if (!fail(connect_target_btn_.Create(L"Connect Target", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_UART_CONNECT_TARGET), L"connect target")) return -1;
    if (!fail(get_device_btn_.Create(L"Get Device ID", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_UART_GET_DEVICE), L"get device")) return -1;
    if (!fail(program_btn_.Create(L"Program APROM", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_UART_PROGRAM), L"program")) return -1;
    if (!fail(run_aprom_btn_.Create(L"Run APROM", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_UART_RUN_APROM), L"run aprom")) return -1;
    if (!fail(reset_target_btn_.Create(L"Reset Target", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_UART_RESET_TARGET), L"reset target")) return -1;
    if (!fail(stop_btn_.Create(L"Stop", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_UART_STOP), L"stop")) return -1;
    if (!fail(target_state_label_.Create(L"Target: not connected", WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE, CRect(), this, IDC_UART_TARGET_STATE), L"target state")) return -1;
    if (!fail(progress_.Create(WS_CHILD | WS_VISIBLE | PBS_SMOOTH, CRect(), this, IDC_UART_PROGRESS), L"progress")) return -1;
    if (!fail(progress_label_.Create(L"Progress: 0%", WS_CHILD | WS_VISIBLE, CRect(), this, IDC_UART_PROGRESS_LABEL), L"progress label")) return -1;

    if (!fail(status_group_.Create(L"Response", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, CRect(), this, IDC_UART_STATUS_GROUP), L"status group")) return -1;
    if (!fail(status_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_READONLY, CRect(), this, IDC_UART_STATUS_EDIT), L"status edit")) return -1;

    std::vector<CWnd*> controls = {
        &conn_group_, &com_label_, &com_combo_, &baud_label_, &baud_combo_, &timeout_label_, &timeout_edit_,
        &refresh_com_btn_, &open_com_btn_, &close_com_btn_, &action_group_, &connect_target_btn_, &get_device_btn_,
        &program_btn_, &run_aprom_btn_, &reset_target_btn_,
        &stop_btn_, &target_state_label_, &progress_, &progress_label_, &status_group_, &status_edit_
    };
    for (CWnd* w : controls) {
        w->SetFont(&ui_font_);
    }

    PopulateBaudCombo();
    RefreshComPorts();
    progress_.SetRange32(0, 100);
    LoadVisibleFromState();
    UpdateEnableState();
    return 0;
}

void CUartIspTab::OnSize(UINT nType, int cx, int cy) {
    CWnd::OnSize(nType, cx, cy);
    if (!::IsWindow(conn_group_.GetSafeHwnd())) {
        return;
    }
    LayoutControls(CRect(0, 0, cx, cy));
}

void CUartIspTab::LayoutControls(const CRect& r) {
    const int margin = 8;
    const int gap = 6;
    const int row = 26;
    const int label_y_pad = 4;
    const int group_top_pad = 22;
    const int left = r.left;
    const int bottom = r.bottom;
    const int inner_left = r.left + margin + 12;
    const int available_w = (std::max)(240, r.Width() - margin * 2);
    const int conn_h = 104;
    const int action_h = 144;
    int x;
    int y = r.top + margin;

    mfc_tool::ui::SafeMoveWindow(conn_group_, left + margin, y, available_w, conn_h);
    x = inner_left;
    {
        const int yy1 = y + group_top_pad;
        const int yy2 = yy1 + row + 8;
        x = mfc_tool::ui::PlaceLabelAndControl(com_label_, com_combo_, x, yy1 + label_y_pad, yy1, 110, row + 120, gap, 8) + gap;
        x = mfc_tool::ui::PlaceLabelAndControl(baud_label_, baud_combo_, x, yy1 + label_y_pad, yy1, 106, row + 120, gap, 8) + gap;
        x = mfc_tool::ui::PlaceLabelAndControl(timeout_label_, timeout_edit_, x, yy1 + label_y_pad, yy1, 80, row, gap, 8) + gap;
        mfc_tool::ui::SafeMoveWindow(refresh_com_btn_, x, yy1, (std::max)(104, mfc_tool::ui::MeasureButtonMinWidth(refresh_com_btn_)), row);

        x = inner_left;
        mfc_tool::ui::SafeMoveWindow(open_com_btn_, x, yy2, (std::max)(94, mfc_tool::ui::MeasureButtonMinWidth(open_com_btn_)), row);
        x += (std::max)(94, mfc_tool::ui::MeasureButtonMinWidth(open_com_btn_)) + gap;
        mfc_tool::ui::SafeMoveWindow(close_com_btn_, x, yy2, (std::max)(94, mfc_tool::ui::MeasureButtonMinWidth(close_com_btn_)), row);
    }

    y += conn_h + gap;
    mfc_tool::ui::SafeMoveWindow(action_group_, left + margin, y, available_w, action_h);
    x = inner_left;
    {
        const int yy = y + group_top_pad;
        const int connect_w = (std::max)(112, mfc_tool::ui::MeasureButtonMinWidth(connect_target_btn_));
        const int device_w = (std::max)(106, mfc_tool::ui::MeasureButtonMinWidth(get_device_btn_));
        const int program_w = (std::max)(118, mfc_tool::ui::MeasureButtonMinWidth(program_btn_));
        const int run_w = (std::max)(90, mfc_tool::ui::MeasureButtonMinWidth(run_aprom_btn_));
        const int reset_w = (std::max)(102, mfc_tool::ui::MeasureButtonMinWidth(reset_target_btn_));
        const int stop_w = (std::max)(68, mfc_tool::ui::MeasureButtonMinWidth(stop_btn_));

        mfc_tool::ui::SafeMoveWindow(connect_target_btn_, x, yy, connect_w, row);
        x += connect_w + gap;
        mfc_tool::ui::SafeMoveWindow(get_device_btn_, x, yy, device_w, row);
        x += device_w + gap;
        mfc_tool::ui::SafeMoveWindow(program_btn_, x, yy, program_w, row);
        x += program_w + gap;
        mfc_tool::ui::SafeMoveWindow(run_aprom_btn_, x, yy, run_w, row);
        x += run_w + gap;
        mfc_tool::ui::SafeMoveWindow(reset_target_btn_, x, yy, reset_w, row);
        x += reset_w + gap;
        mfc_tool::ui::SafeMoveWindow(stop_btn_, x, yy, stop_w, row);

        mfc_tool::ui::SafeMoveWindow(target_state_label_, inner_left, yy + row + 8, available_w - 24, 24);
        mfc_tool::ui::SafeMoveWindow(progress_, inner_left, yy + row + 38, available_w - 24, 18);
        mfc_tool::ui::SafeMoveWindow(progress_label_, inner_left, yy + row + 60, available_w - 24, 18);
    }

    y += action_h + gap;
    mfc_tool::ui::SafeMoveWindow(status_group_, left + margin, y, available_w, (std::max)(80, bottom - y - margin));
    mfc_tool::ui::SafeMoveWindow(status_edit_, inner_left, y + group_top_pad, available_w - 24,
                                 (std::max)(46, bottom - y - margin - group_top_pad - 8));
}

void CUartIspTab::RefreshComPorts() {
    if (!::IsWindow(com_combo_.GetSafeHwnd())) {
        return;
    }
    const std::wstring selected_before = SelectedComPort();
    com_combo_.ResetContent();
    for (int i = 1; i <= 256; ++i) {
        wchar_t name[16] = {};
        swprintf_s(name, L"COM%d", i);
        wchar_t target[256] = {};
        DWORD len = QueryDosDeviceW(name, target, static_cast<DWORD>(sizeof(target) / sizeof(target[0])));
        if (len > 0u) {
            com_combo_.AddString(name);
        }
    }
    if (com_combo_.GetCount() > 0) {
        std::wstring preferred = selected_before.empty() ? state_.com_port : selected_before;
        int idx = preferred.empty() ? -1 : com_combo_.FindStringExact(-1, preferred.c_str());
        com_combo_.SetCurSel(idx >= 0 ? idx : 0);
    }
}

void CUartIspTab::PopulateBaudCombo() {
    baud_combo_.ResetContent();
    baud_combo_.AddString(L"115200");
    baud_combo_.AddString(L"460800");
    baud_combo_.AddString(L"921600");
    baud_combo_.SetCurSel(0);
}

void CUartIspTab::UpdateEnableState() {
    if (!::IsWindow(open_com_btn_.GetSafeHwnd())) {
        return;
    }
    SaveVisibleToState();
    const bool connected = client_.IsConnected();
    const bool image_loaded = firmware_image_data_ && !firmware_image_data_().empty();
    mfc_tool::ui::SafeEnableWindow(com_combo_, !connected && !busy_);
    mfc_tool::ui::SafeEnableWindow(baud_combo_, !connected && !busy_);
    mfc_tool::ui::SafeEnableWindow(timeout_edit_, !connected && !busy_);
    mfc_tool::ui::SafeEnableWindow(refresh_com_btn_, !connected && !busy_);
    mfc_tool::ui::SafeEnableWindow(open_com_btn_, !connected && !busy_);
    mfc_tool::ui::SafeEnableWindow(close_com_btn_, connected && !busy_);
    mfc_tool::ui::SafeEnableWindow(connect_target_btn_, connected && !busy_);
    mfc_tool::ui::SafeEnableWindow(get_device_btn_, connected && !busy_);
    mfc_tool::ui::SafeEnableWindow(program_btn_, connected && !busy_ && image_loaded);
    mfc_tool::ui::SafeEnableWindow(run_aprom_btn_, connected && !busy_);
    mfc_tool::ui::SafeEnableWindow(reset_target_btn_, connected && !busy_);
    mfc_tool::ui::SafeEnableWindow(stop_btn_, busy_);
}

void CUartIspTab::SaveVisibleToState() {
    if (loading_ || !::IsWindow(com_combo_.GetSafeHwnd())) {
        return;
    }
    state_.com_port = SelectedComPort();
    state_.baud = std::to_wstring(SelectedBaudrate());
    state_.timeout_ms = GetEditText(timeout_edit_);
}

void CUartIspTab::LoadVisibleFromState() {
    if (!::IsWindow(com_combo_.GetSafeHwnd())) {
        return;
    }
    loading_ = true;
    ApplyComboSelection(com_combo_, state_.com_port);
    ApplyComboSelection(baud_combo_, state_.baud);
    timeout_edit_.SetWindowTextW(state_.timeout_ms.c_str());
    loading_ = false;
}

void CUartIspTab::EnsureUartConnected() {
    if (!client_.IsConnected()) {
        throw std::runtime_error("UART is not open.");
    }
}

void CUartIspTab::LoadImageFromPath(const std::wstring& path) {
    if (path.empty()) {
        throw std::invalid_argument("select a binary image first");
    }
    std::ifstream in(std::filesystem::path(path), std::ios::binary);
    if (!in) {
        throw std::runtime_error("failed to open binary image");
    }
    image_data_.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    if (image_data_.empty()) {
        throw std::runtime_error("binary image is empty");
    }
    state_.image_path = path;
    SetImagePathText();
    SetProgress(0, L"Progress: 0%");
    if (log_) {
        log_(L"UART ISP image loaded: " + path + L" (" + std::to_wstring(image_data_.size()) + L" bytes)");
    }
}

void CUartIspTab::SetImagePathText() {
    if (::IsWindow(path_edit_.GetSafeHwnd())) {
        path_edit_.SetWindowTextW(state_.image_path.c_str());
    }
    if (::IsWindow(file_info_label_.GetSafeHwnd())) {
        const std::wstring info = image_data_.empty()
            ? L"No image loaded"
            : (L"Image size: " + std::to_wstring(image_data_.size()) + L" bytes");
        file_info_label_.SetWindowTextW(info.c_str());
    }
}

void CUartIspTab::SetStatusText(const std::wstring& text) {
    if (::IsWindow(status_edit_.GetSafeHwnd())) {
        status_edit_.SetWindowTextW(text.c_str());
    }
}

void CUartIspTab::AppendStatusText(const std::wstring& text) {
    if (!::IsWindow(status_edit_.GetSafeHwnd())) {
        return;
    }
    int len = status_edit_.GetWindowTextLengthW();
    status_edit_.SetSel(len, len);
    status_edit_.ReplaceSel((text + L"\r\n").c_str());
}

void CUartIspTab::SetProgress(int value, const std::wstring& text) {
    if (::IsWindow(progress_.GetSafeHwnd())) {
        progress_.SetPos(std::clamp(value, 0, 100));
    }
    if (::IsWindow(progress_label_.GetSafeHwnd())) {
        progress_label_.SetWindowTextW(text.c_str());
    }
}

void CUartIspTab::SetTargetReady(bool ready, const std::wstring& text) {
    target_ready_ = ready;
    if (::IsWindow(target_state_label_.GetSafeHwnd())) {
        const std::wstring label = text.empty()
            ? (ready ? L"Target Ready - load BIN and click Program APROM" : L"Target: not connected")
            : text;
        target_state_label_.SetWindowTextW(label.c_str());
        target_state_label_.Invalidate(FALSE);
    }
}

HBRUSH CUartIspTab::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor) {
    HBRUSH brush = CWnd::OnCtlColor(pDC, pWnd, nCtlColor);
    if (pWnd != nullptr && pWnd->GetSafeHwnd() == target_state_label_.GetSafeHwnd()) {
        if (target_ready_) {
            pDC->SetTextColor(RGB(255, 255, 255));
            pDC->SetBkColor(RGB(48, 158, 98));
            return static_cast<HBRUSH>(target_ready_brush_.GetSafeHandle());
        }
        pDC->SetTextColor(RGB(32, 32, 32));
        pDC->SetBkColor(RGB(226, 226, 226));
        return static_cast<HBRUSH>(target_idle_brush_.GetSafeHandle());
    }
    return brush;
}

void CUartIspTab::PumpUiMessages() {
    MSG msg;
    while (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE) != FALSE) {
        if (!AfxGetApp()->PreTranslateMessage(&msg)) {
            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);
        }
    }
}

void CUartIspTab::LoadState(const mfc_tool::core::AppState& state) {
    state_ = state.uart_isp;
    LoadVisibleFromState();
    UpdateEnableState();
}

void CUartIspTab::SaveState(mfc_tool::core::AppState* state) const {
    if (state == nullptr) {
        return;
    }
    const_cast<CUartIspTab*>(this)->SaveVisibleToState();
    state->uart_isp = state_;
}

void CUartIspTab::OnShutdown() {
    cancel_requested_ = true;
    client_.Disconnect();
    busy_ = false;
    SetTargetReady(false);
    UpdateEnableState();
}

void CUartIspTab::RefreshSharedImageState() {
    UpdateEnableState();
}

void CUartIspTab::OnRefreshCom() {
    RefreshComPorts();
    UpdateEnableState();
}

void CUartIspTab::OnOpenCom() {
    try {
        client_.Connect(SelectedComPort(), SelectedBaudrate(), CurrentTimeoutMs());
        AppendStatusText(L"UART opened: " + SelectedComPort() + L", baud=" + std::to_wstring(SelectedBaudrate()));
        if (log_) {
            log_(L"UART ISP opened: " + SelectedComPort());
        }
        if (persist_settings_) {
            persist_settings_();
        }
    } catch (const std::exception& e) {
        ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"Open UART", MB_ICONERROR | MB_OK);
    }
    UpdateEnableState();
}

void CUartIspTab::OnCloseCom() {
    client_.Disconnect();
    SetTargetReady(false, L"Target: not connected");
    AppendStatusText(L"UART closed.");
    if (log_) {
        log_(L"UART ISP closed.");
    }
    UpdateEnableState();
}

void CUartIspTab::OnLoadImage() {
    CFileDialog dlg(TRUE, L"bin", nullptr, OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST,
                    L"Binary Files (*.bin)|*.bin|All Files (*.*)|*.*||", this);
    if (dlg.DoModal() != IDOK) {
        return;
    }
    try {
        LoadImageFromPath(dlg.GetPathName().GetString());
        if (persist_settings_) {
            persist_settings_();
        }
    } catch (const std::exception& e) {
        ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"Load Binary", MB_ICONERROR | MB_OK);
    }
    UpdateEnableState();
}

void CUartIspTab::OnConnectTarget() {
    try {
        EnsureUartConnected();
        cancel_requested_ = false;
        busy_ = true;
        UpdateEnableState();
        struct BusyGuard {
            CUartIspTab* self;
            ~BusyGuard() {
                self->busy_ = false;
                self->UpdateEnableState();
            }
        } guard{this};

        auto cancel = [this]() -> bool { return cancel_requested_; };
        auto poll = [this]() { PumpUiMessages(); };
        SetStatusText(L"Waiting for UART ISP handshake...");
        SetTargetReady(false, L"Target: waiting for UART ISP handshake...");
        client_.ConnectTarget(cancel, poll);
        const std::uint8_t fw = client_.GetFirmwareVersion();
        const std::uint32_t id = client_.GetDeviceId();
        SetTargetReady(true, L"Target Ready - load BIN and click Program APROM");
        SetProgress(100, L"Connect: UART ISP ready");
        AppendStatusText(L"Target ready. ISP FW=0x" + FormatHex32(fw).substr(8) + L", DeviceID=" + FormatHex32(id));
    } catch (const std::exception& e) {
        SetTargetReady(false, IsCancelError(e) ? L"Target: connect stopped" : L"Target: connect failed");
        AppendStatusText(L"Connect failed: " + AnsiToWide(e.what()));
        if (!IsCancelError(e)) {
            ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"Connect Target", MB_ICONERROR | MB_OK);
        }
    }
}

void CUartIspTab::OnGetDeviceId() {
    try {
        EnsureUartConnected();
        cancel_requested_ = false;
        busy_ = true;
        UpdateEnableState();
        struct BusyGuard {
            CUartIspTab* self;
            ~BusyGuard() {
                self->busy_ = false;
                self->UpdateEnableState();
            }
        } guard{this};

        auto cancel = [this]() -> bool { return cancel_requested_; };
        auto poll = [this]() { PumpUiMessages(); };
        client_.ConnectTarget(cancel, poll);
        const std::uint8_t fw = client_.GetFirmwareVersion();
        const std::uint32_t id = client_.GetDeviceId();
        SetTargetReady(true, L"Target Ready - load BIN and click Program APROM");
        AppendStatusText(L"ISP FW=0x" + FormatHex32(fw).substr(8) + L", DeviceID=" + FormatHex32(id));
    } catch (const std::exception& e) {
        SetTargetReady(false, IsCancelError(e) ? L"Target: Get Device ID stopped" : L"Target: Get Device ID failed");
        AppendStatusText(L"Get Device ID failed: " + AnsiToWide(e.what()));
        if (!IsCancelError(e)) {
            ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"Get Device ID", MB_ICONERROR | MB_OK);
        }
    }
}

void CUartIspTab::OnProgram() {
    try {
        EnsureUartConnected();
        if (!firmware_image_data_) {
            throw std::invalid_argument("load firmware image first");
        }
        const std::vector<std::uint8_t>& image = firmware_image_data_();
        if (image.empty()) {
            throw std::invalid_argument("load firmware image first");
        }

        cancel_requested_ = false;
        busy_ = true;
        UpdateEnableState();
        struct BusyGuard {
            CUartIspTab* self;
            ~BusyGuard() {
                self->busy_ = false;
                self->UpdateEnableState();
            }
        } guard{this};

        SetStatusText(L"Waiting for UART ISP handshake...");
        SetProgress(0, L"Connect: waiting for UART ISP...");
        SetTargetReady(false, L"Target: waiting for UART ISP handshake...");
        auto cancel = [this]() -> bool { return cancel_requested_; };
        auto poll = [this]() { PumpUiMessages(); };
        client_.ConnectTarget(cancel, poll);
        SetTargetReady(true, L"Target Ready - programming APROM...");
        AppendStatusText(L"Program APROM start. Image size=" + std::to_wstring(image.size()) + L" bytes");

        client_.ProgramAprom(
            image,
            [this](const mfc_tool::core::NuvoIspUartProgress& progress) {
                const int percent = progress.total_bytes == 0u
                    ? 0
                    : static_cast<int>((static_cast<unsigned long long>(progress.bytes_done) * 100ull) / progress.total_bytes);
                SetProgress(percent, L"Program: " + std::to_wstring(progress.bytes_done) + L"/" +
                                     std::to_wstring(progress.total_bytes) + L" bytes, packet " +
                                     std::to_wstring(progress.packet_index));
            },
            cancel,
            poll);

        SetProgress(100, L"Program: complete");
        SetTargetReady(true, L"Program complete - click Run APROM or program another image");
        AppendStatusText(L"Program APROM complete.");
        if (persist_settings_) {
            persist_settings_();
        }
    } catch (const std::exception& e) {
        SetTargetReady(false, IsCancelError(e) ? L"Target: program stopped" : L"Target: program failed");
        AppendStatusText(L"Program failed: " + AnsiToWide(e.what()));
        if (!IsCancelError(e)) {
            ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"Program APROM", MB_ICONERROR | MB_OK);
        }
    }
}

void CUartIspTab::OnRunAprom() {
    try {
        EnsureUartConnected();
        client_.RunAprom();
        SetTargetReady(false, L"Target: Run APROM command sent");
        AppendStatusText(L"Run APROM command sent.");
    } catch (const std::exception& e) {
        ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"Run APROM", MB_ICONERROR | MB_OK);
    }
}

void CUartIspTab::OnResetTarget() {
    try {
        EnsureUartConnected();
        client_.ResetTarget();
        SetTargetReady(false, L"Target: Reset Target command sent");
        AppendStatusText(L"Reset Target command sent.");
    } catch (const std::exception& e) {
        ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"Reset Target", MB_ICONERROR | MB_OK);
    }
}

void CUartIspTab::OnStop() {
    if (busy_) {
        cancel_requested_ = true;
        SetTargetReady(false, L"Target: stop requested");
        AppendStatusText(L"Stopping after current UART ISP transaction...");
        UpdateEnableState();
    }
}

std::wstring CUartIspTab::SelectedComPort() const {
    CString text;
    const_cast<CComboBox&>(com_combo_).GetWindowTextW(text);
    return text.GetString();
}

std::uint32_t CUartIspTab::SelectedBaudrate() const {
    CString text;
    const_cast<CComboBox&>(baud_combo_).GetWindowTextW(text);
    const std::wstring s = text.GetString();
    if (s == L"460800") return 460800u;
    if (s == L"921600") return 921600u;
    return 115200u;
}

int CUartIspTab::CurrentTimeoutMs() const {
    return std::clamp(mfc_tool::core::ParseInt(GetEditText(timeout_edit_)), 100, 120000);
}

std::wstring CUartIspTab::GetEditText(const CEdit& edit) const {
    CString s;
    const_cast<CEdit&>(edit).GetWindowTextW(s);
    return s.GetString();
}

void CUartIspTab::ApplyComboSelection(CComboBox& combo, const std::wstring& value) {
    if (!::IsWindow(combo.GetSafeHwnd()) || value.empty()) {
        return;
    }
    const int idx = combo.FindStringExact(-1, value.c_str());
    if (idx >= 0) {
        combo.SetCurSel(idx);
    }
}

std::wstring CUartIspTab::AnsiToWide(const char* text) {
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

std::wstring CUartIspTab::FormatHex32(std::uint32_t value) {
    std::wstringstream ss;
    ss << L"0x" << std::uppercase << std::hex << std::setw(8) << std::setfill(L'0') << value;
    return ss.str();
}
