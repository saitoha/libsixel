/* SPDX-License-Identifier: MIT */
#include <sixel.h>

int test_highcolor_diffusion_neighborhood(unsigned char *, int, int, int,
                                         int const *);

int
test_highcolor_0092(int argc, char **argv)
{
    /* The leading pixels make the source red residue 7. The horizontal
     * two-pixel tap adds floor((7 + 4) * 3 / 16) = 2 to red 30. */
    static int const expected[] = {
        0, 0, 0,  0, 0, 0,  3, 0, 6,  0, 0, 0,  13, 0, 0,
        0, 0, 0,  0, 0, 0,  0, 0, 0,  0, 0, 0,  0, 0, 0
    };
    unsigned char pixels[] = {
        0, 0, 0,  0, 0, 0,  6, 0, 15,  0, 0, 0,  30, 0, 0,
        0, 0, 0,  0, 0, 0,  0, 0, 0,  0, 0, 0,  0, 0, 0
    };

    (void)argc;
    (void)argv;
    return test_highcolor_diffusion_neighborhood(
        pixels, 5, 2, SIXEL_DIFFUSE_SIERRA2, expected);
}
