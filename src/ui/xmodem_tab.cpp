#include "xmodem_tab.h"

#include <afxdlgs.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <utility>

#include "../core/text_utils.h"
#include "layout_utils.h"

namespace {

enum : UINT {
    IDC_XMD_CONN_GROUP = 24000,
    IDC_XMD_COM_LABEL,
    IDC_XMD_COM_COMBO,
    IDC_XMD_BAUD_LABEL,
    IDC_XMD_BAUD_COMBO,
    IDC_XMD_TIMEOUT_LABEL,
    IDC_XMD_TIMEOUT_EDIT,
    IDC_XMD_REFRESH_COM,
    IDC_XMD_OPEN_COM,
    IDC_XMD_CLOSE_COM,
    IDC_XMD_FILE_GROUP,
    IDC_XMD_PATH_LABEL,
    IDC_XMD_PATH_EDIT,
    IDC_XMD_LOAD_IMAGE,
    IDC_XMD_FILE_INFO,
    IDC_XMD_ACTION_GROUP,
    IDC_XMD_SEND,
    IDC_XMD_CANCEL,
    IDC_XMD_TRANSFER_STATE,
    IDC_XMD_PROGRESS,
    IDC_XMD_PROGRESS_LABEL,
    IDC_XMD_STATUS_GROUP,
    IDC_XMD_STATUS_EDIT,
};

bool IsCancelError(const std::exception& e) {
    return std::string(e.what()).find("cancelled") != std::string::npos ||
        std::string(e.what()).find("canceled") != std::string::npos ||
        std::string(e.what()).find("cancelled") != std::string::npos;
}

} // namespace

BEGIN_MESSAGE_MAP(CXmodemTab, CWnd)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_BN_CLICKED(IDC_XMD_REFRESH_COM, &CXmodemTab::OnRefreshCom)
    ON_BN_CLICKED(IDC_XMD_OPEN_COM, &CXmodemTab::OnOpenCom)
    ON_BN_CLICKED(IDC_XMD_CLOSE_COM, &CXmodemTab::OnCloseCom)
    ON_BN_CLICKED(IDC_XMD_LOAD_IMAGE, &CXmodemTab::OnLoadImage)
    ON_BN_CLICKED(IDC_XMD_SEND, &CXmodemTab::OnSendXmodem)
    ON_BN_CLICKED(IDC_XMD_CANCEL, &CXmodemTab::OnCancel)
END_MESSAGE_MAP()

BOOL CXmodemTab::Create(CWnd* parent, const RECT& rect, UINT id) {
    CString cls = AfxRegisterWndClass(CS_DBLCLKS, ::LoadCursor(nullptr, IDC_ARROW),
                                      reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1), nullptr);
    return CWnd::CreateEx(WS_EX_CONTROLPARENT, cls, L"XmodemTab",
                          WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, rect, parent, id);
}

void CXmodemTab::Bind(std::function<void(const std::wstring&)> logger,
                      std::function<const std::vector<std::uint8_t>&()> firmware_image_data,
                      std::function<std::wstring()> firmware_image_path,
                      std::function<void()> persist_settings) {
    log_ = std::move(logger);
    firmware_image_data_ = std::move(firmware_image_data);
    firmware_image_path_ = std::move(firmware_image_path);
    persist_settings_ = std::move(persist_settings);
}

