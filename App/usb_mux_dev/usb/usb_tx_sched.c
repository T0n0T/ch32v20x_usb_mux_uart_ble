#include "usb_tx_sched.h"

#include <string.h>

#include "../config/board_caps.h"
#include "../include/app_log.h"
#include "../include/app_task.h"
#include "../proto/vendor_proto_codec.h"
#include "usb_dev_ll.h"

#define USBTX_QUEUE_DEPTH 4U

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
    uint16_t len;
    uint8_t  data[APP_USB_MAX_FRAME_LEN];
} usbtx_frame_t;

typedef struct {
    usbtx_frame_t slots[USBTX_QUEUE_DEPTH];
    uint8_t       head;
    uint8_t       tail;
    uint8_t       count;
} usbtx_queue_t;

typedef struct {
    usbtx_queue_t rsp_q;
    usbtx_queue_t evt_q;
    usbtx_queue_t data_q;
    uint16_t      seq;
    uint16_t      pending_bitmap;
    uint16_t      dropped_bitmap;
    uint8_t       hint_dirty;
} usbtx_ctx_t;

static usbtx_ctx_t g_usb_tx_ctx;

static void USBTX_WriteLe16(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)(value & 0xFFU);
    dst[1] = (uint8_t)(value >> 8);
}

static void USBTX_SerializeHeader(uint8_t *dst, const vp_hdr_t *hdr)
{
    USBTX_WriteLe16(&dst[VP_HDR_MAGIC_OFFSET], hdr->magic);
    dst[VP_HDR_VERSION_OFFSET] = hdr->version;
    dst[VP_HDR_HEADER_LEN_OFFSET] = hdr->header_len;
    USBTX_WriteLe16(&dst[VP_HDR_TOTAL_LEN_OFFSET], hdr->total_len);
    USBTX_WriteLe16(&dst[VP_HDR_SEQ_OFFSET], hdr->seq);
    USBTX_WriteLe16(&dst[VP_HDR_REF_SEQ_OFFSET], hdr->ref_seq);
    dst[VP_HDR_CH_TYPE_OFFSET] = hdr->ch_type;
    dst[VP_HDR_CH_ID_OFFSET] = hdr->ch_id;
    dst[VP_HDR_MSG_TYPE_OFFSET] = hdr->msg_type;
    dst[VP_HDR_OPCODE_OFFSET] = hdr->opcode;
    USBTX_WriteLe16(&dst[VP_HDR_FLAGS_OFFSET], hdr->flags);
    USBTX_WriteLe16(&dst[VP_HDR_STATUS_OFFSET], hdr->status);
    USBTX_WriteLe16(&dst[VP_HDR_PAYLOAD_LEN_OFFSET], hdr->payload_len);
    USBTX_WriteLe16(&dst[VP_HDR_CRC16_OFFSET], hdr->header_crc16);
    USBTX_WriteLe16(&dst[VP_HDR_RESERVED_OFFSET], hdr->reserved);
}

static void USBTX_UpdatePendingBitmap(void)
{
    uint16_t pending = 0U;

    if(g_usb_tx_ctx.rsp_q.count > 0U)
    {
        pending |= USBTX_PENDING_RSP;
    }

    if(g_usb_tx_ctx.evt_q.count > 0U)
    {
        pending |= USBTX_PENDING_EVT;
    }

    if(g_usb_tx_ctx.data_q.count > 0U)
    {
        pending |= USBTX_PENDING_DATA;
    }

    if(g_usb_tx_ctx.pending_bitmap != pending)
    {
        g_usb_tx_ctx.pending_bitmap = pending;
        g_usb_tx_ctx.hint_dirty = 1U;
    }
}

static int USBTX_QueueFrame(usbtx_queue_t *queue,
                            uint16_t dropped_bit,
                            uint8_t ch_type,
                            uint8_t ch_id,
                            uint8_t msg_type,
                            uint8_t opcode,
                            uint16_t ref_seq,
                            vp_status_t status,
                            const void *payload,
                            uint16_t payload_len)
{
    usbtx_frame_t *frame;
    vp_hdr_t hdr;

    if(payload_len > (APP_USB_MAX_FRAME_LEN - VP_HEADER_LEN))
    {
        g_usb_tx_ctx.dropped_bitmap |= dropped_bit;
        g_usb_tx_ctx.hint_dirty = 1U;
        APP_LOG_USB("queue drop oversize ch=0x%02X id=%u msg=0x%02X op=0x%02X len=%u",
                    ch_type,
                    ch_id,
                    msg_type,
                    opcode,
                    payload_len);
        return -1;
    }

    if((queue == 0) || (queue->count >= USBTX_QUEUE_DEPTH))
    {
        g_usb_tx_ctx.dropped_bitmap |= dropped_bit;
        g_usb_tx_ctx.hint_dirty = 1U;
        APP_LOG_USB("queue full ch=0x%02X id=%u msg=0x%02X op=0x%02X len=%u",
                    ch_type,
                    ch_id,
                    msg_type,
                    opcode,
                    payload_len);
        return -1;
    }

    frame = &queue->slots[queue->tail];
    memset(&hdr, 0, sizeof(hdr));
    hdr.seq = g_usb_tx_ctx.seq++;
    hdr.ref_seq = ref_seq;
    hdr.ch_type = ch_type;
    hdr.ch_id = ch_id;
    hdr.msg_type = msg_type;
    hdr.opcode = opcode;
    hdr.status = (uint16_t)status;
    hdr.payload_len = payload_len;

    if(VP_EncodeHeader(&hdr) != 0)
    {
        g_usb_tx_ctx.dropped_bitmap |= dropped_bit;
        g_usb_tx_ctx.hint_dirty = 1U;
        APP_LOG_USB("encode hdr fail ch=0x%02X id=%u msg=0x%02X op=0x%02X",
                    ch_type,
                    ch_id,
                    msg_type,
                    opcode);
        return -1;
    }

    USBTX_SerializeHeader(frame->data, &hdr);
    if((payload != 0) && (payload_len > 0U))
    {
        memcpy(&frame->data[VP_HEADER_LEN], payload, payload_len);
    }
    frame->len = hdr.total_len;

    queue->tail = (uint8_t)((queue->tail + 1U) % USBTX_QUEUE_DEPTH);
    queue->count++;
    USBTX_UpdatePendingBitmap();
    AppTask_KickUsbTx();
    APP_LOG_USB("queue ok ch=0x%02X id=%u msg=0x%02X op=0x%02X seq=%u len=%u count=%u",
                ch_type,
                ch_id,
                msg_type,
                opcode,
                hdr.seq,
                frame->len,
                queue->count);

    return 0;
}

