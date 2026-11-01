/* SPDX-License-Identifier: MIT */
/* Verify delta workspace legacy. */
#include <sixel.h>
int test_delta_work_extended(int);

int
test_delta_native_0098(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_work_extended(2);
}
