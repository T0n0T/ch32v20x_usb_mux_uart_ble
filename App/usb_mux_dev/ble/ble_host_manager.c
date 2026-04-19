#include "ble_host_manager.h"

#include <string.h>

#include "ble_att_cache.h"
#include "ble_link_fsm.h"
#include "board_caps.h"
#include "config.h"
#include "usb_tx_sched.h"

#define BLE_SLOT_STATE_IDLE        0U
#define BLE_SLOT_STATE_CONNECTING  1U
#define BLE_SLOT_STATE_CONNECTED   2U

#define BLE_SCAN_MODE_ALL          DEVDISC_MODE_ALL
#define BLE_SCAN_ACTIVE_DEFAULT    TRUE
#define BLE_SCAN_WHITE_DEFAULT     FALSE

typedef struct {
    uint16_t conn_handle;
    uint8_t state;
    uint8_t addr_type;
    uint8_t addr[B_ADDR_LEN];
} ble_link_slot_t;

typedef struct {
    uint8_t task_id;
    uint8_t state;
    uint8_t pending_connect_slot;
    uint8_t active_scan;
    uint8_t scan_mode;
    uint8_t scan_active;
    uint8_t scan_white_list;
    uint16_t scan_interval;
    uint16_t scan_window;
    uint16_t scan_duration;
    ble_link_slot_t slots[APP_BLE_MAX_LINKS];
} ble_host_mgr_t;

static ble_host_mgr_t g_ble_mgr;

static void BleHostMgr_WriteLe16(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)(value & 0xFFU);
    dst[1] = (uint8_t)(value >> 8);
}

static uint16_t BleHostMgr_ReadLe16(const uint8_t *src)
{
    return (uint16_t)((uint16_t)src[0] | ((uint16_t)src[1] << 8));
}

static void BleHostMgr_QueueStatusRsp(const vp_hdr_t *hdr, vp_status_t status)
{
    (void)USBTX_QueueRsp(hdr, status, 0, 0U);
}

static vp_status_t BleHostMgr_MapBleStatus(bStatus_t ble_status)
{
    switch(ble_status)
    {
        case SUCCESS:
            return VP_STATUS_OK;

        case INVALIDPARAMETER:
            return VP_STATUS_ERR_INVALID_PARAM;

        case bleNotConnected:
            return VP_STATUS_ERR_BLE_NOT_CONNECTED;

        case blePending:
            return VP_STATUS_ERR_BUSY;

        case MSG_BUFFER_NOT_AVAIL:
        case bleMemAllocError:
            return VP_STATUS_ERR_NO_RESOURCE;

        case bleTimeout:
            return VP_STATUS_ERR_TIMEOUT;

        default:
            return VP_STATUS_ERR_BLE_ATT;
    }
}

static void BleHostMgr_UpdateState(void)
{
    uint8_t idx;
    uint8_t connected = 0U;

    for(idx = 0U; idx < APP_BLE_MAX_LINKS; ++idx)
    {
        if(g_ble_mgr.slots[idx].state == BLE_SLOT_STATE_CONNECTED)
        {
            connected = 1U;
            break;
        }
    }

    if(g_ble_mgr.active_scan != 0U)
    {
        g_ble_mgr.state = connected ? BLE_G_MIXED : BLE_G_SCANNING;
    }
    else if(g_ble_mgr.pending_connect_slot < APP_BLE_MAX_LINKS)
    {
        g_ble_mgr.state = BLE_G_CONNECTING;
    }
    else if(connected != 0U)
    {
        g_ble_mgr.state = BLE_G_MIXED;
    }
    else
    {
        g_ble_mgr.state = BLE_G_READY;
    }
}

static int BleHostMgr_FindSlotByConnHandle(uint16_t conn_handle)
{
    uint8_t idx;

    for(idx = 0U; idx < APP_BLE_MAX_LINKS; ++idx)
    {
        if(g_ble_mgr.slots[idx].conn_handle == conn_handle)
        {
            return (int)idx;
        }
    }

    return -1;
}

