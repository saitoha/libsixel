/* SPDX-License-Identifier: MIT */
/* Verify lso2 t0 deferred. */
#include <sixel.h>
#include <6cells.h>
#include "tests/_support/lso2-gpu/expected.h"
int test_direct_lso2_case(int, int, int, unsigned char const *);

int
test_dither_gpu_direct_0026(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_direct_lso2_case(SIXEL_LSO2_GAIN_STANDARD, 1, 1,
        lso2_gpu_expected_t0);
}
