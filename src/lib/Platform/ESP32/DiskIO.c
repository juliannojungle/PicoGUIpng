/*-----------------------------------------------------------------------*/
/* Low level disk I/O module for ESP32 (FatFS diskio interface)          */
/*-----------------------------------------------------------------------*/
/* Implements the FatFS diskio.h interface using ESP-IDF SPI master      */
/* driver. Uses injectable SPI ops for testability.                      */
/*-----------------------------------------------------------------------*/

#include "ff.h"
#include "diskio.h"
#include "HALConfig.h"

#ifndef DISKIO_TEST_MODE
#include "driver/spi_master.h"
#include "driver/gpio.h"
#endif

#include <string.h>
#include <stdbool.h>
#include <stdint.h>

/*-----------------------------------------------------------------------*/
/* Type Definitions                                                      */
/*-----------------------------------------------------------------------*/

typedef unsigned int uint;

/* Injectable SPI operations interface for testability */
typedef struct {
    int (*spi_init)(uint32_t baudrate);
    int (*spi_transfer)(const uint8_t* tx, uint8_t* rx, size_t len);
    void (*gpio_set)(uint pin, uint value);
} spi_ops_t;

/* Internal SD card state */
typedef struct {
    bool initialized;
    uint32_t sector_count;
    uint16_t sector_size;
    uint32_t block_size;
} sd_card_state_t;

/*-----------------------------------------------------------------------*/
/* Module Private Variables                                               */
/*-----------------------------------------------------------------------*/

static sd_card_state_t sd_state = {
    .initialized = false,
    .sector_count = 0,
    .sector_size = 512,
    .block_size = 512
};

static const spi_ops_t* injected_ops = NULL;

/*-----------------------------------------------------------------------*/
/* Default ESP-IDF SPI Operations (used when no ops injected)            */
/*-----------------------------------------------------------------------*/

#ifndef DISKIO_TEST_MODE

static spi_device_handle_t spi_handle = NULL;

static int default_spi_init(uint32_t baudrate)
{
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = SD_SPI_MOSI,
        .miso_io_num = SD_SPI_MISO,
        .sclk_io_num = SD_SPI_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 512 + 16
    };

    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        return -1;
    }

    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = (int)baudrate,
        .mode = 0,
        .spics_io_num = SD_SPI_CS,
        .queue_size = 1
    };

    ret = spi_bus_add_device(SPI2_HOST, &dev_cfg, &spi_handle);
    if (ret != ESP_OK) {
        spi_bus_free(SPI2_HOST);
        return -1;
    }

    return 0;
}

static int default_spi_transfer(const uint8_t* tx, uint8_t* rx, size_t len)
{
    if (spi_handle == NULL) {
        return -1;
    }

    spi_transaction_t trans = {
        .length = len * 8,
        .tx_buffer = tx,
        .rx_buffer = rx
    };

    esp_err_t ret = spi_device_transmit(spi_handle, &trans);
    if (ret != ESP_OK) {
        return -1;
    }

    return 0;
}

static void default_gpio_set(uint pin, uint value)
{
    gpio_set_level((gpio_num_t)pin, (uint32_t)value);
}

static const spi_ops_t default_ops = {
    .spi_init = default_spi_init,
    .spi_transfer = default_spi_transfer,
    .gpio_set = default_gpio_set
};

#endif /* DISKIO_TEST_MODE */

/*-----------------------------------------------------------------------*/
/* Get active SPI operations                                             */
/*-----------------------------------------------------------------------*/

static const spi_ops_t* get_ops(void)
{
    if (injected_ops != NULL) {
        return injected_ops;
    }
#ifndef DISKIO_TEST_MODE
    return &default_ops;
#else
    return NULL;
#endif
}

/*-----------------------------------------------------------------------*/
/* Public: Inject SPI ops for testing                                    */
/*-----------------------------------------------------------------------*/

void diskio_set_spi_ops(const spi_ops_t* ops)
{
    injected_ops = ops;
}

/**
 * Reset internal disk state (for testing).
 */
void diskio_reset_state(void)
{
    sd_state.initialized = false;
    sd_state.sector_count = 0;
    sd_state.sector_size = 512;
    sd_state.block_size = 512;
}

/*-----------------------------------------------------------------------*/
/* SD Card Command Helpers                                               */
/*-----------------------------------------------------------------------*/

