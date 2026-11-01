/* Verify type-2 TGA BGR bytes expand to exact RGB component lanes. */

#include "loader_builtin_tga_test_common.h"

int
test_loader_0100_loader_builtin_tga_truecolor24_numeric(
    int argc,
    char **argv)
{
    unsigned char buffer[64];
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
    edge_tga_begin(&writer, 0u, 2u, 0u, 0u, 2u, 1u, 24u, 0x20u);
    edge_put_u8(&writer, 0x00u);
    edge_put_u8(&writer, 0x00u);
    edge_put_u8(&writer, 0xffu);
    edge_put_u8(&writer, 0x00u);
    edge_put_u8(&writer, 0xffu);
    edge_put_u8(&writer, 0x00u);
    if (writer.failed != 0) {
        return 1;
    }
    return edge_expect_rgb("TGA type-2 24-bit component lanes",
                           buffer,
                           writer.length,
                           1,
                           2,
                           1,
                           1,
                           expected,
                           sizeof(expected));
}
