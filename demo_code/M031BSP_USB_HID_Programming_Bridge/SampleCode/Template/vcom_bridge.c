#include <string.h>

#include "NuMicro.h"
#include "bridge_le.h"
#include "bridge_protocol.h"
#include "hid_tool_api.h"
#include "vcom_bridge.h"

#define VCOM_FRAME_SOF0              0x55u
#define VCOM_FRAME_SOF1              0xAAu
#define VCOM_FRAME_VERSION           0x01u
#define VCOM_FRAME_TYPE_REQUEST      0x10u
#define VCOM_FRAME_TYPE_RESPONSE     0x20u
#define VCOM_FRAME_HEADER_SIZE       12u
#define VCOM_FRAME_TAIL_CRC_SIZE     2u
#define VCOM_RX_RING_SIZE            256u

typedef enum
{
    VCOM_PARSE_WAIT_SOF0 = 0,
    VCOM_PARSE_WAIT_SOF1,
    VCOM_PARSE_HEADER,
    VCOM_PARSE_PAYLOAD
} VCOM_PARSE_STATE_T;

static volatile uint8_t g_vcom_rx_ring[VCOM_RX_RING_SIZE];
static volatile uint16_t g_vcom_rx_head = 0u;
static volatile uint16_t g_vcom_rx_tail = 0u;

static VCOM_PARSE_STATE_T g_vcom_parse_state = VCOM_PARSE_WAIT_SOF0;
static uint8_t g_vcom_header[VCOM_FRAME_HEADER_SIZE];
static uint8_t g_vcom_payload[BRIDGE_MAX_PAYLOAD + VCOM_FRAME_TAIL_CRC_SIZE];
static uint16_t g_vcom_header_index = 0u;
static uint16_t g_vcom_payload_index = 0u;
static uint16_t g_vcom_payload_len = 0u;

static uint16_t VcomBridge_Crc16Ccitt(const uint8_t *data, uint16_t len)
{
    uint16_t crc;
    uint16_t index;
    uint8_t bit;

    crc = 0xFFFFu;
    for (index = 0u; index < len; ++index)
    {
        crc ^= (uint16_t)((uint16_t)data[index] << 8);
        for (bit = 0u; bit < 8u; ++bit)
        {
            if ((crc & 0x8000u) != 0u)
            {
                crc = (uint16_t)((crc << 1) ^ 0x1021u);
            }
            else
            {
                crc = (uint16_t)(crc << 1);
            }
        }
    }
    return crc;
}

static void VcomBridge_ResetParser(void)
{
    g_vcom_parse_state = VCOM_PARSE_WAIT_SOF0;
    g_vcom_header_index = 0u;
    g_vcom_payload_index = 0u;
    g_vcom_payload_len = 0u;
}

void VcomBridge_ResetState(void)
{
    g_vcom_rx_head = 0u;
    g_vcom_rx_tail = 0u;
    VcomBridge_ResetParser();
    memset(g_vcom_header, 0, sizeof(g_vcom_header));
    memset(g_vcom_payload, 0, sizeof(g_vcom_payload));
}

void VcomBridge_OnRxByte(uint8_t data)
{
    uint16_t next;

    next = (uint16_t)((g_vcom_rx_head + 1u) % VCOM_RX_RING_SIZE);
    if (next == g_vcom_rx_tail)
    {
        return;
    }

    g_vcom_rx_ring[g_vcom_rx_head] = data;
    g_vcom_rx_head = next;
}

static uint8_t VcomBridge_PopRxByte(uint8_t *data)
{
    if (g_vcom_rx_tail == g_vcom_rx_head)
    {
        return 0u;
    }

    *data = g_vcom_rx_ring[g_vcom_rx_tail];
    g_vcom_rx_tail = (uint16_t)((g_vcom_rx_tail + 1u) % VCOM_RX_RING_SIZE);
    return 1u;
}

static void VcomBridge_WriteByte(uint8_t data)
{
    while (UART_IS_TX_FULL(UART0) != 0u)
    {
    }
    UART_WRITE(UART0, data);
}

static void VcomBridge_WriteBytes(const uint8_t *data, uint16_t len)
{
    uint16_t index;

    for (index = 0u; index < len; ++index)
    {
        VcomBridge_WriteByte(data[index]);
    }
}

