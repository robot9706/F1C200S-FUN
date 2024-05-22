#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

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

enum USB_MUX_STATE
{
    USB_MUX_DEVICE,
    USB_MUX_DISABLE,
    USB_MUX_HOST
};

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

typedef void (*usbm_sector_callback)(uint8_t* buffer, uint32_t blockIndex, uint32_t numBlocks);

void usb_mux(enum USB_MUX_STATE i);
void usb_deinit(void);
void usbd_init(uint32_t numBlocks, usbm_sector_callback readSector, usbm_sector_callback writeSector);
void usbd_handler(void);

#ifdef __cplusplus
}
#endif