int CXmodemTab::OnCreate(LPCREATESTRUCT lpCreateStruct) {
    if (CWnd::OnCreate(lpCreateStruct) == -1) {
        return -1;
    }

    (void)mfc_tool::ui::CreatePointFontForWindow(ui_font_, *this, 90);
    auto fail = [this](BOOL ok, const wchar_t* name) -> bool {
        if (!ok && log_) {
            log_(std::wstring(L"Create XMODEM control failed: ") + name);
        }
        return ok != FALSE;
    };

    if (!fail(conn_group_.Create(L"XMODEM Connection", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, CRect(), this, IDC_XMD_CONN_GROUP), L"connection group")) return -1;
    if (!fail(com_label_.Create(L"COM", WS_CHILD | WS_VISIBLE, CRect(), this, IDC_XMD_COM_LABEL), L"COM label")) return -1;
    if (!fail(com_combo_.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, CRect(), this, IDC_XMD_COM_COMBO), L"COM combo")) return -1;
    if (!fail(baud_label_.Create(L"Baud", WS_CHILD | WS_VISIBLE, CRect(), this, IDC_XMD_BAUD_LABEL), L"baud label")) return -1;
    if (!fail(baud_combo_.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, CRect(), this, IDC_XMD_BAUD_COMBO), L"baud combo")) return -1;
    if (!fail(timeout_label_.Create(L"Handshake ms", WS_CHILD | WS_VISIBLE, CRect(), this, IDC_XMD_TIMEOUT_LABEL), L"timeout label")) return -1;
    if (!fail(timeout_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, CRect(), this, IDC_XMD_TIMEOUT_EDIT), L"timeout edit")) return -1;
    if (!fail(refresh_com_btn_.Create(L"Refresh COM", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_XMD_REFRESH_COM), L"refresh COM")) return -1;
    if (!fail(open_com_btn_.Create(L"Open UART", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_XMD_OPEN_COM), L"open UART")) return -1;
    if (!fail(close_com_btn_.Create(L"Close UART", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_XMD_CLOSE_COM), L"close UART")) return -1;

    if (!fail(action_group_.Create(L"XMODEM Transfer", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, CRect(), this, IDC_XMD_ACTION_GROUP), L"action group")) return -1;
    if (!fail(send_xmodem_btn_.Create(L"Send XMODEM", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_XMD_SEND), L"send XMODEM")) return -1;
    if (!fail(cancel_btn_.Create(L"Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_XMD_CANCEL), L"cancel")) return -1;
    if (!fail(transfer_state_label_.Create(L"Transfer: idle", WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE, CRect(), this, IDC_XMD_TRANSFER_STATE), L"transfer state")) return -1;
    if (!fail(progress_.Create(WS_CHILD | WS_VISIBLE | PBS_SMOOTH, CRect(), this, IDC_XMD_PROGRESS), L"progress")) return -1;
    if (!fail(progress_label_.Create(L"Progress: 0%", WS_CHILD | WS_VISIBLE, CRect(), this, IDC_XMD_PROGRESS_LABEL), L"progress label")) return -1;

    if (!fail(status_group_.Create(L"Response", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, CRect(), this, IDC_XMD_STATUS_GROUP), L"status group")) return -1;
    if (!fail(status_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_READONLY, CRect(), this, IDC_XMD_STATUS_EDIT), L"status edit")) return -1;

    std::vector<CWnd*> controls = {
        &conn_group_, &com_label_, &com_combo_, &baud_label_, &baud_combo_, &timeout_label_, &timeout_edit_,
        &refresh_com_btn_, &open_com_btn_, &close_com_btn_,
        &action_group_, &send_xmodem_btn_, &cancel_btn_, &transfer_state_label_, &progress_, &progress_label_,
        &status_group_, &status_edit_
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

void CXmodemTab::OnSize(UINT nType, int cx, int cy) {
    CWnd::OnSize(nType, cx, cy);
    if (!::IsWindow(conn_group_.GetSafeHwnd())) {
        return;
    }
    LayoutControls(CRect(0, 0, cx, cy));
    RedrawWindow(nullptr, nullptr,
                 RDW_INVALIDATE | RDW_ERASE | RDW_ERASENOW | RDW_FRAME | RDW_ALLCHILDREN | RDW_UPDATENOW);
}

void CXmodemTab::RefreshDpiLayout() {
    if (!::IsWindow(GetSafeHwnd())) {
        return;
    }
    (void)mfc_tool::ui::CreatePointFontForWindow(ui_font_, *this, 90);
    mfc_tool::ui::ApplyFontToChildWindows(*this, ui_font_, FALSE);
    CRect page;
    GetClientRect(&page);
    LayoutControls(page);
    RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN | RDW_UPDATENOW);
}

void CXmodemTab::LayoutControls(const CRect& r) {
    const mfc_tool::ui::DpiScaler dpi = mfc_tool::ui::DpiScaler::FromWindow(*this);
    const mfc_tool::ui::LayoutMetrics metrics = mfc_tool::ui::MetricsForWindow(*this);
    const int margin = metrics.margin8;
    const int gap = metrics.gap;
    const int row = metrics.row26;
    const int label_y_pad = dpi.Scale(4);
    const int group_top_pad = metrics.groupTopPad;
    const int label_h = metrics.label18;
    const int label_pad = dpi.Scale(8);
    const int left = r.left;
    const int bottom = r.bottom;
    const int inner_left = r.left + margin + dpi.Scale(12);
    const int available_w = (std::max)(dpi.Scale(240), r.Width() - margin * 2);
    const int conn_h = dpi.Scale(104);
    const int action_h = dpi.Scale(132);
    int x;
    int y = r.top + margin;

    mfc_tool::ui::SafeMoveWindow(conn_group_, left + margin, y, available_w, conn_h);
    x = inner_left;
    {
        const int yy1 = y + group_top_pad;
        const int yy2 = yy1 + row + dpi.Scale(8);
        x = mfc_tool::ui::PlaceLabelAndControl(com_label_, com_combo_, x, yy1 + label_y_pad, yy1, dpi.Scale(110), row + metrics.comboDrop120, gap, label_pad, label_h) + gap;
        x = mfc_tool::ui::PlaceLabelAndControl(baud_label_, baud_combo_, x, yy1 + label_y_pad, yy1, dpi.Scale(106), row + metrics.comboDrop120, gap, label_pad, label_h) + gap;
        x = mfc_tool::ui::PlaceLabelAndControl(timeout_label_, timeout_edit_, x, yy1 + label_y_pad, yy1, dpi.Scale(80), row, gap, label_pad, label_h) + gap;
        mfc_tool::ui::SafeMoveWindow(refresh_com_btn_, x, yy1, (std::max)(dpi.Scale(104), mfc_tool::ui::MeasureButtonMinWidth(refresh_com_btn_)), row);

        x = inner_left;
        mfc_tool::ui::SafeMoveWindow(open_com_btn_, x, yy2, (std::max)(dpi.Scale(94), mfc_tool::ui::MeasureButtonMinWidth(open_com_btn_)), row);
        x += (std::max)(dpi.Scale(94), mfc_tool::ui::MeasureButtonMinWidth(open_com_btn_)) + gap;
        mfc_tool::ui::SafeMoveWindow(close_com_btn_, x, yy2, (std::max)(dpi.Scale(94), mfc_tool::ui::MeasureButtonMinWidth(close_com_btn_)), row);
    }

    y += conn_h + gap;
    mfc_tool::ui::SafeMoveWindow(action_group_, left + margin, y, available_w, action_h);
    x = inner_left;
    {
        const int yy = y + group_top_pad;
        const int send_w = (std::max)(dpi.Scale(112), mfc_tool::ui::MeasureButtonMinWidth(send_xmodem_btn_));
        const int cancel_w = (std::max)(dpi.Scale(78), mfc_tool::ui::MeasureButtonMinWidth(cancel_btn_));
        mfc_tool::ui::SafeMoveWindow(send_xmodem_btn_, x, yy, send_w, row);
        x += send_w + gap;
        mfc_tool::ui::SafeMoveWindow(cancel_btn_, x, yy, cancel_w, row);
        mfc_tool::ui::SafeMoveWindow(transfer_state_label_, inner_left, yy + row + dpi.Scale(8), available_w - dpi.Scale(24), metrics.row24);
        mfc_tool::ui::SafeMoveWindow(progress_, inner_left, yy + row + dpi.Scale(38), available_w - dpi.Scale(24), label_h);
        mfc_tool::ui::SafeMoveWindow(progress_label_, inner_left, yy + row + dpi.Scale(60), available_w - dpi.Scale(24), label_h);
    }

    y += action_h + gap;
    mfc_tool::ui::SafeMoveWindow(status_group_, left + margin, y, available_w, (std::max)(dpi.Scale(80), bottom - y - margin));
    mfc_tool::ui::SafeMoveWindow(status_edit_, inner_left, y + group_top_pad, available_w - dpi.Scale(24),
                                 (std::max)(dpi.Scale(46), bottom - y - margin - group_top_pad - dpi.Scale(8)));
}

void CXmodemTab::RefreshComPorts() {
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

void CXmodemTab::PopulateBaudCombo() {
    baud_combo_.ResetContent();
    baud_combo_.AddString(L"115200");
    baud_combo_.AddString(L"460800");
    baud_combo_.AddString(L"921600");
    baud_combo_.SetCurSel(0);
}

void CXmodemTab::UpdateEnableState() {
    if (!::IsWindow(open_com_btn_.GetSafeHwnd())) {
        return;
    }
    SaveVisibleToState();
    const bool connected = sender_.IsConnected();
    const bool image_loaded = firmware_image_data_ && !firmware_image_data_().empty();
    mfc_tool::ui::SafeEnableWindow(com_combo_, !connected && !busy_);
    mfc_tool::ui::SafeEnableWindow(baud_combo_, !connected && !busy_);
    mfc_tool::ui::SafeEnableWindow(timeout_edit_, !busy_);
    mfc_tool::ui::SafeEnableWindow(refresh_com_btn_, !connected && !busy_);
    mfc_tool::ui::SafeEnableWindow(open_com_btn_, !connected && !busy_);
    mfc_tool::ui::SafeEnableWindow(close_com_btn_, connected && !busy_);
    mfc_tool::ui::SafeEnableWindow(send_xmodem_btn_, connected && !busy_ && image_loaded);
    mfc_tool::ui::SafeEnableWindow(cancel_btn_, busy_);
}

void CXmodemTab::SaveVisibleToState() {
    if (loading_ || !::IsWindow(com_combo_.GetSafeHwnd())) {
        return;
    }
    state_.com_port = SelectedComPort();
    state_.baud = std::to_wstring(SelectedBaudrate());
    state_.handshake_timeout_ms = GetEditText(timeout_edit_);
}

void CXmodemTab::LoadVisibleFromState() {
    if (!::IsWindow(com_combo_.GetSafeHwnd())) {
        return;
    }
    loading_ = true;
    ApplyComboSelection(com_combo_, state_.com_port);
    ApplyComboSelection(baud_combo_, state_.baud);
    timeout_edit_.SetWindowTextW(state_.handshake_timeout_ms.c_str());
    loading_ = false;
}

void CXmodemTab::EnsureConnected() {
    if (!sender_.IsConnected()) {
        throw std::runtime_error("XMODEM UART is not open.");
    }
}

void CXmodemTab::LoadImageFromPath(const std::wstring& path) {
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
        log_(L"XMODEM image loaded: " + path + L" (" + std::to_wstring(image_data_.size()) + L" bytes)");
    }
}

void CXmodemTab::SetImagePathText() {
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

void CXmodemTab::SetStatusText(const std::wstring& text) {
    if (::IsWindow(status_edit_.GetSafeHwnd())) {
        status_edit_.SetWindowTextW(text.c_str());
    }
}

void CXmodemTab::AppendStatusText(const std::wstring& text) {
    if (!::IsWindow(status_edit_.GetSafeHwnd())) {
        return;
    }
    int len = status_edit_.GetWindowTextLengthW();
    status_edit_.SetSel(len, len);
    status_edit_.ReplaceSel((text + L"\r\n").c_str());
}

void CXmodemTab::SetProgress(int value, const std::wstring& text) {
    if (::IsWindow(progress_.GetSafeHwnd())) {
        progress_.SetPos(std::clamp(value, 0, 100));
    }
    if (::IsWindow(progress_label_.GetSafeHwnd())) {
        progress_label_.SetWindowTextW(text.c_str());
    }
}

void CXmodemTab::PumpUiMessages() {
    MSG msg;
    while (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE) != FALSE) {
        if (!AfxGetApp()->PreTranslateMessage(&msg)) {
            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);
        }
    }
}

void CXmodemTab::LoadState(const mfc_tool::core::AppState& state) {
    state_ = state.xmodem;
    LoadVisibleFromState();
    UpdateEnableState();
}

void CXmodemTab::SaveState(mfc_tool::core::AppState* state) const {
    if (state == nullptr) {
        return;
    }
    const_cast<CXmodemTab*>(this)->SaveVisibleToState();
    state->xmodem = state_;
}

void CXmodemTab::OnShutdown() {
    cancel_requested_ = true;
    sender_.CancelTransfer();
    sender_.Disconnect();
    busy_ = false;
    UpdateEnableState();
}

void CXmodemTab::RefreshSharedImageState() {
    UpdateEnableState();
}

void CXmodemTab::OnRefreshCom() {
    RefreshComPorts();
    UpdateEnableState();
}

void CXmodemTab::OnOpenCom() {
    try {
        sender_.Connect(SelectedComPort(), SelectedBaudrate());
        AppendStatusText(L"XMODEM UART opened: " + SelectedComPort() + L", baud=" + std::to_wstring(SelectedBaudrate()));
        if (log_) {
            log_(L"XMODEM UART opened: " + SelectedComPort());
        }
        if (persist_settings_) {
            persist_settings_();
        }
    } catch (const std::exception& e) {
        ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"Open UART", MB_ICONERROR | MB_OK);
    }
    UpdateEnableState();
}

