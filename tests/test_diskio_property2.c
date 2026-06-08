/* Feature: multi-platform-fatfs, Property 2: Uninitialized disk blocks all operations */
/*
 * Property-based test: For any sector number, count value, buffer, and ioctl
 * command, when the disk is in STA_NOINIT state, calling disk_read, disk_write,
 * and disk_ioctl SHALL return RES_NOTRDY without performing any SPI hardware access.
 *
 * Validates: Requirements 9.2
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Must pass -DDISKIO_TEST_MODE when compiling */

#include "ff.h"
#include "diskio.h"

/*-----------------------------------------------------------------------*/
/* Redeclare spi_ops_t and extern functions from diskio.c (test mode)     */
/*-----------------------------------------------------------------------*/

typedef unsigned int uint;

typedef struct {
    int (*spi_init)(uint32_t baudrate);
    int (*spi_transfer)(const uint8_t* tx, uint8_t* rx, size_t len);
    void (*gpio_set)(uint pin, uint value);
} spi_ops_t;

extern void diskio_set_spi_ops(const spi_ops_t* ops);
extern void diskio_reset_state(void);

/*-----------------------------------------------------------------------*/
/* Mock SPI ops with call counter                                         */
/*-----------------------------------------------------------------------*/

static int mock_spi_call_count = 0;

static int mock_spi_init(uint32_t baudrate) {
    (void)baudrate;
    mock_spi_call_count++;
    return 0;
}

static int mock_spi_transfer(const uint8_t* tx, uint8_t* rx, size_t len) {
    (void)tx; (void)rx; (void)len;
    mock_spi_call_count++;
    return 0;
}

static void mock_gpio_set(uint pin, uint value) {
    (void)pin; (void)value;
    mock_spi_call_count++;
}

static const spi_ops_t mock_ops = {
    .spi_init = mock_spi_init,
    .spi_transfer = mock_spi_transfer,
    .gpio_set = mock_gpio_set
};

/*-----------------------------------------------------------------------*/
/* Lightweight xorshift32 PRNG                                           */
/*-----------------------------------------------------------------------*/

static uint32_t prng_state = 0xDEADBEEF;

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

#define NUM_ITERATIONS 100
#define BUFFER_SIZE 512

int main(void) {
    int failures = 0;
    uint8_t buffer[BUFFER_SIZE];
    memset(buffer, 0xAA, BUFFER_SIZE);

    printf("Property 2: Uninitialized disk blocks all operations\n");
    printf("Running %d iterations...\n", NUM_ITERATIONS);

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        /* Generate random inputs */
        uint32_t sector = xorshift32();
        uint32_t count  = (xorshift32() % 256) + 1; /* 1..256 */
        uint8_t ioctl_cmd = (uint8_t)(xorshift32() & 0xFF);

        /* Reset state to uninitialized */
        diskio_reset_state();

        /* Install mock SPI ops and reset call counter */
        diskio_set_spi_ops(&mock_ops);
        mock_spi_call_count = 0;

        /* --- Test disk_read returns RES_NOTRDY --- */
        DRESULT read_result = disk_read(0, buffer, (LBA_t)sector, (UINT)count);
        if (read_result != RES_NOTRDY) {
            printf("  FAIL [iter %d]: disk_read(sector=%u, count=%u) returned %d, expected RES_NOTRDY (%d)\n",
                   i, sector, count, (int)read_result, (int)RES_NOTRDY);
            failures++;
            continue;
        }

        /* --- Test disk_write returns RES_NOTRDY --- */
        DRESULT write_result = disk_write(0, buffer, (LBA_t)sector, (UINT)count);
        if (write_result != RES_NOTRDY) {
            printf("  FAIL [iter %d]: disk_write(sector=%u, count=%u) returned %d, expected RES_NOTRDY (%d)\n",
                   i, sector, count, (int)write_result, (int)RES_NOTRDY);
            failures++;
            continue;
        }

        /* --- Test disk_ioctl returns RES_NOTRDY --- */
        uint32_t ioctl_buffer = 0;
        DRESULT ioctl_result = disk_ioctl(0, ioctl_cmd, &ioctl_buffer);
        if (ioctl_result != RES_NOTRDY) {
            printf("  FAIL [iter %d]: disk_ioctl(cmd=%u) returned %d, expected RES_NOTRDY (%d)\n",
                   i, ioctl_cmd, (int)ioctl_result, (int)RES_NOTRDY);
            failures++;
            continue;
        }

        /* --- Assert no SPI calls were made --- */
        if (mock_spi_call_count != 0) {
            printf("  FAIL [iter %d]: Expected 0 SPI calls but got %d (sector=%u, count=%u, cmd=%u)\n",
                   i, mock_spi_call_count, sector, count, ioctl_cmd);
            failures++;
            continue;
        }
    }

    printf("\nResults: %d/%d iterations passed\n", NUM_ITERATIONS - failures, NUM_ITERATIONS);

    if (failures > 0) {
        printf("FAILED: %d iterations produced incorrect results\n", failures);
        return 1;
    }

    printf("PASSED: All operations correctly blocked when disk uninitialized\n");
    return 0;
}
