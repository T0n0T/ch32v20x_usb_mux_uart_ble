#include "usb_dev_ll.h"

#include <string.h>

#include "Peripheral/inc/ch32v20x_usb.h"
#include "debug.h"

#include "../include/app_log.h"
#include "../include/app_task.h"
#include "../config/board_caps.h"
#include "usb_rx_fsm.h"

#define USB_REQ_TYPE_DIR_MASK        0x80U
#define USB_REQ_TYPE_STANDARD        0x00U

#define USB_REQ_GET_STATUS           0x00U
#define USB_REQ_SET_ADDRESS          0x05U
#define USB_REQ_GET_DESCRIPTOR       0x06U
#define USB_REQ_GET_CONFIGURATION    0x08U
#define USB_REQ_SET_CONFIGURATION    0x09U

#define USB_DESC_TYPE_DEVICE         0x01U
#define USB_DESC_TYPE_CONFIGURATION  0x02U
#define USB_DESC_TYPE_STRING         0x03U

#define USB_EP2_DMA_SIZE             (USBDEV_EP2_SIZE * 2U)
#define USB_EP2_IN_OFFSET            USBDEV_EP2_SIZE
#define USB_ADDRESS_PENDING_NONE     0xFFU

#if defined(__GNUC__) && defined(__riscv)
#define USBDEV_IRQ_ATTR __attribute__((interrupt("WCH-Interrupt-fast")))
#else
#define USBDEV_IRQ_ATTR
#endif

typedef struct __attribute__((packed)) {
    uint8_t  bmRequestType;
    uint8_t  bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} usb_setup_pkt_t;

typedef struct {
    const uint8_t *data;
    uint16_t       remaining;
    uint8_t        pending_address;
} usbdev_ctrl_ep0_t;

typedef struct {
    uint8_t       configured;
    uint8_t       ep1_tx_busy;
    uint8_t       ep2_tx_active;
    usbdev_ctrl_ep0_t ep0;
    uint16_t      ep2_tx_len;
    uint16_t      ep2_tx_offset;
    uint8_t       ep2_tx_stage[APP_USB_MAX_FRAME_LEN];
} usbdev_ctx_t;

static usbdev_ctx_t g_usbdev_ctx;

static __attribute__((aligned(4))) uint8_t g_ep0_dma[USBDEV_EP0_SIZE];
static __attribute__((aligned(4))) uint8_t g_ep1_dma[USBDEV_EP1_SIZE];
static __attribute__((aligned(4))) uint8_t g_ep2_dma[USB_EP2_DMA_SIZE];

static const uint8_t g_usb_dev_desc[] = {
    0x12, 0x01, 0x10, 0x01, 0xFF, 0x00, 0x00, USBDEV_EP0_SIZE,
    0x86, 0x1A, 0x80, 0x20, 0x00, 0x01, 0x01, 0x02,
    0x03, 0x01,
};

static const uint8_t g_usb_cfg_desc[] = {
    0x09, 0x02, 0x27, 0x00, 0x01, 0x01, 0x00, 0x80, 0x32,
    0x09, 0x04, 0x00, 0x00, 0x03, 0xFF, 0x00, 0x00, 0x00,
    0x07, 0x05, 0x81, 0x03, USBDEV_EP1_SIZE, 0x00, 0x01,
    0x07, 0x05, 0x02, 0x02, USBDEV_EP2_SIZE, 0x00, 0x00,
    0x07, 0x05, 0x82, 0x02, USBDEV_EP2_SIZE, 0x00, 0x00,
};

static const uint8_t g_usb_lang_desc[] = {
    0x04, 0x03, 0x09, 0x04,
};

static const uint8_t g_usb_manu_desc[] = {
    0x14, 0x03,
    'C', 0x00, 'o', 0x00, 'd', 0x00, 'e', 0x00,
    'x', 0x00, ' ', 0x00, 'L', 0x00, 'a', 0x00, 'b', 0x00,
};

