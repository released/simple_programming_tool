#ifndef __VCOM_BRIDGE_H__
#define __VCOM_BRIDGE_H__

#include <stdint.h>

void VcomBridge_ResetState(void);
void VcomBridge_OnRxByte(uint8_t data);
void VcomBridge_Process(void);

#endif /* __VCOM_BRIDGE_H__ */
