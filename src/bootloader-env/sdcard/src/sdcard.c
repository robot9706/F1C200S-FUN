#include "sdcard.h"
#include "f1c100s_clock.h"
#include "f1c100s_gpio.h"
#include "print.h"
#include "system.h"
#include <stdint.h>
#include <stdbool.h>

#define SD_TRY(strAction, sdFuncCall) { \
    printStr(strAction);                \
    if (!(sdFuncCall))                  \
    {                                   \
        printStr("fail\n");             \
        return 0;                       \
    }                                   \
    printStr("OK\n");                   \
}

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
#define SD_OK (0)
static uint32_t sd_transfer_command(SD_T* sd, uint32_t commandRegister, uint32_t argument)
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
        return SD_OK;
    }

    uint32_t error = irq & SD_RISR_REG_ERROR_MASK;
    printf("(error %x4)", error);

    return error;
}

// Resets the card usind CMD0
static int sd_go_idle(SD_T* sd)
{
    return sd_transfer_command(sd, SD_CMD_REG_SENT_INIT_SEQ, 0) == SD_OK;
}

// Sends the interface conditions to the SD card
static int sd_send_interface_conditions(SD_T* sd, sdcard_info* card)
{
    // SD.pdf page 92, section 4.3.13
    uint8_t checkPattern = 0xAA;
    uint32_t argument = (1 << 8) | checkPattern; // 2.7-3.6V, 0xAA = pattern check

    uint32_t cmd8Result;
    if ((cmd8Result = sd_transfer_command(sd, SD_CMD_REG_CHK_RESP_CRC | SD_CMD_REG_RESP_RCV | 8, argument)) != SD_OK)
    {
        if (cmd8Result & SD_RISR_REG_RESPONSE_TIMEOUT)
        {
            // Probably no card
            printStr("(CMD8 fail -> no card?)");
            return 0;
        }

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
    if (sd_transfer_command(sd, SD_CMD_REG_CHK_RESP_CRC | SD_CMD_REG_RESP_RCV | 55, arg) != SD_OK)
    {
        printStr("(CMD55 fail)\n");
        return 0;
    }

    uint8_t cardStatus = sd->SD_RESP0_REG;
    if (!(cardStatus & CARDSTATUS_APPCMD))
    {
        printf("(APPCMD status fail %x4)\n", cardStatus);
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
        if (sd_transfer_command(sd, SD_CMD_REG_RESP_RCV | 41, acmd41Arg) != SD_OK)
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
        printStr("(HC/XC) ");
    }

    return 1;
}

static int sd_get_cid(SD_T* sd, sdcard_info* card)
{
    // Send CMD2, response is R2 which is 136bits
    if (sd_transfer_command(sd, SD_CMD_REG_RESP_RCV | SD_CMD_REG_LONG_RESP | SD_CMD_REG_CHK_RESP_CRC | 2, 0) != SD_OK)
    {
        printStr("(CMD2 fail)");
        
        return 0;
    }

    uint32_t cid0 = sd->SD_RESP0_REG;
    uint32_t cid1 = sd->SD_RESP1_REG;
    uint32_t cid2 = sd->SD_RESP2_REG;
    uint32_t cid3 = sd->SD_RESP3_REG;

    printf("%x4 %x4 %x4 %x4 ", cid0, cid1, cid2, cid3);

    // Whatever, SD.pdf page 180, section 5.12

    return 1;
}

// Ask the card to publish a new relative address which is unsed in addressed commands
static int sd_get_rca(SD_T* sd, sdcard_info* card)
{
    // Send CMD3, response is R6
    if (sd_transfer_command(sd, SD_CMD_REG_RESP_RCV | SD_CMD_REG_CHK_RESP_CRC | 3, 0) != SD_OK)
    {
        printStr("(CMD3 fail)");
        
        return 0;
    }

    uint32_t resp = sd->SD_RESP0_REG;
    card->rca = (resp >> 16) & 0xFFFF;

    printf("(RCA %d2)", card->rca);

    return 1;
}

static int decode_csd_v2(sdcard_info* card, uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3)
{
    // SD.pdf page 182, section 5.3.2
    uint16_t read_bl_len = 1 << ((d2 >> 16) & 0x0F);
    printf("\n\t\tRead block length %d2", read_bl_len);
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
    printf("\n\t\tBlock count 0x%x4", card->blockCount);

    card->capacity = (uint64_t)card->blockCount * read_bl_len;
    printf("\n\t\tCapacity 0x%x8", card->capacity);

    return 1;
}

// Get the Card-Specific data, SD.pdf page 181, section 5.3
static int sd_get_csd(SD_T* sd, sdcard_info* card)
{
    uint32_t arg = card->rca << 16;

    // Send CMD9, response is R6, 148bits
    if (sd_transfer_command(sd, SD_CMD_REG_RESP_RCV | SD_CMD_REG_LONG_RESP | SD_CMD_REG_CHK_RESP_CRC | 9, arg) != SD_OK)
    {
        printStr("(CMD9 fail)");
        
        return 0;
    }

    // CSD_STRUCTURE bits
    uint32_t d0 = sd->SD_RESP0_REG; // 0-31
    uint32_t d1 = sd->SD_RESP1_REG; // 32-63
    uint32_t d2 = sd->SD_RESP2_REG; // 64-95
    uint32_t d3 = sd->SD_RESP3_REG; // 96-127

    printf("%x4 %x4 %x4 %x4\n", d0, d1, d2, d3);

    // Process CSD
    if ((d0 & 1) != 1)
    {
        printStr("\t\tUnexpected CSD format\n");
        return 0;
    }

    uint8_t csdVersion = (d3 >> 30) & 3;
    printf("\t\tCSD version %d1", csdVersion);

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
    if (sd_transfer_command(sd, SD_CMD_REG_RESP_RCV | SD_CMD_REG_CHK_RESP_CRC | 7, arg) != SD_OK)
    {
        printStr("(CMD7 fail)");
        
        return 0;
    }

    return 1;
}

static int sd_set_block_length(SD_T* sd, uint32_t blockLength)
{
    // Send CMD16
    if (sd_transfer_command(sd, SD_CMD_REG_RESP_RCV | SD_CMD_REG_CHK_RESP_CRC | 16, blockLength) != SD_OK)
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

    if (sd_transfer_command(sd, SD_CMD_REG_DATA_TRANS | SD_CMD_REG_WAIT_PRE_OVER | SD_CMD_REG_RESP_RCV | 51, 0) != SD_OK)
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
            printf("Data recv error %x4\n", irq);
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
    printf("0x%x4 ", scr);

    uint8_t scr_structure = (scr >> 28) & 0x0F;
    uint8_t sd_spec = (scr >> 24) & 0x0F;
    // uint8_t data_after_erase = (scr >> 23) & 1;
    // uint8_t sd_security = (scr >> 21) & 7;
    uint8_t sd_bus_widths = (scr >> 16) & 0x0f;
    uint8_t sd_spec3 = (scr >> 15) & 1;
    // uint8_t ex_security = (scr >> 11) & 0x0f;
    uint8_t sd_spec4 = (scr >> 10) & 1;
    uint8_t sd_specx = (scr >> 6) & 0x0f;

    if (scr_structure != 0)
    {
        printf("(Invalid SCR version %d1)", scr_structure);
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
        printf("(Unknown SD version %d1, %d1, %d1, %d1)", sd_spec, sd_spec3, sd_spec4, sd_specx);
        return 0;
    }

    printf("(SD ver %d2)", card->version);
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
        printStr("(CMD55 fail) ");
        return 0;
    }

    uint32_t resp = sd->SD_RESP0_REG;

    uint32_t arg = (bus4 ? 2 : 0);
    if (sd_transfer_command(sd, SD_CMD_REG_RESP_RCV | 6, arg) != SD_OK)
    {
        printStr("(ACMD6 fail) ");
        return 0;
    }

    resp = sd->SD_RESP0_REG;
    
    printf("0x%x4 ", resp);

    return 1;
}

