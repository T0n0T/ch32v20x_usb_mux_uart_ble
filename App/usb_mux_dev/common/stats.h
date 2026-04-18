#ifndef APP_USB_MUX_DEV_STATS_H
#define APP_USB_MUX_DEV_STATS_H

#include <stdint.h>

#define STATS_BLOCK_COUNTER_COUNT 8U

typedef uint32_t stats_counter_t;

typedef struct {
    stats_counter_t counters[STATS_BLOCK_COUNTER_COUNT];
} stats_block_t;

void Stats_Reset(void *stats, uint16_t size);
void Stats_ResetBlock(stats_block_t *stats);
void Stats_Inc(stats_counter_t *counter);
void Stats_Add(stats_counter_t *counter, uint32_t value);

#endif
