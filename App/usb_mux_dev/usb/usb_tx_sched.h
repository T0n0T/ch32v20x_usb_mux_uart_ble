#ifndef APP_USB_MUX_DEV_USB_TX_SCHED_H
#define APP_USB_MUX_DEV_USB_TX_SCHED_H

#include <stdint.h>

#include "../proto/vendor_proto.h"

enum {
    USBTX_PENDING_RSP  = 0x0001,
    USBTX_PENDING_EVT  = 0x0002,
    USBTX_PENDING_DATA = 0x0004,
};

void USBTX_Init(void);
void USBTX_Process(void);

int USBTX_QueueRsp(const vp_hdr_t *req_hdr, vp_status_t status, const void *payload, uint16_t payload_len);
int USBTX_QueueEvt(uint8_t ch_type, uint8_t ch_id, uint8_t opcode, const void *payload, uint16_t payload_len);
int USBTX_QueueData(uint8_t ch_type, uint8_t ch_id, const void *payload, uint16_t payload_len);

#endif