static const uint8_t g_usb_prod_desc[] = {
    0x22, 0x03,
    'C', 0x00, 'H', 0x00, '3', 0x00, '2', 0x00,
    'V', 0x00, '2', 0x00, '0', 0x00, '8', 0x00,
    ' ', 0x00, 'M', 0x00, 'u', 0x00, 'x', 0x00,
    ' ', 0x00, 'D', 0x00, 'e', 0x00, 'v', 0x00,
};

static const uint8_t g_usb_serial_desc[] = {
    0x12, 0x03,
    '0', 0x00, '0', 0x00, '0', 0x00, '1', 0x00,
    'T', 0x00, '3', 0x00, '0', 0x00, '0', 0x00,
};

static void USBDEV_ConfigClock(void)
{
    RCC_ClocksTypeDef clocks;

    RCC_GetClocksFreq(&clocks);

    if(clocks.SYSCLK_Frequency == 144000000U)
    {
        RCC_USBCLKConfig(RCC_USBCLKSource_PLLCLK_Div3);
    }
    else if(clocks.SYSCLK_Frequency == 96000000U)
    {
        RCC_USBCLKConfig(RCC_USBCLKSource_PLLCLK_Div2);
    }
    else if(clocks.SYSCLK_Frequency == 48000000U)
    {
        RCC_USBCLKConfig(RCC_USBCLKSource_PLLCLK_Div1);
    }
#if defined(CH32V20x_D8W) || defined(CH32V20x_D8)
    else if((clocks.SYSCLK_Frequency == 240000000U) && (RCC_USB5PRE_JUDGE() == SET))
    {
        RCC_USBCLKConfig(RCC_USBCLKSource_PLLCLK_Div5);
    }
#endif

    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_OTG_FS, ENABLE);
    Delay_Us(100U);
}

static void USBDEV_LogLineState(const char *tag)
{
#if APP_LOG_ENABLED(APP_LOG_LEVEL_DEBUG)
    uint8_t base_ctrl = USBFSD->BASE_CTRL;
    uint8_t udev_ctrl = USBFSD->UDEV_CTRL;
    uint8_t int_en = USBFSD->INT_EN;
    uint8_t int_fg = USBFSD->INT_FG;
    uint8_t int_st = USBFSD->INT_ST;

    APP_LOG_USB_DEBUG("%s ext=0x%08lX base=0x%02X udev=0x%02X ien=0x%02X ifg=0x%02X ist=0x%02X dp=%u dm=%u",
                      tag,
                      (unsigned long)EXTEN->EXTEN_CTR,
                      base_ctrl,
                      udev_ctrl,
                      int_en,
                      int_fg,
                      int_st,
                      (udev_ctrl & USBFS_UD_DP_PIN) != 0U ? 1U : 0U,
                      (udev_ctrl & USBFS_UD_DM_PIN) != 0U ? 1U : 0U);
#else
    (void)tag;
#endif
}

static void USBDEV_ResetDataEndpoints(void)
{
    g_usbdev_ctx.configured = 0U;
    g_usbdev_ctx.ep1_tx_busy = 0U;
    g_usbdev_ctx.ep2_tx_active = 0U;
    g_usbdev_ctx.ep2_tx_len = 0U;
    g_usbdev_ctx.ep2_tx_offset = 0U;
    g_usbdev_ctx.ep0.data = 0;
    g_usbdev_ctx.ep0.remaining = 0U;
    g_usbdev_ctx.ep0.pending_address = USB_ADDRESS_PENDING_NONE;

    USBFSD->DEV_ADDR = 0U;
    USBFSD->UEP0_TX_LEN = 0U;
    USBFSD->UEP0_TX_CTRL = USBFS_UEP_T_RES_NAK;
    USBFSD->UEP0_RX_CTRL = USBFS_UEP_R_RES_ACK;
    USBFSD->UEP1_TX_LEN = 0U;
    USBFSD->UEP1_TX_CTRL = USBFS_UEP_T_RES_NAK;
    USBFSD->UEP2_TX_LEN = 0U;
    USBFSD->UEP2_TX_CTRL = USBFS_UEP_T_AUTO_TOG | USBFS_UEP_T_RES_NAK;
    USBFSD->UEP2_RX_CTRL = USBFS_UEP_R_AUTO_TOG | USBFS_UEP_R_RES_ACK;
}

