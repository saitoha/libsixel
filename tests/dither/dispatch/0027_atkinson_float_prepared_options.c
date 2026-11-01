/* SPDX-License-Identifier: MIT */
/* Prepared atkinson float settings survive destruction of the request. */
#include <sixel.h>
int prepared_options_test(char const *, int, int);

int
test_prepared_options_27(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return prepared_options_test("dither/atkinson.float32",
        SIXEL_DIFFUSE_ATKINSON, SIXEL_PIXELFORMAT_RGBFLOAT32);
}
