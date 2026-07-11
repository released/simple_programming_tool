#include "usb_hid_isp_tab.h"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <utility>

#include "../core/text_utils.h"
#include "../hid/hid_scan.h"
#include "layout_utils.h"

namespace {

enum : UINT {
    IDC_USBHID_CONN_GROUP = 25000,
    IDC_USBHID_VID_LABEL,
    IDC_USBHID_VID_EDIT,
    IDC_USBHID_PID_LABEL,
    IDC_USBHID_PID_EDIT,
    IDC_USBHID_TIMEOUT_LABEL,
    IDC_USBHID_TIMEOUT_EDIT,
    IDC_USBHID_DEVICE_LABEL,
    IDC_USBHID_DEVICE_COMBO,
    IDC_USBHID_REFRESH,
    IDC_USBHID_CONNECT,
    IDC_USBHID_DISCONNECT,
    IDC_USBHID_ACTION_GROUP,
    IDC_USBHID_CONNECT_TARGET,
    IDC_USBHID_GET_DEVICE,
    IDC_USBHID_PROGRAM,
    IDC_USBHID_RUN_APROM,
    IDC_USBHID_RESET_TARGET,
    IDC_USBHID_STOP,
    IDC_USBHID_TARGET_STATE,
    IDC_USBHID_PROGRESS,
    IDC_USBHID_PROGRESS_LABEL,
    IDC_USBHID_STATUS_GROUP,
    IDC_USBHID_STATUS_EDIT,
};

constexpr std::uint16_t kNuvotonVid = 0x0416u;
constexpr std::uint16_t kNuvotonUsbIspPid = 0x3F00u;
constexpr std::uint16_t kNuvotonLegacyUsbIspPid = 0xA316u;

bool IsCancelError(const std::exception& e) {
    return std::string(e.what()).find("cancelled") != std::string::npos ||
        std::string(e.what()).find("canceled") != std::string::npos;
}

} // namespace

BEGIN_MESSAGE_MAP(CUsbHidIspTab, CWnd)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_BN_CLICKED(IDC_USBHID_REFRESH, &CUsbHidIspTab::OnRefreshHid)
    ON_BN_CLICKED(IDC_USBHID_CONNECT, &CUsbHidIspTab::OnConnectHid)
    ON_BN_CLICKED(IDC_USBHID_DISCONNECT, &CUsbHidIspTab::OnDisconnectHid)
    ON_BN_CLICKED(IDC_USBHID_CONNECT_TARGET, &CUsbHidIspTab::OnConnectTarget)
    ON_BN_CLICKED(IDC_USBHID_GET_DEVICE, &CUsbHidIspTab::OnGetDeviceId)
    ON_BN_CLICKED(IDC_USBHID_PROGRAM, &CUsbHidIspTab::OnProgram)
    ON_BN_CLICKED(IDC_USBHID_RUN_APROM, &CUsbHidIspTab::OnRunAprom)
    ON_BN_CLICKED(IDC_USBHID_RESET_TARGET, &CUsbHidIspTab::OnResetTarget)
    ON_BN_CLICKED(IDC_USBHID_STOP, &CUsbHidIspTab::OnStop)
    ON_WM_CTLCOLOR()
END_MESSAGE_MAP()

BOOL CUsbHidIspTab::Create(CWnd* parent, const RECT& rect, UINT id) {
    CString cls = AfxRegisterWndClass(CS_DBLCLKS, ::LoadCursor(nullptr, IDC_ARROW),
                                      reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1), nullptr);
    return CWnd::CreateEx(WS_EX_CONTROLPARENT, cls, L"UsbHidIspTab",
                          WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, rect, parent, id);
}

void CUsbHidIspTab::Bind(std::function<void(const std::wstring&)> logger,
                         std::function<const std::vector<std::uint8_t>&()> firmware_image_data,
                         std::function<std::wstring()> firmware_image_path,
                         std::function<void()> persist_settings) {
    log_ = std::move(logger);
    firmware_image_data_ = std::move(firmware_image_data);
    firmware_image_path_ = std::move(firmware_image_path);
    persist_settings_ = std::move(persist_settings);
    RefreshDevices();
}

