#include <stdint.h>
#include "io.h"
#include <math.h>
#include <string.h>
#include "system.h"
#include "arm32.h"
#include "f1c100s_uart.h"
#include "f1c100s_gpio.h"
#include "f1c100s_clock.h"
#include "f1c100s_de.h"
#include "f1c100s_timer.h"
#include "f1c100s_intc.h"
#include "print.h"

#define __I volatile const
#define __O volatile
#define __IO volatile

#define PACKED __attribute__((packed))

#define SWAP_BYTES32(U32) ( \
    (((U32) & 0xff000000) >> 24) | \
    (((U32) & 0x00ff0000) >>  8) | \
    (((U32) & 0x0000ff00) <<  8) | \
    (((U32) & 0x000000ff) << 24) \
)

#define SD_VERSION_UNKNOWN (0)
#define SD_VERSION_SD_V1 (1)
#define SD_VERSION_SD_V1_1 (2)
#define SD_VERSION_SD_V2 (2)
#define SD_VERSION_SD_V3 (3)
#define SD_VERSION_SD_V4 (4)
#define SD_VERSION_SD_V5 (5)
#define SD_VERSION_SD_V6 (6)

#define SD_FLAG_CCS (1 << 0) // Card capacity flag, it means a high capacity card
#define SD_FLAG_BUS4 (1 << 1) // Card supports 4 bit bus

typedef struct 
{
    uint8_t version;
    uint8_t flags;
    uint16_t rca;

    uint16_t blockLength;
    uint32_t blockCount;
    uint64_t capacity;
} sdcard_info;

// Allwinner PDF page 255, section 7.1
typedef struct
{
    __IO uint32_t SD_GCTL_REG; // 0x000 SD Control Register 
    __IO uint32_t SD_CKCR_REG; // 0x004 SD Clock Control Register 
    __IO uint32_t SD_TMOR_REG; // 0x008 SD Time Out Register 
    __IO uint32_t SD_BWDR_REG; // 0x00C SD Bus Width Register 
    __IO uint32_t SD_BKSR_REG; // 0x010 SD Block size Register 
    __IO uint32_t SD_BYCR_REG; // 0x014 SD Byte Count Register 
    __IO uint32_t SD_CMDR_REG; // 0x018 SD Command Register 
    __IO uint32_t SD_CAGR_REG; // 0x01C SD Command Argument Register 
    __IO uint32_t SD_RESP0_REG; // 0x020 SD Response Register 0 
    __IO uint32_t SD_RESP1_REG; // 0x024 SD Response Register 1 
    __IO uint32_t SD_RESP2_REG; // 0x028 SD Response Register 2 
    __IO uint32_t SD_RESP3_REG; // 0x02C SD Response Register 3 
    __IO uint32_t SD_IMKR_REG; // 0x030 SD Interrupt Mask Register 
    __IO uint32_t SD_MISR_REG; // 0x034 SD Masked Interrupt Status Register 
    __IO uint32_t SD_RISR_REG; // 0x038 SD Raw Interrupt Status Register 
    __IO uint32_t SD_STAR_REG; // 0x03C SD Status Register 
    __IO uint32_t SD_FWLR_REG; // 0x040 SD FIFO Water Level Register 
    __IO uint32_t SD_FUNS_REG; // 0x044 SD FIFO Function Select Register 
    __IO uint32_t SD_CBCR_REG; // 0x048 SD Transferred CIU Card Byte Count Register 
    __IO uint32_t SD_BBCR_REG; // 0x04C SD Transferred Host To BIU-FIFO Byte Count Register 
    __IO uint32_t SD_DBGC_REG; // 0x050 SD Current Debug Control Address Register 
    __I uint32_t RES1; // 0x054
    __IO uint32_t SD_A12A_REG; // 0x058 SD Auto Command 12 Argument Register 
    __I uint32_t RES2[7];
    __IO uint32_t SD_HWRST_REG; // 0x078 SD Hardware Reset Register 
    __I uint32_t RES3;
    __IO uint32_t SD_DMAC_REG; // 0x080 SD BUS Mode Control Register 
    __IO uint32_t SD_DLBA_REG; // 0x084 SD Descriptor List Base Address Register 
    __IO uint32_t SD_IDST_REG; // 0x088 SD DMAC Status Register 
    __IO uint32_t SD_IDIE_REG; // 0x08C SD DMAC Interrupt Enable Register 
    __IO uint32_t SD_CHDA_REG; // 0x090 SD Current Host Descriptor Address Register 
    __IO uint32_t SD_CBDA_REG; // 0x094 SD Current Buffer Descriptor Address Register 
    __I uint32_t RES4[26];
    __IO uint32_t CARD_THLDC_REG; // 0x100 Card Threshold Control Register
    __I uint32_t RES5[2];
    __IO uint32_t EMMC_DSBD_REG; // 0x10C EMMC4.5 DDR Start Bit Detection Control Register
    __I uint32_t RES6[60]; 
    __IO uint32_t SD_FIFO_REG; // 0x200 SD FIFO Register
} SD_T;

