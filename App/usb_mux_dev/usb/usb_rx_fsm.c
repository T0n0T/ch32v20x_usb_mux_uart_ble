#include "usb_rx_fsm.h"

#include <string.h>

#include "../common/ring_buffer.h"
#include "../config/board_caps.h"
#include "../include/app_log.h"
#include "../include/app_task.h"
#include "../proto/vendor_proto.h"
#include "../proto/vendor_proto_codec.h"
#include "../proto/vendor_router.h"

#define USBRX_RAW_RING_SIZE (APP_USB_MAX_FRAME_LEN * 2U)

enum {
    VP_HDR_MAGIC_OFFSET       = 0,
    VP_HDR_VERSION_OFFSET     = 2,
    VP_HDR_HEADER_LEN_OFFSET  = 3,
    VP_HDR_TOTAL_LEN_OFFSET   = 4,
    VP_HDR_SEQ_OFFSET         = 6,
    VP_HDR_REF_SEQ_OFFSET     = 8,
    VP_HDR_CH_TYPE_OFFSET     = 10,
    VP_HDR_CH_ID_OFFSET       = 11,
    VP_HDR_MSG_TYPE_OFFSET    = 12,
    VP_HDR_OPCODE_OFFSET      = 13,
    VP_HDR_FLAGS_OFFSET       = 14,
    VP_HDR_STATUS_OFFSET      = 16,
    VP_HDR_PAYLOAD_LEN_OFFSET = 18,
    VP_HDR_CRC16_OFFSET       = 20,
    VP_HDR_RESERVED_OFFSET    = 22,
};

typedef struct {
    usb_rx_state_t state;
    ring_buffer_t  raw_rb;
    uint8_t        raw_storage[USBRX_RAW_RING_SIZE];
    uint8_t        frame_buf[APP_USB_MAX_FRAME_LEN];
    uint16_t       frame_len;
    uint16_t       frame_target_len;
    uint16_t       drop_remaining;
    vp_hdr_t       hdr;
} usb_rx_ctx_t;

static usb_rx_ctx_t g_usb_rx_ctx;

static uint16_t USBRX_ReadLe16(const uint8_t *src)
{
    return (uint16_t)((uint16_t)src[0] | ((uint16_t)src[1] << 8));
}

static int USBRX_ParseHeader(vp_hdr_t *hdr, const uint8_t *buf)
{
    uint8_t header_bytes[VP_HEADER_LEN];

    if((hdr == 0) || (buf == 0))
    {
        return -1;
    }

    hdr->magic = USBRX_ReadLe16(&buf[VP_HDR_MAGIC_OFFSET]);
    hdr->version = buf[VP_HDR_VERSION_OFFSET];
    hdr->header_len = buf[VP_HDR_HEADER_LEN_OFFSET];
    hdr->total_len = USBRX_ReadLe16(&buf[VP_HDR_TOTAL_LEN_OFFSET]);
    hdr->seq = USBRX_ReadLe16(&buf[VP_HDR_SEQ_OFFSET]);
    hdr->ref_seq = USBRX_ReadLe16(&buf[VP_HDR_REF_SEQ_OFFSET]);
    hdr->ch_type = buf[VP_HDR_CH_TYPE_OFFSET];
    hdr->ch_id = buf[VP_HDR_CH_ID_OFFSET];
    hdr->msg_type = buf[VP_HDR_MSG_TYPE_OFFSET];
    hdr->opcode = buf[VP_HDR_OPCODE_OFFSET];
    hdr->flags = USBRX_ReadLe16(&buf[VP_HDR_FLAGS_OFFSET]);
    hdr->status = USBRX_ReadLe16(&buf[VP_HDR_STATUS_OFFSET]);
    hdr->payload_len = USBRX_ReadLe16(&buf[VP_HDR_PAYLOAD_LEN_OFFSET]);
    hdr->header_crc16 = USBRX_ReadLe16(&buf[VP_HDR_CRC16_OFFSET]);
    hdr->reserved = USBRX_ReadLe16(&buf[VP_HDR_RESERVED_OFFSET]);

    if(hdr->magic != VP_MAGIC)
    {
        return -1;
    }

    if(hdr->version != VP_VERSION)
    {
        return -1;
    }

    if(hdr->header_len != VP_HEADER_LEN)
    {
        return -1;
    }

    if(hdr->total_len != (uint16_t)(hdr->header_len + hdr->payload_len))
    {
        return -1;
    }

    if(hdr->total_len > APP_USB_MAX_FRAME_LEN)
    {
        return -1;
    }

    memcpy(header_bytes, buf, VP_HEADER_LEN);
    header_bytes[VP_HDR_CRC16_OFFSET] = 0U;
    header_bytes[VP_HDR_CRC16_OFFSET + 1U] = 0U;

    if(VP_Crc16(header_bytes, VP_HEADER_LEN) != hdr->header_crc16)
    {
        return -1;
    }

    return 0;
}