int CUsbHidIspTab::OnCreate(LPCREATESTRUCT lpCreateStruct) {
    if (CWnd::OnCreate(lpCreateStruct) == -1) {
        return -1;
    }

    (void)mfc_tool::ui::CreatePointFontForWindow(ui_font_, *this, 90);
    target_ready_brush_.CreateSolidBrush(RGB(48, 158, 98));
    target_idle_brush_.CreateSolidBrush(RGB(226, 226, 226));
    auto fail = [this](BOOL ok, const wchar_t* name) -> bool {
        if (!ok && log_) {
            log_(std::wstring(L"Create USB HID ISP control failed: ") + name);
        }
        return ok != FALSE;
    };

    if (!fail(conn_group_.Create(L"USB HID ISP Connection", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, CRect(), this, IDC_USBHID_CONN_GROUP), L"connection group")) return -1;
    if (!fail(vid_label_.Create(L"VID", WS_CHILD | WS_VISIBLE, CRect(), this, IDC_USBHID_VID_LABEL), L"VID label")) return -1;
    if (!fail(vid_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, CRect(), this, IDC_USBHID_VID_EDIT), L"VID edit")) return -1;
    if (!fail(pid_label_.Create(L"PID", WS_CHILD | WS_VISIBLE, CRect(), this, IDC_USBHID_PID_LABEL), L"PID label")) return -1;
    if (!fail(pid_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, CRect(), this, IDC_USBHID_PID_EDIT), L"PID edit")) return -1;
    if (!fail(timeout_label_.Create(L"Timeout ms", WS_CHILD | WS_VISIBLE, CRect(), this, IDC_USBHID_TIMEOUT_LABEL), L"timeout label")) return -1;
    if (!fail(timeout_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, CRect(), this, IDC_USBHID_TIMEOUT_EDIT), L"timeout edit")) return -1;
    if (!fail(device_label_.Create(L"HID Device", WS_CHILD | WS_VISIBLE, CRect(), this, IDC_USBHID_DEVICE_LABEL), L"device label")) return -1;
    if (!fail(device_combo_.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, CRect(), this, IDC_USBHID_DEVICE_COMBO), L"device combo")) return -1;
    if (!fail(refresh_hid_btn_.Create(L"Refresh HID", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_USBHID_REFRESH), L"refresh HID")) return -1;
    if (!fail(connect_hid_btn_.Create(L"Connect HID", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_USBHID_CONNECT), L"connect HID")) return -1;
    if (!fail(disconnect_hid_btn_.Create(L"Disconnect HID", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_USBHID_DISCONNECT), L"disconnect HID")) return -1;

    if (!fail(action_group_.Create(L"Nuvoton USB HID ISP", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, CRect(), this, IDC_USBHID_ACTION_GROUP), L"action group")) return -1;
    if (!fail(connect_target_btn_.Create(L"Connect Target", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_USBHID_CONNECT_TARGET), L"connect target")) return -1;
    if (!fail(get_device_btn_.Create(L"Get Device ID", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_USBHID_GET_DEVICE), L"get device")) return -1;
    if (!fail(program_btn_.Create(L"Program APROM", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_USBHID_PROGRAM), L"program")) return -1;
    if (!fail(run_aprom_btn_.Create(L"Run APROM", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_USBHID_RUN_APROM), L"run aprom")) return -1;
    if (!fail(reset_target_btn_.Create(L"Reset Target", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_USBHID_RESET_TARGET), L"reset target")) return -1;
    if (!fail(stop_btn_.Create(L"Stop", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_USBHID_STOP), L"stop")) return -1;
    if (!fail(target_state_label_.Create(L"Target: not connected", WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE, CRect(), this, IDC_USBHID_TARGET_STATE), L"target state")) return -1;
    if (!fail(progress_.Create(WS_CHILD | WS_VISIBLE | PBS_SMOOTH, CRect(), this, IDC_USBHID_PROGRESS), L"progress")) return -1;
    if (!fail(progress_label_.Create(L"Progress: 0%", WS_CHILD | WS_VISIBLE, CRect(), this, IDC_USBHID_PROGRESS_LABEL), L"progress label")) return -1;

    if (!fail(status_group_.Create(L"Response", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, CRect(), this, IDC_USBHID_STATUS_GROUP), L"status group")) return -1;
    if (!fail(status_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_READONLY, CRect(), this, IDC_USBHID_STATUS_EDIT), L"status edit")) return -1;

    std::vector<CWnd*> controls = {
        &conn_group_, &vid_label_, &vid_edit_, &pid_label_, &pid_edit_, &timeout_label_, &timeout_edit_,
        &device_label_, &device_combo_, &refresh_hid_btn_, &connect_hid_btn_, &disconnect_hid_btn_,
        &action_group_, &connect_target_btn_, &get_device_btn_, &program_btn_, &run_aprom_btn_,
        &reset_target_btn_, &stop_btn_, &target_state_label_, &progress_, &progress_label_,
        &status_group_, &status_edit_
    };
    for (CWnd* w : controls) {
        w->SetFont(&ui_font_);
    }

    progress_.SetRange32(0, 100);
    LoadVisibleFromState();
    RefreshDevices();
    UpdateEnableState();
    return 0;
}

