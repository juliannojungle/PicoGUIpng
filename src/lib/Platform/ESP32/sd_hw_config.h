#ifndef _SD_HW_CONFIG_H_
#define _SD_HW_CONFIG_H_

#include "sd_config.h"

/* -----------------------------------------------------------------------
 * Default SPI and SD card configuration for ESP32.
 * Users can expand these arrays to support multiple SPIs / SD cards.
 * ----------------------------------------------------------------------- */

static spi_t spis[] = {  // One for each SPI.
    {
        .host_id = 2,           // SPI2_HOST
        .miso_gpio = 19,
        .mosi_gpio = 23,
        .sck_gpio = 18,
        .baud_rate = 25 * 1000 * 1000
    }
};

static sd_card_t sd_cards[] = {  // One for each SD card.
    {
        .pcName = "0:",             // Name used to mount device
        .type = SD_IF_SPI,
        .spi_if.spi = &spis[0],    // Pointer to the SPI driver
        .spi_if.ss_gpio = 5,       // The SPI slave select GPIO
        .use_card_detect = true,
        .card_detect_gpio = 21,    // Card detect GPIO
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

#endif /* _SD_HW_CONFIG_H_ */
