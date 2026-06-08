/* Feature: multi-platform-fatfs, Property 3: Out-of-bounds sector returns RES_PARERR */
/*
 * Property-Based Test: For any sector number >= sector_count, and for any count value,
 * both disk_read and disk_write SHALL return RES_PARERR without issuing SPI commands.
 *
 * Validates: Requirements 9.3, 9.4
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

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
/* Mock SPI tracking                                                     */
/*-----------------------------------------------------------------------*/

static int g_spi_transfer_count_after_init = 0;
static bool g_init_phase_complete = false;

/* State machine for SD card initialization mock */
typedef enum {
    INIT_IDLE,
    INIT_CMD0_SENT,
    INIT_CMD8_SENT,
    INIT_ACMD41_PENDING,
    INIT_CSD_READING,
    INIT_COMPLETE
} init_state_t;

static init_state_t g_init_state = INIT_IDLE;
static int g_cmd_byte_count = 0;
static uint8_t g_last_cmd = 0;
static int g_acmd41_attempts = 0;
static int g_csd_bytes_sent = 0;
static bool g_waiting_for_cmd55_response = false;

/* Known sector count for test (set via CSD mock) */
#define TEST_SECTOR_COUNT 1000

/* CSD Version 2.0 (SDHC) with c_size that gives TEST_SECTOR_COUNT sectors */
/* sector_count = (c_size + 1) * 1024, so c_size = (TEST_SECTOR_COUNT / 1024) - 1 */
/* For 1000 sectors, we cannot get exact with CSD v2 (must be multiple of 1024) */
/* Let's use c_size = 0 => sector_count = 1024 for simplicity */
#define MOCK_CSD_SECTOR_COUNT 1024

static uint8_t mock_csd[16] = {
    0x40, /* CSD structure version 2 (bits 7:6 = 01) */
    0x0E, 0x00, 0x32, 0x5B, 0x59, 0x00, 0x00,
    0x00, 0x00, /* c_size bytes: csd[7]&0x3F=0, csd[8]=0, csd[9]=0 => c_size=0, sectors=(0+1)*1024=1024 */
    0x7F, 0x80, 0x0A, 0x40, 0x00, 0x01
};

/*-----------------------------------------------------------------------*/
/* Mock SPI implementation for initialization                            */
/*-----------------------------------------------------------------------*/

static int mock_spi_init(uint32_t baudrate) {
    (void)baudrate;
    return 0; /* Success */
}

/* Simple state machine to handle SD init protocol over SPI */
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

/* Track which command we're processing */
static int g_cmd_frame_pos = 0;
static uint8_t g_cmd_frame[6];
static bool g_in_cmd_frame = false;
static bool g_expect_r7 = false;
static int g_csd_data_phase = 0; /* 0: not in CSD, 1: waiting for token read, 2: sending data */