static void USBDEV_LoadEp0Tx(const uint8_t *data, uint16_t len)
{
    if((data != 0) && (len > 0U))
    {
        memcpy(g_ep0_dma, data, len);
    }

    USBFSD->UEP0_TX_LEN = len;
    USBFSD->UEP0_TX_CTRL = (uint8_t)((USBFSD->UEP0_TX_CTRL & USBFS_UEP_T_TOG) | USBFS_UEP_T_RES_ACK);
    USBFSD->UEP0_RX_CTRL = (uint8_t)((USBFSD->UEP0_RX_CTRL & USBFS_UEP_R_TOG) | USBFS_UEP_R_RES_ACK);
}

static void USBDEV_ContinueEp0Tx(void)
{
    uint16_t chunk_len;

    if(g_usbdev_ctx.ep0.remaining == 0U)
    {
        USBDEV_LoadEp0Tx(0, 0U);
        return;
    }

    chunk_len = g_usbdev_ctx.ep0.remaining;
    if(chunk_len > USBDEV_EP0_SIZE)
    {
        chunk_len = USBDEV_EP0_SIZE;
    }

    USBDEV_LoadEp0Tx(g_usbdev_ctx.ep0.data, chunk_len);
    g_usbdev_ctx.ep0.data += chunk_len;
    g_usbdev_ctx.ep0.remaining -= chunk_len;
}

static void USBDEV_StartEp0Data(const uint8_t *data, uint16_t len)
{
    g_usbdev_ctx.ep0.data = data;
    g_usbdev_ctx.ep0.remaining = len;
    USBFSD->UEP0_TX_CTRL = USBFS_UEP_T_TOG | USBFS_UEP_T_RES_NAK;
    USBFSD->UEP0_RX_CTRL = USBFS_UEP_R_TOG | USBFS_UEP_R_RES_ACK;
    USBDEV_ContinueEp0Tx();
}

static void USBDEV_StallEp0(void)
{
    APP_LOG_USB_WARNING("ep0 stall");
    USBFSD->UEP0_TX_CTRL = USBFS_UEP_T_TOG | USBFS_UEP_T_RES_STALL;
    USBFSD->UEP0_RX_CTRL = USBFS_UEP_R_TOG | USBFS_UEP_R_RES_STALL;
}

static void USBDEV_HandleStandardRequest(const usb_setup_pkt_t *setup)
{
    const uint8_t *desc = 0;
    uint16_t desc_len = 0U;

    APP_LOG_USB_DEBUG("std req type=0x%02X req=0x%02X wValue=0x%04X wIndex=0x%04X wLen=%u",
                      setup->bmRequestType,
                      setup->bRequest,
                      setup->wValue,
                      setup->wIndex,
                      setup->wLength);

    switch(setup->bRequest)
    {
        case USB_REQ_GET_DESCRIPTOR:
            switch((uint8_t)(setup->wValue >> 8))
            {
                case USB_DESC_TYPE_DEVICE:
                    desc = g_usb_dev_desc;
                    desc_len = sizeof(g_usb_dev_desc);
                    break;

                case USB_DESC_TYPE_CONFIGURATION:
                    desc = g_usb_cfg_desc;
                    desc_len = sizeof(g_usb_cfg_desc);
                    break;

                case USB_DESC_TYPE_STRING:
                    switch((uint8_t)(setup->wValue & 0xFFU))
                    {
                        case 0U:
                            desc = g_usb_lang_desc;
                            desc_len = sizeof(g_usb_lang_desc);
                            break;

                        case 1U:
                            desc = g_usb_manu_desc;
                            desc_len = sizeof(g_usb_manu_desc);
                            break;

                        case 2U:
                            desc = g_usb_prod_desc;
                            desc_len = sizeof(g_usb_prod_desc);
                            break;

                        case 3U:
                            desc = g_usb_serial_desc;
                            desc_len = sizeof(g_usb_serial_desc);
                            break;

                        default:
                            USBDEV_StallEp0();
                            return;
                    }
                    break;

                default:
                    USBDEV_StallEp0();
                    return;
            }

            if(desc_len > setup->wLength)
            {
                desc_len = setup->wLength;
            }
            USBDEV_StartEp0Data(desc, desc_len);
            break;

        case USB_REQ_SET_ADDRESS:
            g_usbdev_ctx.ep0.pending_address = (uint8_t)(setup->wValue & USBFS_USB_ADDR_MASK);
            APP_LOG_USB_INFO("set address pending=%u", g_usbdev_ctx.ep0.pending_address);
            USBDEV_LoadEp0Tx(0, 0U);
            break;

        case USB_REQ_SET_CONFIGURATION:
            g_usbdev_ctx.configured = (uint8_t)(setup->wValue & 0xFFU);
            APP_LOG_USB_INFO("set configuration=%u", g_usbdev_ctx.configured);
            USBDEV_LoadEp0Tx(0, 0U);
            break;

        case USB_REQ_GET_CONFIGURATION:
            g_ep0_dma[0] = g_usbdev_ctx.configured;
            USBDEV_StartEp0Data(g_ep0_dma, 1U);
            break;

        case USB_REQ_GET_STATUS:
            g_ep0_dma[0] = 0U;
            g_ep0_dma[1] = 0U;
            USBDEV_StartEp0Data(g_ep0_dma, 2U);
            break;

        default:
            USBDEV_StallEp0();
            break;
    }
}

