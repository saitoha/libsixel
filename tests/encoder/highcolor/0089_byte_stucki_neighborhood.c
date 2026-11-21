/* SPDX-License-Identifier: MIT */
#include <sixel.h>

int test_highcolor_diffusion_neighborhood(unsigned char *, int, int, int,
                                         int const *);

int
test_highcolor_0089(int argc, char **argv)
{
    /* The right tap adds floor((7 + 4) * 8 / 42) = 2.
     * Red 30 needs both units to enter the next 5-bit cell. */
    static int const expected[] = {
        0, 0, 0,  0, 0, 0,  3, 0, 6,  13, 0, 0,  0, 0, 0,
        0, 0, 0,  0, 0, 0,  0, 0, 0,  0, 0, 0,  0, 0, 0,
        0, 0, 0,  0, 0, 0,  0, 0, 0,  0, 0, 0,  0, 0, 0
    };
    unsigned char pixels[] = {
        0, 0, 0,  0, 0, 0,  7, 0, 16,  30, 0, 0,  0, 0, 0,
        0, 0, 0,  0, 0, 0,  0, 0, 0,  0, 0, 0,  0, 0, 0,
        0, 0, 0,  0, 0, 0,  0, 0, 0,  0, 0, 0,  0, 0, 0
    };

    (void)argc;
    (void)argv;
    return test_highcolor_diffusion_neighborhood(
        pixels, 5, 3, SIXEL_DIFFUSE_STUCKI, expected);
}
