#include "event_queue.h"

static uint16_t EventQueue_NextIndex(const event_queue_t *queue, uint16_t index)
{
    ++index;
    if(index >= queue->size)
    {
        index = 0U;
    }
    return index;
}

void EventQueue_Init(event_queue_t *queue, app_event_t *buf, uint16_t size)
{
    if(queue == 0)
    {
        return;
    }

    queue->buf = buf;
    queue->size = size;
    queue->head = 0U;
    queue->tail = 0U;
}

void EventQueue_Reset(event_queue_t *queue)
{
    if(queue == 0)
    {
        return;
    }

    queue->head = 0U;
    queue->tail = 0U;
}

uint16_t EventQueue_Count(const event_queue_t *queue)
{
    if((queue == 0) || (queue->size < 2U))
    {
        return 0U;
    }

    if(queue->head >= queue->tail)
    {
        return (uint16_t)(queue->head - queue->tail);
    }

    return (uint16_t)(queue->size - queue->tail + queue->head);
}

uint16_t EventQueue_Space(const event_queue_t *queue)
{
    if((queue == 0) || (queue->size < 2U))
    {
        return 0U;
    }

    return (uint16_t)((queue->size - 1U) - EventQueue_Count(queue));
}

int EventQueue_IsEmpty(const event_queue_t *queue)
{
    return EventQueue_Count(queue) == 0U;
}

int EventQueue_IsFull(const event_queue_t *queue)
{
    return EventQueue_Space(queue) == 0U;
}

int EventQueue_Push(event_queue_t *queue, const app_event_t *event)
{
    uint16_t next_head;

    if((queue == 0) || (queue->buf == 0) || (event == 0) || (queue->size < 2U))
    {
        return -1;
    }

    next_head = EventQueue_NextIndex(queue, queue->head);
    if(next_head == queue->tail)
    {
        return -1;
    }

    queue->buf[queue->head] = *event;
    queue->head = next_head;

    return 0;
}

int EventQueue_Pop(event_queue_t *queue, app_event_t *event)
{
    if((queue == 0) || (queue->buf == 0) || (event == 0))
    {
        return -1;
    }

    if(queue->head == queue->tail)
    {
        return -1;
    }

    *event = queue->buf[queue->tail];
    queue->tail = EventQueue_NextIndex(queue, queue->tail);

    return 0;
}
