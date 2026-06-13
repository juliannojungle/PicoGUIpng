# Implementation Plan: Multi-Platform FatFS

## Overview

Refatorar o gui.ll de mono-plataforma (RP2040 com no-OS-FatFS) para multi-plataforma (RP2040 + ESP32) usando FatFS puro de ChaN. O plano segue uma abordagem incremental: primeiro a infraestrutura (submódulo, diretórios, configuração), depois as implementações de plataforma, refatoração do código de aplicação, sistema de build, e finalmente testes.

## Tasks

- [x] 1. Set up FatFS dependency and project structure
  - [x] 1.1 Add ChaN FatFS as a Git submodule at `src/Dependency/fatfs`
    - Run `git submodule add` for the ChaN FatFS repository into `src/Dependency/fatfs`
    - Verify that `src/Dependency/fatfs/source/ff.h`, `diskio.h`, `ff.c`, `ffsystem.c`, `ffunicode.c` exist
    - _Requirements: 1.1_

  - [x] 1.2 Remove the no-OS-FatFS submodule from `src/Dependency/no-OS-FatFS`
    - Run `git submodule deinit` and `git rm` on `src/Dependency/no-OS-FatFS`
    - Remove entries from `.gitmodules` and `.git/config`
    - _Requirements: 1.2_

  - [x] 1.3 Create platform directory structure under `src/lib/Platform/`
    - Create `src/lib/Platform/RP2040/` and `src/lib/Platform/ESP32/` directories
    - Move existing `src/pre-executable.cmake` to `src/lib/Platform/RP2040/pre-executable.cmake`
    - Move existing `src/pos-executable.cmake` to `src/lib/Platform/RP2040/pos-executable.cmake`
    - _Requirements: 6.1, 4.3_

- [x] 2. Implement platform configuration headers
  - [x] 2.1 Create `src/lib/Platform/RP2040/platform_config.h`
    - Define SPI pin constants (SCLK, MOSI, MISO, CS) with `#ifndef` guards for compile-time override
    - Define `SD_SPI_BAUDRATE` defaulting to 25 MHz
    - Declare `bool Platform_SDCard_Init(void)` prototype
    - _Requirements: 8.1, 8.3, 8.4, 4.4_

  - [x] 2.2 Create `src/lib/Platform/ESP32/platform_config.h`
    - Define SPI pin constants (SCLK, MOSI, MISO, CS) with `#ifndef` guards for compile-time override
    - Define `SD_SPI_BAUDRATE` defaulting to 25 MHz
    - Declare `bool Platform_SDCard_Init(void)` prototype
    - _Requirements: 8.2, 8.3, 8.4, 4.4_

- [x] 3. Implement diskio for RP2040
  - [x] 3.1 Create `src/lib/Platform/RP2040/diskio.c`
    - Include `diskio.h`, `platform_config.h`, and `hardware/spi.h` from Pico SDK
    - Define internal `sd_card_state_t` struct to track initialization state, sector count, sector size, and block size
    - Implement `disk_initialize`: init SPI peripheral, send SD card initialization sequence (CMD0, CMD8, ACMD41), populate state
    - Implement `disk_status`: return current status based on internal state
    - Implement `disk_read`: validate state and sector bounds, perform SPI read via CMD17/CMD18
    - Implement `disk_write`: validate state and sector bounds, perform SPI write via CMD24/CMD25
    - Implement `disk_ioctl`: handle `CTRL_SYNC`, `GET_SECTOR_COUNT`, `GET_SECTOR_SIZE`, `GET_BLOCK_SIZE`; return `RES_PARERR` for unsupported commands
    - _Requirements: 2.1, 2.2, 2.3, 2.4, 2.5, 2.6, 9.1, 9.2, 9.3, 9.4, 9.5, 9.6_

  - [x] 3.2 Write property test: Initialization failure returns STA_NOINIT
    - **Property 1: Initialization failure returns STA_NOINIT**
    - Use theft library with mock SPI ops injected via function pointers
    - Generate arbitrary SPI failure modes (timeout, bus error, no card, invalid response)
    - Assert `disk_initialize` returns status with `STA_NOINIT` flag set for all failure modes
    - **Validates: Requirements 9.1**

  - [x] 3.3 Write property test: Uninitialized disk blocks all operations
    - **Property 2: Uninitialized disk blocks all operations**
    - Generate arbitrary sector, count, buffer pointer, and ioctl command values
    - Set disk state to `STA_NOINIT`
    - Assert `disk_read`, `disk_write`, and `disk_ioctl` all return `RES_NOTRDY` without SPI calls
    - **Validates: Requirements 9.2**

  - [x] 3.4 Write property test: Out-of-bounds sector returns RES_PARERR
    - **Property 3: Out-of-bounds sector returns RES_PARERR**
    - Generate sector values >= `sector_count` and arbitrary count values
    - Assert `disk_read` and `disk_write` both return `RES_PARERR` without SPI activity
    - **Validates: Requirements 9.3, 9.4**

  - [x] 3.5 Write property test: SPI failure during I/O returns RES_ERROR
    - **Property 4: SPI failure during I/O returns RES_ERROR**
    - Generate valid sector/count combinations with mock SPI returning transfer errors
    - Assert `disk_read` and `disk_write` return `RES_ERROR`
    - **Validates: Requirements 9.5**

  - [x] 3.6 Write property test: Unsupported ioctl command returns RES_PARERR
    - **Property 5: Unsupported ioctl command returns RES_PARERR**
    - Generate arbitrary `uint8_t` ioctl command values excluding `CTRL_SYNC`, `GET_SECTOR_COUNT`, `GET_SECTOR_SIZE`, `GET_BLOCK_SIZE`
    - Assert `disk_ioctl` returns `RES_PARERR`
    - **Validates: Requirements 9.6**

