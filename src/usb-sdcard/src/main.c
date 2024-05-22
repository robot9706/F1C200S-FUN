#include <stdint.h>
#include "io.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "system.h"
#include "arm32.h"
#include "f1c100s_uart.h"
#include "sdcard.h"
#include "f1c100s_gpio.h"
#include "f1c100s_clock.h"
#include "f1c100s_sdc.h"

static sdcard_t sdcard;

#define __I volatile const
#define __O volatile
#define __IO volatile

#define u32 uint32_t
#define u16 uint16_t
#define u8 uint8_t

#define PACKED __attribute__((packed))

/* PIO */
typedef struct
{
    __IO uint32_t CFG0; // 0x00 Configure Register 0
    __IO uint32_t CFG1; // 0x04 Configure Register 1
    __IO uint32_t CFG2; // 0x08 Configure Register 2
    __IO uint32_t CFG3; // 0x0C Configure Register 3
    __IO uint32_t DAT;  // 0x10 Data Register
    __IO uint32_t DRV0; // 0x14 Multi-Driving Register 0
    __IO uint32_t DRV1; // 0x18 Multi-Driving Register 1
    __IO uint32_t PUL0; // 0x1C Pull Register 0
    __IO uint32_t PUL1; // 0x20 Pull Register 1
} PIO_T;
#define PA ((PIO_T *)0x01C20800)
#define PB ((PIO_T *)0x01C20824)
#define PC ((PIO_T *)0x01C20848)
#define PD ((PIO_T *)0x01C2086C)
#define PE ((PIO_T *)0x01C20890)
#define PF ((PIO_T *)0x01C208B4)

/* CCU */
typedef struct
{
    __IO u32 PLL_CPU_CTRL; // 0x00 PLL_CPU Control Register
    __IO u32 RES0;
    __IO u32 PLL_AUDIO_CTRL; // 0x08 PLL_AUDIO Control Register
    __IO u32 RES1;
    __IO u32 PLL_VIDEO_CTRL; // 0x10 PLL_VIDEO Control Register
    __IO u32 RES2;
    __IO u32 PLL_VE_CTRL; // 0x18 PLL_VE Control Register
    __IO u32 RES3;
    __IO u32 PLL_DDR_CTRL; // 0x20 PLL_DDR Control Register
    __IO u32 RES4;
    __IO u32 PLL_PERIPH_CTRL; // 0x28 PLL_PERIPH Control Register
    __IO u32 RES5[9];
    __IO u32 CPU_CLK_SRC; // 0x50 CPU Clock Source Register
    __IO u32 AHB_APB_CFG; // 0x54 AHB/APB/HCLKC Configuration Register
    __IO u32 RES6[2];
    __IO u32 BUS_CLK_GATING0; // 0x60 Bus Clock Gating Register 0
    __IO u32 BUS_CLK_GATING1; // 0x64 Bus Clock Gating Register 1
    __IO u32 BUS_CLK_GATING2; // 0x68 Bus Clock Gating Register 2
    __IO u32 RES7[7];
    __IO u32 SDMMC0_CLK; // 0x88 SDMMC0 Clock Register
    __IO u32 SDMMC1_CLK; // 0x8C SDMMC1 Clock Register
    __IO u32 RES8[8];
    __IO u32 DAUDIO_CLK; // 0xB0 DAUDIO Clock Register
    __IO u32 OWA_CLK;    // 0xB4 OWA Clock Register
    __IO u32 CIR_CLK;    // 0xB8 CIR Clock Register
    __IO u32 RES9[4];
    __IO u32 USBPHY_CFG; // 0xCC USBPHY Clock Register
    __IO u32 RES10[12];
    __IO u32 DRAM_GATING; // 0x100 DRAM GATING Register
    __IO u32 BE_CLK;      // 0x104 BE Clock Register
    __IO u32 RES11;
    __IO u32 FE_CLK; // 0x10C FE Clock Register
    __IO u32 RES12[2];
    __IO u32 TCON_CLK; // 0x118 TCON Clock Register
    __IO u32 DI_CLK;   // 0x11C De-interlacer Clock Register
    __IO u32 TVE_CLK;  // 0x120 TVE Clock Register
    __IO u32 TVD_CLK;  // 0x124 TVD Clock Register
    __IO u32 RES13[3];
    __IO u32 CSI_CLK; // 0x134 CSI Clock Register
    __IO u32 RES14;
    __IO u32 VE_CLK;     // 0x13C VE Clock Register
    __IO u32 AC_DIG_CLK; // 0x140 Audio Codec Clock Register
    __IO u32 AVS_CLK;    // 0x144 AVS Clock Register
    __IO u32 RES15[46];
    __IO u32 PLL_STABLE0; // 0x200 PLL stable time 0
    __IO u32 PLL_STABLE1; // 0x204 PLL stable time 1
    __IO u32 RES16[6];
    __IO u32 PLL_CPU_BIAS;    // 0x220 PLL CPU Bias Register
    __IO u32 PLL_AUDIO_BIAS;  // 0x224 PLL Audio Bias Register
    __IO u32 PLL_VIDEO_BIAS;  // 0x228 PLL Video Bias Register
    __IO u32 PLL_VE_BIAS;     // 0x22c PLL VE Bias Register
    __IO u32 PLL_DDR_BIAS;    // 0x230 PLL DDR Bias Register
    __IO u32 PLL_PERIPH_BIAS; // 0x234 PLL PERIPH Bias Register
    __IO u32 RES17[6];
    __IO u32 PLL_CPU_TUNING; // 0x250 PLL CPU Tuning Register
    __IO u32 RES18[3];
    __IO u32 PLL_DDR_TUNING; // 0x260 PLL DDR Tuning Register
    __IO u32 RES19[8];
    __IO u32 PLL_AUDIO_PAT; // 0x284 PLL Audio Pattern Control Register
    __IO u32 PLL_VIDEO_PAT; // 0x288 PLL Video Pattern Control Register
    __IO u32 RES20;
    __IO u32 PLL_DDR_PAT; // 0x290 PLL DDR Pattern Control Register
    __IO u32 RES21[11];
    __IO u32 BUS_SOFT_RST0; // 0x2C0 Bus Software Reset Register 0
    __IO u32 BUS_SOFT_RST1; // 0x2C4 Bus Software Reset Register 1
    __IO u32 RES22[2];
    __IO u32 BUS_SOFT_RST2; // 0x2D0 Bus Software Reset Register 2
} CCU_T;
#define CCU ((CCU_T *)0x01C20000)

