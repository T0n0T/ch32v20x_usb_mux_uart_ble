#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define HEARTBEAT_PERIOD_MS 500U
#define RTC_FREQ_HZ         16000U

static uint32_t BuggyHeartbeatNowMs(uint32_t rtc_ticks)
{
    return (rtc_ticks * 1000U) / RTC_FREQ_HZ;
}

static uint32_t HeartbeatPeriodTick(void)
{
    uint32_t period_tick =
        (uint32_t)((((uint64_t)HEARTBEAT_PERIOD_MS) * ((uint64_t)RTC_FREQ_HZ)) / 1000ULL);

    if(period_tick == 0U)
    {
        period_tick = 1U;
    }

    return period_tick;
}

static int Test_BuggyMsConversionStopsBeingMonotonic(void)
{
    const uint32_t overflow_tick = (UINT32_MAX / 1000U) + 1U;
    uint32_t before_ms = BuggyHeartbeatNowMs(overflow_tick - 1U);
    uint32_t after_ms = BuggyHeartbeatNowMs(overflow_tick);

    if(after_ms >= before_ms)
    {
        fprintf(stderr,
                "expected ms conversion to wrap, got before=%u after=%u\n",
                before_ms,
                after_ms);
        return 1;
    }

    return 0;
}

static int Test_TickDeadlineKeepsAdvancingAcrossOldWrapPoint(void)
{
    uint32_t period_tick = HeartbeatPeriodTick();
    uint32_t now_tick = (UINT32_MAX / 1000U) - (RTC_FREQ_HZ * 2U);
    uint32_t next_deadline_tick = now_tick + period_tick;
    unsigned int toggles = 0U;

    while((int32_t)(now_tick - next_deadline_tick) < 0)
    {
        now_tick += 1U;
    }

    for(; toggles < 16U; ++toggles)
    {
        if((int32_t)(now_tick - next_deadline_tick) < 0)
        {
            fprintf(stderr,
                    "deadline not reached at toggle=%u now=%u deadline=%u\n",
                    toggles,
                    now_tick,
                    next_deadline_tick);
            return 1;
        }

        next_deadline_tick += period_tick;
        now_tick += period_tick;
    }

    return 0;
}

int main(void)
{
    if(Test_BuggyMsConversionStopsBeingMonotonic() != 0)
    {
        return 1;
    }

    if(Test_TickDeadlineKeepsAdvancingAcrossOldWrapPoint() != 0)
    {
        return 1;
    }

    puts("heartbeat math ok");
    return 0;
}