static int mock_spi_transfer(const uint8_t* tx, uint8_t* rx, size_t len) {
    /* After init is complete, track any SPI activity */
    if (g_init_phase_complete) {
        g_spi_transfer_count_after_init++;
        /* Fill rx with 0xFF if needed */
        if (rx) {
            memset(rx, 0xFF, len);
        }
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
                g_last_cmd = cmd;

                switch (cmd) {
                    case 0x40: /* CMD0 */
                        g_init_state = INIT_CMD0_SENT;
                        /* Response will be 0x01 on next exchange */
                        queue_response(0x01);
                        break;
                    case 0x48: /* CMD8 */
                        g_init_state = INIT_CMD8_SENT;
                        /* R1 = 0x01, then 4 bytes R7 */
                        queue_response(0x01);
                        queue_response(0x00);
                        queue_response(0x00);
                        queue_response(0x01);
                        queue_response(0xAA);
                        break;
                    case 0x77: /* CMD55 */
                        g_waiting_for_cmd55_response = true;
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
                        /* Then data token */
                        queue_response(0xFE); /* SD_DATA_TOKEN */
                        /* Then 16 bytes of CSD */
                        queue_responses(mock_csd, 16);
                        /* Then 2 bytes CRC (ignored) */
                        queue_response(0x00);
                        queue_response(0x00);
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
/* Simpler approach: directly manipulate disk state after init            */
/* Since disk_initialize does full protocol, let's just call it with mock */
/*-----------------------------------------------------------------------*/

static void reset_mock_state(void) {
    g_spi_transfer_count_after_init = 0;
    g_init_phase_complete = false;
    g_init_state = INIT_IDLE;
    g_cmd_byte_count = 0;
    g_last_cmd = 0;
    g_acmd41_attempts = 0;
    g_csd_bytes_sent = 0;
    g_waiting_for_cmd55_response = false;
    g_response_queue_pos = 0;
    g_response_queue_len = 0;
    g_cmd_frame_pos = 0;
    g_in_cmd_frame = false;
    g_expect_r7 = false;
    g_csd_data_phase = 0;
}

/*-----------------------------------------------------------------------*/
/* Xorshift32 PRNG                                                       */
/*-----------------------------------------------------------------------*/

static uint32_t xorshift_state = 0xDEADBEEF;

static uint32_t xorshift32(void) {
    uint32_t x = xorshift_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    xorshift_state = x;
    return x;
}

/*-----------------------------------------------------------------------*/
/* Test harness                                                           */
/*-----------------------------------------------------------------------*/

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_EQ(actual, expected, msg) do { \
    if ((actual) != (expected)) { \
        printf("  FAIL [iter %d]: %s (got %d, expected %d)\n", \
               i, msg, (int)(actual), (int)(expected)); \
        tests_failed++; \
        failed = 1; \
    } \
} while(0)

#define NUM_ITERATIONS 150

int main(void) {
    printf("=== Property 3: Out-of-bounds sector returns RES_PARERR ===\n");
    printf("Validates: Requirements 9.3, 9.4\n");
    printf("Iterations: %d\n\n", NUM_ITERATIONS);

    /* Step 1: Initialize the disk via the mock SPI protocol */
    reset_mock_state();
    diskio_reset_state();
    diskio_set_spi_ops(&mock_ops);

    DSTATUS init_status = disk_initialize(0);
    if (init_status != 0) {
        printf("FATAL: disk_initialize failed with status 0x%02X\n", init_status);
        printf("  (Mock init protocol may need adjustment)\n");
        return 1;
    }

    /* Mark init as complete so we can track post-init SPI activity */
    g_init_phase_complete = true;
    printf("Disk initialized successfully (sector_count = %u)\n\n", MOCK_CSD_SECTOR_COUNT);

    /* Step 2: Run property test iterations */
    /* The disk now has sector_count = MOCK_CSD_SECTOR_COUNT (1024) */
    /* We generate sector values >= MOCK_CSD_SECTOR_COUNT and arbitrary count */

    uint8_t read_buffer[512];
    uint8_t write_buffer[512];
    memset(write_buffer, 0xAA, sizeof(write_buffer));

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        int failed = 0;
        tests_run++;

        /* Generate out-of-bounds sector: sector >= MOCK_CSD_SECTOR_COUNT */
        uint32_t raw = xorshift32();
        LBA_t sector;

        if (i < NUM_ITERATIONS / 3) {
            /* Boundary: exactly at sector_count */
            sector = (LBA_t)MOCK_CSD_SECTOR_COUNT;
        } else if (i < 2 * NUM_ITERATIONS / 3) {
            /* Just above sector_count */
            sector = (LBA_t)(MOCK_CSD_SECTOR_COUNT + (raw % 1000) + 1);
        } else {
            /* Large random values */
            sector = (LBA_t)(MOCK_CSD_SECTOR_COUNT + raw % (UINT32_MAX - MOCK_CSD_SECTOR_COUNT));
        }

        /* Generate arbitrary count (1 to avoid overflow issues with sector+count, but
         * also test with larger values) */
        UINT count = (UINT)((xorshift32() % 64) + 1);

        /* Ensure sector + count > sector_count (it should be, since sector >= sector_count and count >= 1) */
        /* But handle edge case: if sector is exactly sector_count and count is 0,
         * the code also checks count==0 separately. We ensure count >= 1 above. */

        /* Reset SPI activity counter */
        g_spi_transfer_count_after_init = 0;

        /* Test disk_read returns RES_PARERR */
        DRESULT read_result = disk_read(0, read_buffer, sector, count);
        ASSERT_EQ(read_result, RES_PARERR, "disk_read should return RES_PARERR for out-of-bounds sector");

        /* Test disk_write returns RES_PARERR */
        DRESULT write_result = disk_write(0, write_buffer, sector, count);
        ASSERT_EQ(write_result, RES_PARERR, "disk_write should return RES_PARERR for out-of-bounds sector");

        /* Verify no SPI activity occurred (bounds check is before any SPI calls) */
        ASSERT_EQ(g_spi_transfer_count_after_init, 0,
                  "No SPI transfers should occur for out-of-bounds sector");

        if (!failed) {
            tests_passed++;
        }
    }

    /* Summary */
    printf("\n=== Results ===\n");
    printf("Total: %d, Passed: %d, Failed: %d\n", tests_run, tests_passed, tests_failed);

    if (tests_failed > 0) {
        printf("\nFAILED: %d test(s) did not satisfy the property.\n", tests_failed);
        return 1;
    }

    printf("\nPASSED: All %d iterations confirm Property 3.\n", tests_passed);
    printf("Out-of-bounds sector access correctly returns RES_PARERR without SPI activity.\n");
    return 0;
}