static vp_status_t BleHostMgr_ValidateSlotPayload(const uint8_t *payload, uint16_t payload_len, uint8_t *slot)
{
    if((payload == 0) || (payload_len < 1U))
    {
        return VP_STATUS_ERR_INVALID_PARAM;
    }

    *slot = payload[0];
    if(*slot >= APP_BLE_MAX_LINKS)
    {
        return VP_STATUS_ERR_BLE_SLOT_INVALID;
    }

    return VP_STATUS_OK;
}

static vp_status_t BleHostMgr_ValidateConnectedSlot(const uint8_t *payload, uint16_t payload_len, uint8_t *slot)
{
    vp_status_t status = BleHostMgr_ValidateSlotPayload(payload, payload_len, slot);

    if(status != VP_STATUS_OK)
    {
        return status;
    }

    if(g_ble_mgr.slots[*slot].state != BLE_SLOT_STATE_CONNECTED)
    {
        return VP_STATUS_ERR_BLE_NOT_CONNECTED;
    }

    return VP_STATUS_OK;
}

static void BleHostMgr_ReportConnState(uint8_t slot,
                                       uint16_t conn_handle,
                                       uint8_t state,
                                       uint16_t status,
                                       const uint8_t *addr,
                                       uint8_t addr_type)
{
    uint8_t payload[14];

    payload[0] = slot;
    payload[1] = state;
    payload[2] = addr_type;
    payload[3] = BleLink_GetState(slot);
    BleHostMgr_WriteLe16(&payload[4], conn_handle);
    BleHostMgr_WriteLe16(&payload[6], status);
    memcpy(&payload[8], addr, B_ADDR_LEN);

    (void)USBTX_QueueEvt(VP_CH_BLE_CONN, slot, VP_BLE_EVT_CONN_STATE, payload, sizeof(payload));
}

static void BleHostMgr_ReportScanResult(const gapRoleEvent_t *evt)
{
    uint8_t payload[16];
    uint8_t copy_len = 0U;
    const uint8_t *adv_data = 0;
    uint8_t adv_len = 0U;
    uint8_t addr_type;
    const uint8_t *addr;
    int8_t rssi;
    uint8_t event_type;

    if(evt->gap.opcode == GAP_DEVICE_INFO_EVENT)
    {
        addr_type = evt->deviceInfo.addrType;
        addr = evt->deviceInfo.addr;
        rssi = evt->deviceInfo.rssi;
        event_type = evt->deviceInfo.eventType;
        adv_len = evt->deviceInfo.dataLen;
        adv_data = evt->deviceInfo.pEvtData;
    }
    else
    {
        addr_type = evt->deviceExtAdvInfo.addrType;
        addr = evt->deviceExtAdvInfo.addr;
        rssi = evt->deviceExtAdvInfo.rssi;
        event_type = evt->deviceExtAdvInfo.eventType;
        adv_len = evt->deviceExtAdvInfo.dataLen;
        adv_data = evt->deviceExtAdvInfo.pEvtData;
    }

    payload[0] = addr_type;
    payload[1] = event_type;
    payload[2] = (uint8_t)rssi;
    payload[3] = adv_len;
    memcpy(&payload[4], addr, B_ADDR_LEN);

    if((adv_data != 0) && (adv_len > 0U))
    {
        copy_len = adv_len;
        if(copy_len > 6U)
        {
            copy_len = 6U;
        }
        memcpy(&payload[10], adv_data, copy_len);
    }
    if(copy_len < 6U)
    {
        memset(&payload[10 + copy_len], 0, 6U - copy_len);
    }

    (void)USBTX_QueueEvt(VP_CH_BLE_MGMT, 0U, VP_BLE_EVT_SCAN_RSP, payload, sizeof(payload));
}

static void BleHostMgr_ReportRssiResult(uint8_t slot, uint16_t conn_handle, int8_t rssi)
{
    uint8_t payload[4];

    BleHostMgr_WriteLe16(&payload[0], conn_handle);
    payload[2] = (uint8_t)rssi;
    payload[3] = 0U;

    (void)USBTX_QueueEvt(VP_CH_BLE_CONN, slot, VP_BLE_EVT_RSSI_RESULT, payload, sizeof(payload));
}

