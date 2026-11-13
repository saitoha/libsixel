/* SPDX-License-Identifier: MIT */
/* Verify deadzone scan parity. */
#include <sixel.h>
int test_atkinson_gpu_carry_observation(int);

int
test_dither_atkinson_gpu_0009(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_atkinson_gpu_carry_observation(0);
}
