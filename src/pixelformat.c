/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2021-2025 libsixel developers. See `AUTHORS`.
 * Copyright (c) 2014-2019 Hayaki Saito
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "config.h"

/* STDC_HEADERS */
#include <stdio.h>
#include <stdlib.h>

#if HAVE_MATH_H
# include <math.h>
#endif  /* HAVE_MATH_H */

#if HAVE_MEMORY_H
# include <memory.h>
#endif  /* HAVE_MEMORY_H */

#include <sixel.h>

#include "pixelformat.h"

#define SIXEL_OKLAB_AB_FLOAT_MIN (-0.5f)
#define SIXEL_OKLAB_AB_FLOAT_MAX (0.5f)
#define SIXEL_CIELAB_AB_FLOAT_MIN (-1.5f)
#define SIXEL_CIELAB_AB_FLOAT_MAX (1.5f)
#define SIXEL_CIELAB_L_FLOAT_MIN  (0.0f)
#define SIXEL_CIELAB_L_FLOAT_MAX  (1.0f)
#define SIXEL_DIN99D_L_FLOAT_MIN  (0.0f)
#define SIXEL_DIN99D_L_FLOAT_MAX  (1.0f)
#define SIXEL_DIN99D_AB_FLOAT_MIN (-1.0f)
#define SIXEL_DIN99D_AB_FLOAT_MAX (1.0f)
#define SIXEL_YUV_Y_FLOAT_MIN     (0.0f)
#define SIXEL_YUV_Y_FLOAT_MAX     (1.0f)
#define SIXEL_YUV_U_FLOAT_MIN     (-0.436f)
#define SIXEL_YUV_U_FLOAT_MAX     (0.436f)
#define SIXEL_YUV_V_FLOAT_MIN     (-0.615f)
#define SIXEL_YUV_V_FLOAT_MAX     (0.615f)

/*
 * Normalize a float32 channel stored in the 0.0-1.0 range and convert
 * the value to an 8-bit sample. Out-of-range or NaN inputs are clamped
 * to sane defaults so downstream conversions always receive valid bytes.
 */
static unsigned char
sixel_pixelformat_float_to_byte(float value)
{
#if HAVE_MATH_H
    if (!isfinite(value)) {
        value = 0.0f;
    }
#endif  /* HAVE_MATH_H */

    if (value <= 0.0f) {
        return 0;
    }
    if (value >= 1.0f) {
        return 255;
    }

    return (unsigned char)(value * 255.0f + 0.5f);
}

static unsigned char
sixel_pixelformat_oklab_L_to_byte(float value)
{
#if HAVE_MATH_H
    if (!isfinite(value)) {
        value = 0.0f;
    }
#endif  /* HAVE_MATH_H */

    if (value <= 0.0f) {
        return 0;
    }
    if (value >= 1.0f) {
        return 255;
    }

    return (unsigned char)(value * 255.0f + 0.5f);
}

static unsigned char
sixel_pixelformat_oklab_ab_to_byte(float value)
{
    float encoded;

#if HAVE_MATH_H
    if (!isfinite(value)) {
        value = 0.0f;
    }
#endif  /* HAVE_MATH_H */

    encoded = value + 0.5f;
    if (encoded <= 0.0f) {
        return 0;
    }
    if (encoded >= 1.0f) {
        return 255;
    }

    return (unsigned char)(encoded * 255.0f + 0.5f);
}

static unsigned char
sixel_pixelformat_cielab_L_to_byte(float value)
{
#if HAVE_MATH_H
    if (!isfinite(value)) {
        value = 0.0f;
    }
#endif  /* HAVE_MATH_H */

    if (value <= 0.0f) {
        return 0;
    }
    if (value >= 1.0f) {
        return 255;
    }

    return (unsigned char)(value * 255.0f + 0.5f);
}

static unsigned char
sixel_pixelformat_cielab_ab_to_byte(float value)
{
    float encoded;

#if HAVE_MATH_H
    if (!isfinite(value)) {
        value = 0.0f;
    }
#endif  /* HAVE_MATH_H */

    encoded = (value / (2.0f * SIXEL_CIELAB_AB_FLOAT_MAX)) + 0.5f;
    if (encoded <= 0.0f) {
        return 0;
    }
    if (encoded >= 1.0f) {
        return 255;
    }

    return (unsigned char)(encoded * 255.0f + 0.5f);
}

static unsigned char
sixel_pixelformat_din99d_L_to_byte(float value)
{
#if HAVE_MATH_H
    if (!isfinite(value)) {
        value = 0.0f;
    }
#endif  /* HAVE_MATH_H */

    if (value <= 0.0f) {
        return 0;
    }
    if (value >= 1.0f) {
        return 255;
    }

    return (unsigned char)(value * 255.0f + 0.5f);
}

