#include "app_init.h"

#include <string.h>

#include "config.h"
#include "hal.h"
#include "heartbeat.h"

tmosTaskID halTaskID = INVALID_TASK_ID;
uint32_t g_LLE_IRQLibHandlerLocation;

static void APP_BleTimeInit(void)
{
    bleClockConfig_t clock_cfg;
    bStatus_t status;

    memset(&clock_cfg, 0, sizeof(clock_cfg));

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
    PWR_BackupAccessCmd(ENABLE);

    RCC_LSICmd(ENABLE);
    while(RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET)
    {
    }

    RCC_LSEConfig(RCC_LSE_OFF);
    RCC_RTCCLKConfig(RCC_RTCCLKSource_LSI);
    RCC_RTCCLKCmd(ENABLE);
    RTC_WaitForLastTask();
    RTC_SetPrescaler(1U);
    RTC_WaitForLastTask();
    RTC_SetCounter(0U);
    RTC_WaitForLastTask();

    clock_cfg.ClockAccuracy = 1000U;
    clock_cfg.ClockFrequency = CAB_LSIFQ / 2U;
    clock_cfg.ClockMaxCount = 0xFFFFFFFFUL;
    clock_cfg.getClockValue = RTC_GetCounter;

    status = TMOS_TimerInit(&clock_cfg);
    if(status != SUCCESS)
    {
        while(1)
        {
        }
    }
}

tmosEvents HAL_ProcessEvent(tmosTaskID task_id, tmosEvents events)
{
    if((events & SYS_EVENT_MSG) != 0U)
    {
        uint8_t *msg_ptr = tmos_msg_receive(task_id);

        if(msg_ptr != 0)
        {
            (void)tmos_msg_deallocate(msg_ptr);
        }

        return events ^ SYS_EVENT_MSG;
    }

    return 0U;
}

void HAL_Init(void)
{
    halTaskID = TMOS_ProcessEventRegister(HAL_ProcessEvent);
    APP_BleTimeInit();
}

void WCHBLE_Init(void)
{
    bleConfig_t cfg;
    uint8_t mac_addr[6];
    bStatus_t status;

    memset(&cfg, 0, sizeof(cfg));
    g_LLE_IRQLibHandlerLocation = (uint32_t)(uintptr_t)LLE_IRQLibHandler;

    OSC->HSE_CAL_CTRL &= ~(0x07UL << 28);
    OSC->HSE_CAL_CTRL |= (0x03UL << 28);
    OSC->HSE_CAL_CTRL |= (0x03UL << 24);

    FLASH_GetMACAddress(mac_addr);

    cfg.MEMAddr = (uint32_t)(uintptr_t)MEM_BUF;
    cfg.MEMLen = BLE_MEMHEAP_SIZE;
    cfg.BufMaxLen = BLE_BUFF_MAX_LEN;
    cfg.BufNumber = BLE_BUFF_NUM;
    cfg.TxNumEvent = BLE_TX_NUM_EVENT;
    cfg.TxPower = BLE_TX_POWER;
    cfg.ConnectNumber = (uint8_t)((PERIPHERAL_MAX_CONNECTION & 0x03U) | (CENTRAL_MAX_CONNECTION << 2));
    cfg.ClockFrequency = CAB_LSIFQ / 2U;
    cfg.ClockAccuracy = 1000U;
    memcpy(cfg.MacAddr, mac_addr, sizeof(mac_addr));

    status = BLE_LibInit(&cfg);
    if(status != SUCCESS)
    {
        while(1)
        {
        }
    }

    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_CRC, ENABLE);
    NVIC_EnableIRQ(BB_IRQn);
    NVIC_EnableIRQ(LLE_IRQn);
}

extern void USBRX_Init(void);
extern void USBTX_Init(void);
extern void USBMUX_DeviceInit(void);
extern void VendorRouter_Init(void);
extern void UartMgr_Init(void);
extern void BleHostMgr_Init(void);

void APP_Init(void)
{
    WCHBLE_Init();
    HAL_Init();
    Heartbeat_Init();
    BleHostMgr_Init();
    USBRX_Init();
    USBTX_Init();
    UartMgr_Init();
    VendorRouter_Init();
    USBMUX_DeviceInit();
}
