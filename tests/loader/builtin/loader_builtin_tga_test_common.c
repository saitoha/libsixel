/* SPDX-License-Identifier: MIT */

#include "loader_builtin_tga_test_common.h"

void
edge_tga_begin(edge_writer_t *writer,
               unsigned int color_map_type,
               unsigned int image_type,
               unsigned int palette_length,
               unsigned int palette_depth,
               unsigned int width,
               unsigned int height,
               unsigned int pixel_depth,
               unsigned int descriptor)
{
    edge_put_u8(writer, 0u);
    edge_put_u8(writer, color_map_type);
    edge_put_u8(writer, image_type);
    edge_put_u16le(writer, 0u);
    edge_put_u16le(writer, palette_length);
    edge_put_u8(writer, palette_depth);
    edge_put_u16le(writer, 0u);
    edge_put_u16le(writer, 0u);
    edge_put_u16le(writer, width);
    edge_put_u16le(writer, height);
    edge_put_u8(writer, pixel_depth);
    edge_put_u8(writer, descriptor);
}
