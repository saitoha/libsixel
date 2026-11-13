/* SPDX-License-Identifier: MIT */
/* Verify reserved key eytzinger bluenoise. */
#include <sixel.h>
int test_reserved_key_case(int, int);

int
test_lookup_dispatch_0045(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_reserved_key_case(SIXEL_LUT_POLICY_EYTZINGER,
        SIXEL_DIFFUSE_BLUENOISE_DITHER);
}
