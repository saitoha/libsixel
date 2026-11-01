/* SPDX-License-Identifier: MIT */
/* Prepared sierra1 float settings survive destruction of the request. */
#include <sixel.h>
int prepared_options_test(char const *, int, int);

int
test_prepared_options_35(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return prepared_options_test("dither/sierra1.float32",
        SIXEL_DIFFUSE_SIERRA1, SIXEL_PIXELFORMAT_RGBFLOAT32);
}