void CUsbHidIspTab::OnSize(UINT nType, int cx, int cy) {
    CWnd::OnSize(nType, cx, cy);
    if (!::IsWindow(conn_group_.GetSafeHwnd())) {
        return;
    }
    LayoutControls(CRect(0, 0, cx, cy));
    RedrawWindow(nullptr, nullptr,
                 RDW_INVALIDATE | RDW_ERASE | RDW_ERASENOW | RDW_FRAME | RDW_ALLCHILDREN | RDW_UPDATENOW);
}

void CUsbHidIspTab::RefreshDpiLayout() {
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

void CUsbHidIspTab::LayoutControls(const CRect& r) {
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
    const int right = r.right;
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
        const int refresh_w = (std::max)(dpi.Scale(104), mfc_tool::ui::MeasureButtonMinWidth(refresh_hid_btn_));
        const int connect_w = (std::max)(dpi.Scale(104), mfc_tool::ui::MeasureButtonMinWidth(connect_hid_btn_));
        const int disconnect_w = (std::max)(dpi.Scale(112), mfc_tool::ui::MeasureButtonMinWidth(disconnect_hid_btn_));

        x = mfc_tool::ui::PlaceLabelAndControl(vid_label_, vid_edit_, x, yy1 + label_y_pad, yy1, dpi.Scale(82), row, gap, label_pad, label_h) + gap;
        x = mfc_tool::ui::PlaceLabelAndControl(pid_label_, pid_edit_, x, yy1 + label_y_pad, yy1, dpi.Scale(82), row, gap, label_pad, label_h) + gap;
        x = mfc_tool::ui::PlaceLabelAndControl(timeout_label_, timeout_edit_, x, yy1 + label_y_pad, yy1, dpi.Scale(82), row, gap, label_pad, label_h) + gap;
        x += mfc_tool::ui::PlaceLabel(device_label_, x, yy1 + label_y_pad, label_pad, label_h) + gap;
        mfc_tool::ui::SafeMoveWindow(refresh_hid_btn_, right - margin - refresh_w, yy1, refresh_w, row);
        mfc_tool::ui::SafeMoveWindow(device_combo_, x, yy1, (std::max)(dpi.Scale(160), right - margin - refresh_w - gap - x), row + metrics.comboDrop120);

        x = inner_left;
        mfc_tool::ui::SafeMoveWindow(connect_hid_btn_, x, yy2, connect_w, row);
        x += connect_w + gap;
        mfc_tool::ui::SafeMoveWindow(disconnect_hid_btn_, x, yy2, disconnect_w, row);
    }

    y += conn_h + gap;
    mfc_tool::ui::SafeMoveWindow(action_group_, left + margin, y, available_w, action_h);
    x = inner_left;
    {
        const int yy = y + group_top_pad;
        const int connect_w = (std::max)(dpi.Scale(112), mfc_tool::ui::MeasureButtonMinWidth(connect_target_btn_));
        const int device_w = (std::max)(dpi.Scale(106), mfc_tool::ui::MeasureButtonMinWidth(get_device_btn_));
        const int program_w = (std::max)(dpi.Scale(118), mfc_tool::ui::MeasureButtonMinWidth(program_btn_));
        const int run_w = (std::max)(dpi.Scale(90), mfc_tool::ui::MeasureButtonMinWidth(run_aprom_btn_));
        const int reset_w = (std::max)(dpi.Scale(102), mfc_tool::ui::MeasureButtonMinWidth(reset_target_btn_));
        const int stop_w = (std::max)(dpi.Scale(68), mfc_tool::ui::MeasureButtonMinWidth(stop_btn_));

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

        mfc_tool::ui::SafeMoveWindow(target_state_label_, inner_left, yy + row + dpi.Scale(8), available_w - dpi.Scale(24), metrics.row24);
        mfc_tool::ui::SafeMoveWindow(progress_, inner_left, yy + row + dpi.Scale(38), available_w - dpi.Scale(24), label_h);
        mfc_tool::ui::SafeMoveWindow(progress_label_, inner_left, yy + row + dpi.Scale(60), available_w - dpi.Scale(24), label_h);
    }

    y += action_h + gap;
    mfc_tool::ui::SafeMoveWindow(status_group_, left + margin, y, available_w, (std::max)(dpi.Scale(80), bottom - y - margin));
    mfc_tool::ui::SafeMoveWindow(status_edit_, inner_left, y + group_top_pad, available_w - dpi.Scale(24),
                                 (std::max)(dpi.Scale(46), bottom - y - margin - group_top_pad - dpi.Scale(8)));
}

