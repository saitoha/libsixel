/* Verify the PIC mixed-RLE 16-bit extended repeat count. */

#include <stdio.h>
#include <string.h>

#include "loader_builtin_pic_test_common.h"

int
test_loader_0099_loader_builtin_pic_mixed_rle_extended_numeric(
    int argc,
    char **argv)
{
    unsigned char buffer[160];
    edge_frame_probe_t probe;
    SIXELSTATUS status;
    edge_writer_t writer;
    size_t index;
    int result;

    (void)argc;
    (void)argv;
    writer.buffer = buffer;
    writer.capacity = sizeof(buffer);
    writer.length = 0u;
    writer.failed = 0;
    memset(&probe, 0, sizeof(probe));
    status = SIXEL_FALSE;
    index = 0u;
    result = 1;
    edge_pic_begin(&writer, 130u, 1u);
    edge_pic_packet(&writer, 0, 2u, 0xe0u);
    edge_put_u8(&writer, 128u);
    edge_put_u16be(&writer, 130u);
    edge_put_u8(&writer, 0x12u);
    edge_put_u8(&writer, 0x34u);
    edge_put_u8(&writer, 0x56u);
    if (writer.failed != 0) {
        return 1;
    }
    result = edge_load_buffer("PIC mixed RLE extended repeat count",
                              buffer,
                              writer.length,
                              1,
                              &probe,
                              &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status) || probe.callback_count != 1 ||
        probe.width[0] != 130 || probe.height[0] != 1) {
        fprintf(stderr, "PIC mixed RLE extended count: metadata mismatch\n");
        return 1;
    }
    for (index = 0u; index < 130u; ++index) {
        if (probe.rgb[0][index * 3u] != 0x12u ||
            probe.rgb[0][index * 3u + 1u] != 0x34u ||
            probe.rgb[0][index * 3u + 2u] != 0x56u) {
            fprintf(stderr,
                    "PIC mixed RLE extended count: pixel %zu mismatch\n",
                    index);
            return 1;
        }
    }
    return 0;
}
