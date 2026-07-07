#include "fw_upload_tab.h"

#include <afxdlgs.h>

#include <algorithm>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "../core/nuvo_isp_i2c.h"
#include "../core/text_utils.h"
#include "layout_utils.h"

namespace {

enum : UINT {
    IDC_FWU_CONFIG_GROUP = 20100,
    IDC_FWU_PORT_LABEL,
    IDC_FWU_PORT_COMBO,
    IDC_FWU_PINS_LABEL,
    IDC_FWU_PINS_COMBO,
    IDC_FWU_SPEED_LABEL,
    IDC_FWU_SPEED_EDIT,
    IDC_FWU_ADDR_LABEL,
    IDC_FWU_ADDR_EDIT,
    IDC_FWU_DELAY_LABEL,
    IDC_FWU_DELAY_EDIT,
    IDC_FWU_ENABLE_HOST,
    IDC_FWU_DISABLE_HOST,
    IDC_FWU_FILE_GROUP,
    IDC_FWU_PATH_LABEL,
    IDC_FWU_PATH_EDIT,
    IDC_FWU_LOAD_IMAGE,
    IDC_FWU_FILE_INFO,
    IDC_FWU_ACTION_GROUP,
    IDC_FWU_CONNECT_TARGET,
    IDC_FWU_GET_DEVICE,
    IDC_FWU_PROGRAM,
    IDC_FWU_RUN_APROM,
    IDC_FWU_RESET_TARGET,
    IDC_FWU_STOP,
    IDC_FWU_TARGET_STATE,
    IDC_FWU_PROGRESS,
    IDC_FWU_PROGRESS_LABEL,
    IDC_FWU_STATUS_GROUP,
    IDC_FWU_STATUS_EDIT,
    IDC_FWU_HID_GROUP,
    IDC_FWU_HID_CHECK,
    IDC_FWU_VID_LABEL,
    IDC_FWU_VID_EDIT,
    IDC_FWU_PID_LABEL,
    IDC_FWU_PID_EDIT,
    IDC_FWU_TIMEOUT_LABEL,
    IDC_FWU_TIMEOUT_EDIT,
    IDC_FWU_REFRESH_HID,
    IDC_FWU_CONNECT_HID,
    IDC_FWU_DISCONNECT_HID,
    IDC_FWU_DEVICE_LABEL,
    IDC_FWU_DEVICE_COMBO,
    IDC_FWU_VCOM_GROUP,
    IDC_FWU_VCOM_CHECK,
    IDC_FWU_VCOM_LABEL,
    IDC_FWU_VCOM_COMBO,
    IDC_FWU_VCOM_BAUD_LABEL,
    IDC_FWU_VCOM_BAUD_COMBO,
    IDC_FWU_REFRESH_VCOM,
    IDC_FWU_CONNECT_VCOM,
    IDC_FWU_DISCONNECT_VCOM,
    IDC_FWU_BRIDGE_STATUS_TITLE,
    IDC_FWU_BRIDGE_STATUS_CHIP,
    IDC_FWU_GET_INFO,
    IDC_FWU_PING,
    IDC_FWU_RESET_MCU,
    IDC_FWU_FW_INFO_TITLE,
    IDC_FWU_FW_INFO_VALUE,
};

constexpr const wchar_t* kFwUploadOwner = L"FW-UPLOAD-M";

bool IsCancelError(const std::exception& e) {
    return std::string(e.what()).find("cancelled") != std::string::npos ||
        std::string(e.what()).find("canceled") != std::string::npos;
}

} // namespace

BEGIN_MESSAGE_MAP(CFwUploadTab, CWnd)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_CBN_SELCHANGE(IDC_FWU_PORT_COMBO, &CFwUploadTab::OnPortChanged)
    ON_CBN_SELCHANGE(IDC_FWU_PINS_COMBO, &CFwUploadTab::OnPinsChanged)
    ON_BN_CLICKED(IDC_FWU_ENABLE_HOST, &CFwUploadTab::OnEnableHost)
    ON_BN_CLICKED(IDC_FWU_DISABLE_HOST, &CFwUploadTab::OnDisableHost)
    ON_BN_CLICKED(IDC_FWU_LOAD_IMAGE, &CFwUploadTab::OnLoadImage)
    ON_BN_CLICKED(IDC_FWU_CONNECT_TARGET, &CFwUploadTab::OnConnectTarget)
    ON_BN_CLICKED(IDC_FWU_GET_DEVICE, &CFwUploadTab::OnGetDeviceId)
    ON_BN_CLICKED(IDC_FWU_PROGRAM, &CFwUploadTab::OnProgram)
    ON_BN_CLICKED(IDC_FWU_RUN_APROM, &CFwUploadTab::OnRunAprom)
    ON_BN_CLICKED(IDC_FWU_RESET_TARGET, &CFwUploadTab::OnResetTarget)
    ON_BN_CLICKED(IDC_FWU_STOP, &CFwUploadTab::OnStop)
    ON_BN_CLICKED(IDC_FWU_REFRESH_HID, &CFwUploadTab::OnBridgeRefreshHid)
    ON_BN_CLICKED(IDC_FWU_CONNECT_HID, &CFwUploadTab::OnBridgeConnectHid)
    ON_BN_CLICKED(IDC_FWU_DISCONNECT_HID, &CFwUploadTab::OnBridgeDisconnectHid)
    ON_BN_CLICKED(IDC_FWU_GET_INFO, &CFwUploadTab::OnBridgeGetInfo)
    ON_BN_CLICKED(IDC_FWU_PING, &CFwUploadTab::OnBridgePing)
    ON_BN_CLICKED(IDC_FWU_RESET_MCU, &CFwUploadTab::OnBridgeResetMcu)
    ON_BN_CLICKED(IDC_FWU_REFRESH_VCOM, &CFwUploadTab::OnBridgeRefreshVcom)
    ON_BN_CLICKED(IDC_FWU_CONNECT_VCOM, &CFwUploadTab::OnBridgeConnectVcom)
    ON_BN_CLICKED(IDC_FWU_DISCONNECT_VCOM, &CFwUploadTab::OnBridgeDisconnectVcom)
    ON_BN_CLICKED(IDC_FWU_HID_CHECK, &CFwUploadTab::OnBridgeSelectHid)
    ON_BN_CLICKED(IDC_FWU_VCOM_CHECK, &CFwUploadTab::OnBridgeSelectVcom)
    ON_CBN_SELCHANGE(IDC_FWU_VCOM_COMBO, &CFwUploadTab::OnBridgeVcomChanged)
    ON_CBN_SELCHANGE(IDC_FWU_VCOM_BAUD_COMBO, &CFwUploadTab::OnBridgeVcomBaudChanged)
    ON_WM_CTLCOLOR()
END_MESSAGE_MAP()

BOOL CFwUploadTab::Create(CWnd* parent, const RECT& rect, UINT id) {
    CString cls = AfxRegisterWndClass(CS_DBLCLKS, ::LoadCursor(nullptr, IDC_ARROW),
                                      reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1), nullptr);
    return CWnd::CreateEx(WS_EX_CONTROLPARENT, cls, L"FWUploadTab",
                          WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, rect, parent, id);
}

void CFwUploadTab::Bind(mfc_tool::core::BridgeService* service,
                        std::function<void(const std::wstring&)> logger,
                        mfc_tool::core::PinUsageRegistry* pin_usage,
                        std::function<const std::vector<std::uint8_t>&()> firmware_image_data,
                        std::function<std::wstring()> firmware_image_path,
                        std::function<void()> persist_settings) {
    service_ = service;
    log_ = std::move(logger);
    pin_usage_ = pin_usage;
    firmware_image_data_ = std::move(firmware_image_data);
    firmware_image_path_ = std::move(firmware_image_path);
    persist_settings_ = std::move(persist_settings);
    RefreshPinUsage();
    RefreshDevices();
    RefreshVcomPorts();
    SetBridgeConnectionUi();
}

