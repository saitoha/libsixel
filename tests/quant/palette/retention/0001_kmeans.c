/* SPDX-License-Identifier: MIT */
/* kmeans retains generated sub-byte coordinates without native output */
#include "retention_test_common.h"

int
test_palette_retention_kmeans(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return retention_generated(SIXEL_QUANTIZE_MODEL_KMEANS) ? 0 : 1;
}
