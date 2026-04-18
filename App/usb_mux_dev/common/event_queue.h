#ifndef APP_USB_MUX_DEV_EVENT_QUEUE_H
#define APP_USB_MUX_DEV_EVENT_QUEUE_H

#include <stdint.h>

typedef struct {
    uint16_t id;
    uint16_t arg;
} app_event_t;

typedef struct {
    app_event_t *buf;
    uint16_t size;
    volatile uint16_t head;
    volatile uint16_t tail;
} event_queue_t;

/* Single-producer/single-consumer helpers only; no locking is provided. */
void EventQueue_Init(event_queue_t *queue, app_event_t *buf, uint16_t size);
void EventQueue_Reset(event_queue_t *queue);
uint16_t EventQueue_Count(const event_queue_t *queue);
uint16_t EventQueue_Space(const event_queue_t *queue);
int EventQueue_IsEmpty(const event_queue_t *queue);
int EventQueue_IsFull(const event_queue_t *queue);
int EventQueue_Push(event_queue_t *queue, const app_event_t *event);
int EventQueue_Pop(event_queue_t *queue, app_event_t *event);

#endif
