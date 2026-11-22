/* SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include <string.h>
#include <sixel.h>

int
test_frame_format_0067_normalize_din99d_chroma(int argc, char **argv)
{
    /* Native chroma [-1, 1] maps to the full unsigned byte range. */
    float const src[] = {0.25f, -1.25f, 0.5f};
    unsigned char const expected[] = {64, 0, 191};
    unsigned char dst[sizeof(expected)];
    int format;
    SIXELSTATUS status;

    (void)argc;
    (void)argv;
    status = sixel_helper_normalize_pixelformat(dst, &format,
        (unsigned char const *)src, SIXEL_PIXELFORMAT_DIN99DFLOAT32, 1, 1);
    return status == SIXEL_OK && format == SIXEL_PIXELFORMAT_RGB888 &&
        memcmp(dst, expected, sizeof(expected)) == 0
        ? EXIT_SUCCESS : EXIT_FAILURE;
}
