/* SPDX-License-Identifier: MIT */
/* Verify delta oklab compare. */
#include <sixel.h>
int test_delta_work_compare(int);

int
test_delta_native_0088(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_work_compare(1);
}