static void BleHostMgr_ProcessGattMsg(gattMsgEvent_t *msg)
{
    int slot;

    if(msg == 0)
    {
        return;
    }

    slot = BleHostMgr_FindSlotByConnHandle(msg->connHandle);
    if(slot >= 0)
    {
        BleLink_HandleGattMsg((uint8_t)slot, msg);
    }
}

static void BleHostMgr_EventCb(gapRoleEvent_t *evt)
{
    if(evt == 0)
    {
        return;
    }

    switch(evt->gap.opcode)
    {
        case GAP_DEVICE_INIT_DONE_EVENT:
            g_ble_mgr.state = BLE_G_READY;
            break;

        case GAP_DEVICE_INFO_EVENT:
        case GAP_EXT_ADV_DEVICE_INFO_EVENT:
            BleHostMgr_ReportScanResult(evt);
            break;

        case GAP_DEVICE_DISCOVERY_EVENT:
            g_ble_mgr.active_scan = 0U;
            BleHostMgr_UpdateState();
            break;

        case GAP_LINK_ESTABLISHED_EVENT:
        {
            uint8_t slot = g_ble_mgr.pending_connect_slot;

            if((evt->gap.hdr.status == SUCCESS) && (slot < APP_BLE_MAX_LINKS))
            {
                g_ble_mgr.slots[slot].conn_handle = evt->linkCmpl.connectionHandle;
                g_ble_mgr.slots[slot].state = BLE_SLOT_STATE_CONNECTED;
                g_ble_mgr.slots[slot].addr_type = evt->linkCmpl.devAddrType;
                memcpy(g_ble_mgr.slots[slot].addr, evt->linkCmpl.devAddr, B_ADDR_LEN);
                BleLink_Attach(slot, evt->linkCmpl.connectionHandle);
                BleHostMgr_ReportConnState(slot,
                                           evt->linkCmpl.connectionHandle,
                                           BLE_SLOT_STATE_CONNECTED,
                                           evt->gap.hdr.status,
                                           evt->linkCmpl.devAddr,
                                           evt->linkCmpl.devAddrType);
            }
            else if(slot < APP_BLE_MAX_LINKS)
            {
                g_ble_mgr.slots[slot].state = BLE_SLOT_STATE_IDLE;
                BleLink_Reset(slot);
                BleHostMgr_ReportConnState(slot,
                                           GAP_CONNHANDLE_INIT,
                                           BLE_SLOT_STATE_IDLE,
                                           evt->gap.hdr.status,
                                           g_ble_mgr.slots[slot].addr,
                                           g_ble_mgr.slots[slot].addr_type);
            }

            g_ble_mgr.pending_connect_slot = 0xFFU;
            BleHostMgr_UpdateState();
        }
        break;

        case GAP_LINK_TERMINATED_EVENT:
        {
            int slot = BleHostMgr_FindSlotByConnHandle(evt->linkTerminate.connectionHandle);

            if(slot >= 0)
            {
                BleAttCache_ResetSlot((uint8_t)slot);
                BleLink_Reset((uint8_t)slot);
                BleHostMgr_ReportConnState((uint8_t)slot,
                                           evt->linkTerminate.connectionHandle,
                                           BLE_SLOT_STATE_IDLE,
                                           evt->linkTerminate.reason,
                                           g_ble_mgr.slots[slot].addr,
                                           g_ble_mgr.slots[slot].addr_type);
                g_ble_mgr.slots[slot].conn_handle = GAP_CONNHANDLE_INIT;
                g_ble_mgr.slots[slot].state = BLE_SLOT_STATE_IDLE;
                memset(g_ble_mgr.slots[slot].addr, 0, sizeof(g_ble_mgr.slots[slot].addr));
                g_ble_mgr.slots[slot].addr_type = 0U;
                BleHostMgr_UpdateState();
            }
        }
        break;

        default:
            break;
    }
}

static void BleHostMgr_RssiCb(uint16_t conn_handle, int8_t rssi)
{
    int slot = BleHostMgr_FindSlotByConnHandle(conn_handle);

    if(slot >= 0)
    {
        BleHostMgr_ReportRssiResult((uint8_t)slot, conn_handle, rssi);
    }
}