/* SYS */
typedef struct
{
    __IO u32 CTRL[16]; // 0x00 SRAM Control registers
} SYS_T;
#define SYS ((SYS_T *)0x01C00000)

/* MUSB */
typedef struct
{
    union
    { // 0x00..0x3F FIFO_0..FIFO_15 (8b,16b,32b access)
        __IO u32 word32;
        __IO u16 word16;
        __IO u8 byte;
    } FIFO[16];
    __IO u8 POWER;   // 0x40 Power management
    __IO u8 DEVCTL;  // 0x41 OTG device control
    __IO u8 EP_IDX;  // 0x42 Index for selecting endpoint
    __IO u8 VEND0;   // 0x43 Controls whether to use DMA mode
    __IO u32 EP_IS;  // 0x44 EP interrupt status (RX-high, TX-low)
    __IO u32 EP_IE;  // 0x48 EP interrupt enable (RX-high, TX-low)
    __IO u32 BUS_IS; // 0x4C USB interrupt status (1-byte)
    __IO u32 BUS_IE; // 0x50 USB interrupt enable (1-byte)
    __IO u32 FRAME;  // 0x54 USB frame number
    __IO u32 res0[10];
    // EP control regs (indexed)
    __IO u16 TXMAXP;    // 0x80
    __IO u16 TXCSR;     // 0x82
    __IO u16 RXMAXP;    // 0x84
    __IO u16 RXCSR;     // 0x86
    __IO u16 RXCOUNT;   // 0x88
    __IO u16 res1;      // 0x8A
    __IO u8 TXTYPE;     // 0x8C
    __IO u8 TXINTERVAL; // 0x8D
    __IO u8 RXTYPE;     // 0x8E
    __IO u8 RXINTERVAL; // 0x8F
    // FIFO control regs (indexed)
    __IO u16 TXFIFOSZ;   // 0x90 TX endpoint FIFO size (2 ^ (size + 3))
    __IO u16 TXFIFOADDR; // 0x92 TX endpoint FIFO address (offset / 8)
    __IO u16 RXFIFOSZ;   // 0x94 RX endpoint FIFO size (2 ^ (size + 3))
    __IO u16 RXFIFOADDR; // 0x96 RX endpoint FIFO address (offset / 8)
    // Multipoint / busctl regs (indexed)
    __IO u8 TXFUNCADDR; // 0x98 Function address register
    __IO u8 res4;       // 0x99
    __IO u8 TXHUBADDR;  // 0x9A
    __IO u8 TXHUBPORT;  // 0x9B
    __IO u8 RXFUNCADDR; // 0x9C
    __IO u8 res5;       // 0x9D
    __IO u8 RXHUBADDR;  // 0x9E
    __IO u8 RXHUBPORT;  // 0x9F
    __IO u8 res6[32];
    // configdata regs
    __IO u8 CONFIGDATA; // 0xc0
    __IO u8 res7[831];
    __IO u32 ISCR;    // 0x400
    __IO u32 PHYCTL;  // 0x404 (v3s @ 0x410)
    __IO u32 PHYBIST; // 0x408
    __IO u32 PHYTUNE; // 0x40c
} USB_T;
#define USB ((USB_T *)0x01C13000)

