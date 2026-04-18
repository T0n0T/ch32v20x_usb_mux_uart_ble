#ifndef APP_USB_MUX_DEV_VENDOR_ROUTER_H
#define APP_USB_MUX_DEV_VENDOR_ROUTER_H

#include <stdint.h>

#include "vendor_proto.h"

typedef enum {
    VP_SYS_OP_GET_DEV_INFO  = 0x01,
    VP_SYS_OP_GET_CAPS      = 0x02,
    VP_SYS_OP_GET_STATS     = 0x03,
    VP_SYS_OP_CLEAR_STATS   = 0x04,
    VP_SYS_OP_SET_LOG_LEVEL = 0x05,
    VP_SYS_OP_GET_LOG_LEVEL = 0x06,
    VP_SYS_OP_HEARTBEAT     = 0x07,
    VP_SYS_OP_SOFT_RESET    = 0x08,
} vp_sys_opcode_t;

typedef struct __attribute__((packed)) {
    uint8_t proto_version;
    uint8_t fw_major;
    uint8_t fw_minor;
    uint8_t fw_patch;
    uint8_t uart_port_count;
    uint8_t ble_max_links;
    uint8_t reserved0;
    uint8_t reserved1;
} vendor_sys_dev_info_rsp_t;

typedef struct __attribute__((packed)) {
    uint32_t caps_bitmap;
    uint8_t  uart_port_count;
    uint8_t  ble_max_links;
    uint8_t  net_reserved;
    uint8_t  reserved0;
} vendor_sys_caps_rsp_t;

void VendorRouter_Init(void);
void VendorRouter_Dispatch(const vp_hdr_t *hdr, const uint8_t *payload, uint16_t payload_len);

#endif
