#include "ringbuffer.h"

int ringbuffer_read(RINGBUFFER* ring)
{
    if (ring->head == ring->tail)
    {
        return -1; // No data in the buffer
    }

    uint8_t data = ring->buffer[ring->head]; // Read the data at the head
    ring->head = (ring->head + 1) % ring->bufferLength; // Advance the head
    return data;
}

int ringbuffer_write(RINGBUFFER* ring, uint8_t data)
{
    ring->buffer[ring->tail] = data;
    ring->tail = (ring->tail + 1) % ring->bufferLength;

    return (ring->tail == ring->head);
}

int ringbuffer_length(RINGBUFFER* ring)
{
    if (ring->tail < ring->head)
    {
        return (ring->bufferLength - ring->head) + ring->tail;
    }

    return ring->tail - ring->head;
}
