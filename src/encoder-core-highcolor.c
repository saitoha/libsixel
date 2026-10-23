/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 * Copyright (c) 2014-2016 Hayaki Saito
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdint.h>
#if HAVE_STRING_H
# include <string.h>
#endif  /* HAVE_STRING_H */

#include <sixel.h>

#include "dither.h"
#include "timeline-logger.h"
#include "encoder-core-private.h"
#include "pixelformat.h"
#include "colorspace.h"
#include "cms.h"
#include "encoder-core-highcolor.h"

enum {
    PALETTE_HIT    = 1,
    PALETTE_CHANGE = 2
};

/*
 * Internal encoder helpers implemented in encoder-core-encode.c.
 * Keep prototypes here to avoid implicit declarations in this module.
 */
SIXELSTATUS sixel_encode_header(
    int width,
    int height,
    int keycolor,
    sixel_output_t *output);
SIXELSTATUS sixel_encode_body(
    sixel_index_t *pixels,
    int width,
    int height,
    unsigned char *palette,
    float const *palette_float,
    int ncolors,
    int keycolor,
    int bodyonly,
    sixel_output_t *output,
    unsigned char *palstate,
    sixel_allocator_t *allocator,
    int pin_threads,
    sixel_timeline_logger_t *logger);
SIXELSTATUS sixel_encode_footer(sixel_output_t *output);

/* Prepare destination samples before emitting any bytes. The no-diffusion
 * path retains float representatives; the bounded byte projection below is
 * only a lookup key. Low-level 15bpp diffusion still owns byte samples.
 * RGB888 with no color conversion never enters this allocation path. */
static SIXELSTATUS
sixel_highcolor_convert(unsigned char **prepared, int *stride,
                        unsigned char const *pixels, size_t count,
                        int pixelformat, sixel_dither_t *dither,
                        sixel_output_t const *output)
{
    sixel_color_def_t *srgb;
    sixel_color_def_t *destination;
    sixel_color_xform_t *forward;
    SIXELSTATUS status;
    double source[3];
    double gamma[3];
    double target[3];
    float sample[3];
    unsigned char *buffer;
    size_t i;
    int c;
    int source_float;
    int target_float;
    int source_stride;
    int byte_format;

    source_float = SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat);
    target_float = dither->method_for_diffuse == SIXEL_DIFFUSE_NONE;
    source_stride = source_float ? (int)sizeof(sample) : 3;
    *stride = target_float ? (int)sizeof(sample) : 3;
    if (count > SIZE_MAX / (size_t)*stride) {
        return SIXEL_BAD_INPUT;
    }
    buffer = sixel_allocator_malloc(dither->allocator,
        count * (size_t)*stride);
    if (buffer == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    srgb = destination = NULL;
    forward = NULL;
    status = SIXEL_BAD_ALLOCATION;
    /* Build once for this frame, never per pixel or register replacement.
     * Working coordinates retain their original primaries and may extend
     * outside sRGB. Only the final destination samples become bounded. */
    if (dither->highcolor_output_definition !=
            SIXEL_PALETTE_OUTPUT_LEGACY) {
        srgb = sixel_cdef_name("srgb");
        destination = dither->highcolor_icc != NULL
            ? sixel_cdef_clone(dither->highcolor_icc)
            : sixel_cdef_name(sixel_cdef_named_name(
                dither->highcolor_output_definition));
        if (srgb == NULL || destination == NULL) {
            goto end;
        }
        forward = sixel_cxform_new(srgb, destination, 1);
        if (forward == NULL) {
            goto end;
        }
    }
    byte_format = SIXEL_PIXELFORMAT_RGBFLOAT32;
    switch (output->source_colorspace) {
    case SIXEL_COLORSPACE_OKLAB:
        byte_format = SIXEL_PIXELFORMAT_OKLABFLOAT32;
        break;
    case SIXEL_COLORSPACE_CIELAB:
        byte_format = SIXEL_PIXELFORMAT_CIELABFLOAT32;
        break;
    case SIXEL_COLORSPACE_DIN99D:
        byte_format = SIXEL_PIXELFORMAT_DIN99DFLOAT32;
        break;
    default:
        break;
    }
    for (i = 0; i < count; ++i) {
        if (source_float) {
            memcpy(sample, pixels + i * (size_t)source_stride,
                sizeof(sample));
        }
        for (c = 0; c < 3; ++c) {
            source[c] = source_float ? (double)sample[c] :
                (double)sixel_pixelformat_byte_to_float(byte_format, c,
                    pixels[i * 3U + (size_t)c]);
        }
        status = SIXEL_BAD_INPUT;
        if (forward != NULL) {
            if (!sixel_color_convert(gamma, source,
                    output->source_colorspace, SIXEL_COLORSPACE_GAMMA) ||
                    !sixel_cxform_apply(forward, gamma, target)) {
                goto end;
            }
        } else if (!sixel_color_convert_output(target, source,
                       output->source_colorspace, output->colorspace)) {
            goto end;
        }
        /* This is the final destination gamut boundary, after all matrix
         * and transfer operations. No source or working component is
         * clipped. The writer alone rounds float representatives to RGB101
         * (or HLS); key quantization must not replace those representatives. */
        for (c = 0; c < 3; ++c) {
            sample[c] = (float)(target[c] < 0.0 ? 0.0 :
                target[c] > 1.0 ? 1.0 : target[c]);
            if (!target_float) {
                buffer[i * 3U + (size_t)c] =
                    sixel_pixelformat_float_channel_to_byte(
                        SIXEL_PIXELFORMAT_RGBFLOAT32, c, sample[c]);
            }
        }
        if (target_float) {
            memcpy(buffer + i * sizeof(sample), sample, sizeof(sample));
        }
    }
    *prepared = buffer;
    buffer = NULL;
    status = SIXEL_OK;
end:
    if (status == SIXEL_BAD_INPUT) {
        sixel_helper_set_additional_message(
            "sixel_encode_highcolor: invalid color conversion.");
    }
    sixel_cxform_free(forward);
    sixel_cdef_free(srgb);
    sixel_cdef_free(destination);
    sixel_allocator_free(dither->allocator, buffer);
    return status;
}

