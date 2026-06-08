/*-----------------------------------------------------------------------/
/  Low level disk I/O module for RP2040 (Pico SDK)                       /
/  Part of Multi-Platform FatFS integration                              /
/-----------------------------------------------------------------------*/

#include "ff.h"
#include "diskio.h"
#include "platform_config.h"

#ifndef DISKIO_TEST_MODE
#include "hardware/spi.h"
#include "hardware/gpio.h"
#else
/* Provide 'uint' typedef used by Pico SDK types when in test mode */
typedef unsigned int uint;
#endif

#include <string.h>

/*-----------------------------------------------------------------------*/
/* SPI Operations Interface (injectable for testing)                      */
/*-----------------------------------------------------------------------*/

typedef struct {
    int (*spi_init)(uint32_t baudrate);
    int (*spi_transfer)(const uint8_t* tx, uint8_t* rx, size_t len);
    void (*gpio_set)(uint pin, uint value);
} spi_ops_t;

/*-----------------------------------------------------------------------*/
/* SD Card State                                                         */
/*-----------------------------------------------------------------------*/

typedef struct {
    bool initialized;
    uint32_t sector_count;
    uint16_t sector_size;
    uint32_t block_size;
} sd_card_state_t;

static sd_card_state_t sd_state = {
    .initialized = false,
    .sector_count = 0,
    .sector_size = 512,
    .block_size = 512
};

/*-----------------------------------------------------------------------*/
/* SD Card Commands                                                       */
/*-----------------------------------------------------------------------*/

#define CMD0    0x40    /* GO_IDLE_STATE */
#define CMD8    0x48    /* SEND_IF_COND */
#define CMD9    0x49    /* SEND_CSD */
#define CMD12   0x4C    /* STOP_TRANSMISSION */
#define CMD17   0x51    /* READ_SINGLE_BLOCK */
#define CMD18   0x52    /* READ_MULTIPLE_BLOCK */
#define CMD24   0x58    /* WRITE_BLOCK */
#define CMD25   0x59    /* WRITE_MULTIPLE_BLOCK */
#define CMD55   0x77    /* APP_CMD */
#define CMD58   0x7A    /* READ_OCR */
#define ACMD41  0x69    /* SD_SEND_OP_COND */

#define SD_INIT_TIMEOUT     1000
#define SD_CMD_TIMEOUT      100
#define SD_DATA_TOKEN       0xFE
#define SD_MULTI_TOKEN      0xFC
#define SD_STOP_TOKEN       0xFD

/*-----------------------------------------------------------------------*/
/* Default SPI operations using Pico SDK hardware                        */
/*-----------------------------------------------------------------------*/

#ifndef DISKIO_TEST_MODE

