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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

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

#include "compat_stub.h"
#include "threading.h"
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

float
sixel_pixelformat_float_channel_min(int pixelformat,
                                    int channel)
{
    return sixel_pixelformat_float_channel_min_internal(pixelformat,
                                                        channel);
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
sixel_pixelformat_float_channel_max(int pixelformat,
                                    int channel)
{
    return sixel_pixelformat_float_channel_max_internal(pixelformat,
                                                        channel);
}

float
sixel_pixelformat_float_channel_range(int pixelformat,
                                      int channel)
{
    float minimum;
    float maximum;
    float range;

    minimum = sixel_pixelformat_float_channel_min_internal(pixelformat,
                                                           channel);
    maximum = sixel_pixelformat_float_channel_max_internal(pixelformat,
                                                           channel);
    range = maximum - minimum;
    if (range <= 0.0f) {
        range = 1.0f;
    }
    return range;
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

typedef void (*sixel_rgb_reader_t)(unsigned char const *data,
                                    unsigned char *r,
                                    unsigned char *g,
                                    unsigned char *b);


static unsigned int
sixel_rgb_read16(unsigned char const *data)
{
    unsigned int pixels;
#if SWAP_BYTES
    unsigned int low;
    unsigned int high;
#endif

    pixels = ((unsigned int)data[0] << 8) | (unsigned int)data[1];

#if SWAP_BYTES
    low = pixels & 0xff;
    high = (pixels >> 8) & 0xff;
    pixels = (low << 8) | high;
#endif

    return pixels;
}


static void
sixel_rgb_from_rgb555(unsigned char const *data,
                      unsigned char *r,
                      unsigned char *g,
                      unsigned char *b)
{
    unsigned int pixels;

    pixels = sixel_rgb_read16(data);

    *r = ((pixels >> 10) & 0x1f) << 3;
    *g = ((pixels >> 5) & 0x1f) << 3;
    *b = ((pixels >> 0) & 0x1f) << 3;
}


static void
sixel_rgb_from_rgb565(unsigned char const *data,
                      unsigned char *r,
                      unsigned char *g,
                      unsigned char *b)
{
    unsigned int pixels;

    pixels = sixel_rgb_read16(data);

    *r = ((pixels >> 11) & 0x1f) << 3;
    *g = ((pixels >> 5) & 0x3f) << 2;
    *b = ((pixels >> 0) & 0x1f) << 3;
}


static void
sixel_rgb_from_bgr555(unsigned char const *data,
                      unsigned char *r,
                      unsigned char *g,
                      unsigned char *b)
{
    unsigned int pixels;

    pixels = sixel_rgb_read16(data);

    *r = ((pixels >> 0) & 0x1f) << 3;
    *g = ((pixels >> 5) & 0x1f) << 3;
    *b = ((pixels >> 10) & 0x1f) << 3;
}


static void
sixel_rgb_from_bgr565(unsigned char const *data,
                      unsigned char *r,
                      unsigned char *g,
                      unsigned char *b)
{
    unsigned int pixels;

    pixels = sixel_rgb_read16(data);

    *r = ((pixels >> 0) & 0x1f) << 3;
    *g = ((pixels >> 5) & 0x3f) << 2;
    *b = ((pixels >> 11) & 0x1f) << 3;
}


static void
sixel_rgb_from_ga88(unsigned char const *data,
                    unsigned char *r,
                    unsigned char *g,
                    unsigned char *b)
{
    unsigned int pixels;

    pixels = sixel_rgb_read16(data);

    *r = (pixels >> 8) & 0xff;
    *g = (pixels >> 8) & 0xff;
    *b = (pixels >> 8) & 0xff;
}


static void
sixel_rgb_from_ag88(unsigned char const *data,
                    unsigned char *r,
                    unsigned char *g,
                    unsigned char *b)
{
    unsigned int pixels;

    pixels = sixel_rgb_read16(data);

    *r = pixels & 0xff;
    *g = pixels & 0xff;
    *b = pixels & 0xff;
}


static void
sixel_rgb_from_rgb888(unsigned char const *data,
                      unsigned char *r,
                      unsigned char *g,
                      unsigned char *b)
{
    *r = data[0];
    *g = data[1];
    *b = data[2];
}


static void
sixel_rgb_from_bgr888(unsigned char const *data,
                      unsigned char *r,
                      unsigned char *g,
                      unsigned char *b)
{
    *r = data[2];
    *g = data[1];
    *b = data[0];
}


static void
sixel_rgb_from_rgba8888(unsigned char const *data,
                        unsigned char *r,
                        unsigned char *g,
                        unsigned char *b)
{
    *r = data[0];
    *g = data[1];
    *b = data[2];
}


static void
sixel_rgb_from_argb8888(unsigned char const *data,
                        unsigned char *r,
                        unsigned char *g,
                        unsigned char *b)
{
    *r = data[1];
    *g = data[2];
    *b = data[3];
}


static void
sixel_rgb_from_bgra8888(unsigned char const *data,
                        unsigned char *r,
                        unsigned char *g,
                        unsigned char *b)
{
    *r = data[2];
    *g = data[1];
    *b = data[0];
}


static void
sixel_rgb_from_abgr8888(unsigned char const *data,
                        unsigned char *r,
                        unsigned char *g,
                        unsigned char *b)
{
    *r = data[3];
    *g = data[2];
    *b = data[1];
}


static void
sixel_rgb_from_g8(unsigned char const *data,
                  unsigned char *r,
                  unsigned char *g,
                  unsigned char *b)
{
    *r = data[0];
    *g = data[0];
    *b = data[0];
}


static void
sixel_rgb_from_rgbfloat32(unsigned char const *data,
                          unsigned char *r,
                          unsigned char *g,
                          unsigned char *b)
{
    float const *fpixels;

    fpixels = (float const *)(void const *)data;

    *r = sixel_pixelformat_float_to_byte(fpixels[0]);
    *g = sixel_pixelformat_float_to_byte(fpixels[1]);
    *b = sixel_pixelformat_float_to_byte(fpixels[2]);
}


static void
sixel_rgb_from_oklabfloat32(unsigned char const *data,
                            unsigned char *r,
                            unsigned char *g,
                            unsigned char *b)
{
    float const *fpixels;

    fpixels = (float const *)(void const *)data;

    *r = sixel_pixelformat_oklab_L_to_byte(fpixels[0]);
    *g = sixel_pixelformat_oklab_ab_to_byte(fpixels[1]);
    *b = sixel_pixelformat_oklab_ab_to_byte(fpixels[2]);
}


static void
sixel_rgb_from_cielabfloat32(unsigned char const *data,
                             unsigned char *r,
                             unsigned char *g,
                             unsigned char *b)
{
    float const *fpixels;

    fpixels = (float const *)(void const *)data;

    *r = sixel_pixelformat_cielab_L_to_byte(fpixels[0]);
    *g = sixel_pixelformat_cielab_ab_to_byte(fpixels[1]);
    *b = sixel_pixelformat_cielab_ab_to_byte(fpixels[2]);
}


static void
sixel_rgb_from_din99dfloat32(unsigned char const *data,
                             unsigned char *r,
                             unsigned char *g,
                             unsigned char *b)
{
    float const *fpixels;

    fpixels = (float const *)(void const *)data;

    *r = sixel_pixelformat_din99d_L_to_byte(fpixels[0]);
    *g = sixel_pixelformat_din99d_ab_to_byte(fpixels[1]);
    *b = sixel_pixelformat_din99d_ab_to_byte(fpixels[2]);
}


static void
sixel_rgb_from_yuvfloat32(unsigned char const *data,
                          unsigned char *r,
                          unsigned char *g,
                          unsigned char *b)
{
    float const *fpixels;

    fpixels = (float const *)(void const *)data;

    *r = sixel_pixelformat_float_to_byte(fpixels[0]);
    *g = sixel_pixelformat_yuv_chroma_to_byte(fpixels[1],
                                              SIXEL_YUV_U_FLOAT_MAX);
    *b = sixel_pixelformat_yuv_chroma_to_byte(fpixels[2],
                                              SIXEL_YUV_V_FLOAT_MAX);
}


static void
sixel_rgb_from_unknown(unsigned char const *data,
                       unsigned char *r,
                       unsigned char *g,
                       unsigned char *b)
{
    (void)data;

    *r = 0;
    *g = 0;
    *b = 0;
}


static sixel_rgb_reader_t
sixel_select_rgb_reader(int pixelformat)
{
    switch (pixelformat) {
    case SIXEL_PIXELFORMAT_RGB555:
        return sixel_rgb_from_rgb555;
    case SIXEL_PIXELFORMAT_RGB565:
        return sixel_rgb_from_rgb565;
    case SIXEL_PIXELFORMAT_RGB888:
        return sixel_rgb_from_rgb888;
    case SIXEL_PIXELFORMAT_RGBA8888:
        return sixel_rgb_from_rgba8888;
    case SIXEL_PIXELFORMAT_ARGB8888:
        return sixel_rgb_from_argb8888;
    case SIXEL_PIXELFORMAT_BGR555:
        return sixel_rgb_from_bgr555;
    case SIXEL_PIXELFORMAT_BGR565:
        return sixel_rgb_from_bgr565;
    case SIXEL_PIXELFORMAT_BGR888:
        return sixel_rgb_from_bgr888;
    case SIXEL_PIXELFORMAT_BGRA8888:
        return sixel_rgb_from_bgra8888;
    case SIXEL_PIXELFORMAT_ABGR8888:
        return sixel_rgb_from_abgr8888;
    case SIXEL_PIXELFORMAT_AG88:
        return sixel_rgb_from_ag88;
    case SIXEL_PIXELFORMAT_GA88:
        return sixel_rgb_from_ga88;
    case SIXEL_PIXELFORMAT_G8:
        return sixel_rgb_from_g8;
    case SIXEL_PIXELFORMAT_RGBFLOAT32:
    case SIXEL_PIXELFORMAT_LINEARRGBFLOAT32:
        return sixel_rgb_from_rgbfloat32;
    case SIXEL_PIXELFORMAT_OKLABFLOAT32:
        return sixel_rgb_from_oklabfloat32;
    case SIXEL_PIXELFORMAT_CIELABFLOAT32:
        return sixel_rgb_from_cielabfloat32;
    case SIXEL_PIXELFORMAT_DIN99DFLOAT32:
        return sixel_rgb_from_din99dfloat32;
    case SIXEL_PIXELFORMAT_YUVFLOAT32:
        return sixel_rgb_from_yuvfloat32;
    default:
        break;
    }

    return sixel_rgb_from_unknown;
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
expand_rgb(unsigned char *restrict dst,
           unsigned char const *restrict src,
           int width, int height,
           int pixelformat, int depth)
{
    int x;
    int y;
    int dst_stride;
    int src_stride;
    sixel_rgb_reader_t reader;
    unsigned char const *src_row;
    unsigned char const *src_pixel;
    unsigned char *dst_row;
    unsigned char *dst_pixel;
    unsigned char r;
    unsigned char g;
    unsigned char b;

    /*
     * Select the reader once to avoid per-pixel branching. The lookup
     * maps each pixelformat to a dedicated decoder so the inner loop
     * only performs pointer math and byte stores.
     */
    reader = sixel_select_rgb_reader(pixelformat);

    /*
     * Pre-compute strides to avoid repeated multiplications in the
     * inner loop. The caller guarantees that the buffers are large
     * enough, so we can advance pointers by depth/3 bytes per pixel
     * instead of recalculating offsets each time.
     */
    dst_stride = width * 3;
    src_stride = width * depth;
    src_row = src;
    dst_row = dst;

    for (y = 0; y < height; y++) {
        src_pixel = src_row;
        dst_pixel = dst_row;
        for (x = 0; x < width; x++) {
            reader(src_pixel, &r, &g, &b);

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


/*
 * Lookup tables for expanding packed palette indices. Each entry holds
 * the unpacked values for one input byte so the inner loops only copy
 * precomputed bytes instead of shifting each pixel.
 */
static unsigned char palette_table1[256][8];
static unsigned char palette_table2[256][4];
static unsigned char palette_table4[256][2];
static int palette_table_initialized;
static sixel_mutex_t palette_table_mutex;
static int palette_table_mutex_ready;


static int
sixel_init_palette_tables(void)
{
    char const *disable_tables;
    int value;
    int i;
    int init_result;

    /*
     * Allow tests to force the shift-based path by disabling table
     * initialization via SIXEL_PALETTE_DISABLE_TABLES. This exercises
     * the fallback without introducing additional code paths in
     * production builds.
     */
    disable_tables = sixel_compat_getenv(
            "SIXEL_PALETTE_DISABLE_TABLES");
    if (disable_tables != NULL && disable_tables[0] != '\0' &&
            disable_tables[0] != '0') {
        return 0;
    }

    /*
     * Tables are generated once on first use to avoid increasing the
     * binary size with large static initializers.
     */
    if (palette_table_initialized) {
        return 1;
    }

    if (palette_table_mutex_ready == 0) {
        init_result = sixel_mutex_init(&palette_table_mutex);
        if (init_result == 0) {
            palette_table_mutex_ready = 1;
        } else {
            palette_table_mutex_ready = -1;
        }
    }

    if (palette_table_mutex_ready < 0) {
        /*
         * Without a mutex we cannot guarantee a race-free initialization.
         * Defer to the shift-based fallback path so multiple threads do not
         * write the static tables concurrently.
         */
        return 0;
    }

    if (palette_table_mutex_ready == 1) {
        sixel_mutex_lock(&palette_table_mutex);
        if (palette_table_initialized) {
            sixel_mutex_unlock(&palette_table_mutex);
            return 1;
        }
    }

    for (value = 0; value < 256; ++value) {
        for (i = 0; i < 8; ++i) {
            palette_table1[value][i] =
                (unsigned char)((value >> (7 - i)) & 0x01);
        }

        for (i = 0; i < 4; ++i) {
            palette_table2[value][i] =
                (unsigned char)((value >> (6 - i * 2)) & 0x03);
        }

        for (i = 0; i < 2; ++i) {
            palette_table4[value][i] =
                (unsigned char)((value >> (4 - i * 4)) & 0x0f);
        }
    }

    palette_table_initialized = 1;

    /*
     * Release the mutex after the single initialization pass so later calls
     * can reuse the tables without redundant locking.
     */
    sixel_mutex_unlock(&palette_table_mutex);

    return 1;
}


/*
 * Expand packed 1 bpp rows by copying a precomputed 8-pixel block per
 * source byte. A tiny tail loop handles the remainder when width is not
 * divisible by 8.
 */
static void
sixel_expand_palette_bpp1(unsigned char *restrict dst,
                          unsigned char const *restrict src,
                          int width, int height)
{
    int y;
    int x;
    int remainder;
    int byte_count;
    unsigned char const *table_entry;

    byte_count = width / 8;
    remainder = width - byte_count * 8;

    if (remainder == 0) {
        /*
         * Fast path for byte-aligned rows. Removing the per-row
         * remainder branch keeps the steady-state inner loop tight.
         */
        for (y = 0; y < height; ++y) {
            for (x = 0; x < byte_count; ++x) {
                table_entry = palette_table1[src[0]];
                memcpy(dst, table_entry, 8);
                dst += 8;
                src += 1;
            }
        }
    } else {
        /*
         * Handle rows with a short tail. The main loop still copies a
         * precomputed 8-pixel block per byte while the tail is expanded
         * via a short memcpy so the steady-state loop remains branch free.
         */
        for (y = 0; y < height; ++y) {
            for (x = 0; x < byte_count; ++x) {
                table_entry = palette_table1[src[0]];
                memcpy(dst, table_entry, 8);
                dst += 8;
                src += 1;
            }

            table_entry = palette_table1[src[0]];
            memcpy(dst, table_entry, (size_t)remainder);
            dst += remainder;
            src += 1;
        }
    }
}


/*
 * Expand packed 2 bpp rows. Each lookup yields four pixels so the inner
 * loop becomes a memcpy per byte, followed by a small tail when the row
 * width leaves a remainder.
 */
static void
sixel_expand_palette_bpp2(unsigned char *restrict dst,
                          unsigned char const *restrict src,
                          int width, int height)
{
    int y;
    int x;
    int remainder;
    int byte_count;
    unsigned char const *table_entry;

    byte_count = width / 4;
    remainder = width - byte_count * 4;

    if (remainder == 0) {
        /*
         * Width aligned to 4 pixels: skip the tail branch and keep the
         * inner loop limited to memcpy plus pointer bumps.
         */
        for (y = 0; y < height; ++y) {
            for (x = 0; x < byte_count; ++x) {
                table_entry = palette_table2[src[0]];
                memcpy(dst, table_entry, 4);
                dst += 4;
                src += 1;
            }
        }
    } else {
        /*
         * Non-multiple-of-four widths still use the table for the bulk
         * of each row and append the remaining pixels via a short memcpy.
         */
        for (y = 0; y < height; ++y) {
            for (x = 0; x < byte_count; ++x) {
                table_entry = palette_table2[src[0]];
                memcpy(dst, table_entry, 4);
                dst += 4;
                src += 1;
            }

            table_entry = palette_table2[src[0]];
            memcpy(dst, table_entry, (size_t)remainder);
            dst += remainder;
            src += 1;
        }
    }
}


/*
 * Expand packed 4 bpp rows using two-pixel lookup entries. Like the
 * other helpers, the remainder loop only executes when the row width is
 * odd.
 */
static void
sixel_expand_palette_bpp4(unsigned char *restrict dst,
                          unsigned char const *restrict src,
                          int width, int height)
{
    int y;
    int x;
    int remainder;
    int byte_count;
    unsigned char const *table_entry;

    byte_count = width / 2;
    remainder = width - byte_count * 2;

    if (remainder == 0) {
        /*
         * When width is an even number of pixels the loop becomes a
         * pure memcpy stream with no per-row branching.
         */
        for (y = 0; y < height; ++y) {
            for (x = 0; x < byte_count; ++x) {
                table_entry = palette_table4[src[0]];
                memcpy(dst, table_entry, 2);
                dst += 2;
                src += 1;
            }
        }
    } else {
        /*
         * Otherwise process the bulk via the lookup table and append the
         * one remaining pixel with a short memcpy.
         */
        for (y = 0; y < height; ++y) {
            for (x = 0; x < byte_count; ++x) {
                table_entry = palette_table4[src[0]];
                memcpy(dst, table_entry, 2);
                dst += 2;
                src += 1;
            }

            table_entry = palette_table4[src[0]];
            memcpy(dst, table_entry, (size_t)remainder);
            dst += remainder;
            src += 1;
        }
    }
}


/*
 * Fallback path that mirrors the original shift-and-mask expansion for
 * packed palette formats. This is selected when the lookup tables cannot be
 * initialized, preserving correctness without concurrent writes to the
 * static buffers.
 */
static void
sixel_expand_palette_fallback(unsigned char *restrict dst,
                              unsigned char const *restrict src,
                              int width,
                              int height,
                              int bpp)
{
    int x;
    int y;
    int i;
    int bytes_per_row;
    int remainder;
    int bits_per_byte;
    int mask;

    bits_per_byte = 8 / bpp;
    mask = (1 << bpp) - 1;
    bytes_per_row = width * bpp / 8;
    remainder = width - bytes_per_row * bits_per_byte;

    for (y = 0; y < height; ++y) {
        for (x = 0; x < bytes_per_row; ++x) {
            for (i = 0; i < bits_per_byte; ++i) {
                *dst++ = (unsigned char)((src[0] >>
                    (bits_per_byte - 1 - i) * bpp) & mask);
            }
            ++src;
        }

        if (remainder > 0) {
            for (i = 0; i < remainder; ++i) {
                *dst++ = (unsigned char)((src[0] >>
                    (bits_per_byte * bpp - (i + 1) * bpp)) & mask);
            }
            ++src;
        }
    }
}


static SIXELSTATUS
expand_palette(unsigned char *restrict dst,
               unsigned char const *restrict src,
               int width, int height, int const pixelformat)
{
    SIXELSTATUS status = SIXEL_FALSE;
    int bpp;  /* bit per plane */
    int use_palette_tables;
    int tables_ready;
    size_t total_pixels;

    /*
     * Reject empty dimensions early. An empty row or column would make the
     * byte count calculations negative and does not represent a valid image
     * to expand.
     */
    if (width <= 0 || height <= 0) {
        sixel_helper_set_additional_message(
            "expand_palette: width and height must be positive.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    use_palette_tables = 0;
    tables_ready = 0;

    switch (pixelformat) {
    case SIXEL_PIXELFORMAT_PAL1:
    case SIXEL_PIXELFORMAT_G1:
        bpp = 1;
        use_palette_tables = 1;
        break;
    case SIXEL_PIXELFORMAT_PAL2:
    case SIXEL_PIXELFORMAT_G2:
        bpp = 2;
        use_palette_tables = 1;
        break;
    case SIXEL_PIXELFORMAT_PAL4:
    case SIXEL_PIXELFORMAT_G4:
        bpp = 4;
        use_palette_tables = 1;
        break;
    case SIXEL_PIXELFORMAT_PAL8:
    case SIXEL_PIXELFORMAT_G8:
        total_pixels = (size_t)width * (size_t)height;

        /*
         * Direct copy for already expanded 8 bpp sources. Using memcpy
         * avoids the per-pixel loop overhead when the input is byte
         * aligned and requires no bit unpacking.
         */
        memcpy(dst, src, total_pixels);
        status = SIXEL_OK;
        goto end;
    default:
        status = SIXEL_BAD_ARGUMENT;
        sixel_helper_set_additional_message(
            "expand_palette: invalid pixelformat.");
        goto end;
    }

    if (use_palette_tables) {
        /*
         * Initialize lookup tables only when packed palette input is
         * present. Formats that are already 8 bpp avoid the setup cost.
         */
        tables_ready = sixel_init_palette_tables();
    }

#if HAVE_DEBUG
    fprintf(stderr, "expanding PAL%d to PAL8...\n", bpp);
#endif

    if (tables_ready) {
        /*
         * Use lookup tables to unroll packed indices. Each path copies an
         * entire byte of indices in one memcpy, leaving only a small
         * remainder loop per row for widths that are not byte-aligned.
         */
        switch (bpp) {
        case 1:
            sixel_expand_palette_bpp1(dst, src, width, height);
            status = SIXEL_OK;
            break;
        case 2:
            sixel_expand_palette_bpp2(dst, src, width, height);
            status = SIXEL_OK;
            break;
        case 4:
            sixel_expand_palette_bpp4(dst, src, width, height);
            status = SIXEL_OK;
            break;
        default:
            status = SIXEL_BAD_ARGUMENT;
            break;
        }
    } else {
        /*
         * Mutex initialization failed or tables are unavailable.
         * Fall back to the original shift-based expansion to avoid
         * concurrent writes to the static lookup buffers.
         */
        sixel_expand_palette_fallback(dst, src, width, height, bpp);
        status = SIXEL_OK;
    }

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

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