static unsigned char
sixel_pixelformat_din99d_ab_to_byte(float value)
{
    float encoded;

#if HAVE_MATH_H
    if (!isfinite(value)) {
        value = 0.0f;
    }
#endif  /* HAVE_MATH_H */

    encoded = (value / (2.0f * SIXEL_DIN99D_AB_FLOAT_MAX)) + 0.5f;
    if (encoded <= 0.0f) {
        return 0;
    }
    if (encoded >= 1.0f) {
        return 255;
    }

    return (unsigned char)(encoded * 255.0f + 0.5f);
}

static unsigned char
sixel_pixelformat_yuv_chroma_to_byte(float value, float range)
{
    float encoded;

#if HAVE_MATH_H
    if (!isfinite(value)) {
        value = 0.0f;
    }
#endif  /* HAVE_MATH_H */

    encoded = (value / (2.0f * range)) + 0.5f;
    if (encoded <= 0.0f) {
        return 0;
    }
    if (encoded >= 1.0f) {
        return 255;
    }

    return (unsigned char)(encoded * 255.0f + 0.5f);
}

static float
sixel_pixelformat_yuv_chroma_from_byte(unsigned char value, float range)
{
    float encoded;

    encoded = (float)value / 255.0f;
    return (encoded - 0.5f) * (2.0f * range);
}

static float
sixel_pixelformat_float_channel_min_internal(int pixelformat,
                                             int channel)
{
    (void)channel;
    if (pixelformat == SIXEL_PIXELFORMAT_OKLABFLOAT32) {
        if (channel == 0) {
            return 0.0f;
        }
        return SIXEL_OKLAB_AB_FLOAT_MIN;
    }
    if (pixelformat == SIXEL_PIXELFORMAT_CIELABFLOAT32) {
        if (channel == 0) {
            return SIXEL_CIELAB_L_FLOAT_MIN;
        }
        return SIXEL_CIELAB_AB_FLOAT_MIN;
    }
    if (pixelformat == SIXEL_PIXELFORMAT_DIN99DFLOAT32) {
        if (channel == 0) {
            return SIXEL_DIN99D_L_FLOAT_MIN;
        }
        return SIXEL_DIN99D_AB_FLOAT_MIN;
    }
    if (pixelformat == SIXEL_PIXELFORMAT_YUVFLOAT32) {
        if (channel == 0) {
            return SIXEL_YUV_Y_FLOAT_MIN;
        }
        if (channel == 1) {
            return SIXEL_YUV_U_FLOAT_MIN;
        }
        return SIXEL_YUV_V_FLOAT_MIN;
    }
    return 0.0f;
}

static float
sixel_pixelformat_float_channel_max_internal(int pixelformat,
                                             int channel)
{
    (void)channel;
    if (pixelformat == SIXEL_PIXELFORMAT_OKLABFLOAT32) {
        if (channel == 0) {
            return 1.0f;
        }
        return SIXEL_OKLAB_AB_FLOAT_MAX;
    }
    if (pixelformat == SIXEL_PIXELFORMAT_CIELABFLOAT32) {
        if (channel == 0) {
            return SIXEL_CIELAB_L_FLOAT_MAX;
        }
        return SIXEL_CIELAB_AB_FLOAT_MAX;
    }
    if (pixelformat == SIXEL_PIXELFORMAT_DIN99DFLOAT32) {
        if (channel == 0) {
            return SIXEL_DIN99D_L_FLOAT_MAX;
        }
        return SIXEL_DIN99D_AB_FLOAT_MAX;
    }
    if (pixelformat == SIXEL_PIXELFORMAT_YUVFLOAT32) {
        if (channel == 0) {
            return SIXEL_YUV_Y_FLOAT_MAX;
        }
        if (channel == 1) {
            return SIXEL_YUV_U_FLOAT_MAX;
        }
        return SIXEL_YUV_V_FLOAT_MAX;
    }
    return 1.0f;
}

float
sixel_pixelformat_float_channel_clamp(int pixelformat,
                                      int channel,
                                      float value)
{
    float minimum;
    float maximum;

#if HAVE_MATH_H
    if (!isfinite(value)) {
        value = 0.0f;
    }
#endif  /* HAVE_MATH_H */

    minimum = sixel_pixelformat_float_channel_min_internal(pixelformat,
                                                           channel);
    maximum = sixel_pixelformat_float_channel_max_internal(pixelformat,
                                                           channel);
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }

    return value;
}

