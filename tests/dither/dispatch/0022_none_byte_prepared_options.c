/* SPDX-License-Identifier: MIT */
/* Prepared none byte settings survive destruction of the request. */
#include <sixel.h>
int prepared_options_test(char const *, int, int);

int
test_prepared_options_22(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return prepared_options_test("dither/none.8bit",
        SIXEL_DIFFUSE_NONE, SIXEL_PIXELFORMAT_RGB888);
}
