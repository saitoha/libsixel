/* SPDX-License-Identifier: MIT */
/* Verify delta p3 gamma transform. */
#include <sixel.h>
int test_delta_p3_transform(int);

int
test_delta_native_0101(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_p3_transform(0);
}