/*
 * Normalize a channel value into the range representable by an unsigned char
 * and make the narrowing explicit for MSVC. Negative errors are clamped to
 * zero and large positive values are capped at 0xff.
 */
static unsigned char
sixel_clamp_channel_to_byte(int value)
{
    if (value < 0) {
        return 0;
    }
    if (value > 0xff) {
        return 0xff;
    }

    return (unsigned char)value;
}

static void
dither_func_none(unsigned char *data, int width)
{
    (void) data;  /* unused */
    (void) width; /* unused */
}


static void
dither_func_fs(unsigned char *data, int width)
{
    int r, g, b;
    int error_r = data[0] & 0x7;
    int error_g = data[1] & 0x7;
    int error_b = data[2] & 0x7;

    /* Floyd Steinberg Method
     *          curr    7/16
     *  3/16    5/48    1/16
     */
    r = (data[3 + 0] + (error_r * 5 >> 4));
    g = (data[3 + 1] + (error_g * 5 >> 4));
    b = (data[3 + 2] + (error_b * 5 >> 4));
    data[3 + 0] = sixel_clamp_channel_to_byte(r);
    data[3 + 1] = sixel_clamp_channel_to_byte(g);
    data[3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[width * 3 - 3 + 0] + (error_r * 3 >> 4);
    g = data[width * 3 - 3 + 1] + (error_g * 3 >> 4);
    b = data[width * 3 - 3 + 2] + (error_b * 3 >> 4);
    data[width * 3 - 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[width * 3 - 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[width * 3 - 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[width * 3 + 0] + (error_r * 5 >> 4);
    g = data[width * 3 + 1] + (error_g * 5 >> 4);
    b = data[width * 3 + 2] + (error_b * 5 >> 4);
    data[width * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[width * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[width * 3 + 2] = sixel_clamp_channel_to_byte(b);
}


static void
dither_func_atkinson(unsigned char *data, int width)
{
    int r, g, b;
    int error_r = data[0] & 0x7;
    int error_g = data[1] & 0x7;
    int error_b = data[2] & 0x7;

    error_r += 4;
    error_g += 4;
    error_b += 4;

    /* Atkinson's Method
     *          curr    1/8    1/8
     *   1/8     1/8    1/8
     *           1/8
     */
    r = data[(width * 0 + 1) * 3 + 0] + (error_r >> 3);
    g = data[(width * 0 + 1) * 3 + 1] + (error_g >> 3);
    b = data[(width * 0 + 1) * 3 + 2] + (error_b >> 3);
    data[(width * 0 + 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 0 + 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 0 + 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 0 + 2) * 3 + 0] + (error_r >> 3);
    g = data[(width * 0 + 2) * 3 + 1] + (error_g >> 3);
    b = data[(width * 0 + 2) * 3 + 2] + (error_b >> 3);
    data[(width * 0 + 2) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 0 + 2) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 0 + 2) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 - 1) * 3 + 0] + (error_r >> 3);
    g = data[(width * 1 - 1) * 3 + 1] + (error_g >> 3);
    b = data[(width * 1 - 1) * 3 + 2] + (error_b >> 3);
    data[(width * 1 - 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 - 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 - 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 + 0) * 3 + 0] + (error_r >> 3);
    g = data[(width * 1 + 0) * 3 + 1] + (error_g >> 3);
    b = data[(width * 1 + 0) * 3 + 2] + (error_b >> 3);
    data[(width * 1 + 0) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 + 0) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 + 0) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = (data[(width * 1 + 1) * 3 + 0] + (error_r >> 3));
    g = (data[(width * 1 + 1) * 3 + 1] + (error_g >> 3));
    b = (data[(width * 1 + 1) * 3 + 2] + (error_b >> 3));
    data[(width * 1 + 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 + 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 + 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = (data[(width * 2 + 0) * 3 + 0] + (error_r >> 3));
    g = (data[(width * 2 + 0) * 3 + 1] + (error_g >> 3));
    b = (data[(width * 2 + 0) * 3 + 2] + (error_b >> 3));
    data[(width * 2 + 0) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 2 + 0) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 2 + 0) * 3 + 2] = sixel_clamp_channel_to_byte(b);
}


static void
dither_func_jajuni(unsigned char *data, int width)
{
    int r, g, b;
    int error_r = data[0] & 0x7;
    int error_g = data[1] & 0x7;
    int error_b = data[2] & 0x7;

    error_r += 4;
    error_g += 4;
    error_b += 4;

    /* Jarvis, Judice & Ninke Method
     *                  curr    7/48    5/48
     *  3/48    5/48    7/48    5/48    3/48
     *  1/48    3/48    5/48    3/48    1/48
     */
    r = data[(width * 0 + 1) * 3 + 0] + (error_r * 7 / 48);
    g = data[(width * 0 + 1) * 3 + 1] + (error_g * 7 / 48);
    b = data[(width * 0 + 1) * 3 + 2] + (error_b * 7 / 48);
    data[(width * 0 + 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 0 + 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 0 + 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 0 + 2) * 3 + 0] + (error_r * 5 / 48);
    g = data[(width * 0 + 2) * 3 + 1] + (error_g * 5 / 48);
    b = data[(width * 0 + 2) * 3 + 2] + (error_b * 5 / 48);
    data[(width * 0 + 2) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 0 + 2) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 0 + 2) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 - 2) * 3 + 0] + (error_r * 3 / 48);
    g = data[(width * 1 - 2) * 3 + 1] + (error_g * 3 / 48);
    b = data[(width * 1 - 2) * 3 + 2] + (error_b * 3 / 48);
    data[(width * 1 - 2) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 - 2) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 - 2) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 - 1) * 3 + 0] + (error_r * 5 / 48);
    g = data[(width * 1 - 1) * 3 + 1] + (error_g * 5 / 48);
    b = data[(width * 1 - 1) * 3 + 2] + (error_b * 5 / 48);
    data[(width * 1 - 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 - 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 - 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 + 0) * 3 + 0] + (error_r * 7 / 48);
    g = data[(width * 1 + 0) * 3 + 1] + (error_g * 7 / 48);
    b = data[(width * 1 + 0) * 3 + 2] + (error_b * 7 / 48);
    data[(width * 1 + 0) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 + 0) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 + 0) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 + 1) * 3 + 0] + (error_r * 5 / 48);
    g = data[(width * 1 + 1) * 3 + 1] + (error_g * 5 / 48);
    b = data[(width * 1 + 1) * 3 + 2] + (error_b * 5 / 48);
    data[(width * 1 + 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 + 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 + 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 + 2) * 3 + 0] + (error_r * 3 / 48);
    g = data[(width * 1 + 2) * 3 + 1] + (error_g * 3 / 48);
    b = data[(width * 1 + 2) * 3 + 2] + (error_b * 3 / 48);
    data[(width * 1 + 2) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 + 2) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 + 2) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 2 - 2) * 3 + 0] + (error_r * 1 / 48);
    g = data[(width * 2 - 2) * 3 + 1] + (error_g * 1 / 48);
    b = data[(width * 2 - 2) * 3 + 2] + (error_b * 1 / 48);
    data[(width * 2 - 2) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 2 - 2) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 2 - 2) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 2 - 1) * 3 + 0] + (error_r * 3 / 48);
    g = data[(width * 2 - 1) * 3 + 1] + (error_g * 3 / 48);
    b = data[(width * 2 - 1) * 3 + 2] + (error_b * 3 / 48);
    data[(width * 2 - 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 2 - 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 2 - 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 2 + 0) * 3 + 0] + (error_r * 5 / 48);
    g = data[(width * 2 + 0) * 3 + 1] + (error_g * 5 / 48);
    b = data[(width * 2 + 0) * 3 + 2] + (error_b * 5 / 48);
    data[(width * 2 + 0) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 2 + 0) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 2 + 0) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 2 + 1) * 3 + 0] + (error_r * 3 / 48);
    g = data[(width * 2 + 1) * 3 + 1] + (error_g * 3 / 48);
    b = data[(width * 2 + 1) * 3 + 2] + (error_b * 3 / 48);
    data[(width * 2 + 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 2 + 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 2 + 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 2 + 2) * 3 + 0] + (error_r * 1 / 48);
    g = data[(width * 2 + 2) * 3 + 1] + (error_g * 1 / 48);
    b = data[(width * 2 + 2) * 3 + 2] + (error_b * 1 / 48);
    data[(width * 2 + 2) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 2 + 2) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 2 + 2) * 3 + 2] = sixel_clamp_channel_to_byte(b);
}