void CUsbHidIspTab::RefreshDevices() {
    if (!::IsWindow(device_combo_.GetSafeHwnd())) {
        return;
    }

    try {
        const std::uint16_t vid = CurrentVid();
        const std::uint16_t pid = CurrentPid();
        scanned_devices_ = mfc_tool::hid::EnumerateHidDevices(vid, pid, true);
        if (vid == kNuvotonVid && pid == kNuvotonUsbIspPid) {
            auto legacy = mfc_tool::hid::EnumerateHidDevices(vid, kNuvotonLegacyUsbIspPid, true);
            scanned_devices_.insert(scanned_devices_.end(), legacy.begin(), legacy.end());
        }
        std::sort(scanned_devices_.begin(), scanned_devices_.end(), [](const auto& a, const auto& b) {
            return DeviceLabel(a, 0) < DeviceLabel(b, 0);
        });

        device_combo_.ResetContent();
        device_combo_.AddString(L"Auto Select");
        for (size_t i = 0; i < scanned_devices_.size(); ++i) {
            device_combo_.AddString(DeviceLabel(scanned_devices_[i], static_cast<int>(i)).c_str());
        }
        if (!state_.device_label.empty()) {
            ApplyComboSelection(device_combo_, state_.device_label);
        }
        if (device_combo_.GetCurSel() == CB_ERR) {
            device_combo_.SetCurSel(scanned_devices_.empty() ? 0 : 1);
        }
        if (log_) {
            log_(L"USB HID ISP scan complete: " + std::to_wstring(scanned_devices_.size()) + L" found");
        }
    } catch (const std::exception& e) {
        if (log_) {
            log_(L"USB HID ISP refresh failed: " + AnsiToWide(e.what()));
        }
        ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"Refresh USB HID", MB_ICONERROR | MB_OK);
    }
}

