cmake_minimum_required(VERSION 3.13)
project(diskio_tests C)

set(CMAKE_C_STANDARD 11)

# Project root (one level up from tests/)
set(PROJECT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

# Paths
set(FATFS_SRC ${PROJECT_ROOT}/src/Dependency/fatfs/source)
set(RP2040_PLATFORM ${PROJECT_ROOT}/src/lib/Platform/RP2040)

# Property Test 1: Initialization failure returns STA_NOINIT
add_executable(test_diskio_property1
    test_diskio_property1.c
    ${RP2040_PLATFORM}/diskio.c
)

target_compile_definitions(test_diskio_property1 PRIVATE
    DISKIO_TEST_MODE
)

target_include_directories(test_diskio_property1 PRIVATE
    ${FATFS_SRC}
    ${RP2040_PLATFORM}
)

# Property Test 2: Uninitialized disk blocks all operations
add_executable(test_diskio_property2
    test_diskio_property2.c
    ${RP2040_PLATFORM}/diskio.c
)

target_compile_definitions(test_diskio_property2 PRIVATE
    DISKIO_TEST_MODE
)

target_include_directories(test_diskio_property2 PRIVATE
    ${FATFS_SRC}
    ${RP2040_PLATFORM}
)

# Property Test 5: Unsupported ioctl command returns RES_PARERR
add_executable(test_diskio_property5
    test_diskio_property5.c
    ${RP2040_PLATFORM}/diskio.c
)

target_compile_definitions(test_diskio_property5 PRIVATE
    DISKIO_TEST_MODE
)

target_include_directories(test_diskio_property5 PRIVATE
    ${FATFS_SRC}
    ${RP2040_PLATFORM}
)

# Property Test 4: SPI failure during I/O returns RES_ERROR
add_executable(test_diskio_property4
    test_diskio_property4.c
    ${RP2040_PLATFORM}/diskio.c
)

target_compile_definitions(test_diskio_property4 PRIVATE
    DISKIO_TEST_MODE
)

target_include_directories(test_diskio_property4 PRIVATE
    ${FATFS_SRC}
    ${RP2040_PLATFORM}
)

# Property Test 3: Out-of-bounds sector returns RES_PARERR
add_executable(test_diskio_property3
    test_diskio_property3.c
    ${RP2040_PLATFORM}/diskio.c
)

target_compile_definitions(test_diskio_property3 PRIVATE
    DISKIO_TEST_MODE
)

target_include_directories(test_diskio_property3 PRIVATE
    ${FATFS_SRC}
    ${RP2040_PLATFORM}
)

# ESP32 Unit Tests: diskio error handling
set(ESP32_PLATFORM ${PROJECT_ROOT}/src/lib/Platform/ESP32)

add_executable(test_esp32_diskio
    test_esp32_diskio.c
    ${ESP32_PLATFORM}/diskio.c
)

target_compile_definitions(test_esp32_diskio PRIVATE
    DISKIO_TEST_MODE
)

target_include_directories(test_esp32_diskio PRIVATE
    ${FATFS_SRC}
    ${ESP32_PLATFORM}
)

# Unit Tests: refactored fileHelper.c
set(HELPER_DIR ${PROJECT_ROOT}/src/lib/Helper)

add_executable(test_filehelper
    test_filehelper.c
)

target_include_directories(test_filehelper PRIVATE
    ${FATFS_SRC}
    ${HELPER_DIR}
    ${RP2040_PLATFORM}
)

# Enable testing
enable_testing()
add_test(NAME property1_init_failure_returns_sta_noinit
    COMMAND test_diskio_property1
)
add_test(NAME property2_uninitialized_disk_blocks_operations
    COMMAND test_diskio_property2
)
add_test(NAME property3_out_of_bounds_sector_returns_res_parerr
    COMMAND test_diskio_property3
)

add_test(NAME property4_spi_failure_returns_res_error
    COMMAND test_diskio_property4
)
add_test(NAME property5_unsupported_ioctl_returns_res_parerr
    COMMAND test_diskio_property5
)

add_test(NAME esp32_diskio_error_handling
    COMMAND test_esp32_diskio
)

add_test(NAME filehelper_unit_tests
    COMMAND test_filehelper
)

# Structure validation smoke tests (Requirements 1.5, 4.3)

# Verify that the pure ChaN FatFS submodule exists with expected header
add_test(NAME fatfs_submodule_exists
    COMMAND ${CMAKE_COMMAND} -E env test -f ${PROJECT_ROOT}/src/Dependency/fatfs/source/ff.h)

# Verify no platform headers included outside src/lib/Platform/ directories
add_test(NAME no_platform_headers_outside_platform_dir
    COMMAND bash -c "! grep -rn 'hardware/spi.h\\|hardware/gpio.h\\|driver/spi_master.h\\|driver/gpio.h' ${PROJECT_ROOT}/src/ --include='*.c' --include='*.h' | grep -v 'src/lib/Platform/'"
)
