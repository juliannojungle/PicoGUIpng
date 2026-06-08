#ifndef PLATFORM_CONFIG_H
#define PLATFORM_CONFIG_H

#include <stdbool.h>

#ifndef SD_SPI_SCLK
#define SD_SPI_SCLK 18
#endif
#ifndef SD_SPI_MOSI
#define SD_SPI_MOSI 19
#endif
#ifndef SD_SPI_MISO
#define SD_SPI_MISO 16
#endif
#ifndef SD_SPI_CS
#define SD_SPI_CS 17
#endif

#ifndef SD_SPI_BAUDRATE
#define SD_SPI_BAUDRATE (25 * 1000 * 1000)
#endif

bool Platform_SDCard_Init(void);

#endif /* PLATFORM_CONFIG_H */
