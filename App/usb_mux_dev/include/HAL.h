#ifndef APP_USB_MUX_DEV_HAL_H
#define APP_USB_MUX_DEV_HAL_H

#include "wchble.h"

extern tmosTaskID halTaskID;

void HAL_Init(void);
tmosEvents HAL_ProcessEvent(tmosTaskID task_id, tmosEvents events);
void WCHBLE_Init(void);

#endif
