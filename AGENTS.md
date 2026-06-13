# AGENTS.md — Project Knowledge Base

## Overview

**PicoGUIpng** is a multi-platform embedded C project that reads PNG files from an SD card
and displays them on a GC9A01 round LCD (240x240). It targets **RP2040** (Raspberry Pi Pico)
and **ESP32** from a single codebase with platform-specific abstractions.

---

## Project Structure

```
PicoGUIpng/
├── CMakeLists.txt                  # Root cmake: if/else by PLATFORM_NAME
├── AGENTS.md                       # This file
├── .gitmodules                     # Submodule config (all locked with update=none)
├── .gitignore                      # build/, sdkconfig
│
├── Toolchain/
│   ├── RP2040/Setup.sh             # Installs arm-none-eabi-gcc, pico-sdk
│   └── ESP32/Setup.sh              # Installs ESP-IDF, xtensa toolchain, Rust, espflash
│
├── src/
│   ├── Sample.c                    # Entry point (app_entry → main or app_main)
│   │
│   ├── lib/
│   │   ├── Helper/
│   │   │   ├── FileHelper.c        # SD card mount/open/close (parametrized by sd_card_t)
│   │   │   └── PNGHelper.c         # PNG decode + LCD display via libpng
│   │   │
│   │   ├── Platform/
│   │   │   ├── RP2040/
│   │   │   │   ├── HAL.c           # HAL: GPIO, SPI, PWM, I2C (Pico SDK)
│   │   │   │   ├── HALConfig.h     # Pin definitions, Platform_SDCard_Init decl
│   │   │   │   ├── SDConfig.h      # spi_t, sd_card_t struct definitions
│   │   │   │   ├── SDHWConfig.h    # Default SPI/SD arrays + sd_get_num/sd_get_by_num
│   │   │   │   ├── RTC.h           # RTC via hardware/rtc.h + get_fattime()
│   │   │   │   ├── DiskIO.c        # FatFS disk I/O (SPI SD protocol)
│   │   │   │   ├── PreExecutable.cmake   # fatfs lib, patch inclusion
│   │   │   │   └── PostExecutable.cmake  # zlib, libpng, link libraries
│   │   │   │
│   │   │   └── ESP32/
│   │   │       ├── CMakeLists.txt   # idf_component_register (ESP-IDF component)
│   │   │       ├── HAL.c           # HAL: GPIO, SPI (ESP-IDF), LEDC PWM compat
│   │   │       ├── HALConfig.h     # Pin definitions
│   │   │       ├── SDConfig.h      # spi_t, sd_card_t struct definitions (ESP32 types)
│   │   │       ├── SDHWConfig.h    # Default SPI/SD arrays
│   │   │       ├── RTC.h           # RTC via settimeofday + get_fattime()
│   │   │       └── DiskIO.c        # FatFS disk I/O (ESP-IDF SPI master)
│   │   │
│   │   ├── Driver/GC9A01/          # LCD driver (uses HAL.c abstractions)
│   │   ├── LCD/                     # LCD commands (LCD_1in28)
│   │   ├── GUI/                     # Paint/drawing utilities
│   │   └── Fonts/                   # Font data
│   │
│   └── Dependency/
│       ├── fatfs/                   # Submodule: ChaN FatFS (DO NOT MODIFY)
│       ├── libpng/                  # Submodule: libpng (DO NOT MODIFY)
│       ├── zlib/                    # Submodule: zlib (DO NOT MODIFY)
│       ├── fatfs.ffconf_patch.cmake # Patches ffconf.h at build time
│       ├── zlibstatic.cmake         # Replaces zlib CMakeLists.txt at build time
│       └── pico_sdk_import.cmake    # Pico SDK cmake helper
│
└── .vscode/
    ├── tasks.json                   # Build/setup tasks (run via WSL)
    └── settings.json                # Git plugin config
```

---

## Architecture Principles

### 0. Naming Convention

