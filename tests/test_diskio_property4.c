/* Feature: multi-platform-fatfs, Property 4: SPI failure during I/O returns RES_ERROR */
/* Validates: Requirements 9.5 */
/*
 * Property: For any valid sector number and count where the underlying SPI
 * transfer fails (returns error status), disk_read and disk_write SHALL return
 * RES_ERROR.
 *
 * Strategy:
 * 1. Successfully initialize the disk with mock SPI that succeeds all init steps
 * 2. Swap to SPI ops where spi_transfer fails on the Nth call (randomized)
 * 3. Generate valid sector/count combinations (sector+count <= sector_count)
 * 4. Assert disk_read and disk_write return RES_ERROR
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "ff.h"
#include "diskio.h"
#include "platform_config.h"

/*-----------------------------------------------------------------------*/
/* External declarations for test hooks in diskio.c                       */
/*-----------------------------------------------------------------------*/

typedef struct {
    int (*spi_init)(uint32_t baudrate);
    int (*spi_transfer)(const uint8_t* tx, uint8_t* rx, size_t len);
    void (*gpio_set)(unsigned int pin, unsigned int value);
} spi_ops_t;

extern void diskio_set_spi_ops(const spi_ops_t* ops);
extern void diskio_reset_state(void);

/*-----------------------------------------------------------------------*/
/* Mock state for initialization phase (all succeed)                      */
/*-----------------------------------------------------------------------*/

/* SD card init protocol state machine */
typedef enum {
    INIT_IDLE,
    INIT_CMD0_SENT,
    INIT_CMD8_SENT,
    INIT_ACMD41_READY,
    INIT_CSD_READY,
    INIT_COMPLETE
} init_phase_t;

static init_phase_t g_init_phase;
static int g_init_cmd_index;  /* Track which command we're processing */
static int g_init_transfer_count;
static bool g_waiting_for_response;

/* For the failing SPI ops */
static int g_fail_on_call_n;    /* Which transfer call to fail on (1-based) */
static int g_transfer_call_count; /* Current call counter for failing ops */

/*-----------------------------------------------------------------------*/
/* Successful init mock: simulates full SD card init sequence             */
/*-----------------------------------------------------------------------*/

static int mock_init_spi_init(uint32_t baudrate) {
    (void)baudrate;
    return 0;
}

