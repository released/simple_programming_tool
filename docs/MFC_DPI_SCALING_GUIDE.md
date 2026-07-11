# MFC DPI Scaling Guide

This note documents the DPI/layout mitigation used by `SimpleProgrammingTool`.

## User-Facing Scale Policy

The GUI is intended to remain usable at these Windows display scales:

- `100%`
- `125%`
- `150%`

The goal is not pixel-identical layout at every scale. The goal is that controls remain readable, reachable, and non-overlapping.

## Supported GUI Areas

- Top-level `Firmware Image` and `INI` rows.
- Top-level tab control and shared log area.
- `FW Upload (I2C)` tab, including HID/VCOM bridge groups, I2C host controls, Nuvoton I2C ISP controls, progress, and response panes.
- `FW Upload (UART ISP)` tab.
- `FW Upload (XMODEM)` tab.

## DPI Helpers

Shared helpers are centralized in `src\ui\layout_utils.h`:

- `GetDpiForHwnd`
- `GetDpiForWnd`
- `DpiScaler`
- `LayoutMetrics`
- `MetricsForWindow`
- `CreatePointFontForWindow`
- `ApplyFontToChildWindows`
- `MeasureTextWidth`
- `MeasureControlTextWidth`
- `MeasureButtonMinWidth`

The helper uses `GetDpiForWindow` when available, then falls back to `GetDeviceCaps(LOGPIXELSX)`.

## Main Frame DPI Handling

`CMainFrame` handles DPI at the window level:

- Tracks `current_dpi_`.
- Handles `WM_DPICHANGED`.
- Applies the suggested DPI-change window rectangle from Windows.
- Recreates the Segoe UI font with `CreatePointFontForWindow`.
- Reapplies the font to top-level controls.
- Calls each tab `RefreshDpiLayout()`.
- Uses `WM_GETMINMAXINFO` to enforce a DPI-scaled minimum window size.
- Scales firmware image row, INI row, tab item size, log controls, margins, rows, and label heights.
- At 150%, gives vertical priority to the tab content by using a compact shared-log height.

## Scaled Metrics

Layouts should use `MetricsForWindow(*this)` and `DpiScaler` instead of fixed pixel constants.

Common scaled values:

- margins: `margin6`, `margin8`
- gap: `gap`
- rows: `row24`, `row26`
- labels: `label18`
- checkboxes: `checkbox20`
- group header offset: `groupTopPad`
- combo drop heights: `comboDrop110`, `comboDrop120`, `comboDrop160`, `comboDrop300`

## Button Text Measurement

Buttons and checkbox-like controls use measured text width before placement:

- `MeasureControlTextWidth`
- `MeasureButtonMinWidth`

This prevents clipped labels such as:

- `Disconnect HID`
- `Refresh COM`
- `Connect Target`
- `Program APROM`
- `Send XMODEM`

When adding a new button, use:

```cpp
const int button_w = (std::max)(dpi.Scale(100), mfc_tool::ui::MeasureButtonMinWidth(button));
```

## Dense Child-Window Tabs

The upload tabs are dense child-window pages: group boxes, combo boxes, edit boxes, progress controls, and status panes are all real child windows. A custom tab-level scrollbar was tested and removed because simply scrolling or relayouting many child controls during scrollbar movement caused stale child-control drawings at 125% scale.

The current mitigation is:

- Do not add tab-level `WS_VSCROLL` for upload tabs.
- Keep the main frame minimum size DPI-scaled so the 100%, 125%, and 150% layouts have enough space.
- Compact the shared bottom log area at 150% so upload controls are not clipped.
- Relayout controls on size/DPI changes.
- Force erase/redraw after layout refresh.

```cpp
SetRedraw(FALSE);
LayoutControls(page);
SetRedraw(TRUE);
RedrawWindow(nullptr, nullptr,
             RDW_INVALIDATE | RDW_ERASE | RDW_ERASENOW |
             RDW_FRAME | RDW_ALLCHILDREN | RDW_UPDATENOW);
```

This avoids stale child-control drawings and stacked ghost controls after switching tabs or scrolling.

## Verification Matrix

| Scale | Required checks |
| --- | --- |
| 100% | Normal and maximized windows, all three upload tabs visible, no clipped buttons, no overlap. |
| 125% | Default validation scale, firmware image/INI rows readable, I2C HID/VCOM groups usable, UART/XMODEM controls reachable. |
| 150% | Dense upload tabs remain reachable at the DPI-scaled minimum window size; no clipped buttons or overlapping controls. |

Also check:

- `Load BIN`, `Save INI`, `Load INI`, and `Reset INI` text is not clipped.
- Tab labels remain readable and centered.
- `FW Upload (I2C)` HID and VCOM groups do not overlap.
- `VCOM Baud` and COM combo boxes remain within group bounds.
- `Response` panes resize with the window.
- Switching between I2C, UART ISP, and XMODEM tabs does not leave stale child windows.

## Non-Goals

- No HID protocol change.
- No VCOM bridge protocol change.
- No UART ISP protocol change.
- No XMODEM packet behavior change.
- No MCU firmware behavior change.