// Continues SD card detection assuming the card is V2.0 or later
static int detect_sd_v2(SD_T* sd, sdcard_info* card)
{
    printStr("\tAssuming SD V2\n");

    SD_TRY("\tSending operating conditions ", sd_send_operating_conditions(sd, card));
    SD_TRY("\tGet card identification ", sd_get_cid(sd, card));
    SD_TRY("\tGet card RCA ", sd_get_rca(sd, card));
    SD_TRY("\tGet card info ", sd_get_csd(sd, card));
    SD_TRY("\tSelect card ", sd_select_card(sd, card));
    SD_TRY("\tGet SCR ", sd_get_scr(sd, card));

    // Standard capacity card?
    if (!(card->flags & SD_FLAG_CCS))
    {
        sd->SD_BKSR_REG = 512;

        SD_TRY("\tSet block length to 512 bytes ", sd_set_block_length(sd, 512));
    }

    // Switch clock
    // NOTE: sd.pdf page 33, section 3.9 says that UHS-I cards in 3.3V signaling can go up to 50MHz
    if (card->version >= SD_VERSION_SD_V2)
    {
        SD_TRY("\tSetting clock to 50MHz ", sd_set_controller_clock(sd, 50000000));
    }

    // Switch bus width
    if (card->flags & SD_FLAG_BUS4)
    {
        SD_TRY("\tSwitching bus width to 4bit ", sd_set_bus_width(sd, card, true));
        sd->SD_BWDR_REG = SD_BWDR_REG_4; // Tell the controller to use 4bits as well

        sdelay(1000000); // Sometimes the first data transfer after the bus width change fails
    }

    return 1;
}

