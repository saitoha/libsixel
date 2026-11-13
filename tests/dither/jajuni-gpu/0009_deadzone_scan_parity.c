/* SPDX-License-Identifier: MIT */
/* Verify deadzone scan parity. */
#include <sixel.h>
int test_jajuni_gpu_carry_observation(int);

int
test_dither_jajuni_gpu_0009(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_jajuni_gpu_carry_observation(0);
}
