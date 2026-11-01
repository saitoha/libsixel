/* SPDX-License-Identifier: MIT */
/* Heckbert allocation failure cannot retry byte storage. */
#include "../../_support/quant/palette/ordinary_float/ordinary_float_test_common.h"

int
test_ordinary_heckbert_fault(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return ordinary_float_failure(SIXEL_QUANTIZE_MODEL_MEDIANCUT, 0, 0) ? 0 : 1;
}
