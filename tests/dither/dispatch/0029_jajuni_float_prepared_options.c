/* SPDX-License-Identifier: MIT */
/* Prepared jajuni float settings survive destruction of the request. */
#include <sixel.h>
int prepared_options_test(char const *, int, int);

int
test_prepared_options_29(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return prepared_options_test("dither/jajuni.float32",
        SIXEL_DIFFUSE_JAJUNI, SIXEL_PIXELFORMAT_RGBFLOAT32);
}