// ============================================================
// USB
// ============================================================
enum USB_MUX_STATE
{
    USB_MUX_DEVICE,
    USB_MUX_DISABLE,
    USB_MUX_HOST
};
enum USB_MUX_STATE usb_mux_state;
void usb_mux(enum USB_MUX_STATE i)
{
    PA->DAT = (PA->DAT & ~3) | (i & 3);
    PA->CFG0 = (PA->CFG0 & ~0xFF) | 0x11;
    usb_mux_state = i;
    sdelay(10);
}

void usb_deinit(void)
{
    CCU->USBPHY_CFG &= ~3;
    CCU->BUS_CLK_GATING0 &= ~(1 << 24);
    CCU->BUS_SOFT_RST0 &= ~(1 << 24);
    usb_mux(USB_MUX_DISABLE);
}

static void phy_write(u8 addr, u8 data, u8 len)
{
    for (u32 i = 0; i < len; i++)
    {
        USB->PHYCTL = (USB->PHYCTL & 0xFFFF0000) | (addr + i) * 256 | (data & 1) * 128;
        USB->PHYCTL |= 1;
        USB->PHYCTL &= ~1;
        data >>= 1;
    }
}

void usbd_init(void)
{
    printf("USB: MUX\r\n");
    usb_mux(USB_MUX_DEVICE);

    printf("USB: CCU\r\n");
    CCU->USBPHY_CFG |= 3;
    CCU->BUS_CLK_GATING0 |= (1 << 24);
    CCU->BUS_SOFT_RST0 &= ~(1 << 24);
    CCU->BUS_SOFT_RST0 |= (1 << 24);

    printf("USB: PHY\r\n");
    phy_write(0x0c, 0x01, 1); // RES45_CAL_EN -> Enable
    phy_write(0x20, 0x14, 5); // SET_TX_AMPLITUDE_TUNE & SET_TX_SLEWRATE_TUNE
    phy_write(0x2A, 0x03, 2); // SET_DISCON_DET_THRESHOLD

    printf("USB: SYS");
    SYS->CTRL[1] |= 1;

    printf("USB: REGS\r\n");
    USB->ISCR = (USB->ISCR & ~0x70) | (0x3F << 12);
    USB->VEND0 = 0;
    USB->BUS_IE = 0;
    USB->BUS_IS = 0xFF;
    USB->EP_IE = 0;
    USB->EP_IS = 0xFFFFFFFF;
    USB->POWER &= ~((1 << 7) | (1 << 6));
    USB->POWER |= ((1 << 6) | (1 << 5));
}

#define EP_BULK_IN 1
#define EP_BULK_OUT 1

typedef union PACKED
{
    u32 dat[2];
    struct
    {
        u8 bmRequestType;
        u8 bRequest;
        u8 wValue_l;
        u8 wValue_h;
        u8 wIndex_l;
        u8 wIndex_h;
        u8 wLength_l;
        u8 wLength_h;
    };
    struct
    {
        u16 wRequest;
        u16 wValue;
        u16 wIndex;
        u16 wLength;
    };
} SETUP_PACKET;
static SETUP_PACKET setup;

typedef struct PACKED
{
    u8 bLength;
    u8 bDescriptorType;
    u16 bcdUSB;
    u8 bDeviceClass;
    u8 bDeviceSubClass;
    u8 bDeviceProtocol;
    u8 bMaxPacketSize0;
    u16 idVendor;
    u16 idProduct;
    u16 bcdDevice;
    u8 iManufacturer;
    u8 iProduct;
    u8 iSerialNumber;
    u8 bNumConfigurations;
} DSC_DEV;

typedef struct PACKED
{
    u8 bLength;
    u8 bDescriptorType;
    u16 bcdUSB;
    u8 bDeviceClass;
    u8 bDeviceSubClass;
    u8 bDeviceProtocol;
    u8 bMaxPacketSize0;
    u8 bNumConfigurations;
    u8 bReserved;
} DSC_QUAL;

typedef struct PACKED
{
    u8 bLength;
    u8 bDescriptorType;
    u16 wTotalLength;
    u8 bNumInterfaces;
    u8 bConfigurationValue;
    u8 iConfiguration;
    u8 bmAttributes;
    u8 bMaxPower;
} DSC_CFG;

