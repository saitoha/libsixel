/* SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include <sixel.h>

int
test_frame_format_0068_normalize_g2_empty_width(int argc, char **argv)
{
    unsigned char const src[] = {0};
    unsigned char dst[1];
    int format;
    SIXELSTATUS status;

    (void)argc;
    (void)argv;
    status = sixel_helper_normalize_pixelformat(dst, &format, src,
        SIXEL_PIXELFORMAT_G2, 0, 1);
    return status == SIXEL_BAD_ARGUMENT ? EXIT_SUCCESS : EXIT_FAILURE;
}
