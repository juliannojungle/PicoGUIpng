#include "pico/stdlib.h"
#include "rtc.h" // rtc for file's timestamp.
#include "f_util.h" // FS functions and declarations.

#include "helper/fileHelper.cpp"
#include "helper/pngHelper.cpp"

int main() {
    // Initialize pico-sdk
    stdio_init_all();
    time_init();

#if _DEBUG
    sleep_ms(3000);
#endif

    // Initialize no-OS-FS
    SetupSpi();
    SetupSdCard();

    if (sd_get_num() > 0) {
        sd_card_t *sdcard = sd_get_by_num(0);
        FIL file;

        if (MountSdCard(sdcard)
            && SelectActiveDrive(sdcard)
            && OpenFile(sdcard, &file, "01.png")) {
            DisplayPng(file);
            CloseFile(&file);
        }

        UnMountSdCard(sdcard);
    }

    while(true) {
    }
}