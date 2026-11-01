/* Verify type-3 TGA gray samples expand to exact RGB bytes. */

#include "loader_builtin_tga_test_common.h"

int
test_loader_0101_loader_builtin_tga_grayscale8_numeric(
    int argc,
    char **argv)
{
    unsigned char buffer[64];
    unsigned char const expected[6] = {
        0x10u, 0x10u, 0x10u,
        0xe0u, 0xe0u, 0xe0u
    };
    edge_writer_t writer;

    (void)argc;
    (void)argv;
    writer.buffer = buffer;
    writer.capacity = sizeof(buffer);
    writer.length = 0u;
    writer.failed = 0;
    edge_tga_begin(&writer, 0u, 3u, 0u, 0u, 2u, 1u, 8u, 0x20u);
    edge_put_u8(&writer, 0x10u);
    edge_put_u8(&writer, 0xe0u);
    if (writer.failed != 0) {
        return 1;
    }
    return edge_expect_rgb("TGA type-3 8-bit grayscale",
                           buffer,
                           writer.length,
                           1,
                           2,
                           1,
                           1,
                           expected,
                           sizeof(expected));
}