static int mock_init_spi_transfer(const uint8_t* tx, uint8_t* rx, size_t len) {
    g_init_transfer_count++;

    /* During init: respond appropriately to each phase */

    /* If rx buffer provided, fill with default 0xFF */
    if (rx) {
        memset(rx, 0xFF, len);
    }

    /* Detect command frames (6 bytes starting with 0x4x) */
    if (tx && len == 6 && (tx[0] & 0xC0) == 0x40) {
        uint8_t cmd = tx[0];

        switch (cmd) {
        case 0x40: /* CMD0 - GO_IDLE_STATE */
            g_init_phase = INIT_CMD0_SENT;
            g_waiting_for_response = true;
            break;
        case 0x48: /* CMD8 - SEND_IF_COND */
            g_init_phase = INIT_CMD8_SENT;
            g_waiting_for_response = true;
            break;
        case 0x77: /* CMD55 - APP_CMD prefix */
            g_waiting_for_response = true;
            break;
        case 0x69: /* ACMD41 - SD_SEND_OP_COND */
            g_init_phase = INIT_ACMD41_READY;
            g_waiting_for_response = true;
            break;
        case 0x49: /* CMD9 - SEND_CSD */
            g_init_phase = INIT_CSD_READY;
            g_waiting_for_response = true;
            break;
        default:
            g_waiting_for_response = true;
            break;
        }
        return 0;
    }

    /* Single byte exchange (tx and rx both 1 byte) - response to commands */
    if (tx && rx && len == 1) {
        if (g_waiting_for_response) {
            switch (g_init_phase) {
            case INIT_CMD0_SENT:
                /* CMD0 response: 0x01 (idle state) */
                rx[0] = 0x01;
                g_waiting_for_response = false;
                break;
            case INIT_CMD8_SENT:
                /* CMD8 response: 0x01 */
                rx[0] = 0x01;
                g_waiting_for_response = false;
                break;
            case INIT_ACMD41_READY:
                /* ACMD41 response: 0x00 (ready) */
                rx[0] = 0x00;
                g_waiting_for_response = false;
                break;
            case INIT_CSD_READY:
                /* CMD9 response: 0x00 (success) */
                rx[0] = 0x00;
                g_waiting_for_response = false;
                break;
            default:
                /* CMD55 or other: respond 0x01 */
                rx[0] = 0x01;
                g_waiting_for_response = false;
                break;
            }
        } else if (g_init_phase == INIT_CSD_READY) {
            /* After CMD9 accepted, next single-byte reads wait for data token */
            rx[0] = 0xFE; /* SD_DATA_TOKEN */
        } else {
            rx[0] = 0xFF; /* Idle/no-op */
        }
        return 0;
    }

    /* 4-byte R7 response after CMD8 */
    if (!tx && rx && len == 4 && g_init_phase == INIT_CMD8_SENT) {
        rx[0] = 0x00;
        rx[1] = 0x00;
        rx[2] = 0x01; /* Voltage accepted */
        rx[3] = 0xAA; /* Check pattern */
        return 0;
    }

    /* 16-byte CSD register read */
    if (!tx && rx && len == 16 && g_init_phase == INIT_CSD_READY) {
        /* CSD v2.0 (SDHC): c_size = 0x000FFF => (0xFFF+1)*1024 = 4194304 sectors = 2GB */
        memset(rx, 0, 16);
        rx[0] = 0x40; /* CSD structure v2.0 */
        rx[7] = 0x00; /* c_size[21:16] */
        rx[8] = 0x0F; /* c_size[15:8] */
        rx[9] = 0xFF; /* c_size[7:0] -> c_size = 0x0FFF = 4095 => (4095+1)*1024 = 4194304 sectors */
        g_init_phase = INIT_COMPLETE;
        return 0;
    }

    /* 10-byte dummy clock burst (CS high, init clocks) */
    if (tx && !rx && len == 10) {
        return 0;
    }

    /* Any other send-only (CS select/deselect clock, CRC bytes, etc) */
    if (tx && !rx) {
        return 0;
    }

    return 0;
}

static void mock_init_gpio_set(unsigned int pin, unsigned int value) {
    (void)pin;
    (void)value;
}

static const spi_ops_t mock_init_ops = {
    .spi_init = mock_init_spi_init,
    .spi_transfer = mock_init_spi_transfer,
    .gpio_set = mock_init_gpio_set
};

/*-----------------------------------------------------------------------*/
/* Failing SPI mock: spi_transfer fails on Nth call                      */
/*-----------------------------------------------------------------------*/

static int mock_fail_spi_init(uint32_t baudrate) {
    (void)baudrate;
    return 0;
}

static int mock_fail_spi_transfer(const uint8_t* tx, uint8_t* rx, size_t len) {
    (void)tx;
    (void)len;

    g_transfer_call_count++;

    if (g_transfer_call_count >= g_fail_on_call_n) {
        /* Fill rx with 0xFF to avoid UB if code reads it despite error */
        if (rx) {
            memset(rx, 0xFF, len);
        }
        return -1; /* SPI failure */
    }

    /* Succeed before the fail point - provide benign responses */
    if (rx) {
        memset(rx, 0xFF, len);

        /* For single-byte exchange, return 0x00 (command accepted) */
        if (len == 1 && tx) {
            rx[0] = 0x00;
        }
    }
    return 0;
}

static void mock_fail_gpio_set(unsigned int pin, unsigned int value) {
    (void)pin;
    (void)value;
}

static const spi_ops_t mock_fail_ops = {
    .spi_init = mock_fail_spi_init,
    .spi_transfer = mock_fail_spi_transfer,
    .gpio_set = mock_fail_gpio_set
};

