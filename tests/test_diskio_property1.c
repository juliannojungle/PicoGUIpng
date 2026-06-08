/* Feature: multi-platform-fatfs, Property 1: Initialization failure returns STA_NOINIT */
/*
 * Property-Based Test: For any SPI bus failure mode (timeout, bus error, no card
 * detected, invalid response), disk_initialize SHALL return a status with the
 * STA_NOINIT flag set.
 *
 * Validates: Requirements 9.1
 *
 * Approach: Lightweight custom PBT runner using xorshift PRNG to generate random
 * SPI failure scenarios across 100+ iterations.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

/* Define 'uint' type used by spi_ops_t (normally from Pico SDK) */
typedef unsigned int uint;

#include "ff.h"
#include "diskio.h"
#include "platform_config.h"

/* Replicate spi_ops_t definition from diskio.c for test access */
typedef struct {
    int (*spi_init)(uint32_t baudrate);
    int (*spi_transfer)(const uint8_t* tx, uint8_t* rx, size_t len);
    void (*gpio_set)(uint pin, uint value);
} spi_ops_t;

/* Forward declarations for diskio test mode injection */
extern void diskio_set_spi_ops(const spi_ops_t* ops);
extern void diskio_reset_state(void);

/*-----------------------------------------------------------------------*/
/* PBT Infrastructure: Xorshift PRNG                                     */
/*-----------------------------------------------------------------------*/

typedef struct {
    uint32_t state;
} xorshift32_t;

static uint32_t xorshift32(xorshift32_t* rng) {
    uint32_t x = rng->state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng->state = x;
    return x;
}

static uint32_t xorshift32_range(xorshift32_t* rng, uint32_t min, uint32_t max) {
    if (min >= max) return min;
    return min + (xorshift32(rng) % (max - min));
}

/*-----------------------------------------------------------------------*/
/* SPI Failure Mode Enumeration                                          */
/*-----------------------------------------------------------------------*/

typedef enum {
    FAIL_SPI_INIT = 0,          /* spi_init returns -1 (hardware failure) */
    FAIL_BUS_ERROR,             /* spi_transfer returns -1 on first call */
    FAIL_NO_CARD,               /* spi_transfer returns 0 but writes 0xFF (timeout/no card) */
    FAIL_INVALID_CMD0_RESPONSE, /* spi_transfer returns 0 but writes invalid response (not 0x01) */
    FAIL_MODE_COUNT
} spi_failure_mode_t;

static const char* failure_mode_names[] = {
    "FAIL_SPI_INIT (spi_init returns -1)",
    "FAIL_BUS_ERROR (spi_transfer returns -1)",
    "FAIL_NO_CARD (spi_transfer returns 0xFF response)",
    "FAIL_INVALID_CMD0_RESPONSE (invalid response to CMD0)"
};

/*-----------------------------------------------------------------------*/
/* Mock SPI Operations                                                   */
/*-----------------------------------------------------------------------*/

/* State for the current test iteration */
static spi_failure_mode_t current_failure_mode;
static int transfer_call_count;
static uint8_t invalid_response_byte; /* For FAIL_INVALID_CMD0_RESPONSE */

static int mock_spi_init(uint32_t baudrate) {
    (void)baudrate;
    if (current_failure_mode == FAIL_SPI_INIT) {
        return -1; /* SPI hardware init failure */
    }
    return 0;
}

static int mock_spi_transfer(const uint8_t* tx, uint8_t* rx, size_t len) {
    transfer_call_count++;

    switch (current_failure_mode) {
    case FAIL_BUS_ERROR:
        /* First transfer after init (the 80-clock-cycle dummy bytes) fails */
        if (transfer_call_count == 1) {
            return -1; /* Bus error */
        }
        /* Shouldn't reach here since init sends clocks first */
        if (rx) memset(rx, 0xFF, len);
        return 0;

    case FAIL_NO_CARD:
        /* All transfers succeed but card never responds (always 0xFF) */
        if (rx) {
            memset(rx, 0xFF, len);
        }
        return 0;

    case FAIL_INVALID_CMD0_RESPONSE:
        /* Transfers succeed, but CMD0 response is invalid (not 0x01) */
        if (rx) {
            /*
             * The initialization sequence:
             * 1. First transfer: 10 bytes of 0xFF for 80 clock cycles (tx only, rx=NULL)
             * 2. CMD0 frame: 6 bytes sent
             * 3. Response polling: single byte exchanges looking for non-0xFF
             *
             * We return the invalid_response_byte as the card's R1 response.
             * Since response polling looks for (response & 0x80) == 0,
             * we need a byte with bit 7 clear but != 0x01.
             */
            memset(rx, invalid_response_byte, len);
        }
        return 0;

    default:
        /* For FAIL_SPI_INIT: we shouldn't reach transfers, but handle gracefully */
        if (rx) memset(rx, 0xFF, len);
        return 0;
    }
}

static void mock_gpio_set(uint pin, uint value) {
    (void)pin;
    (void)value;
    /* No-op for testing */
}

static const spi_ops_t mock_ops = {
    .spi_init = mock_spi_init,
    .spi_transfer = mock_spi_transfer,
    .gpio_set = mock_gpio_set
};

/*-----------------------------------------------------------------------*/
/* Property Test Runner                                                  */
/*-----------------------------------------------------------------------*/

#define NUM_ITERATIONS 200
#define INITIAL_SEED 0xDEADBEEF

typedef struct {
    int total;
    int passed;
    int failed;
    uint32_t failing_seed;
    int failing_iteration;
    spi_failure_mode_t failing_mode;
    uint8_t failing_invalid_byte;
    DSTATUS failing_result;
} test_result_t;

