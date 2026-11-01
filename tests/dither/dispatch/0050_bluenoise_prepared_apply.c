/* SPDX-License-Identifier: MIT */
/* At (0,0), the red perturbation is -32 for a/x and 32*52/255 for BN. */
#include <sixel.h>
int prepared_query_test(char const *, int, int);

int
test_prepared_query_50(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return prepared_query_test("dither/bluenoise.8bit",
        SIXEL_DIFFUSE_BLUENOISE_DITHER, 134);
}
