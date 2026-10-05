/* SPDX-License-Identifier: MIT */
/* Reject bright delta history for a virtual DIN99d query. */
#include <sixel.h>
int test_delta_work_virtual(int);

int
test_delta_native_0099(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_work_virtual(0);
}
