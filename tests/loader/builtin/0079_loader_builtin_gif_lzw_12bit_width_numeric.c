/* Verify GIF LZW dictionary growth through the 12-bit code width. */

#include <stdio.h>
#include <string.h>

#include "loader_builtin_gif_test_common.h"

int
test_loader_0079_loader_builtin_gif_lzw_12bit_width_numeric(
    int argc,
    char **argv)
{
    unsigned char buffer[EDGE_BUFFER_CAPACITY];
    unsigned char pixels[4090];
    edge_writer_t writer;
    edge_frame_probe_t probe;
    SIXELSTATUS status;
    size_t index;
    size_t rgb_offset;
    unsigned int palette_index;
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
    rgb_offset = 0u;
    palette_index = 0u;
    result = 1;
    for (index = 0u; index < sizeof(pixels); ++index) {
        pixels[index] = (unsigned char)(index & 3u);
    }
    edge_gif_begin(&writer, 1, 4090u, 1u);
    edge_gif_image(&writer,
                   0u,
                   0u,
                   4090u,
                   1u,
                   0,
                   pixels,
                   sizeof(pixels),
                   0);
    edge_put_u8(&writer, 0x3bu);
    if (writer.failed != 0) {
        return 1;
    }
    result = edge_load_buffer("GIF LZW 12-bit code width",
                              buffer,
                              writer.length,
                              1,
                              &probe,
                              &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status) || probe.callback_count != 1 ||
        probe.width[0] != 4090 || probe.height[0] != 1 ||
        probe.rgb_size[0] != 4090u * 3u) {
        fprintf(stderr, "GIF LZW 12-bit code width: metadata mismatch\n");
        return 1;
    }
    for (index = 0u; index < sizeof(pixels); ++index) {
        palette_index = (unsigned int)pixels[index];
        rgb_offset = index * 3u;
        if (memcmp(probe.rgb[0] + rgb_offset,
                   edge_palette_rgb + palette_index * 3u,
                   3u) != 0) {
            fprintf(stderr,
                    "GIF LZW 12-bit code width: pixel %zu mismatch\n",
                    index);
            return 1;
        }
    }
    return 0;
}