void CUsbHidIspTab::UpdateEnableState() {
    if (!::IsWindow(connect_hid_btn_.GetSafeHwnd())) {
        return;
    }
    SaveVisibleToState();
    const bool connected = client_.IsConnected();
    const bool image_loaded = firmware_image_data_ && !firmware_image_data_().empty();
    mfc_tool::ui::SafeEnableWindow(vid_edit_, !connected && !busy_);
    mfc_tool::ui::SafeEnableWindow(pid_edit_, !connected && !busy_);
    mfc_tool::ui::SafeEnableWindow(timeout_edit_, !connected && !busy_);
    mfc_tool::ui::SafeEnableWindow(device_combo_, !connected && !busy_);
    mfc_tool::ui::SafeEnableWindow(refresh_hid_btn_, !connected && !busy_);
    mfc_tool::ui::SafeEnableWindow(connect_hid_btn_, !busy_);
    mfc_tool::ui::SafeEnableWindow(disconnect_hid_btn_, connected && !busy_);
    mfc_tool::ui::SafeEnableWindow(connect_target_btn_, !busy_);
    mfc_tool::ui::SafeEnableWindow(get_device_btn_, !busy_);
    mfc_tool::ui::SafeEnableWindow(program_btn_, !busy_ && image_loaded);
    mfc_tool::ui::SafeEnableWindow(run_aprom_btn_, connected && !busy_);
    mfc_tool::ui::SafeEnableWindow(reset_target_btn_, connected && !busy_);
    mfc_tool::ui::SafeEnableWindow(stop_btn_, busy_);
}

void CUsbHidIspTab::SaveVisibleToState() {
    if (loading_ || !::IsWindow(vid_edit_.GetSafeHwnd())) {
        return;
    }
    state_.vid = GetEditText(vid_edit_);
    state_.pid = GetEditText(pid_edit_);
    state_.timeout_ms = GetEditText(timeout_edit_);

    CString combo_text;
    device_combo_.GetWindowTextW(combo_text);
    state_.device_label = combo_text.GetString();
}

void CUsbHidIspTab::LoadVisibleFromState() {
    if (!::IsWindow(vid_edit_.GetSafeHwnd())) {
        return;
    }
    loading_ = true;
    vid_edit_.SetWindowTextW(state_.vid.c_str());
    pid_edit_.SetWindowTextW(state_.pid.c_str());
    timeout_edit_.SetWindowTextW(state_.timeout_ms.c_str());
    loading_ = false;
}

void CUsbHidIspTab::EnsureHidConnected() {
    if (!client_.IsConnected()) {
        throw std::runtime_error("USB HID is not connected.");
    }
}

