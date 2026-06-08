/* Unit Tests: Refactored fileHelper.c */
/*
 * Tests the refactored fileHelper.c for correct behavior:
 * 1. Platform_SDCard_Init failure aborts initialization (no f_mount called)
 * 2. f_mount error is logged and MountSdCard returns false
 * 3. Successful mount returns true
 *
 * Validates: Requirements 5.3, 5.4
 *
 * Approach: Since fileHelper.c is a single-compilation-unit (included as .c
 * with #ifndef guard), we provide mock implementations of all its dependencies
 * BEFORE including it. This lets us intercept and verify behavior.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/*-----------------------------------------------------------------------*/
/* Include ff.h for type definitions (FRESULT, FATFS, FIL, etc.)         */
/*-----------------------------------------------------------------------*/

#include "ff.h"

/*-----------------------------------------------------------------------*/
/* Prevent fileHelper.c from including the real platform_config.h         */
/* by defining its include guard before fileHelper.c is pulled in.        */
/*-----------------------------------------------------------------------*/

#define PLATFORM_CONFIG_H

/*-----------------------------------------------------------------------*/
/* Test Infrastructure                                                   */
/*-----------------------------------------------------------------------*/

static int g_tests_run = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(condition, msg) do { \
    g_tests_run++; \
    if (condition) { \
        g_tests_passed++; \
    } else { \
        g_tests_failed++; \
        printf("  FAIL: %s\n", msg); \
    } \
} while(0)

#define TEST_ASSERT_EQ(actual, expected, msg) do { \
    g_tests_run++; \
    if ((actual) == (expected)) { \
        g_tests_passed++; \
    } else { \
        g_tests_failed++; \
        printf("  FAIL: %s (got %d, expected %d)\n", msg, (int)(actual), (int)(expected)); \
    } \
} while(0)

/*-----------------------------------------------------------------------*/
/* Mock State Variables                                                   */
/*-----------------------------------------------------------------------*/

static bool mock_platform_init_return = true;
static bool mock_platform_init_called = false;

static FRESULT mock_f_mount_return = FR_OK;
static bool mock_f_mount_called = false;
static FATFS* mock_f_mount_fs_arg = NULL;

static FRESULT mock_f_open_return = FR_OK;
static bool mock_f_open_called = false;

static FRESULT mock_f_close_return = FR_OK;
static bool mock_f_close_called = false;

static FRESULT mock_f_chdrive_return = FR_OK;
static bool mock_f_chdrive_called = false;

static bool mock_f_unmount_called = false;

/*-----------------------------------------------------------------------*/
/* Mock Platform_SDCard_Init                                              */
/*-----------------------------------------------------------------------*/

bool Platform_SDCard_Init(void) {
    mock_platform_init_called = true;
    return mock_platform_init_return;
}

/*-----------------------------------------------------------------------*/
/* Mock FatFS API functions                                               */
/* These override the real FatFS functions. Since we do NOT link against  */
/* ff.c, these mocks are the only implementations available at link time. */
/*-----------------------------------------------------------------------*/

FRESULT f_mount(FATFS* fs, const TCHAR* path, BYTE opt) {
    (void)path;
    /* Distinguish real mount from unmount (f_unmount macro expands to f_mount(0, path, 0)) */
    if (fs == 0 && opt == 0) {
        mock_f_unmount_called = true;
        return FR_OK;
    }
    mock_f_mount_called = true;
    mock_f_mount_fs_arg = fs;
    return mock_f_mount_return;
}

FRESULT f_open(FIL* fp, const TCHAR* path, BYTE mode) {
    (void)fp;
    (void)path;
    (void)mode;
    mock_f_open_called = true;
    return mock_f_open_return;
}

FRESULT f_close(FIL* fp) {
    (void)fp;
    mock_f_close_called = true;
    return mock_f_close_return;
}

FRESULT f_chdrive(const TCHAR* path) {
    (void)path;
    mock_f_chdrive_called = true;
    return mock_f_chdrive_return;
}

