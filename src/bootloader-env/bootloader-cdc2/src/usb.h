#pragma once

#include <stdint.h>
#include "f1c100s_periph.h"

#define __I volatile const
#define __O volatile
#define __IO volatile

#define PACKED __attribute__((packed))

typedef struct
{
    union
    { // 0x00..0x3F FIFO_0..FIFO_15 (8b,16b,32b access)
        __IO uint32_t word32;
        __IO uint16_t word16;
        __IO uint8_t byte;
    } FIFO[16];
    __IO uint8_t POWER;   // 0x40 Power management
    __IO uint8_t DEVCTL;  // 0x41 OTG device control
    __IO uint8_t EP_IDX;  // 0x42 Index for selecting endpoint
    __IO uint8_t VEND0;   // 0x43 Controls whether to use DMA mode
    __IO uint32_t EP_IS;  // 0x44 EP interrupt status (RX-high, TX-low) - section 3.2.3 ?
    __IO uint32_t EP_IE;  // 0x48 EP interrupt enable (RX-high, TX-low)
    __IO uint32_t BUS_IS; // 0x4C USB interrupt status (1-byte) - section 3.2.7
    __IO uint32_t BUS_IE; // 0x50 USB interrupt enable (1-byte)
    __IO uint32_t FRAME;  // 0x54 USB frame number
    __IO uint32_t res0[10];
    // EP control regs (indexed)
    __IO uint16_t TXMAXP;    // 0x80
    __IO uint16_t TXCSR;     // 0x82 - section 3.3.8
    __IO uint16_t RXMAXP;    // 0x84
    __IO uint16_t RXCSR;     // 0x86
    __IO uint16_t RXCOUNT;   // 0x88
    __IO uint16_t res1;      // 0x8A
    __IO uint8_t TXTYPE;     // 0x8C
    __IO uint8_t TXINTERVAL; // 0x8D
    __IO uint8_t RXTYPE;     // 0x8E
    __IO uint8_t RXINTERVAL; // 0x8F
    // FIFO control regs (indexed)
    __IO uint16_t TXFIFOSZ;   // 0x90 TX endpoint FIFO size (2 ^ (size + 3))
    __IO uint16_t TXFIFOADDR; // 0x92 TX endpoint FIFO address (offset / 8)
    __IO uint16_t RXFIFOSZ;   // 0x94 RX endpoint FIFO size (2 ^ (size + 3))
    __IO uint16_t RXFIFOADDR; // 0x96 RX endpoint FIFO address (offset / 8)
    // Multipoint / busctl regs (indexed)
    __IO uint8_t TXFUNCADDR; // 0x98 Function address register
    __IO uint8_t res4;       // 0x99
    __IO uint8_t TXHUBADDR;  // 0x9A
    __IO uint8_t TXHUBPORT;  // 0x9B
    __IO uint8_t RXFUNCADDR; // 0x9C
    __IO uint8_t res5;       // 0x9D
    __IO uint8_t RXHUBADDR;  // 0x9E
    __IO uint8_t RXHUBPORT;  // 0x9F
    __IO uint8_t res6[32];
    // configdata regs
    __IO uint8_t CONFIGDATA; // 0xc0
    __IO uint8_t res7[831];
    __IO uint32_t ISCR;    // 0x400
    __IO uint32_t PHYCTL;  // 0x404 (v3s @ 0x410)
    __IO uint32_t PHYBIST; // 0x408
    __IO uint32_t PHYTUNE; // 0x40c
} USB_T;
#define USB ((USB_T *)USB_BASE)

typedef union PACKED
{
    uint32_t dat[2];
    struct
    {
        uint8_t bmRequestType;
        uint8_t bRequest;
        uint8_t wValue_l;
        uint8_t wValue_h;
        uint8_t wIndex_l;
        uint8_t wIndex_h;
        uint8_t wLength_l;
        uint8_t wLength_h;
    };
    struct
    {
        uint16_t wRequest;
        uint16_t wValue;
        uint16_t wIndex;
        uint16_t wLength;
    };
} SETUP_PACKET;

typedef struct PACKED
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t bcdUSB;
    uint8_t bDeviceClass;
    uint8_t bDeviceSubClass;
    uint8_t bDeviceProtocol;
    uint8_t bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t iManufacturer;
    uint8_t iProduct;
    uint8_t iSerialNumber;
    uint8_t bNumConfigurations;
} DSC_DEV;

typedef struct PACKED
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t bcdUSB;
    uint8_t bDeviceClass;
    uint8_t bDeviceSubClass;
    uint8_t bDeviceProtocol;
    uint8_t bMaxPacketSize0;
    uint8_t bNumConfigurations;
    uint8_t bReserved;
} DSC_QUAL;

typedef struct PACKED
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t wTotalLength;
    uint8_t bNumInterfaces;
    uint8_t bConfigurationValue;
    uint8_t iConfiguration;
    uint8_t bmAttributes;
    uint8_t bMaxPower;
} DSC_CFG;

typedef struct PACKED
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t wLANGID;
} DSC_LNID;

typedef struct PACKED
{
    uint8_t bLength;
	uint8_t bDescriptorType;
    uint8_t bFirstInterface;
	uint8_t bInterfaceCount;
	uint8_t bFunctionClass;
	uint8_t bFunctionSubClass;
	uint8_t bFunctionProtocol;
	uint8_t iFunction;
} DSC_IAD;

typedef struct PACKED
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bSubType;
    uint16_t bcdCDC;
} DSC_FUNC;

typedef struct PACKED
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bSubType;
    uint8_t bmCapabilities;
} DSC_FUNC_ACM;

typedef struct PACKED
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bSubType;
    uint8_t bControlInterface;
    uint8_t bSubordinateInterface0;
} DSC_FUNC_UNION;

typedef struct PACKED
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bSubType;
    uint8_t bmCapabilities;
    uint8_t bDataInterface;
} DSC_FUNC_CALL;

typedef struct PACKED
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bInterfaceNumber;
    uint8_t bAlternateSetting;
    uint8_t bNumEndpoints;
    uint8_t bInterfaceClass;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    uint8_t iInterface;
} DSC_IF;

typedef struct PACKED
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bEndpointAddress;
    uint8_t bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t bInterval;
} DSC_EP;
