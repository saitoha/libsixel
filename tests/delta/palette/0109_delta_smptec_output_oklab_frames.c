/* SPDX-License-Identifier: MIT */
/* smptec output with oklab working coordinates preserves wire values and
 * paint/keep/change/invalidate state. */
#include <sixel.h>
int test_delta_output_frames(int, int, int);

int
test_delta_native_0136(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_output_frames(2, 1, 0);
}