int CFwUploadTab::OnCreate(LPCREATESTRUCT lpCreateStruct) {
    if (CWnd::OnCreate(lpCreateStruct) == -1) {
        return -1;
    }

    ui_font_.CreatePointFont(90, L"Segoe UI");
    bridge_status_brush_.CreateSolidBrush(bridge_status_color_);
    target_ready_brush_.CreateSolidBrush(RGB(48, 158, 98));
    target_idle_brush_.CreateSolidBrush(RGB(226, 226, 226));

    auto fail = [this](BOOL ok, const wchar_t* name) -> bool {
        if (!ok && log_) {
            log_(std::wstring(L"Create FW Upload control failed: ") + name);
        }
        return ok != FALSE;
    };

    if (!fail(hid_group_.Create(L"I2C Bridge - HID", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, CRect(), this, IDC_FWU_HID_GROUP), L"HID group")) return -1;
    if (!fail(hid_check_.Create(L"HID", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, CRect(), this, IDC_FWU_HID_CHECK), L"HID checkbox")) return -1;
    if (!fail(vid_label_.Create(L"HID VID", WS_CHILD | WS_VISIBLE, CRect(), this, IDC_FWU_VID_LABEL), L"VID label")) return -1;
    if (!fail(vid_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, CRect(), this, IDC_FWU_VID_EDIT), L"VID edit")) return -1;
    if (!fail(pid_label_.Create(L"HID PID", WS_CHILD | WS_VISIBLE, CRect(), this, IDC_FWU_PID_LABEL), L"PID label")) return -1;
    if (!fail(pid_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, CRect(), this, IDC_FWU_PID_EDIT), L"PID edit")) return -1;
    if (!fail(timeout_label_.Create(L"Timeout(ms)", WS_CHILD | WS_VISIBLE, CRect(), this, IDC_FWU_TIMEOUT_LABEL), L"timeout label")) return -1;
    if (!fail(timeout_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, CRect(), this, IDC_FWU_TIMEOUT_EDIT), L"timeout edit")) return -1;
    if (!fail(refresh_btn_.Create(L"Refresh", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_FWU_REFRESH_HID), L"refresh HID")) return -1;
    if (!fail(connect_btn_.Create(L"Connect HID", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_FWU_CONNECT_HID), L"connect HID")) return -1;
    if (!fail(disconnect_btn_.Create(L"Disconnect HID", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_FWU_DISCONNECT_HID), L"disconnect HID")) return -1;
    if (!fail(device_label_.Create(L"HID Device", WS_CHILD | WS_VISIBLE, CRect(), this, IDC_FWU_DEVICE_LABEL), L"device label")) return -1;
    if (!fail(device_combo_.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, CRect(), this, IDC_FWU_DEVICE_COMBO), L"device combo")) return -1;

    if (!fail(vcom_group_.Create(L"I2C Bridge - VCOM", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, CRect(), this, IDC_FWU_VCOM_GROUP), L"VCOM group")) return -1;
    if (!fail(vcom_check_.Create(L"VCOM", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, CRect(), this, IDC_FWU_VCOM_CHECK), L"VCOM checkbox")) return -1;
    if (!fail(vcom_label_.Create(L"COM", WS_CHILD | WS_VISIBLE, CRect(), this, IDC_FWU_VCOM_LABEL), L"VCOM label")) return -1;
    if (!fail(vcom_combo_.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, CRect(), this, IDC_FWU_VCOM_COMBO), L"VCOM combo")) return -1;
    if (!fail(vcom_baud_label_.Create(L"Baud", WS_CHILD | WS_VISIBLE, CRect(), this, IDC_FWU_VCOM_BAUD_LABEL), L"VCOM baud label")) return -1;
    if (!fail(vcom_baud_combo_.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, CRect(), this, IDC_FWU_VCOM_BAUD_COMBO), L"VCOM baud combo")) return -1;
    if (!fail(refresh_vcom_btn_.Create(L"Refresh COM", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_FWU_REFRESH_VCOM), L"refresh VCOM")) return -1;
    if (!fail(connect_vcom_btn_.Create(L"Open VCOM", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_FWU_CONNECT_VCOM), L"open VCOM")) return -1;
    if (!fail(disconnect_vcom_btn_.Create(L"Close VCOM", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_FWU_DISCONNECT_VCOM), L"close VCOM")) return -1;

    if (!fail(bridge_status_title_.Create(L"Bridge:", WS_CHILD | WS_VISIBLE, CRect(), this, IDC_FWU_BRIDGE_STATUS_TITLE), L"bridge status title")) return -1;
    if (!fail(bridge_status_chip_.Create(L"Disconnected", WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE, CRect(), this, IDC_FWU_BRIDGE_STATUS_CHIP), L"bridge status chip")) return -1;
    if (!fail(get_info_btn_.Create(L"Get Info", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_FWU_GET_INFO), L"get info")) return -1;
    if (!fail(ping_btn_.Create(L"Ping", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_FWU_PING), L"ping")) return -1;
    if (!fail(reset_mcu_btn_.Create(L"Reset MCU", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_FWU_RESET_MCU), L"reset MCU")) return -1;
    if (!fail(fw_info_title_.Create(L"FW:", WS_CHILD | WS_VISIBLE, CRect(), this, IDC_FWU_FW_INFO_TITLE), L"FW info title")) return -1;
    if (!fail(fw_info_value_.Create(L"-", WS_CHILD | WS_VISIBLE | SS_PATHELLIPSIS, CRect(), this, IDC_FWU_FW_INFO_VALUE), L"FW info value")) return -1;

    if (!fail(config_group_.Create(L"I2C Host", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, CRect(), this, IDC_FWU_CONFIG_GROUP), L"config group")) return -1;
    if (!fail(port_label_.Create(L"Port", WS_CHILD | WS_VISIBLE, CRect(), this, IDC_FWU_PORT_LABEL), L"port label")) return -1;
    if (!fail(port_combo_.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, CRect(), this, IDC_FWU_PORT_COMBO), L"port combo")) return -1;
    if (!fail(pins_label_.Create(L"Pins", WS_CHILD | WS_VISIBLE, CRect(), this, IDC_FWU_PINS_LABEL), L"pins label")) return -1;
    if (!fail(pins_combo_.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, CRect(), this, IDC_FWU_PINS_COMBO), L"pins combo")) return -1;
    if (!fail(speed_label_.Create(L"Speed", WS_CHILD | WS_VISIBLE, CRect(), this, IDC_FWU_SPEED_LABEL), L"speed label")) return -1;
    if (!fail(speed_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, CRect(), this, IDC_FWU_SPEED_EDIT), L"speed edit")) return -1;
    if (!fail(addr_label_.Create(L"Target", WS_CHILD | WS_VISIBLE, CRect(), this, IDC_FWU_ADDR_LABEL), L"addr label")) return -1;
    if (!fail(addr_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, CRect(), this, IDC_FWU_ADDR_EDIT), L"addr edit")) return -1;
    if (!fail(delay_label_.Create(L"Delay ms", WS_CHILD | WS_VISIBLE, CRect(), this, IDC_FWU_DELAY_LABEL), L"delay label")) return -1;
    if (!fail(delay_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, CRect(), this, IDC_FWU_DELAY_EDIT), L"delay edit")) return -1;
    if (!fail(enable_host_btn_.Create(L"Enable Host", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_FWU_ENABLE_HOST), L"enable host")) return -1;
    if (!fail(disable_host_btn_.Create(L"Disable Host", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_FWU_DISABLE_HOST), L"disable host")) return -1;

    if (!fail(action_group_.Create(L"Nuvoton I2C ISP", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, CRect(), this, IDC_FWU_ACTION_GROUP), L"action group")) return -1;
    if (!fail(connect_target_btn_.Create(L"Connect Target", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_FWU_CONNECT_TARGET), L"connect target")) return -1;
    if (!fail(get_device_btn_.Create(L"Get Device ID", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_FWU_GET_DEVICE), L"get device")) return -1;
    if (!fail(program_btn_.Create(L"Program APROM", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_FWU_PROGRAM), L"program")) return -1;
    if (!fail(run_aprom_btn_.Create(L"Run APROM", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_FWU_RUN_APROM), L"run aprom")) return -1;
    if (!fail(reset_target_btn_.Create(L"Reset Target", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_FWU_RESET_TARGET), L"reset target")) return -1;
    if (!fail(stop_btn_.Create(L"Stop", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_FWU_STOP), L"stop")) return -1;
    if (!fail(target_state_label_.Create(L"Target: not connected", WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE, CRect(), this, IDC_FWU_TARGET_STATE), L"target state")) return -1;
    if (!fail(progress_.Create(WS_CHILD | WS_VISIBLE | PBS_SMOOTH, CRect(), this, IDC_FWU_PROGRESS), L"progress")) return -1;
    if (!fail(progress_label_.Create(L"Progress: 0%", WS_CHILD | WS_VISIBLE, CRect(), this, IDC_FWU_PROGRESS_LABEL), L"progress label")) return -1;

    if (!fail(status_group_.Create(L"Response", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, CRect(), this, IDC_FWU_STATUS_GROUP), L"status group")) return -1;
    if (!fail(status_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_READONLY, CRect(), this, IDC_FWU_STATUS_EDIT), L"status edit")) return -1;

    std::vector<CWnd*> controls = {
        &hid_group_, &hid_check_, &vid_label_, &vid_edit_, &pid_label_, &pid_edit_, &timeout_label_, &timeout_edit_,
        &refresh_btn_, &connect_btn_, &disconnect_btn_, &device_label_, &device_combo_,
        &vcom_group_, &vcom_check_, &vcom_label_, &vcom_combo_, &vcom_baud_label_, &vcom_baud_combo_,
        &refresh_vcom_btn_, &connect_vcom_btn_, &disconnect_vcom_btn_,
        &bridge_status_title_, &bridge_status_chip_, &get_info_btn_, &ping_btn_, &reset_mcu_btn_,
        &fw_info_title_, &fw_info_value_,
        &config_group_, &port_label_, &port_combo_, &pins_label_, &pins_combo_, &speed_label_, &speed_edit_,
        &addr_label_, &addr_edit_, &delay_label_, &delay_edit_, &enable_host_btn_, &disable_host_btn_,
        &action_group_, &connect_target_btn_, &get_device_btn_, &program_btn_, &run_aprom_btn_, &reset_target_btn_,
        &stop_btn_, &target_state_label_, &progress_, &progress_label_, &status_group_, &status_edit_
    };
    for (CWnd* w : controls) {
        w->SetFont(&ui_font_);
    }

    PopulatePortCombo();
    vcom_baud_combo_.AddString(L"115200");
    vcom_baud_combo_.AddString(L"460800");
    vcom_baud_combo_.AddString(L"921600");
    vcom_baud_combo_.SetCurSel(0);
    LoadVisibleFromState();
    LoadBridgeVisibleFromState();
    progress_.SetRange32(0, 100);
    SetConnected(false);
    return 0;
}

