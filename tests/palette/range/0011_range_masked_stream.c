/* SPDX-License-Identifier: MIT
 * Preserve native sample coordinates and visible population during binning.
 */
#include "../../_support/quant/palette/adaptive_range/adaptive_range_test_common.h"

int
test_0011_range_masked_stream(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return adaptive_range_test_stream(1);
}
