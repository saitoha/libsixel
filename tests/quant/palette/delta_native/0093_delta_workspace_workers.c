/* SPDX-License-Identifier: MIT */
/* Verify delta workspace workers. */
#include <sixel.h>
int test_delta_work_fixed(int);

int
test_delta_native_0093(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_work_fixed(1);
}
