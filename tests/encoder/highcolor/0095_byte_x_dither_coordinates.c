/* SPDX-License-Identifier: MIT */
#include <sixel.h>

int test_highcolor_diffusion_neighborhood(unsigned char *, int, int, int,
                                         int const *);

int
test_highcolor_0095(int argc, char **argv)
{
    /* At x=0, only red is reduced before the byte store.
     * The XOR mask is positive at x=1, including green. */
    static int const expected[] = {
        12, 25, 38,  0, 13, 100
    };
    unsigned char pixels[] = {
        32, 64, 96,  0, 32, 255
    };

    (void)argc;
    (void)argv;
    return test_highcolor_diffusion_neighborhood(
        pixels, 2, 1, SIXEL_DIFFUSE_X_DITHER, expected);
}
