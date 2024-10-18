#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "f1c100s_periph.h"

#define PACKED __attribute__((packed))

typedef union PACKED
{
    uint8_t data[7];
    struct
    {
        uint32_t dwDTERate; // Baud
        uint8_t bCharFormat; // 0 = 1 stop bit, 1 = 1.5 stop bit, 2 = 2 stop bit
        uint8_t bParityType; // 0 = none, 1 = odd, 2 = even, 3 = mark, 4 = space
        uint8_t bDataBits; // 5, 6, 7, 8, 16
    };
} CDC_LINECODING;

#ifdef __cplusplus
}
#endif
