/* SPDX-License-Identifier: MIT */
/* Observe paint-time pixels: a final indexed palette loses overwritten
 * register definitions. No quality metric can substitute for full coverage. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/output.h"
#include "src/sixel_decode_pixels.h"
#include "src/threading.h"

typedef struct hc_band_stream {
    unsigned char bytes[131072];
    size_t size;
} hc_band_stream_t;

static int
hc_band_write(char *data, int size, void *context)
{
    hc_band_stream_t *stream;

    stream = context;
    if (size < 0 || (size_t)size > sizeof(stream->bytes) - stream->size) {
        return -1;
    }
    memcpy(stream->bytes + stream->size, data, (size_t)size);
    stream->size += (size_t)size;
    return size;
}

int
test_highcolor_band(int scenario, int width, int height)
{
    sixel_allocator_t *allocator;
    sixel_dither_t *dither;
    sixel_output_t *output;
    sixel_output_options_t options;
    hc_band_stream_t *stream;
    unsigned char *source;
    unsigned char *decoded;
    unsigned char *palette;
    float *samples;
    unsigned int flags;
    int format;
    int previous_threads;
    int w;
    int h;
    int ncolors;
    int left;
    int top;
    int i;
    int x;
    int y;
    int c;
    int key;
    int value;
    int expected;
    int source_index;
    int ok;

    allocator = NULL;
    dither = NULL;
    output = NULL;
    source = decoded = palette = NULL;
    samples = NULL;
    stream = NULL;
    w = h = 0;
    ok = 0;
    previous_threads = sixel_threads_resolve();
    sixel_set_encoder_threads(scenario == 46 ? 4 : 1);
    left = scenario == 44 ? 3 : scenario == 45 ? 2 : 0;
    top = scenario == 44 || scenario == 49 ? 5 : scenario == 45 ? 11 : 0;
    format = scenario == 43 ? SIXEL_PIXELFORMAT_RGBFLOAT32 :
        SIXEL_PIXELFORMAT_RGB888;
    stream = calloc(1, sizeof(*stream));
    source = malloc((size_t)width * (size_t)height * 3U);
    samples = malloc((size_t)width * (size_t)height * 3U * sizeof(float));
    if (stream == NULL || source == NULL || samples == NULL) {
        goto end;
    }
    for (i = 0; i < width * height; ++i) {
        key = scenario == 42 ? i % 512 : i;
        for (c = 0; c < 3; ++c) {
            value = ((key >> (c * 5)) & 31) * 8 + 2;
            source[i * 3 + c] = (unsigned char)value;
            samples[i * 3 + c] = (float)value / 255.0f;
        }
    }
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL,
                NULL)) ||
            SIXEL_FAILED(sixel_dither_new(&dither, -1, allocator)) ||
            SIXEL_FAILED(sixel_output_new(&output, hc_band_write, stream,
                allocator))) {
        goto end;
    }
    sixel_dither_set_diffusion_type(dither, SIXEL_DIFFUSE_NONE);
    sixel_dither_set_pixelformat(dither, format);
    sixel_output_set_palette_type(output, SIXEL_PALETTETYPE_RGB);
    if (SIXEL_FAILED(sixel_output_get_encoder_options(output, &options))) {
        goto end;
    }
    options.transparent_offset_left = left;
    options.transparent_offset_top = top;
    if (SIXEL_FAILED(sixel_output_set_encoder_options(output, &options)) ||
            SIXEL_FAILED(sixel_output_set_encoder_format(output, format,
                SIXEL_COLORSPACE_GAMMA, SIXEL_COLORSPACE_GAMMA)) ||
            SIXEL_FAILED(sixel_encode(scenario == 43 ?
                (unsigned char *)samples : source, width, height, 0,
                dither, output)) ||
            SIXEL_FAILED(sixel_output_get_encoder_options(output, &options)) ||
            options.transparent_offset_top != top ||
            options.transparent_offset_left != left ||
            SIXEL_FAILED(sixel_decode_direct_with_options(stream->bytes,
                (int)stream->size, 0, &decoded, &w, &h, &palette, &ncolors,
                &flags, allocator))) {
        goto end;
    }
    if (w != width + left || h != height + top) {
        fprintf(stderr, "dimensions %dx%d expected %dx%d\n", w, h,
            width + left, height + top);
        goto end;
    }
    for (y = 0; y < h; ++y) {
        for (x = 0; x < w; ++x) {
            i = (y * w + x) * 4;
            if (x < left || y < top) {
                if (decoded[i + 3] != 0) {
                    fprintf(stderr, "paint in margin at %d,%d\n", x, y);
                    goto end;
                }
                continue;
            }
            if (decoded[i + 3] != 255) {
                fprintf(stderr, "unpainted pixel at %d,%d\n", x, y);
                goto end;
            }
            source_index = ((y - top) * width + x - left) * 3;
            for (c = 0; c < 3; ++c) {
                /* Independent wire rounding: byte -> percentage -> byte.
                 * Float input stores the same rational sample. Its closest
                 * percentage tie is > .0098 away; float storage and the
                 * identity transfer contribute < 1e-4 field units. */
                expected = (source[source_index + c] * 100 + 127) / 255;
                expected = (expected * 255 + 50) / 100;
                if (decoded[i + c] != expected) {
                    fprintf(stderr, "pixel %d,%d channel %d: %u want %d\n",
                        x, y, c, decoded[i + c], expected);
                    goto end;
                }
            }
        }
    }
    if (scenario != 47 &&
            (flags & SIXEL_DECODE_PIXELS_RESULT_PALETTE_REDEFINED) == 0U) {
        fprintf(stderr, "fixture did not redefine palette registers\n");
        goto end;
    }
    ok = 1;
end:
    if (!ok) {
        fprintf(stderr, "high-color band %d: %s\n", scenario,
            sixel_helper_get_additional_message());
    }
    sixel_allocator_free(allocator, decoded);
    sixel_allocator_free(allocator, palette);
    sixel_output_unref(output);
    sixel_dither_unref(dither);
    sixel_allocator_unref(allocator);
    free(stream);
    free(source);
    free(samples);
    sixel_set_encoder_threads(previous_threads);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
