/* Verify GIF disposal method 2 clears the previous dirty rectangle. */

#include "loader_builtin_gif_test_common.h"

int
test_loader_0078_loader_builtin_gif_disposal2_numeric(int argc, char **argv)
{
    unsigned char buffer[256];
    unsigned char const red[1] = { 1u };
    unsigned char const green[1] = { 2u };
    unsigned char const expected[12] = {
        0xffu, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u,
        0x00u, 0xffu, 0x00u
    };
    edge_writer_t writer;

    (void)argc;
    (void)argv;
    writer.buffer = buffer;
    writer.capacity = sizeof(buffer);
    writer.length = 0u;
    writer.failed = 0;
    edge_gif_begin(&writer, 1, 2u, 1u);
    edge_gif_graphic_control(&writer, 2u);
    edge_gif_image(&writer, 0u, 0u, 1u, 1u, 0, red, 1u, 1);
    edge_gif_image(&writer, 1u, 0u, 1u, 1u, 0, green, 1u, 1);
    edge_put_u8(&writer, 0x3bu);
    if (writer.failed != 0) {
        return 1;
    }
    return edge_expect_rgb("GIF disposal method 2",
                           buffer,
                           writer.length,
                           0,
                           2,
                           1,
                           2,
                           expected,
                           6u);
}