- **PascalCase** for all project file and directory names
- **Acronyms in UPPERCASE** (HAL, RTC, PNG, SD, IO, HW)
- **Exceptions** (not renamed):
  - Project meta files: `AGENTS.md`, `.gitmodules`, `.gitignore`, `CMakeLists.txt`
  - IDE directories/files: `.kiro/`, `.vscode/`, `tasks.json`, `settings.json`
  - Source root: `src/`, `lib/`
  - Anything inside `src/Dependency/` (submodule contents are untouched)

### 1. Platform Abstraction

All platform-specific code lives under `src/lib/Platform/<PLATFORM_NAME>/`.
The helpers and drivers are platform-agnostic — they call abstract functions
(`DigitalWrite`, `SPIWriteByte`, `Delay`, etc.) defined in each platform's `HAL.c`.

Include resolution works via cmake `include_directories` pointing to the active
platform folder. A single `#include "HAL.c"` in `Sample.c` pulls the right one.

### 2. Multi-SD-Card Support

The design supports multiple SPI buses and multiple SD cards. Each platform defines:
- `SDConfig.h` — struct definitions (`spi_t`, `sd_card_t`, `sd_spi_if_t`)
- `SDHWConfig.h` — default hardware arrays (`spis[]`, `sd_cards[]`) and
  `sd_get_num()` / `sd_get_by_num()` implementations

The `FileHelper.c` functions are parametrized by `sd_card_t*`.
Users expand the arrays in `SDHWConfig.h` to add more SD cards.

### 3. Entry Point

```c
void app_entry(void) { /* main logic */ }

#ifdef ESP_PLATFORM
void app_main(void) { app_entry(); }  // ESP-IDF entry
#else
int main(void) { app_entry(); return 0; }  // RP2040 entry
#endif
```

### 4. RTC / Timestamps

Each platform provides `RTC.h` with:
- `time_init()` — initializes timekeeping (hardware RTC on RP2040, settimeofday on ESP32)
- `get_fattime()` — provides FAT timestamps to FatFS (required when `FF_FS_NORTC == 0`)

`get_fattime()` must NOT be `static` — FatFS declares it as extern in `ff.h`.

---

## Build System

### RP2040
- Uses **cmake + make** directly
- Pico SDK is included via `pico_sdk_import.cmake`
- FatFS compiled as static library in `PreExecutable.cmake`
- zlib/libpng compiled as static libraries in `PostExecutable.cmake`
- Generates `.uf2` for drag-and-drop flashing

### ESP32
- Uses **idf.py** (ESP-IDF build system) which internally calls cmake + ninja
- `EXTRA_COMPONENT_DIRS` points to `src/lib/Platform/ESP32` (avoids needing a `main/` folder)
- Component registered via `idf_component_register()` with all sources and includes
- Requires `source ~/esp-idf/export.sh` before build (sets toolchain in PATH)
- Third-party code (libpng, zlib) compiled with `-Wno-error=maybe-uninitialized` to suppress
  warnings promoted to errors by ESP-IDF's strict `-Werror=all`

### Incremental Build (both platforms)

The "Build: Incremental" task auto-detects the platform:
```bash
cd ~/PicoGUIpng/build && if [ -f build.ninja ]; then
    source ~/esp-idf/export.sh 2>/dev/null && ninja
else
    make
fi
```

---

## Dependency Management — CRITICAL RULES

### DO NOT modify submodule contents directly

All dependencies are git submodules with `update = none` and `ignore = all`:
- `src/Dependency/fatfs` — ChaN FatFS
- `src/Dependency/libpng` — libpng
- `src/Dependency/zlib` — zlib

**If a dependency needs configuration changes, use cmake patches applied at build time.**

### Existing patches:

| File | What it does |
|------|-------------|
| `fatfs.ffconf_patch.cmake` | Sets `FF_FS_RPATH=1` and `FF_VOLUMES=2` in `ffconf.h` |
| `zlibstatic.cmake` | Replaces zlib's `CMakeLists.txt` with a minimal static-only build |
| `configure_file(pnglibconf.h.prebuilt → pnglibconf.h)` | Generates required libpng config header |