/* SD command indices */
#define CMD0    0   /* GO_IDLE_STATE */
#define CMD8    8   /* SEND_IF_COND */
#define CMD12   12  /* STOP_TRANSMISSION */
#define CMD17   17  /* READ_SINGLE_BLOCK */
#define CMD18   18  /* READ_MULTIPLE_BLOCK */
#define CMD24   24  /* WRITE_BLOCK */
#define CMD25   25  /* WRITE_MULTIPLE_BLOCK */
#define CMD55   55  /* APP_CMD */
#define CMD58   58  /* READ_OCR */
#define ACMD41  41  /* SD_SEND_OP_COND */

/* R1 response bits */
#define R1_IDLE_STATE   0x01
#define R1_ILLEGAL_CMD  0x04

/* Token values */
#define DATA_TOKEN_CMD17    0xFE
#define DATA_TOKEN_CMD18    0xFE
#define DATA_TOKEN_CMD24    0xFE
#define DATA_TOKEN_CMD25    0xFC
#define STOP_TRAN_TOKEN     0xFD

/* Data response mask */
#define DATA_RESP_MASK      0x1F
#define DATA_RESP_ACCEPTED  0x05

static int sd_send_cmd(uint8_t cmd, uint32_t arg, uint8_t* response)
{
    const spi_ops_t* ops = get_ops();
    if (ops == NULL) return -1;

    uint8_t tx[6];
    uint8_t rx[6];

    tx[0] = 0x40 | cmd;
    tx[1] = (uint8_t)(arg >> 24);
    tx[2] = (uint8_t)(arg >> 16);
    tx[3] = (uint8_t)(arg >> 8);
    tx[4] = (uint8_t)(arg);

    /* CRC (only needed for CMD0 and CMD8 in SPI mode) */
    if (cmd == CMD0) {
        tx[5] = 0x95;
    } else if (cmd == CMD8) {
        tx[5] = 0x87;
    } else {
        tx[5] = 0x01; /* dummy CRC + stop bit */
    }

    if (ops->spi_transfer(tx, rx, 6) != 0) {
        return -1;
    }

    /* Wait for response (R1 format: MSB = 0) */
    uint8_t dummy_tx = 0xFF;
    uint8_t resp_byte = 0xFF;
    int timeout = 10;

    while (timeout > 0) {
        if (ops->spi_transfer(&dummy_tx, &resp_byte, 1) != 0) {
            return -1;
        }
        if ((resp_byte & 0x80) == 0) {
            break;
        }
        timeout--;
    }

    if (timeout == 0) {
        return -1;
    }

    *response = resp_byte;
    return 0;
}

static int sd_send_acmd(uint8_t acmd, uint32_t arg, uint8_t* response)
{
    uint8_t r1;

    /* Send CMD55 first to indicate next command is application-specific */
    if (sd_send_cmd(CMD55, 0, &r1) != 0) {
        return -1;
    }

    return sd_send_cmd(acmd, arg, response);
}

static int sd_wait_ready(void)
{
    const spi_ops_t* ops = get_ops();
    if (ops == NULL) return -1;

    uint8_t dummy_tx = 0xFF;
    uint8_t rx;
    int timeout = 500;

    while (timeout > 0) {
        if (ops->spi_transfer(&dummy_tx, &rx, 1) != 0) {
            return -1;
        }
        if (rx == 0xFF) {
            return 0;
        }
        timeout--;
    }

    return -1;
}

static int sd_wait_data_token(uint8_t token)
{
    const spi_ops_t* ops = get_ops();
    if (ops == NULL) return -1;

    uint8_t dummy_tx = 0xFF;
    uint8_t rx;
    int timeout = 1000;

    while (timeout > 0) {
        if (ops->spi_transfer(&dummy_tx, &rx, 1) != 0) {
            return -1;
        }
        if (rx == token) {
            return 0;
        }
        if (rx != 0xFF) {
            return -1; /* Error token received */
        }
        timeout--;
    }

    return -1;
}

