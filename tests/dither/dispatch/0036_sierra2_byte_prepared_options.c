/* SPDX-License-Identifier: MIT */
/* Prepared sierra2 byte settings survive destruction of the request. */
#include <sixel.h>
int prepared_options_test(char const *, int, int);

int
test_prepared_options_36(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return prepared_options_test("dither/sierra2.8bit",
        SIXEL_DIFFUSE_SIERRA2, SIXEL_PIXELFORMAT_RGB888);
}
