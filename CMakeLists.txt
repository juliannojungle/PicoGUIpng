# version requirements
cmake_minimum_required(VERSION 3.13)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 14)

# initialization
include(${CMAKE_SOURCE_DIR}/src/Dependency/pico_sdk_import.cmake)

# project definition
project(picoguipng)

pico_sdk_init()

include(${CMAKE_SOURCE_DIR}/src/pre-executable-picoguipng.cmake)

add_executable(${PROJECT_NAME} src/main.cpp)

include(${CMAKE_SOURCE_DIR}/src/pos-executable-picoguipng.cmake)

pico_enable_stdio_usb(${PROJECT_NAME} 1) # enable usb output
pico_enable_stdio_uart(${PROJECT_NAME} 0) # disable uart output
pico_add_extra_outputs(${PROJECT_NAME}) # create map/bin/hex/uf2 files in addition to elf