/*-----------------------------------------------------------------------*/
/* Include fileHelper.c directly (single-compilation-unit pattern)        */
/*                                                                       */
/* At this point:                                                         */
/* - ff.h is already included (satisfies #include "ff.h" in fileHelper.c)*/
/* - PLATFORM_CONFIG_H is defined (prevents including platform_config.h) */
/* - Platform_SDCard_Init is already defined (satisfies the call)         */
/* - f_mount, f_open, f_close, f_chdrive are our mocks                   */
/*-----------------------------------------------------------------------*/

#include "fileHelper.c"

/*-----------------------------------------------------------------------*/
/* Helper: Reset all mock state                                          */
/*-----------------------------------------------------------------------*/

static void reset_mocks(void) {
    mock_platform_init_return = true;
    mock_platform_init_called = false;

    mock_f_mount_return = FR_OK;
    mock_f_mount_called = false;
    mock_f_mount_fs_arg = NULL;

    mock_f_open_return = FR_OK;
    mock_f_open_called = false;

    mock_f_close_return = FR_OK;
    mock_f_close_called = false;

    mock_f_chdrive_return = FR_OK;
    mock_f_chdrive_called = false;

    mock_f_unmount_called = false;
}

/*-----------------------------------------------------------------------*/
/* Test 1: Platform_SDCard_Init failure aborts initialization             */
/* When Platform_SDCard_Init returns false, MountSdCard must return false */
/* and must NOT call f_mount.                                            */
/* Validates: Requirement 5.4                                            */
/*-----------------------------------------------------------------------*/

static void test_mount_fails_when_platform_init_fails(void) {
    printf("\n[Test 1] Platform_SDCard_Init failure aborts initialization\n");

    reset_mocks();
    mock_platform_init_return = false;

    bool result = MountSdCard();

    TEST_ASSERT_EQ(result, false,
        "MountSdCard should return false when Platform_SDCard_Init fails");
    TEST_ASSERT(mock_platform_init_called,
        "Platform_SDCard_Init should have been called");
    TEST_ASSERT(!mock_f_mount_called,
        "f_mount should NOT be called when Platform_SDCard_Init fails");
}

/*-----------------------------------------------------------------------*/
/* Test 2: f_mount error is logged and returns false                      */
/* When f_mount returns an error, MountSdCard must return false.         */
/* Validates: Requirement 5.3, 5.4                                       */
/*-----------------------------------------------------------------------*/

static void test_mount_fails_when_f_mount_fails(void) {
    printf("\n[Test 2] f_mount error returns false\n");

    /* Test with FR_NOT_READY */
    reset_mocks();
    mock_platform_init_return = true;
    mock_f_mount_return = FR_NOT_READY;

    bool result = MountSdCard();

    TEST_ASSERT_EQ(result, false,
        "MountSdCard should return false when f_mount returns FR_NOT_READY");
    TEST_ASSERT(mock_platform_init_called,
        "Platform_SDCard_Init should have been called");
    TEST_ASSERT(mock_f_mount_called,
        "f_mount should have been called after successful Platform_SDCard_Init");

    /* Test with FR_DISK_ERR */
    reset_mocks();
    mock_platform_init_return = true;
    mock_f_mount_return = FR_DISK_ERR;

    result = MountSdCard();

    TEST_ASSERT_EQ(result, false,
        "MountSdCard should return false when f_mount returns FR_DISK_ERR");

    /* Test with FR_NO_FILESYSTEM */
    reset_mocks();
    mock_platform_init_return = true;
    mock_f_mount_return = FR_NO_FILESYSTEM;

    result = MountSdCard();

    TEST_ASSERT_EQ(result, false,
        "MountSdCard should return false when f_mount returns FR_NO_FILESYSTEM");
}

/*-----------------------------------------------------------------------*/
/* Test 3: Successful mount returns true                                  */
/* When both Platform_SDCard_Init and f_mount succeed, MountSdCard       */
/* returns true.                                                         */
/* Validates: Requirement 5.3, 5.4                                       */
/*-----------------------------------------------------------------------*/

