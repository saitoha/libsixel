/* SPDX-License-Identifier: MIT */
/* Verify delta p3 cache failure. */
#include <sixel.h>
int test_delta_p3_cache(int);

int
test_delta_native_0110(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_p3_cache(1);
}
