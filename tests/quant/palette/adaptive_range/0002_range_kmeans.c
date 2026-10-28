/* SPDX-License-Identifier: MIT
 * Retain dark ramp steps and sparse bright UI colors in kmeans.
 */
#include <sixel.h>
#include "adaptive_range_test_common.h"

int
test_0002_range_kmeans(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return adaptive_range_test_palette(SIXEL_QUANTIZE_MODEL_KMEANS);
}