void CFwUploadTab::OnSize(UINT nType, int cx, int cy) {
    CWnd::OnSize(nType, cx, cy);
    if (!::IsWindow(config_group_.GetSafeHwnd())) {
        return;
    }
    CRect r(0, 0, cx, cy);
    LayoutControls(r);
}

void CFwUploadTab::LayoutControls(const CRect& r) {
    const int margin = 8;
    const int gap = 6;
    const int row = 26;
    const int label_y_pad = 4;
    const int group_top_pad = 22;
    const int left = static_cast<int>(r.left);
    const int right = static_cast<int>(r.right);
    const int top = static_cast<int>(r.top);
    const int bottom = static_cast<int>(r.bottom);
    const int inner_left = r.left + margin + 12;
    const int available_w = (std::max)(240, r.Width() - margin * 2);
    const int bridge_h = 120;
    const int config_h = 104;
    const int action_h = 144;
    int btn_w = 86;
    int x;
    int y;

    btn_w = (std::max)(btn_w, mfc_tool::ui::MeasureButtonMinWidth(refresh_btn_));
    btn_w = (std::max)(btn_w, mfc_tool::ui::MeasureButtonMinWidth(connect_btn_));

    y = top + margin;
    {
        const int transport_w = available_w;
        const int hid_w = (transport_w - gap) / 2;
        const int vcom_w = transport_w - hid_w - gap;
        const int hid_x = left + margin;
        const int vcom_x = hid_x + hid_w + gap;
        const int hid_inner = hid_x + 12;
        const int hid_right = hid_x + hid_w - 12;
        const int vcom_inner = vcom_x + 12;
        const int vcom_right = vcom_x + vcom_w - 12;
        const int connect_w = (std::max)(btn_w, mfc_tool::ui::MeasureButtonMinWidth(connect_btn_));
        const int disconnect_w = (std::max)(btn_w, mfc_tool::ui::MeasureButtonMinWidth(disconnect_btn_));
        const int refresh_vcom_w = (std::max)(btn_w, mfc_tool::ui::MeasureButtonMinWidth(refresh_vcom_btn_));
        const int connect_vcom_w = (std::max)(btn_w, mfc_tool::ui::MeasureButtonMinWidth(connect_vcom_btn_));
        const int disconnect_vcom_w = (std::max)(btn_w, mfc_tool::ui::MeasureButtonMinWidth(disconnect_vcom_btn_));

        mfc_tool::ui::SafeMoveWindow(hid_group_, hid_x, y, hid_w, bridge_h);
        {
            const int yy1 = y + group_top_pad;
            const int yy2 = yy1 + row + 6;
            const int yy3 = yy2 + row + 6;
            x = hid_inner;
            mfc_tool::ui::SafeMoveWindow(hid_check_, x, yy1 + 2, 56, 20);
            x += 62;
            x = mfc_tool::ui::PlaceLabelAndControl(vid_label_, vid_edit_, x, yy1 + label_y_pad, yy1, 72, row, gap, 8) + gap;
            x = mfc_tool::ui::PlaceLabelAndControl(pid_label_, pid_edit_, x, yy1 + label_y_pad, yy1, 72, row, gap, 8) + gap;
            mfc_tool::ui::SafeMoveWindow(refresh_btn_, (std::max)(x, hid_right - btn_w), yy1, btn_w, row);

            x = hid_inner;
            x = mfc_tool::ui::PlaceLabelAndControl(timeout_label_, timeout_edit_, x, yy2 + label_y_pad, yy2, 68, row, gap, 8) + gap;
            x += mfc_tool::ui::PlaceLabel(device_label_, x, yy2 + label_y_pad, 8) + gap;
            mfc_tool::ui::SafeMoveWindow(device_combo_, x, yy2, (std::max)(120, hid_right - x), row + 120);

            x = hid_inner;
            mfc_tool::ui::SafeMoveWindow(connect_btn_, x, yy3, connect_w, row);
            x += connect_w + gap;
            mfc_tool::ui::SafeMoveWindow(disconnect_btn_, x, yy3, disconnect_w, row);
        }

        mfc_tool::ui::SafeMoveWindow(vcom_group_, vcom_x, y, vcom_w, bridge_h);
        {
            const int yy1 = y + group_top_pad;
            const int yy2 = yy1 + row + 6;
            x = vcom_inner;
            mfc_tool::ui::SafeMoveWindow(vcom_check_, x, yy1 + 2, 68, 20);
            x += 74;
            x = mfc_tool::ui::PlaceLabelAndControl(vcom_label_, vcom_combo_, x, yy1 + label_y_pad, yy1, 124, row + 120, gap, 8) + gap;
            x += mfc_tool::ui::PlaceLabel(vcom_baud_label_, x, yy1 + label_y_pad, 8) + gap;
            mfc_tool::ui::SafeMoveWindow(vcom_baud_combo_, x, yy1, (std::max)(90, vcom_right - x), row + 120);

            x = vcom_inner;
            mfc_tool::ui::SafeMoveWindow(refresh_vcom_btn_, x, yy2, refresh_vcom_w, row);
            x += refresh_vcom_w + gap;
            mfc_tool::ui::SafeMoveWindow(connect_vcom_btn_, x, yy2, connect_vcom_w, row);
            x += connect_vcom_w + gap;
            mfc_tool::ui::SafeMoveWindow(disconnect_vcom_btn_, x, yy2, disconnect_vcom_w, row);
        }
    }

    y += bridge_h + gap;
    x = left + margin;
    x += mfc_tool::ui::PlaceLabel(bridge_status_title_, x, y + label_y_pad, 10) + gap;
    mfc_tool::ui::SafeMoveWindow(bridge_status_chip_, x, y + 1, 138, row - 2);
    x += 148;
    mfc_tool::ui::SafeMoveWindow(get_info_btn_, x, y, btn_w, row);
    x += btn_w + gap;
    mfc_tool::ui::SafeMoveWindow(ping_btn_, x, y, btn_w, row);
    x += btn_w + gap;
    mfc_tool::ui::SafeMoveWindow(reset_mcu_btn_, x, y, btn_w + 8, row);
    x += btn_w + 18;
    x += mfc_tool::ui::PlaceLabel(fw_info_title_, x, y + label_y_pad, 10) + gap;
    mfc_tool::ui::SafeMoveWindow(fw_info_value_, x, y + label_y_pad, (std::max)(120, right - margin - x), 18);

    y += row + gap;
    mfc_tool::ui::SafeMoveWindow(config_group_, left + margin, y, available_w, config_h);
    x = inner_left;
    {
        const int yy1 = y + group_top_pad;
        const int yy2 = yy1 + row + 8;
        const int pins_label_w = mfc_tool::ui::MeasureControlTextWidth(pins_label_, 8);

        x = mfc_tool::ui::PlaceLabelAndControl(port_label_, port_combo_, x, yy1 + label_y_pad, yy1, 76, row + 110, gap, 8) + gap;
        const int pins_w = (std::max)(160, right - margin - x - pins_label_w - gap - 12);
        mfc_tool::ui::PlaceLabelAndControl(pins_label_, pins_combo_, x, yy1 + label_y_pad, yy1, pins_w, row + 160, gap, 8);

        x = inner_left;
        x = mfc_tool::ui::PlaceLabelAndControl(speed_label_, speed_edit_, x, yy2 + label_y_pad, yy2, 84, row, gap, 8) + gap;
        x = mfc_tool::ui::PlaceLabelAndControl(addr_label_, addr_edit_, x, yy2 + label_y_pad, yy2, 70, row, gap, 8) + gap;
        x = mfc_tool::ui::PlaceLabelAndControl(delay_label_, delay_edit_, x, yy2 + label_y_pad, yy2, 58, row, gap, 8) + gap;
        mfc_tool::ui::SafeMoveWindow(enable_host_btn_, x, yy2, 104, row);
        x += 110;
        mfc_tool::ui::SafeMoveWindow(disable_host_btn_, x, yy2, 104, row);
    }

    y += config_h + gap;
    mfc_tool::ui::SafeMoveWindow(action_group_, left + margin, y, available_w, action_h);
    x = inner_left;
    {
        const int yy = y + group_top_pad;
        const int btn_h = row;
        const int connect_w = (std::max)(112, mfc_tool::ui::MeasureButtonMinWidth(connect_target_btn_));
        const int device_w = (std::max)(106, mfc_tool::ui::MeasureButtonMinWidth(get_device_btn_));
        const int program_w = (std::max)(118, mfc_tool::ui::MeasureButtonMinWidth(program_btn_));
        const int run_w = (std::max)(90, mfc_tool::ui::MeasureButtonMinWidth(run_aprom_btn_));
        const int reset_w = (std::max)(102, mfc_tool::ui::MeasureButtonMinWidth(reset_target_btn_));
        const int stop_w = (std::max)(68, mfc_tool::ui::MeasureButtonMinWidth(stop_btn_));

        mfc_tool::ui::SafeMoveWindow(connect_target_btn_, x, yy, connect_w, btn_h);
        x += connect_w + gap;
        mfc_tool::ui::SafeMoveWindow(get_device_btn_, x, yy, device_w, btn_h);
        x += device_w + gap;
        mfc_tool::ui::SafeMoveWindow(program_btn_, x, yy, program_w, btn_h);
        x += program_w + gap;
        mfc_tool::ui::SafeMoveWindow(run_aprom_btn_, x, yy, run_w, btn_h);
        x += run_w + gap;
        mfc_tool::ui::SafeMoveWindow(reset_target_btn_, x, yy, reset_w, btn_h);
        x += reset_w + gap;
        mfc_tool::ui::SafeMoveWindow(stop_btn_, x, yy, stop_w, btn_h);

        mfc_tool::ui::SafeMoveWindow(target_state_label_, inner_left, yy + row + 8, available_w - 24, 24);
        mfc_tool::ui::SafeMoveWindow(progress_, inner_left, yy + row + 38, available_w - 24, 18);
        mfc_tool::ui::SafeMoveWindow(progress_label_, inner_left, yy + row + 60, available_w - 24, 18);
    }

    y += action_h + gap;
    mfc_tool::ui::SafeMoveWindow(status_group_, left + margin, y, available_w, (std::max)(80, bottom - y - margin));
    mfc_tool::ui::SafeMoveWindow(status_edit_, inner_left, y + group_top_pad, available_w - 24,
                                 (std::max)(46, bottom - y - margin - group_top_pad - 8));
}