static void USBDEV_HandleEp0Setup(void)
{
    const usb_setup_pkt_t *setup = (const usb_setup_pkt_t *)g_ep0_dma;

    g_usbdev_ctx.ep0.data = 0;
    g_usbdev_ctx.ep0.remaining = 0U;
    USBFSD->UEP0_TX_CTRL = USBFS_UEP_T_TOG | USBFS_UEP_T_RES_NAK;
    USBFSD->UEP0_RX_CTRL = USBFS_UEP_R_TOG | USBFS_UEP_R_RES_ACK;

    if((setup->bmRequestType & 0x60U) != USB_REQ_TYPE_STANDARD)
    {
        USBDEV_StallEp0();
        return;
    }

    USBDEV_HandleStandardRequest(setup);
}

static void USBDEV_HandleEp0In(void)
{
    if(g_usbdev_ctx.ep0.pending_address != USB_ADDRESS_PENDING_NONE)
    {
        USBFSD->DEV_ADDR = g_usbdev_ctx.ep0.pending_address;
        APP_LOG_USB_INFO("address applied=%u", g_usbdev_ctx.ep0.pending_address);
        g_usbdev_ctx.ep0.pending_address = USB_ADDRESS_PENDING_NONE;
    }

    if(g_usbdev_ctx.ep0.remaining > 0U)
    {
        USBFSD->UEP0_TX_CTRL ^= USBFS_UEP_T_TOG;
        USBDEV_ContinueEp0Tx();
    }
    else
    {
        USBFSD->UEP0_TX_LEN = 0U;
        USBFSD->UEP0_TX_CTRL = USBFS_UEP_T_RES_NAK;
        USBFSD->UEP0_RX_CTRL = USBFS_UEP_R_RES_ACK;
    }
}