unsigned char
sixel_pixelformat_float_channel_to_byte(int pixelformat,
                                        int channel,
                                        float value)
{
    float clamped;

    clamped = sixel_pixelformat_float_channel_clamp(pixelformat,
                                                    channel,
                                                    value);
    if (pixelformat == SIXEL_PIXELFORMAT_OKLABFLOAT32) {
        if (channel == 0) {
            return sixel_pixelformat_oklab_L_to_byte(clamped);
        }
        return sixel_pixelformat_oklab_ab_to_byte(clamped);
    }
    if (pixelformat == SIXEL_PIXELFORMAT_CIELABFLOAT32) {
        if (channel == 0) {
            return sixel_pixelformat_cielab_L_to_byte(clamped);
        }
        return sixel_pixelformat_cielab_ab_to_byte(clamped);
    }
    if (pixelformat == SIXEL_PIXELFORMAT_DIN99DFLOAT32) {
        if (channel == 0) {
            return sixel_pixelformat_din99d_L_to_byte(clamped);
        }
        return sixel_pixelformat_din99d_ab_to_byte(clamped);
    }
    if (pixelformat == SIXEL_PIXELFORMAT_YUVFLOAT32) {
        if (channel == 0) {
            return sixel_pixelformat_float_to_byte(clamped);
        }
        if (channel == 1) {
            return sixel_pixelformat_yuv_chroma_to_byte(
                clamped,
                SIXEL_YUV_U_FLOAT_MAX);
        }
        return sixel_pixelformat_yuv_chroma_to_byte(clamped,
                                                    SIXEL_YUV_V_FLOAT_MAX);
    }

    (void)channel;
    return sixel_pixelformat_float_to_byte(clamped);
}

float
sixel_pixelformat_byte_to_float(int pixelformat,
                                int channel,
                                unsigned char value)
{
    float decoded;

    if (pixelformat == SIXEL_PIXELFORMAT_OKLABFLOAT32) {
        if (channel == 0) {
            return (float)value / 255.0f;
        }
        decoded = (float)value / 255.0f;
        return decoded - 0.5f;
    }
    if (pixelformat == SIXEL_PIXELFORMAT_CIELABFLOAT32) {
        if (channel == 0) {
            return (float)value / 255.0f;
        }
        decoded = (float)value / 255.0f;
        decoded = (decoded - 0.5f)
                 * (2.0f * SIXEL_CIELAB_AB_FLOAT_MAX);
        return decoded;
    }
    if (pixelformat == SIXEL_PIXELFORMAT_DIN99DFLOAT32) {
        if (channel == 0) {
            return (float)value / 255.0f;
        }
        decoded = (float)value / 255.0f;
        decoded = (decoded - 0.5f)
                 * (2.0f * SIXEL_DIN99D_AB_FLOAT_MAX);
        return decoded;
    }
    if (pixelformat == SIXEL_PIXELFORMAT_YUVFLOAT32) {
        if (channel == 0) {
            return (float)value / 255.0f;
        }
        if (channel == 1) {
            return sixel_pixelformat_yuv_chroma_from_byte(
                value,
                SIXEL_YUV_U_FLOAT_MAX);
        }
        return sixel_pixelformat_yuv_chroma_from_byte(value,
                                                      SIXEL_YUV_V_FLOAT_MAX);
    }

    (void)channel;
    return (float)value / 255.0f;
}