void CFwUploadTab::SetConnected(bool connected) {
    connected_ = connected;
    if (!connected_) {
        OnDisconnected();
    }
    SetBridgeConnectionUi();
    UpdateEnableState();
}

void CFwUploadTab::OnDisconnected() {
    connected_ = false;
    host_enabled_ = false;
    busy_ = false;
    cancel_requested_ = true;
    SetTargetReady(false);
    RefreshPinUsage();
    SetBridgeConnectionUi();
    UpdateEnableState();
}

void CFwUploadTab::RefreshSharedImageState() {
    UpdateEnableState();
}

void CFwUploadTab::LoadState(const mfc_tool::core::AppState& state) {
    ui_state_ = state.ui;
    state_ = state.fw_upload;
    LoadVisibleFromState();
    LoadBridgeVisibleFromState();
    RefreshDevices();
    RefreshVcomPorts();
}

void CFwUploadTab::SaveState(mfc_tool::core::AppState* state) const {
    if (state == nullptr) {
        return;
    }
    const_cast<CFwUploadTab*>(this)->SaveVisibleToState();
    const_cast<CFwUploadTab*>(this)->SaveBridgeVisibleToState();
    state->fw_upload = state_;
    state->ui.vid = ui_state_.vid;
    state->ui.pid = ui_state_.pid;
    state->ui.timeout_ms = ui_state_.timeout_ms;
    state->ui.device_label = ui_state_.device_label;
    state->ui.bridge_interface = ui_state_.bridge_interface;
    state->ui.vcom_port = ui_state_.vcom_port;
    state->ui.vcom_baud = ui_state_.vcom_baud;
    state->ui.expected_fw_version = ui_state_.expected_fw_version;
    state->ui.last_seen_fw_version = ui_state_.last_seen_fw_version;
}

void CFwUploadTab::RefreshDevices() {
    if (!::IsWindow(device_combo_.GetSafeHwnd())) {
        return;
    }
    if (service_ == nullptr) {
        device_combo_.ResetContent();
        device_combo_.AddString(L"Auto Select");
        device_combo_.SetCurSel(0);
        return;
    }

    try {
        int vid = mfc_tool::core::ParseInt(GetEditText(vid_edit_));
        int pid = mfc_tool::core::ParseInt(GetEditText(pid_edit_));
        scanned_devices_ = service_->ScanDevices(static_cast<std::uint16_t>(vid), static_cast<std::uint16_t>(pid));
        std::sort(scanned_devices_.begin(), scanned_devices_.end(), [](const auto& a, const auto& b) {
            return DeviceLabel(a, 0) < DeviceLabel(b, 0);
        });

        device_combo_.ResetContent();
        device_combo_.AddString(L"Auto Select");
        for (size_t i = 0; i < scanned_devices_.size(); ++i) {
            device_combo_.AddString(DeviceLabel(scanned_devices_[i], static_cast<int>(i)).c_str());
        }
        if (!ui_state_.device_label.empty()) {
            ApplyComboSelection(device_combo_, ui_state_.device_label);
        }
        if (device_combo_.GetCurSel() == CB_ERR) {
            device_combo_.SetCurSel(scanned_devices_.empty() ? 0 : 1);
        }
        if (log_) {
            log_(L"HID bridge scan complete: " + std::to_wstring(scanned_devices_.size()) + L" found");
        }
    } catch (const std::exception& e) {
        if (log_) {
            log_(L"HID bridge refresh failed: " + AnsiToWide(e.what()));
        }
        ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"Refresh HID", MB_ICONERROR | MB_OK);
    }
}

void CFwUploadTab::RefreshVcomPorts() {
    if (!::IsWindow(vcom_combo_.GetSafeHwnd())) {
        return;
    }

    const std::wstring selected_before = SelectedVcomPort();
    vcom_combo_.ResetContent();

    for (int i = 1; i <= 256; ++i) {
        wchar_t name[16] = {};
        swprintf_s(name, L"COM%d", i);

        wchar_t target[256] = {};
        DWORD len = QueryDosDeviceW(name, target, static_cast<DWORD>(sizeof(target) / sizeof(target[0])));
        if (len > 0u) {
            vcom_combo_.AddString(name);
        }
    }

    if (vcom_combo_.GetCount() <= 0) {
        if (log_) {
            log_(L"I2C bridge VCOM scan complete: no COM ports found.");
        }
        return;
    }

    std::wstring preferred = selected_before.empty() ? ui_state_.vcom_port : selected_before;
    int idx = -1;
    if (!preferred.empty()) {
        idx = vcom_combo_.FindStringExact(-1, preferred.c_str());
    }
    if (idx < 0) {
        idx = 0;
    }
    vcom_combo_.SetCurSel(idx);
    if (log_) {
        log_(L"I2C bridge VCOM scan complete: " + std::to_wstring(vcom_combo_.GetCount()) + L" COM ports found");
    }
}

void CFwUploadTab::SetBridgeConnectionUi() {
    if (!::IsWindow(connect_btn_.GetSafeHwnd())) {
        return;
    }

    const bool hid_connected = service_ != nullptr && service_->IsHidConnected();
    const bool vcom_connected = service_ != nullptr && service_->IsVcomConnected();
    const bool connected = service_ != nullptr && service_->IsConnected();
    const bool hid_selected = IsHidInterfaceSelected();
    const bool hid_setup_enabled = hid_selected && !connected;
    const bool vcom_setup_enabled = !hid_selected && !connected;

    connected_ = connected;

    mfc_tool::ui::SafeEnableWindow(hid_group_, hid_selected);
    mfc_tool::ui::SafeEnableWindow(vcom_group_, !hid_selected);
    mfc_tool::ui::SafeEnableWindow(hid_check_, !connected);
    mfc_tool::ui::SafeEnableWindow(vcom_check_, !connected);

    mfc_tool::ui::SafeEnableWindow(vid_label_, hid_setup_enabled);
    mfc_tool::ui::SafeEnableWindow(vid_edit_, hid_setup_enabled);
    mfc_tool::ui::SafeEnableWindow(pid_label_, hid_setup_enabled);
    mfc_tool::ui::SafeEnableWindow(pid_edit_, hid_setup_enabled);
    mfc_tool::ui::SafeEnableWindow(timeout_label_, hid_setup_enabled);
    mfc_tool::ui::SafeEnableWindow(timeout_edit_, hid_setup_enabled);
    mfc_tool::ui::SafeEnableWindow(refresh_btn_, hid_setup_enabled);
    mfc_tool::ui::SafeEnableWindow(connect_btn_, hid_selected && !connected);
    mfc_tool::ui::SafeEnableWindow(disconnect_btn_, hid_connected);
    mfc_tool::ui::SafeEnableWindow(device_label_, hid_setup_enabled);
    mfc_tool::ui::SafeEnableWindow(device_combo_, hid_setup_enabled);

    mfc_tool::ui::SafeEnableWindow(vcom_label_, vcom_setup_enabled);
    mfc_tool::ui::SafeEnableWindow(vcom_combo_, vcom_setup_enabled);
    mfc_tool::ui::SafeEnableWindow(vcom_baud_label_, vcom_setup_enabled);
    mfc_tool::ui::SafeEnableWindow(vcom_baud_combo_, vcom_setup_enabled);
    mfc_tool::ui::SafeEnableWindow(refresh_vcom_btn_, vcom_setup_enabled);
    mfc_tool::ui::SafeEnableWindow(connect_vcom_btn_, !hid_selected && !connected);
    mfc_tool::ui::SafeEnableWindow(disconnect_vcom_btn_, vcom_connected);

    mfc_tool::ui::SafeEnableWindow(reset_mcu_btn_, connected);
    mfc_tool::ui::SafeEnableWindow(get_info_btn_, connected);
    mfc_tool::ui::SafeEnableWindow(ping_btn_, connected);

    const std::wstring status = connected && service_ != nullptr
        ? (service_->ActiveTransportLabel() + L" Connected")
        : L"Disconnected";
    bridge_status_chip_.SetWindowTextW(status.c_str());
    bridge_status_color_ = connected ? RGB(46, 139, 87) : RGB(95, 104, 117);
    UpdateBridgeStatusChipColor();
    UpdateFirmwareInfoUi();
}

