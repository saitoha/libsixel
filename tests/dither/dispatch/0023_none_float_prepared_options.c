/* SPDX-License-Identifier: MIT */
/* Prepared none float settings survive destruction of the request. */
#include <sixel.h>
int prepared_options_test(char const *, int, int);

int
test_prepared_options_23(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return prepared_options_test("dither/none.float32",
        SIXEL_DIFFUSE_NONE, SIXEL_PIXELFORMAT_RGBFLOAT32);
}