/*-----------------------------------------------------------------------*/
/* Initialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize(BYTE pdrv)
{
    const spi_ops_t* ops;
    uint8_t response;
    int timeout;

    if (pdrv != 0) return STA_NOINIT;

    ops = get_ops();
    if (ops == NULL) return STA_NOINIT;

    /* Initialize SPI bus at low speed for card detection */
    if (ops->spi_init(400000) != 0) {
        sd_state.initialized = false;
        return STA_NOINIT;
    }

    /* CS high during power-up sequence */
    ops->gpio_set(SD_SPI_CS, 1);

    /* Send 80+ clock cycles with CS high (10 bytes of 0xFF) */
    uint8_t init_clocks[10];
    uint8_t dummy_rx[10];
    memset(init_clocks, 0xFF, sizeof(init_clocks));
    if (ops->spi_transfer(init_clocks, dummy_rx, 10) != 0) {
        sd_state.initialized = false;
        return STA_NOINIT;
    }

    /* Select card (CS low) */
    ops->gpio_set(SD_SPI_CS, 0);

    /* CMD0: GO_IDLE_STATE - reset card to SPI mode */
    if (sd_send_cmd(CMD0, 0, &response) != 0 || response != R1_IDLE_STATE) {
        ops->gpio_set(SD_SPI_CS, 1);
        sd_state.initialized = false;
        return STA_NOINIT;
    }

    /* CMD8: SEND_IF_COND - check voltage range (SDv2 detection) */
    if (sd_send_cmd(CMD8, 0x000001AA, &response) != 0) {
        ops->gpio_set(SD_SPI_CS, 1);
        sd_state.initialized = false;
        return STA_NOINIT;
    }

    if (response == R1_IDLE_STATE) {
        /* SDv2 card - read trailing 4 bytes of R7 response */
        uint8_t r7_tx[4] = {0xFF, 0xFF, 0xFF, 0xFF};
        uint8_t r7_rx[4];
        if (ops->spi_transfer(r7_tx, r7_rx, 4) != 0) {
            ops->gpio_set(SD_SPI_CS, 1);
            sd_state.initialized = false;
            return STA_NOINIT;
        }

        /* Verify echo pattern */
        if (r7_rx[3] != 0xAA) {
            ops->gpio_set(SD_SPI_CS, 1);
            sd_state.initialized = false;
            return STA_NOINIT;
        }
    }

    /* ACMD41: SD_SEND_OP_COND - wait for card to leave idle state */
    timeout = 1000;
    do {
        if (sd_send_acmd(ACMD41, 0x40000000, &response) != 0) {
            ops->gpio_set(SD_SPI_CS, 1);
            sd_state.initialized = false;
            return STA_NOINIT;
        }
        timeout--;
    } while (response != 0x00 && timeout > 0);

    if (response != 0x00) {
        ops->gpio_set(SD_SPI_CS, 1);
        sd_state.initialized = false;
        return STA_NOINIT;
    }

    /* Read OCR to determine card capacity type */
    if (sd_send_cmd(CMD58, 0, &response) != 0) {
        ops->gpio_set(SD_SPI_CS, 1);
        sd_state.initialized = false;
        return STA_NOINIT;
    }

    /* Read OCR register (4 bytes) */
    uint8_t ocr_tx[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t ocr_rx[4];
    if (ops->spi_transfer(ocr_tx, ocr_rx, 4) != 0) {
        ops->gpio_set(SD_SPI_CS, 1);
        sd_state.initialized = false;
        return STA_NOINIT;
    }

    /* Deselect card */
    ops->gpio_set(SD_SPI_CS, 1);

    /* Re-initialize SPI at full speed */
    if (ops->spi_init(SD_SPI_BAUDRATE) != 0) {
        sd_state.initialized = false;
        return STA_NOINIT;
    }

    /* Set default card parameters */
    sd_state.sector_size = 512;
    sd_state.block_size = 512;
    sd_state.sector_count = 0; /* Will be determined by CSD read if needed */
    sd_state.initialized = true;

    return 0;
}

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status(BYTE pdrv)
{
    if (pdrv != 0) return STA_NOINIT;

    if (!sd_state.initialized) {
        return STA_NOINIT;
    }

    return 0;
}

