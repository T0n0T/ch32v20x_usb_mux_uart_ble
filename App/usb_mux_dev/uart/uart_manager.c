#include "uart_manager.h"

#include <string.h>

#include "board_caps.h"
#include "ch32v20x_gpio.h"
#include "app_task.h"
#include "ring_buffer.h"
#include "uart_map_config.h"
#include "uart_port_ch32v20x.h"
#include "usb_tx_sched.h"

#define UART_RX_RB_SIZE      256U
#define UART_TX_RB_SIZE      256U
#define UART_USB_CHUNK_SIZE   64U

typedef struct {
    uint8_t logic_port;
    uint8_t phy_uart_id;
    uint8_t state;
    ring_buffer_t rx_rb;
    ring_buffer_t tx_rb;
    uint8_t rx_storage[UART_RX_RB_SIZE];
    uint8_t tx_storage[UART_TX_RB_SIZE];
    uint32_t baudrate;
    uint32_t rx_bytes;
    uint32_t tx_bytes;
    uint32_t drop_rx_bytes;
    uint32_t drop_tx_bytes;
    uint8_t data_bits;
    uint8_t parity;
    uint8_t stop_bits;
} uart_port_ctx_t;

static uart_port_ctx_t g_uart_ports[APP_UART_PORT_COUNT];

static uint32_t UartMgr_ReadLe32(const uint8_t *src)
{
    return (uint32_t)src[0]
         | ((uint32_t)src[1] << 8)
         | ((uint32_t)src[2] << 16)
         | ((uint32_t)src[3] << 24);
}

static void UartMgr_WriteLe16(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)(value & 0xFFU);
    dst[1] = (uint8_t)(value >> 8);
}

static void UartMgr_WriteLe32(uint8_t *dst, uint32_t value)
{
    dst[0] = (uint8_t)(value & 0xFFU);
    dst[1] = (uint8_t)((value >> 8) & 0xFFU);
    dst[2] = (uint8_t)((value >> 16) & 0xFFU);
    dst[3] = (uint8_t)((value >> 24) & 0xFFU);
}

static uint8_t UartMgr_PortCode(uint32_t gpio_addr)
{
    GPIO_TypeDef *gpio = (GPIO_TypeDef *)(uintptr_t)gpio_addr;

    if(gpio == GPIOA)
    {
        return 0U;
    }
    if(gpio == GPIOB)
    {
        return 1U;
    }
    if(gpio == GPIOC)
    {
        return 2U;
    }
    if(gpio == GPIOD)
    {
        return 3U;
    }

    return 0xFFU;
}

static uart_port_ctx_t *UartMgr_GetPort(uint8_t logic_port)
{
    if(logic_port >= APP_UART_PORT_COUNT)
    {
        return 0;
    }

    return &g_uart_ports[logic_port];
}

static vp_status_t UartMgr_ValidateCtrlPort(const vp_hdr_t *hdr, uart_port_ctx_t **port_out)
{
    uart_port_ctx_t *port;

    if(hdr->ch_id >= APP_UART_PORT_COUNT)
    {
        return VP_STATUS_ERR_INVALID_PARAM;
    }

    port = &g_uart_ports[hdr->ch_id];
    if(port->state == UART_DISABLED)
    {
        return VP_STATUS_ERR_UART_MAP_INVALID;
    }

    if(port_out != 0)
    {
        *port_out = port;
    }

    return VP_STATUS_OK;
}

static void UartMgr_QueueStatusRsp(const vp_hdr_t *hdr, vp_status_t status)
{
    (void)USBTX_QueueRsp(hdr, status, 0, 0U);
}

static void UartMgr_ReplyPortCap(const vp_hdr_t *hdr, const uart_port_ctx_t *port)
{
    uint8_t payload[12];

    payload[0] = port->logic_port;
    payload[1] = port->phy_uart_id;
    payload[2] = port->state;
    payload[3] = 0U;
    UartMgr_WriteLe16(&payload[4], UART_RX_RB_SIZE - 1U);
    UartMgr_WriteLe16(&payload[6], UART_TX_RB_SIZE - 1U);
    payload[8] = 1U;
    payload[9] = 1U;
    payload[10] = 0U;
    payload[11] = 0U;

    (void)USBTX_QueueRsp(hdr, VP_STATUS_OK, payload, sizeof(payload));
}

static void UartMgr_ReplyPortMap(const vp_hdr_t *hdr, const uart_port_ctx_t *port)
{
    uint8_t payload[12];
    const uart_map_cfg_t *cfg = &g_uart_map_cfg[port->logic_port];

    payload[0] = cfg->enable;
    payload[1] = cfg->phy_uart_id;
    payload[2] = UartMgr_PortCode(cfg->tx_port);
    payload[3] = UartMgr_PortCode(cfg->rx_port);
    UartMgr_WriteLe16(&payload[4], cfg->tx_pin);
    UartMgr_WriteLe16(&payload[6], cfg->rx_pin);
    UartMgr_WriteLe32(&payload[8], port->baudrate);

    (void)USBTX_QueueRsp(hdr, VP_STATUS_OK, payload, sizeof(payload));
}

