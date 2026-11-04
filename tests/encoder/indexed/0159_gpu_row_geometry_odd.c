/* SPDX-License-Identifier: MIT */
/* GPU chunk geometry for odd raster.
 * Policy: docs/threading/encoder.md
 */
#include "src/gpu-palette.h"

int
test_gpu_row_geometry_odd(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return sixel_gpu_palette_rows_per_chunk(1920, 1009) == 174 ? 0 : 1;
}
