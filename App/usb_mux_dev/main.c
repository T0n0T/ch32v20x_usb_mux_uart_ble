#include "debug.h"
#include "app_init.h"
#include "app_log.h"
#include "app_build_info.h"
#include "config.h"

#ifndef BLE_MEMHEAP_SIZE
#define BLE_MEMHEAP_SIZE 4096U
#endif

extern void TMOS_SystemProcess(void);

__attribute__((aligned(4))) uint32_t MEM_BUF[BLE_MEMHEAP_SIZE / 4];

__attribute__((section(".highcode")))
__attribute__((noinline))
static void Main_Circulation(void)
{
    while(1)
    {
        TMOS_SystemProcess();
    }
}

static void APP_PrintBootBanner(void)
{
    printf("\r\n========================================\r\n\r\n");
    printf("   CH32V208 USB MUX BLE HOST\r\n");
    printf("   Git   : %s\r\n", APP_BUILD_GIT_DESC);
    printf("   Build : %s %s\r\n", APP_BUILD_DATE, APP_BUILD_TIME);
    printf("\r\n========================================\r\n\r\n");
}

int main(void)
{
    SystemCoreClockUpdate();
    Delay_Init();
#if(APP_ENABLE_DEBUG == TRUE)
#if(SDI_PRINT == SDI_PR_OPEN)
    SDI_Printf_Enable();
#else
    USART_Printf_Init(115200);
#endif
#endif
    APP_PrintBootBanner();
    APP_Init();
    Main_Circulation();
}