typedef struct PACKED
{
    u8 bLength;
    u8 bDescriptorType;
    u8 bInterfaceNumber;
    u8 bAlternateSetting;
    u8 bNumEndpoints;
    u8 bInterfaceClass;
    u8 bInterfaceSubClass;
    u8 bInterfaceProtocol;
    u8 iInterface;
} DSC_IF;

typedef struct PACKED
{
    u8 bLength;
    u8 bDescriptorType;
    u8 bEndpointAddress;
    u8 bmAttributes;
    u16 wMaxPacketSize;
    u8 bInterval;
} DSC_EP;

typedef struct PACKED
{
    u8 bLength;
    u8 bDescriptorType;
    u16 wLANGID;
} DSC_LNID;

static struct
{
    DSC_DEV dev;
    DSC_QUAL qual;
    DSC_CFG cfg;
    DSC_IF itf;
    DSC_EP ep1;
    DSC_EP ep2;
    DSC_LNID str0;
} dsc = {
    {
        /* Device Descriptor */
        sizeof(DSC_DEV), // bLength
        1,               // bDescriptorType
        0x0200,          // bcdUSB
        0,               // bDeviceClass
        0,               // bDeviceSubClass
        0,               // bDeviceProtocol
        64,              // bMaxPacketSize0
        0x1111,          // idVendor
        0x0000,          // idProduct
        0x0100,          // bcdDevice
        1,               // iManufacturer
        2,               // iProduct
        3,               // iSerialNumber
        1                // bNumConfigurations
    },
    {
        /* Qualifier Descriptor */
        sizeof(DSC_QUAL), // bLength
        6,                // bDescriptorType
        0x0200,           // bcdUSB
        0,                // bDeviceClass
        0,                // bDeviceSubClass
        0,                // bDeviceProtocol
        64,               // bMaxPacketSize0
        1,                // bNumConfigurations
        0                 // bReserved
    },
    {
        /* Configuration Descriptor */
        sizeof(DSC_CFG),  // bLength
        2,                // bDescriptorType
        sizeof(DSC_CFG) + // wTotalLength
            sizeof(DSC_IF) +
            sizeof(DSC_EP) +
            sizeof(DSC_EP),
        1,      // bNumInterfaces
        1,      // bConfigurationValue
        0,      // iConfiguration
        128,    // bmAttributes:        Bus powered
        100 / 2 // bMaxPower:           100 mA
    },
    {
        /*  Interface Descriptor */
        sizeof(DSC_IF), // bLength
        4,              // bDescriptorType
        0,              // bInterfaceNumber
        0,              // bAlternateSetting
        2,              // bNumEndpoints
        8,              // bInterfaceClass      Mass Storage Class
        6,              // bInterfaceSubClass   SCSI
        80,             // bInterfaceProtocol   Bulk Only Protocol
        0               // iInterface
    },
    {
        /* Endpoint 1 Descriptor */
        sizeof(DSC_EP),   // bLength
        5,                // bDescriptorType
        128 | EP_BULK_IN, // bEndpointAddress:    In
        2,                // bmAttributes:        Bulk
        64,               // wMaxPacketSize
        0                 // bInterval
    },
    {
        /* Endpoint 2 Descriptor */
        sizeof(DSC_EP),  // bLength
        5,               // bDescriptorType:     Out
        0 | EP_BULK_OUT, // bEndpointAddress:    Bulk
        2,               // bmAttributes
        64,              // wMaxPacketSize
        0                // bInterval
    },
    {
        /* String Descriptor 0 */
        sizeof(DSC_LNID), // bLength
        3,                // bDescriptorType
        0x0409            // wLANGID:             English (US)
    }};

static u8 USB_Config;

static u8 vendor[] = "MiniLogic";
static u8 device[] = "USB Mass Storage Device";
static u8 serial[] = "000000000000";

static void ep0_send_buf(void *buf, u16 len)
{
    u16 ctr;
    u8 *ptr = buf;
    USB->TXCSR = 0x40; // Serviced RxPktRdy
    while (len)
    {
        while (USB->TXCSR & 2)
            ;
        ctr = len < 64 ? len : 64;
        len -= ctr;
        do
            USB->FIFO[0].byte = *ptr++;
        while (--ctr);
        USB->TXCSR = 2; // TxPktRdy
    }
    USB->TXCSR = 8; // DataEnd
}

static void ep0_send_dsc(void *ptr)
{
    u8 *dsc = ptr;
    u16 len = setup.wLength, dlen = dsc[0];
    if (dsc[1] == 2)
        dlen = dsc[2];
    if (dlen < len)
        len = dlen;
    ep0_send_buf(dsc, len);
}

