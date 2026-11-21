/* SPDX-License-Identifier: MIT */
#include <sixel.h>

int test_highcolor_diffusion_neighborhood(unsigned char *, int, int, int,
                                         int const *);

int
test_highcolor_0096(int argc, char **argv)
{
    /* The right tap adds two to both red and green. Red stays at 255
     * and green crosses a 5-bit cell, making diffusion observable. */
    static int const expected[] = {
        0, 0, 0,  3, 3, 6,  100, 13, 0,
        0, 0, 0,  0, 0, 0,  0, 0, 0
    };
    unsigned char pixels[] = {
        0, 0, 0,  7, 7, 16,  255, 31, 0,
        0, 0, 0,  0, 0, 0,  0, 0, 0
    };

    (void)argc;
    (void)argv;
    return test_highcolor_diffusion_neighborhood(
        pixels, 3, 2, SIXEL_DIFFUSE_FS, expected);
}