static void
dither_func_stucki(unsigned char *data, int width)
{
    int r, g, b;
    int error_r = data[0] & 0x7;
    int error_g = data[1] & 0x7;
    int error_b = data[2] & 0x7;

    error_r += 4;
    error_g += 4;
    error_b += 4;

    /* Stucki's Method
     *                  curr    8/42    4/42
     *  2/42    4/42    8/42    4/42    2/42
     *  1/42    2/42    4/42    2/42    1/42
     */
    r = data[(width * 0 + 1) * 3 + 0] + (error_r * 8 / 42);
    g = data[(width * 0 + 1) * 3 + 1] + (error_g * 8 / 42);
    b = data[(width * 0 + 1) * 3 + 2] + (error_b * 8 / 42);
    data[(width * 0 + 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 0 + 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 0 + 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 0 + 2) * 3 + 0] + (error_r * 4 / 42);
    g = data[(width * 0 + 2) * 3 + 1] + (error_g * 4 / 42);
    b = data[(width * 0 + 2) * 3 + 2] + (error_b * 4 / 42);
    data[(width * 0 + 2) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 0 + 2) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 0 + 2) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 - 2) * 3 + 0] + (error_r * 2 / 42);
    g = data[(width * 1 - 2) * 3 + 1] + (error_g * 2 / 42);
    b = data[(width * 1 - 2) * 3 + 2] + (error_b * 2 / 42);
    data[(width * 1 - 2) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 - 2) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 - 2) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 - 1) * 3 + 0] + (error_r * 4 / 42);
    g = data[(width * 1 - 1) * 3 + 1] + (error_g * 4 / 42);
    b = data[(width * 1 - 1) * 3 + 2] + (error_b * 4 / 42);
    data[(width * 1 - 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 - 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 - 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 + 0) * 3 + 0] + (error_r * 8 / 42);
    g = data[(width * 1 + 0) * 3 + 1] + (error_g * 8 / 42);
    b = data[(width * 1 + 0) * 3 + 2] + (error_b * 8 / 42);
    data[(width * 1 + 0) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 + 0) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 + 0) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 + 1) * 3 + 0] + (error_r * 4 / 42);
    g = data[(width * 1 + 1) * 3 + 1] + (error_g * 4 / 42);
    b = data[(width * 1 + 1) * 3 + 2] + (error_b * 4 / 42);
    data[(width * 1 + 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 + 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 + 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 + 2) * 3 + 0] + (error_r * 2 / 42);
    g = data[(width * 1 + 2) * 3 + 1] + (error_g * 2 / 42);
    b = data[(width * 1 + 2) * 3 + 2] + (error_b * 2 / 42);
    data[(width * 1 + 2) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 + 2) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 + 2) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 2 - 2) * 3 + 0] + (error_r * 1 / 42);
    g = data[(width * 2 - 2) * 3 + 1] + (error_g * 1 / 42);
    b = data[(width * 2 - 2) * 3 + 2] + (error_b * 1 / 42);
    data[(width * 2 - 2) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 2 - 2) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 2 - 2) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 2 - 1) * 3 + 0] + (error_r * 2 / 42);
    g = data[(width * 2 - 1) * 3 + 1] + (error_g * 2 / 42);
    b = data[(width * 2 - 1) * 3 + 2] + (error_b * 2 / 42);
    data[(width * 2 - 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 2 - 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 2 - 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 2 + 0) * 3 + 0] + (error_r * 4 / 42);
    g = data[(width * 2 + 0) * 3 + 1] + (error_g * 4 / 42);
    b = data[(width * 2 + 0) * 3 + 2] + (error_b * 4 / 42);
    data[(width * 2 + 0) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 2 + 0) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 2 + 0) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 2 + 1) * 3 + 0] + (error_r * 2 / 42);
    g = data[(width * 2 + 1) * 3 + 1] + (error_g * 2 / 42);
    b = data[(width * 2 + 1) * 3 + 2] + (error_b * 2 / 42);
    data[(width * 2 + 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 2 + 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 2 + 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 2 + 2) * 3 + 0] + (error_r * 1 / 42);
    g = data[(width * 2 + 2) * 3 + 1] + (error_g * 1 / 42);
    b = data[(width * 2 + 2) * 3 + 2] + (error_b * 1 / 42);
    data[(width * 2 + 2) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 2 + 2) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 2 + 2) * 3 + 2] = sixel_clamp_channel_to_byte(b);
}


