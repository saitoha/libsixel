/* SPDX-License-Identifier: MIT
 * Retain dark ramp steps and sparse bright UI colors in heckbert.
 */
#include <sixel.h>
#include "../../_support/quant/palette/adaptive_range/adaptive_range_test_common.h"

int
test_0001_range_heckbert(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return adaptive_range_test_palette(SIXEL_QUANTIZE_MODEL_MEDIANCUT);
}

