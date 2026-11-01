/* Verify a GIF image rectangle outside the logical screen is rejected. */

#include "loader_builtin_gif_test_common.h"

int
test_loader_0074_loader_builtin_gif_rectangle_oob_reject(
    int argc,
    char **argv)
{
    unsigned char buffer[160];
    unsigned char const pixels[2] = { 1u, 1u };
    edge_writer_t writer;

    (void)argc;
    (void)argv;
    writer.buffer = buffer;
    writer.capacity = sizeof(buffer);
    writer.length = 0u;
    writer.failed = 0;
    edge_gif_begin(&writer, 1, 2u, 1u);
    edge_gif_image(&writer, 1u, 0u, 2u, 1u, 0, pixels, 2u, 1);
    edge_put_u8(&writer, 0x3bu);
    if (writer.failed != 0) {
        return 1;
    }
    return edge_expect_failure("GIF out-of-bounds rectangle",
                               buffer,
                               writer.length);
}