static void
dither_func_burkes(unsigned char *data, int width)
{
    int r, g, b;
    int error_r = data[0] & 0x7;
    int error_g = data[1] & 0x7;
    int error_b = data[2] & 0x7;

    error_r += 2;
    error_g += 2;
    error_b += 2;

    /* Burkes' Method
     *                  curr    4/16    2/16
     *  1/16    2/16    4/16    2/16    1/16
     */
    r = data[(width * 0 + 1) * 3 + 0] + (error_r * 4 / 16);
    g = data[(width * 0 + 1) * 3 + 1] + (error_g * 4 / 16);
    b = data[(width * 0 + 1) * 3 + 2] + (error_b * 4 / 16);
    data[(width * 0 + 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 0 + 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 0 + 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 0 + 2) * 3 + 0] + (error_r * 2 / 16);
    g = data[(width * 0 + 2) * 3 + 1] + (error_g * 2 / 16);
    b = data[(width * 0 + 2) * 3 + 2] + (error_b * 2 / 16);
    data[(width * 0 + 2) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 0 + 2) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 0 + 2) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 - 2) * 3 + 0] + (error_r * 1 / 16);
    g = data[(width * 1 - 2) * 3 + 1] + (error_g * 1 / 16);
    b = data[(width * 1 - 2) * 3 + 2] + (error_b * 1 / 16);
    data[(width * 1 - 2) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 - 2) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 - 2) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 - 1) * 3 + 0] + (error_r * 2 / 16);
    g = data[(width * 1 - 1) * 3 + 1] + (error_g * 2 / 16);
    b = data[(width * 1 - 1) * 3 + 2] + (error_b * 2 / 16);
    data[(width * 1 - 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 - 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 - 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 + 0) * 3 + 0] + (error_r * 4 / 16);
    g = data[(width * 1 + 0) * 3 + 1] + (error_g * 4 / 16);
    b = data[(width * 1 + 0) * 3 + 2] + (error_b * 4 / 16);
    data[(width * 1 + 0) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 + 0) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 + 0) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 + 1) * 3 + 0] + (error_r * 2 / 16);
    g = data[(width * 1 + 1) * 3 + 1] + (error_g * 2 / 16);
    b = data[(width * 1 + 1) * 3 + 2] + (error_b * 2 / 16);
    data[(width * 1 + 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 + 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 + 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 + 2) * 3 + 0] + (error_r * 1 / 16);
    g = data[(width * 1 + 2) * 3 + 1] + (error_g * 1 / 16);
    b = data[(width * 1 + 2) * 3 + 2] + (error_b * 1 / 16);
    data[(width * 1 + 2) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 + 2) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 + 2) * 3 + 2] = sixel_clamp_channel_to_byte(b);
}


