/* Verify PIC raw RGB packets populate exact component lanes. */

#include "loader_builtin_pic_test_common.h"

int
test_loader_0098_loader_builtin_pic_raw_rgb_numeric(int argc, char **argv)
{
    unsigned char buffer[160];
    unsigned char const expected[6] = {
        0x10u, 0x20u, 0x30u,
        0x40u, 0x50u, 0x60u
    };
    edge_writer_t writer;

    (void)argc;
    (void)argv;
    writer.buffer = buffer;
    writer.capacity = sizeof(buffer);
    writer.length = 0u;
    writer.failed = 0;
    edge_pic_begin(&writer, 2u, 1u);
    edge_pic_packet(&writer, 0, 0u, 0xe0u);
    edge_put_u8(&writer, 0x10u);
    edge_put_u8(&writer, 0x20u);
    edge_put_u8(&writer, 0x30u);
    edge_put_u8(&writer, 0x40u);
    edge_put_u8(&writer, 0x50u);
    edge_put_u8(&writer, 0x60u);
    if (writer.failed != 0) {
        return 1;
    }
    return edge_expect_rgb("PIC raw RGB component lanes",
                           buffer,
                           writer.length,
                           1,
                           2,
                           1,
                           1,
                           expected,
                           sizeof(expected));
}