### Rationale
- Submodules stay at pinned commits (shallow clones)
- `update = none` prevents accidental updates via `git submodule update --remote`
- Patches are idempotent (safe to re-run, use regex replace)
- Anyone cloning the repo gets a working build without manual intervention

---

## Development Environment

### Host: Windows + WSL (Ubuntu)

- Code edited on Windows (VS Code / Kiro) via `\\wsl.localhost\Ubuntu\...`
- Compilation happens inside WSL
- Tasks use `wsl -e bash -c "..."` to invoke Linux commands from Windows

### Toolchain Setup

Run the setup tasks (idempotent — safe to re-run):
- **"Setup: RP2040 toolchain in WSL"** → `Toolchain/RP2040/Setup.sh`
- **"Setup: ESP32 toolchain in WSL"** → `Toolchain/ESP32/Setup.sh`

Setup scripts check for existing installations before downloading.
ESP32 setup installs: apt deps, ESP-IDF, idf_tools (xtensa, gdb, openocd),
Python env, Rust, and espflash.

### Known Issues

- **Git pager in WSL**: The git config may have a pager that blocks non-interactive
  sessions. Use `GIT_PAGER=cat` or `git -c core.pager=` when scripting.
- **cmd.exe escaping**: Complex bash commands with pipes, parentheses, or nested quotes
  fail when passed inline via `wsl -e bash -c "..."`. Solution: put scripts in `.sh` files
  and call them via `wsl -e bash -c "~/path/to/Script.sh"`.
- **Pico SDK `picotool`**: Auto-downloaded by Pico SDK 2.x if not installed globally.
  Takes time on first full build. Can be pre-installed for faster builds.

### Language

All code comments, documentation, and commit messages must be in **English**.
The AGENTS.md itself is the reference for this rule.

---

## VS Code / Kiro Settings

`.vscode/settings.json`:
```json
{
    "git.detectSubmodules": false,
    "git.autoRepositoryDetection": "openEditors"
}
```
This prevents the git plugin from showing false "modified" files in submodules
(caused by line-ending differences between Windows and Linux).

---

## Build Tasks Reference

| Task | What it does |
|------|-------------|
| Build: Full RP2040 | Clean build with cmake + make |
| Build: Full ESP32 | Clean build with idf.py |
| Build: Incremental | Detects platform, runs make or ninja |
| Copy UF2 to Windows | Copies .uf2 to C:\temp for flashing |
| Setup: RP2040 toolchain | Installs arm toolchain + pico-sdk |
| Setup: ESP32 toolchain | Installs ESP-IDF + tools + Rust + espflash |

---

## Current Status (as of last session)

- **RP2040**: Builds successfully, generates `.uf2`, zero errors and warnings.
- **ESP32**: Builds successfully, generates `.bin`, zero errors and warnings.
  - libpng false-positive warnings suppressed via `set_source_files_properties` in ESP32 `CMakeLists.txt`

---

## Design Decisions Log

1. **FatFS over no-OS-FatFS**: Switched from no-OS-FatFS-SD-SPI-RPi-Pico to pure ChaN FatFS
   for portability across platforms. The `sd_card_t` parametrization was preserved in
   `SDConfig.h` / `SDHWConfig.h` to maintain multi-card support.

2. **EXTRA_COMPONENT_DIRS over COMPONENT_DIRS**: Using `EXTRA_COMPONENT_DIRS` in ESP32 cmake
   adds our component alongside ESP-IDF's built-in components (driver, esp_system, etc.).
   `COMPONENT_DIRS` would replace all defaults and break things.

3. **No `main/` directory for ESP32**: The ESP-IDF convention of requiring a `main/` folder
   is bypassed via `EXTRA_COMPONENT_DIRS` pointing directly to the platform folder.

4. **Setup scripts in `Toolchain/`**: Avoids cmd.exe escaping issues with complex inline
   bash commands in tasks.json. Scripts are idempotent and self-documenting.

5. **`#include "HAL.c"` pattern**: The project uses a single-translation-unit approach
   where .c files are included directly (not compiled separately). This is intentional —
   do not refactor into separate compilation units unless explicitly requested.