void CXmodemTab::OnCloseCom() {
    sender_.Disconnect();
    AppendStatusText(L"XMODEM UART closed.");
    if (log_) {
        log_(L"XMODEM UART closed.");
    }
    UpdateEnableState();
}

void CXmodemTab::OnLoadImage() {
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

void CXmodemTab::OnSendXmodem() {
    try {
        EnsureConnected();
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
            CXmodemTab* self;
            ~BusyGuard() {
                self->busy_ = false;
                self->UpdateEnableState();
            }
        } guard{this};

        mfc_tool::core::XmodemOptions options;
        options.handshake_timeout_ms = CurrentHandshakeTimeoutMs();
        sender_.SetOptions(options);

        SetStatusText(L"Waiting for target to start XMODEM transfer...");
        transfer_state_label_.SetWindowTextW(L"Transfer: waiting for target...");
        SetProgress(0, L"Progress: 0%");
        AppendStatusText(L"XMODEM send start. Image size=" + std::to_wstring(image.size()) + L" bytes");

        auto cancel = [this]() -> bool { return cancel_requested_; };
        auto poll = [this]() { PumpUiMessages(); };
        sender_.Send(
            image,
            [this](const mfc_tool::core::XmodemProgress& progress) {
                const int percent = progress.total_bytes == 0u
                    ? 0
                    : static_cast<int>((static_cast<unsigned long long>(progress.bytes_done) * 100ull) / progress.total_bytes);
                SetProgress(percent, L"XMODEM: " + std::to_wstring(progress.bytes_done) + L"/" +
                                     std::to_wstring(progress.total_bytes) + L" bytes, block " +
                                     std::to_wstring(progress.block_index));
            },
            cancel,
            poll);

        SetProgress(100, L"XMODEM: complete");
        transfer_state_label_.SetWindowTextW(L"Transfer: complete");
        AppendStatusText(L"XMODEM transfer complete.");
        if (persist_settings_) {
            persist_settings_();
        }
    } catch (const std::exception& e) {
        transfer_state_label_.SetWindowTextW(IsCancelError(e) ? L"Transfer: stopped" : L"Transfer: failed");
        AppendStatusText(L"XMODEM failed: " + AnsiToWide(e.what()));
        if (!IsCancelError(e)) {
            ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"Send XMODEM", MB_ICONERROR | MB_OK);
        }
    }
}