#define SD_GCTL_REG_SOFT_RST (1 << 0)
#define SD_GCTL_REG_FIFO_RST (1 << 1)
#define SD_GCTL_REG_DMA_RST (1 << 2)
#define SD_GCTL_REG_CD_DBC_ENB (1 << 8) // Card detect debounce
#define SD_GCTL_REG_FIFO_AC_MOD (1 << 31) // 0 = DMA access, 1 = AHB bus access

#define SD_CKCR_REG_CCLK_ENB (1 << 16) // Card clock enable
#define SD_CKCR_REG_CCLK_CTRL (1 << 17) // Card clock output control: Turn off clock when FSM in IDLE

#define SD_BWDR_REG_1 (0)
#define SD_BWDR_REG_4 (1)
#define SD_BWDR_REG_8 (2)

#define SD_CMD_REG_CMD_LOAD (1 << 31) // Start command, cleared when the current command is sent
#define SD_CMD_REG_USE_HOLD (1 << 29) // Use HOLD register
#define SD_CMD_REG_VOL_SW (1 << 28) // Switch voltage, CMD11 only
#define SD_CMD_REG_BOOT_ABORT (1 << 27) // Abort boot process
#define SD_CMD_REG_EXP_BOOT_ACK (1 << 26) // Expect boot ack
#define SD_CMD_REG_BOOT_MOD (1 << 24) // 2bit, (0 = normal command, 1 = mandatory boot operation, 2 = alternate boot operation)
#define SD_CMD_REG_PRG_CLK (1 << 21) // Change clock, no command will be sent
#define SD_CMD_REG_SENT_INIT_SEQ (1 << 15) // Send init sequence
#define SD_CMD_REG_STOP_ABT_CMD (1 << 14) // Send stop abort command
#define SD_CMD_REG_WAIT_PRE_OVER (1 << 13) // Wait data transfer over
#define SD_CMD_REG_STOP_CMD_FLAG (1 << 12) // Send stop command (CMD12) after data transfer
#define SD_CMD_REG_TRANS_MODE (1 << 11) // Transfer mode (0 = Block data transfer, 1 = Stream data transfer)
#define SD_CMD_REG_TRANS_DIR (1 << 10) // Transfer direction (0 = Read, 1 = Write)
#define SD_CMD_REG_DATA_TRANS (1 << 9) // Data transfer (0 = No data, 1 = Has data)
#define SD_CMD_REG_CHK_RESP_CRC (1 << 8) // Check response CRC
#define SD_CMD_REG_LONG_RESP (1 << 7) // Response type (0 = 48 bits, 1 = 136 bits)
#define SD_CMD_REG_RESP_RCV (1 << 6) // Response receive (0 = command without response, 1 = command with response)
#define SD_CMD_REG_CMD_IDX_OFFSET (0) // 5bit command index offset

// Allwinner pdf page 265
#define SD_RISR_REG_CARD_REMOVED (1 << 31)
#define SD_RISR_REG_CARD_INSERTED (1 << 30)
#define SD_RISR_REG_SDIO_IRQ (1 << 16)
#define SD_RISR_REG_DATA_END_BIT_ERROR (1 << 15)
#define SD_RISR_REG_AUTO_COMMAND_DONE (1 << 14)
#define SD_RISR_REG_DATA_START_ERROR (1 << 13)
#define SD_RISR_REG_COMMAND_BUSY (1 << 12)
#define SD_RISR_REG_FIFO_ERROR (1 << 11)
#define SD_RISR_REG_DATA_STARVATION_OR_VOLTAGE_CHANGE (1 << 10)
#define SD_RISR_REG_DATA_TIMEOUT (1 << 9)
#define SD_RISR_REG_RESPONSE_TIMEOUT (1 << 8)
#define SD_RISR_REG_DATA_CRC_ERROR (1 << 7)
#define SD_RISR_REG_RESPONSE_CRC_ERROR (1 << 6)
#define SD_RISR_REG_DATA_RECEIVE_REQUEST (1 << 5)
#define SD_RISR_REG_DATA_TRANSMIT_REQUEST (1 << 4)
#define SD_RISR_REG_DATA_TRANSFER_COMPLETE (1 << 3)
#define SD_RISR_REG_COMMAND_COMPLETE (1 << 2)
#define SD_RISR_REG_RESPONSE_ERROR (1 << 1) // No response or CRC error

