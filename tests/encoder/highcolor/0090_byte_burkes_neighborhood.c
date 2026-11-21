/* SPDX-License-Identifier: MIT */
#include <sixel.h>

int test_highcolor_diffusion_neighborhood(unsigned char *, int, int, int,
                                         int const *);

int
test_highcolor_0090(int argc, char **argv)
{
    /* The tap directly below adds floor((7 + 2) * 4 / 16) = 2.
     * Red 30 crosses a cell boundary on the second row. */
    static int const expected[] = {
        0, 0, 0,  0, 0, 0,  3, 0, 6,  0, 0, 0,  0, 0, 0,
        0, 0, 0,  0, 0, 0,  13, 0, 0,  0, 0, 0,  0, 0, 0
    };
    unsigned char pixels[] = {
        0, 0, 0,  0, 0, 0,  7, 0, 16,  0, 0, 0,  0, 0, 0,
        0, 0, 0,  0, 0, 0,  30, 0, 0,  0, 0, 0,  0, 0, 0
    };

    (void)argc;
    (void)argv;
    return test_highcolor_diffusion_neighborhood(
        pixels, 5, 2, SIXEL_DIFFUSE_BURKES, expected);
}
