/* SPDX-License-Identifier: MIT */
/* Heckbert Ward retains signed pair means. */
#include "ordinary_float_test_common.h"

int
test_ordinary_heckbert_ward(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return ordinary_float_ward(SIXEL_QUANTIZE_MODEL_MEDIANCUT) ? 0 : 1;
}
