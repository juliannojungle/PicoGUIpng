#ifndef _SD_CONFIG_H_
#define _SD_CONFIG_H_

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"

/*-----------------------------------------------------------------------*/
/* SPI configuration                                                     */
/*-----------------------------------------------------------------------*/

typedef struct {
    spi_inst_t *hw_inst;    // SPI hardware instance (spi0, spi1)
    uint miso_gpio;
    uint mosi_gpio;
    uint sck_gpio;
    uint baud_rate;
} spi_t;

/*-----------------------------------------------------------------------*/
/* SD Card interface type                                                */
/*-----------------------------------------------------------------------*/

typedef enum {
    SD_IF_SPI
} sd_if_type_t;

/*-----------------------------------------------------------------------*/
/* SD Card SPI interface                                                 */
/*-----------------------------------------------------------------------*/

typedef struct {
    spi_t *spi;             // Pointer to the SPI driver instance
    uint ss_gpio;           // The SPI slave select GPIO for this SD card
} sd_spi_if_t;

/*-----------------------------------------------------------------------*/
/* SD Card configuration                                                 */
/*-----------------------------------------------------------------------*/

typedef struct {
    const char *pcName;         // Name used to mount device (e.g. "0:", "1:")
    sd_if_type_t type;          // Interface type
    sd_spi_if_t spi_if;         // SPI interface config
    bool use_card_detect;       // Whether to use card detect GPIO
    uint card_detect_gpio;      // Card detect GPIO pin
    bool card_detected_true;    // What the GPIO reads when card is present
} sd_card_t;

/*-----------------------------------------------------------------------*/
/* Macros                                                                */
/*-----------------------------------------------------------------------*/

#ifndef count_of
#define count_of(a) (sizeof(a) / sizeof((a)[0]))
#endif

/*-----------------------------------------------------------------------*/
/* API                                                                   */
/*-----------------------------------------------------------------------*/

size_t sd_get_num(void);
sd_card_t *sd_get_by_num(size_t num);

#endif /* _SD_CONFIG_H_ */
