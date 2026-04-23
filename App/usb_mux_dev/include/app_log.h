#ifndef APP_USB_MUX_DEV_APP_LOG_H
#define APP_USB_MUX_DEV_APP_LOG_H

#include "config.h"

#if (APP_ENABLE_DEBUG == TRUE)

#define APP_LOG(tag, format, ...) \
    AppLog_Printf("[" tag "] " format "\r\n", ##__VA_ARGS__)

void AppLog_Init(void);
void AppLog_TaskInit(void);
void AppLog_Printf(const char *format, ...);
void AppLog_Process(void);
tmosEvents AppLog_ProcessEvent(tmosTaskID task_id, tmosEvents events);
uint32_t AppLog_GetDroppedBytes(void);
#else
#define APP_LOG(tag, format, ...)
#define AppLog_Init()
#define AppLog_TaskInit()
#define AppLog_Printf(format, ...)
#define AppLog_Process()
#define AppLog_ProcessEvent(task_id, events) 0U
#define AppLog_GetDroppedBytes() 0U
#endif

#define APP_LOG_BLE(format, ...) APP_LOG("BLE", format, ##__VA_ARGS__)
#define APP_LOG_USB(format, ...) APP_LOG("USB", format, ##__VA_ARGS__)

#endif
