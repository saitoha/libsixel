/* SPDX-License-Identifier: MIT */
/* Fixed delta: fixed mask failure. */
#include <stddef.h>
int test_delta_fixed_mask_failure(void);

int
test_delta_native_0050(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_fixed_mask_failure();
}