#define SD_RISR_REG_ERROR_MASK \
    (SD_RISR_REG_RESPONSE_ERROR | SD_RISR_REG_RESPONSE_CRC_ERROR | SD_RISR_REG_DATA_CRC_ERROR | SD_RISR_REG_RESPONSE_TIMEOUT | \
     SD_RISR_REG_DATA_TIMEOUT | SD_RISR_REG_FIFO_ERROR | SD_RISR_REG_COMMAND_BUSY | SD_RISR_REG_DATA_START_ERROR | SD_RISR_REG_DATA_END_BIT_ERROR)
#define SD_RISR_REG_DONE_MASK \
    (SD_RISR_REG_AUTO_COMMAND_DONE | SD_RISR_REG_DATA_TRANSFER_COMPLETE | SD_RISR_REG_COMMAND_COMPLETE | SD_RISR_REG_DATA_STARVATION_OR_VOLTAGE_CHANGE)

// Allwinner pdf page 255
#define SD_STAR_FIFO_RX_TRIGGER (1 << 0)
#define SD_STAR_FIFO_TX_TRIGGER (1 << 1)
#define SD_STAR_FIFO_EMPTY (1 << 2)
#define SD_STAR_FIFO_FULL  (1 << 3)
#define SD_STAR_CARD_PRESENT  (1 << 8)
#define SD_STAR_CARD_BUSY  (1 << 9)
#define SD_STAR_FIFO_LEVEL(REG) ((REG >> 17) & 0x1F)
#define SD_STAR_DMA_REQ  (1 << 31)

// SD.pdf page 120, section 4.10.1
#define CARDSTATUS_APPCMD (1 << 5) // The card is expecting an APPCMD

// SD.pdf page 178, section 5.1
#define OCR_POWERUP_READY (1 << 31) // The docs say that this is a busy flag, but in fact this is a ready flag :)
#define OCR_CCS (1 << 30) // 0 = SDSD, 1 = SDHC/SDXC
#define OCR_VDD_MASK (0xFF8000) // 2.7 - 3.6V in 100mV steps

// Resets the SD controller (FIFO, DMA) and enables the debounce on card detect
static void sd_reset_full(SD_T* sd)
{
    sd->SD_GCTL_REG = SD_GCTL_REG_SOFT_RST | SD_GCTL_REG_FIFO_RST | SD_GCTL_REG_DMA_RST | SD_GCTL_REG_CD_DBC_ENB;
    while ((sd->SD_GCTL_REG & SD_GCTL_REG_SOFT_RST) || (sd->SD_GCTL_REG & SD_GCTL_REG_FIFO_RST))
    {
        // Wait until soft reset and fifo reset clears
    }
}

// Sets the SD controller clock, but does not update the SD card.
static int sd_set_controller_clock(SD_T* sd, uint32_t hz)
{
    uint32_t sdBase = (uint32_t)sd;
    switch (sdBase)
    {
        case SDC0_BASE:
            clk_sdc_config(CCU_SDMMC0_CLK, hz);
            break;
        case SDC1_BASE:
            clk_sdc_config(CCU_SDMMC1_CLK, hz);
            break;

        default:
            return 0;
    }

    // Reset the clock
    sd->SD_CKCR_REG = 0;
    sd->SD_CMDR_REG = SD_CMD_REG_CMD_LOAD | SD_CMD_REG_PRG_CLK | SD_CMD_REG_WAIT_PRE_OVER;
    while (sd -> SD_CMDR_REG & SD_CMD_REG_CMD_LOAD)
    {
        // Wait for the command to be sent
    }

    // Set the clock enable and commit
    sd->SD_CKCR_REG = sd->SD_CKCR_REG | SD_CKCR_REG_CCLK_ENB;
    sd->SD_CMDR_REG = SD_CMD_REG_CMD_LOAD | SD_CMD_REG_PRG_CLK | SD_CMD_REG_WAIT_PRE_OVER;
    while (sd -> SD_CMDR_REG & SD_CMD_REG_CMD_LOAD)
    {
        // Wait for the command to be sent
    }

    return 1;
}

// Transfers a command to the SD card
static int sd_transfer_command(SD_T* sd, uint32_t commandRegister, uint32_t argument)
{
    sd->SD_CAGR_REG = argument;
    sd->SD_CMDR_REG = SD_CMD_REG_CMD_LOAD | commandRegister;

    while (sd -> SD_CMDR_REG & SD_CMD_REG_CMD_LOAD)
    {
        // Wait for the command to be sent
    }

    uint32_t irq;
    do
    {
        // Wait until either the command completes or fails
        irq = sd->SD_RISR_REG;
    } while (!(irq & SD_RISR_REG_COMMAND_COMPLETE) && !(irq & SD_RISR_REG_ERROR_MASK));
    
    // Clear all interrupts
    sd->SD_RISR_REG = 0xFFFFFFFF;

    if (irq & SD_RISR_REG_COMMAND_COMPLETE)
    {
        return 1;
    }

    uint32_t error = irq & SD_RISR_REG_ERROR_MASK;
    printStr("(error ");
    print32(error);
    printChar(')');

    return 0;
}

