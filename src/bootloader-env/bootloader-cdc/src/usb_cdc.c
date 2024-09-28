#include <string.h>
#include "f1c100s_clock.h"
#include "usb_cdc.h"
#include "ringbuffer.h"
#include <stdio.h>

#define TX_FIFOSZ_BYTES 512
#define TX_FIFOSZ 6

static DSC_DEV deviceDescriptor = {
    sizeof(DSC_DEV), // bLength
    1,               // bDescriptorType = device
    0x0200,          // bcdUSB
    0,               // bDeviceClass
    0,               // bDeviceSubClass
    0,               // bDeviceProtocol
    64,              // bMaxPacketSize0
    0x1EAF,          // idVendor - VID
    0x0024,          // idProduct - PID
    0x0100,          // bcdDevice
    1,               // iManufacturer
    2,               // iProduct
    3,               // iSerialNumber
    1                // bNumConfigurations
};

static DSC_QUAL qualifierDescriptor = {
    sizeof(DSC_QUAL), // bLength
    6,                // bDescriptorType
    0x0200,           // bcdUSB
    0,                // bDeviceClass
    0,                // bDeviceSubClass
    0,                // bDeviceProtocol
    64,               // bMaxPacketSize0
    1,                // bNumConfigurations
    0                 // bReserved
};

static struct
{
    DSC_CFG config;

    DSC_IAD iad;

    DSC_IF managementInterface;
    DSC_FUNC managementFunc;
    DSC_FUNC_ACM managementACM;
    DSC_FUNC_UNION managementUnion;
    DSC_FUNC_CALL managementCall;
    DSC_EP managementEndpoint;

    DSC_IF dataInterface;
    DSC_EP dataInEndpoint;
    DSC_EP dataOutEndpoint;
} configDescriptor = {
    // Configuration descriptor
    {
        sizeof(DSC_CFG), // bLength
        2,               // bDescriptorType
        sizeof(DSC_CFG) + sizeof(DSC_IAD) +
            sizeof(DSC_IF) + sizeof(DSC_FUNC) + sizeof(DSC_FUNC_ACM) + sizeof(DSC_FUNC_UNION) + sizeof(DSC_FUNC_CALL) + sizeof(DSC_EP) +
            sizeof(DSC_IF) + sizeof(DSC_EP) + sizeof(DSC_EP), // wTotalLength
        2,                                                    // bNumInterfaces
        1,                                                    // bConfigurationValue
        0,                                                    // iConfiguration
        0x80,                                                 // bmAttributes:        Bus powered
        100 / 2                                               // bMaxPower:           100 mA
    },

    // Interface association descriptor
    {
        sizeof(DSC_IAD), // bLength
        0x0B, // bDescriptorType
        0x00, // bFirstInterface
        0x02, // bInterfaceCount
        0x02, // bFunctionClass
        0x02, // bFunctionSubClass
        0x01, // bFunctionProtocol
        0x02, // iFunction
    },

    // Management interface
    {
        sizeof(DSC_IF), // bLength
        4,              // bDescriptorType
        0,              // bInterfaceNumber
        0,              // bAlternateSetting
        1,              // bNumEndpoints
        0x02,           // bInterfaceClass      CDC
        0x02,           // bInterfaceSubClass   CDC ?
        0x01,           // bInterfaceProtocol   ?????
        0               // iInterface
    },

    // Header Functional Descriptor
    {
        sizeof(DSC_FUNC), // bLength
        0x24,             // bDescriptorType,
        0x00,             // bSubType = Header Functional Descriptor
        0x0110,           // bcdCDC
    },

    // Abstract Control Model Functional Descriptor
    {
        sizeof(DSC_FUNC_ACM), // bLength
        0x24,                 // bDescriptorType
        0x02,                 // bSubType = ACM FD
        0x02,                 // bmCapabilities
    },

    // Union Functional Descriptor
    {
        sizeof(DSC_FUNC_UNION), // bLength
        0x24,                   // bDescriptorType
        0x06,                   // bSubType = Union FD
        0x00,                   // bControlInterface = Interface number of Management interface
        0x01,                   // bSubordinateInterface0 = Interface number of Data interface
    },

    // Call Management Functional Descriptor
    {
        sizeof(DSC_FUNC_CALL),
        0x24, // bDescriptorType
        0x01, // bSubType = Call management FD
        0x00, // bmCapabilities
        0x01, // bDataInterface = Interface number of Data interface
    },

    // Management endpoint
    {
        sizeof(DSC_EP), // bLength
        5,              // bDescriptorType
        0x81,           // bEndpointAddress:    EP1 & In (Device -> Host)
        0x03,           // bmAttributes:        Interrupt
        16,             // wMaxPacketSize
        0xFF            // bInterval
    },

    // Data interface
    {
        sizeof(DSC_IF), // bLength
        4,              // bDescriptorType
        1,              // bInterfaceNumber
        0,              // bAlternateSetting
        2,              // bNumEndpoints
        0x0A,           // bInterfaceClass      CDC Data
        0x00,           // bInterfaceSubClass   None
        0x00,           // bInterfaceProtocol   None
        0               // iInterface
    },

    // Data in endpoint
    {
        sizeof(DSC_EP), // bLength
        5,              // bDescriptorType
        0x82,           // bEndpointAddress:    EP2 & In (Device -> Host)
        2,              // bmAttributes:        Bulk
        64,             // wMaxPacketSize
        0               // bInterval
    },

    // Data out endpoint
    {
        sizeof(DSC_EP), // bLength
        5,              // bDescriptorType
        0x03,           // bEndpointAddress:    EP3 & Out (Host -> Device)
        2,              // bmAttributes:        Bulk
        64,             // wMaxPacketSize
        0               // bInterval
    }};