static void
get_rgb(unsigned char const *data,
        int const pixelformat,
        int depth,
        unsigned char *r,
        unsigned char *g,
        unsigned char *b)
{
    unsigned int pixels = 0;
#if SWAP_BYTES
    unsigned int low;
    unsigned int high;
#endif
    int count = 0;

    if (pixelformat == SIXEL_PIXELFORMAT_RGBFLOAT32
            || pixelformat == SIXEL_PIXELFORMAT_LINEARRGBFLOAT32) {
        float const *fpixels = (float const *)(void const *)data;

        *r = sixel_pixelformat_float_to_byte(fpixels[0]);
        *g = sixel_pixelformat_float_to_byte(fpixels[1]);
        *b = sixel_pixelformat_float_to_byte(fpixels[2]);
        return;
    }
    if (pixelformat == SIXEL_PIXELFORMAT_OKLABFLOAT32) {
        float const *fpixels = (float const *)(void const *)data;

        *r = sixel_pixelformat_oklab_L_to_byte(fpixels[0]);
        *g = sixel_pixelformat_oklab_ab_to_byte(fpixels[1]);
        *b = sixel_pixelformat_oklab_ab_to_byte(fpixels[2]);
        return;
    }
    if (pixelformat == SIXEL_PIXELFORMAT_CIELABFLOAT32) {
        float const *fpixels = (float const *)(void const *)data;

        *r = sixel_pixelformat_cielab_L_to_byte(fpixels[0]);
        *g = sixel_pixelformat_cielab_ab_to_byte(fpixels[1]);
        *b = sixel_pixelformat_cielab_ab_to_byte(fpixels[2]);
        return;
    }
    if (pixelformat == SIXEL_PIXELFORMAT_DIN99DFLOAT32) {
        float const *fpixels = (float const *)(void const *)data;

        *r = sixel_pixelformat_din99d_L_to_byte(fpixels[0]);
        *g = sixel_pixelformat_din99d_ab_to_byte(fpixels[1]);
        *b = sixel_pixelformat_din99d_ab_to_byte(fpixels[2]);
        return;
    }
    if (pixelformat == SIXEL_PIXELFORMAT_YUVFLOAT32) {
        float const *fpixels = (float const *)(void const *)data;

        *r = sixel_pixelformat_float_to_byte(fpixels[0]);
        *g = sixel_pixelformat_yuv_chroma_to_byte(fpixels[1],
                                                  SIXEL_YUV_U_FLOAT_MAX);
        *b = sixel_pixelformat_yuv_chroma_to_byte(fpixels[2],
                                                  SIXEL_YUV_V_FLOAT_MAX);
        return;
    }

    while (count < depth) {
        pixels = *(data + count) | (pixels << 8);
        count++;
    }

    /* TODO: we should swap bytes (only necessary on LSByte first hardware?) */
#if SWAP_BYTES
    if (depth == 2) {
        low    = pixels & 0xff;
        high   = (pixels >> 8) & 0xff;
        pixels = (low << 8) | high;
    }
#endif

    switch (pixelformat) {
    case SIXEL_PIXELFORMAT_RGB555:
        *r = ((pixels >> 10) & 0x1f) << 3;
        *g = ((pixels >>  5) & 0x1f) << 3;
        *b = ((pixels >>  0) & 0x1f) << 3;
        break;
    case SIXEL_PIXELFORMAT_RGB565:
        *r = ((pixels >> 11) & 0x1f) << 3;
        *g = ((pixels >>  5) & 0x3f) << 2;
        *b = ((pixels >>  0) & 0x1f) << 3;
        break;
    case SIXEL_PIXELFORMAT_RGB888:
        *r = (pixels >> 16) & 0xff;
        *g = (pixels >>  8) & 0xff;
        *b = (pixels >>  0) & 0xff;
        break;
    case SIXEL_PIXELFORMAT_BGR555:
        *r = ((pixels >>  0) & 0x1f) << 3;
        *g = ((pixels >>  5) & 0x1f) << 3;
        *b = ((pixels >> 10) & 0x1f) << 3;
        break;
    case SIXEL_PIXELFORMAT_BGR565:
        *r = ((pixels >>  0) & 0x1f) << 3;
        *g = ((pixels >>  5) & 0x3f) << 2;
        *b = ((pixels >> 11) & 0x1f) << 3;
        break;
    case SIXEL_PIXELFORMAT_BGR888:
        *r = (pixels >>  0) & 0xff;
        *g = (pixels >>  8) & 0xff;
        *b = (pixels >> 16) & 0xff;
        break;
    case SIXEL_PIXELFORMAT_RGBA8888:
        *r = (pixels >> 24) & 0xff;
        *g = (pixels >> 16) & 0xff;
        *b = (pixels >>  8) & 0xff;
        break;
    case SIXEL_PIXELFORMAT_ARGB8888:
        *r = (pixels >> 16) & 0xff;
        *g = (pixels >>  8) & 0xff;
        *b = (pixels >>  0) & 0xff;
        break;
    case SIXEL_PIXELFORMAT_BGRA8888:
        *r = (pixels >>  8) & 0xff;
        *g = (pixels >> 16) & 0xff;
        *b = (pixels >> 24) & 0xff;
        break;
    case SIXEL_PIXELFORMAT_ABGR8888:
        *r = (pixels >>  0) & 0xff;
        *g = (pixels >>  8) & 0xff;
        *b = (pixels >> 16) & 0xff;
        break;
    case SIXEL_PIXELFORMAT_GA88:
        *r = *g = *b = (pixels >> 8) & 0xff;
        break;
    case SIXEL_PIXELFORMAT_G8:
    case SIXEL_PIXELFORMAT_AG88:
        *r = *g = *b = pixels & 0xff;
        break;
    default:
        *r = *g = *b = 0;
        break;
    }
}


SIXELAPI int
sixel_helper_compute_depth(int pixelformat)
{
    int depth = (-1);  /* unknown */

    switch (pixelformat) {
    case SIXEL_PIXELFORMAT_ARGB8888:
    case SIXEL_PIXELFORMAT_RGBA8888:
    case SIXEL_PIXELFORMAT_ABGR8888:
    case SIXEL_PIXELFORMAT_BGRA8888:
        depth = 4;
        break;
    case SIXEL_PIXELFORMAT_RGB888:
    case SIXEL_PIXELFORMAT_BGR888:
        depth = 3;
        break;
    case SIXEL_PIXELFORMAT_RGB555:
    case SIXEL_PIXELFORMAT_RGB565:
    case SIXEL_PIXELFORMAT_BGR555:
    case SIXEL_PIXELFORMAT_BGR565:
    case SIXEL_PIXELFORMAT_AG88:
    case SIXEL_PIXELFORMAT_GA88:
        depth = 2;
        break;
    case SIXEL_PIXELFORMAT_G1:
    case SIXEL_PIXELFORMAT_G2:
    case SIXEL_PIXELFORMAT_G4:
    case SIXEL_PIXELFORMAT_G8:
    case SIXEL_PIXELFORMAT_PAL1:
    case SIXEL_PIXELFORMAT_PAL2:
    case SIXEL_PIXELFORMAT_PAL4:
    case SIXEL_PIXELFORMAT_PAL8:
        depth = 1;
        break;
    case SIXEL_PIXELFORMAT_RGBFLOAT32:
    case SIXEL_PIXELFORMAT_LINEARRGBFLOAT32:
    case SIXEL_PIXELFORMAT_OKLABFLOAT32:
    case SIXEL_PIXELFORMAT_CIELABFLOAT32:
    case SIXEL_PIXELFORMAT_DIN99DFLOAT32:
    case SIXEL_PIXELFORMAT_YUVFLOAT32:
        depth = (int)(sizeof(float) * 3);
        break;
    default:
        break;
    }

    return depth;
}