static void
dither_func_sierra1(unsigned char *data, int width)
{
    int r, g, b;
    int error_r = data[0] & 0x7;
    int error_g = data[1] & 0x7;
    int error_b = data[2] & 0x7;

    error_r += 2;
    error_g += 2;
    error_b += 2;

    /* Sierra Lite Method
     *          curr    2/4
     *  1/4     1/4
     */
    r = data[(width * 0 + 1) * 3 + 0] + (error_r * 2 / 4);
    g = data[(width * 0 + 1) * 3 + 1] + (error_g * 2 / 4);
    b = data[(width * 0 + 1) * 3 + 2] + (error_b * 2 / 4);
    data[(width * 0 + 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 0 + 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 0 + 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 - 1) * 3 + 0] + (error_r * 1 / 4);
    g = data[(width * 1 - 1) * 3 + 1] + (error_g * 1 / 4);
    b = data[(width * 1 - 1) * 3 + 2] + (error_b * 1 / 4);
    data[(width * 1 - 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 - 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 - 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 + 0) * 3 + 0] + (error_r * 1 / 4);
    g = data[(width * 1 + 0) * 3 + 1] + (error_g * 1 / 4);
    b = data[(width * 1 + 0) * 3 + 2] + (error_b * 1 / 4);
    data[(width * 1 + 0) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 + 0) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 + 0) * 3 + 2] = sixel_clamp_channel_to_byte(b);
}


static void
dither_func_sierra2(unsigned char *data, int width)
{
    int r, g, b;
    int error_r = data[0] & 0x7;
    int error_g = data[1] & 0x7;
    int error_b = data[2] & 0x7;

    error_r += 4;
    error_g += 4;
    error_b += 4;

    /* Sierra Two-row Method
     *                  curr    4/16    3/16
     *  1/16    2/16    3/16    2/16    1/16
     */
    r = data[(width * 0 + 1) * 3 + 0] + (error_r * 4 / 16);
    g = data[(width * 0 + 1) * 3 + 1] + (error_g * 4 / 16);
    b = data[(width * 0 + 1) * 3 + 2] + (error_b * 4 / 16);
    data[(width * 0 + 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 0 + 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 0 + 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 0 + 2) * 3 + 0] + (error_r * 3 / 16);
    g = data[(width * 0 + 2) * 3 + 1] + (error_g * 3 / 16);
    b = data[(width * 0 + 2) * 3 + 2] + (error_b * 3 / 16);
    data[(width * 0 + 2) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 0 + 2) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 0 + 2) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 - 2) * 3 + 0] + (error_r * 1 / 16);
    g = data[(width * 1 - 2) * 3 + 1] + (error_g * 1 / 16);
    b = data[(width * 1 - 2) * 3 + 2] + (error_b * 1 / 16);
    data[(width * 1 - 2) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 - 2) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 - 2) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 - 1) * 3 + 0] + (error_r * 2 / 16);
    g = data[(width * 1 - 1) * 3 + 1] + (error_g * 2 / 16);
    b = data[(width * 1 - 1) * 3 + 2] + (error_b * 2 / 16);
    data[(width * 1 - 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 - 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 - 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 + 0) * 3 + 0] + (error_r * 3 / 16);
    g = data[(width * 1 + 0) * 3 + 1] + (error_g * 3 / 16);
    b = data[(width * 1 + 0) * 3 + 2] + (error_b * 3 / 16);
    data[(width * 1 + 0) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 + 0) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 + 0) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 + 1) * 3 + 0] + (error_r * 2 / 16);
    g = data[(width * 1 + 1) * 3 + 1] + (error_g * 2 / 16);
    b = data[(width * 1 + 1) * 3 + 2] + (error_b * 2 / 16);
    data[(width * 1 + 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 + 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 + 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 + 2) * 3 + 0] + (error_r * 1 / 16);
    g = data[(width * 1 + 2) * 3 + 1] + (error_g * 1 / 16);
    b = data[(width * 1 + 2) * 3 + 2] + (error_b * 1 / 16);
    data[(width * 1 + 2) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 + 2) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 + 2) * 3 + 2] = sixel_clamp_channel_to_byte(b);
}