// Resets the card usind CMD0
static int sd_go_idle(SD_T* sd)
{
    return sd_transfer_command(sd, SD_CMD_REG_SENT_INIT_SEQ, 0);
}

// Sends the interface conditions to the SD card
static int sd_send_interface_conditions(SD_T* sd, sdcard_info* card)
{
    // SD.pdf page 92, section 4.3.13
    uint8_t checkPattern = 0xAA;
    uint32_t argument = (1 << 8) | checkPattern; // 2.7-3.6V, 0xAA = pattern check

    if (!sd_transfer_command(sd, SD_CMD_REG_CHK_RESP_CRC | SD_CMD_REG_RESP_RCV | 8, argument))
    {
        // TODO: Check illegal command flag
        printStr("(CMD8 fail -> probably V1)");

        card->version = SD_VERSION_SD_V1;

        return 1;
    }

    // CMD8 has 48bits of response (-start bit, -transmission bit, -crc, -stop bit)
    uint32_t r0 = sd->SD_RESP0_REG;

    if ((r0 & 0xFF) != checkPattern)
    {
        printStr("(pattern fail)");
        return 0;
    }

    if ((r0 & (1 << 8)) == 0)
    {
        printStr("(voltage fail)");
        return 0;
    }

    // If CMD8 works, it's a V2 card
    card->version = SD_VERSION_SD_V2;

    return 1;
}

static int sd_start_app_cmd(SD_T* sd, uint16_t rca)
{
    uint32_t arg = rca << 16;

    // Send CMD55 because the next command is an app specific command
    if (!sd_transfer_command(sd, SD_CMD_REG_CHK_RESP_CRC | SD_CMD_REG_RESP_RCV | 55, arg))
    {
        printStr("(CMD55 fail)\n");
        return 0;
    }

    uint8_t cardStatus = sd->SD_RESP0_REG;
    if (!(cardStatus & CARDSTATUS_APPCMD))
    {
        printStr("(APPCMD status fail ");
        print32(cardStatus);
        printStr(")\n");
        return 0;
    }

    return 1;
}

// Ask the card to send its operating conditions
static int sd_send_operating_conditions(SD_T* sd, sdcard_info* card)
{
    uint32_t voltageWindow = 0x00ff8000; // All voltages set from 2.7V to 3.6V

    uint32_t ocr = 0;
    do
    {
        // Send CMD55 because the next command is an app specific command
        if (!sd_start_app_cmd(sd, 0))
        {
            printStr("(CMD55 fail)");
            return 0;
        }

        uint32_t acmd41Arg = 0x50000000 | voltageWindow; // HCS + XPC + voltage window
        if (!sd_transfer_command(sd, SD_CMD_REG_RESP_RCV | 41, acmd41Arg))
        {
            printStr("(ACMD41 fail)\n");
            return 0;
        }

        ocr = sd->SD_RESP0_REG;

        if (!(ocr & OCR_POWERUP_READY))
        {
            sdelay(1000000);
        }
    } while (!(ocr & OCR_POWERUP_READY));

    if ((ocr & OCR_VDD_MASK) == 0)
    {
        printStr("(ACMD41 unsupported voltage range)");

        return 0;
    }

    if (ocr & OCR_CCS)
    {
        card->flags |= SD_FLAG_CCS;
        printStr("(HC/XC)");
    }

    return 1;
}

static int sd_get_cid(SD_T* sd, sdcard_info* card)
{
    // Send CMD2, response is R2 which is 136bits
    if (!sd_transfer_command(sd, SD_CMD_REG_RESP_RCV | SD_CMD_REG_LONG_RESP | SD_CMD_REG_CHK_RESP_CRC | 2, 0))
    {
        printStr("(CMD2 fail)");
        
        return 0;
    }

    uint32_t cid0 = sd->SD_RESP0_REG;
    uint32_t cid1 = sd->SD_RESP1_REG;
    uint32_t cid2 = sd->SD_RESP2_REG;
    uint32_t cid3 = sd->SD_RESP3_REG;

    print32(cid0);
    printChar(' ');
    print32(cid1);
    printChar(' ');
    print32(cid2);
    printChar(' ');
    print32(cid3);
    printChar(' ');

    // Whatever, SD.pdf page 180, section 5.12

    return 1;
}

