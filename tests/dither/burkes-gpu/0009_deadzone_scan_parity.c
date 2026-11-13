/* SPDX-License-Identifier: MIT */
/* Verify deadzone scan parity. */
#include <sixel.h>
int test_burkes_gpu_carry_observation(int);

int
test_dither_burkes_gpu_0009(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_burkes_gpu_carry_observation(0);
}
