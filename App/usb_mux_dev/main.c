#include "debug.h"
#include "app_init.h"
#include "config.h"
#include "heartbeat.h"
#include "App/usb_mux_dev/usb/usb_rx_fsm.h"
#include "App/usb_mux_dev/usb/usb_tx_sched.h"
#include "App/usb_mux_dev/uart/uart_manager.h"

#ifndef BLE_MEMHEAP_SIZE
#define BLE_MEMHEAP_SIZE 4096U
#endif

extern void TMOS_SystemProcess(void);

__attribute__((aligned(4))) uint32_t MEM_BUF[BLE_MEMHEAP_SIZE / 4];

int main(void)
{
    SystemCoreClockUpdate();
    Delay_Init();
#if(APP_ENABLE_DEBUG_UART == TRUE)
    USART_Printf_Init(115200);
#endif
    APP_Init();

    while(1)
    {
        Heartbeat_Process();
        USBRX_Process();
        UartMgr_Process();
        USBTX_Process();
        TMOS_SystemProcess();
    }
}
