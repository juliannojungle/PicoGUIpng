# fatfs.ffconf_patch.cmake
# Applies required configuration changes to ffconf.h at configure time.
# Similar to how zlibstatic.cmake replaces the zlib CMakeLists.txt.
#
# Changes:
#   FF_FS_RPATH:  0 -> 1  (enables f_chdrive/f_chdir for multi-drive support)
#   FF_VOLUMES:   1 -> 2  (allows multiple logical drives)

set(FFCONF_PATH ${CMAKE_CURRENT_LIST_DIR}/fatfs/source/ffconf.h)

file(READ ${FFCONF_PATH} FFCONF_CONTENT)

# FF_FS_RPATH: 0 -> 1
# Match "#define FF_FS_RPATH" followed by whitespace and "0"
string(REGEX REPLACE
    "(#define FF_FS_RPATH[^0-9]+)0"
    "\\11"
    FFCONF_CONTENT "${FFCONF_CONTENT}"
)

# FF_VOLUMES: 1 -> 2
# Match "#define FF_VOLUMES" followed by whitespace and "1"
string(REGEX REPLACE
    "(#define FF_VOLUMES[^0-9]+)1"
    "\\12"
    FFCONF_CONTENT "${FFCONF_CONTENT}"
)

file(WRITE ${FFCONF_PATH} "${FFCONF_CONTENT}")
