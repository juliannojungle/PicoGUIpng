#ifndef _FILE_HELPER_C_
#define _FILE_HELPER_C_

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "ff.h"
#include "platform_config.h"

static FATFS fatfs;
static const char* sd_drive = "0:";

bool MountSdCard(void) {
    if (!Platform_SDCard_Init()) {
        printf("Platform_SDCard_Init failed\n");
        return false;
    }

    FRESULT result = f_mount(&fatfs, sd_drive, 1);
    if (result != FR_OK) {
        printf("f_mount error: %d\n", result);
        return false;
    }
    return true;
}

bool SelectActiveDrive(void) {
    FRESULT result = f_chdrive(sd_drive);
    if (result != FR_OK) {
        printf("f_chdrive error: %d\n", result);
        f_unmount(sd_drive);
        return false;
    }
    return true;
}

bool OpenFile(FIL *file, const char *filename) {
    FRESULT result = f_open(file, filename, FA_OPEN_EXISTING | FA_READ);
    if (result != FR_OK && result != FR_EXIST) {
        printf("f_open(%s) error: %d\n", filename, result);
        f_unmount(sd_drive);
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

void UnMountSdCard(void) {
    f_unmount(sd_drive);
}

#endif
