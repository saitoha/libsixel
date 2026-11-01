/* SPDX-License-Identifier: MIT */
/* Prepared x_dither byte settings survive destruction of the request. */
#include <sixel.h>
int prepared_options_test(char const *, int, int);

int
test_prepared_options_44(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return prepared_options_test("dither/x_dither.8bit",
        SIXEL_DIFFUSE_X_DITHER, SIXEL_PIXELFORMAT_RGB888);
}