// Ask the card to publish a new relative address which is unsed in addressed commands
static int sd_get_rca(SD_T* sd, sdcard_info* card)
{
    // Send CMD3, response is R6
    if (!sd_transfer_command(sd, SD_CMD_REG_RESP_RCV | SD_CMD_REG_CHK_RESP_CRC | 3, 0))
    {
        printStr("(CMD3 fail)");
        
        return 0;
    }

    uint32_t resp = sd->SD_RESP0_REG;
    card->rca = (resp >> 16) & 0xFFFF;

    printStr("(RCA ");
    printDec16(card->rca);
    printChar(')');

    return 1;
}

static int decode_csd_v2(sdcard_info* card, uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3)
{
    // SD.pdf page 182, section 5.3.2
    uint16_t read_bl_len = 1 << ((d2 >> 16) & 0x0F);
    printStr("\n\t\tRead block length ");
    printDec16(read_bl_len);
    if (read_bl_len != 512)
    {
        // CSD v2 only support 512 block sizes
        printStr("\n\t\tUnexpected block size!");
        return 0;
    }

    card->blockLength = read_bl_len; // CSD v2 spec says that read_bl_len = write_bl_len;

    uint32_t c_size1 = (d1 >> 16) & 0xFFFF;
    uint32_t c_size2 = (d2 & 0x3F);
    uint32_t c_size = c_size1 | (c_size2 << 16);

    card->blockCount = (c_size + 1) << 10;
    printStr("\n\t\tBlock count 0x");
    print32(card->blockCount);

    card->capacity = (uint64_t)card->blockCount * read_bl_len;
    printStr("\n\t\tCapacity 0x");
    print64(card->capacity);

    printChar('\n');
    return 1;
}

// Get the Card-Specific data, SD.pdf page 181, section 5.3
static int sd_get_csd(SD_T* sd, sdcard_info* card)
{
    uint32_t arg = card->rca << 16;

    // Send CMD9, response is R6, 148bits
    if (!sd_transfer_command(sd, SD_CMD_REG_RESP_RCV | SD_CMD_REG_LONG_RESP | SD_CMD_REG_CHK_RESP_CRC | 9, arg))
    {
        printStr("(CMD9 fail)");
        
        return 0;
    }

    // CSD_STRUCTURE bits
    uint32_t d0 = sd->SD_RESP0_REG; // 0-31
    uint32_t d1 = sd->SD_RESP1_REG; // 32-63
    uint32_t d2 = sd->SD_RESP2_REG; // 64-95
    uint32_t d3 = sd->SD_RESP3_REG; // 96-127

    print32(d0);
    printChar(' ');
    print32(d1);
    printChar(' ');
    print32(d2);
    printChar(' ');
    print32(d3);
    printChar('\n');

    // Process CSD
    if ((d0 & 1) != 1)
    {
        printStr("\t\tUnexpected CSD format\n");
        return 0;
    }

    uint8_t csdVersion = (d3 >> 30) & 3;
    printStr("\t\tCSD version ");
    printDec8(csdVersion);

    switch (csdVersion)
    {
        case 1:
            return decode_csd_v2(card, d0, d1, d2, d3);
        case 0:
            printStr("\t\tCSD v1 not implemented!\n");
            return 0;
        default:
            printStr("\t\tUnknown CSD version\n");
            return 0;
    }
    
    return 0;
}

static int sd_select_card(SD_T* sd, sdcard_info* card)
{
    uint32_t arg = card->rca << 16;

    // Send CMD7, response is R1b
    if (!sd_transfer_command(sd, SD_CMD_REG_RESP_RCV | SD_CMD_REG_CHK_RESP_CRC | 7, arg))
    {
        printStr("(CMD7 fail)");
        
        return 0;
    }

    return 1;
}

static int sd_set_block_length(SD_T* sd, uint32_t blockLength)
{
    // Send CMD16
    if (!sd_transfer_command(sd, SD_CMD_REG_RESP_RCV | SD_CMD_REG_CHK_RESP_CRC | 16, blockLength))
    {
        printStr("(CMD16 fail)");
        
        return 0;
    }

    return 1;
}

