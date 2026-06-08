/* Feature: multi-platform-fatfs, Property 5: Unsupported ioctl command returns RES_PARERR */
/*
 * Property-Based Test: For any ioctl command byte value that is NOT one of
 * CTRL_SYNC (0), GET_SECTOR_COUNT (1), GET_SECTOR_SIZE (2), or GET_BLOCK_SIZE (3),
 * disk_ioctl SHALL return RES_PARERR.
 *
 * Validates: Requirements 9.6
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#include "ff.h"
#include "diskio.h"

/*-----------------------------------------------------------------------*/
/* SPI Operations Interface (must match diskio.c)                        */
/*-----------------------------------------------------------------------*/

typedef unsigned int uint;

typedef struct {
    int (*spi_init)(uint32_t baudrate);
    int (*spi_transfer)(const uint8_t* tx, uint8_t* rx, size_t len);
    void (*gpio_set)(uint pin, uint value);
} spi_ops_t;

/* External functions from diskio.c (test mode) */
extern void diskio_set_spi_ops(const spi_ops_t* ops);
extern void diskio_reset_state(void);

/*-----------------------------------------------------------------------*/
/* Mock SPI implementation for successful initialization                 */
/*-----------------------------------------------------------------------*/

static bool g_init_phase_complete = false;

/* Response queue for mock SPI */
static int g_response_queue_pos = 0;
static uint8_t g_response_queue[512];
static int g_response_queue_len = 0;

static void queue_response(uint8_t byte) {
    if (g_response_queue_len < (int)sizeof(g_response_queue)) {
        g_response_queue[g_response_queue_len++] = byte;
    }
}

static void queue_responses(const uint8_t* data, int len) {
    for (int i = 0; i < len; i++) {
        queue_response(data[i]);
    }
}

/* Command frame tracking */
static int g_cmd_frame_pos = 0;
static uint8_t g_cmd_frame[6];
static bool g_in_cmd_frame = false;
static int g_acmd41_attempts = 0;

/* CSD Version 2.0 (SDHC) with c_size = 0 => sector_count = (0+1)*1024 = 1024 */
static uint8_t mock_csd[16] = {
    0x40, /* CSD structure version 2 (bits 7:6 = 01) */
    0x0E, 0x00, 0x32, 0x5B, 0x59, 0x00, 0x00,
    0x00, 0x00, /* c_size bytes: csd[7]&0x3F=0, csd[8]=0, csd[9]=0 => c_size=0 */
    0x7F, 0x80, 0x0A, 0x40, 0x00, 0x01
};

static int mock_spi_init(uint32_t baudrate) {
    (void)baudrate;
    return 0;
}

static int mock_spi_transfer(const uint8_t* tx, uint8_t* rx, size_t len) {
    if (g_init_phase_complete) {
        if (rx) memset(rx, 0xFF, len);
        return 0;
    }

    for (size_t i = 0; i < len; i++) {
        uint8_t tx_byte = tx ? tx[i] : 0xFF;
        uint8_t rx_byte = 0xFF;

        /* Check if this is a command byte (starts with 0x4X) */
        if (!g_in_cmd_frame && (tx_byte & 0xC0) == 0x40) {
            g_in_cmd_frame = true;
            g_cmd_frame_pos = 0;
            g_cmd_frame[g_cmd_frame_pos++] = tx_byte;
        } else if (g_in_cmd_frame && g_cmd_frame_pos < 6) {
            g_cmd_frame[g_cmd_frame_pos++] = tx_byte;
            if (g_cmd_frame_pos == 6) {
                /* Full command received, prepare response */
                g_in_cmd_frame = false;
                uint8_t cmd = g_cmd_frame[0];

                switch (cmd) {
                    case 0x40: /* CMD0 */
                        queue_response(0x01);
                        break;
                    case 0x48: /* CMD8 */
                        queue_response(0x01);
                        queue_response(0x00);
                        queue_response(0x00);
                        queue_response(0x01);
                        queue_response(0xAA);
                        break;
                    case 0x77: /* CMD55 */
                        queue_response(0x01);
                        break;
                    case 0x69: /* ACMD41 */
                        g_acmd41_attempts++;
                        if (g_acmd41_attempts >= 2) {
                            queue_response(0x00); /* Ready */
                        } else {
                            queue_response(0x01); /* Still initializing */
                        }
                        break;
                    case 0x49: /* CMD9 - SEND_CSD */
                        queue_response(0x00); /* R1 OK */
                        queue_response(0xFE); /* SD_DATA_TOKEN */
                        queue_responses(mock_csd, 16);
                        queue_response(0x00); /* CRC byte 1 */
                        queue_response(0x00); /* CRC byte 2 */
                        break;
                    default:
                        queue_response(0x01);
                        break;
                }
            }
        } else {
            /* Not in command frame - serve queued responses */
            if (g_response_queue_pos < g_response_queue_len) {
                rx_byte = g_response_queue[g_response_queue_pos++];
            }
        }

        if (rx) {
            rx[i] = rx_byte;
        }
    }

    return 0;
}

