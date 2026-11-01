/* SPDX-License-Identifier: MIT */
/* Verify delta linear skip. */
#include <sixel.h>
int test_delta_work_residual(int);

int
test_delta_native_0086(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_work_residual(0);
}
