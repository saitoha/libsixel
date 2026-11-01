/* SPDX-License-Identifier: MIT */
/* Prepared bluenoise byte settings survive destruction of the request. */
#include <sixel.h>
int prepared_options_test(char const *, int, int);

int
test_prepared_options_46(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return prepared_options_test("dither/bluenoise.8bit",
        SIXEL_DIFFUSE_BLUENOISE_DITHER, SIXEL_PIXELFORMAT_RGB888);
}
