/* Verify GIF extension sub-blocks preserve the next image boundary. */

#include "loader_builtin_gif_test_common.h"

int
test_loader_0075_loader_builtin_gif_extension_subblocks_numeric(
    int argc,
    char **argv)
{
    static unsigned char const comment[] = {
        0x21u, 0xfeu, 3u, 'a', 'b', 'c', 2u, 'd', 'e', 0u
    };
    static unsigned char const plain_text[] = {
        0x21u, 0x01u, 12u,
        0u, 0u, 0u, 0u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 0u,
        1u, 'x', 0u
    };
    static unsigned char const application[] = {
        0x21u, 0xffu, 11u,
        'E', 'D', 'G', 'E', 'T', 'E', 'S', 'T', '0', '0', '1',
        2u, 0xaau, 0x55u, 0u
    };
    unsigned char buffer[256];
    unsigned char const pixels[1] = { 2u };
    unsigned char const expected[3] = { 0u, 0xffu, 0u };
    edge_writer_t writer;

    (void)argc;
    (void)argv;
    writer.buffer = buffer;
    writer.capacity = sizeof(buffer);
    writer.length = 0u;
    writer.failed = 0;
    edge_gif_begin(&writer, 1, 1u, 1u);
    edge_put_bytes(&writer, comment, sizeof(comment));
    edge_put_bytes(&writer, plain_text, sizeof(plain_text));
    edge_put_bytes(&writer, application, sizeof(application));
    edge_gif_image(&writer, 0u, 0u, 1u, 1u, 0, pixels, 1u, 1);
    edge_put_u8(&writer, 0x3bu);
    if (writer.failed != 0) {
        return 1;
    }
    return edge_expect_rgb("GIF extension sub-block skipping",
                           buffer,
                           writer.length,
                           1,
                           1,
                           1,
                           1,
                           expected,
                           sizeof(expected));
}
