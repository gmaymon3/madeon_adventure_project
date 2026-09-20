/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    user_diskio.c
  * @brief   SD card disk I/O driver using SPI
  ******************************************************************************
  */
/* USER CODE END Header */

#include "user_diskio.h"
#include "main.h"
#include "ff_gen_drv.h"

#include <string.h>
volatile uint8_t sd_debug_stage = 0;
volatile uint8_t sd_debug_response = 0;

/* SPI peripheral used for SD card */
extern SPI_HandleTypeDef hspi2;


/* SD commands */
#define CMD0     (0)
#define CMD1     (1)
#define CMD8     (8)
#define CMD9     (9)
#define CMD10    (10)
#define CMD12    (12)
#define CMD16    (16)
#define CMD17    (17)
#define CMD18    (18)
#define CMD24    (24)
#define CMD25    (25)
#define CMD55    (55)
#define CMD58    (58)

#define ACMD41   (41)

#define MY_SD_CS_GPIO_Port GPIOC
#define MY_SD_CS_Pin       GPIO_PIN_0

#define MY_SD_CS_LOW() \
    HAL_GPIO_WritePin(MY_SD_CS_GPIO_Port, MY_SD_CS_Pin, GPIO_PIN_RESET)

#define MY_SD_CS_HIGH() \
    HAL_GPIO_WritePin(MY_SD_CS_GPIO_Port, MY_SD_CS_Pin, GPIO_PIN_SET)

/* -------------------------------------------------------------------------- */
/* SPI helper                                                                 */
/* -------------------------------------------------------------------------- */

static uint8_t SD_SPI_TxRx(uint8_t data)
{
    uint8_t rx = 0xFF;

    HAL_SPI_TransmitReceive(
        &hspi2,
        &data,
        &rx,
        1,
        HAL_MAX_DELAY
    );

    return rx;
}



/* -------------------------------------------------------------------------- */
/* Send SD command                                                            */
/* -------------------------------------------------------------------------- */

static uint8_t SD_SendCommand(uint8_t cmd, uint32_t arg)
{
    uint8_t response;

    /* Command packet */
    SD_SPI_TxRx(0x40 | cmd);

    SD_SPI_TxRx((uint8_t)(arg >> 24));
    SD_SPI_TxRx((uint8_t)(arg >> 16));
    SD_SPI_TxRx((uint8_t)(arg >> 8));
    SD_SPI_TxRx((uint8_t)(arg));

    /* Valid CRC is required for CMD0 and CMD8 */
    if (cmd == CMD0)
    {
        SD_SPI_TxRx(0x95);
    }
    else if (cmd == CMD8)
    {
        SD_SPI_TxRx(0x87);
    }
    else
    {
        SD_SPI_TxRx(0x01);
    }

    /* Wait for response */
    for (uint8_t i = 0; i < 10; i++)
    {
        response = SD_SPI_TxRx(0xFF);

        if ((response & 0x80) == 0)
        {
            return response;
        }
    }

    return 0xFF;
}


/* -------------------------------------------------------------------------- */
/* Initialize SD card                                                         */
/* -------------------------------------------------------------------------- */

static uint8_t SD_Init(void)
{
    uint8_t response;
    uint8_t r7[4];

    sd_debug_stage = 1;

    MY_SD_CS_HIGH();

    for (uint8_t i = 0; i < 10; i++)
    {
        SD_SPI_TxRx(0xFF);
    }

    // CMD0
    sd_debug_stage = 2;

    MY_SD_CS_LOW();
    response = SD_SendCommand(CMD0, 0);

    sd_debug_response = response;

    MY_SD_CS_HIGH();
    SD_SPI_TxRx(0xFF);

    if (response != 0x01)
    {
        return 1;
    }

    // CMD8
    sd_debug_stage = 3;

    MY_SD_CS_LOW();
    response = SD_SendCommand(CMD8, 0x000001AA);

    sd_debug_response = response;

    if (response == 0x01)
    {
        for (uint8_t i = 0; i < 4; i++)
        {
            r7[i] = SD_SPI_TxRx(0xFF);
        }

        MY_SD_CS_HIGH();
        SD_SPI_TxRx(0xFF);

        if (r7[2] != 0x01 || r7[3] != 0xAA)
        {
            sd_debug_stage = 4;
            sd_debug_response = r7[3];
            return 1;
        }

        // ACMD41
        uint32_t timeout = HAL_GetTick() + 1000;

        do
        {
            sd_debug_stage = 5;

            MY_SD_CS_LOW();
            response = SD_SendCommand(CMD55, 0);

            MY_SD_CS_HIGH();
            SD_SPI_TxRx(0xFF);

            sd_debug_stage = 6;

            MY_SD_CS_LOW();
            response = SD_SendCommand(ACMD41, 0x40000000);

            sd_debug_response = response;

            MY_SD_CS_HIGH();
            SD_SPI_TxRx(0xFF);

            if (response == 0x00)
            {
                break;
            }

            if (HAL_GetTick() > timeout)
            {
                sd_debug_stage = 7;
                return 1;
            }

        } while (1);

        // CMD58
        sd_debug_stage = 8;

        MY_SD_CS_LOW();

        response = SD_SendCommand(CMD58, 0);

        sd_debug_response = response;

        if (response != 0x00)
        {
            MY_SD_CS_HIGH();
            SD_SPI_TxRx(0xFF);
            return 1;
        }

        for (uint8_t i = 0; i < 4; i++)
        {
            SD_SPI_TxRx(0xFF);
        }

        MY_SD_CS_HIGH();
        SD_SPI_TxRx(0xFF);

        sd_debug_stage = 9;

        return 0;
    }

    MY_SD_CS_HIGH();
    SD_SPI_TxRx(0xFF);

    return 1;
}


