# fatfs_patch.cmake
# Applies required configuration changes to ffconf.h at configure time.
# Similar to how zlibstatic.cmake replaces the zlib CMakeLists.txt.
#
# Changes:
#   FF_FS_RPATH:  0 -> 1  (enables f_chdrive/f_chdir for multi-drive support)
#   FF_VOLUMES:   1 -> 2  (allows multiple logical drives)

set(FFCONF_PATH ${CMAKE_SOURCE_DIR}/src/Dependency/fatfs/source/ffconf.h)

file(READ ${FFCONF_PATH} FFCONF_CONTENT)

# FF_FS_RPATH: 0 -> 1
string(REGEX REPLACE
    "#define FF_FS_RPATH[\\t ]+0"
    "#define FF_FS_RPATH\t\t1"
    FFCONF_CONTENT "${FFCONF_CONTENT}"
)

# FF_VOLUMES: 1 -> 2
string(REGEX REPLACE
    "#define FF_VOLUMES[\\t ]+1"
    "#define FF_VOLUMES\t\t2"
    FFCONF_CONTENT "${FFCONF_CONTENT}"
)

file(WRITE ${FFCONF_PATH} "${FFCONF_CONTENT}")