static void
dither_func_sierra3(unsigned char *data, int width)
{
    int r, g, b;
    int error_r = data[0] & 0x7;
    int error_g = data[1] & 0x7;
    int error_b = data[2] & 0x7;

    error_r += 4;
    error_g += 4;
    error_b += 4;

    /* Sierra-3 Method
     *                  curr    5/32    3/32
     *  2/32    4/32    5/32    4/32    2/32
     *                  2/32    3/32    2/32
     */
    r = data[(width * 0 + 1) * 3 + 0] + (error_r * 5 / 32);
    g = data[(width * 0 + 1) * 3 + 1] + (error_g * 5 / 32);
    b = data[(width * 0 + 1) * 3 + 2] + (error_b * 5 / 32);
    data[(width * 0 + 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 0 + 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 0 + 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 0 + 2) * 3 + 0] + (error_r * 3 / 32);
    g = data[(width * 0 + 2) * 3 + 1] + (error_g * 3 / 32);
    b = data[(width * 0 + 2) * 3 + 2] + (error_b * 3 / 32);
    data[(width * 0 + 2) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 0 + 2) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 0 + 2) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 - 2) * 3 + 0] + (error_r * 2 / 32);
    g = data[(width * 1 - 2) * 3 + 1] + (error_g * 2 / 32);
    b = data[(width * 1 - 2) * 3 + 2] + (error_b * 2 / 32);
    data[(width * 1 - 2) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 - 2) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 - 2) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 - 1) * 3 + 0] + (error_r * 4 / 32);
    g = data[(width * 1 - 1) * 3 + 1] + (error_g * 4 / 32);
    b = data[(width * 1 - 1) * 3 + 2] + (error_b * 4 / 32);
    data[(width * 1 - 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 - 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 - 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 + 0) * 3 + 0] + (error_r * 5 / 32);
    g = data[(width * 1 + 0) * 3 + 1] + (error_g * 5 / 32);
    b = data[(width * 1 + 0) * 3 + 2] + (error_b * 5 / 32);
    data[(width * 1 + 0) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 + 0) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 + 0) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 + 1) * 3 + 0] + (error_r * 4 / 32);
    g = data[(width * 1 + 1) * 3 + 1] + (error_g * 4 / 32);
    b = data[(width * 1 + 1) * 3 + 2] + (error_b * 4 / 32);
    data[(width * 1 + 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 + 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 + 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 + 2) * 3 + 0] + (error_r * 2 / 32);
    g = data[(width * 1 + 2) * 3 + 1] + (error_g * 2 / 32);
    b = data[(width * 1 + 2) * 3 + 2] + (error_b * 2 / 32);
    data[(width * 1 + 2) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 + 2) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 + 2) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 2 - 1) * 3 + 0] + (error_r * 2 / 32);
    g = data[(width * 2 - 1) * 3 + 1] + (error_g * 2 / 32);
    b = data[(width * 2 - 1) * 3 + 2] + (error_b * 2 / 32);
    data[(width * 2 - 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 2 - 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 2 - 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 2 + 0) * 3 + 0] + (error_r * 3 / 32);
    g = data[(width * 2 + 0) * 3 + 1] + (error_g * 3 / 32);
    b = data[(width * 2 + 0) * 3 + 2] + (error_b * 3 / 32);
    data[(width * 2 + 0) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 2 + 0) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 2 + 0) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 2 + 1) * 3 + 0] + (error_r * 2 / 32);
    g = data[(width * 2 + 1) * 3 + 1] + (error_g * 2 / 32);
    b = data[(width * 2 + 1) * 3 + 2] + (error_b * 2 / 32);
    data[(width * 2 + 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 2 + 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 2 + 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
}


static void
dither_func_a_dither(unsigned char *data, int width, int x, int y)
{
    int c;
    float value;
    float mask;

    (void) width; /* unused */

    for (c = 0; c < 3; c ++) {
        mask = (float)((((x + c * 17) + y * 236) * 119) & 255);
        mask = ((mask - 128) / 256.0f);
        value = data[c] + mask;
        if (value < 0) {
            value = 0;
        }
        value = value > 255 ? 255 : value;
        data[c] = sixel_clamp_channel_to_byte((int)value);
    }
}


static void
dither_func_x_dither(unsigned char *data, int width, int x, int y)
{
    int c;
    float value;
    float mask;

    (void) width;  /* unused */

    for (c = 0; c < 3; c ++) {
        mask = (float)((((x + c * 17) ^ (y * 236)) * 1234) & 511);
        mask = ((mask - 128) / 512.0f);
        value = data[c] + mask;
        if (value < 0) {
            value = 0;
        }
        value = value > 255 ? 255 : value;
        data[c] = sixel_clamp_channel_to_byte((int)value);
    }
}


static void
sixel_apply_15bpp_dither(
    unsigned char *pixels,
    int x,
    int y,
    int width,
    int height,
    int method_for_diffuse)
{
    /* apply floyd steinberg dithering */
    switch (method_for_diffuse) {
    case SIXEL_DIFFUSE_FS:
        if (x < width - 1 && y < height - 1) {
            dither_func_fs(pixels, width);
        }
        break;
    case SIXEL_DIFFUSE_ATKINSON:
        if (x < width - 2 && y < height - 2) {
            dither_func_atkinson(pixels, width);
        }
        break;
    case SIXEL_DIFFUSE_JAJUNI:
        if (x < width - 2 && y < height - 2) {
            dither_func_jajuni(pixels, width);
        }
        break;
    case SIXEL_DIFFUSE_STUCKI:
        if (x < width - 2 && y < height - 2) {
            dither_func_stucki(pixels, width);
        }
        break;
    case SIXEL_DIFFUSE_BURKES:
        if (x < width - 2 && y < height - 1) {
            dither_func_burkes(pixels, width);
        }
        break;
    case SIXEL_DIFFUSE_SIERRA1:
        if (x < width - 1 && y < height - 1) {
            dither_func_sierra1(pixels, width);
        }
        break;
    case SIXEL_DIFFUSE_SIERRA2:
        if (x < width - 2 && y < height - 1) {
            dither_func_sierra2(pixels, width);
        }
        break;
    case SIXEL_DIFFUSE_SIERRA3:
        if (x < width - 2 && y < height - 2) {
            dither_func_sierra3(pixels, width);
        }
        break;
    case SIXEL_DIFFUSE_A_DITHER:
        dither_func_a_dither(pixels, width, x, y);
        break;
    case SIXEL_DIFFUSE_X_DITHER:
        dither_func_x_dither(pixels, width, x, y);
        break;
    case SIXEL_DIFFUSE_NONE:
    default:
        dither_func_none(pixels, width);
        break;
    }
}


/*
 * Encode high-color data (15bpp dithering).
 *
 * Flow overview:
 * - Normalize input pixels to RGB888 when needed.
 * - Track palette usage per pass to keep the palette within 255 entries.
 * - Emit sixel bands while updating palette hits and palette-change flags.
 */
SIXELSTATUS
sixel_encode_highcolor(
    unsigned char *pixels,
    int width,
    int height,
    sixel_dither_t *dither,
    sixel_output_t *output)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_index_t *paletted_pixels = NULL;
    unsigned char *normalized_pixels = NULL;
    unsigned char *converted_pixels = NULL;
    float palette_float[SIXEL_PALETTE_MAX * 3];
    float sample[3];
    unsigned char key[3];
    unsigned char *representative;
    int stride;
    int pixelformat;
    int use_float;
    int channel;
    /* Mark sixel line pixels which have been already drawn. */
    unsigned char *marks;
    unsigned char *rgbhit;
    unsigned char *rgb2pal;
    unsigned char palhitcount[SIXEL_PALETTE_MAX];
    unsigned char palstate[SIXEL_PALETTE_MAX];
    int output_count;
    int encoded_width;
    int encoded_height;
    int saved_offset_top;
    unsigned char const *saved_mask;
    unsigned char const *source_mask;
    int const maxcolors = 1 << 15;
    int whole_size = width * height  /* for paletted_pixels */
                   + maxcolors       /* for rgbhit */
                   + maxcolors       /* for rgb2pal */
                   + width * 6;      /* for marks */
    int x, y;
    unsigned char *dst;
    unsigned char *mptr;
    int dirty;
    int mod_y;
    int band_rows;
    int nextpal;
    int threshold;
    int pix;
    unsigned char mapped_index;
    int orig_height;
    unsigned char *pal;
    unsigned char *palette_entries = NULL;
    sixel_palette_t *palette_obj = NULL;
    size_t palette_count = 0U;
    sixel_palette_entries_view_t palette_view;

    memset(&palette_view, 0, sizeof(palette_view));
    if (pixels == NULL || dither == NULL || output == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encode_highcolor: invalid argument "
            "(pixels == NULL || dither == NULL || output == NULL)");
        return SIXEL_BAD_ARGUMENT;
    }

    /*
     * height is rewritten per pass below, so the emitted image extent has to
     * be taken from the original geometry.  The top margin belongs to the
     * first emitted body only: later passes continue from the sixel position
     * the previous body left behind, so re-emitting the margin would push
     * them further down the page.
     */
    saved_offset_top = output->transparent_offset_top;
    saved_mask = output->transparent_mask;
    source_mask = dither->pipeline_transparent_mask;
    status = sixel_output_compute_transparent_extent(output,
                                                     width,
                                                     height,
                                                     &encoded_width,
                                                     &encoded_height);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    /* The borrowed view covers the original frame, before height is reduced
     * by pass rewinds. Keep it on output for P2 and fill suppression, then
     * restore the caller's view on every exit, including conversion errors. */
    if (source_mask != NULL &&
            (dither->pipeline_transparent_mask_size <
                (size_t)width * (size_t)height || output->ormode)) {
        sixel_helper_set_additional_message(
            "high-color source mask requires full size and normal output.");
        return SIXEL_BAD_INPUT;
    }
    output->transparent_mask = source_mask;

    pixelformat = dither->pixelformat;
    stride = 3;
    use_float = 0;
    if (pixelformat != SIXEL_PIXELFORMAT_RGB888 &&
            !SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat)) {
        /* normalize pixelformat */
        normalized_pixels = (unsigned char *)sixel_allocator_malloc(
            dither->allocator,
            (size_t)(width * height * 3));
        if (normalized_pixels == NULL) {
            goto error;
        }
        status = sixel_helper_normalize_pixelformat(
            normalized_pixels,
            &pixelformat,
            pixels,
            dither->pixelformat,
            width,
            height);
        if (SIXEL_FAILED(status)) {
            goto error;
        }
        pixels = normalized_pixels;
    }

    if (SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat) ||
            output->source_colorspace != output->colorspace ||
            dither->highcolor_output_definition !=
                SIXEL_PALETTE_OUTPUT_LEGACY) {
        status = sixel_highcolor_convert(&converted_pixels, &stride, pixels,
            (size_t)width * (size_t)height, pixelformat, dither, output);
        if (SIXEL_FAILED(status)) {
            goto error;
        }
        pixels = converted_pixels;
        use_float = stride != 3;
    }
    memset(palette_float, 0, sizeof(palette_float));

    palette_entries = NULL;
    palette_obj = NULL;
    palette_count = 0U;
    palette_obj = dither->palette;
    if (palette_obj == NULL || palette_obj->vtbl == NULL ||
            palette_obj->vtbl->get_entries == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto error;
    }
    status = palette_obj->vtbl->get_entries(palette_obj, &palette_view);
    if (SIXEL_FAILED(status) || palette_view.entries == NULL ||
            palette_view.depth != 3) {
        goto error;
    }
    palette_count = palette_view.entry_count;
    palette_entries = (unsigned char *)sixel_allocator_malloc(
        dither->allocator,
        palette_count * 3U);
    if (palette_entries == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto error;
    }
    memcpy(palette_entries, palette_view.entries, palette_count * 3U);

    paletted_pixels = (sixel_index_t *)sixel_allocator_malloc(
        dither->allocator,
        (size_t)whole_size);
    if (paletted_pixels == NULL) {
        goto error;
    }
    rgbhit = paletted_pixels + width * height;
    memset(rgbhit, 0, (size_t)(maxcolors * 2 + width * 6));
    rgb2pal = rgbhit + maxcolors;
    marks = rgb2pal + maxcolors;
    output_count = 0;

