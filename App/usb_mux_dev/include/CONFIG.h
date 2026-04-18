#ifndef APP_USB_MUX_DEV_CONFIG_H
#define APP_USB_MUX_DEV_CONFIG_H

#include "wchble.h"
#include "ch32v20x.h"
#include "ch32v20x_flash.h"
#include "ch32v20x_rtc.h"

#define CHIP_ID                   0x0208
#define HAL_SLEEP                 FALSE
#define HAL_KEY                   FALSE
#define HAL_LED                   FALSE
#define TEM_SAMPLE                FALSE
#define BLE_CALIBRATION_ENABLE    FALSE
#define BLE_SNV                   FALSE
#define CLK_OSC32K                1
#define BLE_MEMHEAP_SIZE          (1024U * 7U)
#define BLE_BUFF_MAX_LEN          27U
#define BLE_BUFF_NUM              5U
#define BLE_TX_NUM_EVENT          1U
#define BLE_TX_POWER              LL_TX_POWEER_0_DBM
#define PERIPHERAL_MAX_CONNECTION 0U
#define CENTRAL_MAX_CONNECTION    3U
#define APP_ENABLE_DEBUG_UART     FALSE

extern uint32_t MEM_BUF[BLE_MEMHEAP_SIZE / 4U];

#endif
