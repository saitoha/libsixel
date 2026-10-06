/* SPDX-License-Identifier: MIT */
/* linear output with linear working coordinates preserves wire values and
 * paint/keep/change/invalidate state. */
#include <sixel.h>
int test_delta_output_frames(int, int, int);

int
test_delta_native_0124(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_output_frames(1, 0, 0);
}
