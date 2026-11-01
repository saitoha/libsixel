/* SPDX-License-Identifier: MIT */
/* linear output thresholds use destination units and return working
 * residuals. */
#include <sixel.h>
int test_delta_output_threshold(int);

int
test_delta_native_0120(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_output_threshold(0);
}
