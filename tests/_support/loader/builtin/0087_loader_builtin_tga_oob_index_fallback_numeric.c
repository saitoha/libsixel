/* Verify an out-of-range TGA palette index uses entry zero. */

#include "loader_builtin_tga_test_common.h"

int
test_loader_0087_loader_builtin_tga_oob_index_fallback_numeric(
    int argc,
    char **argv)
{
    unsigned char buffer[64];
    unsigned char const expected[3] = { 0u, 0u, 0u };
    edge_writer_t writer;

    (void)argc;
    (void)argv;
    writer.buffer = buffer;
    writer.capacity = sizeof(buffer);
    writer.length = 0u;
    writer.failed = 0;
    edge_tga_begin(&writer, 1u, 1u, 1u, 24u, 1u, 1u, 8u, 0x20u);
    edge_put_u8(&writer, 0u);
    edge_put_u8(&writer, 0u);
    edge_put_u8(&writer, 0u);
    edge_put_u8(&writer, 1u);
    if (writer.failed != 0) {
        return 1;
    }
    return edge_expect_rgb("TGA out-of-range index compatibility fallback",
                           buffer,
                           writer.length,
                           1,
                           1,
                           1,
                           1,
                           expected,
                           sizeof(expected));
}