static void USBRX_ResetFrame(void)
{
    g_usb_rx_ctx.frame_len = 0U;
    g_usb_rx_ctx.frame_target_len = VP_HEADER_LEN;
    g_usb_rx_ctx.drop_remaining = 0U;
    g_usb_rx_ctx.state = USB_RX_IDLE;
}

static int USBRX_ReadByte(uint8_t *byte)
{
    return RingBuffer_PopByte(&g_usb_rx_ctx.raw_rb, byte);
}

static void USBRX_ShiftHeaderWindow(void)
{
    if(g_usb_rx_ctx.frame_len == 0U)
    {
        return;
    }

    memmove(g_usb_rx_ctx.frame_buf, &g_usb_rx_ctx.frame_buf[1], g_usb_rx_ctx.frame_len - 1U);
    g_usb_rx_ctx.frame_len--;
}

void USBRX_Init(void)
{
    memset(&g_usb_rx_ctx, 0, sizeof(g_usb_rx_ctx));
    RingBuffer_Init(&g_usb_rx_ctx.raw_rb, g_usb_rx_ctx.raw_storage, sizeof(g_usb_rx_ctx.raw_storage));
    USBRX_ResetFrame();
}

void USBRX_PushBytes(const uint8_t *data, uint16_t len)
{
    if((data == 0) || (len == 0U))
    {
        return;
    }

    (void)RingBuffer_Write(&g_usb_rx_ctx.raw_rb, data, len);
    AppTask_KickUsbRx();
}

void USBRX_Process(void)
{
    uint8_t byte;

    while(USBRX_ReadByte(&byte) == 0)
    {
        if(g_usb_rx_ctx.state == USB_RX_IDLE)
        {
            g_usb_rx_ctx.state = USB_RX_HEADER;
            g_usb_rx_ctx.frame_len = 0U;
            g_usb_rx_ctx.frame_target_len = VP_HEADER_LEN;
        }

        if(g_usb_rx_ctx.state == USB_RX_DROP)
        {
            if(g_usb_rx_ctx.drop_remaining > 0U)
            {
                g_usb_rx_ctx.drop_remaining--;
            }

            if(g_usb_rx_ctx.drop_remaining == 0U)
            {
                USBRX_ResetFrame();
            }
            continue;
        }

        g_usb_rx_ctx.frame_buf[g_usb_rx_ctx.frame_len++] = byte;

        if(g_usb_rx_ctx.state == USB_RX_HEADER)
        {
            if(g_usb_rx_ctx.frame_len < VP_HEADER_LEN)
            {
                continue;
            }

            if(USBRX_ParseHeader(&g_usb_rx_ctx.hdr, g_usb_rx_ctx.frame_buf) != 0)
            {
                USBRX_ShiftHeaderWindow();
                continue;
            }

            APP_LOG_USB("rx hdr ch=0x%02X id=%u msg=0x%02X op=0x%02X seq=%u total=%u",
                        g_usb_rx_ctx.hdr.ch_type,
                        g_usb_rx_ctx.hdr.ch_id,
                        g_usb_rx_ctx.hdr.msg_type,
                        g_usb_rx_ctx.hdr.opcode,
                        g_usb_rx_ctx.hdr.seq,
                        g_usb_rx_ctx.hdr.total_len);

            g_usb_rx_ctx.frame_target_len = g_usb_rx_ctx.hdr.total_len;

            if(g_usb_rx_ctx.frame_target_len == VP_HEADER_LEN)
            {
                g_usb_rx_ctx.state = USB_RX_DISPATCH;
            }
            else
            {
                g_usb_rx_ctx.state = USB_RX_PAYLOAD;
            }
        }

        if((g_usb_rx_ctx.state == USB_RX_PAYLOAD) &&
           (g_usb_rx_ctx.frame_len >= g_usb_rx_ctx.frame_target_len))
        {
            g_usb_rx_ctx.state = USB_RX_DISPATCH;
        }

        if(g_usb_rx_ctx.state == USB_RX_DISPATCH)
        {
            APP_LOG_USB("rx dispatch ch=0x%02X id=%u msg=0x%02X op=0x%02X payload=%u",
                        g_usb_rx_ctx.hdr.ch_type,
                        g_usb_rx_ctx.hdr.ch_id,
                        g_usb_rx_ctx.hdr.msg_type,
                        g_usb_rx_ctx.hdr.opcode,
                        g_usb_rx_ctx.hdr.payload_len);
            VendorRouter_Dispatch(&g_usb_rx_ctx.hdr,
                                  &g_usb_rx_ctx.frame_buf[VP_HEADER_LEN],
                                  g_usb_rx_ctx.hdr.payload_len);
            USBRX_ResetFrame();
        }
    }
}