static void ep0_send_str(void *ptr)
{
    u8 *dsc = ptr;
    u16 dlen, len = setup.wLength;
    for (dlen = 0; dsc[dlen]; dlen++)
        ;
    dlen = dlen * 2 + 2;
    if (dlen < len)
        len = dlen;
    USB->TXCSR = 0x40; // Serviced RxPktRdy
    USB->FIFO[0].word16 = 0x0300 | dlen;
    while (len -= 2)
        USB->FIFO[0].word16 = *dsc++;
    USB->TXCSR = 0x0A; // TxPktRdy | DataEnd
}

static void ep0_handler(void)
{
    USB->EP_IDX = 0;
    u16 csr = USB->TXCSR;
    if (csr & 8)
    {
        USB->TXCSR = csr & ~8;
        return;
    } // Status stage is not complete?
    if (csr & 4)
        USB->TXCSR = csr & ~4; // EP0 Sent Stall
    if (csr & 16)
        USB->TXCSR = 0x80; // EP0 Serviced Setup End
    if (csr & 1)           // EP0 RxPkt Ready
    {
        printf("EP0 \r\n");
        if (USB->RXCOUNT != 8)
        {
            printf("Invalid Packet Length \r\n");
            while (1)
                ; // Wait for watchdog!
        }
        printf("(%d) \r\n", USB->RXCOUNT);
        setup.dat[0] = USB->FIFO[0].word32;
        setup.dat[1] = USB->FIFO[0].word32;
        printf("%08x %08x \r\n", setup.dat[0], setup.dat[1]);
        if (setup.wRequest == 0xFEA1)
        {
            USB->TXCSR = 0x40; // Serviced RxPktRdy
            USB->FIFO[0].byte = 0;
            USB->TXCSR = 0x0A; // TxPktRdy | DataEnd
            printf("Get Max LUN\r\n");
        }
        else if (setup.wRequest == 0x0500)
        {
            dsc.ep1.wMaxPacketSize = dsc.ep2.wMaxPacketSize = USB->POWER & 16 ? 512 : 64;
            setup.wValue_l &= 127;
            USB->TXCSR = 0x48;
            while (USB->TXCSR & 0x08)
                ;
            USB->TXFUNCADDR = setup.wValue_l;
            printf("Set Addr(%d) %cS-mode\n", setup.wValue, dsc.ep1.wMaxPacketSize > 64 ? 'H' : 'F');
        }
        else if (setup.wRequest == 0x0900)
        {
            USB_Config = setup.wValue_l;
            // if(USB_Config)
            {
                USB->EP_IDX = EP_BULK_IN; // in ep: device -> host
                USB->TXFIFOSZ = 6;        // 2^(size + 3): 2^(6+3)=512
                USB->TXFIFOADDR = 64 / 8; // Offset(addr * 8)
                USB->TXMAXP = dsc.ep1.wMaxPacketSize;
                USB->TXCSR = 0x2048; // fifo flush, clr data toggle, auto set, mode in
#if EP_BULK_IN != EP_BULK_OUT
                USB->EP_IDX = EP_BULK_OUT; // out ep: host -> device
#endif
                USB->RXFIFOSZ = 6;                // 2^(size + 3): 2^(6+3)=512
                USB->RXFIFOADDR = (512 + 64) / 8; // Offset(addr * 8)
                USB->RXMAXP = dsc.ep2.wMaxPacketSize;
                USB->RXCSR = 0x0090; // fifo flush, clr data toggle, auto clr, ?
                USB->EP_IDX = 0;
                USB->TXCSR = 0x48; // Serviced RxPktRdy | DataEnd
                                   // USB->EP_IE = 0xFFFFFFFF;
                // USB->EP_TX_IE = 0xFFFF;
                // USB->EP_RX_IE = 0xFFFF;
            }
            // else
            //{
            // }
            printf("Set Cfg(%d)\n", setup.wValue_l);
        }
        else if (setup.wRequest == 0x0880)
        {
            USB->TXCSR = 0x40; // Serviced RxPktRdy
            USB->FIFO[0].byte = USB_Config;
            USB->TXCSR = 0x0A; // TxPktRdy | DataEnd
            printf("Get Cfg(%d)\n", USB_Config);
        }
        else
        {
            if (setup.wRequest == 0x0680)
            {
                if (setup.wValue_h == 1)
                {
                    printf("Get Dev Dsc\r\n");
                    ep0_send_dsc(&dsc.dev);
                    return;
                }
                else if (setup.wValue_h == 2)
                {
                    printf("Get Cfg Dsc\r\n");
                    ep0_send_dsc(&dsc.cfg);
                    return;
                }
                else if (setup.wValue_h == 3)
                {
                    printf("Get Str_%u Dsc\n", setup.wValue_l);
                    if (setup.wValue_l == 0)
                    {
                        ep0_send_dsc(&dsc.str0);
                        return;
                    }
                    else if (setup.wValue_l == 1)
                    {
                        ep0_send_str(vendor);
                        return;
                    }
                    else if (setup.wValue_l == 2)
                    {
                        ep0_send_str(device);
                        return;
                    }
                    else if (setup.wValue_l == 3)
                    {
                        ep0_send_str(serial);
                        return;
                    }
                }
                else if (setup.wValue_h == 6)
                {
                    printf("Get Qual Dsc\r\n");
                    ep0_send_dsc(&dsc.qual);
                    return;
                }
            }
            printf("Invalid Request\r\n");
            USB->TXCSR = 0x60; // Serviced RxPktRdy | SendStall
        }
    }
}

