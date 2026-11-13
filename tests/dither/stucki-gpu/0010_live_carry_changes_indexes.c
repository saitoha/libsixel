/* SPDX-License-Identifier: MIT */
/* Verify live carry changes indexes. */
#include <sixel.h>
int test_stucki_gpu_carry_observation(int);

int
test_dither_stucki_gpu_0010(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_stucki_gpu_carry_observation(1);
}
