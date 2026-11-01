/* Verify a truncated GIF raster sub-block is rejected before callback. */

#include "loader_builtin_gif_test_common.h"

int
test_loader_0076_loader_builtin_gif_truncated_raster_reject(
    int argc,
    char **argv)
{
    unsigned char buffer[128];
    edge_writer_t writer;

    (void)argc;
    (void)argv;
    writer.buffer = buffer;
    writer.capacity = sizeof(buffer);
    writer.length = 0u;
    writer.failed = 0;
    edge_gif_begin(&writer, 1, 1u, 1u);
    edge_put_u8(&writer, 0x2cu);
    edge_put_u16le(&writer, 0u);
    edge_put_u16le(&writer, 0u);
    edge_put_u16le(&writer, 1u);
    edge_put_u16le(&writer, 1u);
    edge_put_u8(&writer, 0u);
    edge_put_u8(&writer, 2u);
    edge_put_u8(&writer, 2u);
    edge_put_u8(&writer, 0x44u);
    if (writer.failed != 0) {
        return 1;
    }
    return edge_expect_failure("GIF truncated raster sub-block",
                               buffer,
                               writer.length);
}