void CFwUploadTab::SetBridgeInterfaceSelection(bool hid_selected) {
    if (::IsWindow(hid_check_.GetSafeHwnd())) {
        hid_check_.SetCheck(hid_selected ? BST_CHECKED : BST_UNCHECKED);
    }
    if (::IsWindow(vcom_check_.GetSafeHwnd())) {
        vcom_check_.SetCheck(hid_selected ? BST_UNCHECKED : BST_CHECKED);
    }
    ui_state_.bridge_interface = hid_selected ? L"HID" : L"VCOM";
}

bool CFwUploadTab::IsHidInterfaceSelected() const {
    if (::IsWindow(hid_check_.GetSafeHwnd())) {
        return const_cast<CButton&>(hid_check_).GetCheck() == BST_CHECKED;
    }
    return ui_state_.bridge_interface != L"VCOM";
}

void CFwUploadTab::UpdateBridgeStatusChipColor() {
    if (bridge_status_brush_.GetSafeHandle()) {
        bridge_status_brush_.DeleteObject();
    }
    bridge_status_brush_.CreateSolidBrush(bridge_status_color_);
    bridge_status_chip_.Invalidate();
}

void CFwUploadTab::UpdateFirmwareInfoUi() {
    std::wstring text = L"expected=" + expected_fw_version_ + L" actual=" + current_fw_version_;
    if (!fw_version_match_) {
        text += L"  MISMATCH";
    }
    if (::IsWindow(fw_info_value_.GetSafeHwnd())) {
        fw_info_value_.SetWindowTextW(text.c_str());
    }
}

void CFwUploadTab::SaveBridgeVisibleToState() {
    if (loading_ || !::IsWindow(vid_edit_.GetSafeHwnd())) {
        return;
    }
    ui_state_.vid = GetEditText(vid_edit_);
    ui_state_.pid = GetEditText(pid_edit_);
    ui_state_.timeout_ms = GetEditText(timeout_edit_);
    ui_state_.vcom_port = SelectedVcomPort();
    ui_state_.vcom_baud = std::to_wstring(SelectedVcomBaudrate());
    ui_state_.bridge_interface = IsHidInterfaceSelected() ? L"HID" : L"VCOM";
    ui_state_.expected_fw_version = expected_fw_version_;
    ui_state_.last_seen_fw_version = current_fw_version_;

    CString combo_text;
    device_combo_.GetWindowTextW(combo_text);
    ui_state_.device_label = combo_text.GetString();
}

void CFwUploadTab::LoadBridgeVisibleFromState() {
    if (!::IsWindow(vid_edit_.GetSafeHwnd())) {
        return;
    }

    loading_ = true;
    vid_edit_.SetWindowTextW(ui_state_.vid.c_str());
    pid_edit_.SetWindowTextW(ui_state_.pid.c_str());
    timeout_edit_.SetWindowTextW(ui_state_.timeout_ms.c_str());
    SetBridgeInterfaceSelection(ui_state_.bridge_interface != L"VCOM");
    ApplyComboSelection(vcom_baud_combo_, ui_state_.vcom_baud);
    expected_fw_version_ = ui_state_.expected_fw_version.empty() ? M032_EXPECTED_FW_VERSION : ui_state_.expected_fw_version;
    current_fw_version_ = ui_state_.last_seen_fw_version.empty() ? L"-" : ui_state_.last_seen_fw_version;
    fw_version_match_ = FirmwareVersionMatch(expected_fw_version_, current_fw_version_) || current_fw_version_ == L"-";
    loading_ = false;

    SetBridgeConnectionUi();
}

std::wstring CFwUploadTab::SelectedDevicePath() const {
    int sel = device_combo_.GetCurSel();
    if (sel == CB_ERR || sel <= 0) {
        if (!scanned_devices_.empty()) {
            return scanned_devices_.front().path;
        }
        return L"";
    }
    const size_t idx = static_cast<size_t>(sel - 1);
    return idx < scanned_devices_.size() ? scanned_devices_[idx].path : L"";
}

std::wstring CFwUploadTab::SelectedVcomPort() const {
    CString text;
    const_cast<CComboBox&>(vcom_combo_).GetWindowTextW(text);
    return text.GetString();
}

std::uint32_t CFwUploadTab::SelectedVcomBaudrate() const {
    CString text;
    const_cast<CComboBox&>(vcom_baud_combo_).GetWindowTextW(text);
    const std::wstring value = text.GetString();
    if (value == L"460800") {
        return 460800u;
    }
    if (value == L"921600") {
        return 921600u;
    }
    return 115200u;
}

void CFwUploadTab::ApplyComboSelection(CComboBox& combo, const std::wstring& value) {
    if (!::IsWindow(combo.GetSafeHwnd()) || value.empty()) {
        return;
    }
    const int idx = combo.FindStringExact(-1, value.c_str());
    if (idx >= 0) {
        combo.SetCurSel(idx);
    }
}