static void BleHostMgr_MtuCb(uint16_t conn_handle, uint16_t max_tx_octets, uint16_t max_rx_octets)
{
    (void)conn_handle;
    (void)max_tx_octets;
    (void)max_rx_octets;
}

static gapCentralRoleCB_t g_ble_role_cb = {
    BleHostMgr_RssiCb,
    BleHostMgr_EventCb,
    BleHostMgr_MtuCb,
};

static tmosEvents BleHostMgr_ProcessEvent(tmosTaskID task_id, tmosEvents events)
{
    if((events & SYS_EVENT_MSG) != 0U)
    {
        uint8_t *msg;

        while((msg = tmos_msg_receive(task_id)) != 0)
        {
            tmos_event_hdr_t *hdr = (tmos_event_hdr_t *)msg;

            if(hdr->event == GATT_MSG_EVENT)
            {
                gattMsgEvent_t *gatt_msg = (gattMsgEvent_t *)msg;

                BleHostMgr_ProcessGattMsg(gatt_msg);
                GATT_bm_free(&gatt_msg->msg, gatt_msg->method);
            }

            (void)tmos_msg_deallocate(msg);
        }

        return events ^ SYS_EVENT_MSG;
    }

    return 0U;
}

static void BleHostMgr_ReplyCap(const vp_hdr_t *hdr)
{
    uint8_t payload[8];

    payload[0] = APP_BLE_MAX_LINKS;
    payload[1] = 1U;
    payload[2] = 1U;
    payload[3] = 1U;
    BleHostMgr_WriteLe16(&payload[4], g_ble_mgr.scan_interval);
    BleHostMgr_WriteLe16(&payload[6], g_ble_mgr.scan_window);

    (void)USBTX_QueueRsp(hdr, VP_STATUS_OK, payload, sizeof(payload));
}

static void BleHostMgr_ReplyConnState(const vp_hdr_t *hdr, uint8_t slot)
{
    uint8_t payload[14];

    payload[0] = slot;
    payload[1] = g_ble_mgr.slots[slot].state;
    payload[2] = g_ble_mgr.slots[slot].addr_type;
    payload[3] = BleLink_GetState(slot);
    BleHostMgr_WriteLe16(&payload[4], g_ble_mgr.slots[slot].conn_handle);
    BleHostMgr_WriteLe16(&payload[6], 0U);
    memcpy(&payload[8], g_ble_mgr.slots[slot].addr, B_ADDR_LEN);

    (void)USBTX_QueueRsp(hdr, VP_STATUS_OK, payload, sizeof(payload));
}

void BleHostMgr_Init(void)
{
    uint8_t slot;

    memset(&g_ble_mgr, 0, sizeof(g_ble_mgr));
    g_ble_mgr.pending_connect_slot = 0xFFU;
    g_ble_mgr.scan_mode = BLE_SCAN_MODE_ALL;
    g_ble_mgr.scan_active = BLE_SCAN_ACTIVE_DEFAULT;
    g_ble_mgr.scan_white_list = BLE_SCAN_WHITE_DEFAULT;
    g_ble_mgr.scan_interval = 16U;
    g_ble_mgr.scan_window = 16U;
    g_ble_mgr.scan_duration = 0U;
    g_ble_mgr.task_id = TMOS_ProcessEventRegister(BleHostMgr_ProcessEvent);

    for(slot = 0U; slot < APP_BLE_MAX_LINKS; ++slot)
    {
        g_ble_mgr.slots[slot].conn_handle = GAP_CONNHANDLE_INIT;
        g_ble_mgr.slots[slot].state = BLE_SLOT_STATE_IDLE;
    }

    BleAttCache_Init();
    BleLink_Init(g_ble_mgr.task_id);
    GAPRole_CentralInit();
    GATT_InitClient();
    GATT_RegisterForInd(g_ble_mgr.task_id);

    GAP_SetParamValue(TGAP_DISC_SCAN_INT, g_ble_mgr.scan_interval);
    GAP_SetParamValue(TGAP_DISC_SCAN_WIND, g_ble_mgr.scan_window);
    GAP_SetParamValue(TGAP_DISC_SCAN, 2400U);
    GAP_SetParamValue(TGAP_CONN_EST_INT_MIN, 20U);
    GAP_SetParamValue(TGAP_CONN_EST_INT_MAX, 100U);
    GAP_SetParamValue(TGAP_CONN_EST_SUPERV_TIMEOUT, 100U);

    (void)GAPRole_CentralStartDevice(g_ble_mgr.task_id, 0, &g_ble_role_cb);
    g_ble_mgr.state = BLE_G_IDLE;
}

