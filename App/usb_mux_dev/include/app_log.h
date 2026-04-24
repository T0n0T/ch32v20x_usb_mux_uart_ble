#ifndef APP_USB_MUX_DEV_APP_LOG_H
#define APP_USB_MUX_DEV_APP_LOG_H

#include "config.h"

#if (APP_ENABLE_DEBUG == TRUE)

typedef enum {
    APP_LOG_DEBUG_LEVEL = APP_LOG_LEVEL_DEBUG,
    APP_LOG_INFO_LEVEL = APP_LOG_LEVEL_INFO,
    APP_LOG_WARNING_LEVEL = APP_LOG_LEVEL_WARNING,
    APP_LOG_ERROR_LEVEL = APP_LOG_LEVEL_ERROR,
} app_log_level_t;

#define APP_LOG_ENABLED(level) ((level) >= APP_LOG_MIN_LEVEL)

#define APP_LOG_WRITE(level_name, tag, format, ...) \
    AppLog_Printf("[" level_name "][" tag "] " format "\r\n", ##__VA_ARGS__)

#if APP_LOG_ENABLED(APP_LOG_LEVEL_DEBUG)
#define APP_LOG_DEBUG(tag, format, ...) \
    APP_LOG_WRITE("DEBUG", tag, format, ##__VA_ARGS__)
#else
#define APP_LOG_DEBUG(tag, format, ...)
#endif

#if APP_LOG_ENABLED(APP_LOG_LEVEL_INFO)
#define APP_LOG_INFO(tag, format, ...) \
    APP_LOG_WRITE("INFO", tag, format, ##__VA_ARGS__)
#else
#define APP_LOG_INFO(tag, format, ...)
#endif

#if APP_LOG_ENABLED(APP_LOG_LEVEL_WARNING)
#define APP_LOG_WARNING(tag, format, ...) \
    APP_LOG_WRITE("WARNING", tag, format, ##__VA_ARGS__)
#else
#define APP_LOG_WARNING(tag, format, ...)
#endif

#if APP_LOG_ENABLED(APP_LOG_LEVEL_ERROR)
#define APP_LOG_ERROR(tag, format, ...) \
    APP_LOG_WRITE("ERROR", tag, format, ##__VA_ARGS__)
#else
#define APP_LOG_ERROR(tag, format, ...)
#endif

#define APP_LOG(tag, format, ...) \
    APP_LOG_INFO(tag, format, ##__VA_ARGS__)

void AppLog_Init(void);
void AppLog_TaskInit(void);
void AppLog_Printf(const char *format, ...);
void AppLog_Process(void);
tmosEvents AppLog_ProcessEvent(tmosTaskID task_id, tmosEvents events);
uint32_t AppLog_GetDroppedBytes(void);
#else
#define APP_LOG_DEBUG(tag, format, ...)
#define APP_LOG_INFO(tag, format, ...)
#define APP_LOG_WARNING(tag, format, ...)
#define APP_LOG_ERROR(tag, format, ...)
#define APP_LOG(tag, format, ...)
#define AppLog_Init()
#define AppLog_TaskInit()
#define AppLog_Printf(format, ...)
#define AppLog_Process()
#define AppLog_ProcessEvent(task_id, events) 0U
#define AppLog_GetDroppedBytes() 0U
#endif

#define APP_LOG_BLE_DEBUG(format, ...) APP_LOG_DEBUG("BLE", format, ##__VA_ARGS__)
#define APP_LOG_BLE_INFO(format, ...) APP_LOG_INFO("BLE", format, ##__VA_ARGS__)
#define APP_LOG_BLE_WARNING(format, ...) APP_LOG_WARNING("BLE", format, ##__VA_ARGS__)
#define APP_LOG_BLE_ERROR(format, ...) APP_LOG_ERROR("BLE", format, ##__VA_ARGS__)

#define APP_LOG_USB_DEBUG(format, ...) APP_LOG_DEBUG("USB", format, ##__VA_ARGS__)
#define APP_LOG_USB_INFO(format, ...) APP_LOG_INFO("USB", format, ##__VA_ARGS__)
#define APP_LOG_USB_WARNING(format, ...) APP_LOG_WARNING("USB", format, ##__VA_ARGS__)
#define APP_LOG_USB_ERROR(format, ...) APP_LOG_ERROR("USB", format, ##__VA_ARGS__)

#define APP_LOG_BLE(format, ...) APP_LOG_BLE_INFO(format, ##__VA_ARGS__)
#define APP_LOG_USB(format, ...) APP_LOG_USB_INFO(format, ##__VA_ARGS__)

#endif
