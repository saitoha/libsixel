/* Verify the PIC mixed-RLE maximum short-form raw count. */

#include <stdio.h>
#include <string.h>

#include "loader_builtin_pic_test_common.h"

int
test_loader_0091_loader_builtin_pic_mixed_rle_raw128_numeric(
    int argc,
    char **argv)
{
    unsigned char buffer[512];
    edge_writer_t writer;
    edge_frame_probe_t probe;
    SIXELSTATUS status;
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
    edge_pic_begin(&writer, 128u, 1u);
    edge_pic_packet(&writer, 0, 2u, 0x80u);
    edge_put_u8(&writer, 127u);
    for (index = 0u; index < 128u; ++index) {
        edge_put_u8(&writer, (unsigned int)index);
    }
    if (writer.failed != 0) {
        return 1;
    }
    result = edge_load_buffer("PIC mixed RLE raw count 128",
                              buffer,
                              writer.length,
                              1,
                              &probe,
                              &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status) || probe.callback_count != 1 ||
        probe.width[0] != 128 || probe.height[0] != 1) {
        fprintf(stderr, "PIC mixed RLE raw count 128: metadata mismatch\n");
        return 1;
    }
    for (index = 0u; index < 128u; ++index) {
        if (probe.rgb[0][index * 3u] != (unsigned char)index ||
            probe.rgb[0][index * 3u + 1u] != 0xffu ||
            probe.rgb[0][index * 3u + 2u] != 0xffu) {
            fprintf(stderr,
                    "PIC mixed RLE raw count 128: pixel %zu mismatch\n",
                    index);
            return 1;
        }
    }
    return 0;
}
