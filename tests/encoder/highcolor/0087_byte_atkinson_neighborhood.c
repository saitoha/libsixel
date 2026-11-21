/* SPDX-License-Identifier: MIT */
#include <sixel.h>

int test_highcolor_diffusion_neighborhood(unsigned char *, int, int, int,
                                         int const *);

int
test_highcolor_0087(int argc, char **argv)
{
    /* The rounded 1/8 share of red error 7 raises 31 to 32. The row below
     * starts at zero and shares the already emitted black register. */
    static int const expected[] = {
        0, 0, 0,  3, 0, 6,  13, 0, 0,  0, 0, 0,
        0, 0, 0,  0, 0, 0,   0, 0, 0,  0, 0, 0,
        0, 0, 0,  0, 0, 0,   0, 0, 0,  0, 0, 0
    };
    unsigned char pixels[] = {
        0, 0, 0,  7, 0, 16,  31, 0, 0,  0, 0, 0,
        0, 0, 0,  0, 0,  0,   0, 0, 0,  0, 0, 0,
        0, 0, 0,  0, 0,  0,   0, 0, 0,  0, 0, 0
    };

    (void)argc;
    (void)argv;
    return test_highcolor_diffusion_neighborhood(
        pixels, 4, 3, SIXEL_DIFFUSE_ATKINSON, expected);
}
