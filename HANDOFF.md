# HANDOFF.md

## Current State

`simple_programming_tool` is the standalone firmware programming workspace split out from `simple_pmbus_smbus_tool`.

The PC GUI builds as `build\SimpleProgrammingTool.exe` and currently exposes three FW upload tabs: I2C via HID/VCOM bridge, direct UART ISP, and XMODEM transfer. The I2C tab uses the M032 EVB as a USB HID or VCOM to I2C host bridge. The UART ISP and XMODEM tabs open direct COM ports to target bootloader firmware.

## Implemented

- Standalone MFC solution/project: `SimpleProgrammingTool.sln`, `SimpleProgrammingTool.vcxproj`.
- HID defaults: VID `0x0416`, PID `0x5020`.
- Standalone INI: `simple_programming_tool.ini`.
- `FW Upload (I2C...)` tab migrated from `simple_pmbus_smbus_tool`.
- VCOM PC transport added using framed serial packets compatible with the `simple_serial_to_CAN_tool` framing style.
- HID and VCOM are I2C bridge left/right checkbox groups inside `FW Upload (I2C...)`; selecting one disables the other group's controls. UART ISP and XMODEM tabs do not use this bridge UI because those tabs open direct COM ports.
- Firmware image path and `Load BIN` are top-level shared controls used by I2C, UART ISP, and XMODEM tabs. INI path and `Save INI`/`Load INI`/`Reset INI` are the second top-level row.
- `FW Upload (UART ISP)` tab added for standard Nuvoton UART ISP 64-byte packet programming.
- `FW Upload (XMODEM)` tab added for bootloaders that previously used Tera Term XMODEM transfer.
- Nuvoton I2C ISP helper migrated as `src\core\nuvo_isp_i2c.cpp`.
- M032 programming bridge firmware copied under `demo_code\M031BSP_USB_HID_Programming_Bridge`.
- M032 programming bridge firmware identity: `m032-programming-bridge/1.0.0`.
- M031 target I2C ISP 4KB and 2KB LDROM samples copied under `demo_code\`.
- VCOM transport assessment documented in `docs\VCOM_TRANSPORT_ASSESSMENT.md`.

## Verification Done

- PC Release x64 build: PASS, `build\SimpleProgrammingTool.exe`, 0 warnings, 0 errors.
- UI layout coordinate smoke test: PASS. Firmware Image and INI rows stay in the main frame; HID/VCOM bridge controls are located inside the visible I2C tab.
- Hardware test in this workspace session: NOT RUN.
- Keil firmware rebuild in this shell: NOT RUN.

## Next Recommended Work

1. Build and flash `demo_code\M031BSP_USB_HID_Programming_Bridge\SampleCode\Template\Keil\Template.uvprojx`.
2. Launch `build\SimpleProgrammingTool.exe`, connect HID, and verify `Get Info` reports `m032-programming-bridge/1.0.0`.
3. Test `Connect Target`, `Get Device ID`, `Program APROM`, and `Run APROM` against both target LDROM variants as applicable.
4. Hardware-validate UART ISP and XMODEM tabs against target bootloaders.
