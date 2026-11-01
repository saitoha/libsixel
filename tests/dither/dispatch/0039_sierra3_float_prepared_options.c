/* SPDX-License-Identifier: MIT */
/* Prepared sierra3 float settings survive destruction of the request. */
#include <sixel.h>
int prepared_options_test(char const *, int, int);

int
test_prepared_options_39(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return prepared_options_test("dither/sierra3.float32",
        SIXEL_DIFFUSE_SIERRA3, SIXEL_PIXELFORMAT_RGBFLOAT32);
}