static usbtx_frame_t *USBTX_PeekQueue(usbtx_queue_t *queue)
{
    if((queue == 0) || (queue->count == 0U))
    {
        return 0;
    }

    return &queue->slots[queue->head];
}

static void USBTX_PopQueue(usbtx_queue_t *queue)
{
    if((queue == 0) || (queue->count == 0U))
    {
        return;
    }

    queue->head = (uint8_t)((queue->head + 1U) % USBTX_QUEUE_DEPTH);
    queue->count--;
    USBTX_UpdatePendingBitmap();
}

static void USBTX_SendHintIfNeeded(void)
{
    vp_irq_hint_t hint;

    if((g_usb_tx_ctx.hint_dirty == 0U) || (USBDEV_CanSendHint() == 0))
    {
        return;
    }

    hint.version = VP_IRQ_HINT_VERSION;
    hint.urgent_flags = 0U;
    hint.pending_bitmap = g_usb_tx_ctx.pending_bitmap;
    hint.dropped_bitmap = g_usb_tx_ctx.dropped_bitmap;
    hint.reserved = 0U;

    if(USBDEV_SendHint(&hint) == 0)
    {
        g_usb_tx_ctx.hint_dirty = 0U;
        g_usb_tx_ctx.dropped_bitmap = 0U;
    }
}

static void USBTX_SendNextFrame(void)
{
    usbtx_frame_t *frame;

    if(USBDEV_CanSendFrame() == 0)
    {
        return;
    }

    frame = USBTX_PeekQueue(&g_usb_tx_ctx.rsp_q);
    if(frame != 0)
    {
        if(USBDEV_SendFrame(frame->data, frame->len) == 0)
        {
            USBTX_PopQueue(&g_usb_tx_ctx.rsp_q);
        }
        return;
    }

    frame = USBTX_PeekQueue(&g_usb_tx_ctx.evt_q);
    if(frame != 0)
    {
        if(USBDEV_SendFrame(frame->data, frame->len) == 0)
        {
            USBTX_PopQueue(&g_usb_tx_ctx.evt_q);
        }
        return;
    }

    frame = USBTX_PeekQueue(&g_usb_tx_ctx.data_q);
    if((frame != 0) && (USBDEV_SendFrame(frame->data, frame->len) == 0))
    {
        USBTX_PopQueue(&g_usb_tx_ctx.data_q);
    }
}

void USBTX_Init(void)
{
    memset(&g_usb_tx_ctx, 0, sizeof(g_usb_tx_ctx));
    g_usb_tx_ctx.seq = 1U;
    g_usb_tx_ctx.hint_dirty = 1U;
}

void USBTX_Process(void)
{
    if(USBDEV_IsConfigured() == 0)
    {
        return;
    }

    USBTX_SendHintIfNeeded();
    USBTX_SendNextFrame();
}

int USBTX_QueueRsp(const vp_hdr_t *req_hdr, vp_status_t status, const void *payload, uint16_t payload_len)
{
    if(req_hdr == 0)
    {
        return -1;
    }

    return USBTX_QueueFrame(&g_usb_tx_ctx.rsp_q,
                            USBTX_PENDING_RSP,
                            req_hdr->ch_type,
                            req_hdr->ch_id,
                            VP_MSG_RSP,
                            req_hdr->opcode,
                            req_hdr->seq,
                            status,
                            payload,
                            payload_len);
}

int USBTX_QueueEvt(uint8_t ch_type, uint8_t ch_id, uint8_t opcode, const void *payload, uint16_t payload_len)
{
    return USBTX_QueueFrame(&g_usb_tx_ctx.evt_q,
                            USBTX_PENDING_EVT,
                            ch_type,
                            ch_id,
                            VP_MSG_EVT,
                            opcode,
                            0U,
                            VP_STATUS_OK,
                            payload,
                            payload_len);
}

int USBTX_QueueData(uint8_t ch_type, uint8_t ch_id, const void *payload, uint16_t payload_len)
{
    return USBTX_QueueFrame(&g_usb_tx_ctx.data_q,
                            USBTX_PENDING_DATA,
                            ch_type,
                            ch_id,
                            VP_MSG_DATA,
                            0U,
                            0U,
                            VP_STATUS_OK,
                            payload,
                            payload_len);
}
