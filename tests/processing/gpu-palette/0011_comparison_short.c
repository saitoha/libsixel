/* SPDX-License-Identifier: MIT */
/* Verify comparison short. */
#include <sixel.h>
int test_gpupal_comparison_request(int);

int
test_gpupal_0011_comparison_short(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_gpupal_comparison_request(1);
}
