#include "ring_buffer.h"

static uint16_t RingBuffer_NextIndex(const ring_buffer_t *rb, uint16_t index)
{
    ++index;
    if(index >= rb->size)
    {
        index = 0U;
    }
    return index;
}

void RingBuffer_Init(ring_buffer_t *rb, uint8_t *buf, uint16_t size)
{
    if(rb == 0)
    {
        return;
    }

    rb->buf = buf;
    rb->size = size;
    rb->head = 0U;
    rb->tail = 0U;
}

void RingBuffer_Reset(ring_buffer_t *rb)
{
    if(rb == 0)
    {
        return;
    }

    rb->head = 0U;
    rb->tail = 0U;
}

uint16_t RingBuffer_Count(const ring_buffer_t *rb)
{
    if((rb == 0) || (rb->size < 2U))
    {
        return 0U;
    }

    if(rb->head >= rb->tail)
    {
        return (uint16_t)(rb->head - rb->tail);
    }

    return (uint16_t)(rb->size - rb->tail + rb->head);
}

uint16_t RingBuffer_Space(const ring_buffer_t *rb)
{
    if((rb == 0) || (rb->size < 2U))
    {
        return 0U;
    }

    return (uint16_t)((rb->size - 1U) - RingBuffer_Count(rb));
}

int RingBuffer_IsEmpty(const ring_buffer_t *rb)
{
    return RingBuffer_Count(rb) == 0U;
}

int RingBuffer_IsFull(const ring_buffer_t *rb)
{
    return RingBuffer_Space(rb) == 0U;
}

int RingBuffer_PushByte(ring_buffer_t *rb, uint8_t data)
{
    uint16_t next_head;

    if((rb == 0) || (rb->buf == 0) || (rb->size < 2U))
    {
        return -1;
    }

    next_head = RingBuffer_NextIndex(rb, rb->head);
    if(next_head == rb->tail)
    {
        return -1;
    }

    rb->buf[rb->head] = data;
    rb->head = next_head;

    return 0;
}

int RingBuffer_PopByte(ring_buffer_t *rb, uint8_t *data)
{
    if((rb == 0) || (rb->buf == 0) || (data == 0))
    {
        return -1;
    }

    if(rb->head == rb->tail)
    {
        return -1;
    }

    *data = rb->buf[rb->tail];
    rb->tail = RingBuffer_NextIndex(rb, rb->tail);

    return 0;
}

uint16_t RingBuffer_Write(ring_buffer_t *rb, const uint8_t *data, uint16_t len)
{
    uint16_t written = 0U;

    if((rb == 0) || (data == 0))
    {
        return 0U;
    }

    while(written < len)
    {
        if(RingBuffer_PushByte(rb, data[written]) != 0)
        {
            break;
        }
        ++written;
    }

    return written;
}

uint16_t RingBuffer_Read(ring_buffer_t *rb, uint8_t *data, uint16_t len)
{
    uint16_t read = 0U;

    if((rb == 0) || (data == 0))
    {
        return 0U;
    }

    while(read < len)
    {
        if(RingBuffer_PopByte(rb, &data[read]) != 0)
        {
            break;
        }
        ++read;
    }

    return read;
}