static int sd_get_scr(SD_T* sd, sdcard_info* card)
{
    sd->SD_GCTL_REG |= SD_GCTL_REG_FIFO_AC_MOD; // Enable AHB bus data access   
    sd->SD_RISR_REG = 0xFFFFFFFF; // Clear all IRQs

    // Send CMD55 with RCA
    if (!sd_start_app_cmd(sd, card->rca))
    {
        printStr("(CMD55 fail)");
        return 0;
    }

    if (!sd_transfer_command(sd, SD_CMD_REG_DATA_TRANS | SD_CMD_REG_WAIT_PRE_OVER | SD_CMD_REG_RESP_RCV | 51, 0))
    {
        printStr("(ACMD51 fail)\n");
        return 0;
    }

    // Rec data
    uint32_t data[2] = {0};
    uint8_t dataIndex = 0;

    uint32_t status;
    uint32_t irq;

    // Read the two bytes of the structure
    volatile uint32_t popFifo;
    do
    {
        status = sd->SD_STAR_REG;
        irq = sd->SD_RISR_REG;

        uint32_t error = (irq & SD_RISR_REG_ERROR_MASK) & (~SD_RISR_REG_DATA_CRC_ERROR); // Ignore CRC errors
        if (error)
        {
            printStr("Data recv error ");
            print32(irq);
            printChar('\n');

            return 0;
        }

        int fifoEmpty = status & SD_STAR_FIFO_EMPTY;
        if (fifoEmpty && (irq & SD_RISR_REG_DATA_TRANSFER_COMPLETE))
        {
            break;
        }

        if (!fifoEmpty)
        {
            popFifo = sd->SD_FIFO_REG;
            if (dataIndex < 2)
            {
                data[dataIndex] = popFifo;
            }

            dataIndex += 1;
        }
    } while (true);

    if (!(sd->SD_STAR_REG & SD_STAR_FIFO_EMPTY))
    {
        printStr("(FIFO not empty!)");
    }

    // Clear IRQs
    sd->SD_RISR_REG = 0xFFFFFFFF;

    // Process SCR
    uint32_t scr = SWAP_BYTES32(data[0]);
    printStr("0x");
    print32(scr);
    printChar(' ');

    uint8_t scr_structure = (scr >> 28) & 0x0F;
    uint8_t sd_spec = (scr >> 24) & 0x0F;
    uint8_t data_after_erase = (scr >> 23) & 1;
    uint8_t sd_security = (scr >> 21) & 7;
    uint8_t sd_bus_widths = (scr >> 16) & 0x0f;
    uint8_t sd_spec3 = (scr >> 15) & 1;
    uint8_t ex_security = (scr >> 11) & 0x0f;
    uint8_t sd_spec4 = (scr >> 10) & 1;
    uint8_t sd_specx = (scr >> 6) & 0x0f;

    if (scr_structure != 0)
    {
        printStr("(Invalid SCR version ");
        printDec8(scr_structure);
        printChar(')');
        return 0;
    }

    if (sd_spec == 0 && sd_spec3 == 0 && sd_spec4 == 0 && sd_specx == 0)
    {
        card->version = SD_VERSION_SD_V1;
    }
    else if (sd_spec == 1 && sd_spec3 == 0 && sd_spec4 == 0 && sd_specx == 0)
    {
        card->version = SD_VERSION_SD_V1_1;
    }
    else if (sd_spec == 2 && sd_spec3 == 0 && sd_spec4 == 0 && sd_specx == 0)
    {
        card->version = SD_VERSION_SD_V2;
    }
    else if (sd_spec == 2 && sd_spec3 == 1 && sd_spec4 == 0 && sd_specx == 0)
    {
        card->version = SD_VERSION_SD_V3;
    }
    else if (sd_spec == 2 && sd_spec3 == 1 && sd_spec4 == 1 && sd_specx == 0)
    {
        card->version = SD_VERSION_SD_V4;
    }
    else if (sd_spec == 2 && sd_spec3 == 1 && sd_specx == 1)
    {
        card->version = SD_VERSION_SD_V5;
    }
    else if (sd_spec == 2 && sd_spec3 == 1 && sd_specx == 2)
    {
        card->version = SD_VERSION_SD_V6;
    }
    else
    {
        printStr("(Unknown SD version ");
        printDec8(sd_spec);
        printChar(',');
        printDec8(sd_spec3);
        printChar(',');
        printDec8(sd_spec4);
        printChar(',');
        printDec8(sd_specx);
        printChar(')');

        return 0;
    }

    printStr("(SD ver ");
    printDec16(card->version);
    printChar(')');

    if (!(sd_bus_widths & 1))
    {
        printStr("(BUS1 mode not supported - invalid card)");
        return 0;
    }

    if (sd_bus_widths & 4)
    {
        card->flags |= SD_FLAG_BUS4;
        printStr("(BUS4 supported)");
    }

    return 1;
}

// Switch card between 1 bit and 4 bit bus
static int sd_set_bus_width(SD_T* sd, sdcard_info* card, int bus4)
{
    // Send CMD55 with RCA
    if (!sd_start_app_cmd(sd, card->rca))
    {
        printStr("(CMD55 fail)");
        return 0;
    }

    uint32_t resp = sd->SD_RESP0_REG;

    uint32_t arg = (bus4 ? 2 : 0);
    if (!sd_transfer_command(sd, SD_CMD_REG_RESP_RCV | 6, arg))
    {
        printStr("(ACMD6 fail)\n");
        return 0;
    }

    resp = sd->SD_RESP0_REG;
    
    printStr("0x");
    print32(resp);

    return 1;
}

