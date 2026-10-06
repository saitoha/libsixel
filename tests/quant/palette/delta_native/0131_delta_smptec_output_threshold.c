/* SPDX-License-Identifier: MIT */
/* smptec output thresholds use destination units and return working
 * residuals. */
#include <sixel.h>
int test_delta_output_threshold(int);

int
test_delta_native_0131(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_output_threshold(1);
}