next:
    dst = paletted_pixels;
    nextpal = 0;
    threshold = 1;
    dirty = 0;
    mptr = marks;
    memset(palstate, 0, sizeof(palstate));
    /* Clear palette hit counters for each pass to avoid stale data. */
    memset(palhitcount, 0, sizeof(palhitcount));
    y = 0;
    band_rows = 0;
    mod_y = output->transparent_offset_top % 6;

    while (1) {
        for (x = 0; x < width; x++, mptr++, dst++, pixels += stride) {
            if (source_mask != NULL &&
                    source_mask[(size_t)y * (size_t)width + (size_t)x]) {
                /* Omitted source pixels neither consume a register nor
                 * select the representative of a visible lookup key. */
                *mptr = 1;
                *dst = 255;
            } else if (*mptr) {
                *dst = 255;
            } else {
                representative = pixels;
                if (use_float) {
                    memcpy(sample, pixels, sizeof(sample));
                    for (channel = 0; channel < 3; ++channel) {
                        key[channel] =
                            sixel_pixelformat_float_channel_to_byte(
                                SIXEL_PIXELFORMAT_RGBFLOAT32, channel,
                                sample[channel]);
                    }
                    representative = key;
                } else {
                    sixel_apply_15bpp_dither(pixels, x, y, width, height,
                        dither->method_for_diffuse);
                }
                pix = ((representative[0] & 0xf8) << 7) |
                      ((representative[1] & 0xf8) << 2) |
                      ((representative[2] >> 3) & 0x1f);

                if (!rgbhit[pix]) {
                    while (1) {
                        if (nextpal >= 255) {
                            if (threshold >= 255) {
                                break;
                            } else {
                                threshold = (threshold == 1) ? 9: 255;
                                nextpal = 0;
                            }
                        } else if (palstate[nextpal]
                                 || palhitcount[nextpal] > threshold) {
                            nextpal++;
                        } else {
                            break;
                        }
                    }

                    if (nextpal >= 255) {
                        dirty = 1;
                        *dst = 255;
                    } else {
                        pal = palette_entries + (nextpal * 3);

                        rgbhit[pix] = 1;
                        if (output_count > 0) {
                            rgbhit[((pal[0] & 0xf8) << 7) |
                                   ((pal[1] & 0xf8) << 2) |
                                   ((pal[2] >> 3) & 0x1f)] = 0;
                        }
                        /*
                         * Palette indices never exceed 255 in this loop, so
                         * the explicit cast keeps MSVC quiet while preserving
                         * the existing behavior.
                         */
                        mapped_index = (unsigned char)nextpal;
                        *dst = mapped_index;
                        rgb2pal[pix] = mapped_index;
                        nextpal++;
                        *mptr = 1;
                        palstate[*dst] = PALETTE_CHANGE;
                        palhitcount[*dst] = 1;
                        memcpy(pal, representative, 3U);
                        if (use_float) {
                            memcpy(palette_float + (size_t)*dst * 3U,
                                sample, sizeof(sample));
                        }
                    }
                } else {
                    *dst = rgb2pal[pix];
                    *mptr = 1;
                    if (!palstate[*dst]) {
                        palstate[*dst] = PALETTE_HIT;
                    }
                    if (palhitcount[*dst] < 255) {
                        palhitcount[*dst]++;
                    }
                }
            }
        }

        ++band_rows;
        if (++y >= height && !dirty) {
            goto end;
        }
        if (dirty && (mod_y == 5 || y >= height)) {
            orig_height = height;

            if (output_count++ == 0) {
                status = sixel_encode_header(encoded_width,
                                             encoded_height,
                                             dither->keycolor,
                                             output);
                if (SIXEL_FAILED(status)) {
                    goto error;
                }
            }
            height = y;
            status = sixel_encode_body(
                paletted_pixels,
                width,
                height,
                palette_entries,
                use_float ? palette_float : NULL,
                255,
                255,
                dither->bodyonly,
                output,
                palstate,
                dither->allocator,
                dither->pipeline_pin_threads,
                NULL);
            if (SIXEL_FAILED(status)) {
                goto error;
            }
            /* The body leaves the cursor at the current band's origin.
             * Even at EOF, unpainted pixels need another palette pass;
             * jumping to end would duplicate this body and omit them.
             * Rewind only the source rows in this physical band. A short
             * final band or the first band after a top margin has fewer
             * than six rows. Preserve its marks and its initial row bit,
             * but never emit the whole top margin a second time. */
            output->transparent_offset_top = y == band_rows ?
                output->transparent_offset_top % 6 : 0;
            if (source_mask != NULL) {
                source_mask += (size_t)(y - band_rows) * (size_t)width;
                output->transparent_mask = source_mask;
            }
            pixels -= band_rows * width * stride;
            height = orig_height - height + band_rows;
            goto next;
        }

        if (++mod_y == 6) {
            mptr = (unsigned char *)memset(marks, 0, (size_t)(width * 6));
            mod_y = 0;
            band_rows = 0;
        }
    }

end:
    if (output_count == 0) {
        status = sixel_encode_header(encoded_width,
                                     encoded_height,
                                     dither->keycolor,
                                     output);
        if (SIXEL_FAILED(status)) {
            goto error;
        }
    }
    status = sixel_encode_body(
        paletted_pixels,
        width,
        height,
        palette_entries,
        use_float ? palette_float : NULL,
        255,
        255,
        dither->bodyonly,
        output,
        palstate,
        dither->allocator,
        dither->pipeline_pin_threads,
        NULL);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_encode_footer(output);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

error:
    output->transparent_offset_top = saved_offset_top;
    output->transparent_mask = saved_mask;
    if (palette_entries != NULL) {
        sixel_allocator_free(dither->allocator, palette_entries);
    }
    sixel_allocator_free(dither->allocator, paletted_pixels);
    sixel_allocator_free(dither->allocator, normalized_pixels);
    sixel_allocator_free(dither->allocator, converted_pixels);

    return status;
}


/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
