###### Waveshare/GUI lib for graphics (platform-agnostic portion)
set(PICO_CODE_LIB ${CMAKE_SOURCE_DIR}/src/lib)
include_directories(${PICO_CODE_LIB}/Platform/ESP32)
include_directories(${PICO_CODE_LIB}/Driver/GC9A01)
include_directories(${PICO_CODE_LIB}/Fonts)
include_directories(${PICO_CODE_LIB}/LCD)
include_directories(${PICO_CODE_LIB}/GUI)
include_directories(${PICO_CODE_LIB}/Helper)

###### zlib dependency to use libpng
set(LIB_ZLIB ${CMAKE_SOURCE_DIR}/src/Dependency/zlib)
file(REMOVE ${LIB_ZLIB}/CMakeLists.txt)
file(COPY ${CMAKE_SOURCE_DIR}/src/Dependency/zlibstatic.cmake DESTINATION ${LIB_ZLIB})
file(RENAME ${LIB_ZLIB}/zlibstatic.cmake ${LIB_ZLIB}/CMakeLists.txt)
add_subdirectory(${LIB_ZLIB} zlib_build)
target_link_libraries(${PROJECT_NAME} PUBLIC zlibstatic)
set(LIB_ZCONF ${CMAKE_BINARY_DIR}/zlib_build)
include_directories(${LIB_ZLIB} ${LIB_ZCONF})
set(PNG_BUILD_ZLIB ON CACHE BOOL "" FORCE)
set(SKIP_INSTALL_ALL ON)
set(ZLIB_LIBRARIES zlibstatic)
set(ZLIB_INCLUDE_DIRS ${LIB_ZLIB} ${LIB_ZCONF})

###### libpng dependencies to handle png files
set(PNG_LIB ${CMAKE_SOURCE_DIR}/src/Dependency/libpng)
set(PNG_SHARED OFF CACHE BOOL "")
set(PNG_STATIC ON CACHE BOOL "")
set(PNG_FRAMEWORK OFF CACHE BOOL "")
set(PNG_EXECUTABLES OFF CACHE BOOL "")
set(PNG_TESTS OFF CACHE BOOL "")
set(PNG_DEBUG OFF CACHE BOOL "")
include_directories(${PNG_LIB})
add_subdirectory(${PNG_LIB})
configure_file(${PNG_LIB}/scripts/pnglibconf.h.prebuilt ${PNG_LIB}/pnglibconf.h)

###### Link libraries for ESP32
target_link_libraries(${PROJECT_NAME}
    PUBLIC
    fatfs
    png_static)

# Note: ESP-IDF SPI and GPIO driver components are linked via idf_component_register
# or directly via the ESP-IDF build system when using idf.cmake
