#include "Platform.c"
#include "rtc.h" // rtc for file's timestamp.
#include "f_util.h" // FS functions and declarations.

#include "fileHelper.c"
#include "pngHelper.c"

int main(void) {
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