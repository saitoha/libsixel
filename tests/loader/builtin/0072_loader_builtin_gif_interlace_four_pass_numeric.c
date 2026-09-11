/* Verify the four GIF interlace passes map to logical-screen row order. */

#include "loader_builtin_gif_test_common.h"

int
test_loader_0072_loader_builtin_gif_interlace_four_pass_numeric(
    int argc,
    char **argv)
{
    unsigned char buffer[160];
    unsigned char const encoded_rows[4] = { 1u, 3u, 2u, 0u };
    unsigned char const expected[12] = {
        0xffu, 0x00u, 0x00u,
        0x00u, 0xffu, 0x00u,
        0x00u, 0x00u, 0xffu,
        0x00u, 0x00u, 0x00u
    };
    edge_writer_t writer;

    (void)argc;
    (void)argv;
    writer.buffer = buffer;
    writer.capacity = sizeof(buffer);
    writer.length = 0u;
    writer.failed = 0;
    edge_gif_begin(&writer, 1, 1u, 4u);
    edge_gif_image(&writer,
                   0u,
                   0u,
                   1u,
                   4u,
                   1,
                   encoded_rows,
                   4u,
                   1);
    edge_put_u8(&writer, 0x3bu);
    if (writer.failed != 0) {
        return 1;
    }
    return edge_expect_rgb("GIF interlace four-pass row order",
                           buffer,
                           writer.length,
                           1,
                           1,
                           4,
                           1,
                           expected,
                           sizeof(expected));
}