/*-----------------------------------------------------------------------*/
/* Simple PRNG (xorshift32)                                              */
/*-----------------------------------------------------------------------*/

static uint32_t prng_state;

static uint32_t xorshift32(void) {
    uint32_t x = prng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    prng_state = x;
    return x;
}

/*-----------------------------------------------------------------------*/
/* Test runner                                                            */
/*-----------------------------------------------------------------------*/

#define NUM_ITERATIONS 150
#define SECTOR_COUNT   4194304u  /* Must match the CSD mock above */
#define MAX_COUNT      8         /* Maximum sectors per operation for test */

int main(void) {
    int passed = 0;
    int failed = 0;

    prng_state = (uint32_t)time(NULL);
    if (prng_state == 0) prng_state = 12345;

    printf("Property 4: SPI failure during I/O returns RES_ERROR\n");
    printf("Running %d iterations...\n", NUM_ITERATIONS);

    for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
        /* --- Phase 1: Initialize the disk successfully --- */
        diskio_reset_state();
        g_init_phase = INIT_IDLE;
        g_init_cmd_index = 0;
        g_init_transfer_count = 0;
        g_waiting_for_response = false;

        diskio_set_spi_ops(&mock_init_ops);

        DSTATUS init_status = disk_initialize(0);
        if (init_status != 0) {
            printf("  FAIL [iter %d]: disk_initialize failed during setup (status=0x%02X)\n",
                   iter, init_status);
            failed++;
            continue;
        }

        /* Verify disk is ready */
        if (disk_status(0) != 0) {
            printf("  FAIL [iter %d]: disk not ready after init\n", iter);
            failed++;
            continue;
        }

        /* --- Phase 2: Generate valid sector/count --- */
        uint32_t sector = xorshift32() % (SECTOR_COUNT - 1); /* Leave room for count */
        uint32_t remaining = SECTOR_COUNT - sector;
        uint32_t count = 1 + (xorshift32() % (remaining < MAX_COUNT ? remaining : MAX_COUNT));

        /* Ensure sector + count <= SECTOR_COUNT */
        if (sector + count > SECTOR_COUNT) {
            count = 1;
        }

        /* --- Phase 3: Inject SPI failure --- */
        /* Randomize which transfer call fails (1 to 10) */
        g_fail_on_call_n = 1 + (int)(xorshift32() % 10);
        g_transfer_call_count = 0;

        diskio_set_spi_ops(&mock_fail_ops);

        /* --- Phase 4: Test disk_read --- */
        uint8_t read_buf[512 * MAX_COUNT];
        memset(read_buf, 0xAA, sizeof(read_buf));

        DRESULT read_result = disk_read(0, read_buf, (LBA_t)sector, (UINT)count);
        if (read_result != RES_ERROR) {
            printf("  FAIL [iter %d]: disk_read(sector=%u, count=%u, fail_on=%d) "
                   "returned %d, expected RES_ERROR(%d)\n",
                   iter, sector, (unsigned)count, g_fail_on_call_n,
                   read_result, RES_ERROR);
            failed++;
            continue;
        }

        /* --- Phase 5: Test disk_write --- */
        g_transfer_call_count = 0; /* Reset counter for write test */

        uint8_t write_buf[512 * MAX_COUNT];
        memset(write_buf, 0x55, sizeof(write_buf));

        DRESULT write_result = disk_write(0, write_buf, (LBA_t)sector, (UINT)count);
        if (write_result != RES_ERROR) {
            printf("  FAIL [iter %d]: disk_write(sector=%u, count=%u, fail_on=%d) "
                   "returned %d, expected RES_ERROR(%d)\n",
                   iter, sector, (unsigned)count, g_fail_on_call_n,
                   write_result, RES_ERROR);
            failed++;
            continue;
        }

        passed++;
    }

    printf("\nResults: %d passed, %d failed out of %d iterations\n",
           passed, failed, NUM_ITERATIONS);

    if (failed > 0) {
        printf("PROPERTY 4 FAILED\n");
        return 1;
    }

    printf("PROPERTY 4 PASSED\n");
    return 0;
}
