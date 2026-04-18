#ifndef APP_USB_MUX_DEV_RING_BUFFER_H
#define APP_USB_MUX_DEV_RING_BUFFER_H

#include <stdint.h>

typedef struct {
    uint8_t *buf;
    uint16_t size;
    volatile uint16_t head;
    volatile uint16_t tail;
} ring_buffer_t;

/* Single-producer/single-consumer helpers only; no locking is provided. */
void RingBuffer_Init(ring_buffer_t *rb, uint8_t *buf, uint16_t size);
void RingBuffer_Reset(ring_buffer_t *rb);
uint16_t RingBuffer_Count(const ring_buffer_t *rb);
uint16_t RingBuffer_Space(const ring_buffer_t *rb);
int RingBuffer_IsEmpty(const ring_buffer_t *rb);
int RingBuffer_IsFull(const ring_buffer_t *rb);
int RingBuffer_PushByte(ring_buffer_t *rb, uint8_t data);
int RingBuffer_PopByte(ring_buffer_t *rb, uint8_t *data);
uint16_t RingBuffer_Write(ring_buffer_t *rb, const uint8_t *data, uint16_t len);
uint16_t RingBuffer_Read(ring_buffer_t *rb, uint8_t *data, uint16_t len);

#endif
