#include "heartbeat.h"

#include "config.h"
#include "ch32v20x_gpio.h"
#include "ch32v20x_rtc.h"

typedef struct {
    uint8_t  initialized;
    uint8_t  led_on;
    uint32_t next_deadline_tick;
} heartbeat_ctx_t;

static heartbeat_ctx_t g_heartbeat_ctx;

static uint32_t Heartbeat_GetNowTick(void)
{
    return RTC_GetCounter();
}

static uint32_t Heartbeat_GetPeriodTick(void)
{
    uint32_t period_tick =
        (uint32_t)((((uint64_t)APP_HEARTBEAT_PERIOD_MS) * ((uint64_t)(CAB_LSIFQ / 2U))) / 1000ULL);

    if(period_tick == 0U)
    {
        period_tick = 1U;
    }

    return period_tick;
}

static void Heartbeat_ApplyOutput(uint8_t led_on)
{
#if(APP_HEARTBEAT_ACTIVE_LOW != 0U)
    BitAction level = (led_on != 0U) ? Bit_RESET : Bit_SET;
#else
    BitAction level = (led_on != 0U) ? Bit_SET : Bit_RESET;
#endif

    GPIO_WriteBit(APP_HEARTBEAT_GPIO, APP_HEARTBEAT_PIN, level);
}

void Heartbeat_Init(void)
{
#if(APP_HEARTBEAT_ENABLE != 0U)
    GPIO_InitTypeDef gpio_cfg;
    uint32_t period_tick = Heartbeat_GetPeriodTick();

    RCC_APB2PeriphClockCmd(APP_HEARTBEAT_GPIO_CLOCK, ENABLE);

    gpio_cfg.GPIO_Pin = APP_HEARTBEAT_PIN;
    gpio_cfg.GPIO_Speed = GPIO_Speed_2MHz;
    gpio_cfg.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(APP_HEARTBEAT_GPIO, &gpio_cfg);

    g_heartbeat_ctx.initialized = 1U;
    g_heartbeat_ctx.led_on = 0U;
    Heartbeat_ApplyOutput(0U);
    g_heartbeat_ctx.next_deadline_tick = Heartbeat_GetNowTick() + period_tick;
#endif
}

void Heartbeat_Process(void)
{
#if(APP_HEARTBEAT_ENABLE != 0U)
    uint32_t now_tick;
    uint32_t period_tick;

    if(g_heartbeat_ctx.initialized == 0U)
    {
        return;
    }

    now_tick = Heartbeat_GetNowTick();
    if((int32_t)(now_tick - g_heartbeat_ctx.next_deadline_tick) < 0)
    {
        return;
    }

    period_tick = Heartbeat_GetPeriodTick();
    g_heartbeat_ctx.led_on ^= 1U;
    Heartbeat_ApplyOutput(g_heartbeat_ctx.led_on);
    g_heartbeat_ctx.next_deadline_tick += period_tick;
#endif
}
