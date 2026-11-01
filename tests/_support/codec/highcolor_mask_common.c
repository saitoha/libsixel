/* SPDX-License-Identifier: MIT */
/* Independent RGB101 paint and omission checks for high-color source masks. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/dither.h"
#include "src/encoder.h"
#include "src/encoder-core-private.h"
#include "src/output.h"
#include "src/sixel_decode_pixels.h"

typedef struct hcm_stream {
    unsigned char bytes[131072];
    size_t size;
} hcm_stream_t;

static int
hcm_write(char *data, int size, void *context)
{
    hcm_stream_t *stream;

    stream = context;
    if (size < 0 || (size_t)size >= sizeof(stream->bytes) - stream->size) {
        return -1;
    }
    memcpy(stream->bytes + stream->size, data, (size_t)size);
    stream->size += (size_t)size;
    stream->bytes[stream->size] = 0;
    return size;
}

static int
hcm_omitted(int scenario, int phase, int index)
{
    int omitted;

    if (scenario == 62) {
        return 1;
    }
    if (scenario == 63 ||
            ((scenario == 64 || scenario == 65) && phase == 1)) {
        return 0;
    }
    if (scenario == 61) {
        return index == 0;
    }
    omitted = index % 7 == 1 || index % 11 == 4;
    return scenario == 64 && phase == 2 ? !omitted : omitted;
}

int
test_highcolor_mask(int scenario)
{
    /* Analytic D65 matrix: linear Display P3 -> linear sRGB. This is fixture
     * construction, not the production CMS. RGB101 reference fields come
     * from device samples. Their distance to a percentage tie exceeds .0098,
     * compared with a 1e-3 field budget for float storage and matrix/transfer
     * arithmetic. The hidden-key witness has >= .2 field units of margin. */
    static double const matrix[3][3] = {
        {1.2249401762806, -.22494017628056, 0},
        {-.042056954709688, 1.0420569547097, 0},
        {-.019637554590334, -.078636045550632, 1.098273600140966}
    };
    sixel_encoder_t *encoder;
    sixel_dither_t *dither;
    sixel_output_t *output;
    sixel_frame_t *frame;
    sixel_frame_interface_t *iface;
    sixel_frame_transparency_t transparency;
    sixel_allocator_t *allocator;
    hcm_stream_t *stream;
    unsigned char bytes[64 * 18 * 3];
    float samples[64 * 18 * 3];
    int fields[64 * 18 * 3];
    unsigned char *mask;
    unsigned char *decoded;
    unsigned char *palette;
    unsigned char *input;
    unsigned int flags;
    double device[3];
    double value;
    char const *policy;
    int width;
    int height;
    int format;
    int p3;
    int use_float;
    int left;
    int top;
    int w;
    int h;
    int ncolors;
    int phase;
    int phases;
    int i;
    int c;
    int j;
    int x;
    int y;
    int index;
    int expected;
    int ok;

    encoder = NULL;
    dither = NULL;
    output = NULL;
    frame = NULL;
    allocator = NULL;
    mask = decoded = palette = NULL;
    stream = NULL;
    ok = 0;
    width = scenario >= 56 && scenario <= 60 ? 32 : 4;
    height = scenario >= 56 && scenario <= 60 ? 18 : 2;
    if (scenario == 58) {
        width = 64;
        height = 11;
    }
    if (scenario == 61) {
        width = 2;
        height = 1;
    }
    left = scenario == 59 ? 3 : 0;
    top = scenario == 59 ? 5 : 0;
    p3 = scenario == 52 || scenario == 57 || scenario == 64;
    use_float = p3 || scenario == 51 || scenario == 61 || scenario == 65;
    format = p3 ? SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 : use_float ?
        SIXEL_PIXELFORMAT_RGBFLOAT32 : SIXEL_PIXELFORMAT_RGB888;
    policy = scenario == 53 || scenario == 59 ? "keep" :
        scenario == 55 ? "composite" : "clear";
    phases = scenario == 64 ? 3 : scenario == 65 ? 2 : 1;
    stream = calloc(1, sizeof(*stream));
    if (stream == NULL ||
            SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL,
                NULL, NULL)) ||
            SIXEL_FAILED(sixel_encoder_new(&encoder, allocator)) ||
            SIXEL_FAILED(sixel_output_new(&output, hcm_write, stream,
                allocator)) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'I', NULL)) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '.',
                use_float ? "float32" : "8bit")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'W',
                p3 ? "linear" : "gamma")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U',
                p3 ? "display-p3" : "gamma")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'A', policy)) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'E', "size")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 't', "rgb")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '=',
                scenario == 60 ? "greedy:B4" : "greedy:B1"))) {
        goto end;
    }
    if (scenario == 59 &&
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '+', "3,5"))) {
        goto end;
    }
    for (i = 0; i < width * height; ++i) {
        for (c = 0; c < 3; ++c) {
            value = (double)(((i >> (c * 5)) & 31) * 8 + 2) / 255.0;
            if (scenario == 61) {
                value = i == 0 ? .112 : .123;
            }
            fields[i * 3 + c] = (int)(value * 100.0 + .5);
            bytes[i * 3 + c] = (unsigned char)(value * 255.0 + .5);
            samples[i * 3 + c] = (float)value;
            device[c] = value <= .04045 ? value / 12.92 :
                pow((value + .055) / 1.055, 2.4);
        }
        if (p3) {
            for (c = 0; c < 3; ++c) {
                value = 0;
                for (j = 0; j < 3; ++j) {
                    value += matrix[c][j] * device[j];
                }
                samples[i * 3 + c] = (float)value;
            }
        }
    }
    input = use_float ? (unsigned char *)samples : bytes;
    for (phase = 0; phase < phases; ++phase) {
        mask = calloc((size_t)width * (size_t)height, 1);
        if (mask == NULL) {
            goto end;
        }
        for (i = 0; i < width * height; ++i) {
            mask[i] = (unsigned char)hcm_omitted(scenario, phase, i);
        }
        stream->size = 0;
        if (scenario == 65) {
            if (dither == NULL &&
                    SIXEL_FAILED(sixel_dither_new(&dither, -1, allocator))) {
                goto end;
            }
            sixel_dither_set_diffusion_type(dither, SIXEL_DIFFUSE_NONE);
            sixel_dither_set_pixelformat(dither, format);
            /* The caller's unrelated borrowed view must survive all exits. */
            output->transparent_mask = bytes;
            if (phase == 0) {
                sixel_dither_set_pipeline_transparent_mask_hint(dither,
                    mask, 1, 255);
                if (sixel_encode(input, width, height, 0, dither, output)
                        != SIXEL_BAD_INPUT || stream->size != 0 ||
                        output->transparent_mask != bytes) {
                    goto end;
                }
                sixel_dither_set_pipeline_transparent_mask_hint(dither,
                    mask, (size_t)width * (size_t)height, 255);
                value = samples[0];
                samples[0] = NAN;
                if (sixel_encode(input, width, height, 0, dither, output)
                        != SIXEL_BAD_INPUT || stream->size != 0 ||
                        output->transparent_mask != bytes) {
                    goto end;
                }
                samples[0] = (float)value;
            } else {
                sixel_dither_set_pipeline_transparent_mask_hint(dither,
                    NULL, 0, 255);
            }
            if (SIXEL_FAILED(sixel_encode(input, width, height, 0,
                        dither, output)) ||
                    output->transparent_mask != bytes) {
                goto end;
            }
            free(mask);
            mask = NULL;
        } else {
            if (SIXEL_FAILED(sixel_frame_new(&frame, allocator)) ||
                    SIXEL_FAILED(sixel_frame_init_borrowed(frame, input,
                        width, height, format, NULL, -1))) {
                goto end;
            }
            memset(&transparency, 0, sizeof(transparency));
            transparency.transparent = -1;
            transparency.alpha_zero_is_transparent = 1;
            transparency.transparent_mask = mask;
            transparency.transparent_mask_size =
                (size_t)width * (size_t)height;
            if (scenario == 64 && phase == 1) {
                free(mask);
                mask = NULL;
                transparency.alpha_zero_is_transparent = 0;
                transparency.transparent_mask = NULL;
                transparency.transparent_mask_size = 0;
            }
            iface = (sixel_frame_interface_t *)frame;
            if (SIXEL_FAILED(iface->vtbl->set_transparency(iface,
                        &transparency))) {
                goto end;
            }
            mask = NULL;
            if (SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame,
                        output))) {
                goto end;
            }
        }
        if ((scenario == 53 || scenario == 55) &&
                memcmp(stream->bytes, "\033P0;1q", 6) != 0) {
            goto end;
        }
        if (scenario == 54 &&
                memcmp(stream->bytes, "\033P0;0q", 6) != 0) {
            goto end;
        }
        if (SIXEL_FAILED(sixel_decode_direct_with_options(stream->bytes,
                    (int)stream->size, 0, &decoded, &w, &h, &palette,
                    &ncolors, &flags, allocator)) ||
                w != width + left || h != height + top) {
            goto end;
        }
        for (y = 0; y < h; ++y) {
            for (x = 0; x < w; ++x) {
                i = (y * w + x) * 4;
                index = (y - top) * width + x - left;
                expected = x < left || y < top ||
                    hcm_omitted(scenario, phase, index) ? 0 : 255;
                if (decoded[i + 3] != expected) {
                    fprintf(stderr, "alpha at %d,%d: %u want %d\n", x, y,
                        decoded[i + 3], expected);
                    goto end;
                }
                if (expected == 0) {
                    continue;
                }
                for (c = 0; c < 3; ++c) {
                    expected = (fields[index * 3 + c] * 255 + 50) / 100;
                    if (decoded[i + c] != expected) {
                        fprintf(stderr, "color at %d,%d/%d: %u want %d\n",
                            x, y, c, decoded[i + c], expected);
                        goto end;
                    }
                }
            }
        }
        if (scenario >= 56 && scenario <= 60 &&
                (flags & SIXEL_DECODE_PIXELS_RESULT_PALETTE_REDEFINED) == 0) {
            goto end;
        }
        sixel_allocator_free(allocator, decoded);
        sixel_allocator_free(allocator, palette);
        decoded = palette = NULL;
        sixel_frame_unref(frame);
        frame = NULL;
    }
    ok = 1;
end:
    if (!ok) {
        fprintf(stderr, "high-color mask %d: %s\n", scenario,
            sixel_helper_get_additional_message());
    }
    free(mask);
    free(stream);
    sixel_allocator_free(allocator, decoded);
    sixel_allocator_free(allocator, palette);
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_dither_unref(dither);
    sixel_encoder_unref(encoder);
    sixel_allocator_unref(allocator);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
