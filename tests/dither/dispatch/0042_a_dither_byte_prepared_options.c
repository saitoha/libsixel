/* SPDX-License-Identifier: MIT */
/* Prepared a_dither byte settings survive destruction of the request. */
#include <sixel.h>
int prepared_options_test(char const *, int, int);

int
test_prepared_options_42(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return prepared_options_test("dither/a_dither.8bit",
        SIXEL_DIFFUSE_A_DITHER, SIXEL_PIXELFORMAT_RGB888);
}