static void
expand_rgb(unsigned char *dst,
           unsigned char const *src,
           int width, int height,
           int pixelformat, int depth)
{
    int x;
    int y;
    int dst_stride;
    int src_stride;
    unsigned char const *src_row;
    unsigned char const *src_pixel;
    unsigned char *dst_row;
    unsigned char *dst_pixel;
    unsigned char r, g, b;

    /*
     * Pre-compute strides to avoid repeated multiplications in
     * the inner loop. The caller guarantees that the buffers are
     * large enough, so we can advance pointers by depth/3 bytes
     * per pixel instead of recalculating offsets each time.
     */
    dst_stride = width * 3;
    src_stride = width * depth;
    src_row = src;
    dst_row = dst;

    for (y = 0; y < height; y++) {
        src_pixel = src_row;
        dst_pixel = dst_row;
        for (x = 0; x < width; x++) {
            get_rgb(src_pixel, pixelformat, depth, &r, &g, &b);

            dst_pixel[0] = r;
            dst_pixel[1] = g;
            dst_pixel[2] = b;

            src_pixel += depth;
            dst_pixel += 3;
        }

        src_row += src_stride;
        dst_row += dst_stride;
    }
}


static SIXELSTATUS
expand_palette(unsigned char *dst, unsigned char const *src,
               int width, int height, int const pixelformat)
{
    SIXELSTATUS status = SIXEL_FALSE;
    int x;
    int y;
    int i;
    int bpp;  /* bit per plane */

    switch (pixelformat) {
    case SIXEL_PIXELFORMAT_PAL1:
    case SIXEL_PIXELFORMAT_G1:
        bpp = 1;
        break;
    case SIXEL_PIXELFORMAT_PAL2:
    case SIXEL_PIXELFORMAT_G2:
        bpp = 2;
        break;
    case SIXEL_PIXELFORMAT_PAL4:
    case SIXEL_PIXELFORMAT_G4:
        bpp = 4;
        break;
    case SIXEL_PIXELFORMAT_PAL8:
    case SIXEL_PIXELFORMAT_G8:
        for (i = 0; i < width * height; ++i, ++src) {
            *dst++ = *src;
        }
        status = SIXEL_OK;
        goto end;
    default:
        status = SIXEL_BAD_ARGUMENT;
        sixel_helper_set_additional_message(
            "expand_palette: invalid pixelformat.");
        goto end;
    }

#if HAVE_DEBUG
    fprintf(stderr, "expanding PAL%d to PAL8...\n", bpp);
#endif

    for (y = 0; y < height; ++y) {
        for (x = 0; x < width * bpp / 8; ++x) {
            for (i = 0; i < 8 / bpp; ++i) {
                *dst++ = *src >> (8 / bpp - 1 - i) * bpp & ((1 << bpp) - 1);
            }
            src++;
        }
        x = width - x * 8 / bpp;
        if (x > 0) {
            for (i = 0; i < x; ++i) {
                *dst++ = *src >> (8 - (i + 1) * bpp) & ((1 << bpp) - 1);
            }
            src++;
        }
    }

    status = SIXEL_OK;

end:
    return status;
}


