#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct
{
    int head; // Points to the next write element
    int tail; // Points to the next read element
    uint8_t* buffer;
    uint16_t bufferLength;
} RINGBUFFER;

// Reads a byte from the buffer or returns -1 if the buffer is empty. 
int ringbuffer_read(RINGBUFFER* ring);

// Writes a byte to the buffer, returns true if the buffer has overflown.
int ringbuffer_write(RINGBUFFER* ring, uint8_t data);

// Returns the number of bytes in the buffer;
int ringbuffer_length(RINGBUFFER* ring);

#ifdef __cplusplus
}
#endif
