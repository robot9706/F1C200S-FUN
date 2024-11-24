/*-----------------------------------------------------------------------*/
/* Low level disk I/O module SKELETON for FatFs     (C)ChaN, 2019        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include "ff.h"			/* Obtains integer types */
#include "diskio.h"		/* Declarations of disk functions */
#include "sdcard.h"
#include "f1c100s_sdc.h"
#include "print.h"

/* Definitions of physical drive number for each drive */
#define DEV_SD 0

static sdcard_info card = 
{
    .version = SD_VERSION_UNKNOWN,
};

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/
DSTATUS disk_status (
	BYTE pdrv		/* Physical drive nmuber to identify the drive */
)
{
    if (pdrv != DEV_SD)
    {
        return STA_NOINIT;
    }

    if (card.version == SD_VERSION_UNKNOWN)
    {
        return STA_NOINIT;
    }

	return 0;
}

/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/
DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive nmuber to identify the drive */
)
{
	if (pdrv != DEV_SD)
    {
        return STA_NOINIT;
    }

    printf("Diskio init drv=%d1\n", pdrv);

    SD_T* sd = (SD_T*)SDC0_BASE;
    if (sdcard_detect(sd, &card))
    {
        return 0;
    }

	return STA_NOINIT;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive nmuber to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	LBA_t sector,	/* Start sector in LBA */
	UINT count		/* Number of sectors to read */
)
{
	if (pdrv != DEV_SD)
    {
        return RES_PARERR;
    }

    SD_T* sd = (SD_T*)SDC0_BASE;

    printf("Diskio read drv=%d1, sector=0x%x4, count=0x%x4\n", pdrv, sector, count);
    for (UINT currentSector = 0; currentSector < count; currentSector++)
    {
        uint8_t *buffer = (uint8_t*)(buff + (currentSector * card.blockLength));
        uint64_t sectorAddress = (sector + currentSector) * card.blockLength;
        if (!sdcard_read_sector(sd, &card, buffer, sectorAddress, card.blockLength))
        {
            return RES_ERROR;
        }

        /*printStr("DATA:\n");
        for (int x = 0; x < 512; x++)
        {
            print8(buffer[x]);
            printChar(' ');

            if ((x + 1) % 16 == 0)
            {
                printChar('\n');
            }
        }*/
    }

	return RES_OK;
}

/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/
#if FF_FS_READONLY == 0

DRESULT disk_write (
	BYTE pdrv,			/* Physical drive nmuber to identify the drive */
	const BYTE *buff,	/* Data to be written */
	LBA_t sector,		/* Start sector in LBA */
	UINT count			/* Number of sectors to write */
)
{
	if (pdrv != DEV_SD)
    {
        return RES_PARERR;
    }

	return 0;
}

#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	if (pdrv != DEV_SD)
    {
        return RES_PARERR;
    }

    switch(cmd) {
        case CTRL_SYNC:
            return RES_OK;
            break;
        case GET_SECTOR_COUNT:
            *(DWORD*)buff = card.blockCount;
            return RES_OK;
            break;
        case GET_SECTOR_SIZE:
            *(WORD*)buff = card.blockLength;
            return RES_OK;
            break;
        case GET_BLOCK_SIZE: // Get erase block size
            *(DWORD*)buff = 1;
            return RES_OK;
            break;
        default:
            break;
    }

	return 0;
}

