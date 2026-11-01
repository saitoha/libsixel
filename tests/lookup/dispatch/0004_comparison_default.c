/* SPDX-License-Identifier: MIT */
/* Verify comparison default. */
#include <sixel.h>
int test_gpupal_comparison_request(int);

int
test_gpupal_0010_comparison_default(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_gpupal_comparison_request(0);
}