static void UartMgr_ReplyStats(const vp_hdr_t *hdr, const uart_port_ctx_t *port)
{
    uint8_t payload[20];

    UartMgr_WriteLe32(&payload[0], port->rx_bytes);
    UartMgr_WriteLe32(&payload[4], port->tx_bytes);
    UartMgr_WriteLe32(&payload[8], port->drop_rx_bytes);
    UartMgr_WriteLe32(&payload[12], port->drop_tx_bytes);
    payload[16] = port->state;
    payload[17] = port->data_bits;
    payload[18] = port->parity;
    payload[19] = port->stop_bits;

    (void)USBTX_QueueRsp(hdr, VP_STATUS_OK, payload, sizeof(payload));
}

static vp_status_t UartMgr_ParseLineCoding(const uint8_t *payload,
                                           uint16_t payload_len,
                                           uint32_t *baudrate,
                                           uint8_t *data_bits,
                                           uint8_t *parity,
                                           uint8_t *stop_bits)
{
    if((payload == 0) || (payload_len < 4U))
    {
        return VP_STATUS_ERR_INVALID_PARAM;
    }

    *baudrate = UartMgr_ReadLe32(payload);
    *data_bits = (payload_len >= 5U) ? payload[4] : 8U;
    *parity = (payload_len >= 6U) ? payload[5] : 0U;
    *stop_bits = (payload_len >= 7U) ? payload[6] : 1U;

    if((*baudrate == 0U) || (*data_bits < 8U) || (*data_bits > 9U) || (*parity > 2U))
    {
        return VP_STATUS_ERR_INVALID_PARAM;
    }

    if((*stop_bits != 1U) && (*stop_bits != 2U))
    {
        return VP_STATUS_ERR_INVALID_PARAM;
    }

    if((payload_len >= 8U) && (payload[7] != 0U))
    {
        return VP_STATUS_ERR_UNSUPPORTED_OPCODE;
    }

    return VP_STATUS_OK;
}

static vp_status_t UartMgr_Open(uart_port_ctx_t *port,
                                uint32_t baudrate,
                                uint8_t data_bits,
                                uint8_t parity,
                                uint8_t stop_bits)
{
    vp_status_t status;

    port->state = UART_OPENING;
    status = UARTPort_Open(port->logic_port, baudrate, data_bits, parity, stop_bits);
    if(status != VP_STATUS_OK)
    {
        port->state = UART_ERROR;
        return status;
    }

    port->baudrate = baudrate;
    port->data_bits = data_bits;
    port->parity = parity;
    port->stop_bits = stop_bits;
    port->state = UART_OPEN;

    return VP_STATUS_OK;
}

void UartMgr_Init(void)
{
    uint8_t port;

    memset(g_uart_ports, 0, sizeof(g_uart_ports));

    for(port = 0U; port < APP_UART_PORT_COUNT; ++port)
    {
        uart_port_ctx_t *ctx = &g_uart_ports[port];

        ctx->logic_port = port;
        ctx->phy_uart_id = g_uart_map_cfg[port].phy_uart_id;
        ctx->state = (g_uart_map_cfg[port].enable != 0U) ? UART_CLOSED : UART_DISABLED;
        ctx->baudrate = 115200UL;
        ctx->data_bits = 8U;
        ctx->parity = 0U;
        ctx->stop_bits = 1U;
        RingBuffer_Init(&ctx->rx_rb, ctx->rx_storage, sizeof(ctx->rx_storage));
        RingBuffer_Init(&ctx->tx_rb, ctx->tx_storage, sizeof(ctx->tx_storage));
    }
}

