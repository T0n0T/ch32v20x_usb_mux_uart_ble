#include "debug.h"
#include "app_init.h"
#include "CONFIG.h"
#include "HAL.h"

#ifndef BLE_MEMHEAP_SIZE
#define BLE_MEMHEAP_SIZE 4096U
#endif

extern void TMOS_SystemProcess(void);

__attribute__((aligned(4))) uint32_t MEM_BUF[BLE_MEMHEAP_SIZE / 4];

int main(void)
{
    SystemCoreClockUpdate();
    Delay_Init();
    USART_Printf_Init(115200);
    APP_Init();

    while(1)
    {
        TMOS_SystemProcess();
    }
}