- [x] 4. Checkpoint - Ensure RP2040 diskio compiles and tests pass
  - Ensure all tests pass, ask the user if questions arise.

- [x] 5. Implement diskio for ESP32
  - [x] 5.1 Create `src/lib/Platform/ESP32/diskio.c`
    - Include `diskio.h`, `platform_config.h`, and `driver/spi_master.h` from ESP-IDF
    - Define internal `sd_card_state_t` struct (same logical structure as RP2040)
    - Implement `disk_initialize`: init SPI bus via `spi_bus_initialize`, add SD card device, send initialization sequence
    - Implement `disk_status`: return current status based on internal state
    - Implement `disk_read`: validate state and sector bounds, perform SPI read using ESP-IDF transaction API
    - Implement `disk_write`: validate state and sector bounds, perform SPI write using ESP-IDF transaction API
    - Implement `disk_ioctl`: handle `CTRL_SYNC`, `GET_SECTOR_COUNT`, `GET_SECTOR_SIZE`, `GET_BLOCK_SIZE`; return `RES_PARERR` for unsupported commands
    - _Requirements: 3.1, 3.2, 3.3, 3.4, 3.5, 3.6, 9.1, 9.2, 9.3, 9.4, 9.5, 9.6_

  - [x] 5.2 Write unit tests for ESP32 diskio error handling
    - Test that uninitialized state blocks operations (returns `RES_NOTRDY`)
    - Test out-of-bounds sector detection (returns `RES_PARERR`)
    - Test unsupported ioctl commands (returns `RES_PARERR`)
    - Mock ESP-IDF SPI APIs for isolated testing
    - _Requirements: 9.1, 9.2, 9.3, 9.4, 9.5, 9.6_

- [x] 6. Refactor fileHelper.c to use pure FatFS API
  - [x] 6.1 Remove no-OS-FatFS dependencies from `fileHelper.c`
    - Remove `#include "hw_config.h"` and any references to `spi_t`, `sd_card_t`
    - Remove implementations of `sd_get_num`, `sd_get_by_num`, `spi_get_num`, `spi_get_by_num`
    - _Requirements: 5.1, 5.2_

  - [x] 6.2 Integrate `Platform_SDCard_Init` into fileHelper.c initialization flow
    - Add `#include "platform_config.h"`
    - Call `Platform_SDCard_Init()` before `f_mount()`; if it returns false, abort without calling `f_mount`
    - Ensure `MountSdCard` uses only standard FatFS API (`f_mount`, `f_chdrive`)
    - _Requirements: 5.3, 5.4, 4.1_

  - [x] 6.3 Write unit tests for refactored fileHelper.c
    - Mock FatFS API functions (`f_mount`, `f_open`, `f_close`)
    - Test that `Platform_SDCard_Init` failure aborts initialization
    - Test that `f_mount` error is logged and returns false
    - Test that no `hw_config.h` types or functions remain
    - _Requirements: 5.1, 5.2, 5.3, 5.4_

- [x] 7. Checkpoint - Ensure refactored fileHelper compiles cleanly
  - Ensure all tests pass, ask the user if questions arise.

- [x] 8. Configure CMake build system for RP2040
  - [x] 8.1 Update `src/lib/Platform/RP2040/pre-executable.cmake`
    - Define `FATFS_SRC` pointing to `src/Dependency/fatfs/source`
    - Create `fatfs` static library target with `ff.c`, `ffsystem.c`, `ffunicode.c`, and RP2040 `diskio.c`
    - Set include directories for FatFS headers and RP2040 platform headers
    - _Requirements: 6.2, 1.3, 1.4, 1.5_

  - [x] 8.2 Update `src/lib/Platform/RP2040/pos-executable.cmake`
    - Link `fatfs` library to the executable target (replacing old `FatFs_SPI` reference)
    - Link Pico SDK `hardware_spi` and `hardware_gpio` libraries
    - _Requirements: 6.3, 6.4_

