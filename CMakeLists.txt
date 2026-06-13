# version requirements
cmake_minimum_required(VERSION 3.16)

# Seleção de plataforma
if(NOT DEFINED PLATFORM_NAME)
    set(PLATFORM_NAME "RP2040")
endif()

# Build system por plataforma
if(PLATFORM_NAME STREQUAL "ESP32")
    # ESP-IDF é o orquestrador do build.
    # EXTRA_COMPONENT_DIRS permite adicionar nosso componente sem perder os do ESP-IDF.
    set(EXTRA_COMPONENT_DIRS
        "${CMAKE_SOURCE_DIR}/src/lib/Platform/ESP32"
    )
    include($ENV{IDF_PATH}/tools/cmake/project.cmake)
    project(picoguipng)

else() # RP2040 (default)
    set(CMAKE_C_STANDARD 11)
    set(CMAKE_CXX_STANDARD 14)

    if(NOT DEFINED PICO_SDK_PATH)
        set(PICO_SDK_PATH ~/pico-sdk)
    endif()
    include(${CMAKE_SOURCE_DIR}/src/Dependency/pico_sdk_import.cmake)

    project(picoguipng)
    pico_sdk_init()

    # Incluir fragmentos de plataforma
    include(${CMAKE_SOURCE_DIR}/src/lib/Platform/RP2040/PreExecutable.cmake)

    add_executable(${PROJECT_NAME} src/Sample.c)

    include(${CMAKE_SOURCE_DIR}/src/lib/Platform/RP2040/PostExecutable.cmake)

    pico_enable_stdio_usb(${PROJECT_NAME} 1)  # enable usb output
    pico_enable_stdio_uart(${PROJECT_NAME} 0) # disable uart output
    pico_add_extra_outputs(${PROJECT_NAME})    # create map/bin/hex/uf2 files in addition to elf
endif()
