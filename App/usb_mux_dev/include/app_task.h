#ifndef APP_USB_MUX_DEV_APP_TASK_H
#define APP_USB_MUX_DEV_APP_TASK_H

#include "config.h"

void AppTask_Init(void);
void AppTask_KickUsbRx(void);
void AppTask_KickUsbTx(void);
void AppTask_KickUart(void);
void AppTask_KickPoll(void);
tmosEvents AppTask_ProcessEvent(tmosTaskID task_id, tmosEvents events);

#endif
