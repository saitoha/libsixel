/* Verify a truncated declared TGA palette is rejected before callback. */

#include "loader_builtin_tga_test_common.h"

int
test_loader_0086_loader_builtin_tga_truncated_palette_reject(
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
    edge_tga_begin(&writer, 1u, 1u, 2u, 24u, 1u, 1u, 8u, 0x20u);
    edge_put_u8(&writer, 0u);
    edge_put_u8(&writer, 0u);
    edge_put_u8(&writer, 0u);
    if (writer.failed != 0) {
        return 1;
    }
    return edge_expect_failure("TGA truncated palette",
                               buffer,
                               writer.length);
}
