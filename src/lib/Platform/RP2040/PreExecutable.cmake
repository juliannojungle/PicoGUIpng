# Compile FatFS (ChaN) as static library
set(FATFS_SRC ${CMAKE_SOURCE_DIR}/src/Dependency/fatfs/source)

# Apply patch to ffconf.h (FF_FS_RPATH=1, FF_VOLUMES=2)
include(${CMAKE_SOURCE_DIR}/src/Dependency/fatfs.ffconf_patch.cmake)

add_library(fatfs STATIC
    ${FATFS_SRC}/ff.c
    ${FATFS_SRC}/ffsystem.c
    ${FATFS_SRC}/ffunicode.c
    ${CMAKE_SOURCE_DIR}/src/lib/Platform/RP2040/DiskIO.c
)

target_include_directories(fatfs PUBLIC
    ${FATFS_SRC}
)

target_include_directories(fatfs PRIVATE
    ${CMAKE_SOURCE_DIR}/src/lib/Platform/RP2040
)

# Link Pico SDK hardware libraries so diskio.c can find hardware/spi.h and hardware/gpio.h
target_link_libraries(fatfs PRIVATE hardware_spi hardware_gpio)
