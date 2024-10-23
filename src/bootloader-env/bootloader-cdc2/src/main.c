#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "system.h"
#include "arm32.h"
#include "armv5_cache.h"
#include "f1c100s_uart.h"
#include "f1c100s_gpio.h"
#include "f1c100s_clock.h"
#include "f1c100s_intc.h"
#include "print.h"
#include "usb.h"
#include "usb_cdc.h"
#include "dma.h"

// 0 - no debug logs = go fast
// 1 - lots of debug logs = slow
#define DEBUG 0

// Bootloader load stuff
#define RAM_BASE 0x80000000
#define BL1_SIZE 0x20000
#define LOAD_ADDR (RAM_BASE + BL1_SIZE)

static uint32_t loadAddress = LOAD_ADDR;

#define EP3COMMAND_UPLOAD_TO_RAM 0xa1 // args: 32bit data length
#define EP3COMMAND_EXECUTE 0xb1

#define EP3STATE_WAIT_COMMAND 0
#define EP3STATE_WAIT_COMMAND_ARGS 1
#define EP3STATE_WAIT_DATA 2
#define EP3STATE_WAIT_DMA 3
static uint32_t ep3State = EP3STATE_WAIT_COMMAND;
static uint32_t ep3CurrentCommand = 0;
static uint32_t ep3DataLength = 0;

// USB descriptors
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
        512,            // wMaxPacketSize
        0               // bInterval
    }
};

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

// USB EP0 state
#define EP0STATE_CONTROL_PACKET 0
#define EP0STATE_CDC_LINESTATE 1
static uint8_t ep0State = EP0STATE_CONTROL_PACKET;

// CDC config
static CDC_LINECODING lineCoding = {0};

// Start the uploaded code
static void execute()
{
    arm32_interrupt_disable(); // Disable IRQs so nothing is going to stop us

    // Set the INTC Vector base address to the new table which should be the first thing in the new code
    intc_set_irq_base(LOAD_ADDR);

    // Invalidate the cache for the loaded code
    cache_inv_range(LOAD_ADDR, loadAddress);

    // Jump
    void (*bootFunc)(void) = (void (*)())LOAD_ADDR;
    bootFunc();

    // How
    printStr("Interesting :>\n");

    while (1);
}

// Writes data to the USB physical layer controller
static void usb_phy_write(uint8_t addr, uint8_t data, uint8_t len)
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

    USB->FIFO[0].word16 = 0x0300 | dlen;
    while (len -= 2)
        USB->FIFO[0].word16 = *dsc++;
    USB->TXCSR = 0x0A; // TxPktRdy | DataEnd
}

