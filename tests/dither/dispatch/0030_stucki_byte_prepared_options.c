/* SPDX-License-Identifier: MIT */
/* Prepared stucki byte settings survive destruction of the request. */
#include <sixel.h>
int prepared_options_test(char const *, int, int);

int
test_prepared_options_30(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return prepared_options_test("dither/stucki.8bit",
        SIXEL_DIFFUSE_STUCKI, SIXEL_PIXELFORMAT_RGB888);
}
