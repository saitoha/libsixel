/* Verify a PIC descriptor table accepts exactly ten packets. */

#include "loader_builtin_pic_test_common.h"

int
test_loader_0093_loader_builtin_pic_ten_packet_limit_numeric(
    int argc,
    char **argv)
{
    unsigned char buffer[192];
    unsigned char const expected[3] = { 9u, 0xffu, 0xffu };
    edge_writer_t writer;
    unsigned int packet_index;

    (void)argc;
    (void)argv;
    writer.buffer = buffer;
    writer.capacity = sizeof(buffer);
    writer.length = 0u;
    writer.failed = 0;
    packet_index = 0u;
    edge_pic_begin(&writer, 1u, 1u);
    for (packet_index = 0u; packet_index < 10u; ++packet_index) {
        edge_pic_packet(&writer,
                        packet_index + 1u < 10u,
                        0u,
                        0x80u);
    }
    for (packet_index = 0u; packet_index < 10u; ++packet_index) {
        edge_put_u8(&writer, packet_index);
    }
    if (writer.failed != 0) {
        return 1;
    }
    return edge_expect_rgb("PIC ten-packet upper bound",
                           buffer,
                           writer.length,
                           1,
                           1,
                           1,
                           1,
                           expected,
                           sizeof(expected));
}
