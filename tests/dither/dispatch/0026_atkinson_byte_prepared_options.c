/* SPDX-License-Identifier: MIT */
/* Prepared atkinson byte settings survive destruction of the request. */
#include <sixel.h>
int prepared_options_test(char const *, int, int);

int
test_prepared_options_26(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return prepared_options_test("dither/atkinson.8bit",
        SIXEL_DIFFUSE_ATKINSON, SIXEL_PIXELFORMAT_RGB888);
}