// Continues SD card detection assuming the card is V2.0 or later
static int detect_sd_v2(SD_T* sd, sdcard_info* card)
{
    printStr("\tAssuming SD V2\n");
    printStr("\tSending operating conditions ");
    if (!sd_send_operating_conditions(sd, card))
    {
        printStr("fail\n");
        return 0;
    }
    printStr("OK\n");

    printStr("\tGet card identification ");
    if (!sd_get_cid(sd, card))
    {
        printStr("fail\n");
        return 0;
    }
    printStr("OK\n");

    printStr("\tGet card RCA ");
    if (!sd_get_rca(sd, card))
    {
        printStr("fail\n");
        return 0;
    }
    printStr("OK\n");

    printStr("\tGet card info ");
    if (!sd_get_csd(sd, card))
    {
        printStr("fail\n");
        return 0;
    }

    printStr("\tSelect card ");
    if (!sd_select_card(sd, card))
    {
        printStr("fail\n");
        return 0;
    }
    printStr("OK\n");

    printStr("\tGet SCR ");
    if (!sd_get_scr(sd, card))
    {
        printStr("fail\n");
        return 0;
    }
    printStr("OK\n");

    // Standard capacity card?
    if (!(card->flags & SD_FLAG_CCS))
    {
        sd->SD_BKSR_REG = 512;

        printStr("\tSet block length to 512 bytes ");
        if (!sd_set_block_length(sd, 512))
        {
            printStr("fail\n");
            return 0;
        }
        printStr("OK\n");
    }

    // Switch clock
    // NOTE: sd.pdf page 33, section 3.9 says that UHS-I cards in 3.3V signaling can go up to 50MHz
    if (card->version >= SD_VERSION_SD_V2)
    {
        printStr("\tSetting clock to 50MHz ");
        if (!sd_set_controller_clock(sd, 50000000))
        {
            printStr("Failed to set controller clock!\n");
            return 0;
        }
        printStr("OK\n");
    }

    // Switch bus width
    if (card->flags & SD_FLAG_BUS4)
    {
        printStr("\tSwitching bus width to 4bit ");
        if (!sd_set_bus_width(sd, card, true))
        {
            printStr("fail\n");
        }
        sd->SD_BWDR_REG = SD_BWDR_REG_4; // Tell the controller to use 4bits as well
        printStr("OK\n");
    }

    printStr("\tGet SCR ");
    if (!sd_get_scr(sd, card))
    {
        printStr("fail\n");
        return 0;
    }
    printStr("OK\n");

    return 1;
}

// Tries to detect the SD/MMC card, the type, the size and everything else
static int detect_sd(SD_T* sd, sdcard_info* card)
{
    printStr("SD reset & io setup\n");
    clk_reset_set(CCU_BUS_SOFT_RST0, 8);
    clk_enable(CCU_BUS_CLK_GATE0, 8);
    clk_reset_clear(CCU_BUS_SOFT_RST0, 8);

    gpio_init(GPIOF, PIN1 | PIN2 | PIN3, GPIO_MODE_AF2, GPIO_PULL_NONE, GPIO_DRV_3);

    // Reset controller
    printStr("Reset SD controller ");
    sd_reset_full(sd);
    printStr("OK\n");

    // Set clock
    printStr("Setting clock ");
    if (!sd_set_controller_clock(sd, 400000)) // In the identification phase it is advised to use 100k-400kHz clocks
    {
        printStr("Failed to set clock!\n");
        return 0;
    }
    printStr("OK\n");

    // Set bus width
    printStr("Set bus width ");
    sd->SD_BWDR_REG = SD_BWDR_REG_1; // In the identification phase 1 bit bus width must be used
    printStr("OK\n");

    // Clear all interrupts
    sd->SD_RISR_REG = 0xFFFFFFFF;

    // Wait a bit for the card
    sdelay(1000000);

    // Do the SD card identification process
    // SD.pdf page 48
    printStr("Card identification...\n");
    {
        // Go IDLE - CMD0
        printStr("\tReset card ");
        if (!sd_go_idle(sd))
        {
            printStr("fail\n");
            return 0;
        }
        printStr("OK\n");

        // CMD8
        printStr("\tSend interface params ");
        if (!sd_send_interface_conditions(sd, card))
        {
            printStr("fail\n");
            return 0;
        }
        printStr("OK\n");

        switch (card->version)
        {
            case SD_VERSION_SD_V1:
                printStr("SD V1 not supported");
                return 0;
            case SD_VERSION_SD_V2:
                return detect_sd_v2(sd, card);
            default:
                printStr("Unknown SD version");
                return 0;
        }
    }

    return 0;
}

