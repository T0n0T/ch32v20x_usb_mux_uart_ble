#include "app_log.h"

#if (APP_ENABLE_DEBUG == TRUE)

#include <stdarg.h>
#include <stdio.h>

#include "debug.h"

#define APP_LOG_EVT_FLUSH 0x0001U
#define APP_LOG_FLUSH_PERIOD_MS 1U

extern int _write(int fd, char *buf, int size);

static uint8_t g_log_buf[APP_LOG_BUFFER_SIZE];
static volatile uint16_t g_log_head;
static volatile uint16_t g_log_tail;
static volatile uint32_t g_log_dropped;
static tmosTaskID g_log_task_id = INVALID_TASK_ID;

static inline uint32_t AppLog_EnterCritical(void)
{
    uint32_t state;

    __asm volatile ("csrr %0, 0x800" : "=r" (state));
    __disable_irq();
    return state;
}

static inline void AppLog_ExitCritical(uint32_t status)
{
    __asm volatile ("csrw 0x800, %0" : : "r" (status));
}

static uint16_t AppLog_NextIndex(uint16_t index)
{
    ++index;
    if(index >= APP_LOG_BUFFER_SIZE)
    {
        index = 0U;
    }
    return index;
}

static int AppLog_PushByte(uint8_t data)
{
    uint16_t next_head = AppLog_NextIndex(g_log_head);

    if(next_head == g_log_tail)
    {
        ++g_log_dropped;
        return -1;
    }

    g_log_buf[g_log_head] = data;
    g_log_head = next_head;
    return 0;
}

static int AppLog_HasData(void)
{
    return g_log_head != g_log_tail;
}

static uint16_t AppLog_GetContiguousLen(void)
{
    uint16_t len;

    if(g_log_head == g_log_tail)
    {
        return 0U;
    }

    if(g_log_head > g_log_tail)
    {
        len = (uint16_t)(g_log_head - g_log_tail);
    }
    else
    {
        len = (uint16_t)(APP_LOG_BUFFER_SIZE - g_log_tail);
    }

    if(len > APP_LOG_FLUSH_CHUNK_SIZE)
    {
        len = APP_LOG_FLUSH_CHUNK_SIZE;
    }

    return len;
}

static void AppLog_AdvanceTail(uint16_t len)
{
    uint16_t tail = g_log_tail;

    tail = (uint16_t)(tail + len);
    if(tail >= APP_LOG_BUFFER_SIZE)
    {
        tail = (uint16_t)(tail - APP_LOG_BUFFER_SIZE);
    }

    g_log_tail = tail;
}

static void AppLog_Write(const uint8_t *data, uint16_t len)
{
    if((data != 0) && (len > 0U))
    {
        (void)_write(1, (char *)data, (int)len);
    }
}

static void AppLog_KickTask(void)
{
    if(g_log_task_id != INVALID_TASK_ID)
    {
        (void)tmos_set_event(g_log_task_id, APP_LOG_EVT_FLUSH);
    }
}

void AppLog_Init(void)
{
    uint32_t status = AppLog_EnterCritical();

    g_log_head = 0U;
    g_log_tail = 0U;
    g_log_dropped = 0U;
    AppLog_ExitCritical(status);
}

void AppLog_TaskInit(void)
{
    g_log_task_id = TMOS_ProcessEventRegister(AppLog_ProcessEvent);
    APP_LOG_INFO("LOG", "tmos task=%u", g_log_task_id);
}

void AppLog_Printf(const char *format, ...)
{
    char line[APP_LOG_LINE_SIZE];
    uint32_t status;
    int len;
    int i;
    va_list args;

    if(format == 0)
    {
        return;
    }

    va_start(args, format);
    len = vsnprintf(line, sizeof(line), format, args);
    va_end(args);

    if(len <= 0)
    {
        return;
    }

    if(len >= (int)sizeof(line))
    {
        len = (int)sizeof(line) - 1;
        ++g_log_dropped;
    }

    status = AppLog_EnterCritical();

    for(i = 0; i < len; ++i)
    {
        if(AppLog_PushByte((uint8_t)line[i]) != 0)
        {
            break;
        }
    }
    AppLog_ExitCritical(status);
    AppLog_KickTask();
}

void AppLog_Process(void)
{
    uint16_t tail;
    uint16_t len;
    uint32_t status = AppLog_EnterCritical();

    tail = g_log_tail;
    len = AppLog_GetContiguousLen();
    AppLog_ExitCritical(status);

    if(len > 0U)
    {
        AppLog_Write(&g_log_buf[tail], len);

        status = AppLog_EnterCritical();
        AppLog_AdvanceTail(len);
        AppLog_ExitCritical(status);
    }
}

tmosEvents AppLog_ProcessEvent(tmosTaskID task_id, tmosEvents events)
{
    (void)task_id;

    if((events & SYS_EVENT_MSG) != 0U)
    {
        uint8_t *msg_ptr;

        while((msg_ptr = tmos_msg_receive(g_log_task_id)) != 0)
        {
            (void)tmos_msg_deallocate(msg_ptr);
        }

        return events ^ SYS_EVENT_MSG;
    }

    if((events & APP_LOG_EVT_FLUSH) != 0U)
    {
        AppLog_Process();
        if(AppLog_HasData())
        {
            (void)tmos_start_task(g_log_task_id,
                                  APP_LOG_EVT_FLUSH,
                                  MS1_TO_SYSTEM_TIME(APP_LOG_FLUSH_PERIOD_MS));
        }
        return events ^ APP_LOG_EVT_FLUSH;
    }

    return 0U;
}

uint32_t AppLog_GetDroppedBytes(void)
{
    return g_log_dropped;
}

#endif
