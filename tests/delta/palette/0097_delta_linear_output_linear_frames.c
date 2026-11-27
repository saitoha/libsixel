/* SPDX-License-Identifier: MIT */
/* linear output with linear working coordinates preserves wire values and
 * paint/keep/change/invalidate state. */
#include <sixel.h>
int test_delta_output_work_frames(int, int, int, char const *);

int
test_delta_native_0124(int argc, char **argv)
{
    return test_delta_output_work_frames(1, 0, 0,
                                       argc > 1 ? argv[1] : "srgb-linear");
}
