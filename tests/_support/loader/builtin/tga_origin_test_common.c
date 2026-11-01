/* SPDX-License-Identifier: MIT */
/* Fix logical palette indices independently of file offsets and PAL8 slots. */

#include <stdio.h>
#include <string.h>

#include "loader_builtin_tga_test_common.h"
#include "tga_origin_test_common.h"

int
tga_origin_test(int variant)
{
    static unsigned char const rgb[9] = {
        255u, 0u, 0u, 0u, 255u, 0u, 0u, 0u, 255u
    };
    static unsigned char const gray[3] = { 17u, 83u, 241u };
    static unsigned int const rgb16[3] = { 0x7c00u, 0x03e0u, 0x001fu };
    unsigned char data[128];
    unsigned char indices[4];
    unsigned char expected[12];
    unsigned char palette[9];
    edge_writer_t writer;
    edge_loader_options_t options;
    edge_frame_probe_t probe;
    SIXELSTATUS status;
    unsigned int origin;
    unsigned int palette_depth;
    unsigned int length;
    unsigned int index;
    unsigned int slot;
    int index16;
    int rle;
    int want_palette;
    int expect_palette;
    int i;
    int c;
    int result;

    origin = 7u;
    length = 3u;
    palette_depth = variant == 8 ? 32u : variant == 12 ? 8u :
        variant == 13 ? 16u : 24u;
    index16 = variant == 4 || variant == 5 || variant == 7;
    rle = variant == 2 || variant == 3 || variant == 5 || variant == 8;
    want_palette = variant == 1 || variant == 3 || variant == 6 ||
        variant == 8 || variant == 11 || variant == 12 || variant == 13;
    expect_palette = want_palette && variant != 11;
    if (index16) {
        origin = 342u;
    }
    if (variant == 6 || variant == 7) {
        origin = variant == 6 ? 254u : 65534u;
        length = 2u;
    }
    indices[0] = 0u;
    indices[1] = 2u;
    indices[2] = 1u;
    indices[3] = rle ? 1u : 0u;
    if (length == 2u) {
        indices[0] = 1u;
        indices[1] = 0u;
        indices[2] = 1u;
        indices[3] = 0u;
    }
    writer.buffer = data;
    writer.capacity = sizeof(data);
    writer.length = 0u;
    writer.failed = 0;
    edge_tga_begin(&writer, 1u, rle ? 9u : 1u, length, palette_depth,
                    2u, 2u, index16 ? 16u : 8u, 0x20u);
    data[0] = 3u;
    data[3] = (unsigned char)(origin & 255u);
    data[4] = (unsigned char)(origin >> 8);
    /* Only the ID length advances the palette's physical file offset. */
    edge_put_bytes(&writer, (unsigned char const *)"ID!", 3u);
    memcpy(palette, rgb, sizeof(palette));
    for (i = 0; i < (int)length; ++i) {
        if (palette_depth == 8u) {
            edge_put_u8(&writer, gray[i]);
            memset(palette + i * 3, gray[i], 3u);
        } else if (palette_depth == 16u) {
            edge_put_u16le(&writer, rgb16[i]);
        } else {
            edge_put_u8(&writer, rgb[i * 3 + 2]);
            edge_put_u8(&writer, rgb[i * 3 + 1]);
            edge_put_u8(&writer, rgb[i * 3]);
            if (palette_depth == 32u) {
                edge_put_u8(&writer, i == 1 ? 0u : 255u);
                if (i == 1) {
                    memset(palette + 3, 0, 3u);
                }
            }
        }
    }
    for (i = 0; i < 4; ++i) {
        if (rle && i == 3) {
            break;
        }
        if (rle && i == 0) {
            edge_put_u8(&writer, 1u); /* Two distinct indices. */
        } else if (rle && i == 2) {
            edge_put_u8(&writer, 129u); /* Two repetitions of one index. */
        }
        index = origin + indices[i];
        if ((variant == 9 || variant == 11) && i == 0) {
            index = origin - 1u;
        } else if (variant == 10 && i == 1) {
            index = origin + length;
            indices[i] = 0u; /* The fallback uses the first stored entry. */
        }
        if (index16) {
            edge_put_u16le(&writer, index);
        } else {
            edge_put_u8(&writer, index);
        }
    }
    if (writer.failed) {
        return 1;
    }
    for (i = 0; i < 4; ++i) {
        slot = indices[i];
        for (c = 0; c < 3; ++c) {
            expected[i * 3 + c] = palette[slot * 3u + (unsigned int)c];
        }
    }
    edge_loader_options_init(&options);
    options.require_static = 1;
    options.use_palette = want_palette;
    memset(&probe, 0, sizeof(probe));
    status = SIXEL_FALSE;
    result = edge_load_buffer_options("TGA colormap origin", data,
                                       writer.length, &options, &probe,
                                       &status);
    if (result != 0 || SIXEL_FAILED(status) || probe.callback_count != 1 ||
        probe.width[0] != 2 || probe.height[0] != 2 ||
        probe.colorspace[0] != SIXEL_COLORSPACE_GAMMA ||
        probe.pixelformat[0] != (expect_palette ? SIXEL_PIXELFORMAT_PAL8 :
                                                SIXEL_PIXELFORMAT_RGB888)) {
        fprintf(stderr, "TGA origin variant %d: load/type failed (%d)\n",
                variant, (int)status);
        return 1;
    }
    if (expect_palette) {
        if (probe.ncolors[0] != (int)length || probe.rgb_size[0] != 4u ||
            memcmp(probe.rgb[0], indices, 4u) != 0 ||
            probe.palette_size[0] != length * 3u ||
            memcmp(probe.palette[0], palette, length * 3u) != 0 ||
            probe.transparent[0] != (variant == 8 ? 1 : -1)) {
            fprintf(stderr, "TGA origin: PAL8 slots/palette/key mismatch\n");
            return 1;
        }
    } else if (probe.rgb_size[0] != sizeof(expected) ||
               memcmp(probe.rgb[0], expected, sizeof(expected)) != 0) {
        fprintf(stderr, "TGA origin: exact RGB pixels mismatch\n");
        return 1;
    }
    return 0;
}
