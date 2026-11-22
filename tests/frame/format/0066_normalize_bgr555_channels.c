/* SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include <string.h>
#include <sixel.h>

int
test_frame_format_0066_normalize_bgr555_channels(int argc, char **argv)
{
    /* 0x1515 stores B=5, G=8, R=21 and reads alike in either byte order. */
    unsigned char const src[] = {0x15, 0x15};
    unsigned char const expected[] = {168, 64, 40};
    unsigned char dst[sizeof(expected)];
    int format;
    SIXELSTATUS status;

    (void)argc;
    (void)argv;
    status = sixel_helper_normalize_pixelformat(dst, &format, src,
        SIXEL_PIXELFORMAT_BGR555, 1, 1);
    return status == SIXEL_OK && format == SIXEL_PIXELFORMAT_RGB888 &&
        memcmp(dst, expected, sizeof(expected)) == 0
        ? EXIT_SUCCESS : EXIT_FAILURE;
}
