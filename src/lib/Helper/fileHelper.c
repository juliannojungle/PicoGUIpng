#ifndef _FILE_HELPER_C_
#define _FILE_HELPER_C_

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "ff.h"
#include "sd_config.h"
#include "sd_hw_config.h" // Platform-specific default SPI/SD arrays + sd_get_num/sd_get_by_num
#include "platform_config.h" // Platform_SDCard_Init declaration

/* -----------------------------------------------------------------------
 * File system helper functions (parametrized by sd_card_t)
 * ----------------------------------------------------------------------- */

static FATFS fatfs;

bool MountSdCard(sd_card_t *sdcard) {
    if (!Platform_SDCard_Init()) {
        printf("Platform_SDCard_Init failed\n");
        return false;
    }

    FRESULT result = f_mount(&fatfs, sdcard->pcName, 1);
    if (result != FR_OK) {
        printf("f_mount error: %d\n", result);
        return false;
    }
    return true;
}

bool SelectActiveDrive(sd_card_t *sdcard) {
    FRESULT result = f_chdrive(sdcard->pcName);
    if (result != FR_OK) {
        printf("f_chdrive error: %d\n", result);
        f_unmount(sdcard->pcName);
        return false;
    }
    return true;
}

bool OpenFile(sd_card_t *sdcard, FIL *file, const char *filename) {
    FRESULT result = f_open(file, filename, FA_OPEN_EXISTING | FA_READ);
    if (result != FR_OK && result != FR_EXIST) {
        printf("f_open(%s) error: %d\n", filename, result);
        f_unmount(sdcard->pcName);
        return false;
    }
    return true;
}

void CloseFile(FIL *file) {
    FRESULT result = f_close(file);
    if (result != FR_OK) {
        printf("f_close error: %d\n", result);
    }
}

void UnMountSdCard(sd_card_t *sdcard) {
    f_unmount(sdcard->pcName);
}

#endif
