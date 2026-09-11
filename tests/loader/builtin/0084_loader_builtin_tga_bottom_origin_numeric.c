/* Verify bottom-origin TGA rows are normalized to top-to-bottom order. */

#include "loader_builtin_tga_test_common.h"

int
test_loader_0084_loader_builtin_tga_bottom_origin_numeric(
    int argc,
    char **argv)
{
    unsigned char buffer[64];
    unsigned char const file_pixels[6] = {
        0xffu, 0x00u, 0x00u,
        0x00u, 0x00u, 0xffu
    };
    unsigned char const expected[6] = {
        0xffu, 0x00u, 0x00u,
        0x00u, 0x00u, 0xffu
    };
    edge_writer_t writer;

    (void)argc;
    (void)argv;
    writer.buffer = buffer;
    writer.capacity = sizeof(buffer);
    writer.length = 0u;
    writer.failed = 0;
    edge_tga_begin(&writer, 0u, 2u, 0u, 0u, 1u, 2u, 24u, 0u);
    edge_put_bytes(&writer, file_pixels, sizeof(file_pixels));
    if (writer.failed != 0) {
        return 1;
    }
    return edge_expect_rgb("TGA bottom-origin row order",
                           buffer,
                           writer.length,
                           1,
                           1,
                           2,
                           1,
                           expected,
                           sizeof(expected));
}
