#include "HAL.c"
#include "RTC.h" // rtc for file's timestamp.

#include "FileHelper.c"
#include "PNGHelper.c"

void app_entry(void) {
    STDIOInitAll();
    time_init();

#if _DEBUG
    Delay(3000);
#endif

    if (sd_get_num() > 0) {
        sd_card_t *sdcard = sd_get_by_num(0);
        FIL file;

        if (MountSdCard(sdcard)
            && SelectActiveDrive(sdcard)
            && OpenFile(sdcard, &file, "01.png")) {
            DisplayPng(&file);
            CloseFile(&file);
        }

        UnMountSdCard(sdcard);
    }

    while(true) {
    }
}

#ifdef ESP_PLATFORM
void app_main(void) {
    app_entry();
}
#else
int main(void) {
    app_entry();
    return 0;
}
#endif
