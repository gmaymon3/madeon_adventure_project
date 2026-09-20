/* USER CODE BEGIN Header */

/**
  ******************************************************************************
  * @file    user_diskio.h
  * @brief   This file contains the common defines and functions prototypes for
  *          the user_diskio driver.
  ******************************************************************************
  */

/* USER CODE END Header */

#ifndef __USER_DISKIO_H
#define __USER_DISKIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ff_gen_drv.h"
#include <stdint.h>

/* Exported functions -------------------------------------------------------- */

DSTATUS USER_initialize(BYTE pdrv);

DSTATUS USER_status(BYTE pdrv);

DRESULT USER_read(
    BYTE pdrv,
    BYTE *buff,
    DWORD sector,
    UINT count
);

#if _USE_WRITE == 1

DRESULT USER_write(
    BYTE pdrv,
    const BYTE *buff,
    DWORD sector,
    UINT count
);

#endif

#if _USE_IOCTL == 1

DRESULT USER_ioctl(
    BYTE pdrv,
    BYTE cmd,
    void *buff
);

#endif

/* Debug information -------------------------------------------------------- */

extern volatile uint8_t sd_debug_stage;
extern volatile uint8_t sd_debug_response;

/* FatFs driver ------------------------------------------------------------- */

extern Diskio_drvTypeDef USER_Driver;

#ifdef __cplusplus
}
#endif

#endif /* __USER_DISKIO_H */
