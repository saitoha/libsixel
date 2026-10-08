/* SPDX-License-Identifier: MIT */
/* Heckbert rebuilds after a float allocation failure. */
#include "ordinary_float_test_common.h"

int
test_ordinary_heckbert_reuse(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return ordinary_float_failure(SIXEL_QUANTIZE_MODEL_MEDIANCUT, 0, 1) ? 0 : 1;
}
