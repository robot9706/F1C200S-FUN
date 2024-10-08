#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "f1c100s_periph.h"

#define __I volatile const
#define __O volatile
#define __IO volatile

#define PACKED __attribute__((packed))

typedef struct
{
    __IO uint32_t CFG;
    __IO uint32_t SRC;
    __IO uint32_t DST;
    __IO uint32_t BYTE_COUNTER;
} NDMA_T; // Normal DMA

typedef struct
{
    __IO uint32_t CFG;
    __IO uint32_t SRC;
    __IO uint32_t DST;
    __IO uint32_t BYTE_COUNTER;
    __IO uint32_t PARAM;
    __IO uint32_t GENERAL_DATA;
} DDMA_T; // Dedicated DMA

typedef struct
{
    __IO uint32_t INT_CTRL;
    __IO uint32_t INT_STATUS;
    __IO uint32_t INT_PRIO;
} DMA_T;
#define DMA ((DMA_T *)DMA_BASE)

#define NDMA_ADR(n) (DMA_BASE + 0x100 + (n * 0x20))
#define DDMA_ADR(n) (DMA_BASE + 0x300 + (n * 0x20))

#define NDMA(n) ((NDMA_T *)NDMA_ADR(n))
#define DDMA(n) ((DDMA_T *)DDMA_ADR(n))

#ifdef __cplusplus
}
#endif
