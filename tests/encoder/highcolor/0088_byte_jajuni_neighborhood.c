/* SPDX-License-Identifier: MIT */
#include <sixel.h>

int test_highcolor_diffusion_neighborhood(unsigned char *, int, int, int,
                                         int const *);

int
test_highcolor_0088(int argc, char **argv)
{
    /* The horizontal two-pixel tap adds floor((7 + 4) * 5 / 48) = 1.
     * Red 31 enters the next cell and is emitted as 13 percent. */
    static int const expected[] = {
        0, 0, 0,  0, 0, 0,  3, 0, 6,  0, 0, 0,  13, 0, 0,
        0, 0, 0,  0, 0, 0,  0, 0, 0,  0, 0, 0,  0, 0, 0,
        0, 0, 0,  0, 0, 0,  0, 0, 0,  0, 0, 0,  0, 0, 0
    };
    unsigned char pixels[] = {
        0, 0, 0,  0, 0, 0,  7, 0, 16,  0, 0, 0,  31, 0, 0,
        0, 0, 0,  0, 0, 0,  0, 0, 0,  0, 0, 0,  0, 0, 0,
        0, 0, 0,  0, 0, 0,  0, 0, 0,  0, 0, 0,  0, 0, 0
    };

    (void)argc;
    (void)argv;
    return test_highcolor_diffusion_neighborhood(
        pixels, 5, 3, SIXEL_DIFFUSE_JAJUNI, expected);
}
