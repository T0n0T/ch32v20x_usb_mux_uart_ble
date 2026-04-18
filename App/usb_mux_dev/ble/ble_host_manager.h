#ifndef APP_USB_MUX_DEV_BLE_HOST_MANAGER_H
#define APP_USB_MUX_DEV_BLE_HOST_MANAGER_H

#include <stdint.h>

#include "vendor_proto.h"

typedef enum {
    BLE_G_IDLE = 0,
    BLE_G_READY,
    BLE_G_SCANNING,
    BLE_G_CONNECTING,
    BLE_G_MIXED,
    BLE_G_STOPPING,
} ble_global_state_t;

void BleHostMgr_Init(void);
void BleHostMgr_HandleMgmt(const vp_hdr_t *hdr, const uint8_t *payload, uint16_t payload_len);

#endif
