/* SPDX-License-Identifier: MIT */
/* Verify delta p3 cache lifetime. */
#include <sixel.h>
int test_delta_p3_cache(int);

int
test_delta_native_0109(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_p3_cache(0);
}
