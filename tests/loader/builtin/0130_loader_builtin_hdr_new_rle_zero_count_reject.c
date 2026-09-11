/* Verify Radiance component RLE rejects a zero-length packet. */

#include <string.h>

#include "loader_builtin_memory_test_common.h"

int
test_loader_0130_loader_builtin_hdr_new_rle_zero_count_reject(
    int argc,
    char **argv)
{
    unsigned char buffer[128];
    unsigned char const header[] =
        "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 1 +X 8\n";
    unsigned char const payload[5] = { 2u, 2u, 0u, 8u, 0u };
    edge_writer_t writer;
    edge_frame_probe_t probe;
    SIXELSTATUS status;
    int result;

    (void)argc;
    (void)argv;
    writer.buffer = buffer;
    writer.capacity = sizeof(buffer);
    writer.length = 0u;
    writer.failed = 0;
    memset(&probe, 0, sizeof(probe));
    status = SIXEL_FALSE;
    edge_put_bytes(&writer, header, sizeof(header) - 1u);
    edge_put_bytes(&writer, payload, sizeof(payload));
    result = edge_load_buffer("HDR zero component RLE count",
                              buffer,
                              writer.length,
                              1,
                              &probe,
                              &status);
    if (writer.failed != 0 || result != 0 ||
        status != SIXEL_STBI_ERROR || probe.callback_count != 0) {
        return 1;
    }
    return 0;
}