void UartMgr_HandleCtrl(const vp_hdr_t *hdr, const uint8_t *payload, uint16_t payload_len)
{
    uart_port_ctx_t *port;
    vp_status_t status;
    uint32_t baudrate;
    uint8_t data_bits;
    uint8_t parity;
    uint8_t stop_bits;

    status = UartMgr_ValidateCtrlPort(hdr, &port);
    if(status != VP_STATUS_OK)
    {
        UartMgr_QueueStatusRsp(hdr, status);
        return;
    }

    switch(hdr->opcode)
    {
        case VP_UART_GET_PORT_CAP:
            if(payload_len != 0U)
            {
                UartMgr_QueueStatusRsp(hdr, VP_STATUS_ERR_INVALID_PARAM);
                return;
            }
            UartMgr_ReplyPortCap(hdr, port);
            return;

        case VP_UART_GET_PORT_MAP:
            if(payload_len != 0U)
            {
                UartMgr_QueueStatusRsp(hdr, VP_STATUS_ERR_INVALID_PARAM);
                return;
            }
            UartMgr_ReplyPortMap(hdr, port);
            return;

        case VP_UART_OPEN:
            status = UartMgr_ParseLineCoding(payload, payload_len, &baudrate, &data_bits, &parity, &stop_bits);
            if(status != VP_STATUS_OK)
            {
                UartMgr_QueueStatusRsp(hdr, status);
                return;
            }
            if((port->state != UART_CLOSED) && (port->state != UART_ERROR))
            {
                UartMgr_QueueStatusRsp(hdr, VP_STATUS_ERR_INVALID_STATE);
                return;
            }
            status = UartMgr_Open(port, baudrate, data_bits, parity, stop_bits);
            UartMgr_QueueStatusRsp(hdr, status);
            return;

        case VP_UART_CLOSE:
            UARTPort_Close(port->logic_port);
            RingBuffer_Reset(&port->rx_rb);
            RingBuffer_Reset(&port->tx_rb);
            port->state = UART_CLOSED;
            UartMgr_QueueStatusRsp(hdr, VP_STATUS_OK);
            return;

        case VP_UART_SET_LINE_CODING:
            status = UartMgr_ParseLineCoding(payload, payload_len, &baudrate, &data_bits, &parity, &stop_bits);
            if(status != VP_STATUS_OK)
            {
                UartMgr_QueueStatusRsp(hdr, status);
                return;
            }
            if(port->state != UART_OPEN)
            {
                UartMgr_QueueStatusRsp(hdr, VP_STATUS_ERR_UART_NOT_OPEN);
                return;
            }
            status = UARTPort_Open(port->logic_port, baudrate, data_bits, parity, stop_bits);
            if(status == VP_STATUS_OK)
            {
                port->baudrate = baudrate;
                port->data_bits = data_bits;
                port->parity = parity;
                port->stop_bits = stop_bits;
            }
            UartMgr_QueueStatusRsp(hdr, status);
            return;

        case VP_UART_GET_STATS:
            if(payload_len != 0U)
            {
                UartMgr_QueueStatusRsp(hdr, VP_STATUS_ERR_INVALID_PARAM);
                return;
            }
            UartMgr_ReplyStats(hdr, port);
            return;

        case VP_UART_FLUSH_RX:
            RingBuffer_Reset(&port->rx_rb);
            UartMgr_QueueStatusRsp(hdr, VP_STATUS_OK);
            return;

        case VP_UART_FLUSH_TX:
            RingBuffer_Reset(&port->tx_rb);
            UartMgr_QueueStatusRsp(hdr, VP_STATUS_OK);
            return;

        default:
            UartMgr_QueueStatusRsp(hdr, VP_STATUS_ERR_UNSUPPORTED_OPCODE);
            return;
    }
}

vp_status_t UartMgr_WriteFromHost(uint8_t logic_port, const uint8_t *payload, uint16_t payload_len)
{
    uart_port_ctx_t *port = UartMgr_GetPort(logic_port);
    uint16_t written;

    if((port == 0) || (port->state == UART_DISABLED))
    {
        return VP_STATUS_ERR_UART_MAP_INVALID;
    }

    if(port->state != UART_OPEN)
    {
        return VP_STATUS_ERR_UART_NOT_OPEN;
    }

    written = RingBuffer_Write(&port->tx_rb, payload, payload_len);
    if(written < payload_len)
    {
        port->drop_tx_bytes += (uint32_t)(payload_len - written);
    }

    UARTPort_TryKickTx(logic_port);
    AppTask_KickUart();
    return (written == payload_len) ? VP_STATUS_OK : VP_STATUS_ERR_OVERFLOW;
}

void UartMgr_Process(void)
{
    uint8_t port;

    for(port = 0U; port < APP_UART_PORT_COUNT; ++port)
    {
        uart_port_ctx_t *ctx = &g_uart_ports[port];
        uint8_t frame[UART_USB_CHUNK_SIZE];
        uint16_t read_len;

        if(ctx->state != UART_OPEN)
        {
            continue;
        }

        read_len = RingBuffer_Read(&ctx->rx_rb, frame, sizeof(frame));
        if(read_len == 0U)
        {
            continue;
        }

        if(USBTX_QueueData(VP_CH_UART_DATA, port, frame, read_len) != 0)
        {
            ctx->drop_rx_bytes += read_len;
        }
    }
}

void UartMgr_IrqRxByte(uint8_t logic_port, uint8_t data)
{
    uart_port_ctx_t *port = UartMgr_GetPort(logic_port);

    if((port == 0) || (port->state != UART_OPEN))
    {
        return;
    }

    if(RingBuffer_PushByte(&port->rx_rb, data) != 0)
    {
        port->drop_rx_bytes++;
        return;
    }

    port->rx_bytes++;
    AppTask_KickUart();
}

int UartMgr_IrqTxNextByte(uint8_t logic_port, uint8_t *data)
{
    uart_port_ctx_t *port = UartMgr_GetPort(logic_port);

    if((port == 0) || (port->state != UART_OPEN) || (data == 0))
    {
        return -1;
    }

    if(RingBuffer_PopByte(&port->tx_rb, data) != 0)
    {
        return -1;
    }

    port->tx_bytes++;
    return 0;
}
