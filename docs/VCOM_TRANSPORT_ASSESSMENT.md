# VCOM Transport Assessment

## Summary

Adding VCOM to `simple_programming_tool` is feasible as a second transport layer after the HID split is stable.

As of 2026-07-06, the PC tool has a VCOM transport path behind `BridgeService`. The shared Nuvoton I2C ISP programming state machine is still used for both HID and VCOM. Hardware validation still depends on matching M032 bridge firmware that parses the framed VCOM commands and issues the same I2C host operations.

The first milestone kept USB HID only. HID had already been validated for target connect, device ID, APROM programming, and run/reset commands. VCOM was then added as a separate PC transport path behind the bridge service.

## Recommended Architecture

Keep the Nuvoton ISP programming flow independent from the PC-to-bridge transport:

- Transport-facing bridge service:
  - connect/disconnect
  - get bridge info
  - initialize I2C host
  - write 64-byte ISP packet
  - read 64-byte ISP response
  - deinitialize I2C host
- HID implementation:
  - wraps current `BridgeService`
  - uses 64-byte HID reports and staged I2C write/read commands
- VCOM implementation:
  - wraps a COM port
  - uses a framed command protocol with sequence, command, length, payload, header CRC16, and payload CRC16
  - M032 bridge firmware parses VCOM frames and calls the same I2C host functions used by HID

Do not duplicate the APROM programming state machine. The retry/checksum/progress/cancel logic should be shared by HID and VCOM.

## Firmware Impact

If VCOM uses the M032 EVB user firmware endpoint, M032 firmware needs:

- CDC/VCOM USB descriptors and endpoints, or an existing UART/CDC path that the PC can open as a COM port.
- A VCOM command parser.
- TX/RX buffering that does not block I2C ISR/progress.
- The same Nuvoton I2C ISP final-byte NACK handling that the HID programming bridge currently exposes for 64-byte packet writes.

If the visible COM port is only a Nu-Link/debug VCOM not connected to the M032 user firmware, it cannot program through this bridge unless the board routes that serial channel into firmware-controlled UART pins and a parser is added.

## Reference Projects

Useful references for the VCOM/serial layer:

- The local `simple_serial_to_CAN_tool` workspace for serial-port UI/state handling.
- Prior M2A23 CAN APROM and PWM/LIN bridge samples for packet and boot-safety patterns.

The likely reusable parts are serial-port UI/state handling, packet framing, timeout/retry patterns, and APROM boot-safety concepts. The I2C ISP command flow should remain based on the Nuvoton `ISP_I2C` packet protocol already implemented here.

## Suggested Milestones

1. Complete HID-only split and verify no FW Upload code remains in `simple_pmbus_smbus_tool`.
2. Add an explicit transport abstraction around the current HID bridge calls and VCOM path.
3. Add M032 firmware VCOM parser and validate connect/device ID only.
4. Validate APROM programming over VCOM and compare GUI log, target log, and LA traces against the HID path.
5. Expand FW upload tabs if target-side update interfaces beyond I2C are added.
