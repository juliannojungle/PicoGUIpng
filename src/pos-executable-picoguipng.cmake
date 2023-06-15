# target configuration
set(CMAKE_C_COMPILER /usr/bin/arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER /usr/bin/arm-none-eabi-g++)

###### Waveshare Pico_code/c/lib/GUI for graphics
set(PICO_CODE_LIB ${CMAKE_SOURCE_DIR}/src/lib)
add_subdirectory(${PICO_CODE_LIB}/Config)
add_subdirectory(${PICO_CODE_LIB}/GUI)
include_directories(${PICO_CODE_LIB}/Config)
include_directories(${PICO_CODE_LIB}/Fonts)
include_directories(${PICO_CODE_LIB}/LCD)
include_directories(${PICO_CODE_LIB}/GUI)
###### END Waveshare Pico_code/c/lib/GUI for graphics

###### zlib dependency to use libpng
set(LIB_ZLIB ${CMAKE_SOURCE_DIR}/src/Dependency/zlib)
# set a custom build script for zlibstatic, skipping the rest
file(REMOVE ${LIB_ZLIB}/CMakeLists.txt)
file(COPY ${CMAKE_SOURCE_DIR}/src/Dependency/zlibstatic.cmake DESTINATION ${LIB_ZLIB})
file(RENAME ${LIB_ZLIB}/zlibstatic.cmake ${LIB_ZLIB}/CMakeLists.txt)
# build zlibstatic
add_subdirectory(${LIB_ZLIB} zlib_build)
target_link_libraries(${PROJECT_NAME} PUBLIC zlibstatic)
# set libpng to use zlibstatic
set(LIB_ZCONF ${CMAKE_BINARY_DIR}/zlib_build)
include_directories(${LIB_ZLIB} ${LIB_ZCONF})
set(PNG_BUILD_ZLIB ON CACHE BOOL "" FORCE)
set(SKIP_INSTALL_ALL ON)
set(ZLIB_LIBRARIES zlibstatic)
set(ZLIB_INCLUDE_DIRS ${LIB_ZLIB} ${LIB_ZCONF})
###### END zlib dependency to use libpng

###### libpng dependencies to handle png files
set(PNG_LIB ${CMAKE_SOURCE_DIR}/src/Dependency/libpng)
# setup libpng to only build png_static, skipping the rest
set(PNG_SHARED OFF CACHE BOOL "")
set(PNG_STATIC ON CACHE BOOL "")
set(PNG_FRAMEWORK OFF CACHE BOOL "")
set(PNG_EXECUTABLES OFF CACHE BOOL "")
set(PNG_TESTS OFF CACHE BOOL "")
set(PNG_DEBUG OFF CACHE BOOL "")
include_directories(${PNG_LIB})
add_subdirectory(${PNG_LIB})
configure_file(${PNG_LIB}/scripts/pnglibconf.h.prebuilt ${PNG_LIB}/pnglibconf.h)
###### END libpng dependencies to handle png files

target_link_libraries(${PROJECT_NAME}
    PUBLIC
    pico_stdlib
    FatFs_SPI
    GUI
    Config
    png_static)
