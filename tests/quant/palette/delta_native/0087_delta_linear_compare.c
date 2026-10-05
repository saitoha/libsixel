/* SPDX-License-Identifier: MIT */
/* Verify delta linear compare. */
#include <sixel.h>
int test_delta_work_compare(int);

int
test_delta_native_0087(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_work_compare(0);
}
