#include "ble_link_fsm.h"

#include <string.h>

#include "board_caps.h"
#include "vendor_proto.h"
#include "usb_tx_sched.h"

#define BLE_LINK_EVT_HDR_SIZE  6U
#define BLE_LINK_UUID_MAX_LEN  16U

typedef enum {
    BLE_LINK_PROC_NONE = 0,
    BLE_LINK_PROC_DISC_SERVICE,
    BLE_LINK_PROC_DISC_CHAR,
    BLE_LINK_PROC_DISC_DESC,
    BLE_LINK_PROC_READ,
    BLE_LINK_PROC_WRITE_REQ,
    BLE_LINK_PROC_SUBSCRIBE,
    BLE_LINK_PROC_UNSUBSCRIBE,
    BLE_LINK_PROC_EXCHANGE_MTU,
} ble_link_proc_t;

typedef struct {
    uint8_t slot_id;
    uint8_t state;
    uint16_t conn_handle;
    uint16_t mtu;
    uint8_t proc_busy;
    uint8_t proc;
    uint16_t proc_handle;
    uint16_t range_start;
    uint16_t range_end;
} ble_link_ctx_t;

static ble_link_ctx_t g_ble_link_ctx[APP_BLE_MAX_LINKS];
static uint8_t g_ble_link_task_id = INVALID_TASK_ID;

static void BleLink_WriteLe16(uint8_t *dst, uint16_t value)
{
    dst[0] = LO_UINT16(value);
    dst[1] = HI_UINT16(value);
}

static ble_link_ctx_t *BleLink_GetCtx(uint8_t slot)
{
    if(slot >= APP_BLE_MAX_LINKS)
    {
        return 0;
    }

    return &g_ble_link_ctx[slot];
}

static uint8_t BleLink_MapOpToState(uint8_t proc)
{
    switch(proc)
    {
        case BLE_LINK_PROC_DISC_SERVICE:
            return BLE_L_DISC_SERVICE;

        case BLE_LINK_PROC_DISC_CHAR:
            return BLE_L_DISC_CHAR;

        case BLE_LINK_PROC_DISC_DESC:
            return BLE_L_DISC_DESC;

        case BLE_LINK_PROC_READ:
            return BLE_L_READING;

        case BLE_LINK_PROC_WRITE_REQ:
            return BLE_L_WRITING_REQ;

        case BLE_LINK_PROC_SUBSCRIBE:
            return BLE_L_SUBSCRIBING;

        case BLE_LINK_PROC_UNSUBSCRIBE:
            return BLE_L_UNSUBSCRIBING;

        case BLE_LINK_PROC_EXCHANGE_MTU:
            return BLE_L_MTU_EXCHANGING;

        default:
            return BLE_L_CONNECTED;
    }
}

static uint8_t BleLink_MapProcToOpcode(uint8_t proc)
{
    switch(proc)
    {
        case BLE_LINK_PROC_WRITE_REQ:
            return VP_BLE_WRITE_REQ;

        case BLE_LINK_PROC_SUBSCRIBE:
            return VP_BLE_SUBSCRIBE;

        case BLE_LINK_PROC_UNSUBSCRIBE:
            return VP_BLE_UNSUBSCRIBE;

        default:
            return 0U;
    }
}

static int BleLink_RequireReadyCtx(ble_link_ctx_t *ctx)
{
    if((ctx == 0) || (ctx->conn_handle == GAP_CONNHANDLE_INIT))
    {
        return VP_STATUS_ERR_BLE_NOT_CONNECTED;
    }

    return VP_STATUS_OK;
}

static int BleLink_RequireIdleProc(ble_link_ctx_t *ctx)
{
    if(ctx->proc_busy != 0U)
    {
        return VP_STATUS_ERR_BUSY;
    }

    ctx->proc_busy = 1U;
    return VP_STATUS_OK;
}