static void usb_init()
{
    // Enable DMA
    clk_enable(CCU_BUS_CLK_GATE0, 6);
    clk_reset_clear(CCU_BUS_SOFT_RST0, 6);
    //DMA->INT_PRIO = DMA->INT_PRIO | (1 << 16); // Auto clock gating

    // Enable USB clock
    clk_usb_config(1, 0);              // Clock ON, disable reset
    clk_enable(CCU_BUS_CLK_GATE0, 24); // Enable clock

    // Reset
    clk_reset_set(CCU_BUS_SOFT_RST0, 24);
    clk_reset_clear(CCU_BUS_SOFT_RST0, 24);

    // Setup PHY
    usb_phy_write(0x0c, 0x01, 1); // RES45_CAL_EN -> Enable
    usb_phy_write(0x20, 0x14, 5); // SET_TX_AMPLITUDE_TUNE & SET_TX_SLEWRATE_TUNE
    usb_phy_write(0x2A, 0x03, 2); // SET_DISCON_DET_THRESHOLD

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

void usb_deinit()
{
    clk_usb_config(0, 1); // Clock off, assert reset

    clk_disable(CCU_BUS_CLK_GATE0, 24); // Assert clock gate

    clk_reset_set(CCU_BUS_SOFT_RST0, 24); // Assert USB OTG Reset
}

static void usb_handle_ep0_control_packet()
{
    uint32_t receiveDataCount = USB->RXCOUNT;
    if (receiveDataCount == 0) // ZLP - Zero length packet ACK, NACK etc
    {
        USB->TXCSR = 0x48; // Serviced RxPktRdy | DataEnd
        return;
    }

    if (receiveDataCount != 8)
    {
        printStr("EP0 Control packet invalid length ");
        printDec8(receiveDataCount);
        printChar('\n');
        return; // Not enough data
    }

    // Copy data from buffer to the packet structure
    SETUP_PACKET setup;
    setup.dat[0] = USB->FIFO[0].word32;
    setup.dat[1] = USB->FIFO[0].word32;

    // Check packet
#if DEBUG
    printStr("\twRequest ");
    print16(setup.wRequest);

    printStr(", wValue ");
    print16(setup.wValue);

    printStr(", wIndex ");
    print16(setup.wIndex);

    printStr(", wLength ");
    print16(setup.wLength);

    printChar('\n');
#endif

    // https://beyondlogic.org/usbnutshell/usb6.shtml

    if (setup.bmRequestType == 0x80 && setup.bRequest == 0x06) // GET_DESCRIPTOR
    {
        USB->TXCSR = 0x40; // Serviced RxPktRdy

#if DEBUG
        printStr("\tRequest descriptor ");
        print8(setup.wValue_h);
        printChar('\n');
#endif

        switch (setup.wValue_h)
        {
        case 0x01:
            // Device descriptor
            printStr("\t\tRead device desc\n");
            ep0_send_dsc(&deviceDescriptor, deviceDescriptor.bLength, setup.wLength);
            break;
        case 0x02:
            // Config descriptor
            printStr("\t\tRead config desc\n");
            ep0_send_dsc(&configDescriptor, configDescriptor.config.wTotalLength, setup.wLength);
            break;
        case 0x03:
            // String descriptors
            int stringIndex = setup.wValue_l;
            if (stringIndex == 0)
            {
                // String descriptor supported languges
                printStr("\t\tRead string lang desc ");
                print8(stringIndex);
                printChar('\n');

                ep0_send_dsc(&languageDescriptor, languageDescriptor.bLength, setup.wLength);
                return;
            }

            if (stringIndex >= 1 && stringIndex <= NUM_STRINGS)
            {
                printStr("\t\tRead string ");
                print8(stringIndex);
                printChar('\n');

                ep0_send_str(stringDescriptors[stringIndex - 1], setup.wLength);
                return;                
            }

            printStr("\t\tReading unknown string ");
            print8(stringIndex);
            printChar('\n');
            break;
        case 0x06:
            ep0_send_dsc(&qualifierDescriptor, qualifierDescriptor.bLength, setup.wLength);
            break;
        }
    }
    else if (setup.bmRequestType == 0x00 && setup.bRequest == 0x05) // SET_ADDRESS
    {
        uint16_t deviceAddress = setup.wValue;

        printStr("\tDevice address: ");
        printDec16(deviceAddress);
        printChar('\n');

        USB->TXCSR = 0x48;
        while (USB->TXCSR & 0x08)
            ;

        // Set the EP0 TX function address to the device address
        USB->TXFUNCADDR = deviceAddress & 0x7F;
    }
    else if (setup.bmRequestType == 0x00 && setup.bRequest == 0x09) // SET_CONFIGURATION
    {
        uint8_t deviceConfiguration = setup.wValue_l;

        printStr("\tSet configuration ");
        printDec16(deviceConfiguration);
        printChar('\n');

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

    // CDC stuff
    else if (setup.bmRequestType == 0xA1 && setup.bRequest == 0x21) // CDC: GET_LINE_CODING
    {
        printStr("\tCDC GET_LINE_CODING\n");

        USB->TXCSR = 0x48; // Serviced RxPktRdy | DataEnd

        ep0_send_buf(&lineCoding, sizeof(CDC_LINECODING));
    }
    else if (setup.bmRequestType == 0x21 && setup.bRequest == 0x20) // CDC: SET_LINE_CODING
    {
        printStr("\tCDC SET_LINE_CODING\n");

        // Get next packet for data - secion 21.1.2
        ep0State = EP0STATE_CDC_LINESTATE;

        USB->TXCSR = 0x40; // Serviced RxPktRdy
    }
    else if (setup.bmRequestType == 0x21 && setup.bRequest == 0x22) // CDC: SET_CONTROL_LINE_STATE
    {
        uint16_t settings = setup.wValue;

        printStr("\tCDC SET_CONTROL_LINE_STATE, tInterface=");
        printDec8(setup.wIndex);
        printStr(", DTE=");
        printDec8(settings & 0x01);
        printStr(", Carrier=");
        printDec8(settings & 0x02);
        printChar('\n');

        USB->EP_IDX = 0;
        USB->TXCSR = 0x48; // Serviced RxPktRdy | DataEnd
    }
    else if (setup.bmRequestType == 0x02 && (setup.bRequest == 0x01 || setup.bRequest == 0x03)) // Endpoint request, CLEAR_FEATURE or SET_FEATURE
    {
        // EP1 feature ENDPOINT_HALT stuff, whatever this is
        printStr(setup.bRequest == 0x01 ? "\nCLEAR_FEATURE\n" : "\nSET_FEATURE\n");

        uint8_t endpointIndex = setup.wIndex & 0x7F;
        printDec8(endpointIndex);
        printChar('\n');

        USB->EP_IDX = endpointIndex;
        if (setup.bRequest == 0x03)
        {
            USB->TXCSR = 0x10; // Send STALL
        }
        else
        {
            USB->TXCSR = 0x48; // Clear stall, reset data toggle
        }

        USB->EP_IDX = 0;
        USB->TXCSR = 0x48; // Serviced RxPktRdy | DataEnd
    }
}

static void usb_handle_ep0_cdc_line_state()
{
    uint32_t receiveDataCount = USB->RXCOUNT;

    ep0State = EP0STATE_CONTROL_PACKET;

    if (receiveDataCount != 7)
    {
        printStr("EP0 Unexpected line coding size ");
        printDec8(receiveDataCount);
        printChar('\n');
        return;
    }

    // Read the data into the CDC_LINECODING struct
    for (int x = 0; x < 7; x++)
    {
        lineCoding.data[x] = USB->FIFO[0].byte;
    }

    printStr("Baud=");
    printDec16(lineCoding.dwDTERate);
    printStr(", stopBits=");
    printDec16(lineCoding.bCharFormat);
    printStr(", partiy=");
    printDec16(lineCoding.bParityType);
    printStr(", bits=");
    printDec16(lineCoding.bDataBits);
    printChar('\n');

    USB->EP_IDX = 0;
    USB->TXCSR = 0x48; // Serviced RxPktRdy | DataEnd
}

static void usb_handle_ep0()
{
    USB->EP_IDX = 0; // Select endpoint 0

    uint16_t csr = USB->TXCSR; // EP0 -> TXCSR = CSR0 (Section 3.3.1)
    
#if DEBUG
    printStr("CSR0: ");
    print32(csr);
    printChar('\n');
#endif

    if (csr & 1) // RxPktRdy
    {
        switch (ep0State)
        {
            case EP0STATE_CONTROL_PACKET:
                usb_handle_ep0_control_packet();
                break;
            case EP0STATE_CDC_LINESTATE:
                usb_handle_ep0_cdc_line_state();
                break;
        }
    }

    if (csr & 4) // SentStall
    {
#if DEBUG
        printStr("STALL\n");
#endif

        USB->TXCSR = USB->TXCSR & ~(1 << 3); // Clear SentStall
    }

    if (csr & 8) // DataEnd
    {
#if DEBUG
        printStr("DataEnd\n");
#endif
    }

    if (csr & 16) // Setup end
    {
#if DEBUG
        printStr("SetupEnd\n");
#endif

        USB->TXCSR = 0x80; // EP0 Serviced Setup End
    }
}

static void handle_ep3_in()
{
    // Section 22.1.2
#if DEBUG
    printStr("EP3 IN\n");
#endif

    uint8_t epAddr = configDescriptor.dataOutEndpoint.bEndpointAddress & 0x0F;
    USB->EP_IDX = epAddr;

    uint16_t csr = USB->RXCSR;

#if DEBUG
    printStr("\tCSR ");
    print16(csr);
    printChar('\n');
#endif

    if ((csr & 1) == 0) // RxPktRdy ?
    {
        printStr("EP3 Data not ready\n");
        USB->EP_IDX = 0;
        return;
    }

    uint16_t ep3Bytes = USB->RXCOUNT;

    switch (ep3State)
    {
        case EP3STATE_WAIT_COMMAND:
            // Pop the command byte from the FIFO
            uint8_t command = USB->FIFO[epAddr].byte;

#if DEBUG
            printStr("EP3 command 0x");
            print8(command);
            printChar('\n');
#endif

            if (command == EP3COMMAND_EXECUTE)
            {
                usb_deinit();

                execute();

                return;
            }
            else
            {
                ep3CurrentCommand = command;
                ep3State = EP3STATE_WAIT_COMMAND_ARGS;
            }

            ep3Bytes = USB->RXCOUNT;
            if (ep3Bytes == 0)
            {
                USB->RXCSR &= ~1;
                break;
            }

        case EP3STATE_WAIT_COMMAND_ARGS:
#if DEBUG
            printStr("EP3 command arg 0x");
            print8(ep3CurrentCommand);
            printChar(' ');
            printDec8(ep3Bytes);
            printChar('\n');
#endif

            if (ep3CurrentCommand == EP3COMMAND_UPLOAD_TO_RAM && ep3Bytes >= 4)
            {
                uint8_t b0 = USB->FIFO[epAddr].byte;
                uint8_t b1 = USB->FIFO[epAddr].byte;
                uint8_t b2 = USB->FIFO[epAddr].byte;
                uint8_t b3 = USB->FIFO[epAddr].byte;

                ep3DataLength = b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
                ep3State = EP3STATE_WAIT_DATA;
                USB->RXCSR &= ~1;

#if DEBUG
                printStr("EP3 data len ");
                print32(ep3DataLength);
                printChar('\n');
#endif
            }
            break;

        case EP3STATE_WAIT_DATA:
            // Got data, start DMA
            NDMA_T* dma = NDMA(0);
            dma->SRC = (uint32_t)USB_BASE + 0x0C;
            dma->DST = loadAddress;
            dma->BYTE_COUNTER = ep3Bytes;
            dma->CFG = (0x11 | (1 << 5) | (0x11 << 16) | (1 << 31));

            while (dma->CFG & (1 << 31))
            {
                // Wait until load clears
            }

#if DEBUG
            printStr("DMA start 0x");
            print32(loadAddress);
            printChar('\n');
#endif

            ep3DataLength -= ep3Bytes;
            loadAddress += ep3Bytes;
            ep3State = EP3STATE_WAIT_DMA;
            break;
    }

    USB->EP_IDX = 0; // Reset the EP index pointer just to be sure
}

static void usb_handler()
{
    // Handle DMA status
    if (ep3State == EP3STATE_WAIT_DMA)
    {
        NDMA_T* dma = NDMA(0);
        if (!(dma->CFG & (1 << 30)))
        {
            // Clear the RxPktRdy
            USB->EP_IDX = configDescriptor.dataOutEndpoint.bEndpointAddress & 0x0F;
            USB->RXCSR &= ~1;
            USB->EP_IDX = 0;

            if (ep3DataLength <= 0)
            {
                ep3DataLength = 0;
                ep3State = EP3STATE_WAIT_COMMAND;

#if DEBUG
                printStr("EP3 data transfer complete\n");
#endif
            }
            else
            {
                ep3State = EP3STATE_WAIT_DATA;

#if DEBUG
                printStr("EP3 data left ");
                print32(ep3DataLength);
                printChar('\n');
#endif
            }
        }
    }

    // Handle USB bus interrupts
    uint8_t busISR = USB->BUS_IS; // IRQs are cleared when this register is read
    USB->BUS_IS = busISR;

#if DEBUG
    if (busISR != 0 && busISR != 8) // Don't care about SOF
    {
        printStr("BUS_IS: ");
        print32(busISR);
        printChar('\n');
    }
#endif

    if (busISR & 1) // Suspend
    {
        printStr("USB Suspend\n");
    }

    if (busISR & 2) // Resume
    {
        printStr("USB Resume\n");
    }

    if (busISR & 3) // Reset
    {
        printStr("USB Reset\n");

        USB->EP_IS = 0xFFFFFFFF; // Clear all endpoint IRQs
        USB->EP_IDX = 0;
        USB->TXFUNCADDR = 0;
    }

#if DEBUG
    //if (busISR & 4) // SOF - Start of frame
    //{
    //    printStr("USB SOF\n"); // Whatever this is
    //}
#endif

    // Handle endpoint interrupts
    uint32_t epISR = USB->EP_IS; // IRQs are cleared when this register is read
    USB->EP_IS = epISR;

    if (epISR == 0)
    {
        return; // No active IRQs
    }

#if DEBUG
    printStr("EP_IS: ");
    print32(epISR);
    printChar('\n');
#endif

    if (epISR & 1)
    {
        // EP0 IRQ
        usb_handle_ep0();
    }

    // Check EP3 RX ISR
    // This is where we receive data from the host
    if (epISR & (1 << 19))
    {
        handle_ep3_in();
    }
}

int main(void)
{
    system_init();            // Initialize clocks, mmu, cache, uart, ...
    arm32_interrupt_enable(); // Enable interrupts

    printStr("USB\n");
    usb_init();

    while (1)
    {
        usb_handler();
    }
    return 0;
}