std::wstring CFwUploadTab::DeviceLabel(const mfc_tool::hid::DeviceInfo& d, int index) {
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

void CFwUploadTab::LogBridgeInfoPayload(const std::vector<std::uint8_t>& rx, bool from_connect) {
    std::wstring bridge_name;
    std::uint8_t reset_reason = 0;

    if (!rx.empty()) {
        bridge_name.assign(rx.begin(), rx.end());
        const auto end_pos = std::find(bridge_name.begin(), bridge_name.end(), L'\0');
        bridge_name.erase(end_pos, bridge_name.end());
        if (rx.size() >= 2u) {
            reset_reason = rx.back();
        }
    }

    current_fw_version_ = ExtractFirmwareVersion(bridge_name);
    if (current_fw_version_.empty()) {
        current_fw_version_ = L"-";
    }
    fw_version_match_ = FirmwareVersionMatch(expected_fw_version_, current_fw_version_) || current_fw_version_ == L"-";
    UpdateFirmwareInfoUi();
    if (log_) {
        log_(std::wstring(from_connect ? L"Connected I2C bridge: " : L"I2C bridge info: ") + bridge_name);
        log_(L"I2C bridge reset reason: " + ResetReasonText(reset_reason));
    }
    if (!fw_version_match_) {
        const std::wstring warn =
            L"Firmware version mismatch: expected " + expected_fw_version_ +
            L", device " + current_fw_version_ + L". Please update M032 EVB programming bridge firmware.";
        if (log_) {
            log_(warn);
        }
        if (from_connect) {
            ::MessageBoxW(m_hWnd, warn.c_str(), L"Firmware Version Mismatch", MB_ICONWARNING | MB_OK);
        }
    } else if (from_connect && log_) {
        log_(L"I2C bridge firmware version check OK: " + current_fw_version_);
    }
}

std::wstring CFwUploadTab::ResetReasonText(std::uint8_t reason) {
    switch (reason) {
    case 0: return L"NONE";
    case 1: return L"WATCHDOG";
    case 2: return L"CMD_RESET";
    default: return L"UNKNOWN";
    }
}

std::wstring CFwUploadTab::ExtractFirmwareVersion(const std::wstring& bridge_name) {
    const auto pos = bridge_name.find(L'/');
    if (pos == std::wstring::npos || pos + 1 >= bridge_name.size()) {
        return L"";
    }
    return bridge_name.substr(pos + 1);
}

bool CFwUploadTab::FirmwareVersionMatch(const std::wstring& expected, const std::wstring& actual) {
    return !expected.empty() && expected == actual;
}

void CFwUploadTab::PopulatePortCombo() {
    port_combo_.ResetContent();
    int idx = port_combo_.AddString(L"I2C0");
    port_combo_.SetItemData(idx, 0);
    idx = port_combo_.AddString(L"I2C1");
    port_combo_.SetItemData(idx, 1);
    port_combo_.SetCurSel(mfc_tool::core::ParseInt(state_.master_i2c_port) == 1 ? 1 : 0);
}

void CFwUploadTab::PopulatePinCombo(const std::wstring& preferred_ini_name) {
    const int port = CurrentPort();
    const auto& pairs = mfc_tool::core::board_i2c::AllPinPairs();
    const std::wstring preferred = preferred_ini_name.empty() ? state_.master_i2c_pins : preferred_ini_name;
    int selected = 0;
    int row = 0;

    pins_combo_.ResetContent();
    for (size_t i = 0; i < pairs.size(); ++i) {
        if (pairs[i].i2c_port != port) {
            continue;
        }
        const int idx = pins_combo_.AddString(pairs[i].label);
        pins_combo_.SetItemData(idx, static_cast<DWORD_PTR>(i));
        if (preferred == pairs[i].ini_name) {
            selected = row;
        }
        ++row;
    }
    if (pins_combo_.GetCount() > 0) {
        pins_combo_.SetCurSel(selected);
    }
}

void CFwUploadTab::UpdateEnableState() {
    if (!::IsWindow(enable_host_btn_.GetSafeHwnd())) {
        return;
    }

    SaveVisibleToState();
    const bool ready = connected_ && service_ != nullptr;
    const bool shared_active = pin_usage_ != nullptr &&
        pin_usage_->AnyActiveExcept(SharedOwnerIds(), {OwnerId()});
    const bool image_loaded = firmware_image_data_ && !firmware_image_data_().empty();

    mfc_tool::ui::SafeEnableWindow(port_combo_, ready && !host_enabled_ && !busy_);
    mfc_tool::ui::SafeEnableWindow(pins_combo_, ready && !host_enabled_ && !busy_);
    mfc_tool::ui::SafeEnableWindow(speed_edit_, ready && !host_enabled_ && !busy_);
    mfc_tool::ui::SafeEnableWindow(addr_edit_, ready && !busy_);
    mfc_tool::ui::SafeEnableWindow(delay_edit_, ready && !busy_);
    mfc_tool::ui::SafeEnableWindow(enable_host_btn_, ready && !host_enabled_ && !busy_ && !shared_active);
    mfc_tool::ui::SafeEnableWindow(disable_host_btn_, ready && host_enabled_ && !busy_);

    mfc_tool::ui::SafeEnableWindow(connect_target_btn_, ready && !busy_);
    mfc_tool::ui::SafeEnableWindow(get_device_btn_, ready && !busy_);
    mfc_tool::ui::SafeEnableWindow(program_btn_, ready && !busy_ && image_loaded);
    mfc_tool::ui::SafeEnableWindow(run_aprom_btn_, ready && !busy_);
    mfc_tool::ui::SafeEnableWindow(reset_target_btn_, ready && !busy_);
    mfc_tool::ui::SafeEnableWindow(stop_btn_, busy_);
}

void CFwUploadTab::RefreshPinUsage() {
    if (pin_usage_ == nullptr || !::IsWindow(port_combo_.GetSafeHwnd())) {
        return;
    }
    const auto& pins = CurrentPinPair();
    pin_usage_->SetLabel(OwnerId(), L"FW upload host");
    pin_usage_->SetClaim(OwnerId(), {pins.sda_pin, pins.scl_pin});
    pin_usage_->SetActive(OwnerId(), host_enabled_);
}

void CFwUploadTab::SaveVisibleToState() {
    if (loading_ || !::IsWindow(port_combo_.GetSafeHwnd())) {
        return;
    }
    state_.master_i2c_port = std::to_wstring(CurrentPort());
    state_.master_i2c_pins = CurrentPinPair().ini_name;
    state_.speed_hz = GetEditText(speed_edit_);
    state_.target_addr = GetEditText(addr_edit_);
    state_.response_delay_ms = GetEditText(delay_edit_);
}

void CFwUploadTab::LoadVisibleFromState() {
    if (!::IsWindow(port_combo_.GetSafeHwnd())) {
        return;
    }

    loading_ = true;
    port_combo_.SetCurSel(mfc_tool::core::ParseInt(state_.master_i2c_port) == 1 ? 1 : 0);
    PopulatePinCombo(state_.master_i2c_pins);
    speed_edit_.SetWindowTextW(state_.speed_hz.c_str());
    addr_edit_.SetWindowTextW(state_.target_addr.c_str());
    delay_edit_.SetWindowTextW(state_.response_delay_ms.c_str());
    loading_ = false;

    RefreshPinUsage();
    UpdateEnableState();
}

void CFwUploadTab::EnsureHostEnabled() {
    if (service_ == nullptr || !connected_) {
        throw std::runtime_error("I2C bridge is not connected.");
    }
    SaveVisibleToState();
    if (host_enabled_) {
        return;
    }
    const auto& pins = CurrentPinPair();
    if (pin_usage_ != nullptr &&
        pin_usage_->AnyActiveExcept(SharedOwnerIds(), {OwnerId()})) {
        throw std::runtime_error("Another I2C host function is already active. Disable it first.");
    }
    if (pin_usage_ != nullptr &&
        pin_usage_->AnyPinOccupied({pins.sda_pin, pins.scl_pin}, {OwnerId()})) {
        throw std::runtime_error("The selected I2C pins are already active in another function.");
    }
    service_->I2cMasterInit(CurrentPort(), pins.sda_pin, pins.scl_pin, CurrentSpeedHz());
    SetHostEnabled(true);
    AppendStatusText(mfc_tool::core::board_i2c::PortLabel(CurrentPort()) + L" host enabled on " + pins.label);
    if (log_) {
        log_(mfc_tool::core::board_i2c::PortLabel(CurrentPort()) + L" FW upload host enabled on " + pins.label);
    }
}

void CFwUploadTab::SetHostEnabled(bool enabled) {
    host_enabled_ = enabled;
    RefreshPinUsage();
    UpdateEnableState();
}

void CFwUploadTab::LoadImageFromPath(const std::wstring& path) {
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
        log_(L"FW image loaded: " + path + L" (" + std::to_wstring(image_data_.size()) + L" bytes)");
    }
}

void CFwUploadTab::SetImagePathText() {
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

void CFwUploadTab::SetStatusText(const std::wstring& text) {
    if (::IsWindow(status_edit_.GetSafeHwnd())) {
        status_edit_.SetWindowTextW(text.c_str());
    }
}

void CFwUploadTab::AppendStatusText(const std::wstring& text) {
    if (!::IsWindow(status_edit_.GetSafeHwnd())) {
        return;
    }
    int len = status_edit_.GetWindowTextLengthW();
    status_edit_.SetSel(len, len);
    status_edit_.ReplaceSel((text + L"\r\n").c_str());
}

void CFwUploadTab::SetProgress(int value, const std::wstring& text) {
    if (::IsWindow(progress_.GetSafeHwnd())) {
        progress_.SetPos(std::clamp(value, 0, 100));
    }
    if (::IsWindow(progress_label_.GetSafeHwnd())) {
        progress_label_.SetWindowTextW(text.c_str());
    }
}

void CFwUploadTab::SetTargetReady(bool ready, const std::wstring& text) {
    target_ready_ = ready;
    if (::IsWindow(target_state_label_.GetSafeHwnd())) {
        const std::wstring label = text.empty()
            ? (ready ? L"Target Ready - load BIN and click Program APROM" : L"Target: not connected")
            : text;
        target_state_label_.SetWindowTextW(label.c_str());
        target_state_label_.Invalidate(FALSE);
    }
}

HBRUSH CFwUploadTab::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor) {
    HBRUSH brush = CWnd::OnCtlColor(pDC, pWnd, nCtlColor);
    if (pWnd != nullptr && pWnd->GetSafeHwnd() == bridge_status_chip_.GetSafeHwnd()) {
        pDC->SetBkColor(bridge_status_color_);
        pDC->SetTextColor(bridge_status_text_color_);
        return static_cast<HBRUSH>(bridge_status_brush_.GetSafeHandle());
    }
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

void CFwUploadTab::PumpUiMessages() {
    MSG msg;
    while (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE) != FALSE) {
        if (!AfxGetApp()->PreTranslateMessage(&msg)) {
            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);
        }
    }
}

void CFwUploadTab::OnBridgeRefreshHid() {
    RefreshDevices();
}

void CFwUploadTab::OnBridgeConnectHid() {
    try {
        if (service_ == nullptr) {
            throw std::runtime_error("I2C bridge service is not available.");
        }
        SaveBridgeVisibleToState();
        int vid = mfc_tool::core::ParseInt(GetEditText(vid_edit_));
        int pid = mfc_tool::core::ParseInt(GetEditText(pid_edit_));
        int timeout = mfc_tool::core::ParseInt(GetEditText(timeout_edit_));
        std::wstring path = SelectedDevicePath();

        service_->Connect(static_cast<std::uint16_t>(vid), static_cast<std::uint16_t>(pid), path, timeout);
        SetConnected(service_->IsConnected());
        if (log_) {
            log_(L"I2C bridge HID connected.");
        }
        try {
            auto rx = service_->GetInfo();
            LogBridgeInfoPayload(rx, true);
        } catch (const std::exception& info_e) {
            if (log_) {
                log_(L"Auto Get Info failed: " + AnsiToWide(info_e.what()));
            }
        }
        if (persist_settings_) {
            persist_settings_();
        }
    } catch (const std::exception& e) {
        ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"Connect HID", MB_ICONERROR | MB_OK);
        if (log_) {
            log_(L"I2C bridge HID connect failed.");
        }
        if (service_ != nullptr) {
            service_->Disconnect();
        }
        SetConnected(false);
    }
}

void CFwUploadTab::OnBridgeDisconnectHid() {
    if (service_ != nullptr) {
        service_->Disconnect();
    }
    OnDisconnected();
    if (log_) {
        log_(L"I2C bridge HID disconnected.");
    }
}

void CFwUploadTab::OnBridgeGetInfo() {
    try {
        if (service_ == nullptr) {
            throw std::runtime_error("I2C bridge service is not available.");
        }
        auto rx = service_->GetInfo();
        LogBridgeInfoPayload(rx, false);
    } catch (const std::exception& e) {
        ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"Get Info", MB_ICONERROR | MB_OK);
    }
}

