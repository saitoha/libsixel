/* SPDX-License-Identifier: MIT */
/* Prepared sierra3 byte settings survive destruction of the request. */
#include <sixel.h>
int prepared_options_test(char const *, int, int);

int
test_prepared_options_38(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return prepared_options_test("dither/sierra3.8bit",
        SIXEL_DIFFUSE_SIERRA3, SIXEL_PIXELFORMAT_RGB888);
}
