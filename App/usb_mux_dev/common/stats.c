#include "stats.h"

#include <string.h>

void Stats_Reset(void *stats, uint16_t size)
{
    if((stats == 0) || (size == 0U))
    {
        return;
    }

    memset(stats, 0, size);
}

void Stats_ResetBlock(stats_block_t *stats)
{
    Stats_Reset(stats, (uint16_t)sizeof(*stats));
}

void Stats_Inc(stats_counter_t *counter)
{
    Stats_Add(counter, 1U);
}

void Stats_Add(stats_counter_t *counter, uint32_t value)
{
    if(counter == 0)
    {
        return;
    }

    *counter += value;
}
