/* Verify legacy Radiance stream RLE before modern scanline dispatch. */

#include <stdio.h>
#include <string.h>

#include "loader_builtin_memory_test_common.h"

int
test_loader_0107_loader_builtin_hdr_legacy_stream_numeric(
    int argc,
    char **argv)
{
    unsigned char buffer[256];
    unsigned char const header[] =
        "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 1 +X 4\n";
    unsigned char const payload[8] = {
        64u, 32u, 16u, 129u, 1u, 1u, 1u, 3u
    };
    edge_writer_t writer;
    edge_frame_probe_t probe;
    SIXELSTATUS status;
    float samples[12];
    size_t index;

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
    if (writer.failed != 0 ||
        edge_load_buffer("HDR legacy stream RLE",
                         buffer,
                         writer.length,
                         1,
                         &probe,
                         &status) != 0 ||
        SIXEL_FAILED(status)) {
        return 1;
    }
    if (probe.callback_count != 1 || probe.width[0] != 4 ||
        probe.height[0] != 1 ||
        probe.pixelformat[0] != SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 ||
        probe.colorspace[0] != SIXEL_COLORSPACE_LINEAR ||
        probe.rgb_size[0] != sizeof(samples)) {
        return 1;
    }
    memcpy(samples, probe.rgb[0], sizeof(samples));
    for (index = 0u; index < 4u; ++index) {
        if (samples[index * 3u] != 0.5f ||
            samples[index * 3u + 1u] != 0.25f ||
            samples[index * 3u + 2u] != 0.125f) {
            fprintf(stderr, "HDR legacy stream sample mismatch\n");
            return 1;
        }
    }
    return 0;
}