/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count)
{
    const spi_ops_t* ops;
    uint8_t response;

    if (pdrv != 0) return RES_PARERR;

    if (!sd_state.initialized) {
        return RES_NOTRDY;
    }

    /* Validate sector bounds */
    if (sd_state.sector_count > 0) {
        if (sector >= sd_state.sector_count ||
            (sector + count) > sd_state.sector_count) {
            return RES_PARERR;
        }
    }

    if (count == 0) return RES_PARERR;

    ops = get_ops();
    if (ops == NULL) return RES_ERROR;

    /* Select card */
    ops->gpio_set(SD_SPI_CS, 0);

    if (sd_wait_ready() != 0) {
        ops->gpio_set(SD_SPI_CS, 1);
        return RES_ERROR;
    }

    if (count == 1) {
        /* CMD17: READ_SINGLE_BLOCK */
        if (sd_send_cmd(CMD17, (uint32_t)sector, &response) != 0 || response != 0x00) {
            ops->gpio_set(SD_SPI_CS, 1);
            return RES_ERROR;
        }

        /* Wait for data token */
        if (sd_wait_data_token(DATA_TOKEN_CMD17) != 0) {
            ops->gpio_set(SD_SPI_CS, 1);
            return RES_ERROR;
        }

        /* Read sector data */
        uint8_t tx_dummy[512];
        memset(tx_dummy, 0xFF, sizeof(tx_dummy));
        if (ops->spi_transfer(tx_dummy, buff, sd_state.sector_size) != 0) {
            ops->gpio_set(SD_SPI_CS, 1);
            return RES_ERROR;
        }

        /* Read and discard CRC (2 bytes) */
        uint8_t crc_tx[2] = {0xFF, 0xFF};
        uint8_t crc_rx[2];
        if (ops->spi_transfer(crc_tx, crc_rx, 2) != 0) {
            ops->gpio_set(SD_SPI_CS, 1);
            return RES_ERROR;
        }
    } else {
        /* CMD18: READ_MULTIPLE_BLOCK */
        if (sd_send_cmd(CMD18, (uint32_t)sector, &response) != 0 || response != 0x00) {
            ops->gpio_set(SD_SPI_CS, 1);
            return RES_ERROR;
        }

        UINT i;
        for (i = 0; i < count; i++) {
            /* Wait for data token */
            if (sd_wait_data_token(DATA_TOKEN_CMD18) != 0) {
                sd_send_cmd(CMD12, 0, &response);
                ops->gpio_set(SD_SPI_CS, 1);
                return RES_ERROR;
            }

            /* Read sector data */
            uint8_t tx_dummy[512];
            memset(tx_dummy, 0xFF, sizeof(tx_dummy));
            if (ops->spi_transfer(tx_dummy, buff + (i * sd_state.sector_size),
                                  sd_state.sector_size) != 0) {
                sd_send_cmd(CMD12, 0, &response);
                ops->gpio_set(SD_SPI_CS, 1);
                return RES_ERROR;
            }

            /* Read and discard CRC (2 bytes) */
            uint8_t crc_tx[2] = {0xFF, 0xFF};
            uint8_t crc_rx[2];
            if (ops->spi_transfer(crc_tx, crc_rx, 2) != 0) {
                sd_send_cmd(CMD12, 0, &response);
                ops->gpio_set(SD_SPI_CS, 1);
                return RES_ERROR;
            }
        }

        /* CMD12: STOP_TRANSMISSION */
        sd_send_cmd(CMD12, 0, &response);
    }

    /* Deselect card */
    ops->gpio_set(SD_SPI_CS, 1);

    return RES_OK;
}

/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

