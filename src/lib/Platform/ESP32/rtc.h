#ifndef _RTC_H_
#define _RTC_H_

#include <time.h>
#include <sys/time.h>

/**
 * Initialize the timekeeping system.
 * Sets a default date/time (2025-01-01 00:00:00) so that
 * get_fattime() returns something reasonable even without
 * NTP or an external RTC.
 */
static inline void time_init(void) {
    struct timeval tv = {
        .tv_sec = 1735689600, // 2025-01-01 00:00:00 UTC
        .tv_usec = 0
    };
    settimeofday(&tv, NULL);
}

/**
 * Provide timestamp to FatFS.
 * Called by FatFS when FF_FS_NORTC == 0.
 * Returns packed date/time in FAT format.
 *
 * NOTE: Not static — FatFS declares get_fattime() as extern in ff.h.
 */
#include "ff.h"
DWORD get_fattime(void) {
    time_t now;
    struct tm timeinfo;

    time(&now);
    localtime_r(&now, &timeinfo);

    return ((DWORD)(timeinfo.tm_year - 80) << 25)
         | ((DWORD)(timeinfo.tm_mon + 1) << 21)
         | ((DWORD)timeinfo.tm_mday << 16)
         | ((DWORD)timeinfo.tm_hour << 11)
         | ((DWORD)timeinfo.tm_min << 5)
         | ((DWORD)(timeinfo.tm_sec / 2));
}

#endif /* _RTC_H_ */
