/* Verify a 16-bit TGA palette index selects the declared entry. */

#include "loader_builtin_tga_test_common.h"

int
test_loader_0083_loader_builtin_tga_index16_numeric(int argc, char **argv)
{
    unsigned char buffer[64];
    unsigned char const palette[6] = {
        0u, 0u, 0u,
        0u, 0u, 0xffu
    };
    unsigned char const expected[3] = { 0xffu, 0u, 0u };
    edge_writer_t writer;

    (void)argc;
    (void)argv;
    writer.buffer = buffer;
    writer.capacity = sizeof(buffer);
    writer.length = 0u;
    writer.failed = 0;
    edge_tga_begin(&writer, 1u, 1u, 2u, 24u, 1u, 1u, 16u, 0x20u);
    edge_put_bytes(&writer, palette, sizeof(palette));
    edge_put_u16le(&writer, 1u);
    if (writer.failed != 0) {
        return 1;
    }
    return edge_expect_rgb("TGA 16-bit palette index",
                           buffer,
                           writer.length,
                           1,
                           1,
                           1,
                           1,
                           expected,
                           sizeof(expected));
}