static void USBDEV_LoadEp2Chunk(void)
{
    uint16_t chunk_len = g_usbdev_ctx.ep2_tx_len - g_usbdev_ctx.ep2_tx_offset;

    if(chunk_len > USBDEV_EP2_SIZE)
    {
        chunk_len = USBDEV_EP2_SIZE;
    }

    memcpy(&g_ep2_dma[USB_EP2_IN_OFFSET], &g_usbdev_ctx.ep2_tx_stage[g_usbdev_ctx.ep2_tx_offset], chunk_len);
    g_usbdev_ctx.ep2_tx_offset += chunk_len;
    USBFSD->UEP2_TX_LEN = chunk_len;
    USBFSD->UEP2_TX_CTRL = (uint8_t)((USBFSD->UEP2_TX_CTRL & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_ACK);
}

static void USBDEV_HandleTransfer(void)
{
    uint8_t token = USBFSD->INT_ST & USBFS_UIS_TOKEN_MASK;
    uint8_t endpoint = USBFSD->INT_ST & USBFS_UIS_ENDP_MASK;

    if(token == USBFS_UIS_TOKEN_SETUP)
    {
        USBDEV_HandleEp0Setup();
        return;
    }

    if((endpoint == 0U) && (token == USBFS_UIS_TOKEN_IN))
    {
        USBDEV_HandleEp0In();
        return;
    }

    if((endpoint == 0U) && (token == USBFS_UIS_TOKEN_OUT))
    {
        USBFSD->UEP0_RX_CTRL = USBFS_UEP_R_RES_ACK;
        return;
    }

    if((endpoint == 1U) && (token == USBFS_UIS_TOKEN_IN))
    {
        g_usbdev_ctx.ep1_tx_busy = 0U;
        USBFSD->UEP1_TX_CTRL = USBFS_UEP_T_RES_NAK;
        return;
    }

    if(endpoint != 2U)
    {
        return;
    }

    if(token == USBFS_UIS_TOKEN_OUT)
    {
        uint16_t rx_len = (uint16_t)USBFSD->RX_LEN;

        if(rx_len > USBDEV_EP2_SIZE)
        {
            rx_len = USBDEV_EP2_SIZE;
        }

        if(rx_len > 0U)
        {
            APP_LOG_USB_DEBUG("ep2 out len=%u", rx_len);
        }
        USBRX_PushBytes(g_ep2_dma, rx_len);
        AppTask_KickUsbRx();
        USBFSD->UEP2_RX_CTRL = USBFS_UEP_R_AUTO_TOG | USBFS_UEP_R_RES_ACK;
        return;
    }

    if(token == USBFS_UIS_TOKEN_IN)
    {
        if(g_usbdev_ctx.ep2_tx_offset < g_usbdev_ctx.ep2_tx_len)
        {
            USBDEV_LoadEp2Chunk();
        }
        else
        {
            g_usbdev_ctx.ep2_tx_active = 0U;
            APP_LOG_USB_DEBUG("ep2 in done len=%u", g_usbdev_ctx.ep2_tx_len);
            AppTask_KickUsbTx();
            USBFSD->UEP2_TX_LEN = 0U;
            USBFSD->UEP2_TX_CTRL = USBFS_UEP_T_AUTO_TOG | USBFS_UEP_T_RES_NAK;
        }
    }
}

void USBMUX_DeviceInit(void)
{
    memset(&g_usbdev_ctx, 0, sizeof(g_usbdev_ctx));
    g_usbdev_ctx.ep0.pending_address = USB_ADDRESS_PENDING_NONE;

    EXTEN->EXTEN_CTR |= EXTEN_USBD_PU_EN;
    USBDEV_ConfigClock();

    USBFSD->BASE_CTRL = USBFS_UC_RESET_SIE | USBFS_UC_CLR_ALL;
    USBFSD->BASE_CTRL = 0U;

    USBFSD->UEP4_1_MOD = USBFS_UEP1_TX_EN;
    USBFSD->UEP2_3_MOD = USBFS_UEP2_RX_EN | USBFS_UEP2_TX_EN;
    USBFSD->UEP5_6_MOD = 0U;
    USBFSD->UEP7_MOD = 0U;

    USBFSD->UEP0_DMA = (uint32_t)(uintptr_t)g_ep0_dma;
    USBFSD->UEP1_DMA = (uint32_t)(uintptr_t)g_ep1_dma;
    USBFSD->UEP2_DMA = (uint32_t)(uintptr_t)g_ep2_dma;

    USBDEV_ResetDataEndpoints();

    USBFSD->INT_FG = 0xFFU;
    USBFSD->INT_EN = USBFS_UIE_BUS_RST | USBFS_UIE_TRANSFER | USBFS_UIE_SUSPEND;
    USBFSD->DEV_ADDR = 0U;
    USBFSD->BASE_CTRL = USBFS_UC_DEV_PU_EN | USBFS_UC_INT_BUSY | USBFS_UC_DMA_EN;
    USBFSD->UDEV_CTRL = USBFS_UD_PD_DIS | USBFS_UD_PORT_EN;

    NVIC_EnableIRQ(USBHD_IRQn);
    APP_LOG_USB_INFO("device init");
    USBDEV_LogLineState("line state after init");
}

int USBDEV_IsConfigured(void)
{
    return g_usbdev_ctx.configured != 0U;
}

uint8_t USBDEV_GetConfiguration(void)
{
    return g_usbdev_ctx.configured;
}

int USBDEV_CanSendHint(void)
{
    return (USBDEV_IsConfigured() != 0) && (g_usbdev_ctx.ep1_tx_busy == 0U);
}

int USBDEV_CanSendFrame(void)
{
    return (USBDEV_IsConfigured() != 0) && (g_usbdev_ctx.ep2_tx_active == 0U);
}

int USBDEV_SendHint(const vp_irq_hint_t *hint)
{
    if((hint == 0) || (USBDEV_CanSendHint() == 0))
    {
        APP_LOG_USB_WARNING("hint send reject");
        return -1;
    }

    memcpy(g_ep1_dma, hint, sizeof(*hint));
    g_usbdev_ctx.ep1_tx_busy = 1U;
    USBFSD->UEP1_TX_LEN = sizeof(*hint);
    USBFSD->UEP1_TX_CTRL = USBFS_UEP_T_RES_ACK;
    APP_LOG_USB_DEBUG("hint send pending=0x%04X dropped=0x%04X",
                      hint->pending_bitmap,
                      hint->dropped_bitmap);

    return 0;
}

int USBDEV_SendFrame(const uint8_t *data, uint16_t len)
{
    if((data == 0) || (len == 0U) || (len > APP_USB_MAX_FRAME_LEN) || (USBDEV_CanSendFrame() == 0))
    {
        APP_LOG_USB_WARNING("frame send reject len=%u", len);
        return -1;
    }

    memcpy(g_usbdev_ctx.ep2_tx_stage, data, len);
    g_usbdev_ctx.ep2_tx_len = len;
    g_usbdev_ctx.ep2_tx_offset = 0U;
    g_usbdev_ctx.ep2_tx_active = 1U;
    APP_LOG_USB_DEBUG("frame send start len=%u", len);
    USBDEV_LoadEp2Chunk();

    return 0;
}

void USBHD_IRQHandler(void) USBDEV_IRQ_ATTR;

void USBHD_IRQHandler(void)
{
    uint8_t int_flag = USBFSD->INT_FG;

    APP_LOG_USB_DEBUG("irq ifg=0x%02X ist=0x%02X mis=0x%02X rx=%lu",
                      int_flag,
                      USBFSD->INT_ST,
                      USBFSD->MIS_ST,
                      (unsigned long)USBFSD->RX_LEN);

    if((int_flag & USBFS_UIF_BUS_RST) != 0U)
    {
        USBDEV_ResetDataEndpoints();
        APP_LOG_USB_INFO("bus reset");
        USBDEV_LogLineState("line state on bus reset");
        USBFSD->INT_FG = USBFS_UIF_BUS_RST;
        int_flag &= (uint8_t)~USBFS_UIF_BUS_RST;
    }

    if((int_flag & USBFS_UIF_TRANSFER) != 0U)
    {
        USBDEV_HandleTransfer();
        USBFSD->INT_FG = USBFS_UIF_TRANSFER;
        int_flag &= (uint8_t)~USBFS_UIF_TRANSFER;
    }

    if((int_flag & USBFS_UIF_SUSPEND) != 0U)
    {
        APP_LOG_USB_INFO("suspend/resume");
        USBDEV_LogLineState("line state on suspend");
        USBFSD->INT_FG = USBFS_UIF_SUSPEND;
        int_flag &= (uint8_t)~USBFS_UIF_SUSPEND;
    }

    if((int_flag & USBFS_UIF_FIFO_OV) != 0U)
    {
        APP_LOG_USB_WARNING("fifo overflow");
        USBFSD->INT_FG = USBFS_UIF_FIFO_OV;
    }
}
