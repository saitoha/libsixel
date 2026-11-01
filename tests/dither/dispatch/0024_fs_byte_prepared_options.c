/* SPDX-License-Identifier: MIT */
/* Prepared fs byte settings survive destruction of the request. */
#include <sixel.h>
int prepared_options_test(char const *, int, int);

int
test_prepared_options_24(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return prepared_options_test("dither/fs.8bit",
        SIXEL_DIFFUSE_FS, SIXEL_PIXELFORMAT_RGB888);
}
