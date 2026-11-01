/* SPDX-License-Identifier: MIT */
/* linear output with cielab working coordinates preserves wire values and
 * paint/keep/change/invalidate state. */
#include <sixel.h>
int test_delta_output_frames(int, int, int);

int
test_delta_native_0126(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_output_frames(3, 0, 0);
}
