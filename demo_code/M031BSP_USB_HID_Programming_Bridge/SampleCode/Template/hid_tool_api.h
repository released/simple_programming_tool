#ifndef __HID_TOOL_API_H__
#define __HID_TOOL_API_H__

#include <stdint.h>

void HidTool_ResetState(void);
void HidTool_OnOutReady(void);
void HidTool_Process(void);

void HidTool_SetInReport(void);
void HidTool_GetOutReport(uint8_t *pu8EpBuf, uint32_t u32Size);

uint8_t BridgeTool_DispatchCommand(uint8_t cmd, const uint8_t *payload, uint16_t payload_len,
                                   uint8_t *out, uint16_t *out_len);
void BridgeTool_OnResponseSent(void);

#endif /* __HID_TOOL_API_H__ */