/* -------------------------------------------------------------------------- */
/* Read one 512-byte block                                                   */
/* -------------------------------------------------------------------------- */

static uint8_t SD_ReadBlock(uint8_t *buffer, uint32_t sector)
{
    uint8_t response;

    MY_SD_CS_LOW();

    response = SD_SendCommand(CMD17, sector);

    if (response != 0x00)
    {
        MY_SD_CS_HIGH();
        SD_SPI_TxRx(0xFF);
        return 1;
    }

    /* Wait for data token */
    uint32_t timeout = HAL_GetTick() + 1000;

    do
    {
        response = SD_SPI_TxRx(0xFF);

        if (response == 0xFE)
        {
            break;
        }

        if (HAL_GetTick() > timeout)
        {
            MY_SD_CS_HIGH();
            SD_SPI_TxRx(0xFF);
            return 1;
        }

    } while (1);


    /* Read 512 bytes */
    for (uint16_t i = 0; i < 512; i++)
    {
        buffer[i] = SD_SPI_TxRx(0xFF);
    }

    /* Read CRC */
    SD_SPI_TxRx(0xFF);
    SD_SPI_TxRx(0xFF);

    MY_SD_CS_HIGH();
    SD_SPI_TxRx(0xFF);

    return 0;
}


/* -------------------------------------------------------------------------- */
/* FatFs initialize                                                           */
/* -------------------------------------------------------------------------- */

DSTATUS USER_initialize(BYTE pdrv)
{
    if (pdrv != 0)
    {
        return STA_NOINIT;
    }

    if (SD_Init() != 0)
    {
        return STA_NOINIT;
    }

    return 0;
}


/* -------------------------------------------------------------------------- */
/* FatFs status                                                               */
/* -------------------------------------------------------------------------- */

DSTATUS USER_status(BYTE pdrv)
{
    if (pdrv != 0)
    {
        return STA_NOINIT;
    }

    return 0;
}


/* -------------------------------------------------------------------------- */
/* FatFs read                                                                 */
/* -------------------------------------------------------------------------- */

DRESULT USER_read(
    BYTE pdrv,
    BYTE *buff,
    DWORD sector,
    UINT count)
{
    if (pdrv != 0)
    {
        return RES_PARERR;
    }

    if (buff == NULL || count == 0)
    {
        return RES_PARERR;
    }

    for (UINT i = 0; i < count; i++)
    {
        if (SD_ReadBlock(
                &buff[i * 512],
                sector + i) != 0)
        {
            return RES_ERROR;
        }
    }

    return RES_OK;
}


/* -------------------------------------------------------------------------- */
/* FatFs write                                                                */
/* -------------------------------------------------------------------------- */

#if _USE_WRITE == 1

DRESULT USER_write(
    BYTE pdrv,
    const BYTE *buff,
    DWORD sector,
    UINT count)
{
    /*
     * Read-only for now.
     */
    (void)pdrv;
    (void)buff;
    (void)sector;
    (void)count;

    return RES_WRPRT;
}

#endif


/* -------------------------------------------------------------------------- */
/* FatFs IOCTL                                                                */
/* -------------------------------------------------------------------------- */

#if _USE_IOCTL == 1

DRESULT USER_ioctl(
    BYTE pdrv,
    BYTE cmd,
    void *buff)
{
    if (pdrv != 0)
    {
        return RES_PARERR;
    }

    switch (cmd)
    {
        case CTRL_SYNC:
            return RES_OK;

        case GET_SECTOR_COUNT:
            /*
             * Not implemented yet.
             */
            return RES_PARERR;

        case GET_SECTOR_SIZE:
            *(WORD *)buff = 512;
            return RES_OK;

        case GET_BLOCK_SIZE:
            *(DWORD *)buff = 1;
            return RES_OK;

        default:
            return RES_PARERR;
    }
}

#endif


/* -------------------------------------------------------------------------- */
/* FatFs driver table                                                         */
/* -------------------------------------------------------------------------- */

Diskio_drvTypeDef USER_Driver =
{
    USER_initialize,
    USER_status,
    USER_read,

#if _USE_WRITE == 1
    USER_write,
#endif

#if _USE_IOCTL == 1
    USER_ioctl,
#endif
};