SIXELAPI SIXELSTATUS
sixel_helper_normalize_pixelformat(
    unsigned char       /* out */ *dst,             /* destination buffer */
    int                 /* out */ *dst_pixelformat, /* converted pixelformat */
    unsigned char const /* in */  *src,             /* source pixels */
    int                 /* in */  src_pixelformat,  /* format of source image */
    int                 /* in */  width,            /* width of source image */
    int                 /* in */  height)           /* height of source image */
{
    SIXELSTATUS status = SIXEL_FALSE;
    int depth;

    switch (src_pixelformat) {
    case SIXEL_PIXELFORMAT_G8:
        expand_rgb(dst, src, width, height, src_pixelformat, 1);
        *dst_pixelformat = SIXEL_PIXELFORMAT_RGB888;
        break;
    case SIXEL_PIXELFORMAT_RGB565:
    case SIXEL_PIXELFORMAT_RGB555:
    case SIXEL_PIXELFORMAT_BGR565:
    case SIXEL_PIXELFORMAT_BGR555:
    case SIXEL_PIXELFORMAT_GA88:
    case SIXEL_PIXELFORMAT_AG88:
        expand_rgb(dst, src, width, height, src_pixelformat, 2);
        *dst_pixelformat = SIXEL_PIXELFORMAT_RGB888;
        break;
    case SIXEL_PIXELFORMAT_RGB888:
    case SIXEL_PIXELFORMAT_BGR888:
        expand_rgb(dst, src, width, height, src_pixelformat, 3);
        *dst_pixelformat = SIXEL_PIXELFORMAT_RGB888;
        break;
    case SIXEL_PIXELFORMAT_RGBFLOAT32:
    case SIXEL_PIXELFORMAT_LINEARRGBFLOAT32:
    case SIXEL_PIXELFORMAT_OKLABFLOAT32:
    case SIXEL_PIXELFORMAT_CIELABFLOAT32:
    case SIXEL_PIXELFORMAT_DIN99DFLOAT32:
        depth = sixel_helper_compute_depth(src_pixelformat);
        if (depth <= 0) {
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        expand_rgb(dst, src, width, height, src_pixelformat, depth);
        *dst_pixelformat = SIXEL_PIXELFORMAT_RGB888;
        break;
    case SIXEL_PIXELFORMAT_RGBA8888:
    case SIXEL_PIXELFORMAT_ARGB8888:
    case SIXEL_PIXELFORMAT_BGRA8888:
    case SIXEL_PIXELFORMAT_ABGR8888:
        expand_rgb(dst, src, width, height, src_pixelformat, 4);
        *dst_pixelformat = SIXEL_PIXELFORMAT_RGB888;
        break;
    case SIXEL_PIXELFORMAT_PAL1:
    case SIXEL_PIXELFORMAT_PAL2:
    case SIXEL_PIXELFORMAT_PAL4:
        *dst_pixelformat = SIXEL_PIXELFORMAT_PAL8;
        status = expand_palette(dst, src, width, height, src_pixelformat);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        break;
    case SIXEL_PIXELFORMAT_G1:
    case SIXEL_PIXELFORMAT_G2:
    case SIXEL_PIXELFORMAT_G4:
        *dst_pixelformat = SIXEL_PIXELFORMAT_G8;
        status = expand_palette(dst, src, width, height, src_pixelformat);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        break;
    case SIXEL_PIXELFORMAT_PAL8:
        memcpy(dst, src, (size_t)(width * height));
        *dst_pixelformat = src_pixelformat;
        break;
    default:
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    status = SIXEL_OK;

end:
    return status;
}


/* Normalize RGB888 input without modification. */
#if HAVE_TESTS
static int
pixelformat_test_rgb888_passthrough(void)
{
    unsigned char dst[3];
    int dst_pixelformat = SIXEL_PIXELFORMAT_RGB888;
    int src_pixelformat = SIXEL_PIXELFORMAT_RGB888;
    unsigned char src[] = { 0x46, 0xf3, 0xe5 };
    int ret = 0;

    int nret = EXIT_FAILURE;

    ret = sixel_helper_normalize_pixelformat(dst,
                                             &dst_pixelformat,
                                             src,
                                             src_pixelformat,
                                             1,
                                             1);
    if (ret != 0) {
        goto error;
    }
    if (dst_pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        goto error;
    }
    if ((dst[0] << 16 | dst[1] << 8 | dst[2])
            != (src[0] << 16 | src[1] << 8 | src[2])) {
        goto error;
    }
    return EXIT_SUCCESS;

error:
    perror("pixelformat_test_rgb888_passthrough");
    return nret;
}


/* Convert RGB555 packed data into RGB888 output. */
static int
pixelformat_test_from_rgb555(void)
{
    unsigned char dst[3];
    int dst_pixelformat = SIXEL_PIXELFORMAT_RGB888;
    int src_pixelformat = SIXEL_PIXELFORMAT_RGB555;
    unsigned char src[] = { 0x47, 0x9c };
    int ret = 0;

    int nret = EXIT_FAILURE;

    ret = sixel_helper_normalize_pixelformat(dst,
                                             &dst_pixelformat,
                                             src,
                                             src_pixelformat,
                                             1,
                                             1);
    if (ret != 0) {
        goto error;
    }
    if (dst_pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        goto error;
    }
    if ((dst[0] >> 3 << 10 | dst[1] >> 3 << 5 | dst[2] >> 3)
            != (src[0] << 8 | src[1])) {
        goto error;
    }
    return EXIT_SUCCESS;

error:
    perror("pixelformat_test_from_rgb555");
    return nret;
}


/* Convert RGB565 packed data into RGB888 output. */
static int
pixelformat_test_from_rgb565(void)
{
    unsigned char dst[3];
    int dst_pixelformat = SIXEL_PIXELFORMAT_RGB888;
    int src_pixelformat = SIXEL_PIXELFORMAT_RGB565;
    unsigned char src[] = { 0x47, 0x9c };
    int ret = 0;

    int nret = EXIT_FAILURE;

    ret = sixel_helper_normalize_pixelformat(dst,
                                             &dst_pixelformat,
                                             src,
                                             src_pixelformat,
                                             1,
                                             1);
    if (ret != 0) {
        goto error;
    }
    if (dst_pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        goto error;
    }
    if ((dst[0] >> 3 << 11 | dst[1] >> 2 << 5 | dst[2] >> 3)
            != (src[0] << 8 | src[1])) {
        goto error;
    }
    return EXIT_SUCCESS;

error:
    perror("pixelformat_test_from_rgb565");
    return nret;
}


/* Swap channels from BGR888 to RGB888. */
static int
pixelformat_test_from_bgr888(void)
{
    unsigned char dst[3];
    int dst_pixelformat = SIXEL_PIXELFORMAT_RGB888;
    int src_pixelformat = SIXEL_PIXELFORMAT_BGR888;
    unsigned char src[] = { 0x46, 0xf3, 0xe5 };
    int ret = 0;

    int nret = EXIT_FAILURE;

    ret = sixel_helper_normalize_pixelformat(dst,
                                             &dst_pixelformat,
                                             src,
                                             src_pixelformat,
                                             1,
                                             1);
    if (ret != 0) {
        goto error;
    }
    if (dst_pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        goto error;
    }
    if ((dst[2] << 16 | dst[1] << 8 | dst[0])
            != (src[0] << 16 | src[1] << 8 | src[2])) {
        goto error;
    }
    return EXIT_SUCCESS;

error:
    perror("pixelformat_test_from_bgr888");
    return nret;
}


/* Convert BGR555 packed data into RGB888 output. */
static int
pixelformat_test_from_bgr555(void)
{
    unsigned char dst[3];
    int dst_pixelformat = SIXEL_PIXELFORMAT_RGB888;
    int src_pixelformat = SIXEL_PIXELFORMAT_BGR555;
    unsigned char src[] = { 0x23, 0xc8 };
    int ret = 0;

    int nret = EXIT_FAILURE;

    ret = sixel_helper_normalize_pixelformat(dst,
                                             &dst_pixelformat,
                                             src,
                                             src_pixelformat,
                                             1,
                                             1);
    if (ret != 0) {
        goto error;
    }
    if (dst_pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        goto error;
    }
    if ((dst[2] >> 3 << 10 | dst[1] >> 3 << 5 | dst[0] >> 3)
            != (src[0] << 8 | src[1])) {
        goto error;
    }
    return EXIT_SUCCESS;

error:
    perror("pixelformat_test_from_bgr555");
    return nret;
}


/* Convert BGR565 packed data into RGB888 output. */
static int
pixelformat_test_from_bgr565(void)
{
    unsigned char dst[3];
    int dst_pixelformat = SIXEL_PIXELFORMAT_RGB888;
    int src_pixelformat = SIXEL_PIXELFORMAT_BGR565;
    unsigned char src[] = { 0x47, 0x88 };
    int ret = 0;

    int nret = EXIT_FAILURE;

    ret = sixel_helper_normalize_pixelformat(dst,
                                             &dst_pixelformat,
                                             src,
                                             src_pixelformat,
                                             1,
                                             1);
    if (ret != 0) {
        goto error;
    }
    if (dst_pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        goto error;
    }
    if ((dst[2] >> 3 << 11 | dst[1] >> 2 << 5 | dst[0] >> 3)
            != (src[0] << 8 | src[1])) {
        goto error;
    }
    return EXIT_SUCCESS;

error:
    perror("pixelformat_test_from_bgr565");
    return nret;
}


/* Convert AG88 data by discarding alpha and keeping gray. */
static int
pixelformat_test_from_ag88(void)
{
    unsigned char dst[3];
    int dst_pixelformat = SIXEL_PIXELFORMAT_RGB888;
    int src_pixelformat = SIXEL_PIXELFORMAT_AG88;
    unsigned char src[] = { 0x47, 0x88 };
    int ret = 0;

    int nret = EXIT_FAILURE;

    ret = sixel_helper_normalize_pixelformat(dst,
                                             &dst_pixelformat,
                                             src,
                                             src_pixelformat,
                                             1,
                                             1);
    if (ret != 0) {
        goto error;
    }
    if (dst_pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        goto error;
    }
    if (dst[0] != src[1]) {
        goto error;
    }
    return EXIT_SUCCESS;

error:
    perror("pixelformat_test_from_ag88");
    return nret;
}


/* Convert GA88 data by duplicating gray channel into RGB. */
static int
pixelformat_test_from_ga88(void)
{
    unsigned char dst[3];
    int dst_pixelformat = SIXEL_PIXELFORMAT_RGB888;
    int src_pixelformat = SIXEL_PIXELFORMAT_GA88;
    unsigned char src[] = { 0x47, 0x88 };
    int ret = 0;

    int nret = EXIT_FAILURE;

    ret = sixel_helper_normalize_pixelformat(dst,
                                             &dst_pixelformat,
                                             src,
                                             src_pixelformat,
                                             1,
                                             1);
    if (ret != 0) {
        goto error;
    }
    if (dst_pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        goto error;
    }
    if (dst[0] != src[0]) {
        goto error;
    }
    return EXIT_SUCCESS;

error:
    perror("pixelformat_test_from_ga88");
    return nret;
}


/* Normalize RGBA8888 by dropping alpha. */
static int
pixelformat_test_from_rgba8888(void)
{
    unsigned char dst[3];
    int dst_pixelformat = SIXEL_PIXELFORMAT_RGB888;
    int src_pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
    unsigned char src[] = { 0x46, 0xf3, 0xe5, 0xf0 };
    int ret = 0;

    int nret = EXIT_FAILURE;

    ret = sixel_helper_normalize_pixelformat(dst,
                                             &dst_pixelformat,
                                             src,
                                             src_pixelformat,
                                             1,
                                             1);
    if (ret != 0) {
        goto error;
    }
    if (dst_pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        goto error;
    }
    if (dst[0] != src[0]) {
        goto error;
    }
    if (dst[1] != src[1]) {
        goto error;
    }
    if (dst[2] != src[2]) {
        goto error;
    }
    return EXIT_SUCCESS;

error:
    perror("pixelformat_test_from_rgba8888");
    return nret;
}


/* Normalize ARGB8888 while skipping the leading alpha byte. */
static int
pixelformat_test_from_argb8888(void)
{
    unsigned char dst[3];
    int dst_pixelformat = SIXEL_PIXELFORMAT_RGB888;
    int src_pixelformat = SIXEL_PIXELFORMAT_ARGB8888;
    unsigned char src[] = { 0x46, 0xf3, 0xe5, 0xf0 };
    int ret = 0;

    int nret = EXIT_FAILURE;

    ret = sixel_helper_normalize_pixelformat(dst,
                                             &dst_pixelformat,
                                             src,
                                             src_pixelformat,
                                             1,
                                             1);
    if (ret != 0) {
        goto error;
    }
    if (dst_pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        goto error;
    }
    if (dst[0] != src[1]) {
        goto error;
    }
    if (dst[1] != src[2]) {
        goto error;
    }
    if (dst[2] != src[3]) {
        goto error;
    }
    return EXIT_SUCCESS;

error:
    perror("pixelformat_test_from_argb8888");
    return nret;
}


/* Convert floating point RGB data to normalized 8-bit output. */
static int
pixelformat_test_from_rgbfloat32(void)
{
    unsigned char dst[3];
    int dst_pixelformat = SIXEL_PIXELFORMAT_RGB888;
    int src_pixelformat = SIXEL_PIXELFORMAT_RGBFLOAT32;
    float srcf[] = { 0.0f, 0.5f, 1.0f };
    unsigned char const *src = (unsigned char const *)srcf;
    int ret = 0;
    int depth;

    int nret = EXIT_FAILURE;

    ret = sixel_helper_normalize_pixelformat(dst,
                                             &dst_pixelformat,
                                             src,
                                             src_pixelformat,
                                             1,
                                             1);
    if (ret != 0) {
        goto error;
    }
    if (dst_pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        goto error;
    }
    if (dst[0] != 0 || dst[1] != 128 || dst[2] != 255) {
        goto error;
    }
    depth = sixel_helper_compute_depth(src_pixelformat);
    if (depth != (int)(sizeof(float) * 3)) {
        goto error;
    }
    return EXIT_SUCCESS;

error:
    perror("pixelformat_test_from_rgbfloat32");
    return nret;
}


SIXELAPI int
sixel_pixelformat_tests_main(void)
{
    int nret = EXIT_FAILURE;
    size_t i;
    typedef int (* testcase)(void);

    static testcase const testcases[] = {
        pixelformat_test_rgb888_passthrough,
        pixelformat_test_from_rgb555,
        pixelformat_test_from_rgb565,
        pixelformat_test_from_bgr888,
        pixelformat_test_from_bgr555,
        pixelformat_test_from_bgr565,
        pixelformat_test_from_ag88,
        pixelformat_test_from_ga88,
        pixelformat_test_from_rgba8888,
        pixelformat_test_from_argb8888,
        pixelformat_test_from_rgbfloat32,
    };

    for (i = 0; i < sizeof(testcases) / sizeof(testcase); ++i) {
        nret = testcases[i]();
        if (nret != EXIT_SUCCESS) {
            goto error;
        }
    }

    nret = EXIT_SUCCESS;

error:
    return nret;
}
#endif  /* HAVE_TESTS */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
