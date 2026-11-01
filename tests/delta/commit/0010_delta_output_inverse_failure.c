/* SPDX-License-Identifier: MIT */
/* Nonfinite/overflow and unsupported output inverses leave the
 * destination unchanged. */
#include <sixel.h>
int test_delta_output_failure(void);

int
test_delta_native_0141(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_output_failure();
}
