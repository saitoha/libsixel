/* Verify Radiance component RLE produces exact linear float samples. */

#include <stdio.h>
#include <string.h>

#include "loader_builtin_memory_test_common.h"

int
test_loader_0105_loader_builtin_hdr_new_rle_numeric(
    int argc,
    char **argv)
{
    unsigned char buffer[256];
    unsigned char const header[] =
        "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 1 +X 8\n";
    unsigned char const scanline[12] = {
        2u, 2u, 0u, 8u,
        136u, 64u, 136u, 32u, 136u, 16u, 136u, 129u
    };
    edge_writer_t writer;
    edge_frame_probe_t probe;
    SIXELSTATUS status;
    float samples[24];
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
    edge_put_bytes(&writer, scanline, sizeof(scanline));
    if (writer.failed != 0 ||
        edge_load_buffer("HDR component RLE",
                         buffer,
                         writer.length,
                         1,
                         &probe,
                         &status) != 0 ||
        SIXEL_FAILED(status)) {
        return 1;
    }
    if (probe.callback_count != 1 || probe.width[0] != 8 ||
        probe.height[0] != 1 ||
        probe.pixelformat[0] != SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 ||
        probe.colorspace[0] != SIXEL_COLORSPACE_LINEAR ||
        probe.rgb_size[0] != sizeof(samples)) {
        return 1;
    }
    memcpy(samples, probe.rgb[0], sizeof(samples));
    for (index = 0u; index < 8u; ++index) {
        if (samples[index * 3u] != 0.5f ||
            samples[index * 3u + 1u] != 0.25f ||
            samples[index * 3u + 2u] != 0.125f) {
            fprintf(stderr, "HDR component RLE sample mismatch\n");
            return 1;
        }
    }
    return 0;
}