static int default_spi_init(uint32_t baudrate) {
    spi_init(spi0, baudrate);

    gpio_set_function(SD_SPI_SCLK, GPIO_FUNC_SPI);
    gpio_set_function(SD_SPI_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(SD_SPI_MISO, GPIO_FUNC_SPI);

    gpio_init(SD_SPI_CS);
    gpio_set_dir(SD_SPI_CS, GPIO_OUT);
    gpio_put(SD_SPI_CS, 1);

    return 0;
}

static int default_spi_transfer(const uint8_t* tx, uint8_t* rx, size_t len) {
    int result;
    if (tx && rx) {
        result = spi_write_read_blocking(spi0, tx, rx, len);
    } else if (tx) {
        result = spi_write_blocking(spi0, tx, len);
    } else if (rx) {
        result = spi_read_blocking(spi0, 0xFF, rx, len);
    } else {
        return -1;
    }
    return (result == (int)len) ? 0 : -1;
}

static void default_gpio_set(uint pin, uint value) {
    gpio_put(pin, value);
}

static const spi_ops_t default_ops = {
    .spi_init = default_spi_init,
    .spi_transfer = default_spi_transfer,
    .gpio_set = default_gpio_set
};

#endif /* DISKIO_TEST_MODE */

/*-----------------------------------------------------------------------*/
/* Active SPI operations pointer                                         */
/*-----------------------------------------------------------------------*/

#ifdef DISKIO_TEST_MODE
static const spi_ops_t* spi_ops = NULL;
#else
static const spi_ops_t* spi_ops = &default_ops;
#endif

/**
 * Inject custom SPI operations (for testing).
 * Pass NULL to restore default hardware ops.
 */
void diskio_set_spi_ops(const spi_ops_t* ops) {
#ifdef DISKIO_TEST_MODE
    spi_ops = ops;
#else
    spi_ops = ops ? ops : &default_ops;
#endif
}

/**
 * Reset internal disk state (for testing).
 */
void diskio_reset_state(void) {
    sd_state.initialized = false;
    sd_state.sector_count = 0;
    sd_state.sector_size = 512;
    sd_state.block_size = 512;
}

/*-----------------------------------------------------------------------*/
/* Internal SPI helpers                                                   */
/*-----------------------------------------------------------------------*/

static void cs_select(void) {
    spi_ops->gpio_set(SD_SPI_CS, 0);
}

static void cs_deselect(void) {
    spi_ops->gpio_set(SD_SPI_CS, 1);
}

static uint8_t spi_exchange_byte(uint8_t tx) {
    uint8_t rx = 0xFF;
    spi_ops->spi_transfer(&tx, &rx, 1);
    return rx;
}

static int spi_send_bytes(const uint8_t* data, size_t len) {
    return spi_ops->spi_transfer(data, NULL, len);
}

static int spi_recv_bytes(uint8_t* data, size_t len) {
    return spi_ops->spi_transfer(NULL, data, len);
}

/**
 * Send an SD command and return the R1 response.
 */
static uint8_t sd_send_cmd(uint8_t cmd, uint32_t arg) {
    uint8_t frame[6];
    uint8_t response;
    int timeout = SD_CMD_TIMEOUT;

    frame[0] = cmd;
    frame[1] = (uint8_t)(arg >> 24);
    frame[2] = (uint8_t)(arg >> 16);
    frame[3] = (uint8_t)(arg >> 8);
    frame[4] = (uint8_t)(arg);

    /* CRC for CMD0 and CMD8 */
    if (cmd == CMD0) {
        frame[5] = 0x95;
    } else if (cmd == CMD8) {
        frame[5] = 0x87;
    } else {
        frame[5] = 0x01; /* Dummy CRC + stop bit */
    }

    /* Send command frame */
    spi_send_bytes(frame, 6);

    /* Wait for response (not 0xFF) */
    do {
        response = spi_exchange_byte(0xFF);
    } while ((response & 0x80) && --timeout > 0);

    return response;
}

/**
 * Send application-specific command (CMD55 prefix).
 */
static uint8_t sd_send_acmd(uint8_t cmd, uint32_t arg) {
    sd_send_cmd(CMD55, 0);
    return sd_send_cmd(cmd, arg);
}

/**
 * Wait for a data token from the card.
 */
static int sd_wait_data_token(uint8_t token) {
    int timeout = SD_INIT_TIMEOUT;
    uint8_t response;

    do {
        response = spi_exchange_byte(0xFF);
        if (response == token) return 0;
        if (response != 0xFF) return -1; /* Error token */
    } while (--timeout > 0);

    return -1; /* Timeout */
}

/**
 * Read CSD register to determine card capacity.
 */
static int sd_read_csd(void) {
    uint8_t csd[16];
    uint8_t response;

    cs_select();
    response = sd_send_cmd(CMD9, 0);
    if (response != 0x00) {
        cs_deselect();
        return -1;
    }

    if (sd_wait_data_token(SD_DATA_TOKEN) != 0) {
        cs_deselect();
        return -1;
    }

    if (spi_recv_bytes(csd, 16) != 0) {
        cs_deselect();
        return -1;
    }

    /* Read and discard 2-byte CRC */
    spi_exchange_byte(0xFF);
    spi_exchange_byte(0xFF);
    cs_deselect();

    /* Parse CSD to get sector count */
    uint8_t csd_ver = (csd[0] >> 6) & 0x03;
    if (csd_ver == 1) {
        /* CSD Version 2.0 (SDHC/SDXC) */
        uint32_t c_size = ((uint32_t)(csd[7] & 0x3F) << 16) |
                          ((uint32_t)csd[8] << 8) |
                          (uint32_t)csd[9];
        sd_state.sector_count = (c_size + 1) * 1024;
    } else {
        /* CSD Version 1.0 (SDSC) */
        uint32_t c_size = ((uint32_t)(csd[6] & 0x03) << 10) |
                          ((uint32_t)csd[7] << 2) |
                          ((uint32_t)(csd[8] >> 6) & 0x03);
        uint32_t c_size_mult = ((uint32_t)(csd[9] & 0x03) << 1) |
                               ((uint32_t)(csd[10] >> 7) & 0x01);
        uint32_t read_bl_len = csd[5] & 0x0F;
        uint32_t block_nr = (c_size + 1) * (1u << (c_size_mult + 2));
        uint32_t block_len = 1u << read_bl_len;
        sd_state.sector_count = (block_nr * block_len) / 512;
    }

    sd_state.sector_size = 512;
    sd_state.block_size = 512;

    return 0;
}

/*-----------------------------------------------------------------------*/
/* Public FatFS diskio interface                                          */
/*-----------------------------------------------------------------------*/

/**
 * Initialize the SD card via SPI.
 */
DSTATUS disk_initialize(BYTE pdrv) {
    uint8_t response;
    int timeout;

    if (pdrv != 0) return STA_NOINIT;
    if (!spi_ops) return STA_NOINIT;

    /* Initialize SPI at slow speed for card detection */
    if (spi_ops->spi_init(400000) != 0) {
        return STA_NOINIT;
    }

    /* Send 80+ clock cycles with CS high to enter SPI mode */
    cs_deselect();
    uint8_t dummy[10];
    memset(dummy, 0xFF, sizeof(dummy));
    if (spi_ops->spi_transfer(dummy, NULL, 10) != 0) {
        return STA_NOINIT;
    }

    /* CMD0: GO_IDLE_STATE */
    cs_select();
    response = sd_send_cmd(CMD0, 0);
    cs_deselect();
    spi_exchange_byte(0xFF);

    if (response != 0x01) {
        return STA_NOINIT;
    }

    /* CMD8: SEND_IF_COND (check voltage range) */
    cs_select();
    response = sd_send_cmd(CMD8, 0x000001AA);
    if (response == 0x01) {
        /* SDv2 card: read 4-byte R7 response */
        uint8_t r7[4];
        spi_recv_bytes(r7, 4);
        cs_deselect();
        spi_exchange_byte(0xFF);

        if (r7[2] != 0x01 || r7[3] != 0xAA) {
            return STA_NOINIT;
        }
    } else {
        cs_deselect();
        spi_exchange_byte(0xFF);
        /* SDv1 or MMC — continue without CMD8 validation */
    }

    /* ACMD41: SD_SEND_OP_COND (wait for card ready) */
    timeout = SD_INIT_TIMEOUT;
    do {
        cs_select();
        response = sd_send_acmd(ACMD41, 0x40000000);
        cs_deselect();
        spi_exchange_byte(0xFF);
    } while (response != 0x00 && --timeout > 0);

    if (response != 0x00) {
        return STA_NOINIT;
    }

    /* Read CSD to get card capacity */
    if (sd_read_csd() != 0) {
        return STA_NOINIT;
    }

    /* Switch to full speed */
    if (spi_ops->spi_init(SD_SPI_BAUDRATE) != 0) {
        return STA_NOINIT;
    }

    sd_state.initialized = true;
    return 0;
}

/**
 * Return disk status.
 */
DSTATUS disk_status(BYTE pdrv) {
    if (pdrv != 0) return STA_NOINIT;
    return sd_state.initialized ? 0 : STA_NOINIT;
}

/**
 * Read sector(s) from the SD card.
 */
DRESULT disk_read(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count) {
    if (pdrv != 0) return RES_PARERR;
    if (!sd_state.initialized) return RES_NOTRDY;
    if (!spi_ops) return RES_NOTRDY;
    if (sector + count > sd_state.sector_count) return RES_PARERR;
    if (count == 0) return RES_PARERR;

    /* Convert sector to byte address for SDSC (SDHC uses block address) */
    uint32_t addr = (uint32_t)sector;

    if (count == 1) {
        /* Single block read: CMD17 */
        cs_select();
        if (sd_send_cmd(CMD17, addr) != 0x00) {
            cs_deselect();
            return RES_ERROR;
        }

        if (sd_wait_data_token(SD_DATA_TOKEN) != 0) {
            cs_deselect();
            return RES_ERROR;
        }

        if (spi_recv_bytes(buff, sd_state.sector_size) != 0) {
            cs_deselect();
            return RES_ERROR;
        }

        /* Discard CRC */
        spi_exchange_byte(0xFF);
        spi_exchange_byte(0xFF);
        cs_deselect();
        spi_exchange_byte(0xFF);
    } else {
        /* Multi-block read: CMD18 */
        cs_select();
        if (sd_send_cmd(CMD18, addr) != 0x00) {
            cs_deselect();
            return RES_ERROR;
        }

        for (UINT i = 0; i < count; i++) {
            if (sd_wait_data_token(SD_DATA_TOKEN) != 0) {
                sd_send_cmd(CMD12, 0);
                cs_deselect();
                return RES_ERROR;
            }

            if (spi_recv_bytes(buff + (i * sd_state.sector_size), sd_state.sector_size) != 0) {
                sd_send_cmd(CMD12, 0);
                cs_deselect();
                return RES_ERROR;
            }

            /* Discard CRC */
            spi_exchange_byte(0xFF);
            spi_exchange_byte(0xFF);
        }

        /* CMD12: STOP_TRANSMISSION */
        sd_send_cmd(CMD12, 0);
        cs_deselect();
        spi_exchange_byte(0xFF);
    }

    return RES_OK;
}

/**
 * Write sector(s) to the SD card.
 */
DRESULT disk_write(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count) {
    if (pdrv != 0) return RES_PARERR;
    if (!sd_state.initialized) return RES_NOTRDY;
    if (!spi_ops) return RES_NOTRDY;
    if (sector + count > sd_state.sector_count) return RES_PARERR;
    if (count == 0) return RES_PARERR;

    /* Convert sector to byte address for SDSC (SDHC uses block address) */
    uint32_t addr = (uint32_t)sector;
    uint8_t response;

    if (count == 1) {
        /* Single block write: CMD24 */
        cs_select();
        if (sd_send_cmd(CMD24, addr) != 0x00) {
            cs_deselect();
            return RES_ERROR;
        }

        /* Send data token */
        spi_exchange_byte(SD_DATA_TOKEN);

        /* Send data */
        if (spi_send_bytes(buff, sd_state.sector_size) != 0) {
            cs_deselect();
            return RES_ERROR;
        }

        /* Send dummy CRC */
        spi_exchange_byte(0xFF);
        spi_exchange_byte(0xFF);

        /* Check data response */
        response = spi_exchange_byte(0xFF);
        if ((response & 0x1F) != 0x05) {
            cs_deselect();
            return RES_ERROR;
        }

        /* Wait for card to finish programming */
        int timeout = SD_INIT_TIMEOUT;
        while (spi_exchange_byte(0xFF) == 0x00 && --timeout > 0);
        cs_deselect();
        spi_exchange_byte(0xFF);

        if (timeout == 0) return RES_ERROR;
    } else {
        /* Multi-block write: CMD25 */
        cs_select();
        if (sd_send_cmd(CMD25, addr) != 0x00) {
            cs_deselect();
            return RES_ERROR;
        }

        for (UINT i = 0; i < count; i++) {
            /* Send multi-block data token */
            spi_exchange_byte(SD_MULTI_TOKEN);

            /* Send data */
            if (spi_send_bytes(buff + (i * sd_state.sector_size), sd_state.sector_size) != 0) {
                /* Send stop token on failure */
                spi_exchange_byte(SD_STOP_TOKEN);
                cs_deselect();
                return RES_ERROR;
            }

            /* Send dummy CRC */
            spi_exchange_byte(0xFF);
            spi_exchange_byte(0xFF);

            /* Check data response */
            response = spi_exchange_byte(0xFF);
            if ((response & 0x1F) != 0x05) {
                spi_exchange_byte(SD_STOP_TOKEN);
                cs_deselect();
                return RES_ERROR;
            }

            /* Wait for card to finish programming */
            int timeout = SD_INIT_TIMEOUT;
            while (spi_exchange_byte(0xFF) == 0x00 && --timeout > 0);
            if (timeout == 0) {
                spi_exchange_byte(SD_STOP_TOKEN);
                cs_deselect();
                return RES_ERROR;
            }
        }

        /* Send stop token */
        spi_exchange_byte(SD_STOP_TOKEN);

        /* Wait for card ready */
        int timeout = SD_INIT_TIMEOUT;
        while (spi_exchange_byte(0xFF) == 0x00 && --timeout > 0);
        cs_deselect();
        spi_exchange_byte(0xFF);

        if (timeout == 0) return RES_ERROR;
    }

    return RES_OK;
}

/**
 * Disk I/O control.
 */
DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff) {
    if (pdrv != 0) return RES_PARERR;
    if (!sd_state.initialized) return RES_NOTRDY;

    switch (cmd) {
    case CTRL_SYNC:
        /* Nothing to flush — SPI writes are synchronous */
        return RES_OK;

    case GET_SECTOR_COUNT:
        *(LBA_t*)buff = sd_state.sector_count;
        return RES_OK;

    case GET_SECTOR_SIZE:
        *(WORD*)buff = sd_state.sector_size;
        return RES_OK;

    case GET_BLOCK_SIZE:
        *(DWORD*)buff = sd_state.block_size;
        return RES_OK;

    default:
        return RES_PARERR;
    }
}

/*-----------------------------------------------------------------------*/
/* Platform SD Card Initialization                                       */
/*-----------------------------------------------------------------------*/

/**
 * Initialize the SPI peripheral and SD card for the RP2040 platform.
 * Configures SPI pins, CS GPIO, and performs full card initialization.
 * Must be called before f_mount().
 *
 * @return true if initialization succeeded, false otherwise.
 */
bool Platform_SDCard_Init(void) {
    DSTATUS status = disk_initialize(0);
    return (status == 0);
}
