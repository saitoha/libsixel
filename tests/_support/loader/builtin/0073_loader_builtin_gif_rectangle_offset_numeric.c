/* Verify a GIF image descriptor offset is composed on the full canvas. */

#include "loader_builtin_gif_test_common.h"

int
test_loader_0073_loader_builtin_gif_rectangle_offset_numeric(
    int argc,
    char **argv)
{
    unsigned char buffer[160];
    unsigned char const pixels[1] = { 1u };
    unsigned char const expected[18] = {
        0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u,
        0xffu, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u
    };
    edge_writer_t writer;

    (void)argc;
    (void)argv;
    writer.buffer = buffer;
    writer.capacity = sizeof(buffer);
    writer.length = 0u;
    writer.failed = 0;
    edge_gif_begin(&writer, 1, 3u, 2u);
    edge_gif_image(&writer, 1u, 1u, 1u, 1u, 0, pixels, 1u, 1);
    edge_put_u8(&writer, 0x3bu);
    if (writer.failed != 0) {
        return 1;
    }
    return edge_expect_rgb("GIF image rectangle offset",
                           buffer,
                           writer.length,
                           1,
                           3,
                           2,
                           1,
                           expected,
                           sizeof(expected));
}
