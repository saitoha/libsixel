/* SPDX-License-Identifier: MIT */
/* A seeded smptec surface follows transfer/definition identity across
 * fixed-palette frames. */
#include <sixel.h>
int test_delta_output_identity(int);

int
test_delta_native_0140(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_output_identity(1);
}
