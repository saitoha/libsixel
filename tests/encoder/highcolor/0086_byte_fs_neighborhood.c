/* SPDX-License-Identifier: MIT */
#include <sixel.h>

int test_highcolor_diffusion_neighborhood(unsigned char *, int, int, int,
                                         int const *);

int
test_highcolor_0086(int argc, char **argv)
{
    /* Red error 7 moves its right neighbor into the next 5-bit cell.
     * The first sample in that cell rounds to 13 percent; 31 rounds to 12. */
    static int const expected[] = {
        0, 0, 0,  3, 0, 6,  13, 0, 0,
        0, 0, 0,  0, 0, 0,   0, 0, 0
    };
    unsigned char pixels[] = {
        0, 0, 0,  7, 0, 16,  31, 0, 0,
        0, 0, 0,  0, 0,  0,   0, 0, 0
    };

    (void)argc;
    (void)argv;
    return test_highcolor_diffusion_neighborhood(
        pixels, 3, 2, SIXEL_DIFFUSE_FS, expected);
}