static void mock_gpio_set(uint pin, uint value) {
    (void)pin;
    (void)value;
}

static const spi_ops_t mock_ops = {
    .spi_init = mock_spi_init,
    .spi_transfer = mock_spi_transfer,
    .gpio_set = mock_gpio_set
};

/*-----------------------------------------------------------------------*/
/* Reset mock state                                                      */
/*-----------------------------------------------------------------------*/

static void reset_mock_state(void) {
    g_init_phase_complete = false;
    g_response_queue_pos = 0;
    g_response_queue_len = 0;
    g_cmd_frame_pos = 0;
    g_in_cmd_frame = false;
    g_acmd41_attempts = 0;
}

/*-----------------------------------------------------------------------*/
/* Xorshift32 PRNG                                                       */
/*-----------------------------------------------------------------------*/

static uint32_t xorshift_state;

static void prng_seed(uint32_t seed) {
    xorshift_state = seed ? seed : 0xDEADBEEF;
}

static uint32_t xorshift32(void) {
    uint32_t x = xorshift_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    xorshift_state = x;
    return x;
}

/*-----------------------------------------------------------------------*/
/* Generate random unsupported ioctl command (values 4-255)              */
/*-----------------------------------------------------------------------*/

static uint8_t generate_unsupported_cmd(void) {
    /* Supported commands: CTRL_SYNC=0, GET_SECTOR_COUNT=1,
     * GET_SECTOR_SIZE=2, GET_BLOCK_SIZE=3.
     * Any value from 4 to 255 is unsupported. */
    return (uint8_t)(4 + (xorshift32() % 252));
}

/*-----------------------------------------------------------------------*/
/* Test harness                                                           */
/*-----------------------------------------------------------------------*/

#define NUM_ITERATIONS 200

int main(void) {
    int tests_passed = 0;
    int tests_failed = 0;

    printf("=== Property 5: Unsupported ioctl command returns RES_PARERR ===\n");
    printf("Validates: Requirements 9.6\n");
    printf("Iterations: %d\n\n", NUM_ITERATIONS);

    /* Seed PRNG */
    prng_seed((uint32_t)time(NULL));

    /* Step 1: Initialize the disk via mock SPI protocol */
    reset_mock_state();
    diskio_reset_state();
    diskio_set_spi_ops(&mock_ops);

    DSTATUS init_status = disk_initialize(0);
    if (init_status != 0) {
        printf("FATAL: disk_initialize failed with status 0x%02X\n", init_status);
        printf("  (Mock init protocol may need adjustment)\n");
        return 1;
    }

    /* Mark init as complete */
    g_init_phase_complete = true;
    printf("Disk initialized successfully.\n\n");

    /* Step 2: Run property test iterations */
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        /* Generate an unsupported command value (4..255) */
        uint8_t cmd = generate_unsupported_cmd();

        /* Provide a dummy buffer for ioctl */
        uint32_t dummy_buff = 0;

        /* Call disk_ioctl with the unsupported command */
        DRESULT result = disk_ioctl(0, cmd, &dummy_buff);

        if (result == RES_PARERR) {
            tests_passed++;
        } else {
            printf("  FAIL [iter %d]: cmd=%u, expected RES_PARERR (%d), got %d\n",
                   i, (unsigned)cmd, RES_PARERR, (int)result);
            tests_failed++;
        }
    }

    /* Summary */
    printf("\n=== Results ===\n");
    printf("Total: %d, Passed: %d, Failed: %d\n",
           NUM_ITERATIONS, tests_passed, tests_failed);

    if (tests_failed > 0) {
        printf("\nFAILED: %d test(s) did not satisfy the property.\n", tests_failed);
        return 1;
    }

    printf("\nPASSED: All %d iterations confirm Property 5.\n", tests_passed);
    printf("Unsupported ioctl commands correctly return RES_PARERR.\n");
    return 0;
}
