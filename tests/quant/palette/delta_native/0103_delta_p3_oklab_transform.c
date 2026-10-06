/* SPDX-License-Identifier: MIT */
/* Verify delta p3 oklab transform. */
#include <sixel.h>
int test_delta_p3_transform(int);

int
test_delta_native_0103(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_p3_transform(2);
}
