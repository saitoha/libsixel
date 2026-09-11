/* Verify pure PIC RLE clips an oversized run to the scanline. */

#include "loader_builtin_pic_test_common.h"

int
test_loader_0090_loader_builtin_pic_pure_rle_oversized_clip_numeric(
    int argc,
    char **argv)
{
    unsigned char buffer[160];
    unsigned char const expected[6] = {
        0x44u, 0xffu, 0xffu,
        0x44u, 0xffu, 0xffu
    };
    edge_writer_t writer;

    (void)argc;
    (void)argv;
    writer.buffer = buffer;
    writer.capacity = sizeof(buffer);
    writer.length = 0u;
    writer.failed = 0;
    edge_pic_begin(&writer, 2u, 1u);
    edge_pic_packet(&writer, 0, 1u, 0x80u);
    edge_put_u8(&writer, 0xffu);
    edge_put_u8(&writer, 0x44u);
    if (writer.failed != 0) {
        return 1;
    }
    return edge_expect_rgb("PIC pure RLE oversized run clipping",
                           buffer,
                           writer.length,
                           1,
                           2,
                           1,
                           1,
                           expected,
                           sizeof(expected));
}