static u32 cbw_tag, cbw_len, cbw_cmd, cbw_addr, bulk_len, bulk_idx;

/* Read Format Capacity Response */
typedef struct PACKED
{
    u32 list_len;  // upper byte must be 8*n (length of formattable capacity descriptor)
    u32 block_num; // Number of Logical Blocks
    u16 dsc_type;  // 0-reserved, 1-unformatted media, 2-formatted media, 3-no media present
    u16 block_size;
} RD_CAPACITIES_RES;

/* Read Capacity(10) Response */
typedef struct PACKED
{
    u32 last_lba;   // The last Logical Block Address of the device
    u32 block_size; // Block size in bytes
} RD_CAPACITY_RES;

/* Request Sense Response */
typedef struct PACKED
{
    u8 res_code;
    u8 rsv1;
    u8 sense_key;
    u32 info;
    u8 sense_len;
    u32 cmd_info;
    u8 sense_code;
    u8 sense_qualifier;
    u8 unit_code;
    u8 sense_key_spec[3];
} REQUEST_SENSE_RES;

/* Mode Sense(6) Response */
typedef struct PACKED
{
    u8 data_len;
    u8 medium_type;
    u8 param;
    u8 bdsc_len;
} MODE_SENSE6_RES;

static union
{
    u32 dat[65536 / 4];
    RD_CAPACITIES_RES res_fmt_cap;
    RD_CAPACITY_RES res_cap;
    REQUEST_SENSE_RES res_reqsense;
    MODE_SENSE6_RES res_sense6;
} buf;

#define CBW_SIGNATURE 0x43425355
#define CSW_SIGNATURE 0x53425355

/* SCSI Command Operation Code */
#define TEST_UNIT_READY 0x00
#define REQUEST_SENSE 0x03
#define INQUIRY 0x12
#define MODE_SELECT6 0x15
#define MODE_SENSE6 0x1A
#define START_STOP_UNIT 0x1B
#define MEDIUM_REMOVAL 0x1E
#define RD_CAPACITIES 0x23
#define RD_CAPACITY 0x25
#define RD10 0x28
#define WR10 0x2A

/* Inquiry Response */
typedef struct PACKED
{
    u8 type;
    u8 rmb;
    u8 version;
    u8 format;
    u8 length;
    u8 sccs;
    u8 rsv1;
    u8 rsv2;
    u8 vid[8];
    u8 pid[16];
    u8 rev[4];
} INQUIRY_RES;

/* USB Mass Storage Responses */
INQUIRY_RES inq = {
    0,      // Connected direct access block device
    128,    // Device is removable
    0,      // Version is not claim conformance to any standard
    2,      // Response Data Format
    36 - 5, // Additional Length
    128,    // SCSI target device contains an embedded storage array controller
    0,      // Reserved
    0,      // Reserved
            // Vendor ID (8 bytes)
    {'F', '1', 'C', '1', '0', '0', 'S', ' '},
    // Product ID (16 bytes)
    {'S', 'D', 'H', 'C', ' ', 'C', 'a', 'r', 'd', ' ', 'R', 'e', 'a', 'd', 'e', 'r'},
    // Product Revision Level (4 bytes)
    {'0', '0', '0', '1'}};

void ums_csw(void)
{
#if EP_BULK_IN != EP_BULK_OUT
    USB->EP_IDX = EP_BULK_IN;
#endif
    while (USB->TXCSR & 3)
        ;
    USB->FIFO[EP_BULK_IN].word32 = 0x53425355; // 'USBS'
    USB->FIFO[EP_BULK_IN].word32 = cbw_tag;
    USB->FIFO[EP_BULK_IN].word32 = cbw_len;
    USB->FIFO[EP_BULK_IN].byte = 0; // bCSWStatus
    USB->TXCSR |= 1;                // TxPktRdy
}

