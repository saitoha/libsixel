/* SPDX-License-Identifier: MIT */
/* linear output preserves retained colors through actual worker dispatch. */
#include <sixel.h>
int test_delta_output_frames(int, int, int);

int
test_delta_native_0128(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_output_frames(2, 0, 1);
}
