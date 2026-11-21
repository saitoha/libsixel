/* SPDX-License-Identifier: MIT */
#include <sixel.h>

int test_highcolor_diffusion_neighborhood(unsigned char *, int, int, int,
                                         int const *);

int
test_highcolor_0091(int argc, char **argv)
{
    /* The leading black pixel adds one to each channel of the source.
     * Its red residue becomes 7, so the right tap adds (7 + 2) / 2 = 4. */
    static int const expected[] = {
        0, 0, 0,  3, 0, 6,  13, 0, 0,
        0, 0, 0,  0, 0, 0,  0, 0, 0
    };
    unsigned char pixels[] = {
        0, 0, 0,  6, 0, 15,  28, 0, 0,
        0, 0, 0,  0, 0, 0,  0, 0, 0
    };

    (void)argc;
    (void)argv;
    return test_highcolor_diffusion_neighborhood(
        pixels, 3, 2, SIXEL_DIFFUSE_SIERRA1, expected);
}
