#include "app_task.h"

#include "app_log.h"
#include "heartbeat.h"
#include "uart_manager.h"
#include "usb_rx_fsm.h"
#include "usb_tx_sched.h"

#define APP_TASK_EVT_HEARTBEAT 0x0001U
#define APP_TASK_EVT_USB_RX    0x0002U
#define APP_TASK_EVT_USB_TX    0x0004U
#define APP_TASK_EVT_UART      0x0008U
#define APP_TASK_EVT_POLL      0x0010U

#define APP_TASK_POLL_PERIOD_MS 1U

static tmosTaskID g_app_task_id = INVALID_TASK_ID;

static void AppTask_SetEvent(tmosEvents event)
{
    if(g_app_task_id != INVALID_TASK_ID)
    {
        (void)tmos_set_event(g_app_task_id, event);
    }
}

static void AppTask_RunIo(void)
{
    USBRX_Process();
    UartMgr_Process();
    USBTX_Process();
}

void AppTask_Init(void)
{
    g_app_task_id = TMOS_ProcessEventRegister(AppTask_ProcessEvent);

    (void)tmos_set_event(g_app_task_id,
                         APP_TASK_EVT_HEARTBEAT |
                         APP_TASK_EVT_USB_RX |
                         APP_TASK_EVT_USB_TX |
                         APP_TASK_EVT_UART |
                         APP_TASK_EVT_POLL);
    APP_LOG("APP", "tmos task=%u", g_app_task_id);
}

void AppTask_KickUsbRx(void)
{
    AppTask_SetEvent(APP_TASK_EVT_USB_RX);
}

void AppTask_KickUsbTx(void)
{
    AppTask_SetEvent(APP_TASK_EVT_USB_TX);
}

void AppTask_KickUart(void)
{
    AppTask_SetEvent(APP_TASK_EVT_UART);
}

void AppTask_KickPoll(void)
{
    AppTask_SetEvent(APP_TASK_EVT_POLL);
}

tmosEvents AppTask_ProcessEvent(tmosTaskID task_id, tmosEvents events)
{
    (void)task_id;

    if((events & SYS_EVENT_MSG) != 0U)
    {
        uint8_t *msg_ptr;

        while((msg_ptr = tmos_msg_receive(g_app_task_id)) != 0)
        {
            (void)tmos_msg_deallocate(msg_ptr);
        }

        return events ^ SYS_EVENT_MSG;
    }

    if((events & APP_TASK_EVT_HEARTBEAT) != 0U)
    {
        Heartbeat_Process();
        (void)tmos_start_task(g_app_task_id,
                              APP_TASK_EVT_HEARTBEAT,
                              MS1_TO_SYSTEM_TIME(APP_HEARTBEAT_PERIOD_MS));
        return events ^ APP_TASK_EVT_HEARTBEAT;
    }

    if((events & APP_TASK_EVT_USB_RX) != 0U)
    {
        AppTask_RunIo();
        return events ^ APP_TASK_EVT_USB_RX;
    }

    if((events & APP_TASK_EVT_USB_TX) != 0U)
    {
        AppTask_RunIo();
        return events ^ APP_TASK_EVT_USB_TX;
    }

    if((events & APP_TASK_EVT_UART) != 0U)
    {
        AppTask_RunIo();
        return events ^ APP_TASK_EVT_UART;
    }

    if((events & APP_TASK_EVT_POLL) != 0U)
    {
        AppTask_RunIo();
        (void)tmos_start_task(g_app_task_id,
                              APP_TASK_EVT_POLL,
                              MS1_TO_SYSTEM_TIME(APP_TASK_POLL_PERIOD_MS));
        return events ^ APP_TASK_EVT_POLL;
    }

    return 0U;
}
