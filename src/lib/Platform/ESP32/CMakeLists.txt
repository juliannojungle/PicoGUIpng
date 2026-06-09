set(PROJ_ROOT ${CMAKE_SOURCE_DIR})
set(FATFS_SRC ${PROJ_ROOT}/src/Dependency/fatfs/source)
set(ZLIB_SRC ${PROJ_ROOT}/src/Dependency/zlib)
set(PNG_SRC ${PROJ_ROOT}/src/Dependency/libpng)
set(PLATFORM_SRC ${PROJ_ROOT}/src/lib/Platform/ESP32)

idf_component_register(
    SRCS
        "${PROJ_ROOT}/src/sample.c"
        "${FATFS_SRC}/ff.c"
        "${FATFS_SRC}/ffsystem.c"
        "${FATFS_SRC}/ffunicode.c"
        "${PLATFORM_SRC}/diskio.c"
        # zlib
        "${ZLIB_SRC}/adler32.c"
        "${ZLIB_SRC}/compress.c"
        "${ZLIB_SRC}/crc32.c"
        "${ZLIB_SRC}/deflate.c"
        "${ZLIB_SRC}/inflate.c"
        "${ZLIB_SRC}/inftrees.c"
        "${ZLIB_SRC}/inffast.c"
        "${ZLIB_SRC}/trees.c"
        "${ZLIB_SRC}/uncompr.c"
        "${ZLIB_SRC}/zutil.c"
        # libpng
        "${PNG_SRC}/png.c"
        "${PNG_SRC}/pngerror.c"
        "${PNG_SRC}/pngget.c"
        "${PNG_SRC}/pngmem.c"
        "${PNG_SRC}/pngpread.c"
        "${PNG_SRC}/pngread.c"
        "${PNG_SRC}/pngrio.c"
        "${PNG_SRC}/pngrtran.c"
        "${PNG_SRC}/pngrutil.c"
        "${PNG_SRC}/pngset.c"
        "${PNG_SRC}/pngtrans.c"
        "${PNG_SRC}/pngwio.c"
        "${PNG_SRC}/pngwrite.c"
        "${PNG_SRC}/pngwtran.c"
        "${PNG_SRC}/pngwutil.c"
    INCLUDE_DIRS
        "${FATFS_SRC}"
        "${ZLIB_SRC}"
        "${PNG_SRC}"
        "${PLATFORM_SRC}"
        "${PROJ_ROOT}/src/lib/Driver/GC9A01"
        "${PROJ_ROOT}/src/lib/Fonts"
        "${PROJ_ROOT}/src/lib/LCD"
        "${PROJ_ROOT}/src/lib/GUI"
        "${PROJ_ROOT}/src/lib/Helper"
    REQUIRES
        driver
        esp_system
)

# libpng precisa do pnglibconf.h gerado
configure_file(
    ${PNG_SRC}/scripts/pnglibconf.h.prebuilt
    ${PNG_SRC}/pnglibconf.h
    COPYONLY
)

# Desabilitar -Werror para dependências de terceiros (libpng, zlib)
# que geram warnings com o toolchain xtensa do ESP-IDF.
# Usa target_compile_options no lib do componente para garantir que
# -Wno-error vem depois de -Werror=all na linha de compilação.
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-error=maybe-uninitialized
    -Wno-error=unused-but-set-variable
)
