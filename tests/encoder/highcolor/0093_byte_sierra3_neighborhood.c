/* SPDX-License-Identifier: MIT */
#include <sixel.h>

int test_highcolor_diffusion_neighborhood(unsigned char *, int, int, int,
                                         int const *);

int
test_highcolor_0093(int argc, char **argv)
{
    /* The lower-left tap adds floor((7 + 4) * 4 / 32) = 1.
     * Only that red target crosses the next 5-bit cell. */
    static int const expected[] = {
        0, 0, 0,  0, 0, 0,  3, 0, 6,  0, 0, 0,  0, 0, 0,
        0, 0, 0,  13, 0, 0,  0, 0, 0,  0, 0, 0,  0, 0, 0,
        0, 0, 0,  0, 0, 0,  0, 0, 0,  0, 0, 0,  0, 0, 0
    };
    unsigned char pixels[] = {
        0, 0, 0,  0, 0, 0,  7, 0, 16,  0, 0, 0,  0, 0, 0,
        0, 0, 0,  31, 0, 0,  0, 0, 0,  0, 0, 0,  0, 0, 0,
        0, 0, 0,  0, 0, 0,  0, 0, 0,  0, 0, 0,  0, 0, 0
    };

    (void)argc;
    (void)argv;
    return test_highcolor_diffusion_neighborhood(
        pixels, 5, 3, SIXEL_DIFFUSE_SIERRA3, expected);
}
