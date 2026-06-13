# Compile FatFS (ChaN) as static library
set(FATFS_SRC ${CMAKE_SOURCE_DIR}/src/Dependency/fatfs/source)

add_library(fatfs STATIC
    ${FATFS_SRC}/ff.c
    ${FATFS_SRC}/ffsystem.c
    ${FATFS_SRC}/ffunicode.c
    ${CMAKE_SOURCE_DIR}/src/lib/Platform/ESP32/diskio.c
)

target_include_directories(fatfs PUBLIC
    ${FATFS_SRC}
)

target_include_directories(fatfs PRIVATE
    ${CMAKE_SOURCE_DIR}/src/lib/Platform/ESP32
)

# ESP-IDF CMake integration
if(DEFINED ESP_IDF_PATH)
    list(APPEND CMAKE_MODULE_PATH ${ESP_IDF_PATH}/tools/cmake)
    # Include ESP-IDF component management if available
    if(EXISTS ${ESP_IDF_PATH}/tools/cmake/idf.cmake)
        include(${ESP_IDF_PATH}/tools/cmake/idf.cmake)
    endif()
endif()