void CFwUploadTab::OnBridgePing() {
    try {
        if (service_ == nullptr) {
            throw std::runtime_error("I2C bridge service is not available.");
        }
        std::vector<std::uint8_t> tx = {0xA1, 0xB2, 0xC3, 0xD4};
        auto rx = service_->Ping(tx);
        if (log_) {
            log_(L"I2C bridge PING RX: " + mfc_tool::core::HexDump(rx));
        }
    } catch (const std::exception& e) {
        ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"Ping", MB_ICONERROR | MB_OK);
    }
}

void CFwUploadTab::OnBridgeResetMcu() {
    try {
        if (service_ == nullptr) {
            throw std::runtime_error("I2C bridge service is not available.");
        }
        service_->ResetMcu();
        if (log_) {
            log_(L"I2C bridge MCU reset command sent.");
        }
        service_->Disconnect();
        OnDisconnected();
        RefreshDevices();
        RefreshVcomPorts();
    } catch (const std::exception& e) {
        ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"Reset MCU", MB_ICONERROR | MB_OK);
    }
}

void CFwUploadTab::OnBridgeRefreshVcom() {
    RefreshVcomPorts();
}

void CFwUploadTab::OnBridgeConnectVcom() {
    try {
        if (service_ == nullptr) {
            throw std::runtime_error("I2C bridge service is not available.");
        }
        SaveBridgeVisibleToState();
        const std::wstring com = SelectedVcomPort();
        const std::uint32_t baud = SelectedVcomBaudrate();
        const int timeout = mfc_tool::core::ParseInt(GetEditText(timeout_edit_));

        service_->ConnectVcom(com, baud, timeout);
        if (log_) {
            log_(L"I2C bridge VCOM opened: " + com + L", baud=" + std::to_wstring(baud));
        }
        {
            auto rx = service_->GetInfo();
            LogBridgeInfoPayload(rx, true);
        }
        SetConnected(service_->IsConnected());
        if (persist_settings_) {
            persist_settings_();
        }
    } catch (const std::exception& e) {
        ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"Open VCOM", MB_ICONERROR | MB_OK);
        if (log_) {
            log_(L"I2C bridge VCOM open failed.");
        }
        if (service_ != nullptr) {
            service_->Disconnect();
        }
        SetConnected(false);
    }
}

void CFwUploadTab::OnBridgeDisconnectVcom() {
    if (service_ != nullptr) {
        service_->Disconnect();
    }
    OnDisconnected();
    if (log_) {
        log_(L"I2C bridge VCOM closed.");
    }
}

void CFwUploadTab::OnBridgeSelectHid() {
    if (service_ != nullptr && service_->IsConnected()) {
        SetBridgeInterfaceSelection(service_->ActiveTransport() != mfc_tool::core::BridgeTransport::Vcom);
        SetBridgeConnectionUi();
        return;
    }
    SetBridgeInterfaceSelection(true);
    SetBridgeConnectionUi();
    if (persist_settings_) {
        persist_settings_();
    }
}

void CFwUploadTab::OnBridgeSelectVcom() {
    if (service_ != nullptr && service_->IsConnected()) {
        SetBridgeInterfaceSelection(service_->ActiveTransport() != mfc_tool::core::BridgeTransport::Vcom);
        SetBridgeConnectionUi();
        return;
    }
    SetBridgeInterfaceSelection(false);
    SetBridgeConnectionUi();
    if (persist_settings_) {
        persist_settings_();
    }
}

void CFwUploadTab::OnBridgeVcomChanged() {
    SaveBridgeVisibleToState();
    if (persist_settings_) {
        persist_settings_();
    }
}

void CFwUploadTab::OnBridgeVcomBaudChanged() {
    SaveBridgeVisibleToState();
    if (persist_settings_) {
        persist_settings_();
    }
}

void CFwUploadTab::OnPortChanged() {
    if (loading_) {
        return;
    }
    SetTargetReady(false);
    SaveVisibleToState();
    PopulatePinCombo();
    RefreshPinUsage();
    UpdateEnableState();
}

void CFwUploadTab::OnPinsChanged() {
    if (loading_) {
        return;
    }
    SetTargetReady(false);
    SaveVisibleToState();
    RefreshPinUsage();
    UpdateEnableState();
}

void CFwUploadTab::OnEnableHost() {
    try {
        EnsureHostEnabled();
    } catch (const std::exception& e) {
        ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"FW Upload", MB_ICONERROR | MB_OK);
    }
}

void CFwUploadTab::OnDisableHost() {
    try {
        if (service_ != nullptr && connected_) {
            service_->I2cDeinit(CurrentPort());
        }
        SetHostEnabled(false);
        SetTargetReady(false);
        AppendStatusText(L"I2C host disabled.");
        if (log_) {
            log_(L"FW upload host disabled.");
        }
    } catch (const std::exception& e) {
        ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"FW Upload", MB_ICONERROR | MB_OK);
    }
}

void CFwUploadTab::OnLoadImage() {
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

void CFwUploadTab::OnConnectTarget() {
    try {
        EnsureHostEnabled();

        cancel_requested_ = false;
        busy_ = true;
        UpdateEnableState();
        struct BusyGuard {
            CFwUploadTab* self;
            ~BusyGuard() {
                self->busy_ = false;
                self->UpdateEnableState();
            }
        } guard{this};

        mfc_tool::core::NuvoIspI2cClient client(service_);
        mfc_tool::core::NuvoIspI2cOptions options;
        options.port = CurrentPort();
        options.addr7 = CurrentAddress();
        options.response_delay_ms = CurrentResponseDelayMs();
        client.SetOptions(options);

        auto cancel = [this]() -> bool { return cancel_requested_; };
        auto poll = [this]() { PumpUiMessages(); };
        SetStatusText(L"Waiting for target ISP handshake...");
        SetProgress(0, L"Connect: waiting for target ISP...");
        SetTargetReady(false, L"Target: waiting for ISP handshake...");
        client.ConnectTarget(cancel, poll);
        if (cancel_requested_) {
            SetTargetReady(false, L"Target: connect stopped");
            AppendStatusText(L"Connect stopped by user.");
            return;
        }

        const std::uint8_t fw = client.GetFirmwareVersion();
        const std::uint32_t id = client.GetDeviceId();
        SetTargetReady(true, L"Target Ready - load BIN and click Program APROM");
        SetProgress(100, L"Connect: target ISP ready - load BIN and click Program APROM");
        AppendStatusText(L"Target ready. ISP FW=0x" + FormatHex32(fw).substr(8) +
                         L", DeviceID=" + FormatHex32(id) + L". Load BIN and click Program APROM.");
        if (log_) {
            log_(L"FW upload target connected: ISP FW=0x" + FormatHex32(fw).substr(8) + L", DeviceID=" + FormatHex32(id));
        }
    } catch (const std::exception& e) {
        if (cancel_requested_ || IsCancelError(e)) {
            SetTargetReady(false, L"Target: connect stopped");
            AppendStatusText(L"Connect stopped by user.");
            return;
        }
        SetTargetReady(false, L"Target: connect failed");
        AppendStatusText(L"Connect failed: " + AnsiToWide(e.what()));
        ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"Connect Target", MB_ICONERROR | MB_OK);
    }
}

void CFwUploadTab::OnGetDeviceId() {
    try {
        EnsureHostEnabled();

        cancel_requested_ = false;
        busy_ = true;
        UpdateEnableState();
        struct BusyGuard {
            CFwUploadTab* self;
            ~BusyGuard() {
                self->busy_ = false;
                self->UpdateEnableState();
            }
        } guard{this};

        mfc_tool::core::NuvoIspI2cClient client(service_);
        mfc_tool::core::NuvoIspI2cOptions options;
        options.port = CurrentPort();
        options.addr7 = CurrentAddress();
        options.response_delay_ms = CurrentResponseDelayMs();
        client.SetOptions(options);
        auto cancel = [this]() -> bool { return cancel_requested_; };
        auto poll = [this]() { PumpUiMessages(); };
        AppendStatusText(L"Waiting for target ISP handshake...");
        SetTargetReady(false, L"Target: waiting for ISP handshake...");
        client.ConnectTarget(cancel, poll);
        if (cancel_requested_) {
            SetTargetReady(false, L"Target: Get Device ID stopped");
            AppendStatusText(L"Get Device ID stopped by user.");
            return;
        }
        const std::uint8_t fw = client.GetFirmwareVersion();
        const std::uint32_t id = client.GetDeviceId();
        SetTargetReady(true, L"Target Ready - load BIN and click Program APROM");
        AppendStatusText(L"ISP FW=0x" + FormatHex32(fw).substr(8) + L", DeviceID=" + FormatHex32(id));
    } catch (const std::exception& e) {
        if (cancel_requested_ || IsCancelError(e)) {
            SetTargetReady(false, L"Target: Get Device ID stopped");
            AppendStatusText(L"Get Device ID stopped by user.");
            return;
        }
        SetTargetReady(false, L"Target: Get Device ID failed");
        AppendStatusText(L"Get Device ID failed: " + AnsiToWide(e.what()));
        ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"Get Device ID", MB_ICONERROR | MB_OK);
    }
}

