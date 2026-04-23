#include "vendor_router.h"

#include <string.h>

#include "../config/board_caps.h"
#include "../ble/ble_host_manager.h"
#include "../include/app_log.h"
#include "../uart/uart_manager.h"
#include "../usb/usb_tx_sched.h"

#define VENDOR_SYS_CAP_UART_MASK          0x0000000FU
#define VENDOR_SYS_CAP_BLE_SCAN           0x00000010U
#define VENDOR_SYS_CAP_BLE_DISCOVERY      0x00000020U
#define VENDOR_SYS_CAP_BLE_NOTIFY_SUB     0x00000040U
#define VENDOR_SYS_CAP_NET_RESERVED       0x00000100U

static vendor_sys_dev_info_rsp_t g_dev_info_rsp;
static vendor_sys_caps_rsp_t g_caps_rsp;

static void VendorRouter_QueueStatusRsp(const vp_hdr_t *hdr, vp_status_t status)
{
    (void)USBTX_QueueRsp(hdr, status, 0, 0U);
}

void VendorRouter_Init(void)
{
    memset(&g_dev_info_rsp, 0, sizeof(g_dev_info_rsp));
    g_dev_info_rsp.proto_version = VP_VERSION;
    g_dev_info_rsp.fw_major = 0U;
    g_dev_info_rsp.fw_minor = 1U;
    g_dev_info_rsp.fw_patch = 0U;
    g_dev_info_rsp.uart_port_count = APP_UART_PORT_COUNT;
    g_dev_info_rsp.ble_max_links = APP_BLE_MAX_LINKS;

    memset(&g_caps_rsp, 0, sizeof(g_caps_rsp));
    g_caps_rsp.caps_bitmap = (uint32_t)APP_UART_PORT_COUNT & VENDOR_SYS_CAP_UART_MASK;
    g_caps_rsp.caps_bitmap |= VENDOR_SYS_CAP_BLE_SCAN;
    g_caps_rsp.caps_bitmap |= VENDOR_SYS_CAP_BLE_DISCOVERY;
    g_caps_rsp.caps_bitmap |= VENDOR_SYS_CAP_BLE_NOTIFY_SUB;
    if(APP_CAP_NET_RESERVED != 0U)
    {
        g_caps_rsp.caps_bitmap |= VENDOR_SYS_CAP_NET_RESERVED;
    }
    g_caps_rsp.uart_port_count = APP_UART_PORT_COUNT;
    g_caps_rsp.ble_max_links = APP_BLE_MAX_LINKS;
    g_caps_rsp.net_reserved = APP_CAP_NET_RESERVED;
}

void VendorRouter_Dispatch(const vp_hdr_t *hdr, const uint8_t *payload, uint16_t payload_len)
{
    if(hdr == 0)
    {
        APP_LOG_USB("router hdr null");
        return;
    }

    APP_LOG_USB("router ch=0x%02X id=%u msg=0x%02X op=0x%02X len=%u",
                hdr->ch_type,
                hdr->ch_id,
                hdr->msg_type,
                hdr->opcode,
                payload_len);

    if(hdr->ch_type == VP_CH_UART_CTRL)
    {
        if(hdr->msg_type != VP_MSG_CMD)
        {
            VendorRouter_QueueStatusRsp(hdr, VP_STATUS_ERR_INVALID_PARAM);
            return;
        }

        UartMgr_HandleCtrl(hdr, payload, payload_len);
        return;
    }

    if(hdr->ch_type == VP_CH_UART_DATA)
    {
        if(hdr->msg_type != VP_MSG_DATA)
        {
            VendorRouter_QueueStatusRsp(hdr, VP_STATUS_ERR_INVALID_PARAM);
            return;
        }

        (void)UartMgr_WriteFromHost(hdr->ch_id, payload, payload_len);
        return;
    }

    if(hdr->ch_type == VP_CH_BLE_MGMT)
    {
        if(hdr->msg_type != VP_MSG_CMD)
        {
            VendorRouter_QueueStatusRsp(hdr, VP_STATUS_ERR_INVALID_PARAM);
            return;
        }

        BleHostMgr_HandleMgmt(hdr, payload, payload_len);
        return;
    }

    if((hdr->ch_type != VP_CH_SYS) || (hdr->ch_id != 0U))
    {
        VendorRouter_QueueStatusRsp(hdr, VP_STATUS_ERR_UNSUPPORTED_CH);
        return;
    }

    if(hdr->msg_type != VP_MSG_CMD)
    {
        VendorRouter_QueueStatusRsp(hdr, VP_STATUS_ERR_INVALID_PARAM);
        return;
    }

    switch(hdr->opcode)
    {
        case VP_SYS_OP_GET_DEV_INFO:
            if(payload_len != 0U)
            {
                VendorRouter_QueueStatusRsp(hdr, VP_STATUS_ERR_INVALID_PARAM);
                return;
            }
            (void)USBTX_QueueRsp(hdr, VP_STATUS_OK, &g_dev_info_rsp, sizeof(g_dev_info_rsp));
            break;

        case VP_SYS_OP_GET_CAPS:
            if(payload_len != 0U)
            {
                VendorRouter_QueueStatusRsp(hdr, VP_STATUS_ERR_INVALID_PARAM);
                return;
            }
            (void)USBTX_QueueRsp(hdr, VP_STATUS_OK, &g_caps_rsp, sizeof(g_caps_rsp));
            break;

        case VP_SYS_OP_HEARTBEAT:
            (void)USBTX_QueueRsp(hdr, VP_STATUS_OK, payload, payload_len);
            break;

        default:
            VendorRouter_QueueStatusRsp(hdr, VP_STATUS_ERR_UNSUPPORTED_OPCODE);
            break;
    }
}
