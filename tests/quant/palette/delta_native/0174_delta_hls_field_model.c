/* SPDX-License-Identifier: MIT */
/* Six chromatic sectors, gray and a double-rounding witness match
 * independent HLS fields and decoded percentages. */
#include <sixel.h>
int test_delta_hls_fields(void);

int
test_delta_native_0174(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_hls_fields();
}