void BleHostMgr_HandleMgmt(const vp_hdr_t *hdr, const uint8_t *payload, uint16_t payload_len)
{
    uint8_t slot;
    vp_status_t status;
    bStatus_t ble_status;

    switch(hdr->opcode)
    {
        case VP_BLE_GET_CAP:
            BleHostMgr_ReplyCap(hdr);
            return;

        case VP_BLE_SET_SCAN_PARAM:
            if((payload == 0) || (payload_len < 8U))
            {
                BleHostMgr_QueueStatusRsp(hdr, VP_STATUS_ERR_INVALID_PARAM);
                return;
            }

            g_ble_mgr.scan_interval = BleHostMgr_ReadLe16(&payload[0]);
            g_ble_mgr.scan_window = BleHostMgr_ReadLe16(&payload[2]);
            g_ble_mgr.scan_duration = BleHostMgr_ReadLe16(&payload[4]);
            g_ble_mgr.scan_mode = payload[6];
            g_ble_mgr.scan_active = payload[7];
            g_ble_mgr.scan_white_list = (payload_len >= 9U) ? payload[8] : FALSE;

            GAP_SetParamValue(TGAP_DISC_SCAN_INT, g_ble_mgr.scan_interval);
            GAP_SetParamValue(TGAP_DISC_SCAN_WIND, g_ble_mgr.scan_window);
            GAP_SetParamValue(TGAP_DISC_SCAN, g_ble_mgr.scan_duration);
            BleHostMgr_QueueStatusRsp(hdr, VP_STATUS_OK);
            return;

        case VP_BLE_SCAN_START:
            if(g_ble_mgr.active_scan != 0U)
            {
                BleHostMgr_QueueStatusRsp(hdr, VP_STATUS_ERR_BUSY);
                return;
            }

            ble_status = GAPRole_CentralStartDiscovery(g_ble_mgr.scan_mode, g_ble_mgr.scan_active, g_ble_mgr.scan_white_list);
            if(ble_status != SUCCESS)
            {
                BleHostMgr_QueueStatusRsp(hdr, VP_STATUS_ERR_BUSY);
                return;
            }

            g_ble_mgr.active_scan = 1U;
            BleHostMgr_UpdateState();
            BleHostMgr_QueueStatusRsp(hdr, VP_STATUS_OK);
            return;

        case VP_BLE_SCAN_STOP:
            ble_status = GAPRole_CentralCancelDiscovery();
            if(ble_status != SUCCESS)
            {
                BleHostMgr_QueueStatusRsp(hdr, VP_STATUS_ERR_INVALID_STATE);
                return;
            }

            g_ble_mgr.active_scan = 0U;
            BleHostMgr_UpdateState();
            BleHostMgr_QueueStatusRsp(hdr, VP_STATUS_OK);
            return;

        case VP_BLE_CONNECT:
            if((payload == 0) || (payload_len < 8U))
            {
                BleHostMgr_QueueStatusRsp(hdr, VP_STATUS_ERR_INVALID_PARAM);
                return;
            }

            slot = payload[0];
            if(slot >= APP_BLE_MAX_LINKS)
            {
                BleHostMgr_QueueStatusRsp(hdr, VP_STATUS_ERR_BLE_SLOT_INVALID);
                return;
            }

            if((g_ble_mgr.pending_connect_slot < APP_BLE_MAX_LINKS) || (g_ble_mgr.slots[slot].state != BLE_SLOT_STATE_IDLE))
            {
                BleHostMgr_QueueStatusRsp(hdr, VP_STATUS_ERR_BUSY);
                return;
            }

            BleAttCache_ResetSlot(slot);
            BleLink_Reset(slot);
            g_ble_mgr.pending_connect_slot = slot;
            g_ble_mgr.slots[slot].state = BLE_SLOT_STATE_CONNECTING;
            g_ble_mgr.slots[slot].addr_type = payload[1];
            memcpy(g_ble_mgr.slots[slot].addr, &payload[2], B_ADDR_LEN);
            ble_status = GAPRole_CentralEstablishLink(FALSE, FALSE, payload[1], &g_ble_mgr.slots[slot].addr[0]);
            if(ble_status != SUCCESS)
            {
                g_ble_mgr.pending_connect_slot = 0xFFU;
                g_ble_mgr.slots[slot].state = BLE_SLOT_STATE_IDLE;
                BleHostMgr_QueueStatusRsp(hdr, VP_STATUS_ERR_BUSY);
                return;
            }

            BleHostMgr_UpdateState();
            BleHostMgr_QueueStatusRsp(hdr, VP_STATUS_OK);
            return;

        case VP_BLE_DISCONNECT:
            status = BleHostMgr_ValidateConnectedSlot(payload, payload_len, &slot);
            if(status != VP_STATUS_OK)
            {
                BleHostMgr_QueueStatusRsp(hdr, status);
                return;
            }

            ble_status = GAPRole_TerminateLink(g_ble_mgr.slots[slot].conn_handle);
            BleHostMgr_QueueStatusRsp(hdr, BleHostMgr_MapBleStatus(ble_status));
            return;

        case VP_BLE_GET_CONN_STATE:
            status = BleHostMgr_ValidateSlotPayload(payload, payload_len, &slot);
            if(status != VP_STATUS_OK)
            {
                BleHostMgr_QueueStatusRsp(hdr, status);
                return;
            }

            BleHostMgr_ReplyConnState(hdr, slot);
            return;

        case VP_BLE_DISCOVER_SERVICES:
            status = BleHostMgr_ValidateConnectedSlot(payload, payload_len, &slot);
            if(status == VP_STATUS_OK)
            {
                status = (vp_status_t)BleLink_StartDiscoverServices(slot);
            }
            BleHostMgr_QueueStatusRsp(hdr, status);
            return;

        case VP_BLE_DISCOVER_CHARACTERISTICS:
            status = BleHostMgr_ValidateConnectedSlot(payload, payload_len, &slot);
            if((status == VP_STATUS_OK) && (payload_len >= 5U))
            {
                status = (vp_status_t)BleLink_StartDiscoverChars(slot,
                                                                 BleHostMgr_ReadLe16(&payload[1]),
                                                                 BleHostMgr_ReadLe16(&payload[3]));
            }
            else if(status == VP_STATUS_OK)
            {
                status = VP_STATUS_ERR_INVALID_PARAM;
            }
            BleHostMgr_QueueStatusRsp(hdr, status);
            return;

        case VP_BLE_DISCOVER_DESCRIPTORS:
            status = BleHostMgr_ValidateConnectedSlot(payload, payload_len, &slot);
            if((status == VP_STATUS_OK) && (payload_len >= 5U))
            {
                status = (vp_status_t)BleLink_StartDiscoverDescs(slot,
                                                                 BleHostMgr_ReadLe16(&payload[1]),
                                                                 BleHostMgr_ReadLe16(&payload[3]));
            }
            else if(status == VP_STATUS_OK)
            {
                status = VP_STATUS_ERR_INVALID_PARAM;
            }
            BleHostMgr_QueueStatusRsp(hdr, status);
            return;

        case VP_BLE_READ:
            status = BleHostMgr_ValidateConnectedSlot(payload, payload_len, &slot);
            if((status == VP_STATUS_OK) && (payload_len >= 3U))
            {
                status = (vp_status_t)BleLink_Read(slot, BleHostMgr_ReadLe16(&payload[1]));
            }
            else if(status == VP_STATUS_OK)
            {
                status = VP_STATUS_ERR_INVALID_PARAM;
            }
            BleHostMgr_QueueStatusRsp(hdr, status);
            return;

        case VP_BLE_WRITE_REQ:
            status = BleHostMgr_ValidateConnectedSlot(payload, payload_len, &slot);
            if((status == VP_STATUS_OK) && (payload_len >= 3U))
            {
                status = (vp_status_t)BleLink_WriteReq(slot,
                                                       BleHostMgr_ReadLe16(&payload[1]),
                                                       &payload[3],
                                                       (uint16_t)(payload_len - 3U));
            }
            else if(status == VP_STATUS_OK)
            {
                status = VP_STATUS_ERR_INVALID_PARAM;
            }
            BleHostMgr_QueueStatusRsp(hdr, status);
            return;

        case VP_BLE_WRITE_CMD:
            status = BleHostMgr_ValidateConnectedSlot(payload, payload_len, &slot);
            if((status == VP_STATUS_OK) && (payload_len >= 3U))
            {
                status = (vp_status_t)BleLink_WriteCmd(slot,
                                                       BleHostMgr_ReadLe16(&payload[1]),
                                                       &payload[3],
                                                       (uint16_t)(payload_len - 3U));
            }
            else if(status == VP_STATUS_OK)
            {
                status = VP_STATUS_ERR_INVALID_PARAM;
            }
            BleHostMgr_QueueStatusRsp(hdr, status);
            return;

        case VP_BLE_SUBSCRIBE:
            status = BleHostMgr_ValidateConnectedSlot(payload, payload_len, &slot);
            if((status == VP_STATUS_OK) && (payload_len >= 3U))
            {
                status = (vp_status_t)BleLink_Subscribe(slot, BleHostMgr_ReadLe16(&payload[1]));
            }
            else if(status == VP_STATUS_OK)
            {
                status = VP_STATUS_ERR_INVALID_PARAM;
            }
            BleHostMgr_QueueStatusRsp(hdr, status);
            return;

        case VP_BLE_UNSUBSCRIBE:
            status = BleHostMgr_ValidateConnectedSlot(payload, payload_len, &slot);
            if((status == VP_STATUS_OK) && (payload_len >= 3U))
            {
                status = (vp_status_t)BleLink_Unsubscribe(slot, BleHostMgr_ReadLe16(&payload[1]));
            }
            else if(status == VP_STATUS_OK)
            {
                status = VP_STATUS_ERR_INVALID_PARAM;
            }
            BleHostMgr_QueueStatusRsp(hdr, status);
            return;

        case VP_BLE_EXCHANGE_MTU:
            status = BleHostMgr_ValidateConnectedSlot(payload, payload_len, &slot);
            if((status == VP_STATUS_OK) && (payload_len >= 3U))
            {
                status = (vp_status_t)BleLink_ExchangeMtu(slot, BleHostMgr_ReadLe16(&payload[1]));
            }
            else if(status == VP_STATUS_OK)
            {
                status = VP_STATUS_ERR_INVALID_PARAM;
            }
            BleHostMgr_QueueStatusRsp(hdr, status);
            return;

        case VP_BLE_READ_RSSI:
            status = BleHostMgr_ValidateConnectedSlot(payload, payload_len, &slot);
            if(status != VP_STATUS_OK)
            {
                BleHostMgr_QueueStatusRsp(hdr, status);
                return;
            }

            ble_status = GAPRole_ReadRssiCmd(g_ble_mgr.slots[slot].conn_handle);
            BleHostMgr_QueueStatusRsp(hdr, BleHostMgr_MapBleStatus(ble_status));
            return;

        case VP_BLE_UPDATE_CONN_PARAM:
            status = BleHostMgr_ValidateConnectedSlot(payload, payload_len, &slot);
            if((status == VP_STATUS_OK) && (payload_len >= 9U))
            {
                ble_status = GAPRole_UpdateLink(g_ble_mgr.slots[slot].conn_handle,
                                                BleHostMgr_ReadLe16(&payload[1]),
                                                BleHostMgr_ReadLe16(&payload[3]),
                                                BleHostMgr_ReadLe16(&payload[5]),
                                                BleHostMgr_ReadLe16(&payload[7]));
                status = BleHostMgr_MapBleStatus(ble_status);
            }
            else if(status == VP_STATUS_OK)
            {
                status = VP_STATUS_ERR_INVALID_PARAM;
            }
            BleHostMgr_QueueStatusRsp(hdr, status);
            return;

        default:
            BleHostMgr_QueueStatusRsp(hdr, VP_STATUS_ERR_UNSUPPORTED_OPCODE);
            return;
    }
}
