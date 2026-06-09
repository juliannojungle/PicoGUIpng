#ifndef _RTC_H_
#define _RTC_H_

#include "pico/stdlib.h"
#include "hardware/rtc.h"
#include "pico/util/datetime.h"

/**
 * Initialize the RTC hardware.
 * Sets a default date/time (2025-01-01 00:00:00) so that
 * get_fattime() returns something reasonable even without
 * an external time source.
 */
static inline void time_init(void) {
    datetime_t t = {
        .year  = 2025,
        .month = 1,
        .day   = 1,
        .dotw  = 3, // Wednesday
        .hour  = 0,
        .min   = 0,
        .sec   = 0
    };
    rtc_init();
    rtc_set_datetime(&t);
}

/**
 * Provide timestamp to FatFS.
 * Called by FatFS when FF_FS_NORTC == 0.
 * Returns packed date/time in FAT format.
 *
 * NOTE: Implemented in rtc.h but NOT static, because FatFS
 * declares get_fattime() as an extern function in ff.h.
 * This will be compiled only once since sample.c is the
 * single translation unit that includes this header.
 */
#include "ff.h"
DWORD get_fattime(void) {
    datetime_t t;
    rtc_get_datetime(&t);

    return ((DWORD)(t.year - 1980) << 25)
         | ((DWORD)t.month << 21)
         | ((DWORD)t.day << 16)
         | ((DWORD)t.hour << 11)
         | ((DWORD)t.min << 5)
         | ((DWORD)(t.sec / 2));
}

#endif /* _RTC_H_ */
