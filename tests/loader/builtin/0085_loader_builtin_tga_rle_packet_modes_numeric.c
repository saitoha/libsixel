/* Verify raw and repeated TGA RLE packets preserve exact pixel order. */

#include "loader_builtin_tga_test_common.h"

int
test_loader_0085_loader_builtin_tga_rle_packet_modes_numeric(
    int argc,
    char **argv)
{
    unsigned char buffer[64];
    unsigned char const expected[9] = {
        0xffu, 0x00u, 0x00u,
        0x00u, 0xffu, 0x00u,
        0x00u, 0x00u, 0xffu
    };
    edge_writer_t writer;

    (void)argc;
    (void)argv;
    writer.buffer = buffer;
    writer.capacity = sizeof(buffer);
    writer.length = 0u;
    writer.failed = 0;
    edge_tga_begin(&writer, 0u, 10u, 0u, 0u, 3u, 1u, 24u, 0x20u);
    edge_put_u8(&writer, 1u);
    edge_put_u8(&writer, 0u);
    edge_put_u8(&writer, 0u);
    edge_put_u8(&writer, 0xffu);
    edge_put_u8(&writer, 0u);
    edge_put_u8(&writer, 0xffu);
    edge_put_u8(&writer, 0u);
    edge_put_u8(&writer, 0x80u);
    edge_put_u8(&writer, 0xffu);
    edge_put_u8(&writer, 0u);
    edge_put_u8(&writer, 0u);
    if (writer.failed != 0) {
        return 1;
    }
    return edge_expect_rgb("TGA raw and repeated RLE packets",
                           buffer,
                           writer.length,
                           1,
                           3,
                           1,
                           1,
                           expected,
                           sizeof(expected));
}