static int BleLink_MapBleStatus(bStatus_t status)
{
    switch(status)
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

static void BleLink_BeginProc(ble_link_ctx_t *ctx,
                              uint8_t proc,
                              uint16_t proc_handle,
                              uint16_t range_start,
                              uint16_t range_end)
{
    ctx->proc = proc;
    ctx->proc_handle = proc_handle;
    ctx->range_start = range_start;
    ctx->range_end = range_end;
    ctx->state = BleLink_MapOpToState(proc);
}

static void BleLink_EndProc(ble_link_ctx_t *ctx, uint8_t next_state)
{
    ctx->proc_busy = 0U;
    ctx->proc = BLE_LINK_PROC_NONE;
    ctx->proc_handle = 0U;
    ctx->range_start = 0U;
    ctx->range_end = 0U;
    ctx->state = next_state;
}

static uint16_t BleLink_CopyUuid(uint8_t *dst, const uint8_t *src, uint16_t len)
{
    uint16_t copy_len = len;

    if(copy_len > BLE_LINK_UUID_MAX_LEN)
    {
        copy_len = BLE_LINK_UUID_MAX_LEN;
    }

    memset(dst, 0, BLE_LINK_UUID_MAX_LEN);
    if((src != 0) && (copy_len > 0U))
    {
        memcpy(dst, src, copy_len);
    }

    return copy_len;
}

static void BleLink_ReportServiceFound(uint8_t slot,
                                       uint16_t start_handle,
                                       uint16_t end_handle,
                                       uint16_t status,
                                       const uint8_t *uuid,
                                       uint16_t uuid_len)
{
    uint8_t payload[24];

    BleLink_WriteLe16(&payload[0], start_handle);
    BleLink_WriteLe16(&payload[2], end_handle);
    BleLink_WriteLe16(&payload[4], status);
    payload[6] = (uint8_t)BleLink_CopyUuid(&payload[8], uuid, uuid_len);
    payload[7] = 0U;

    (void)USBTX_QueueEvt(VP_CH_BLE_CONN, slot, VP_BLE_EVT_SERVICE_FOUND, payload, sizeof(payload));
}

static void BleLink_ReportCharFound(uint8_t slot,
                                    uint16_t decl_handle,
                                    uint16_t value_handle,
                                    uint16_t status,
                                    uint8_t properties,
                                    const uint8_t *uuid,
                                    uint16_t uuid_len)
{
    uint8_t payload[24];

    BleLink_WriteLe16(&payload[0], decl_handle);
    BleLink_WriteLe16(&payload[2], value_handle);
    BleLink_WriteLe16(&payload[4], status);
    payload[6] = properties;
    payload[7] = (uint8_t)BleLink_CopyUuid(&payload[8], uuid, uuid_len);

    (void)USBTX_QueueEvt(VP_CH_BLE_CONN, slot, VP_BLE_EVT_CHAR_FOUND, payload, sizeof(payload));
}

static void BleLink_ReportDescFound(uint8_t slot, uint16_t handle, uint16_t status, const uint8_t *uuid, uint16_t uuid_len)
{
    uint8_t payload[22];

    BleLink_WriteLe16(&payload[0], handle);
    BleLink_WriteLe16(&payload[2], status);
    payload[4] = (uint8_t)BleLink_CopyUuid(&payload[6], uuid, uuid_len);
    payload[5] = 0U;

    (void)USBTX_QueueEvt(VP_CH_BLE_CONN, slot, VP_BLE_EVT_DESC_FOUND, payload, sizeof(payload));
}

static void BleLink_ReportReadResult(uint8_t slot, uint16_t handle, uint16_t status, const uint8_t *buf, uint16_t len)
{
    uint8_t payload[APP_USB_MAX_FRAME_LEN - VP_HEADER_LEN];
    uint16_t copy_len = len;

    if(copy_len > (uint16_t)(sizeof(payload) - BLE_LINK_EVT_HDR_SIZE))
    {
        copy_len = (uint16_t)(sizeof(payload) - BLE_LINK_EVT_HDR_SIZE);
    }

    BleLink_WriteLe16(&payload[0], handle);
    BleLink_WriteLe16(&payload[2], status);
    BleLink_WriteLe16(&payload[4], copy_len);
    if((buf != 0) && (copy_len > 0U))
    {
        memcpy(&payload[6], buf, copy_len);
    }

    (void)USBTX_QueueEvt(VP_CH_BLE_CONN, slot, VP_BLE_EVT_READ_RESULT, payload, (uint16_t)(BLE_LINK_EVT_HDR_SIZE + copy_len));
}

static void BleLink_ReportWriteResult(uint8_t slot, uint16_t handle, uint16_t status, uint8_t op)
{
    uint8_t payload[6];

    BleLink_WriteLe16(&payload[0], handle);
    BleLink_WriteLe16(&payload[2], status);
    payload[4] = op;
    payload[5] = 0U;

    (void)USBTX_QueueEvt(VP_CH_BLE_CONN, slot, VP_BLE_EVT_WRITE_RESULT, payload, sizeof(payload));
}

static void BleLink_ReportValueEvent(uint8_t slot, uint8_t opcode, uint16_t value_handle, const uint8_t *buf, uint16_t len)
{
    uint8_t payload[APP_USB_MAX_FRAME_LEN - VP_HEADER_LEN];
    uint16_t copy_len = len;

    if(copy_len > (uint16_t)(sizeof(payload) - 4U))
    {
        copy_len = (uint16_t)(sizeof(payload) - 4U);
    }

    BleLink_WriteLe16(&payload[0], value_handle);
    BleLink_WriteLe16(&payload[2], copy_len);
    if((buf != 0) && (copy_len > 0U))
    {
        memcpy(&payload[4], buf, copy_len);
    }

    (void)USBTX_QueueEvt(VP_CH_BLE_CONN, slot, opcode, payload, (uint16_t)(4U + copy_len));
}

static bStatus_t BleLink_AllocWriteReq(uint16_t conn_handle,
                                       uint16_t opcode,
                                       uint16_t attr_handle,
                                       const uint8_t *buf,
                                       uint16_t len,
                                       uint8_t cmd,
                                       attWriteReq_t *req)
{
    memset(req, 0, sizeof(*req));
    req->handle = attr_handle;
    req->len = len;
    req->cmd = cmd;
    req->sig = FALSE;
    req->pValue = GATT_bm_alloc(conn_handle, opcode, len, 0, 0U);
    if((req->pValue == 0) && (len > 0U))
    {
        return bleMemAllocError;
    }

    if((buf != 0) && (len > 0U))
    {
        memcpy(req->pValue, buf, len);
    }

    return SUCCESS;
}

static void BleLink_HandleDiscoverServices(ble_link_ctx_t *ctx, gattMsgEvent_t *msg)
{
    if((msg->method == ATT_READ_BY_GRP_TYPE_RSP) &&
       (msg->msg.readByGrpTypeRsp.numGrps > 0U) &&
       (msg->msg.readByGrpTypeRsp.len >= 4U))
    {
        uint8_t *data = msg->msg.readByGrpTypeRsp.pDataList;
        uint16_t pair_len = msg->msg.readByGrpTypeRsp.len;
        uint16_t uuid_len = (pair_len > 4U) ? (uint16_t)(pair_len - 4U) : 0U;
        uint16_t idx;

        for(idx = 0U; idx < msg->msg.readByGrpTypeRsp.numGrps; ++idx)
        {
            BleLink_ReportServiceFound(ctx->slot_id,
                                       BUILD_UINT16(data[0], data[1]),
                                       BUILD_UINT16(data[2], data[3]),
                                       msg->hdr.status,
                                       &data[4],
                                       uuid_len);
            data += pair_len;
        }

        if((msg->hdr.status == bleProcedureComplete) || (msg->hdr.status == bleTimeout))
        {
            BleLink_EndProc(ctx, BLE_L_READY);
        }
        return;
    }

    if((msg->method == ATT_ERROR_RSP) && (msg->msg.errorRsp.reqOpcode == ATT_READ_BY_GRP_TYPE_REQ))
    {
        BleLink_ReportServiceFound(ctx->slot_id, 0U, 0U, msg->msg.errorRsp.errCode, 0, 0U);
        BleLink_EndProc(ctx, BLE_L_READY);
    }
}

static void BleLink_HandleDiscoverChars(ble_link_ctx_t *ctx, gattMsgEvent_t *msg)
{
    if((msg->method == ATT_READ_BY_TYPE_RSP) &&
       (msg->msg.readByTypeRsp.numPairs > 0U) &&
       (msg->msg.readByTypeRsp.len >= 5U))
    {
        uint8_t *data = msg->msg.readByTypeRsp.pDataList;
        uint16_t pair_len = msg->msg.readByTypeRsp.len;
        uint16_t uuid_len = (pair_len > 5U) ? (uint16_t)(pair_len - 5U) : 0U;
        uint16_t last_value_handle = 0U;
        uint16_t idx;

        for(idx = 0U; idx < msg->msg.readByTypeRsp.numPairs; ++idx)
        {
            uint16_t decl_handle = BUILD_UINT16(data[0], data[1]);
            uint16_t value_handle = BUILD_UINT16(data[3], data[4]);

            last_value_handle = value_handle;
            BleLink_ReportCharFound(ctx->slot_id,
                                    decl_handle,
                                    value_handle,
                                    msg->hdr.status,
                                    data[2],
                                    &data[5],
                                    uuid_len);
            data += pair_len;
        }

        if((msg->hdr.status == SUCCESS) && (last_value_handle != 0U) && (last_value_handle < ctx->range_end))
        {
            if(GATT_DiscAllChars(ctx->conn_handle,
                                 (uint16_t)(last_value_handle + 1U),
                                 ctx->range_end,
                                 g_ble_link_task_id) != SUCCESS)
            {
                BleLink_EndProc(ctx, BLE_L_READY);
            }
            return;
        }

        if((msg->hdr.status == bleProcedureComplete) || (msg->hdr.status == bleTimeout) || (last_value_handle >= ctx->range_end))
        {
            BleLink_EndProc(ctx, BLE_L_READY);
        }
        return;
    }

    if((msg->method == ATT_ERROR_RSP) && (msg->msg.errorRsp.reqOpcode == ATT_READ_BY_TYPE_REQ))
    {
        BleLink_ReportCharFound(ctx->slot_id, 0U, 0U, msg->msg.errorRsp.errCode, 0U, 0, 0U);
        BleLink_EndProc(ctx, BLE_L_READY);
    }
}

static void BleLink_HandleDiscoverDescs(ble_link_ctx_t *ctx, gattMsgEvent_t *msg)
{
    if((msg->method == ATT_FIND_INFO_RSP) &&
       (msg->msg.findInfoRsp.numInfo > 0U))
    {
        uint8_t *data = msg->msg.findInfoRsp.pInfo;
        uint16_t pair_len;
        uint16_t last_handle = 0U;
        uint16_t idx;

        if(msg->msg.findInfoRsp.format == 0x01U)
        {
            pair_len = 4U;
        }
        else if(msg->msg.findInfoRsp.format == 0x02U)
        {
            pair_len = 18U;
        }
        else
        {
            BleLink_EndProc(ctx, BLE_L_READY);
            return;
        }

        for(idx = 0U; idx < msg->msg.findInfoRsp.numInfo; ++idx)
        {
            last_handle = BUILD_UINT16(data[0], data[1]);
            BleLink_ReportDescFound(ctx->slot_id,
                                    last_handle,
                                    msg->hdr.status,
                                    &data[2],
                                    (uint16_t)(pair_len - 2U));
            data += pair_len;
        }

        if((msg->hdr.status == SUCCESS) && (last_handle != 0U) && (last_handle < ctx->range_end))
        {
            if(GATT_DiscAllCharDescs(ctx->conn_handle,
                                     (uint16_t)(last_handle + 1U),
                                     ctx->range_end,
                                     g_ble_link_task_id) != SUCCESS)
            {
                BleLink_EndProc(ctx, BLE_L_READY);
            }
            return;
        }

        if((msg->hdr.status == bleProcedureComplete) || (msg->hdr.status == bleTimeout) || (last_handle >= ctx->range_end))
        {
            BleLink_EndProc(ctx, BLE_L_READY);
        }
        return;
    }

    if((msg->method == ATT_ERROR_RSP) && (msg->msg.errorRsp.reqOpcode == ATT_FIND_INFO_REQ))
    {
        BleLink_ReportDescFound(ctx->slot_id, 0U, msg->msg.errorRsp.errCode, 0, 0U);
        BleLink_EndProc(ctx, BLE_L_READY);
    }
}

static void BleLink_HandleReadProc(ble_link_ctx_t *ctx, gattMsgEvent_t *msg)
{
    if(msg->method == ATT_READ_RSP)
    {
        BleLink_ReportReadResult(ctx->slot_id,
                                 ctx->proc_handle,
                                 msg->hdr.status,
                                 msg->msg.readRsp.pValue,
                                 msg->msg.readRsp.len);
        BleLink_EndProc(ctx, BLE_L_READY);
        return;
    }

    if((msg->method == ATT_ERROR_RSP) && (msg->msg.errorRsp.reqOpcode == ATT_READ_REQ))
    {
        BleLink_ReportReadResult(ctx->slot_id, msg->msg.errorRsp.handle, msg->msg.errorRsp.errCode, 0, 0U);
        BleLink_EndProc(ctx, BLE_L_READY);
    }
}

static void BleLink_HandleWriteProc(ble_link_ctx_t *ctx, gattMsgEvent_t *msg)
{
    if(msg->method == ATT_WRITE_RSP)
    {
        BleLink_ReportWriteResult(ctx->slot_id,
                                  ctx->proc_handle,
                                  msg->hdr.status,
                                  BleLink_MapProcToOpcode(ctx->proc));
        BleLink_EndProc(ctx, BLE_L_READY);
        return;
    }

    if((msg->method == ATT_ERROR_RSP) && (msg->msg.errorRsp.reqOpcode == ATT_WRITE_REQ))
    {
        BleLink_ReportWriteResult(ctx->slot_id,
                                  msg->msg.errorRsp.handle,
                                  msg->msg.errorRsp.errCode,
                                  BleLink_MapProcToOpcode(ctx->proc));
        BleLink_EndProc(ctx, BLE_L_READY);
    }
}

static void BleLink_HandleMtuProc(ble_link_ctx_t *ctx, gattMsgEvent_t *msg)
{
    if(msg->method == ATT_EXCHANGE_MTU_RSP)
    {
        ctx->mtu = msg->msg.exchangeMTURsp.serverRxMTU;
        BleLink_EndProc(ctx, BLE_L_READY);
        return;
    }

    if((msg->method == ATT_ERROR_RSP) && (msg->msg.errorRsp.reqOpcode == ATT_EXCHANGE_MTU_REQ))
    {
        BleLink_EndProc(ctx, BLE_L_READY);
    }
}

void BleLink_Init(uint8_t task_id)
{
    uint8_t slot;

    memset(g_ble_link_ctx, 0, sizeof(g_ble_link_ctx));
    g_ble_link_task_id = task_id;

    for(slot = 0U; slot < APP_BLE_MAX_LINKS; ++slot)
    {
        g_ble_link_ctx[slot].slot_id = slot;
        g_ble_link_ctx[slot].conn_handle = GAP_CONNHANDLE_INIT;
        g_ble_link_ctx[slot].mtu = ATT_MTU_SIZE;
        g_ble_link_ctx[slot].state = BLE_L_IDLE;
    }
}

void BleLink_Attach(uint8_t slot, uint16_t conn_handle)
{
    ble_link_ctx_t *ctx = BleLink_GetCtx(slot);

    if(ctx == 0)
    {
        return;
    }

    ctx->conn_handle = conn_handle;
    ctx->mtu = ATT_GetMTU(conn_handle);
    if(ctx->mtu == 0U)
    {
        ctx->mtu = ATT_MTU_SIZE;
    }
    ctx->proc_busy = 0U;
    ctx->proc = BLE_LINK_PROC_NONE;
    ctx->proc_handle = 0U;
    ctx->range_start = 0U;
    ctx->range_end = 0U;
    ctx->state = BLE_L_CONNECTED;
}

void BleLink_Reset(uint8_t slot)
{
    ble_link_ctx_t *ctx = BleLink_GetCtx(slot);

    if(ctx == 0)
    {
        return;
    }

    ctx->conn_handle = GAP_CONNHANDLE_INIT;
    ctx->mtu = ATT_MTU_SIZE;
    ctx->proc_busy = 0U;
    ctx->proc = BLE_LINK_PROC_NONE;
    ctx->proc_handle = 0U;
    ctx->range_start = 0U;
    ctx->range_end = 0U;
    ctx->state = BLE_L_IDLE;
}

uint8_t BleLink_GetState(uint8_t slot)
{
    ble_link_ctx_t *ctx = BleLink_GetCtx(slot);

    return (ctx != 0) ? ctx->state : BLE_L_IDLE;
}

uint16_t BleLink_GetMtu(uint8_t slot)
{
    ble_link_ctx_t *ctx = BleLink_GetCtx(slot);

    return (ctx != 0) ? ctx->mtu : ATT_MTU_SIZE;
}

int BleLink_StartDiscoverServices(uint8_t slot)
{
    ble_link_ctx_t *ctx = BleLink_GetCtx(slot);
    bStatus_t ble_status;
    int status;

    status = BleLink_RequireReadyCtx(ctx);
    if(status != VP_STATUS_OK)
    {
        return status;
    }

    status = BleLink_RequireIdleProc(ctx);
    if(status != VP_STATUS_OK)
    {
        return status;
    }

    ble_status = GATT_DiscAllPrimaryServices(ctx->conn_handle, g_ble_link_task_id);
    if(ble_status != SUCCESS)
    {
        ctx->proc_busy = 0U;
        return BleLink_MapBleStatus(ble_status);
    }

    BleLink_BeginProc(ctx, BLE_LINK_PROC_DISC_SERVICE, 0U, 0U, 0U);
    return VP_STATUS_OK;
}

int BleLink_StartDiscoverChars(uint8_t slot, uint16_t start_hdl, uint16_t end_hdl)
{
    ble_link_ctx_t *ctx = BleLink_GetCtx(slot);
    bStatus_t ble_status;
    int status;

    status = BleLink_RequireReadyCtx(ctx);
    if(status != VP_STATUS_OK)
    {
        return status;
    }

    if((start_hdl == 0U) || (end_hdl < start_hdl))
    {
        return VP_STATUS_ERR_INVALID_PARAM;
    }

    status = BleLink_RequireIdleProc(ctx);
    if(status != VP_STATUS_OK)
    {
        return status;
    }

    ble_status = GATT_DiscAllChars(ctx->conn_handle, start_hdl, end_hdl, g_ble_link_task_id);
    if(ble_status != SUCCESS)
    {
        ctx->proc_busy = 0U;
        return BleLink_MapBleStatus(ble_status);
    }

    BleLink_BeginProc(ctx, BLE_LINK_PROC_DISC_CHAR, 0U, start_hdl, end_hdl);
    return VP_STATUS_OK;
}

int BleLink_StartDiscoverDescs(uint8_t slot, uint16_t start_hdl, uint16_t end_hdl)
{
    ble_link_ctx_t *ctx = BleLink_GetCtx(slot);
    bStatus_t ble_status;
    int status;

    status = BleLink_RequireReadyCtx(ctx);
    if(status != VP_STATUS_OK)
    {
        return status;
    }

    if((start_hdl == 0U) || (end_hdl < start_hdl))
    {
        return VP_STATUS_ERR_INVALID_PARAM;
    }

    status = BleLink_RequireIdleProc(ctx);
    if(status != VP_STATUS_OK)
    {
        return status;
    }

    ble_status = GATT_DiscAllCharDescs(ctx->conn_handle, start_hdl, end_hdl, g_ble_link_task_id);
    if(ble_status != SUCCESS)
    {
        ctx->proc_busy = 0U;
        return BleLink_MapBleStatus(ble_status);
    }

    BleLink_BeginProc(ctx, BLE_LINK_PROC_DISC_DESC, 0U, start_hdl, end_hdl);
    return VP_STATUS_OK;
}

int BleLink_Read(uint8_t slot, uint16_t attr_handle)
{
    ble_link_ctx_t *ctx = BleLink_GetCtx(slot);
    attReadReq_t req;
    bStatus_t ble_status;
    int status;

    status = BleLink_RequireReadyCtx(ctx);
    if(status != VP_STATUS_OK)
    {
        return status;
    }

    if(attr_handle == 0U)
    {
        return VP_STATUS_ERR_INVALID_PARAM;
    }

    status = BleLink_RequireIdleProc(ctx);
    if(status != VP_STATUS_OK)
    {
        return status;
    }

    req.handle = attr_handle;
    ble_status = GATT_ReadCharValue(ctx->conn_handle, &req, g_ble_link_task_id);
    if(ble_status != SUCCESS)
    {
        ctx->proc_busy = 0U;
        return BleLink_MapBleStatus(ble_status);
    }

    BleLink_BeginProc(ctx, BLE_LINK_PROC_READ, attr_handle, 0U, 0U);
    return VP_STATUS_OK;
}

int BleLink_WriteReq(uint8_t slot, uint16_t attr_handle, const uint8_t *buf, uint16_t len)
{
    ble_link_ctx_t *ctx = BleLink_GetCtx(slot);
    attWriteReq_t req;
    bStatus_t ble_status;
    int status;

    status = BleLink_RequireReadyCtx(ctx);
    if(status != VP_STATUS_OK)
    {
        return status;
    }

    if(attr_handle == 0U)
    {
        return VP_STATUS_ERR_INVALID_PARAM;
    }

    status = BleLink_RequireIdleProc(ctx);
    if(status != VP_STATUS_OK)
    {
        return status;
    }

    ble_status = BleLink_AllocWriteReq(ctx->conn_handle, ATT_WRITE_REQ, attr_handle, buf, len, FALSE, &req);
    if(ble_status != SUCCESS)
    {
        ctx->proc_busy = 0U;
        return BleLink_MapBleStatus(ble_status);
    }

    ble_status = GATT_WriteCharValue(ctx->conn_handle, &req, g_ble_link_task_id);
    if(ble_status != SUCCESS)
    {
        ctx->proc_busy = 0U;
        GATT_bm_free((gattMsg_t *)&req, ATT_WRITE_REQ);
        return BleLink_MapBleStatus(ble_status);
    }

    BleLink_BeginProc(ctx, BLE_LINK_PROC_WRITE_REQ, attr_handle, 0U, 0U);
    return VP_STATUS_OK;
}

int BleLink_WriteCmd(uint8_t slot, uint16_t attr_handle, const uint8_t *buf, uint16_t len)
{
    ble_link_ctx_t *ctx = BleLink_GetCtx(slot);
    attWriteReq_t req;
    bStatus_t ble_status;
    int status;

    status = BleLink_RequireReadyCtx(ctx);
    if(status != VP_STATUS_OK)
    {
        return status;
    }

    if(attr_handle == 0U)
    {
        return VP_STATUS_ERR_INVALID_PARAM;
    }

    if(ctx->proc_busy != 0U)
    {
        return VP_STATUS_ERR_BUSY;
    }

    ble_status = BleLink_AllocWriteReq(ctx->conn_handle, ATT_WRITE_REQ, attr_handle, buf, len, TRUE, &req);
    if(ble_status != SUCCESS)
    {
        return BleLink_MapBleStatus(ble_status);
    }

    ble_status = GATT_WriteNoRsp(ctx->conn_handle, &req);
    if(ble_status != SUCCESS)
    {
        GATT_bm_free((gattMsg_t *)&req, ATT_WRITE_REQ);
        return BleLink_MapBleStatus(ble_status);
    }

    BleLink_ReportWriteResult(slot, attr_handle, SUCCESS, VP_BLE_WRITE_CMD);
    return VP_STATUS_OK;
}

int BleLink_Subscribe(uint8_t slot, uint16_t cccd_handle)
{
    static const uint8_t cfg_notify[2] = { LO_UINT16(GATT_CLIENT_CFG_NOTIFY), HI_UINT16(GATT_CLIENT_CFG_NOTIFY) };

    ble_link_ctx_t *ctx = BleLink_GetCtx(slot);
    attWriteReq_t req;
    bStatus_t ble_status;
    int status;

    status = BleLink_RequireReadyCtx(ctx);
    if(status != VP_STATUS_OK)
    {
        return status;
    }

    if(cccd_handle == 0U)
    {
        return VP_STATUS_ERR_INVALID_PARAM;
    }

    status = BleLink_RequireIdleProc(ctx);
    if(status != VP_STATUS_OK)
    {
        return status;
    }

    ble_status = BleLink_AllocWriteReq(ctx->conn_handle, ATT_WRITE_REQ, cccd_handle, cfg_notify, sizeof(cfg_notify), FALSE, &req);
    if(ble_status != SUCCESS)
    {
        ctx->proc_busy = 0U;
        return BleLink_MapBleStatus(ble_status);
    }

    ble_status = GATT_WriteCharDesc(ctx->conn_handle, &req, g_ble_link_task_id);
    if(ble_status != SUCCESS)
    {
        ctx->proc_busy = 0U;
        GATT_bm_free((gattMsg_t *)&req, ATT_WRITE_REQ);
        return BleLink_MapBleStatus(ble_status);
    }

    BleLink_BeginProc(ctx, BLE_LINK_PROC_SUBSCRIBE, cccd_handle, 0U, 0U);
    return VP_STATUS_OK;
}

int BleLink_Unsubscribe(uint8_t slot, uint16_t cccd_handle)
{
    static const uint8_t cfg_off[2] = { 0U, 0U };

    ble_link_ctx_t *ctx = BleLink_GetCtx(slot);
    attWriteReq_t req;
    bStatus_t ble_status;
    int status;

    status = BleLink_RequireReadyCtx(ctx);
    if(status != VP_STATUS_OK)
    {
        return status;
    }

    if(cccd_handle == 0U)
    {
        return VP_STATUS_ERR_INVALID_PARAM;
    }

    status = BleLink_RequireIdleProc(ctx);
    if(status != VP_STATUS_OK)
    {
        return status;
    }

    ble_status = BleLink_AllocWriteReq(ctx->conn_handle, ATT_WRITE_REQ, cccd_handle, cfg_off, sizeof(cfg_off), FALSE, &req);
    if(ble_status != SUCCESS)
    {
        ctx->proc_busy = 0U;
        return BleLink_MapBleStatus(ble_status);
    }

    ble_status = GATT_WriteCharDesc(ctx->conn_handle, &req, g_ble_link_task_id);
    if(ble_status != SUCCESS)
    {
        ctx->proc_busy = 0U;
        GATT_bm_free((gattMsg_t *)&req, ATT_WRITE_REQ);
        return BleLink_MapBleStatus(ble_status);
    }

    BleLink_BeginProc(ctx, BLE_LINK_PROC_UNSUBSCRIBE, cccd_handle, 0U, 0U);
    return VP_STATUS_OK;
}

int BleLink_ExchangeMtu(uint8_t slot, uint16_t mtu)
{
    ble_link_ctx_t *ctx = BleLink_GetCtx(slot);
    attExchangeMTUReq_t req;
    bStatus_t ble_status;
    int status;

    status = BleLink_RequireReadyCtx(ctx);
    if(status != VP_STATUS_OK)
    {
        return status;
    }

    if(mtu < ATT_MTU_SIZE)
    {
        return VP_STATUS_ERR_INVALID_PARAM;
    }

    status = BleLink_RequireIdleProc(ctx);
    if(status != VP_STATUS_OK)
    {
        return status;
    }

    req.clientRxMTU = mtu;
    ble_status = GATT_ExchangeMTU(ctx->conn_handle, &req, g_ble_link_task_id);
    if(ble_status != SUCCESS)
    {
        ctx->proc_busy = 0U;
        return BleLink_MapBleStatus(ble_status);
    }

    BleLink_BeginProc(ctx, BLE_LINK_PROC_EXCHANGE_MTU, 0U, 0U, 0U);
    return VP_STATUS_OK;
}

void BleLink_HandleGattMsg(uint8_t slot, gattMsgEvent_t *msg)
{
    ble_link_ctx_t *ctx = BleLink_GetCtx(slot);

    if((ctx == 0) || (msg == 0))
    {
        return;
    }

    if(msg->method == ATT_MTU_UPDATED_EVENT)
    {
        ctx->mtu = msg->msg.mtuEvt.MTU;
        return;
    }

    if(msg->method == ATT_HANDLE_VALUE_NOTI)
    {
        BleLink_ReportValueEvent(slot,
                                 VP_BLE_EVT_NOTIFY_DATA,
                                 msg->msg.handleValueNoti.handle,
                                 msg->msg.handleValueNoti.pValue,
                                 msg->msg.handleValueNoti.len);
        return;
    }

    if(msg->method == ATT_HANDLE_VALUE_IND)
    {
        BleLink_ReportValueEvent(slot,
                                 VP_BLE_EVT_INDICATE_DATA,
                                 msg->msg.handleValueInd.handle,
                                 msg->msg.handleValueInd.pValue,
                                 msg->msg.handleValueInd.len);
        (void)ATT_HandleValueCfm(msg->connHandle);
        return;
    }

    if(ctx->proc_busy == 0U)
    {
        return;
    }

    switch(ctx->proc)
    {
        case BLE_LINK_PROC_DISC_SERVICE:
            BleLink_HandleDiscoverServices(ctx, msg);
            break;

        case BLE_LINK_PROC_DISC_CHAR:
            BleLink_HandleDiscoverChars(ctx, msg);
            break;

        case BLE_LINK_PROC_DISC_DESC:
            BleLink_HandleDiscoverDescs(ctx, msg);
            break;

        case BLE_LINK_PROC_READ:
            BleLink_HandleReadProc(ctx, msg);
            break;

        case BLE_LINK_PROC_WRITE_REQ:
        case BLE_LINK_PROC_SUBSCRIBE:
        case BLE_LINK_PROC_UNSUBSCRIBE:
            BleLink_HandleWriteProc(ctx, msg);
            break;

        case BLE_LINK_PROC_EXCHANGE_MTU:
            BleLink_HandleMtuProc(ctx, msg);
            break;

        default:
            break;
    }
}