CUsbHidIspTab::BootloaderConnectResult CUsbHidIspTab::ConnectBootloaderFlow() {
    SaveVisibleToState();
    BootloaderConnectResult result;
    const std::uint16_t vid = CurrentVid();
    const std::uint16_t pid = CurrentPid();
    const int timeout_ms = CurrentTimeoutMs();
    const std::wstring selected_path = SelectedDevicePath();
    std::string last_open_error;
    std::string last_connect_error;

    auto cancel = [this]() -> bool { return cancel_requested_; };
    auto poll = [this]() { PumpUiMessages(); };
    auto throw_if_cancelled = [this]() {
        if (cancel_requested_) {
            throw std::runtime_error("operation cancelled by user");
        }
    };
    auto sleep_with_ui = [&](int total_ms) {
        const int step_ms = 25;
        int elapsed_ms = 0;
        while (elapsed_ms < total_ms) {
            throw_if_cancelled();
            poll();
            const int wait_ms = (std::min)(step_ms, total_ms - elapsed_ms);
            std::this_thread::sleep_for(std::chrono::milliseconds(wait_ms));
            elapsed_ms += wait_ms;
        }
    };

    SetStatusText(L"Waiting for USB HID ISP bootloader...");
    SetProgress(0, L"Connect: waiting for bootloader HID");
    SetTargetReady(false, L"Waiting for bootloader HID");

    while (true) {
        throw_if_cancelled();

        if (!client_.IsConnected()) {
            SetTargetReady(false, L"Waiting for bootloader HID");
            SetProgress(5, L"Connect: waiting for bootloader HID");
            try {
                client_.Connect(vid, pid, selected_path, timeout_ms);
            } catch (const std::exception& e) {
                last_open_error = e.what();
                sleep_with_ui(250);
                continue;
            }
        }

        SetTargetReady(false, L"Handshaking CMD_CONNECT");
        SetProgress(20, L"Connect: handshaking CMD_CONNECT");
        if (client_.TryConnectTargetOnce(40, cancel, poll, &last_connect_error)) {
            result.fw_version = client_.GetFirmwareVersion();
            result.device_id = client_.GetDeviceId();
            result.config = client_.ReadConfig();
            result.config_valid = true;
            SetTargetReady(true, L"Connected");
            SetProgress(100, L"Connect: Connected");
            AppendStatusText(L"Connected. ISP FW=0x" + FormatHex32(result.fw_version).substr(8) +
                             L", DeviceID=" + FormatHex32(result.device_id));
            AppendStatusText(L"ReadConfig: CFG0=" + FormatHex32(result.config[0]) +
                             L", CFG1=" + FormatHex32(result.config[1]) +
                             L", CFG2=" + FormatHex32(result.config[2]) +
                             L", CFG3=" + FormatHex32(result.config[3]));
            return result;
        }
        throw_if_cancelled();

        client_.Disconnect();
        if (!last_open_error.empty() && !last_connect_error.empty()) {
            SetProgress(10, L"Connect: retrying HID probe");
        }
        sleep_with_ui(200);
    }
}

void CUsbHidIspTab::SetStatusText(const std::wstring& text) {
    if (::IsWindow(status_edit_.GetSafeHwnd())) {
        status_edit_.SetWindowTextW(text.c_str());
    }
}

void CUsbHidIspTab::AppendStatusText(const std::wstring& text) {
    if (!::IsWindow(status_edit_.GetSafeHwnd())) {
        return;
    }
    int len = status_edit_.GetWindowTextLengthW();
    status_edit_.SetSel(len, len);
    status_edit_.ReplaceSel((text + L"\r\n").c_str());
}

void CUsbHidIspTab::SetProgress(int value, const std::wstring& text) {
    if (::IsWindow(progress_.GetSafeHwnd())) {
        progress_.SetPos(std::clamp(value, 0, 100));
    }
    if (::IsWindow(progress_label_.GetSafeHwnd())) {
        progress_label_.SetWindowTextW(text.c_str());
    }
}

void CUsbHidIspTab::SetTargetReady(bool ready, const std::wstring& text) {
    target_ready_ = ready;
    if (::IsWindow(target_state_label_.GetSafeHwnd())) {
        const std::wstring label = text.empty()
            ? (ready ? L"Target Ready - load BIN and click Program APROM" : L"Target: not connected")
            : text;
        target_state_label_.SetWindowTextW(label.c_str());
        target_state_label_.Invalidate(FALSE);
    }
}

HBRUSH CUsbHidIspTab::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor) {
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

void CUsbHidIspTab::PumpUiMessages() {
    MSG msg;
    while (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE) != FALSE) {
        if (!AfxGetApp()->PreTranslateMessage(&msg)) {
            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);
        }
    }
}

void CUsbHidIspTab::LoadState(const mfc_tool::core::AppState& state) {
    state_ = state.usb_hid_isp;
    LoadVisibleFromState();
    RefreshDevices();
    UpdateEnableState();
}

void CUsbHidIspTab::SaveState(mfc_tool::core::AppState* state) const {
    if (state == nullptr) {
        return;
    }
    const_cast<CUsbHidIspTab*>(this)->SaveVisibleToState();
    state->usb_hid_isp = state_;
}

void CUsbHidIspTab::OnShutdown() {
    cancel_requested_ = true;
    client_.Disconnect();
    busy_ = false;
    SetTargetReady(false);
    UpdateEnableState();
}

