#ifndef APP_USB_MUX_DEV_BLE_LINK_FSM_H
#define APP_USB_MUX_DEV_BLE_LINK_FSM_H

#include <stdint.h>

#include "config.h"

typedef enum {
    BLE_L_IDLE = 0,
    BLE_L_CONNECT_PENDING,
    BLE_L_CONNECTING,
    BLE_L_CONNECTED,
    BLE_L_MTU_EXCHANGING,
    BLE_L_DISC_SERVICE,
    BLE_L_DISC_CHAR,
    BLE_L_DISC_DESC,
    BLE_L_READY,
    BLE_L_READING,
    BLE_L_WRITING_REQ,
    BLE_L_SUBSCRIBING,
    BLE_L_UNSUBSCRIBING,
    BLE_L_DISCONNECTING,
    BLE_L_ERROR,
} ble_link_state_t;

void BleLink_Init(uint8_t task_id);
void BleLink_Attach(uint8_t slot, uint16_t conn_handle);
void BleLink_Reset(uint8_t slot);
uint8_t BleLink_GetState(uint8_t slot);
uint16_t BleLink_GetMtu(uint8_t slot);

int BleLink_StartDiscoverServices(uint8_t slot);
int BleLink_StartDiscoverChars(uint8_t slot, uint16_t start_hdl, uint16_t end_hdl);
int BleLink_StartDiscoverDescs(uint8_t slot, uint16_t start_hdl, uint16_t end_hdl);
int BleLink_Read(uint8_t slot, uint16_t attr_handle);
int BleLink_WriteReq(uint8_t slot, uint16_t attr_handle, const uint8_t *buf, uint16_t len);
int BleLink_WriteCmd(uint8_t slot, uint16_t attr_handle, const uint8_t *buf, uint16_t len);
int BleLink_Subscribe(uint8_t slot, uint16_t cccd_handle);
int BleLink_Unsubscribe(uint8_t slot, uint16_t cccd_handle);
int BleLink_ExchangeMtu(uint8_t slot, uint16_t mtu);
void BleLink_HandleGattMsg(uint8_t slot, gattMsgEvent_t *msg);

#endif