static void bulk_init(void)
{
    bulk_idx = 0;
    bulk_len = cbw_len > sizeof(buf.dat) ? sizeof(buf.dat) : cbw_len;
    cbw_len -= bulk_len;
}

void bulk_out_handler(void)
{
    u8 *ptr;
    u32 i;
    USB->EP_IDX = EP_BULK_OUT;
    if (cbw_cmd == 0x2A)
    {
        i = USB->RXCOUNT / 4;
        while (i--)
            buf.dat[bulk_idx++] = USB->FIFO[EP_BULK_OUT].word32;
        USB->RXCSR &= ~1; // RxPktRdy
        if (bulk_idx == bulk_len / 4)
        {
            sdcard_write(&sdcard, (uint8_t *)&buf.dat[0], cbw_addr, bulk_len / 512);
            cbw_addr += bulk_len / 512;
            if (cbw_len)
                bulk_init();
            else
            {
                cbw_cmd = 0;
                ums_csw();
            }
        }
    }
    else if (USB->RXCOUNT != 31 || USB->FIFO[EP_BULK_OUT].word32 != CBW_SIGNATURE)
    {
        printf("mInvalid CBW\r\n");
        while (1)
            ; // Wait for watchdog!
    }
    else
    {
        cbw_tag = USB->FIFO[EP_BULK_OUT].word32;       // dCBWTag
        cbw_len = USB->FIFO[EP_BULK_OUT].word32;       // dCBWDataTransferLength
        cbw_cmd = USB->FIFO[EP_BULK_OUT].word32 >> 24; // CBWCB, bCBWCBLength, bCBWLUN, bmCBWFlags
        cbw_addr = USB->FIFO[EP_BULK_OUT].word32 >> 8;
        cbw_addr = __builtin_bswap32(cbw_addr | (USB->FIFO[EP_BULK_OUT].word32 << 24));
        USB->FIFO[EP_BULK_OUT].word32;
        USB->FIFO[EP_BULK_OUT].word16;
        USB->FIFO[EP_BULK_OUT].byte;
        USB->RXCSR &= ~1; // RxPktRdy
        printf("EP%d CMD %08X\r\n", EP_BULK_OUT, cbw_cmd);
        if (cbw_cmd == WR10)
        {
            printf("WR10 0x%08lX %lu\r\n", cbw_addr, cbw_len);
            bulk_init();
        }
        else if (cbw_cmd == RD10)
        {
            printf("RD10 0x%08lX %lu\r\n", cbw_addr, cbw_len);
#if EP_BULK_IN != EP_BULK_OUT
            USB->EP_IDX = EP_BULK_IN;
#endif
            do
            {
                bulk_init();
                sdcard_read(&sdcard, (uint8_t *)&buf.dat[0], cbw_addr, bulk_len / 512);
                cbw_addr += bulk_len / 512;
                for (bulk_len /= dsc.ep2.wMaxPacketSize; bulk_len; bulk_len--)
                {
                    for (i = 0; i < dsc.ep2.wMaxPacketSize; i += 4)
                        USB->FIFO[EP_BULK_IN].word32 = buf.dat[bulk_idx++];
                    for (USB->TXCSR |= 1; USB->TXCSR & 3;)
                    {
                    };
                }
            } while (cbw_len);
            ums_csw();
        }
        else
        {
            i = 0;
            ptr = (u8 *)&buf;
            memset(buf.dat, 0, 512);
            switch (cbw_cmd)
            {
            case INQUIRY:
                printf("Inquiry\r\n");
                i = sizeof(inq);
                ptr = (u8 *)&inq;
                break;
            case RD_CAPACITIES:
                printf("Read Format Capacity\r\n");
                uint32_t blocks = (uint32_t)sdcard.blk_cnt; // Downcast ?

                buf.res_fmt_cap.list_len = 8 << 24;
                buf.res_fmt_cap.block_num = __builtin_bswap32(blocks);
                buf.res_fmt_cap.dsc_type = 0x0002;
                buf.res_fmt_cap.block_size = 0x0002;
                i = sizeof(buf.res_fmt_cap);
                break;
            case RD_CAPACITY:
                printf("Read Capacity\r\n");
                uint32_t lba = (uint32_t)sdcard.blk_cnt; // Downcast ?

                buf.res_cap.last_lba = __builtin_bswap32(lba - 1);
                buf.res_cap.block_size = 0x00020000;
                i = sizeof(buf.res_cap);
                break;
            case TEST_UNIT_READY:
                printf("Test Unit Ready\r\n");
                break;
            case 0x1E:
                printf("Pre Allow Medium Removal\r\n");
                break;
            case MODE_SENSE6:
                printf("Mode Sense(6)\r\n");
                buf.res_sense6.data_len = 3;
                i = sizeof(buf.res_sense6);
                break;
            case REQUEST_SENSE:
                printf("Request Sense\r\n");
                buf.res_reqsense.res_code = 0x70;
                i = sizeof(buf.res_reqsense);
                break;
            default:
                printf("Command Not Support\r\n");
                USB->EP_IDX = EP_BULK_IN;
                USB->TXCSR = 0x10; // SendStall
                return;
            }
            if (i)
            {
                cbw_len -= i;
#if EP_BULK_IN != EP_BULK_OUT
                USB->EP_IDX = EP_BULK_IN;
#endif
                do
                {
                    USB->FIFO[EP_BULK_IN].byte = *ptr++;
                } while (--i);
                USB->TXCSR |= 1; // TxPktRdy
            }
            ums_csw();
        }
    }
}