void CUsbHidIspTab::RefreshSharedImageState() {
    UpdateEnableState();
}

void CUsbHidIspTab::OnRefreshHid() {
    RefreshDevices();
    UpdateEnableState();
}

void CUsbHidIspTab::OnConnectHid() {
    try {
        cancel_requested_ = false;
        busy_ = true;
        UpdateEnableState();
        struct BusyGuard {
            CUsbHidIspTab* self;
            ~BusyGuard() {
                self->busy_ = false;
                self->UpdateEnableState();
            }
        } guard{this};

        (void)ConnectBootloaderFlow();
        SetTargetReady(true, L"Target Ready - load BIN and click Program APROM");
        if (log_) {
            log_(L"USB HID ISP connected.");
        }
        if (persist_settings_) {
            persist_settings_();
        }
    } catch (const std::exception& e) {
        SetTargetReady(false, IsCancelError(e) ? L"Target: connect stopped" : L"Target: connect failed");
        AppendStatusText(L"Connect failed: " + AnsiToWide(e.what()));
        if (!IsCancelError(e)) {
            ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"Connect USB HID", MB_ICONERROR | MB_OK);
        }
    }
    UpdateEnableState();
}

void CUsbHidIspTab::OnDisconnectHid() {
    client_.Disconnect();
    SetTargetReady(false, L"Target: not connected");
    AppendStatusText(L"USB HID closed.");
    if (log_) {
        log_(L"USB HID ISP closed.");
    }
    UpdateEnableState();
}

void CUsbHidIspTab::OnConnectTarget() {
    try {
        cancel_requested_ = false;
        busy_ = true;
        UpdateEnableState();
        struct BusyGuard {
            CUsbHidIspTab* self;
            ~BusyGuard() {
                self->busy_ = false;
                self->UpdateEnableState();
            }
        } guard{this};

        (void)ConnectBootloaderFlow();
        SetTargetReady(true, L"Target Ready - load BIN and click Program APROM");
    } catch (const std::exception& e) {
        SetTargetReady(false, IsCancelError(e) ? L"Target: connect stopped" : L"Target: connect failed");
        AppendStatusText(L"Connect failed: " + AnsiToWide(e.what()));
        if (!IsCancelError(e)) {
            ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"Connect Target", MB_ICONERROR | MB_OK);
        }
    }
}

void CUsbHidIspTab::OnGetDeviceId() {
    try {
        cancel_requested_ = false;
        busy_ = true;
        UpdateEnableState();
        struct BusyGuard {
            CUsbHidIspTab* self;
            ~BusyGuard() {
                self->busy_ = false;
                self->UpdateEnableState();
            }
        } guard{this};

        const BootloaderConnectResult result = ConnectBootloaderFlow();
        SetTargetReady(true, L"Target Ready - load BIN and click Program APROM");
        AppendStatusText(L"ISP FW=0x" + FormatHex32(result.fw_version).substr(8) +
                         L", DeviceID=" + FormatHex32(result.device_id));
    } catch (const std::exception& e) {
        SetTargetReady(false, IsCancelError(e) ? L"Target: Get Device ID stopped" : L"Target: Get Device ID failed");
        AppendStatusText(L"Get Device ID failed: " + AnsiToWide(e.what()));
        if (!IsCancelError(e)) {
            ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"Get Device ID", MB_ICONERROR | MB_OK);
        }
    }
}

