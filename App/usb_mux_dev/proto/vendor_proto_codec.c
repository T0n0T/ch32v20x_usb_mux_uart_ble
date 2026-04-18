#include "vendor_proto_codec.h"

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

static void VP_WriteLe16(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)(value & 0xFFU);
    dst[1] = (uint8_t)(value >> 8);
}

static uint16_t VP_ReadLe16(const uint8_t *src)
{
    return (uint16_t)((uint16_t)src[0] | ((uint16_t)src[1] << 8));
}

static void VP_SerializeHeader(uint8_t *buf, const vp_hdr_t *hdr, uint16_t header_crc16)
{
    VP_WriteLe16(&buf[VP_HDR_MAGIC_OFFSET], hdr->magic);
    buf[VP_HDR_VERSION_OFFSET] = hdr->version;
    buf[VP_HDR_HEADER_LEN_OFFSET] = hdr->header_len;
    VP_WriteLe16(&buf[VP_HDR_TOTAL_LEN_OFFSET], hdr->total_len);
    VP_WriteLe16(&buf[VP_HDR_SEQ_OFFSET], hdr->seq);
    VP_WriteLe16(&buf[VP_HDR_REF_SEQ_OFFSET], hdr->ref_seq);
    buf[VP_HDR_CH_TYPE_OFFSET] = hdr->ch_type;
    buf[VP_HDR_CH_ID_OFFSET] = hdr->ch_id;
    buf[VP_HDR_MSG_TYPE_OFFSET] = hdr->msg_type;
    buf[VP_HDR_OPCODE_OFFSET] = hdr->opcode;
    VP_WriteLe16(&buf[VP_HDR_FLAGS_OFFSET], hdr->flags);
    VP_WriteLe16(&buf[VP_HDR_STATUS_OFFSET], hdr->status);
    VP_WriteLe16(&buf[VP_HDR_PAYLOAD_LEN_OFFSET], hdr->payload_len);
    VP_WriteLe16(&buf[VP_HDR_CRC16_OFFSET], header_crc16);
    VP_WriteLe16(&buf[VP_HDR_RESERVED_OFFSET], hdr->reserved);
}

static void VP_DeserializeHeader(vp_hdr_t *hdr, const uint8_t *buf)
{
    hdr->magic = VP_ReadLe16(&buf[VP_HDR_MAGIC_OFFSET]);
    hdr->version = buf[VP_HDR_VERSION_OFFSET];
    hdr->header_len = buf[VP_HDR_HEADER_LEN_OFFSET];
    hdr->total_len = VP_ReadLe16(&buf[VP_HDR_TOTAL_LEN_OFFSET]);
    hdr->seq = VP_ReadLe16(&buf[VP_HDR_SEQ_OFFSET]);
    hdr->ref_seq = VP_ReadLe16(&buf[VP_HDR_REF_SEQ_OFFSET]);
    hdr->ch_type = buf[VP_HDR_CH_TYPE_OFFSET];
    hdr->ch_id = buf[VP_HDR_CH_ID_OFFSET];
    hdr->msg_type = buf[VP_HDR_MSG_TYPE_OFFSET];
    hdr->opcode = buf[VP_HDR_OPCODE_OFFSET];
    hdr->flags = VP_ReadLe16(&buf[VP_HDR_FLAGS_OFFSET]);
    hdr->status = VP_ReadLe16(&buf[VP_HDR_STATUS_OFFSET]);
    hdr->payload_len = VP_ReadLe16(&buf[VP_HDR_PAYLOAD_LEN_OFFSET]);
    hdr->header_crc16 = VP_ReadLe16(&buf[VP_HDR_CRC16_OFFSET]);
    hdr->reserved = VP_ReadLe16(&buf[VP_HDR_RESERVED_OFFSET]);
}

static int VP_CheckHeaderShape(const vp_hdr_t *hdr)
{
    if(hdr == 0)
    {
        return -1;
    }

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

    return 0;
}

uint16_t VP_Crc16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFFU;
    uint16_t i;

    if(data == 0)
    {
        return 0U;
    }

    for(i = 0; i < len; ++i)
    {
        uint8_t bit;

        crc ^= (uint16_t)data[i] << 8;
        for(bit = 0; bit < 8; ++bit)
        {
            if((crc & 0x8000U) != 0U)
            {
                crc = (uint16_t)((crc << 1) ^ 0x1021U);
            }
            else
            {
                crc <<= 1;
            }
        }
    }

    return crc;
}

int VP_EncodeHeader(vp_hdr_t *hdr)
{
    uint8_t header_bytes[VP_HEADER_LEN];
    uint16_t crc;

    if(hdr == 0)
    {
        return -1;
    }

    hdr->magic = VP_MAGIC;
    hdr->version = VP_VERSION;
    hdr->header_len = VP_HEADER_LEN;
    hdr->total_len = (uint16_t)(hdr->header_len + hdr->payload_len);
    hdr->header_crc16 = 0U;

    VP_SerializeHeader(header_bytes, hdr, 0U);
    crc = VP_Crc16(header_bytes, VP_HEADER_LEN);
    hdr->header_crc16 = crc;

    return 0;
}

int VP_CheckFrameBounds(const vp_hdr_t *hdr, uint16_t max_len)
{
    if(VP_CheckHeaderShape(hdr) != 0)
    {
        return -1;
    }

    if(hdr->total_len > max_len)
    {
        return -1;
    }

    return 0;
}

int VP_DecodeHeader(const uint8_t *buf, uint16_t len, vp_hdr_t *hdr)
{
    uint8_t header_bytes[VP_HEADER_LEN];
    vp_hdr_t local_hdr;
    uint16_t expected_crc;

    if((buf == 0) || (hdr == 0))
    {
        return -1;
    }

    if(len < VP_HEADER_LEN)
    {
        return -1;
    }

    VP_DeserializeHeader(&local_hdr, buf);

    if(VP_CheckFrameBounds(&local_hdr, len) != 0)
    {
        return -1;
    }

    expected_crc = local_hdr.header_crc16;
    VP_SerializeHeader(header_bytes, &local_hdr, 0U);
    if(VP_Crc16(header_bytes, VP_HEADER_LEN) != expected_crc)
    {
        return -1;
    }

    *hdr = local_hdr;
    hdr->header_crc16 = expected_crc;

    return 0;
}