static void usbd_handler()
{
    u32 isr;
    isr = USB->BUS_IS;
    USB->BUS_IS = isr;

    // bus events
    if (isr & 1)
    {
        printf("USB Suspend\r\n");
        // USB->BUS_IS |= 1;
    }
    if (isr & 2)
    {
        printf("USB Resume\r\n");
        // USB->BUS_IS |= 2;
    }
    if (isr & 4)
    {
        printf("USB Reset\r\n");
        // USB->BUS_IS = 0xFF;
        USB->EP_IS = 0xFFFFFFFF;
        USB->EP_IDX = 0;
        USB->TXFUNCADDR = 0;
    }
    isr = USB->EP_IS;
    USB->EP_IS = isr;

    if (isr & 1)
    {
        ep0_handler();
    }
    if (isr & (1 << (EP_BULK_OUT + 16)))
    {
        bulk_out_handler();
    }
}

void dev_enable(int state)
{
    PE->CFG0 = (PE->CFG0 & 0xFF0FF0FF) | 0x00000100; // PE2:DEV_EN,PE5:SWITCH_STAT
    PE->PUL0 = (PE->PUL0 & ~(3 << 10)) | (1 << 10);  // PE5:switch state(pull-up)
    if (state)
        PE->DAT |= 4;
    else
        PE->DAT &= ~4;
}

int main(void)
{
    system_init();            // Initialize clocks, mmu, cache, uart, ...
    arm32_interrupt_enable(); // Enable interrupts

    // Init MMC interface
    clk_reset_set(CCU_BUS_SOFT_RST0, 8);
    clk_enable(CCU_BUS_CLK_GATE0, 8);
    clk_reset_clear(CCU_BUS_SOFT_RST0, 8);

    gpio_init(GPIOF, PIN1 | PIN2 | PIN3, GPIO_MODE_AF2, GPIO_PULL_NONE, GPIO_DRV_3);

    sdcard.sdc_base = SDC0_BASE;
    sdcard.voltage = MMC_VDD_27_36;
    sdcard.width = MMC_BUS_WIDTH_1;
    sdcard.clock = 50000000;

    // Log some stuff
    printf("PLL_CPU: %ld\n", clk_pll_get_freq(PLL_CPU));
    printf("PLL_AUDIO: %ld\n", clk_pll_get_freq(PLL_AUDIO));
    printf("PLL_VIDEO: %ld\n", clk_pll_get_freq(PLL_VIDEO));
    printf("PLL_VE: %ld\n", clk_pll_get_freq(PLL_VE));
    printf("PLL_DDR: %ld\n", clk_pll_get_freq(PLL_DDR));
    printf("PLL_PERIPH: %ld\n", clk_pll_get_freq(PLL_PERIPH));
    printf("\n");
    printf("CPU: %ld\n", clk_cpu_get_freq());
    printf("HCLK: %ld\n", clk_hclk_get_freq());
    printf("AHB: %ld\n", clk_ahb_get_freq());
    printf("APB: %ld\n", clk_apb_get_freq());
    printf("\n");

    while (1)
    {
        if (sdcard_detect(&sdcard))
        {
            printf("SD card detected\n");

            printf("Version: %lu\n", sdcard.version);
            printf("High capacity: %lu\n", sdcard.high_capacity);
            printf("Read block length: %lu\n", sdcard.read_bl_len);
            printf("Write block length: %lu\n", sdcard.write_bl_len);
            printf("Block count: %llu\n", sdcard.blk_cnt);

            printf("Init USB mux\n");
            usb_mux(USB_MUX_DEVICE);
            usbd_init();

            while (sdcard_status(&sdcard))
            {
                usbd_handler();
            }

            printf("USB deinit\n");
            usb_deinit();
        }
        else
        {
            printf("SD card not detected\n");
            while (!sdcard_detect(&sdcard))
            {
                sdelay(5000);
            }
            printf("yissss\n");
        }
    }
    return 0;
}
