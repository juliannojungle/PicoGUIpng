#ifndef _FILE_HELPER_C_
#define _FILE_HELPER_C_

#include <stdio.h>
#include <string.h>
#include "hw_config.h" // no-OS-FatFS declarations

// #include "fileHelper.h";

// static std::vector<spi_t *> spis;
// static std::vector<sd_card_t *> sd_cards;

static spi_t spis[] = {  // One for each SPI.
    {
        .hw_inst = spi0,
        .miso_gpio = 16,
        .mosi_gpio = 19,
        .sck_gpio = 18,
        .baud_rate = 25 * 1000 * 1000
    }};

static sd_card_t sd_cards[] = {  // One for each SD card
    {
        .pcName = "0:",   // Name used to mount device
        .type = SD_IF_SPI,
        .spi_if.spi = &spis[0],  // Pointer to the SPI driving this card
        .spi_if.ss_gpio = 17,    // The SPI slave select GPIO for this SD card
        .use_card_detect = true,
        .card_detect_gpio = 20,  // Card detect
        .card_detected_true = 0  // What the GPIO read returns when a card is present.
    }};

/* BEGIN no-OS-FatFS implementations */
size_t sd_get_num(void) {
    return count_of(sd_cards);
}

sd_card_t *sd_get_by_num(size_t num) {
    if (num <= sd_get_num()) {
        // return sd_cards[num];
        return &sd_cards[num];
    } else {
        return NULL;
    }
}

size_t spi_get_num(void) {
    // return spis.size();
    return count_of(spis);
}

spi_t *spi_get_by_num(size_t num) {
    if (num <= spi_get_num()) {
        // return spis[num];
        return &spis[num];
    } else {
        return NULL;
    }
}

// void add_spi(spi_t *spi) {
//     spis.push_back(spi);
// }

// void add_sd_card(sd_card_t *sd_card) {
//     sd_cards.push_back(sd_card);
// }

// void SetupSpi(void) {
//     spi_t *p_spi = new spi_t;
//     memset(p_spi, 0, sizeof(spi_t));

//     if (!p_spi) panic("Out of memory");

//     p_spi->hw_inst = spi0;
//     p_spi->miso_gpio = 16;
//     p_spi->mosi_gpio = 19;
//     p_spi->sck_gpio = 18;
//     p_spi->baud_rate = 25 * 1000 * 1000;  // Actual frequency: 20833333.
//     add_spi(p_spi);
// }

// void SetupSdCard(void) {
//     sd_card_t *p_sd_card = new sd_card_t;
//     memset(p_sd_card, 0, sizeof(sd_card_t));

//     if (!p_sd_card) panic("Out of memory");

//     p_sd_card->pcName = "0:";
//     p_sd_card->type = SD_IF_SPI,
//     p_sd_card->spi_if.spi = spi_get_by_num(0);
//     p_sd_card->spi_if.ss_gpio = 17;
//     p_sd_card->use_card_detect = true;
//     p_sd_card->card_detect_gpio = 20;
//     p_sd_card->card_detected_true = 0; // What the GPIO_read returns when a sdcard is present.
//     add_sd_card(p_sd_card);
// }
/* END no-OS-FatFS implementations */

bool MountSdCard(sd_card_t *pSD) {
#if _DEBUG
    printf("mounting sdcard...\n");
#endif
    FRESULT result = f_mount(&pSD->fatfs, pSD->pcName, 1);

    if (result != FR_OK) {
        printf("f_mount error: %s (%d)\n", FRESULT_str(result), result);
        return false;
    }

    return true;
}

bool SelectActiveDrive(sd_card_t * pSD) {
#if _DEBUG
    printf("selecting active drive...\n");
#endif
    FRESULT result = f_chdrive(pSD->pcName);

    if (result != FR_OK) {
        printf("f_chdrive error: %s (%d)\n", FRESULT_str(result), result);
        f_unmount(pSD->pcName);
        return false;
    }

    return true;
}

bool OpenFile(sd_card_t * pSD, FIL *file, const char *filename) {
#if _DEBUG
    printf("opening file...\n");
#endif
    FRESULT result = f_open(file, filename, FA_OPEN_EXISTING | FA_READ);

    if (result != FR_OK && result != FR_EXIST) {
        printf("f_open(%s) error: %s (%d)\n", filename, FRESULT_str(result), result);
        f_unmount(pSD->pcName);
        return false;
    }

    return true;
}

void CloseFile(FIL *file) {
#if _DEBUG
    printf("closing file...\n");
#endif
    FRESULT result = f_close(file);

    if (result != FR_OK) {
        printf("f_close error: %s (%d)\n", FRESULT_str(result), result);
    }
}

void UnMountSdCard(sd_card_t *sdcard) {
#if _DEBUG
    printf("unmounting sdcard...\n");
#endif
    f_unmount(sdcard->pcName);
}

#endif