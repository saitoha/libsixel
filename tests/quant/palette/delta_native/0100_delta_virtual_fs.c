/* SPDX-License-Identifier: MIT */
/* Paint the chosen FS color when virtual delta history is worse. */
#include <sixel.h>
int test_delta_work_virtual(int);

int
test_delta_native_0100(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_work_virtual(1);
}
