/* SPDX-License-Identifier: MIT */
/* GPU chunk geometry for small raster.
 * Policy: docs/threading/encoder.md
 */
#include "src/gpu-palette.h"

int
test_gpu_row_geometry_small(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return sixel_gpu_palette_rows_per_chunk(16, 16) == 0 ? 0 : 1;
}
