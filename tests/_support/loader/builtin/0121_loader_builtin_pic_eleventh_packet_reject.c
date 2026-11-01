/* Verify an eleventh chained PIC packet is rejected. */

#include "loader_builtin_pic_test_common.h"

int
test_loader_0121_loader_builtin_pic_eleventh_packet_reject(
    int argc,
    char **argv)
{
    unsigned char buffer[192];
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
    for (packet_index = 0u; packet_index < 11u; ++packet_index) {
        edge_pic_packet(&writer,
                        packet_index + 1u < 11u,
                        0u,
                        0x80u);
    }
    if (writer.failed != 0) {
        return 1;
    }
    return edge_expect_failure("PIC eleventh packet",
                               buffer,
                               writer.length);
}
