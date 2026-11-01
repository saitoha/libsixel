/* SPDX-License-Identifier: MIT */
/* Verify delta p3 cielab transform. */
#include <sixel.h>
int test_delta_p3_transform(int);

int
test_delta_native_0104(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_p3_transform(3);
}
