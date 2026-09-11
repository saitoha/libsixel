/* Verify the later PIC packet wins when it writes the same channel. */

#include "loader_builtin_pic_test_common.h"

int
test_loader_0089_pic_repeated_channel_numeric(
    int argc,
    char **argv)
{
    unsigned char buffer[160];
    unsigned char const expected[6] = {
        0x30u, 0xffu, 0xffu,
        0x40u, 0xffu, 0xffu
    };
    edge_writer_t writer;

    (void)argc;
    (void)argv;
    writer.buffer = buffer;
    writer.capacity = sizeof(buffer);
    writer.length = 0u;
    writer.failed = 0;
    edge_pic_begin(&writer, 2u, 1u);
    edge_pic_packet(&writer, 1, 0u, 0x80u);
    edge_pic_packet(&writer, 0, 0u, 0x80u);
    edge_put_u8(&writer, 0x10u);
    edge_put_u8(&writer, 0x20u);
    edge_put_u8(&writer, 0x30u);
    edge_put_u8(&writer, 0x40u);
    if (writer.failed != 0) {
        return 1;
    }
    return edge_expect_rgb("PIC repeated channel packet last write wins",
                           buffer,
                           writer.length,
                           1,
                           2,
                           1,
                           1,
                           expected,
                           sizeof(expected));
}
