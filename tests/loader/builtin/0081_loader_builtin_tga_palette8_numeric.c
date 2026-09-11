/* Verify an 8-bit TGA palette entry expands through the grayscale path. */

#include "loader_builtin_tga_test_common.h"

int
test_loader_0081_loader_builtin_tga_palette8_numeric(int argc, char **argv)
{
    unsigned char buffer[64];
    unsigned char const expected[3] = { 0x7fu, 0x7fu, 0x7fu };
    edge_writer_t writer;

    (void)argc;
    (void)argv;
    writer.buffer = buffer;
    writer.capacity = sizeof(buffer);
    writer.length = 0u;
    writer.failed = 0;
    edge_tga_begin(&writer, 1u, 1u, 1u, 8u, 1u, 1u, 8u, 0x20u);
    edge_put_u8(&writer, 0x7fu);
    edge_put_u8(&writer, 0u);
    if (writer.failed != 0) {
        return 1;
    }
    return edge_expect_rgb("TGA 8-bit grayscale palette entry",
                           buffer,
                           writer.length,
                           1,
                           1,
                           1,
                           1,
                           expected,
                           sizeof(expected));
}
