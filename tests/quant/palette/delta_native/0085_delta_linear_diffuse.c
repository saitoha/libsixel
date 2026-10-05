/* SPDX-License-Identifier: MIT */
/* Verify delta linear diffuse. */
#include <sixel.h>
int test_delta_work_residual(int);

int
test_delta_native_0085(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_work_residual(1);
}
