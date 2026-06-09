#ifndef _FILE_HELPER_C_
#define _FILE_HELPER_C_

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "ff.h"
#include "sd_config.h"

/* -----------------------------------------------------------------------
 * Default SPI and SD card configuration.
 * Users can expand these arrays to support multiple SPIs / SD cards.
 * ----------------------------------------------------------------------- */

static spi_t spis[] = {  // One for each SPI.
    {
        .hw_inst = spi0,
        .miso_gpio = 16,
        .mosi_gpio = 19,
        .sck_gpio = 18,
        .baud_rate = 25 * 1000 * 1000
    }
};

static sd_card_t sd_cards[] = {  // One for each SD card.
    {
        .pcName = "0:",             // Name used to mount device
        .type = SD_IF_SPI,
        .spi_if.spi = &spis[0],    // Pointer to the SPI driver
        .spi_if.ss_gpio = 17,      // The SPI slave select GPIO
        .use_card_detect = true,
        .card_detect_gpio = 20,    // Card detect GPIO
        .card_detected_true = 0    // What the GPIO reads when card is present
    }
};

/* BEGIN no-OS-FatFS-compatible implementations */

size_t sd_get_num(void) {
    return count_of(sd_cards);
}

sd_card_t *sd_get_by_num(size_t num) {
    if (num < sd_get_num()) {
        return &sd_cards[num];
    } else {
        return NULL;
    }
}

/* END no-OS-FatFS-compatible implementations */

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