DRESULT disk_write(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count)
{
    const spi_ops_t* ops;
    uint8_t response;
    uint8_t data_resp;

    if (pdrv != 0) return RES_PARERR;

    if (!sd_state.initialized) {
        return RES_NOTRDY;
    }

    /* Validate sector bounds */
    if (sd_state.sector_count > 0) {
        if (sector >= sd_state.sector_count ||
            (sector + count) > sd_state.sector_count) {
            return RES_PARERR;
        }
    }

    if (count == 0) return RES_PARERR;

    ops = get_ops();
    if (ops == NULL) return RES_ERROR;

    /* Select card */
    ops->gpio_set(SD_SPI_CS, 0);

    if (sd_wait_ready() != 0) {
        ops->gpio_set(SD_SPI_CS, 1);
        return RES_ERROR;
    }

    if (count == 1) {
        /* CMD24: WRITE_BLOCK */
        if (sd_send_cmd(CMD24, (uint32_t)sector, &response) != 0 || response != 0x00) {
            ops->gpio_set(SD_SPI_CS, 1);
            return RES_ERROR;
        }

        /* Send data token */
        uint8_t token = DATA_TOKEN_CMD24;
        uint8_t dummy_rx;
        if (ops->spi_transfer(&token, &dummy_rx, 1) != 0) {
            ops->gpio_set(SD_SPI_CS, 1);
            return RES_ERROR;
        }

        /* Send sector data */
        uint8_t rx_dummy[512];
        if (ops->spi_transfer(buff, rx_dummy, sd_state.sector_size) != 0) {
            ops->gpio_set(SD_SPI_CS, 1);
            return RES_ERROR;
        }

        /* Send dummy CRC (2 bytes) */
        uint8_t crc_tx[2] = {0xFF, 0xFF};
        uint8_t crc_rx[2];
        if (ops->spi_transfer(crc_tx, crc_rx, 2) != 0) {
            ops->gpio_set(SD_SPI_CS, 1);
            return RES_ERROR;
        }

        /* Read data response */
        uint8_t dummy_tx = 0xFF;
        if (ops->spi_transfer(&dummy_tx, &data_resp, 1) != 0) {
            ops->gpio_set(SD_SPI_CS, 1);
            return RES_ERROR;
        }

        if ((data_resp & DATA_RESP_MASK) != DATA_RESP_ACCEPTED) {
            ops->gpio_set(SD_SPI_CS, 1);
            return RES_ERROR;
        }

        /* Wait for write to complete */
        if (sd_wait_ready() != 0) {
            ops->gpio_set(SD_SPI_CS, 1);
            return RES_ERROR;
        }
    } else {
        /* CMD25: WRITE_MULTIPLE_BLOCK */
        if (sd_send_cmd(CMD25, (uint32_t)sector, &response) != 0 || response != 0x00) {
            ops->gpio_set(SD_SPI_CS, 1);
            return RES_ERROR;
        }

        UINT i;
        for (i = 0; i < count; i++) {
            /* Send multi-block data token */
            uint8_t token = DATA_TOKEN_CMD25;
            uint8_t dummy_rx;
            if (ops->spi_transfer(&token, &dummy_rx, 1) != 0) {
                ops->gpio_set(SD_SPI_CS, 1);
                return RES_ERROR;
            }

            /* Send sector data */
            uint8_t rx_dummy[512];
            if (ops->spi_transfer(buff + (i * sd_state.sector_size), rx_dummy,
                                  sd_state.sector_size) != 0) {
                ops->gpio_set(SD_SPI_CS, 1);
                return RES_ERROR;
            }

            /* Send dummy CRC (2 bytes) */
            uint8_t crc_tx[2] = {0xFF, 0xFF};
            uint8_t crc_rx[2];
            if (ops->spi_transfer(crc_tx, crc_rx, 2) != 0) {
                ops->gpio_set(SD_SPI_CS, 1);
                return RES_ERROR;
            }

            /* Read data response */
            uint8_t dummy_tx = 0xFF;
            if (ops->spi_transfer(&dummy_tx, &data_resp, 1) != 0) {
                ops->gpio_set(SD_SPI_CS, 1);
                return RES_ERROR;
            }

            if ((data_resp & DATA_RESP_MASK) != DATA_RESP_ACCEPTED) {
                /* Send stop token and abort */
                uint8_t stop = STOP_TRAN_TOKEN;
                ops->spi_transfer(&stop, &dummy_rx, 1);
                ops->gpio_set(SD_SPI_CS, 1);
                return RES_ERROR;
            }

            /* Wait for write to complete */
            if (sd_wait_ready() != 0) {
                uint8_t stop = STOP_TRAN_TOKEN;
                ops->spi_transfer(&stop, &dummy_rx, 1);
                ops->gpio_set(SD_SPI_CS, 1);
                return RES_ERROR;
            }
        }

        /* Send stop transmission token */
        uint8_t stop = STOP_TRAN_TOKEN;
        uint8_t dummy_rx;
        if (ops->spi_transfer(&stop, &dummy_rx, 1) != 0) {
            ops->gpio_set(SD_SPI_CS, 1);
            return RES_ERROR;
        }

        /* Wait for card to finish */
        if (sd_wait_ready() != 0) {
            ops->gpio_set(SD_SPI_CS, 1);
            return RES_ERROR;
        }
    }

    /* Deselect card */
    ops->gpio_set(SD_SPI_CS, 1);

    return RES_OK;
}

/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff)
{
    if (pdrv != 0) return RES_PARERR;

    if (!sd_state.initialized) {
        return RES_NOTRDY;
    }

    switch (cmd) {
    case CTRL_SYNC:
        /* No pending write operations in SPI mode */
        return RES_OK;

    case GET_SECTOR_COUNT:
        if (buff == NULL) return RES_PARERR;
        *(LBA_t*)buff = (LBA_t)sd_state.sector_count;
        return RES_OK;

    case GET_SECTOR_SIZE:
        if (buff == NULL) return RES_PARERR;
        *(WORD*)buff = (WORD)sd_state.sector_size;
        return RES_OK;

    case GET_BLOCK_SIZE:
        if (buff == NULL) return RES_PARERR;
        *(DWORD*)buff = (DWORD)sd_state.block_size;
        return RES_OK;

    default:
        return RES_PARERR;
    }
}

/*-----------------------------------------------------------------------*/
/* Platform_SDCard_Init - High-level initialization entry point          */
/*-----------------------------------------------------------------------*/

bool Platform_SDCard_Init(void)
{
    DSTATUS status = disk_initialize(0);
    return (status == 0);
}
