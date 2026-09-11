/* Verify GIF87a recognition and exact pixels with an in-memory stream. */

#include "loader_builtin_gif_test_common.h"

int
test_loader_0071_loader_builtin_gif87a_numeric(int argc, char **argv)
{
    unsigned char buffer[128];
    unsigned char const pixels[2] = { 1u, 2u };
    unsigned char const expected[6] = {
        0xffu, 0x00u, 0x00u,
        0x00u, 0xffu, 0x00u
    };
    edge_writer_t writer;

    (void)argc;
    (void)argv;
    writer.buffer = buffer;
    writer.capacity = sizeof(buffer);
    writer.length = 0u;
    writer.failed = 0;
    edge_gif_begin(&writer, 0, 2u, 1u);
    edge_gif_image(&writer, 0u, 0u, 2u, 1u, 0, pixels, 2u, 1);
    edge_put_u8(&writer, 0x3bu);
    if (writer.failed != 0) {
        return 1;
    }
    return edge_expect_rgb("GIF87a decode",
                           buffer,
                           writer.length,
                           1,
                           2,
                           1,
                           1,
                           expected,
                           sizeof(expected));
}
