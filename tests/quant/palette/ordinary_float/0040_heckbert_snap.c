/* SPDX-License-Identifier: MIT */
/* Heckbert retains partial iteration and final snap. */
#include "ordinary_float_test_common.h"

int
test_ordinary_heckbert_snap(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return ordinary_float_snap(SIXEL_QUANTIZE_MODEL_MEDIANCUT) ? 0 : 1;
}
