# gui.ll (GUI Low Level)

> ⚠️ **This project is under active development. The documentation is growing along the project as it's a work-in-progress.**

A lightweight, bare-metal GUI library for embedded systems. Draws visual elements directly on a GC9A01 round LCD (240×240) and renders PNG images streamed from an SD card — all with minimal memory footprint, no OS, no heap allocations in the critical path.

Currently supports **[RP2040-LCD-1.28](https://www.waveshare.com/wiki/RP2040-LCD-1.28)** (RP2040 Raspberry Pi Pico) and **[ESP32-S3-LCD-1.28](https://www.waveshare.com/wiki/ESP32-S3-LCD-1.28)** (ESP32 S3) from a single codebase.

---

## ✨ Features

- 🎨 **Direct LCD rendering** — pixels, lines, circles, rectangles, and text written straight to the display without a framebuffer
- 🖼️ **PNG from SD card** — PNG files decoded on-the-fly from an SD card and streamed directly to the LCD, row by row
- 🔌 **Multi-platform** — same application code compiles for RP2040 and ESP32; platform differences are fully encapsulated in the HAL layer
- ⚡ **Bare-metal** — no RTOS, no dynamic memory allocator required, runs on the metal
- 📦 **Self-contained** — zlib, libpng, and FatFS are bundled as submodules; no package manager needed
- 🔧 **CMake-native** — integrates cleanly as a CMake component into any parent project

---

## 🖥️ Supported Platforms

| Platform | Toolchain | Output |
|---|---|---|
| RP2040 (Raspberry Pi Pico) | arm-none-eabi-gcc + Pico SDK | `.uf2` |
| ESP32 | xtensa-esp32-elf-gcc + ESP-IDF | `.bin` |

---

## 🏗️ Architecture

```
gui.ll/
├── src/
│   ├── Sample.c                    # Entry point — app_entry()
│   ├── lib/
│   │   ├── Helper/
│   │   │   ├── FileHelper.c        # SD card mount / open / close (FatFS)
│   │   │   └── PNGHelper.c         # PNG decode + LCD streaming (libpng)
│   │   ├── Platform/
│   │   │   ├── RP2040/             # HAL, SPI, DiskIO, RTC (Pico SDK)
│   │   │   └── ESP32/              # HAL, SPI, DiskIO, RTC (ESP-IDF)
│   │   ├── Driver/GC9A01/          # LCD driver
│   │   ├── LCD/                    # Display command layer
│   │   ├── GUI/                    # Paint / drawing primitives
│   │   └── Fonts/                  # Bitmap font data
│   └── Dependency/
│       ├── fatfs/                  # ChaN FatFS
│       ├── libpng/                 # libpng
│       └── zlib/                   # zlib (libpng dependency)
└── Toolchain/
    ├── RP2040/Setup.sh
    └── ESP32/Setup.sh
```

### Platform abstraction

All hardware-specific code lives under `src/lib/Platform/<PLATFORM>/`. The application layer (`FileHelper`, `PNGHelper`, `Sample.c`) calls abstract functions — `DigitalWrite`, `SPIWriteByte`, `Delay`, etc. — defined in each platform's `HAL.c`. CMake `include_directories` points to the active platform folder at build time, so `#include "HAL.c"` resolves to the correct implementation with no `#ifdef` scattered through application code.

### Entry point

```c
void app_entry(void) { /* your application code */ }

#ifdef ESP_PLATFORM
void app_main(void) { app_entry(); }  // ESP-IDF entry
#else
int main(void) { app_entry(); return 0; }  // RP2040 / standard entry
#endif
```

---

## 🎨 Drawing Primitives

The `GUI_Paint` module writes directly to the LCD without requiring a full framebuffer in RAM.

```c
// Canvas setup
Paint_NewImage(buffer, 240, 240, ROTATE_0, WHITE);
Paint_Clear(BLACK);

// Geometry
Paint_DrawPoint(120, 120, RED, DOT_PIXEL_2X2, DOT_FILL_AROUND);
Paint_DrawLine(0, 0, 240, 240, WHITE, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
Paint_DrawRectangle(10, 10, 100, 80, BLUE, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
Paint_DrawCircle(120, 120, 60, GREEN, DOT_PIXEL_1X1, DRAW_FILL_FULL);

// Text
Paint_DrawString_EN(20, 20, "Hello!", &Font16, WHITE, BLACK);
Paint_DrawNum(20, 50, 3.14, &Font12, 2, YELLOW, BLACK);
Paint_DrawTime(10, 200, &sPaint_time, &Font12, WHITE, BLACK);
```

Available colors: `WHITE`, `BLACK`, `RED`, `GREEN`, `BLUE`, `YELLOW`, `CYAN`, `MAGENTA`, `GRAY`, and more.
Dot sizes: `DOT_PIXEL_1X1` through `DOT_PIXEL_8X8`.
Line styles: `LINE_STYLE_SOLID`, `LINE_STYLE_DOTTED`.
Fill modes: `DRAW_FILL_EMPTY`, `DRAW_FILL_FULL`.

---

## 🖼️ PNG Rendering from SD Card

PNG files are decoded with libpng and written to the display row by row. Memory usage is bounded to a single decoded row at a time — the full image never needs to fit in RAM.

```c
sd_card_t *sdcard = sd_get_by_num(0);
FIL file;

if (MountSdCard(sdcard)
    && SelectActiveDrive(sdcard)
    && OpenFile(sdcard, &file, "image.png")) {
    DisplayPng(&file);
    CloseFile(&file);
}

UnMountSdCard(sdcard);
```

---

## 📐 Using gui.ll in Your Own Project

The `CMakeLists.txt` at the root is a functional standalone example. To integrate gui.ll as a component in your own CMake project, include the platform-specific fragments directly:

```cmake
set(PLATFORM_NAME "RP2040")  # or "ESP32"

# RP2040 only — include before add_executable
include(path/to/gui.ll/src/lib/Platform/RP2040/PreExecutable.cmake)

add_executable(my_app src/main.c)

# RP2040 only — include after add_executable
include(path/to/gui.ll/src/lib/Platform/RP2040/PostExecutable.cmake)
```

For ESP32, set `EXTRA_COMPONENT_DIRS` to point at `src/lib/Platform/ESP32` before calling `project()`:

```cmake
set(EXTRA_COMPONENT_DIRS "path/to/gui.ll/src/lib/Platform/ESP32")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(my_app)
```

Your application entry point mirrors `Sample.c` — implement `app_entry()` and let the platform macro route `main` or `app_main` automatically.

---

## 🚀 Getting Started

### 1. Clone with submodules

```bash
git clone --recurse-submodules https://github.com/juliannojungle/gui.ll.git
```

This single command clones the repository and all bundled dependencies (FatFS, libpng, zlib) in one step. No separate dependency installation needed.

### 2. Set up the toolchain

Before building for the first time, install the required toolchain for your target platform. The recommended way is via the built-in tasks.

**In Kiro or VS Code** — open the Command Palette (`Ctrl+Shift+P`) and select **Tasks: Run Task**, then choose:

- `Setup: RP2040 toolchain in WSL` — installs arm-none-eabi-gcc and Pico SDK
- `Setup: ESP32 toolchain in WSL` — installs ESP-IDF, xtensa toolchain, Rust, and espflash

These scripts are idempotent — safe to run again if you need to repair an installation.

### 3. Build

Once the toolchain is ready, build using the tasks:

**In Kiro or VS Code** — open the Command Palette (`Ctrl+Shift+P`) → **Tasks: Run Task**:

| Task | Description |
|---|---|
| `Build: Full RP2040` | Clean CMake configure + make for RP2040 |
| `Build: Full ESP32` | Clean idf.py build for ESP32 |
| `Build: Incremental` | Auto-detects platform, runs `make` or `ninja` |
| `Copy UF2 to Windows` | Copies `.uf2` to `C:\temp` for drag-and-drop flashing |

Alternatively, trigger the default build task directly with `Ctrl+Shift+B` (runs **Build: Incremental**).

---

## 💡 IDE

The preferred IDE is **[Kiro](https://kiro.dev)**, which provides agent-assisted development with project-aware context via `AGENTS.md`. The project also works fully with **VS Code** — all tasks and settings are configured in `.vscode/`.

---

## 📋 Requirements

- **Host**: Windows + WSL (Ubuntu) — code edited on Windows, compiled inside WSL
- **RP2040**: arm-none-eabi-gcc, Pico SDK (installed via `Setup: RP2040 toolchain`)
- **ESP32**: ESP-IDF ≥ 5.x, xtensa toolchain, espflash (installed via `Setup: ESP32 toolchain`)
- **CMake** ≥ 3.16

---

## 📄 License

See [LICENSE](LICENSE).
