/* Verify GIF disposal method 3 restores the pre-frame canvas. */

#include "loader_builtin_gif_test_common.h"

int
test_loader_0103_loader_builtin_gif_disposal3_numeric(
    int argc,
    char **argv)
{
    unsigned char buffer[320];
    unsigned char const blue[2] = { 3u, 3u };
    unsigned char const red[1] = { 1u };
    unsigned char const green[1] = { 2u };
    unsigned char const expected[18] = {
        0x00u, 0x00u, 0xffu, 0x00u, 0x00u, 0xffu,
        0xffu, 0x00u, 0x00u, 0x00u, 0x00u, 0xffu,
        0x00u, 0x00u, 0xffu, 0x00u, 0xffu, 0x00u
    };
    edge_writer_t writer;

    (void)argc;
    (void)argv;
    writer.buffer = buffer;
    writer.capacity = sizeof(buffer);
    writer.length = 0u;
    writer.failed = 0;
    edge_gif_begin(&writer, 1, 2u, 1u);
    edge_gif_image(&writer, 0u, 0u, 2u, 1u, 0, blue, 2u, 1);
    edge_gif_graphic_control(&writer, 3u);
    edge_gif_image(&writer, 0u, 0u, 1u, 1u, 0, red, 1u, 1);
    edge_gif_image(&writer, 1u, 0u, 1u, 1u, 0, green, 1u, 1);
    edge_put_u8(&writer, 0x3bu);
    if (writer.failed != 0) {
        return 1;
    }
    return edge_expect_rgb("GIF disposal method 3",
                           buffer,
                           writer.length,
                           0,
                           2,
                           1,
                           3,
                           expected,
                           6u);
}
