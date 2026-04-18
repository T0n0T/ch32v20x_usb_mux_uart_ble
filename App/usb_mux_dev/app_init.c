#include "app_init.h"

__attribute__((weak)) void WCHBLE_Init(void)
{
}

__attribute__((weak)) void HAL_Init(void)
{
}

extern void USBRX_Init(void);
extern void USBTX_Init(void);
extern void USBMUX_DeviceInit(void);
extern void VendorRouter_Init(void);

void APP_Init(void)
{
    WCHBLE_Init();
    HAL_Init();
    USBRX_Init();
    USBTX_Init();
    VendorRouter_Init();
    USBMUX_DeviceInit();
}
