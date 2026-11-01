/* SPDX-License-Identifier: MIT */
/* Prepared lso2 float settings survive destruction of the request. */
#include <sixel.h>
int prepared_options_test(char const *, int, int);

int
test_prepared_options_41(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return prepared_options_test("dither/lso2.float32",
        SIXEL_DIFFUSE_LSO2, SIXEL_PIXELFORMAT_RGBFLOAT32);
}
