#ifndef __SDCARD_H__
#define __SDCARD_H__

#include <stdint.h>

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

int sdcard_detect(SD_T* sd, sdcard_info* card);
int sdcard_read_sector(SD_T* sd, sdcard_info* card, uint8_t* buffer, uint64_t address, uint32_t numBytes);
int sdcard_write_sector(SD_T* sd, sdcard_info* card, uint8_t* buffer, uint64_t address, uint32_t numBytes);

#endif