static DSC_LNID languageDescriptor = {
    sizeof(DSC_LNID), // bLength
    3,                // bDescriptorType
    0x0409            // wLANGID:             English (US)
};

#define NUM_STRINGS 3
static uint8_t vendorString[] = "Vendor";
static uint8_t nameString[] = "F1C200s CDC";
static uint8_t serialString[] = "000000000000";
static uint8_t *stringDescriptors[NUM_STRINGS] = {
    vendorString,
    nameString,
    serialString,
};

static uint8_t deviceAddress;
static uint8_t deviceConfiguration;

static CDC_LINECODING lineCoding = {0};

#define BUFFERS_SIZE 512

static uint8_t rxBufferData[BUFFERS_SIZE] = {0};
static uint8_t txBufferData[BUFFERS_SIZE] = {0};

static RINGBUFFER rxBuffer = {
    0,
    0,
    rxBufferData,
    BUFFERS_SIZE,
};

static RINGBUFFER txBuffer = {
    0,
    0,
    txBufferData,
    BUFFERS_SIZE,
};

static int txInProgress = 0;

static void phy_write(uint8_t addr, uint8_t data, uint8_t len)
{
    for (uint32_t i = 0; i < len; i++)
    {
        USB->PHYCTL = (USB->PHYCTL & 0xFFFF0000) | (addr + i) * 256 | (data & 1) * 128;
        USB->PHYCTL |= 1;
        USB->PHYCTL &= ~1;
        data >>= 1;
    }
}