static void VcomBridge_SendResponse(uint16_t seq, uint8_t cmd, uint8_t status,
                                    const uint8_t *payload, uint16_t payload_len)
{
    uint8_t header[VCOM_FRAME_HEADER_SIZE];
    uint8_t tail[VCOM_FRAME_TAIL_CRC_SIZE];
    uint16_t crc;

    if (payload_len > BRIDGE_MAX_PAYLOAD)
    {
        payload_len = 0u;
        status = BRIDGE_STATUS_BAD_PAYLOAD;
    }

    header[0] = VCOM_FRAME_SOF0;
    header[1] = VCOM_FRAME_SOF1;
    header[2] = VCOM_FRAME_VERSION;
    header[3] = VCOM_FRAME_TYPE_RESPONSE;
    Bridge_WriteU16Le(&header[4], seq);
    header[6] = cmd;
    header[7] = status;
    Bridge_WriteU16Le(&header[8], payload_len);
    Bridge_WriteU16Le(&header[10], VcomBridge_Crc16Ccitt(&header[2], 8u));

    VcomBridge_WriteBytes(header, VCOM_FRAME_HEADER_SIZE);
    if ((payload != 0) && (payload_len > 0u))
    {
        VcomBridge_WriteBytes(payload, payload_len);
    }

    crc = VcomBridge_Crc16Ccitt(payload, payload_len);
    Bridge_WriteU16Le(tail, crc);
    VcomBridge_WriteBytes(tail, VCOM_FRAME_TAIL_CRC_SIZE);
    UART_WAIT_TX_EMPTY(UART0);
    BridgeTool_OnResponseSent();
}

static void VcomBridge_ProcessFrame(void)
{
    uint16_t seq;
    uint8_t cmd;
    uint16_t payload_crc;
    uint16_t calc_crc;
    uint8_t out[BRIDGE_MAX_PAYLOAD];
    uint16_t out_len;
    uint8_t status;

    seq = Bridge_ReadU16Le(&g_vcom_header[4]);
    cmd = g_vcom_header[6];
    payload_crc = Bridge_ReadU16Le(&g_vcom_payload[g_vcom_payload_len]);
    calc_crc = VcomBridge_Crc16Ccitt(g_vcom_payload, g_vcom_payload_len);
    if (payload_crc != calc_crc)
    {
        VcomBridge_ResetParser();
        return;
    }

    out_len = 0u;
    status = BridgeTool_DispatchCommand(cmd, g_vcom_payload, g_vcom_payload_len, out, &out_len);
    VcomBridge_SendResponse(seq, cmd, status, out, out_len);
    VcomBridge_ResetParser();
}

static void VcomBridge_HandleHeaderComplete(void)
{
    uint16_t header_crc;
    uint16_t calc_crc;
    uint16_t seq;
    uint8_t cmd;

    seq = Bridge_ReadU16Le(&g_vcom_header[4]);
    cmd = g_vcom_header[6];
    if ((g_vcom_header[2] != VCOM_FRAME_VERSION) ||
        (g_vcom_header[3] != VCOM_FRAME_TYPE_REQUEST))
    {
        VcomBridge_ResetParser();
        return;
    }

    header_crc = Bridge_ReadU16Le(&g_vcom_header[10]);
    calc_crc = VcomBridge_Crc16Ccitt(&g_vcom_header[2], 8u);
    if (header_crc != calc_crc)
    {
        VcomBridge_ResetParser();
        return;
    }

    g_vcom_payload_len = Bridge_ReadU16Le(&g_vcom_header[8]);
    if (g_vcom_payload_len > BRIDGE_MAX_PAYLOAD)
    {
        VcomBridge_SendResponse(seq, cmd, BRIDGE_STATUS_BAD_PAYLOAD, 0, 0u);
        VcomBridge_ResetParser();
        return;
    }

    g_vcom_payload_index = 0u;
    g_vcom_parse_state = VCOM_PARSE_PAYLOAD;
}

static void VcomBridge_ParseByte(uint8_t data)
{
    switch (g_vcom_parse_state)
    {
        case VCOM_PARSE_WAIT_SOF0:
            if (data == VCOM_FRAME_SOF0)
            {
                g_vcom_header[0] = data;
                g_vcom_parse_state = VCOM_PARSE_WAIT_SOF1;
            }
            break;

        case VCOM_PARSE_WAIT_SOF1:
            if (data == VCOM_FRAME_SOF1)
            {
                g_vcom_header[1] = data;
                g_vcom_header_index = 2u;
                g_vcom_parse_state = VCOM_PARSE_HEADER;
            }
            else if (data == VCOM_FRAME_SOF0)
            {
                g_vcom_header[0] = data;
            }
            else
            {
                VcomBridge_ResetParser();
            }
            break;

        case VCOM_PARSE_HEADER:
            g_vcom_header[g_vcom_header_index] = data;
            ++g_vcom_header_index;
            if (g_vcom_header_index >= VCOM_FRAME_HEADER_SIZE)
            {
                VcomBridge_HandleHeaderComplete();
            }
            break;

        case VCOM_PARSE_PAYLOAD:
            g_vcom_payload[g_vcom_payload_index] = data;
            ++g_vcom_payload_index;
            if (g_vcom_payload_index >= (uint16_t)(g_vcom_payload_len + VCOM_FRAME_TAIL_CRC_SIZE))
            {
                VcomBridge_ProcessFrame();
            }
            break;

        default:
            VcomBridge_ResetParser();
            break;
    }
}

void VcomBridge_Process(void)
{
    uint8_t data;

    while (VcomBridge_PopRxByte(&data) != 0u)
    {
        VcomBridge_ParseByte(data);
    }
}