void CXmodemTab::OnCancel() {
    if (busy_) {
        cancel_requested_ = true;
        sender_.CancelTransfer();
        transfer_state_label_.SetWindowTextW(L"Transfer: cancel requested");
        AppendStatusText(L"XMODEM cancel requested.");
        UpdateEnableState();
    }
}

std::wstring CXmodemTab::SelectedComPort() const {
    CString text;
    const_cast<CComboBox&>(com_combo_).GetWindowTextW(text);
    return text.GetString();
}

std::uint32_t CXmodemTab::SelectedBaudrate() const {
    CString text;
    const_cast<CComboBox&>(baud_combo_).GetWindowTextW(text);
    const std::wstring s = text.GetString();
    if (s == L"460800") return 460800u;
    if (s == L"921600") return 921600u;
    return 115200u;
}

int CXmodemTab::CurrentHandshakeTimeoutMs() const {
    return std::clamp(mfc_tool::core::ParseInt(GetEditText(timeout_edit_)), 1000, 300000);
}

std::wstring CXmodemTab::GetEditText(const CEdit& edit) const {
    CString s;
    const_cast<CEdit&>(edit).GetWindowTextW(s);
    return s.GetString();
}

void CXmodemTab::ApplyComboSelection(CComboBox& combo, const std::wstring& value) {
    if (!::IsWindow(combo.GetSafeHwnd()) || value.empty()) {
        return;
    }
    const int idx = combo.FindStringExact(-1, value.c_str());
    if (idx >= 0) {
        combo.SetCurSel(idx);
    }
}

std::wstring CXmodemTab::AnsiToWide(const char* text) {
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
