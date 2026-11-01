/* SPDX-License-Identifier: MIT */
/* Prepared a_dither float settings survive destruction of the request. */
#include <sixel.h>
int prepared_options_test(char const *, int, int);

int
test_prepared_options_43(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return prepared_options_test("dither/a_dither.float32",
        SIXEL_DIFFUSE_A_DITHER, SIXEL_PIXELFORMAT_RGBFLOAT32);
}