void CFwUploadTab::OnProgram() {
    try {
        EnsureHostEnabled();
        if (!firmware_image_data_) {
            throw std::invalid_argument("load firmware image first");
        }
        const std::vector<std::uint8_t>& image = firmware_image_data_();
        if (image.empty()) {
            throw std::invalid_argument("load firmware image first");
        }
        const std::wstring image_path = firmware_image_path_ ? firmware_image_path_() : L"";

        cancel_requested_ = false;
        busy_ = true;
        UpdateEnableState();
        struct BusyGuard {
            CFwUploadTab* self;
            ~BusyGuard() {
                self->busy_ = false;
                self->UpdateEnableState();
            }
        } guard{this};

        mfc_tool::core::NuvoIspI2cClient client(service_);
        mfc_tool::core::NuvoIspI2cOptions options;
        options.port = CurrentPort();
        options.addr7 = CurrentAddress();
        options.response_delay_ms = CurrentResponseDelayMs();
        client.SetOptions(options);

        SetStatusText(L"Waiting for target ISP handshake...");
        SetProgress(0, L"Connect: waiting for target ISP...");
        SetTargetReady(false, L"Target: waiting for ISP handshake...");
        auto cancel = [this]() -> bool { return cancel_requested_; };
        auto poll = [this]() { PumpUiMessages(); };

        client.ConnectTarget(cancel, poll);
        SetTargetReady(true, L"Target Ready - programming APROM...");
        SetProgress(0, L"Program: 0%");
        AppendStatusText(L"Program APROM start. Image size=" + std::to_wstring(image.size()) + L" bytes");

        client.ProgramAprom(
            image,
            [this](const mfc_tool::core::NuvoIspI2cProgress& progress) {
                const int percent = progress.total_bytes == 0u
                    ? 0
                    : static_cast<int>((static_cast<unsigned long long>(progress.bytes_done) * 100ull) / progress.total_bytes);
                std::wstring label = L"Program: " + std::to_wstring(progress.bytes_done) + L"/" +
                    std::to_wstring(progress.total_bytes) + L" bytes, packet " +
                    std::to_wstring(progress.packet_index);
                SetProgress(percent, label);
            },
            cancel,
            poll);

        if (cancel_requested_) {
            SetTargetReady(false, L"Target: program stopped");
            AppendStatusText(L"Program stopped by user.");
            if (log_) {
                log_(L"FW upload stopped by user.");
            }
            return;
        }

        SetProgress(100, L"Program: complete");
        SetTargetReady(true, L"Program complete - click Run APROM or program another image");
        AppendStatusText(L"Program APROM complete. Packet checksum/readback validation passed.");
        if (log_) {
            log_(L"FW upload Program APROM complete: " + image_path);
        }
        if (persist_settings_) {
            persist_settings_();
        }
    } catch (const std::exception& e) {
        if (cancel_requested_ || IsCancelError(e)) {
            SetTargetReady(false, L"Target: program stopped");
            AppendStatusText(L"Program stopped by user.");
            if (log_) {
                log_(L"FW upload stopped by user.");
            }
            return;
        }
        SetTargetReady(false, L"Target: program failed");
        AppendStatusText(L"Program failed: " + AnsiToWide(e.what()));
        ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"Program APROM", MB_ICONERROR | MB_OK);
    }
}

void CFwUploadTab::OnRunAprom() {
    try {
        EnsureHostEnabled();

        cancel_requested_ = false;
        busy_ = true;
        UpdateEnableState();
        struct BusyGuard {
            CFwUploadTab* self;
            ~BusyGuard() {
                self->busy_ = false;
                self->UpdateEnableState();
            }
        } guard{this};

        mfc_tool::core::NuvoIspI2cClient client(service_);
        mfc_tool::core::NuvoIspI2cOptions options;
        options.port = CurrentPort();
        options.addr7 = CurrentAddress();
        options.response_delay_ms = CurrentResponseDelayMs();
        client.SetOptions(options);
        auto cancel = [this]() -> bool { return cancel_requested_; };
        auto poll = [this]() { PumpUiMessages(); };
        AppendStatusText(L"Waiting for target ISP handshake...");
        SetTargetReady(false, L"Target: waiting for ISP handshake...");
        client.ConnectTarget(cancel, poll);
        if (cancel_requested_) {
            SetTargetReady(false, L"Target: Run APROM stopped");
            AppendStatusText(L"Run APROM stopped by user.");
            return;
        }
        client.RunAprom();
        SetTargetReady(false, L"Target: Run APROM command sent");
        AppendStatusText(L"Run APROM command sent.");
        if (log_) {
            log_(L"FW upload Run APROM command sent.");
        }
    } catch (const std::exception& e) {
        if (cancel_requested_ || IsCancelError(e)) {
            SetTargetReady(false, L"Target: Run APROM stopped");
            AppendStatusText(L"Run APROM stopped by user.");
            return;
        }
        SetTargetReady(false, L"Target: Run APROM failed");
        AppendStatusText(L"Run APROM failed: " + AnsiToWide(e.what()));
        ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"Run APROM", MB_ICONERROR | MB_OK);
    }
}

void CFwUploadTab::OnResetTarget() {
    try {
        EnsureHostEnabled();

        cancel_requested_ = false;
        busy_ = true;
        UpdateEnableState();
        struct BusyGuard {
            CFwUploadTab* self;
            ~BusyGuard() {
                self->busy_ = false;
                self->UpdateEnableState();
            }
        } guard{this};

        mfc_tool::core::NuvoIspI2cClient client(service_);
        mfc_tool::core::NuvoIspI2cOptions options;
        options.port = CurrentPort();
        options.addr7 = CurrentAddress();
        options.response_delay_ms = CurrentResponseDelayMs();
        client.SetOptions(options);
        auto cancel = [this]() -> bool { return cancel_requested_; };
        auto poll = [this]() { PumpUiMessages(); };
        AppendStatusText(L"Waiting for target ISP handshake...");
        SetTargetReady(false, L"Target: waiting for ISP handshake...");
        client.ConnectTarget(cancel, poll);
        if (cancel_requested_) {
            SetTargetReady(false, L"Target: reset stopped");
            AppendStatusText(L"Reset Target stopped by user.");
            return;
        }
        client.ResetTarget();
        SetTargetReady(false, L"Target: Reset Target command sent");
        AppendStatusText(L"Reset Target command sent.");
        if (log_) {
            log_(L"FW upload Reset Target command sent.");
        }
    } catch (const std::exception& e) {
        if (cancel_requested_ || IsCancelError(e)) {
            SetTargetReady(false, L"Target: reset stopped");
            AppendStatusText(L"Reset Target stopped by user.");
            return;
        }
        SetTargetReady(false, L"Target: reset failed");
        AppendStatusText(L"Reset Target failed: " + AnsiToWide(e.what()));
        ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"Reset Target", MB_ICONERROR | MB_OK);
    }
}

void CFwUploadTab::OnStop() {
    if (busy_) {
        cancel_requested_ = true;
        SetTargetReady(false, L"Target: stop requested");
        AppendStatusText(L"Stopping after current ISP transaction...");
        UpdateEnableState();
    }
}

int CFwUploadTab::CurrentPort() const {
    const int sel = port_combo_.GetCurSel();
    if (sel != CB_ERR) {
        return static_cast<int>(port_combo_.GetItemData(sel));
    }
    try {
        return mfc_tool::core::ParseInt(state_.master_i2c_port) == 1 ? 1 : 0;
    } catch (...) {
        return 0;
    }
}

int CFwUploadTab::CurrentAddress() const {
    return mfc_tool::core::ParseInt(GetEditText(addr_edit_)) & 0x7F;
}

int CFwUploadTab::CurrentSpeedHz() const {
    return std::clamp(mfc_tool::core::ParseInt(GetEditText(speed_edit_)), 10000, 1000000);
}

int CFwUploadTab::CurrentResponseDelayMs() const {
    return std::clamp(mfc_tool::core::ParseInt(GetEditText(delay_edit_)), 0, 1000);
}

std::wstring CFwUploadTab::GetEditText(const CEdit& edit) const {
    CString s;
    const_cast<CEdit&>(edit).GetWindowTextW(s);
    return s.GetString();
}

const mfc_tool::core::board_i2c::PinPair& CFwUploadTab::CurrentPinPair() const {
    const auto& pairs = mfc_tool::core::board_i2c::AllPinPairs();
    const int sel = pins_combo_.GetCurSel();
    if (sel != CB_ERR) {
        const auto idx = static_cast<size_t>(pins_combo_.GetItemData(sel));
        if (idx < pairs.size()) {
            return pairs[idx];
        }
    }
    const auto* fallback = mfc_tool::core::board_i2c::DefaultPinPair(CurrentPort());
    return fallback != nullptr ? *fallback : pairs.front();
}

std::wstring CFwUploadTab::OwnerId() {
    return kFwUploadOwner;
}

std::vector<std::wstring> CFwUploadTab::SharedOwnerIds() {
    return {
        kFwUploadOwner,
    };
}

std::wstring CFwUploadTab::AnsiToWide(const char* text) {
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

std::wstring CFwUploadTab::FormatHex32(std::uint32_t value) {
    std::wstringstream ss;
    ss << L"0x" << std::uppercase << std::hex << std::setw(8) << std::setfill(L'0') << value;
    return ss.str();
}
