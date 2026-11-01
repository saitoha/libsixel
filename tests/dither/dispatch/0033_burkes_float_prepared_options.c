/* SPDX-License-Identifier: MIT */
/* Prepared burkes float settings survive destruction of the request. */
#include <sixel.h>
int prepared_options_test(char const *, int, int);

int
test_prepared_options_33(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return prepared_options_test("dither/burkes.float32",
        SIXEL_DIFFUSE_BURKES, SIXEL_PIXELFORMAT_RGBFLOAT32);
}
