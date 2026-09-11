/* Verify a GIF local table replaces the global palette for its frame. */

#include "loader_builtin_gif_test_common.h"

int
test_loader_0104_loader_builtin_gif_local_palette_numeric(
    int argc,
    char **argv)
{
    unsigned char buffer[256];
    unsigned char const pixel[1] = { 1u };
    unsigned char const local_palette[12] = {
        0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0xffu,
        0x00u, 0xffu, 0x00u,
        0xffu, 0x00u, 0x00u
    };
    unsigned char const expected[6] = {
        0xffu, 0x00u, 0x00u,
        0x00u, 0x00u, 0xffu
    };
    edge_writer_t writer;

    (void)argc;
    (void)argv;
    writer.buffer = buffer;
    writer.capacity = sizeof(buffer);
    writer.length = 0u;
    writer.failed = 0;
    edge_gif_begin(&writer, 1, 1u, 1u);
    edge_gif_image(&writer, 0u, 0u, 1u, 1u, 0, pixel, 1u, 1);
    edge_gif_local_image(&writer,
                         0u,
                         0u,
                         1u,
                         1u,
                         local_palette,
                         pixel,
                         1u);
    edge_put_u8(&writer, 0x3bu);
    if (writer.failed != 0) {
        return 1;
    }
    return edge_expect_rgb("GIF local palette override",
                           buffer,
                           writer.length,
                           0,
                           1,
                           1,
                           2,
                           expected,
                           3u);
}
