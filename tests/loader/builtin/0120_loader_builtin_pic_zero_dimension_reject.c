/* Verify PIC rejects zero-width and zero-height canvases. */

#include "loader_builtin_pic_test_common.h"

int
test_loader_0120_loader_builtin_pic_zero_dimension_reject(
    int argc,
    char **argv)
{
    unsigned char zero_width[128];
    unsigned char zero_height[128];
    edge_writer_t writer;

    (void)argc;
    (void)argv;
    writer.buffer = zero_width;
    writer.capacity = sizeof(zero_width);
    writer.length = 0u;
    writer.failed = 0;
    edge_pic_begin(&writer, 0u, 1u);
    edge_pic_packet(&writer, 0, 0u, 0xe0u);
    if (writer.failed != 0 ||
        edge_expect_failure("PIC zero width",
                            zero_width,
                            writer.length) != 0) {
        return 1;
    }
    writer.buffer = zero_height;
    writer.capacity = sizeof(zero_height);
    writer.length = 0u;
    writer.failed = 0;
    edge_pic_begin(&writer, 1u, 0u);
    edge_pic_packet(&writer, 0, 0u, 0xe0u);
    if (writer.failed != 0) {
        return 1;
    }
    return edge_expect_failure("PIC zero height",
                               zero_height,
                               writer.length);
}
