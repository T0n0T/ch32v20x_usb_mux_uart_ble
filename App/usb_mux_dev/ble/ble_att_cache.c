#include "ble_att_cache.h"

#include <string.h>

#include "board_caps.h"

typedef struct {
    uint16_t svc_start;
    uint16_t svc_end;
    uint16_t char_handle;
    uint16_t cccd_handle;
} ble_att_slot_cache_t;

static ble_att_slot_cache_t g_ble_att_cache[APP_BLE_MAX_LINKS];

void BleAttCache_Init(void)
{
    memset(g_ble_att_cache, 0, sizeof(g_ble_att_cache));
}

void BleAttCache_ResetSlot(uint8_t slot)
{
    if(slot >= APP_BLE_MAX_LINKS)
    {
        return;
    }

    memset(&g_ble_att_cache[slot], 0, sizeof(g_ble_att_cache[slot]));
}