void CUsbHidIspTab::OnProgram() {
    try {
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
            CUsbHidIspTab* self;
            ~BusyGuard() {
                self->busy_ = false;
                self->UpdateEnableState();
            }
        } guard{this};

        auto cancel = [this]() -> bool { return cancel_requested_; };
        auto poll = [this]() { PumpUiMessages(); };
        (void)ConnectBootloaderFlow();
        SetTargetReady(true, L"Target Ready - programming APROM...");
        AppendStatusText(L"Program APROM start. Image size=" + std::to_wstring(image.size()) + L" bytes");

        client_.ProgramAprom(
            image,
            [this](const mfc_tool::core::NuvoIspUsbHidProgress& progress) {
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

void CUsbHidIspTab::OnRunAprom() {
    try {
        EnsureHidConnected();
        client_.RunAprom();
        SetTargetReady(false, L"Target: Run APROM command sent");
        AppendStatusText(L"Run APROM command sent.");
    } catch (const std::exception& e) {
        ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"Run APROM", MB_ICONERROR | MB_OK);
    }
}

void CUsbHidIspTab::OnResetTarget() {
    try {
        EnsureHidConnected();
        client_.ResetTarget();
        SetTargetReady(false, L"Target: Reset Target command sent");
        AppendStatusText(L"Reset Target command sent.");
    } catch (const std::exception& e) {
        ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"Reset Target", MB_ICONERROR | MB_OK);
    }
}

void CUsbHidIspTab::OnStop() {
    if (busy_) {
        cancel_requested_ = true;
        SetTargetReady(false, L"Target: stop requested");
        AppendStatusText(L"Stopping after current USB HID operation...");
        UpdateEnableState();
    }
}

std::uint16_t CUsbHidIspTab::CurrentVid() const {
    const int value = mfc_tool::core::ParseInt(GetEditText(vid_edit_));
    if (value < 0 || value > 0xFFFF) {
        throw std::invalid_argument("VID must be 0x0000..0xFFFF");
    }
    return static_cast<std::uint16_t>(value);
}

std::uint16_t CUsbHidIspTab::CurrentPid() const {
    const int value = mfc_tool::core::ParseInt(GetEditText(pid_edit_));
    if (value < 0 || value > 0xFFFF) {
        throw std::invalid_argument("PID must be 0x0000..0xFFFF");
    }
    return static_cast<std::uint16_t>(value);
}

int CUsbHidIspTab::CurrentTimeoutMs() const {
    return std::clamp(mfc_tool::core::ParseInt(GetEditText(timeout_edit_)), 100, 120000);
}

std::wstring CUsbHidIspTab::SelectedDevicePath() const {
    int sel = device_combo_.GetCurSel();
    if (sel == CB_ERR || sel <= 0) {
        return L"";
    }
    const size_t idx = static_cast<size_t>(sel - 1);
    return idx < scanned_devices_.size() ? scanned_devices_[idx].path : L"";
}

std::wstring CUsbHidIspTab::GetEditText(const CEdit& edit) const {
    CString s;
    const_cast<CEdit&>(edit).GetWindowTextW(s);
    return s.GetString();
}

void CUsbHidIspTab::ApplyComboSelection(CComboBox& combo, const std::wstring& value) {
    if (!::IsWindow(combo.GetSafeHwnd()) || value.empty()) {
        return;
    }
    const int idx = combo.FindStringExact(-1, value.c_str());
    if (idx >= 0) {
        combo.SetCurSel(idx);
    }
}

std::wstring CUsbHidIspTab::DeviceLabel(const mfc_tool::hid::DeviceInfo& d, int index) {
    std::wstringstream ss;
    ss << L"#" << index + 1 << L"  VID:PID ";
    ss << std::uppercase << std::hex;
    ss.width(4);
    ss.fill(L'0');
    ss << d.vendor_id << L":";
    ss.width(4);
    ss << d.product_id;
    ss << std::dec << L"  IN:" << d.input_report_len << L" OUT:" << d.output_report_len;
    if (!d.manufacturer.empty() || !d.product.empty()) {
        ss << L"  [" << d.manufacturer;
        if (!d.manufacturer.empty() && !d.product.empty()) {
            ss << L" - ";
        }
        ss << d.product << L"]";
    }
    return ss.str();
}

std::wstring CUsbHidIspTab::AnsiToWide(const char* text) {
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

std::wstring CUsbHidIspTab::FormatHex32(std::uint32_t value) {
    std::wstringstream ss;
    ss << L"0x" << std::uppercase << std::hex << std::setw(8) << std::setfill(L'0') << value;
    return ss.str();
}