static int sd_read_data(SD_T* sd, sdcard_info* card, uint8_t* buffer, uint64_t address, uint64_t byteCount)
{
    if (address % card->blockLength != 0)
    {
        printStr("(Unaligned read!)");
        return 0;
    }

    sd->SD_GCTL_REG |= SD_GCTL_REG_FIFO_AC_MOD; // Enable AHB bus data access

    // Clear all IRQs
    sd->SD_RISR_REG = 0xFFFFFFFF;

    if (!sd_transfer_command(sd, SD_CMD_REG_DATA_TRANS | SD_CMD_REG_WAIT_PRE_OVER | SD_CMD_REG_CHK_RESP_CRC | SD_CMD_REG_RESP_RCV | 17, address))
    {
        printStr("(CMD17 fail)\n");
        return 0;
    }

     // Rec data
    uint32_t data[128] = {0}; // 512 bytes of data
    uint8_t dataIndex = 0;

    uint32_t status;
    uint32_t irq;

    // Receive bytes
    volatile uint32_t popFifo;
    do
    {
        status = sd->SD_STAR_REG;
        irq = sd->SD_RISR_REG;

        uint32_t error = (irq & SD_RISR_REG_ERROR_MASK) & (~SD_RISR_REG_DATA_CRC_ERROR); // Ignore CRC errors
        if (error)
        {
            printStr("Data recv error ");
            print32(irq);
            printChar('\n');

            return 0;
        }

        int fifoEmpty = status & SD_STAR_FIFO_EMPTY;
        if (fifoEmpty && (irq & SD_RISR_REG_DATA_TRANSFER_COMPLETE))
        {
            break;
        }

        if (!fifoEmpty)
        {
            popFifo = sd->SD_FIFO_REG;
            if (dataIndex < 128)
            {
                data[dataIndex] = popFifo;
            }

            dataIndex += 1;
        }
    } while (true);

    if (!(sd->SD_STAR_REG & SD_STAR_FIFO_EMPTY))
    {
        printStr("(FIFO not empty!)");
    }

    // Clear IRQs
    sd->SD_RISR_REG = 0xFFFFFFFF;

    printStr("Got data \n");
    for (int x = 0; x < 128; x++)
    {
        print32(data[x]);
        printChar(' ');

        if ((x + 1) % 8 == 0)
        {
            printChar('\n');
        }
    }

    return 1;
}

int main(void)
{
    system_init();
    arm32_interrupt_enable(); // Enable interrupts

    // Wait for input
    printStr("Wait for input\n");
    while (uart_get_rx_fifo_level(UART1) == 0)
    {
    }
    uart_get_rx(UART1);

    SD_T* sd = (SD_T*)SDC0_BASE;

    sdcard_info card = 
    {
        .version = SD_VERSION_UNKNOWN,
        .flags = 0,
        .rca = 0,
        .capacity = 0,
    };
    if (detect_sd(sd, &card))
    {
        printStr("SD card detected\n");

        uint8_t sector[512] = {0};
        uint64_t index = 0;
        while (true)
        {
            printStr("Read sector at 0x");
            print64(index);
            printChar('\n');
            if (!sd_read_data(sd, &card, sector, index, 512))
            {
                printStr("Sector read fail\n");
            }

            while (uart_get_rx_fifo_level(UART1) == 0)
            {
                // Wait for keypress
            }
            uart_get_rx(UART1);

            index += card.blockLength;
        }
    }
    else
    {
        printStr("SD card detection failed!\n");
    }

    while (1)
    {
    }

    return 0;
}

/*#include <stdint.h>
#include "io.h"
#include <math.h>
#include <string.h>
#include "system.h"
#include "arm32.h"
#include "f1c100s_uart.h"
#include "f1c100s_gpio.h"
#include "f1c100s_clock.h"
#include "f1c100s_de.h"
#include "f1c100s_timer.h"
#include "f1c100s_intc.h"
#include "print.h"
#include "sd_other.h"
#include "f1c100s_sdc.h"

static sdcard_t sdcard;

int main(void)
{
    system_init();
    arm32_interrupt_enable();

    printStr("RESET\n");

    clk_reset_set(CCU_BUS_SOFT_RST0, 8);
    clk_enable(CCU_BUS_CLK_GATE0, 8);
    clk_reset_clear(CCU_BUS_SOFT_RST0, 8);

    gpio_init(GPIOF, PIN1 | PIN2 | PIN3, GPIO_MODE_AF2, GPIO_PULL_NONE, GPIO_DRV_3);

    printStr("Detect\n");

    sdcard.sdc_base = SDC0_BASE;
    sdcard.voltage = MMC_VDD_27_36;
    sdcard.width = MMC_BUS_WIDTH_1;
    sdcard.clock = 50000000;

    if (sdcard_detect(&sdcard) == 1)
    {
        printStr("OK\n");
        print32(sdcard.read_bl_len);
    }
    else
    {
        printStr("FAIL\n");
    }

    while (1)
    {
    }
}*/