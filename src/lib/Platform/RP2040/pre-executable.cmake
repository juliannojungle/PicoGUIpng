# Compile FatFS puro (ChaN)
set(FATFS_SRC ${CMAKE_SOURCE_DIR}/src/Dependency/fatfs/source)

add_library(fatfs STATIC
    ${FATFS_SRC}/ff.c
    ${FATFS_SRC}/ffsystem.c
    ${FATFS_SRC}/ffunicode.c
    ${CMAKE_SOURCE_DIR}/src/lib/Platform/RP2040/diskio.c
)

target_include_directories(fatfs PUBLIC
    ${FATFS_SRC}
)

target_include_directories(fatfs PRIVATE
    ${CMAKE_SOURCE_DIR}/src/lib/Platform/RP2040
)

# Link Pico SDK hardware libraries so diskio.c can find hardware/spi.h and hardware/gpio.h
target_link_libraries(fatfs PRIVATE hardware_spi hardware_gpio)