static void test_mount_succeeds_when_all_ok(void) {
    printf("\n[Test 3] Successful mount returns true\n");

    reset_mocks();
    mock_platform_init_return = true;
    mock_f_mount_return = FR_OK;

    bool result = MountSdCard();

    TEST_ASSERT_EQ(result, true,
        "MountSdCard should return true when everything succeeds");
    TEST_ASSERT(mock_platform_init_called,
        "Platform_SDCard_Init should have been called");
    TEST_ASSERT(mock_f_mount_called,
        "f_mount should have been called");
}

/*-----------------------------------------------------------------------*/
/* Test 5: OpenFile and CloseFile behavior                                */
/* Validates basic file operation wrappers work correctly.                */
/*-----------------------------------------------------------------------*/

static void test_open_and_close_file(void) {
    printf("\n[Test 5] OpenFile and CloseFile behavior\n");

    FIL file;
    memset(&file, 0, sizeof(file));

    /* Test successful open */
    reset_mocks();
    mock_f_open_return = FR_OK;

    bool result = OpenFile(&file, "test.png");
    TEST_ASSERT_EQ(result, true,
        "OpenFile should return true when f_open succeeds");
    TEST_ASSERT(mock_f_open_called,
        "f_open should have been called");

    /* Test failed open */
    reset_mocks();
    mock_f_open_return = FR_NO_FILE;

    result = OpenFile(&file, "missing.png");
    TEST_ASSERT_EQ(result, false,
        "OpenFile should return false when f_open returns FR_NO_FILE");

    /* Test CloseFile with success */
    reset_mocks();
    mock_f_close_return = FR_OK;
    CloseFile(&file);
    TEST_ASSERT(mock_f_close_called,
        "f_close should have been called by CloseFile");
}

/*-----------------------------------------------------------------------*/
/* Test 6: SelectActiveDrive behavior                                     */
/*-----------------------------------------------------------------------*/

static void test_select_active_drive(void) {
    printf("\n[Test 6] SelectActiveDrive behavior\n");

    /* Test successful chdrive */
    reset_mocks();
    mock_f_chdrive_return = FR_OK;

    bool result = SelectActiveDrive();
    TEST_ASSERT_EQ(result, true,
        "SelectActiveDrive should return true when f_chdrive succeeds");
    TEST_ASSERT(mock_f_chdrive_called,
        "f_chdrive should have been called");

    /* Test failed chdrive */
    reset_mocks();
    mock_f_chdrive_return = FR_INVALID_DRIVE;

    result = SelectActiveDrive();
    TEST_ASSERT_EQ(result, false,
        "SelectActiveDrive should return false when f_chdrive fails");
}

/*-----------------------------------------------------------------------*/
/* Test 7: UnMountSdCard calls f_unmount                                  */
/*-----------------------------------------------------------------------*/

static void test_unmount_sd_card(void) {
    printf("\n[Test 7] UnMountSdCard behavior\n");

    reset_mocks();
    UnMountSdCard();

    TEST_ASSERT(mock_f_unmount_called,
        "f_unmount (f_mount with NULL fs) should have been called");
}

/*-----------------------------------------------------------------------*/
/* Main                                                                  */
/*-----------------------------------------------------------------------*/

int main(void) {
    printf("=== Unit Tests: Refactored fileHelper.c ===\n");
    printf("Validates: Requirements 5.3, 5.4\n");

    test_mount_fails_when_platform_init_fails();
    test_mount_fails_when_f_mount_fails();
    test_mount_succeeds_when_all_ok();
    test_open_and_close_file();
    test_select_active_drive();
    test_unmount_sd_card();

    printf("\n=== Results ===\n");
    printf("Total: %d, Passed: %d, Failed: %d\n",
           g_tests_run, g_tests_passed, g_tests_failed);

    if (g_tests_failed > 0) {
        printf("\nFAILED: %d test(s) did not pass.\n", g_tests_failed);
        return 1;
    }

    printf("\nALL TESTS PASSED\n");
    return 0;
}
