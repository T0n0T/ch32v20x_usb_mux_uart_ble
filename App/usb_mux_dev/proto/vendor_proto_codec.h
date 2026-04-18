#ifndef APP_USB_MUX_DEV_VENDOR_PROTO_CODEC_H
#define APP_USB_MUX_DEV_VENDOR_PROTO_CODEC_H

#include <stdint.h>

#include "vendor_proto.h"

uint16_t VP_Crc16(const uint8_t *data, uint16_t len);
int VP_EncodeHeader(vp_hdr_t *hdr);
int VP_DecodeHeader(const uint8_t *buf, uint16_t len, vp_hdr_t *hdr);
int VP_CheckFrameBounds(const vp_hdr_t *hdr, uint16_t max_len);

#endif
