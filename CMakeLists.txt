# version requirements
cmake_minimum_required(VERSION 3.13)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 14)

# Seleção de plataforma
if(NOT DEFINED PLATFORM_NAME)
    set(PLATFORM_NAME "RP2040")
endif()

# SDK paths por plataforma
if(PLATFORM_NAME STREQUAL "RP2040")
    if(NOT DEFINED PICO_SDK_PATH)
        set(PICO_SDK_PATH ~/pico-sdk)
    endif()
    include(${CMAKE_SOURCE_DIR}/src/Dependency/pico_sdk_import.cmake)
elseif(PLATFORM_NAME STREQUAL "ESP32")
    if(NOT DEFINED ESP_IDF_PATH)
        set(ESP_IDF_PATH ~/esp-idf)
    endif()
endif()

# project definition
project(picoguipng)

# Inicialização específica da plataforma
if(PLATFORM_NAME STREQUAL "RP2040")
    pico_sdk_init()
endif()

# Incluir fragmentos de plataforma
include(${CMAKE_SOURCE_DIR}/src/lib/Platform/${PLATFORM_NAME}/pre-executable.cmake)

add_executable(${PROJECT_NAME} src/sample.c)

include(${CMAKE_SOURCE_DIR}/src/lib/Platform/${PLATFORM_NAME}/pos-executable.cmake)

# Extras do RP2040 (somente se aplicável)
if(PLATFORM_NAME STREQUAL "RP2040")
    pico_enable_stdio_usb(${PROJECT_NAME} 1) # enable usb output
    pico_enable_stdio_uart(${PROJECT_NAME} 0) # disable uart output
    pico_add_extra_outputs(${PROJECT_NAME}) # create map/bin/hex/uf2 files in addition to elf
endif()