static void ep0_send_buf(void *buf, uint16_t len)
{
    uint16_t ctr;
    uint8_t *ptr = buf;
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

static void ep0_send_dsc(void *ptr, uint16_t dataLength, uint16_t maxLength)
{
    ep0_send_buf(ptr, dataLength > maxLength ? maxLength : dataLength);
}

static void ep0_send_str(void *ptr, uint16_t descLength)
{
    uint8_t *dsc = ptr;
    uint16_t dlen, len = descLength;
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

void cdc_init()
{
    clk_usb_config(1, 0);              // Clock ON, disable reset
    clk_enable(CCU_BUS_CLK_GATE0, 24); // Enable clock

    // Reset
    clk_reset_set(CCU_BUS_SOFT_RST0, 24);
    clk_reset_clear(CCU_BUS_SOFT_RST0, 24);

    // Setup PHY
    phy_write(0x0c, 0x01, 1); // RES45_CAL_EN -> Enable
    phy_write(0x20, 0x14, 5); // SET_TX_AMPLITUDE_TUNE & SET_TX_SLEWRATE_TUNE
    phy_write(0x2A, 0x03, 2); // SET_DISCON_DET_THRESHOLD

    // Usb thingy
    USB->ISCR = (USB->ISCR & ~0x70) | (0x3F << 12); // FORCE_VBUS_VALID to HIGH, FORCE_ID to HIGH, DPDM_PULLUP_EN enable, ID_PULLUP_EN enable
    USB->VEND0 = 0;
    USB->BUS_IE = 0xFF; // enable all
    USB->BUS_IS = 0xFF; // Clear interrupts
    USB->EP_IE = 0;
    USB->EP_IS = 0xFFFFFFFF;              // Clear endpoint iterrupts
    USB->POWER &= ~((1 << 7) | (1 << 6)); // Disable ISO update & soft connect
    USB->POWER |= ((1 << 6) | (1 << 5));  // Enable soft connect & high speed mode
}

void cdc_deinit()
{
    clk_usb_config(0, 1); // CLock off, assert reset

    clk_disable(CCU_BUS_CLK_GATE0, 24); // Assert clock gate

    clk_reset_set(CCU_BUS_SOFT_RST0, 24); // Assert USB OTG Reset
}

static void handle_ep0()
{
    USB->EP_IDX = 0; // Select endpoint 0

    uint16_t csr = USB->TXCSR; // EP0 -> TXCSR = CSR0

    if (csr & 8) // DataEnd set?
    {
        printf("DataEnd\n");
        USB->TXCSR = csr & ~8;
        return;
    }

    if (csr & 4) // Stall?
    {
        USB->TXCSR = csr & ~4; // EP0 Sent Stall
    }

    if (csr & 16) // Setup end?
    {
        USB->TXCSR = 0x80; // EP0 Serviced Setup End
    }

    if (csr & 1) // Received packet ready
    {
        if (USB->RXCOUNT == 0)
        {
            printf("Zero length EP0\n");
            USB->TXCSR = 0x40; // Serviced RxPktRdy
            return;
        }

        if (USB->RXCOUNT > 8 || USB->RXCOUNT < 4)
        {
            printf("Invalid EP0 packet length %d! HALT\n", USB->RXCOUNT);

            while (1)
                ; // Reset USB?
        }

        // Copy setup packet bytes into struct
        SETUP_PACKET setup;
        setup.dat[0] = USB->FIFO[0].word32;
        setup.dat[1] = USB->FIFO[0].word32;

        // https://beyondlogic.org/usbnutshell/usb6.shtml

        printf("bmRequestType=%d, bRequest=%d\n", setup.bmRequestType, setup.bRequest);
        if (setup.bmRequestType == 0x80 && setup.bRequest == 0x06) // GET_DESCRIPTOR
        {
            printf("\tRequest descriptor %d\n", setup.wValue_h);
            switch (setup.wValue_h)
            {
            case 0x01:
                // Device descriptor
                printf("\t\tRead device desc\n");
                ep0_send_dsc(&deviceDescriptor, deviceDescriptor.bLength, setup.wLength);
                break;
            case 0x02:
                // Config descriptor
                printf("\t\tRead config desc\n");
                ep0_send_dsc(&configDescriptor, configDescriptor.config.wTotalLength, setup.wLength);
                break;
            case 0x03:
                // String descriptors
                int stringIndex = setup.wValue_l;

                if (stringIndex == 0)
                {
                    // String descriptor supported languges
                    printf("\t\tRead string lang desc %d\n", stringIndex);
                    ep0_send_dsc(&languageDescriptor, languageDescriptor.bLength, setup.wLength);
                    return;
                }

                if (stringIndex >= 1 && stringIndex <= NUM_STRINGS)
                {
                    printf("\t\tRead string %d\n", stringIndex);
                    ep0_send_str(stringDescriptors[stringIndex - 1], setup.wLength);
                    return;
                }

                printf("\t\tReading unknown string %d\n", stringIndex);
                break;
            case 0x06:
                ep0_send_dsc(&qualifierDescriptor, qualifierDescriptor.bLength, setup.wLength);
                break;
            }
        }
        else if (setup.bmRequestType == 0x00 && setup.bRequest == 0x05) // SET_ADDRESS
        {
            deviceAddress = setup.wValue;

            printf("\tDevice address: %d\n", deviceAddress);

            USB->TXCSR = 0x48;
            while (USB->TXCSR & 0x08)
                ;

            // Set the EP0 TX function address to the device address
            USB->TXFUNCADDR = deviceAddress & 0x7F;
        }
        else if (setup.bmRequestType == 0x00 && setup.bRequest == 0x09) // SET_CONFIGURATION
        {
            deviceConfiguration = setup.wValue_l;

            printf("\tSet configuration %d\n", deviceConfiguration);

            // Setup interrupt endpoint
            USB->EP_IDX = configDescriptor.managementEndpoint.bEndpointAddress & 0xF;
            USB->TXFIFOSZ = 0x04;     // 4 = 128bytes
            USB->TXFIFOADDR = 64 / 8; // EP0
            USB->TXMAXP = configDescriptor.managementEndpoint.wMaxPacketSize;
            USB->TXCSR = 0x0090;

            // Setup data in endpoint
            USB->EP_IDX = configDescriptor.dataInEndpoint.bEndpointAddress & 0xF;
            USB->TXFIFOSZ = 0x06;              // 6 = 512bytes
            USB->TXFIFOADDR = (64 + 128) / 8; // EP0 + EP1
            USB->TXMAXP = configDescriptor.dataInEndpoint.wMaxPacketSize;
            USB->TXCSR = 0xA048; // [AutoSet, TX mode], [ClrDataTog, FlushFIFO]

            // Setup data out endpoint
            USB->EP_IDX = configDescriptor.dataOutEndpoint.bEndpointAddress & 0xF;
            USB->RXFIFOSZ = 0x06;                    // 6 = 512bytes
            USB->RXFIFOADDR = (64 + 128 + 512) / 8; // EP0 + EP1 + EP2
            USB->RXMAXP = configDescriptor.dataOutEndpoint.wMaxPacketSize;
            USB->RXCSR = 0x8090; // [Auto clear], [ClrDataTog, FlushFIFO]

            // Setup complete
            USB->EP_IDX = 0;
            USB->TXCSR = 0x48; // Serviced RxPktRdy | DataEnd
        }
        else if (setup.bmRequestType == 0xA1 && setup.bRequest == 0x21) // CDC: GET_LINE_CODING
        {
            printf("\tCDC GET_LINE_CODING\n");
            ep0_send_buf(&lineCoding, sizeof(CDC_LINECODING));
        }
        else if (setup.bmRequestType == 0x21 && setup.bRequest == 0x20) // CDC: SET_LINE_CODING
        {
            printf("\tCDC SET_LINE_CODING\n");

            // Get next packet for data - secion 21.1.2
            USB->TXCSR = 0x40; // Set ServicedRxPktRdy
            
            while ((USB->EP_IS & 1) == 0); // Wait for another EP0 RIQ
            while ((USB->TXCSR & 1) == 0); // Wait for the RxPktRdy flag

            // Now the FIFO has the next packet
            uint16_t count0 = USB->RXCOUNT;
            if (count0 != 7)
            {
                printf("\tUnexpected line coding size\n");
            }
            else
            {
                // Read the data into the CDC_LINECODING struct
                for (int x = 0; x < 7; x++)
                {
                    lineCoding.data[x] = USB->FIFO[0].byte;
                }
            }

            printf("\tBaud=%ld, stopBits=%d, partiy=%d, bits=%d\n", lineCoding.dwDTERate,
                lineCoding.bCharFormat, lineCoding.bParityType, lineCoding.bDataBits);

            USB->EP_IDX = 0;
            USB->TXCSR = 0x48; // Serviced RxPktRdy | DataEnd
        }
        else if (setup.bmRequestType == 0x21 && setup.bRequest == 0x22) // CDC: SET_CONTROL_LINE_STATE
        {
            uint16_t settings = setup.wValue;

            printf("\tCDC SET_CONTROL_LINE_STATE\n");
            printf("\tInterface = %d, DTE = %d, Carrier = %d\n", setup.wIndex, settings & 0x01, settings & 0x02);

            USB->EP_IDX = 0;
            USB->TXCSR = 0x48; // Serviced RxPktRdy | DataEnd
        }
        /*else if (setup.bmRequestType == 0x02 && setup.bRequest == 0x01) // Something sent to the IRQ EP
        {
            //USB->EP_IDX = 0;
            //USB->TXCSR = 0x40; // Serviced RxPktRdy

            printf("wValue %02x %02x wIndex %02x %02x wLength %02x %02x\n",
            setup.wValue_l, setup.wValue_h, setup.wIndex_l, setup.wIndex_h, setup.wLength_l, setup.wLength_h);
        }*/
        else
        {
            printf("%02x %02x %02x %02x %02x %02x %02x %02x\n", setup.bmRequestType, setup.bRequest,
            setup.wValue_l, setup.wValue_h, setup.wIndex_l, setup.wIndex_h, setup.wLength_l, setup.wLength_h);
        }
    }
    else
    {
        printf("Other thingy CSR %08x\n", csr);
    }
}

static void transfer_bytes()
{
    if (txInProgress)
    {
        return;
    }

    txInProgress = 1;

    USB->EP_IDX = configDescriptor.dataInEndpoint.bEndpointAddress & 0x0F;

    int bytesInBuffer = ringbuffer_length(&txBuffer);
    if (bytesInBuffer > TX_FIFOSZ_BYTES)
    {
        bytesInBuffer = TX_FIFOSZ_BYTES;
    }

    printf("\tTransfering %d\n", bytesInBuffer);

    for (int x = 0; x < bytesInBuffer; x++)
    {
        uint8_t data = ringbuffer_read(&txBuffer);
        USB->FIFO[configDescriptor.dataInEndpoint.bEndpointAddress & 0x0F].byte = data;
    }
    
    USB->TXCSR |= 1; // TxPktRdy

    USB->EP_IDX = 0;
}

static void handle_ep3_in()
{
    // Section 22.1.2
    printf("EP3 IN\n");

    uint8_t epAddr = configDescriptor.dataOutEndpoint.bEndpointAddress & 0x0F;
    USB->EP_IDX = epAddr;

    uint16_t csr = USB->RXCSR;
    printf("\tCSR = %08x\n", csr);
    if ((csr & 1) == 0) // RxPktRdy ?
    {
        printf("\tData not ready\n");
        USB->EP_IDX = 0;
        return;
    }

    uint16_t rec = USB->RXCOUNT;

    printf("\t[%d] ", rec);
    for (int x = 0; x < rec; x++)
    {
        uint8_t data = USB->FIFO[epAddr].byte;
        
        ringbuffer_write(&rxBuffer, data);

        printf("%02x ", data);
    }
    printf("\n");

    USB->RXCSR &= ~1; // Clear the RxPktRdy
    USB->EP_IDX = 0; // Reset the EP index pointer just to be sure
}

static void handle_ep2_iqr()
{
    printf("EP2 IRQ");

    txInProgress = 0; // Got EP2 ISR meaning the last transfer is complete

    if (ringbuffer_length(&txBuffer) == 0)
    {
        // Nothing to transfer
        printf("\tEmpty TX Buffer");
        return;
    }

    transfer_bytes();
}

void cdc_handler()
{
    // Handle USB BUS events first
    uint32_t isr;
    isr = USB->BUS_IS;
    USB->BUS_IS = isr;

    if (isr & 1)
    {
        printf("USB suspend\n");
    }
    if (isr & 2)
    {
        printf("USB resume\n");
    }
    if (isr & 4)
    {
        printf("USB reset\n");

        USB->EP_IS = 0xFFFFFFFF;
        USB->EP_IDX = 0;
        USB->TXFUNCADDR = 0;
    }

    isr = USB->EP_IS; // Read endpoint irq status register
    USB->EP_IS = isr;

    if (isr != 0)
    {
        printf("ISR %08lx\n", isr);
    }
    
    // Check EP0 RX ISR
    if (isr & 1)
    {
        handle_ep0();
    }

    // Check EP2 transfer ready ISR
    if (isr & (1 << 2))
    {
        handle_ep2_iqr();
    }

    // Check EP3 RX ISR
    // This is where we receive data from the host
    if (isr & (1 << 19))
    {
        handle_ep3_in();
    }
}

int cdc_bytes_in()
{
    return ringbuffer_length(&rxBuffer);
}

int cdc_read_byte()
{
    return ringbuffer_read(&rxBuffer);
}

int cdc_write_byte(uint8_t data)
{
    int res = ringbuffer_write(&txBuffer, data);

    transfer_bytes();

    return res;
}

// PS: No idea what I'm doing or why this even works.
