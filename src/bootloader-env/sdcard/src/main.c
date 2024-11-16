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

#define SD_VERSION_UNKNOWN (0)
#define SD_VERSION_1 (1)
#define SD_VERSION_2 (2)

typedef struct 
{
    uint8_t version;
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

// SD.pdf page 178, section 5.1
#define OCR_BUSY (1 << 31)
#define OCR_CCS (1 << 30) // 0 = SDSD, 1 = SDHC/SDXC

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
static int sd_send_interface_conditions(SD_T* sd)
{
    // SD.pdf page 92, section 4.3.13
    uint8_t checkPattern = 0xAA;
    uint32_t argument = (1 << 8) | checkPattern; // 2.7-3.6V, 0xAA = pattern check

    if (!sd_transfer_command(sd, SD_CMD_REG_RESP_RCV | 8, argument))
    {
        printStr("(CMD8 fail)");
        return 0;
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

    return 1;
}

static int sd_send_operating_conditions(SD_T* sd)
{
    // Send CMD55 because the next command is an app specific command
    if (!sd_transfer_command(sd, SD_CMD_REG_RESP_RCV | 55, 0))
    {
        printStr("(CMD55 fail)");
        return 0;
    }

    for (int acmd41Tries = 0; acmd41Tries < 100; acmd41Tries++)
    {
        // Send ACMD41 - Operating conditions
        uint32_t acmd41Arg = (1 << 30); // HCS = 1 (high capacity support)
        if (!sd_transfer_command(sd, SD_CMD_REG_RESP_RCV | 41, acmd41Arg))
        {
            printStr("(ACMD41 fail)");
            return 0;
        }

        uint32_t ocr = sd->SD_RESP0_REG;
        if (!(ocr & OCR_BUSY))
        {
            print32(ocr);

            return 1;
        }

        // Wait a bit before the next ACMD41
        sdelay(1000);
    }

    return 0;
}

// Continues SD card detection assuming the card is V2.0 or later
static int detect_sd_v2(SD_T* sd, sdcard_info* card)
{
    card->version = SD_VERSION_2;

    printStr("\tSending operating conditions ");
    if (!sd_send_operating_conditions(sd))
    {
        printStr("fail\n");

        return 0;
    }
    printStr("OK\n");
}

// Tries to detect the SD/MMC card, the type, the size and everything else
static int detect_sd(uint32_t sdBase, sdcard_info* card)
{
    printStr("SD reset & io setup\n");
    clk_reset_set(CCU_BUS_SOFT_RST0, 8);
    clk_enable(CCU_BUS_CLK_GATE0, 8);
    clk_reset_clear(CCU_BUS_SOFT_RST0, 8);

    gpio_init(GPIOF, PIN1 | PIN2 | PIN3, GPIO_MODE_AF2, GPIO_PULL_NONE, GPIO_DRV_3);

    SD_T* sd = (SD_T*)sdBase;

    // Reset controller
    printStr("Reset SD controller ");
    sd_reset_full(sd);
    printStr("OK\n");

    // Set clock
    printStr("Setting clock ");
    if (!sd_set_controller_clock(sd, 400000)) // In the identification phase it is advised to use 100-400kHz clocks
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

    // Do the SD card identification process
    // SD.pdf page 48
    printStr("Card identification...\n");
    {
        // Go IDLE - CMD0
        printStr("\tGo idle ");
        if (!sd_go_idle(sd))
        {
            printStr("fail\n");
            return 0;
        }
        printStr("OK\n");

        // CMD8
        printStr("\tSend interface params ");
        if (!sd_send_interface_conditions(sd))
        {
            // If CMD8 is an illegal command it should be a V1.X SD card, however I have no way of testing this so...

            printStr("fail\n");
            return 0;
        }
        printStr("OK\n");

        // Detect V2 card
        printStr("\tV2 card detected\n");
        if (!detect_sd_v2(sd, card))
        {
            return 0;
        }
    }

    return 1;
}

int main(void)
{
    system_init();
    arm32_interrupt_enable(); // Enable interrupts

    sdcard_info card = 
    {
        .version = SD_VERSION_UNKNOWN,
    };
    if (detect_sd(SDC0_BASE, &card))
    {
        printStr("SD card detected\n");
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