- [x] 9. Configure CMake build system for ESP32
  - [x] 9.1 Create `src/lib/Platform/ESP32/pre-executable.cmake`
    - Define `FATFS_SRC` pointing to `src/Dependency/fatfs/source`
    - Create `fatfs` static library target with `ff.c`, `ffsystem.c`, `ffunicode.c`, and ESP32 `diskio.c`
    - Set include directories for FatFS headers and ESP32 platform headers
    - Include ESP-IDF CMake integration from `ESP_IDF_PATH`
    - _Requirements: 7.3, 7.4, 7.6_

  - [x] 9.2 Create `src/lib/Platform/ESP32/pos-executable.cmake`
    - Link `fatfs` library to the executable target
    - Link ESP-IDF SPI and GPIO driver components
    - _Requirements: 7.3_

  - [x] 9.3 Update root `CMakeLists.txt` for multi-platform support
    - Add `PLATFORM_NAME` variable with default `RP2040`
    - Add `ESP_IDF_PATH` variable with default `~/esp-idf`
    - Include platform-specific `pre-executable.cmake` and `pos-executable.cmake` using `${PLATFORM_NAME}`
    - Conditionally apply RP2040-specific SDK calls (`pico_sdk_init`, `pico_add_extra_outputs`)
    - _Requirements: 7.1, 7.2, 7.5, 7.7_

- [x] 10. Implement Platform_SDCard_Init for each platform
  - [x] 10.1 Implement `Platform_SDCard_Init` in RP2040 (new file or within diskio.c)
    - Initialize SPI peripheral using pin constants from `platform_config.h`
    - Configure CS GPIO as output, set high (deselected)
    - Set SPI baudrate to `SD_SPI_BAUDRATE`
    - Return true on success, false on failure
    - _Requirements: 4.4, 8.1, 8.4_

  - [x] 10.2 Implement `Platform_SDCard_Init` in ESP32 (new file or within diskio.c)
    - Initialize SPI bus using pin constants from `platform_config.h` via `spi_bus_initialize`
    - Configure CS GPIO
    - Set SPI clock to `SD_SPI_BAUDRATE`
    - Return true on success, false on failure
    - _Requirements: 4.4, 8.2, 8.4_

- [x] 11. Integration and wiring
  - [x] 11.1 Verify RP2040 build produces `.uf2` binary
    - Run CMake configure and build with `PLATFORM_NAME=RP2040`
    - Verify no link errors and `.uf2` file is generated
    - Verify no references to `no-OS-FatFS` remain in compiled output
    - _Requirements: 6.4, 1.2, 4.2_

  - [x] 11.2 Verify ESP32 build compiles successfully
    - Run CMake configure and build with `PLATFORM_NAME=ESP32` and `ESP_IDF_PATH` set
    - Verify no link errors
    - Verify platform headers (`driver/spi_master.h`) are only included within `src/lib/Platform/ESP32/`
    - _Requirements: 7.4, 7.6, 7.7, 4.2, 4.3_

  - [x] 11.3 Add CTest smoke tests for project structure validation
    - Add test verifying `src/Dependency/fatfs/source/ff.h` exists
    - Add test verifying no platform headers are included outside `src/lib/Platform/`
    - _Requirements: 1.2, 1.5, 4.3_

- [x] 12. Final checkpoint - Ensure all tests pass
  - Ensure all tests pass, ask the user if questions arise.

## Notes

- Tasks marked with `*` are optional and can be skipped for faster MVP
- Each task references specific requirements for traceability
- Checkpoints ensure incremental validation
- Property tests validate universal correctness properties from the design document (Properties 1-5) using the `theft` C PBT library
- Unit tests validate specific examples and edge cases
- The project uses C11 for all implementation code
- SPI mock injection via function pointers enables isolated testing of diskio logic
- ESP32 build uses ESP-IDF as a pure CMake dependency (no `idf.py` or Python required)

## Task Dependency Graph

```json
{
  "waves": [
    { "id": 0, "tasks": ["1.1", "1.2"] },
    { "id": 1, "tasks": ["1.3", "2.1", "2.2"] },
    { "id": 2, "tasks": ["3.1", "5.1"] },
    { "id": 3, "tasks": ["3.2", "3.3", "3.4", "3.5", "3.6", "5.2"] },
    { "id": 4, "tasks": ["6.1", "10.1", "10.2"] },
    { "id": 5, "tasks": ["6.2"] },
    { "id": 6, "tasks": ["6.3", "8.1", "9.1"] },
    { "id": 7, "tasks": ["8.2", "9.2", "9.3"] },
    { "id": 8, "tasks": ["11.1", "11.2"] },
    { "id": 9, "tasks": ["11.3"] }
  ]
}
```
