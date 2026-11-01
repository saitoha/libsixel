/* Verify a TGA RLE packet cannot exceed the remaining raster. */

#include "loader_builtin_tga_test_common.h"

int
test_loader_0123_loader_builtin_tga_rle_overrun_reject(
    int argc,
    char **argv)
{
    unsigned char buffer[64];
    edge_writer_t writer;

    (void)argc;
    (void)argv;
    writer.buffer = buffer;
    writer.capacity = sizeof(buffer);
    writer.length = 0u;
    writer.failed = 0;
    edge_tga_begin(&writer, 0u, 10u, 0u, 0u, 2u, 1u, 24u, 0x20u);
    edge_put_u8(&writer, 0x82u);
    edge_put_u8(&writer, 0u);
    edge_put_u8(&writer, 0u);
    edge_put_u8(&writer, 255u);
    if (writer.failed != 0) {
        return 1;
    }
    return edge_expect_failure("TGA RLE raster overrun",
                               buffer,
                               writer.length);
}
