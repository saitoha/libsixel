/* SPDX-License-Identifier: MIT */
/* Check ACT fixed points against the emitted SIXEL RGB definitions. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compat_stub.h"

int
test_palette_0045_snap_grid(int argc, char **argv)
{
    FILE *stream;
    unsigned char data[773];
    size_t length;
    unsigned int count;
    unsigned int index;
    unsigned int channel;
    unsigned int percent;
    unsigned int rgb[3];
    unsigned int seen;
    char const *cursor;

    if (argc != 3) {
        return EXIT_FAILURE;
    }
    stream = sixel_compat_fopen(argv[1], "rb");
    if (stream == NULL) {
        return EXIT_FAILURE;
    }
    length = fread(data, 1U, sizeof(data), stream);
    fclose(stream);
    if (length != 772U) {
        return EXIT_FAILURE;
    }
    count = (unsigned int)data[768] * 256U + data[769];
    if (count == 0U) {
        count = 256U;
    }
    if (count > 256U) {
        return EXIT_FAILURE;
    }
    for (index = 0U; index < count * 3U; ++index) {
        percent = (100U * data[index] + 127U) / 255U;
        if ((255U * percent + 50U) / 100U != data[index]) {
            return EXIT_FAILURE;
        }
    }
    seen = 0U;
    cursor = argv[2];
    while ((cursor = strchr(cursor, '#')) != NULL) {
        if (sscanf(cursor, "#%u;2;%u;%u;%u", &index,
                   &rgb[0], &rgb[1], &rgb[2]) == 4) {
            if (index >= count) {
                return EXIT_FAILURE;
            }
            for (channel = 0U; channel < 3U; ++channel) {
                if (rgb[channel] > 100U
                        || (255U * rgb[channel] + 50U) / 100U
                            != data[index * 3U + channel]) {
                    return EXIT_FAILURE;
                }
            }
            ++seen;
        }
        ++cursor;
    }
    return seen == count ? EXIT_SUCCESS : EXIT_FAILURE;
}