static bool run_single_test(spi_failure_mode_t mode, uint8_t inv_byte) {
    /* Reset diskio state */
    diskio_reset_state();
    transfer_call_count = 0;

    /* Configure failure mode */
    current_failure_mode = mode;
    invalid_response_byte = inv_byte;

    /* Inject mock ops */
    diskio_set_spi_ops(&mock_ops);

    /* Call disk_initialize */
    DSTATUS status = disk_initialize(0);

    /* Property: STA_NOINIT flag MUST be set */
    return (status & STA_NOINIT) != 0;
}

static test_result_t run_property_test(uint32_t seed) {
    test_result_t result = {0};
    xorshift32_t rng = { .state = seed };

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        result.total++;

        /* Generate random failure mode */
        spi_failure_mode_t mode = (spi_failure_mode_t)xorshift32_range(&rng, 0, FAIL_MODE_COUNT);

        /* Generate random invalid response byte for FAIL_INVALID_CMD0_RESPONSE */
        /* Must have bit 7 clear (so it's seen as a response) but != 0x01 (valid idle) */
        uint8_t inv_byte = 0x00;
        if (mode == FAIL_INVALID_CMD0_RESPONSE) {
            /* Generate byte with bit 7 = 0, but != 0x01 */
            do {
                inv_byte = (uint8_t)(xorshift32(&rng) & 0x7F);
            } while (inv_byte == 0x01);
        }

        bool passed = run_single_test(mode, inv_byte);

        if (passed) {
            result.passed++;
        } else {
            result.failed++;
            if (result.failed == 1) {
                /* Record first failure as counterexample */
                result.failing_seed = seed;
                result.failing_iteration = i;
                result.failing_mode = mode;
                result.failing_invalid_byte = inv_byte;
                result.failing_result = disk_initialize(0); /* Re-run to capture */
                /* Actually we already know it failed, let's just record what we got */
                diskio_reset_state();
                transfer_call_count = 0;
                current_failure_mode = mode;
                invalid_response_byte = inv_byte;
                diskio_set_spi_ops(&mock_ops);
                result.failing_result = disk_initialize(0);
            }
        }
    }

    return result;
}

/*-----------------------------------------------------------------------*/
/* Also test: NULL spi_ops returns STA_NOINIT                            */
/*-----------------------------------------------------------------------*/

static bool test_null_ops(void) {
    diskio_reset_state();
    diskio_set_spi_ops(NULL);
    DSTATUS status = disk_initialize(0);
    return (status & STA_NOINIT) != 0;
}

/*-----------------------------------------------------------------------*/
/* Also test: invalid pdrv returns STA_NOINIT                            */
/*-----------------------------------------------------------------------*/

static bool test_invalid_pdrv(xorshift32_t* rng) {
    diskio_reset_state();
    diskio_set_spi_ops(&mock_ops);
    /* pdrv != 0 should always return STA_NOINIT */
    BYTE pdrv = (BYTE)xorshift32_range(rng, 1, 256);
    DSTATUS status = disk_initialize(pdrv);
    return (status & STA_NOINIT) != 0;
}

/*-----------------------------------------------------------------------*/
/* Main                                                                  */
/*-----------------------------------------------------------------------*/

int main(void) {
    printf("=== Property 1: Initialization failure returns STA_NOINIT ===\n");
    printf("Validates: Requirements 9.1\n");
    printf("Iterations: %d\n", NUM_ITERATIONS);
    printf("Seed: 0x%08X\n\n", INITIAL_SEED);

    int overall_pass = 1;

    /* Test: NULL spi_ops */
    printf("[1/3] Testing NULL spi_ops -> STA_NOINIT... ");
    if (test_null_ops()) {
        printf("PASS\n");
    } else {
        printf("FAIL\n");
        printf("  Counterexample: spi_ops=NULL, disk_initialize(0) did not return STA_NOINIT\n");
        overall_pass = 0;
    }

    /* Test: invalid pdrv */
    printf("[2/3] Testing invalid pdrv -> STA_NOINIT (10 random values)... ");
    {
        xorshift32_t rng = { .state = 0xCAFEBABE };
        int pdrv_pass = 1;
        for (int i = 0; i < 10; i++) {
            if (!test_invalid_pdrv(&rng)) {
                pdrv_pass = 0;
                break;
            }
        }
        if (pdrv_pass) {
            printf("PASS\n");
        } else {
            printf("FAIL\n");
            printf("  Counterexample: pdrv != 0 did not return STA_NOINIT\n");
            overall_pass = 0;
        }
    }

    /* Main property test: all SPI failure modes */
    printf("[3/3] Property test: %d iterations with random SPI failures... ", NUM_ITERATIONS);
    fflush(stdout);

    test_result_t result = run_property_test(INITIAL_SEED);

    if (result.failed == 0) {
        printf("PASS (%d/%d)\n", result.passed, result.total);
    } else {
        printf("FAIL (%d/%d passed, %d failed)\n", result.passed, result.total, result.failed);
        printf("\n  === COUNTEREXAMPLE ===\n");
        printf("  Seed: 0x%08X\n", result.failing_seed);
        printf("  Iteration: %d\n", result.failing_iteration);
        printf("  Failure mode: %s\n", failure_mode_names[result.failing_mode]);
        if (result.failing_mode == FAIL_INVALID_CMD0_RESPONSE) {
            printf("  Invalid response byte: 0x%02X\n", result.failing_invalid_byte);
        }
        printf("  disk_initialize returned: 0x%02X (expected STA_NOINIT=0x%02X set)\n",
               result.failing_result, STA_NOINIT);
        printf("  =====================\n");
        overall_pass = 0;
    }

    printf("\n");
    if (overall_pass) {
        printf("ALL TESTS PASSED\n");
        return 0;
    } else {
        printf("SOME TESTS FAILED\n");
        return 1;
    }
}
