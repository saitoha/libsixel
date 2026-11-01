/* SPDX-License-Identifier: MIT */
/* Prepared stucki float settings survive destruction of the request. */
#include <sixel.h>
int prepared_options_test(char const *, int, int);

int
test_prepared_options_31(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return prepared_options_test("dither/stucki.float32",
        SIXEL_DIFFUSE_STUCKI, SIXEL_PIXELFORMAT_RGBFLOAT32);
}