// Tries to detect the SD/MMC card, the type, the size and everything else
int sdcard_detect(SD_T* sd, sdcard_info* card)
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
    SD_TRY("Setting clock ", sd_set_controller_clock(sd, 400000)); // In the identification phase it is advised to use 100k-400kHz clocks

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
        SD_TRY("\tReset card ", sd_go_idle(sd));

        // CMD8 - interface conditions
        SD_TRY("\tSend interface params ", sd_send_interface_conditions(sd, card));

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

    printStr("SDcard detected OK\n");
    return 0;
}

int sdcard_read_sector(SD_T* sd, sdcard_info* card, uint8_t* buffer, uint64_t address, uint32_t numBytes)
{
    if (numBytes != card->blockLength)
    {
        printStr("Unable to read non sector sizes!");
        return 0;
    }

    if (address % card->blockLength != 0)
    {
        printStr("(Unaligned read!)");
        return 0;
    }

    sd->SD_GCTL_REG |= SD_GCTL_REG_FIFO_AC_MOD; // Enable AHB bus data access

    // Clear all IRQs
    sd->SD_RISR_REG = 0xFFFFFFFF;

    sd->SD_BKSR_REG = card->blockLength;
    sd->SD_BYCR_REG = numBytes;

    // SD.pdf page 106, footnote 2 - High capacity cards use sector adressing instead of byte addressing
    uint32_t arg = ((card->flags & SD_FLAG_CCS) ? (address / card->blockLength) : address);

    if (sd_transfer_command(sd, SD_CMD_REG_DATA_TRANS | SD_CMD_REG_WAIT_PRE_OVER | SD_CMD_REG_CHK_RESP_CRC | SD_CMD_REG_RESP_RCV | 17, arg) != SD_OK)
    {
        printStr("(CMD17 fail)\n");
        return 0;
    }

     // Rec data
    uint32_t *data = (uint32_t*)buffer;
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
            printf("Data recv error %x4\n", irq);
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

    return 1;
}

int sdcard_write_sector(SD_T* sd, sdcard_info* card, uint8_t* buffer, uint64_t address, uint32_t numBytes)
{
    if (numBytes != card->blockLength)
    {
        printStr("Unable to write non sector sizes!");
        return 0;
    }

    if (address % card->blockLength != 0)
    {
        printStr("(Unaligned write!)");
        return 0;
    }

    sd->SD_GCTL_REG |= SD_GCTL_REG_FIFO_AC_MOD; // Enable AHB bus data access

    // Clear all IRQs
    sd->SD_RISR_REG = 0xFFFFFFFF;

    // TODO: Implement write command
    if (sd_transfer_command(sd, SD_CMD_REG_DATA_TRANS | SD_CMD_REG_WAIT_PRE_OVER | SD_CMD_REG_CHK_RESP_CRC | SD_CMD_REG_RESP_RCV | 17, address) != SD_OK)
    {
        printStr("(CMD17 fail)\n");
        return 0;
    }

    // Rec data
    uint32_t *data = (uint32_t*)buffer;
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
            printf("Data recv error %x4\n", irq);
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

    return 1;
}
