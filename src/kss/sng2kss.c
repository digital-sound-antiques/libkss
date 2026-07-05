#include <stdlib.h>
#include <string.h>
#include "kss.h"

int KSS_isSNGdata(uint8_t *data, uint32_t size) {
    uint32_t song_length, pattern_count, i;
    if (size < 0x7E5 + 0x600) return 0;

    song_length = data[0x780];
    if (song_length == 0 || song_length > 0x64) return 0;

    pattern_count = (size - 0x7E5) / 0x600;
    if (pattern_count == 0) return 0;

    for (i = 0; i < song_length; i++) {
        if (data[0x781 + i] >= pattern_count) return 0;
    }

    return 1;
}

void KSS_get_info_sngdata(KSS *kss, uint8_t *data, uint32_t size) {
    (void)data; (void)size;
    strcpy((char *)kss->idstr, "SNG");
    strcpy((char *)kss->title, "-");
    kss->loop_detectable = 0;
    kss->stop_detectable = 0;
}

KSS *KSS_sng2kss(uint8_t *data, uint32_t size) {
    if (!KSS_isSNGdata(data, size)) return NULL;
    return KSS_new(data, size);
}
