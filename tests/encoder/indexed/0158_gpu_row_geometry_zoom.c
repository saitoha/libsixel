/* SPDX-License-Identifier: MIT */
/* GPU chunk geometry for zoom raster.
 * Policy: docs/threading/encoder.md
 */
#include "src/gpu-palette.h"

int
test_gpu_row_geometry_zoom(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return sixel_gpu_palette_rows_per_chunk(1792, 1008) == 168 ? 0 : 1;
}
