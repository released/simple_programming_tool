# SELF_CHECK.md

## Current Verification Results

Date: 2026-07-06

## PC Tool

Command:

```bat
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\build_mfc.ps1 -Configuration Release -Platform x64
```

Result: PASS

- Built `build\SimpleProgrammingTool.exe`
- MSBuild result: 0 warnings, 0 errors
- Latest run after moving I2C bridge HID/VCOM controls into the I2C tab: PASS at 2026-07-06 23:28:29
- UI coordinate smoke test: PASS. Top-level rows are Firmware Image then INI/Build; visible I2C tab contains `I2C Bridge - HID`, `I2C Bridge - VCOM`, Bridge status, I2C Host, Nuvoton I2C ISP, and Response sections without main-frame overlap.

## Firmware Keil Build

Result: NOT RUN

Reason: Keil `UV4`/`armclang` command line is not available in this shell.

## Hardware Test

Result: NOT RUN in this workspace session.

Recommended hardware smoke tests:

1. Flash the M032 programming bridge firmware.
2. Launch `build\SimpleProgrammingTool.exe`.
3. Scan/connect VID `0x0416`, PID `0x5020`.
4. Click `Get Info`; expect `m032-programming-bridge/1.0.0`.
5. Wire the selected M032 I2C pin pair to the target board I2C ISP pins and ground.
6. Boot/reset the target into I2C ISP LDROM.
7. Click `Connect Target`; confirm target-ready status and PDID/FW log.
8. Load a small APROM `.bin`, click `Program APROM`, then click `Run APROM`.

## Known Gaps

- Hardware validation must be rerun after the split.
- VCOM PC transport is implemented, but VCOM hardware validation and matching M032 bridge firmware parser validation are NOT RUN in this workspace session.
- UART ISP and XMODEM PC tabs are implemented, but hardware validation against the referenced Nuvoton UART ISP and XMODEM bootloaders is NOT RUN in this workspace session.
