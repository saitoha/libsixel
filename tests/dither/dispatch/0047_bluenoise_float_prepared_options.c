/* SPDX-License-Identifier: MIT */
/* Prepared bluenoise float settings survive destruction of the request. */
#include <sixel.h>
int prepared_options_test(char const *, int, int);

int
test_prepared_options_47(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return prepared_options_test("dither/bluenoise.float32",
        SIXEL_DIFFUSE_BLUENOISE_DITHER, SIXEL_PIXELFORMAT_RGBFLOAT32);
}
