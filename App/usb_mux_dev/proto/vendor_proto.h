#ifndef APP_USB_MUX_DEV_VENDOR_PROTO_H
#define APP_USB_MUX_DEV_VENDOR_PROTO_H

#include <stdint.h>

#define VP_MAGIC            0xA55AU
#define VP_VERSION          0x01U
#define VP_IRQ_HINT_VERSION 0x01U

typedef enum {
    VP_CH_SYS       = 0x00,
    VP_CH_UART_CTRL = 0x01,
    VP_CH_UART_DATA = 0x02,
    VP_CH_BLE_MGMT  = 0x10,
    VP_CH_BLE_CONN  = 0x11,
    VP_CH_NET_MGMT  = 0x20,
    VP_CH_NET_DATA  = 0x21,
} vp_channel_type_t;

typedef enum {
    VP_MSG_CMD  = 0x01,
    VP_MSG_RSP  = 0x02,
    VP_MSG_EVT  = 0x03,
    VP_MSG_DATA = 0x04,
} vp_msg_type_t;

typedef enum {
    VP_STATUS_OK                     = 0x0000,
    VP_STATUS_ERR_BAD_MAGIC          = 0x0001,
    VP_STATUS_ERR_BAD_VERSION        = 0x0002,
    VP_STATUS_ERR_BAD_LEN            = 0x0003,
    VP_STATUS_ERR_BAD_HDR_CRC        = 0x0004,
    VP_STATUS_ERR_BAD_PAYLOAD_CRC    = 0x0005,
    VP_STATUS_ERR_UNSUPPORTED_CH     = 0x0006,
    VP_STATUS_ERR_UNSUPPORTED_OPCODE = 0x0007,
    VP_STATUS_ERR_INVALID_PARAM      = 0x0008,
    VP_STATUS_ERR_INVALID_STATE      = 0x0009,
    VP_STATUS_ERR_BUSY               = 0x000A,
    VP_STATUS_ERR_NO_RESOURCE        = 0x000B,
    VP_STATUS_ERR_TIMEOUT            = 0x000C,
    VP_STATUS_ERR_OVERFLOW           = 0x000D,
    VP_STATUS_ERR_UART_MAP_INVALID   = 0x0010,
    VP_STATUS_ERR_UART_NOT_OPEN      = 0x0011,
    VP_STATUS_ERR_BLE_SLOT_INVALID   = 0x0020,
    VP_STATUS_ERR_BLE_NOT_CONNECTED  = 0x0021,
    VP_STATUS_ERR_BLE_DISC_NOT_READY = 0x0022,
    VP_STATUS_ERR_BLE_ATT            = 0x0023,
    VP_STATUS_ERR_INTERNAL           = 0x0030,
} vp_status_t;

typedef struct __attribute__((packed)) {
    uint16_t magic;
    uint8_t  version;
    uint8_t  header_len;
    uint16_t total_len;
    uint16_t seq;
    uint16_t ref_seq;
    uint8_t  ch_type;
    uint8_t  ch_id;
    uint8_t  msg_type;
    uint8_t  opcode;
    uint16_t flags;
    uint16_t status;
    uint16_t payload_len;
    uint16_t header_crc16;
    uint16_t reserved;
} vp_hdr_t;

typedef struct __attribute__((packed)) {
    uint8_t  version;
    uint8_t  urgent_flags;
    uint16_t pending_bitmap;
    uint16_t dropped_bitmap;
    uint16_t reserved;
} vp_irq_hint_t;

#define VP_HEADER_LEN 24U

typedef char vp_hdr_size_must_be_24_bytes[(sizeof(vp_hdr_t) == VP_HEADER_LEN) ? 1 : -1];

#endif
