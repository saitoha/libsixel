/* SPDX-License-Identifier: MIT */
/* Prepared jajuni byte settings survive destruction of the request. */
#include <sixel.h>
int prepared_options_test(char const *, int, int);

int
test_prepared_options_28(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return prepared_options_test("dither/jajuni.8bit",
        SIXEL_DIFFUSE_JAJUNI, SIXEL_PIXELFORMAT_RGB888);
}
