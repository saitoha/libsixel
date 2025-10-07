/*
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
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

#include <math.h>
#include <stddef.h>

#include <sixel.h>

#include "colorspace.h"

#define SIXEL_COLORSPACE_LUT_SIZE 256
#define SIXEL_OKLAB_AB_OFFSET 0.5
#define SIXEL_OKLAB_AB_SCALE  255.0

static const double sixel_linear_srgb_to_smptec_matrix[3][3] = {
    { 1.0651944799343782, -0.05539144537002962, -0.009975616485882548 },
    { -0.019633066659433226,  1.0363870284433383, -0.016731961783904975 },
    { 0.0016324889176928742,  0.004413466273704836,  0.994192644808602 }
};

static const double sixel_linear_smptec_to_srgb_matrix[3][3] = {
    { 0.9397048483892231,  0.05018036042570272,  0.010273409684415205 },
    { 0.01777536262173348, 0.9657705626655305,  0.01643197976410589 },
    { -0.0016219271954016755, -0.00436969856687614,  1.0057514450874723 }
};

static unsigned char gamma_to_linear_lut[SIXEL_COLORSPACE_LUT_SIZE];
static unsigned char linear_to_gamma_lut[SIXEL_COLORSPACE_LUT_SIZE];
static int tables_initialized = 0;

static inline double
sixel_clamp_unit(double value)
{
    if (value < 0.0) {
        return 0.0;
    }
    if (value > 1.0) {
        return 1.0;
    }
    return value;
}

static unsigned char
sixel_colorspace_clamp(int value)
{
    if (value < 0) {
        return 0;
    }
    if (value > 255) {
        return 255;
    }
    return (unsigned char)value;
}

static inline double
sixel_srgb_to_linear_double(unsigned char v)
{
    double x = (double)v / 255.0;

    if (x <= 0.04045) {
        return x / 12.92;
    }

    return pow((x + 0.055) / 1.055, 2.4);
}

static inline unsigned char
sixel_linear_double_to_srgb(double v)
{
    double y;

    if (v <= 0.0) {
        return 0;
    }
    if (v >= 1.0) {
        return 255;
    }

    if (v <= 0.0031308) {
        y = v * 12.92;
    } else {
        y = 1.055 * pow(v, 1.0 / 2.4) - 0.055;
    }

    return sixel_colorspace_clamp((int)(y * 255.0 + 0.5));
}

static inline unsigned char
sixel_linear_double_to_byte(double v)
{
    if (v <= 0.0) {
        return 0;
    }
    if (v >= 1.0) {
        return 255;
    }

    return sixel_colorspace_clamp((int)(v * 255.0 + 0.5));
}

static inline double
sixel_smptec_to_linear_double(unsigned char v)
{
    double x = (double)v / 255.0;

    if (x <= 0.0) {
        return 0.0;
    }
    if (x >= 1.0) {
        return 1.0;
    }

    return pow(x, 2.2);
}

static inline unsigned char
sixel_linear_double_to_smptec(double v)
{
    double y;

    if (v <= 0.0) {
        return 0;
    }
    if (v >= 1.0) {
        return 255;
    }

    y = pow(v, 1.0 / 2.2);
    return sixel_colorspace_clamp((int)(y * 255.0 + 0.5));
}

static inline unsigned char
sixel_oklab_encode_L(double L)
{
    if (L < 0.0) {
        L = 0.0;
    } else if (L > 1.0) {
        L = 1.0;
    }

    return sixel_colorspace_clamp((int)(L * 255.0 + 0.5));
}

static inline unsigned char
sixel_oklab_encode_ab(double value)
{
    double encoded = value + SIXEL_OKLAB_AB_OFFSET;

    if (encoded < 0.0) {
        encoded = 0.0;
    } else if (encoded > 1.0) {
        encoded = 1.0;
    }

    return sixel_colorspace_clamp((int)(encoded * SIXEL_OKLAB_AB_SCALE + 0.5));
}

static inline double
sixel_oklab_decode_ab(unsigned char v)
{
    return (double)v / SIXEL_OKLAB_AB_SCALE - SIXEL_OKLAB_AB_OFFSET;
}

static void
sixel_linear_to_oklab(double r, double g, double b,
                      double *L, double *A, double *B)
{
    double l;
    double m;
    double s;
    double l_;
    double m_;
    double s_;

    l = 0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b;
    m = 0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b;
    s = 0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b;

    l_ = cbrt(l);
    m_ = cbrt(m);
    s_ = cbrt(s);

    *L = 0.2104542553 * l_ + 0.7936177850 * m_ - 0.0040720468 * s_;
    *A = 1.9779984951 * l_ - 2.4285922050 * m_ + 0.4505937099 * s_;
    *B = 0.0259040371 * l_ + 0.7827717662 * m_ - 0.8086757660 * s_;
}

static void
sixel_oklab_to_linear(double L, double A, double B,
                      double *r, double *g, double *b)
{
    double l_;
    double m_;
    double s_;
    double l;
    double m;
    double s;

    l_ = L + 0.3963377774 * A + 0.2158037573 * B;
    m_ = L - 0.1055613458 * A - 0.0638541728 * B;
    s_ = L - 0.0894841775 * A - 1.2914855480 * B;

    l = l_ * l_ * l_;
    m = m_ * m_ * m_;
    s = s_ * s_ * s_;

    *r = 4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s;
    *g = -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s;
    *b = -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s;

    if (*r < 0.0) {
        *r = 0.0;
    }
    if (*g < 0.0) {
        *g = 0.0;
    }
    if (*b < 0.0) {
        *b = 0.0;
    }
}

static void
sixel_linear_srgb_to_smptec(double r, double g, double b,
                            double *rs, double *gs, double *bs)
{
    double sr;
    double sg;
    double sb;

    sr = sixel_linear_srgb_to_smptec_matrix[0][0] * r
       + sixel_linear_srgb_to_smptec_matrix[0][1] * g
       + sixel_linear_srgb_to_smptec_matrix[0][2] * b;
    sg = sixel_linear_srgb_to_smptec_matrix[1][0] * r
       + sixel_linear_srgb_to_smptec_matrix[1][1] * g
       + sixel_linear_srgb_to_smptec_matrix[1][2] * b;
    sb = sixel_linear_srgb_to_smptec_matrix[2][0] * r
       + sixel_linear_srgb_to_smptec_matrix[2][1] * g
       + sixel_linear_srgb_to_smptec_matrix[2][2] * b;

    *rs = sixel_clamp_unit(sr);
    *gs = sixel_clamp_unit(sg);
    *bs = sixel_clamp_unit(sb);
}

static void
sixel_linear_smptec_to_srgb(double rs, double gs, double bs,
                            double *r, double *g, double *b)
{
    double r_lin;
    double g_lin;
    double b_lin;

    r_lin = sixel_linear_smptec_to_srgb_matrix[0][0] * rs
          + sixel_linear_smptec_to_srgb_matrix[0][1] * gs
          + sixel_linear_smptec_to_srgb_matrix[0][2] * bs;
    g_lin = sixel_linear_smptec_to_srgb_matrix[1][0] * rs
          + sixel_linear_smptec_to_srgb_matrix[1][1] * gs
          + sixel_linear_smptec_to_srgb_matrix[1][2] * bs;
    b_lin = sixel_linear_smptec_to_srgb_matrix[2][0] * rs
          + sixel_linear_smptec_to_srgb_matrix[2][1] * gs
          + sixel_linear_smptec_to_srgb_matrix[2][2] * bs;

    *r = sixel_clamp_unit(r_lin);
    *g = sixel_clamp_unit(g_lin);
    *b = sixel_clamp_unit(b_lin);
}

static void
sixel_colorspace_init_tables(void)
{
    int i;
    double gamma_value;
    double linear_value;
    double converted;

    if (tables_initialized) {
        return;
    }

    for (i = 0; i < SIXEL_COLORSPACE_LUT_SIZE; ++i) {
        gamma_value = (double)i / 255.0;
        if (gamma_value <= 0.04045) {
            converted = gamma_value / 12.92;
        } else {
            converted = pow((gamma_value + 0.055) / 1.055, 2.4);
        }
        gamma_to_linear_lut[i] =
            sixel_colorspace_clamp((int)(converted * 255.0 + 0.5));
    }

    for (i = 0; i < SIXEL_COLORSPACE_LUT_SIZE; ++i) {
        linear_value = (double)i / 255.0;
        if (linear_value <= 0.0031308) {
            converted = linear_value * 12.92;
        } else {
            converted = 1.055 * pow(linear_value, 1.0 / 2.4) - 0.055;
        }
        linear_to_gamma_lut[i] =
            sixel_colorspace_clamp((int)(converted * 255.0 + 0.5));
    }

    tables_initialized = 1;
}

static void
sixel_decode_linear_from_colorspace(int colorspace,
                                    unsigned char r8,
                                    unsigned char g8,
                                    unsigned char b8,
                                    double *r_lin,
                                    double *g_lin,
                                    double *b_lin)
{
    switch (colorspace) {
    case SIXEL_COLORSPACE_GAMMA:
        *r_lin = sixel_srgb_to_linear_double(r8);
        *g_lin = sixel_srgb_to_linear_double(g8);
        *b_lin = sixel_srgb_to_linear_double(b8);
        break;
    case SIXEL_COLORSPACE_LINEAR:
        *r_lin = (double)r8 / 255.0;
        *g_lin = (double)g8 / 255.0;
        *b_lin = (double)b8 / 255.0;
        break;
    case SIXEL_COLORSPACE_OKLAB:
    {
        double L = (double)r8 / 255.0;
        double A = sixel_oklab_decode_ab(g8);
        double B = sixel_oklab_decode_ab(b8);
        sixel_oklab_to_linear(L, A, B, r_lin, g_lin, b_lin);
        break;
    }
    case SIXEL_COLORSPACE_SMPTEC:
    {
        double r_smptec = sixel_smptec_to_linear_double(r8);
        double g_smptec = sixel_smptec_to_linear_double(g8);
        double b_smptec = sixel_smptec_to_linear_double(b8);
        sixel_linear_smptec_to_srgb(r_smptec, g_smptec, b_smptec,
                                    r_lin, g_lin, b_lin);
        break;
    }
    default:
        *r_lin = (double)r8 / 255.0;
        *g_lin = (double)g8 / 255.0;
        *b_lin = (double)b8 / 255.0;
        break;
    }
}

static void
sixel_encode_linear_to_colorspace(int colorspace,
                                  double r_lin,
                                  double g_lin,
                                  double b_lin,
                                  unsigned char *r8,
                                  unsigned char *g8,
                                  unsigned char *b8)
{
    double L;
    double A;
    double B;

    switch (colorspace) {
    case SIXEL_COLORSPACE_GAMMA:
        *r8 = sixel_linear_double_to_srgb(r_lin);
        *g8 = sixel_linear_double_to_srgb(g_lin);
        *b8 = sixel_linear_double_to_srgb(b_lin);
        break;
    case SIXEL_COLORSPACE_LINEAR:
        *r8 = sixel_linear_double_to_byte(r_lin);
        *g8 = sixel_linear_double_to_byte(g_lin);
        *b8 = sixel_linear_double_to_byte(b_lin);
        break;
    case SIXEL_COLORSPACE_OKLAB:
        sixel_linear_to_oklab(r_lin, g_lin, b_lin, &L, &A, &B);
        *r8 = sixel_oklab_encode_L(L);
        *g8 = sixel_oklab_encode_ab(A);
        *b8 = sixel_oklab_encode_ab(B);
        break;
    case SIXEL_COLORSPACE_SMPTEC:
    {
        double r_smptec;
        double g_smptec;
        double b_smptec;

        sixel_linear_srgb_to_smptec(r_lin, g_lin, b_lin,
                                     &r_smptec, &g_smptec, &b_smptec);

        *r8 = sixel_linear_double_to_smptec(r_smptec);
        *g8 = sixel_linear_double_to_smptec(g_smptec);
        *b8 = sixel_linear_double_to_smptec(b_smptec);
        break;
    }
    default:
        *r8 = sixel_linear_double_to_byte(r_lin);
        *g8 = sixel_linear_double_to_byte(g_lin);
        *b8 = sixel_linear_double_to_byte(b_lin);
        break;
    }
}

static SIXELSTATUS
sixel_convert_pixels_via_linear(unsigned char *pixels,
                                size_t size,
                                int pixelformat,
                                int colorspace_src,
                                int colorspace_dst)
{
    size_t i;
    int step;
    int index_r;
    int index_g;
    int index_b;

    if (colorspace_src == colorspace_dst) {
        return SIXEL_OK;
    }

    switch (pixelformat) {
    case SIXEL_PIXELFORMAT_RGB888:
        step = 3;
        index_r = 0;
        index_g = 1;
        index_b = 2;
        break;
    case SIXEL_PIXELFORMAT_BGR888:
        step = 3;
        index_r = 2;
        index_g = 1;
        index_b = 0;
        break;
    case SIXEL_PIXELFORMAT_RGBA8888:
        step = 4;
        index_r = 0;
        index_g = 1;
        index_b = 2;
        break;
    case SIXEL_PIXELFORMAT_BGRA8888:
        step = 4;
        index_r = 2;
        index_g = 1;
        index_b = 0;
        break;
    case SIXEL_PIXELFORMAT_ARGB8888:
        step = 4;
        index_r = 1;
        index_g = 2;
        index_b = 3;
        break;
    case SIXEL_PIXELFORMAT_ABGR8888:
        step = 4;
        index_r = 3;
        index_g = 2;
        index_b = 1;
        break;
    case SIXEL_PIXELFORMAT_G8:
        step = 1;
        index_r = 0;
        index_g = 0;
        index_b = 0;
        break;
    case SIXEL_PIXELFORMAT_GA88:
        step = 2;
        index_r = 0;
        index_g = 0;
        index_b = 0;
        break;
    case SIXEL_PIXELFORMAT_AG88:
        step = 2;
        index_r = 1;
        index_g = 1;
        index_b = 1;
        break;
    default:
        return SIXEL_BAD_INPUT;
    }

    if (size % (size_t)step != 0) {
        return SIXEL_BAD_INPUT;
    }

    for (i = 0; i < size; i += (size_t)step) {
        unsigned char *pr = pixels + i + (size_t)index_r;
        unsigned char *pg = pixels + i + (size_t)index_g;
        unsigned char *pb = pixels + i + (size_t)index_b;
        double r_lin;
        double g_lin;
        double b_lin;

        sixel_decode_linear_from_colorspace(colorspace_src,
                                            *pr,
                                            *pg,
                                            *pb,
                                            &r_lin,
                                            &g_lin,
                                            &b_lin);

        sixel_encode_linear_to_colorspace(colorspace_dst,
                                          r_lin,
                                          g_lin,
                                          b_lin,
                                          pr,
                                          pg,
                                          pb);
    }

    return SIXEL_OK;
}

static unsigned char
sixel_colorspace_convert_component(unsigned char value,
                                   int colorspace_src,
                                   int colorspace_dst)
{
    if (colorspace_src == colorspace_dst) {
        return value;
    }

    if (colorspace_src == SIXEL_COLORSPACE_GAMMA &&
            colorspace_dst == SIXEL_COLORSPACE_LINEAR) {
        return gamma_to_linear_lut[value];
    }

    if (colorspace_src == SIXEL_COLORSPACE_LINEAR &&
            colorspace_dst == SIXEL_COLORSPACE_GAMMA) {
        return linear_to_gamma_lut[value];
    }

    return value;
}

int
sixel_colorspace_supports_pixelformat(int pixelformat)
{
    switch (pixelformat) {
    case SIXEL_PIXELFORMAT_RGB888:
    case SIXEL_PIXELFORMAT_BGR888:
    case SIXEL_PIXELFORMAT_RGBA8888:
    case SIXEL_PIXELFORMAT_ARGB8888:
    case SIXEL_PIXELFORMAT_BGRA8888:
    case SIXEL_PIXELFORMAT_ABGR8888:
    case SIXEL_PIXELFORMAT_G8:
    case SIXEL_PIXELFORMAT_GA88:
    case SIXEL_PIXELFORMAT_AG88:
        return 1;
    default:
        break;
    }

    return 0;
}

SIXELAPI SIXELSTATUS
sixel_helper_convert_colorspace(unsigned char *pixels,
                                size_t size,
                                int pixelformat,
                                int colorspace_src,
                                int colorspace_dst)
{
    size_t i;

    if (pixels == NULL) {
        sixel_helper_set_additional_message(
            "sixel_helper_convert_colorspace: pixels is null.");
        return SIXEL_BAD_ARGUMENT;
    }

    if (colorspace_src == colorspace_dst) {
        return SIXEL_OK;
    }

    if (!sixel_colorspace_supports_pixelformat(pixelformat)) {
        sixel_helper_set_additional_message(
            "sixel_helper_convert_colorspace: unsupported pixelformat.");
        return SIXEL_BAD_INPUT;
    }

    sixel_colorspace_init_tables();

    if (colorspace_src == SIXEL_COLORSPACE_OKLAB ||
            colorspace_dst == SIXEL_COLORSPACE_OKLAB ||
            colorspace_src == SIXEL_COLORSPACE_SMPTEC ||
            colorspace_dst == SIXEL_COLORSPACE_SMPTEC) {
        SIXELSTATUS status = sixel_convert_pixels_via_linear(pixels,
                                                             size,
                                                             pixelformat,
                                                             colorspace_src,
                                                             colorspace_dst);
        if (SIXEL_FAILED(status)) {
            sixel_helper_set_additional_message(
                "sixel_helper_convert_colorspace: unsupported pixelformat for conversion.");
        }
        return status;
    }

    switch (pixelformat) {
    case SIXEL_PIXELFORMAT_RGB888:
        if (size % 3 != 0) {
            sixel_helper_set_additional_message(
                "sixel_helper_convert_colorspace: invalid data size.");
            return SIXEL_BAD_INPUT;
        }
        for (i = 0; i + 2 < size; i += 3) {
            pixels[i + 0] = sixel_colorspace_convert_component(
                pixels[i + 0], colorspace_src, colorspace_dst);
            pixels[i + 1] = sixel_colorspace_convert_component(
                pixels[i + 1], colorspace_src, colorspace_dst);
            pixels[i + 2] = sixel_colorspace_convert_component(
                pixels[i + 2], colorspace_src, colorspace_dst);
        }
        break;
    case SIXEL_PIXELFORMAT_BGR888:
        if (size % 3 != 0) {
            sixel_helper_set_additional_message(
                "sixel_helper_convert_colorspace: invalid data size.");
            return SIXEL_BAD_INPUT;
        }
        for (i = 0; i + 2 < size; i += 3) {
            pixels[i + 0] = sixel_colorspace_convert_component(
                pixels[i + 0], colorspace_src, colorspace_dst);
            pixels[i + 1] = sixel_colorspace_convert_component(
                pixels[i + 1], colorspace_src, colorspace_dst);
            pixels[i + 2] = sixel_colorspace_convert_component(
                pixels[i + 2], colorspace_src, colorspace_dst);
        }
        break;
    case SIXEL_PIXELFORMAT_RGBA8888:
        if (size % 4 != 0) {
            sixel_helper_set_additional_message(
                "sixel_helper_convert_colorspace: invalid data size.");
            return SIXEL_BAD_INPUT;
        }
        for (i = 0; i + 3 < size; i += 4) {
            pixels[i + 0] = sixel_colorspace_convert_component(
                pixels[i + 0], colorspace_src, colorspace_dst);
            pixels[i + 1] = sixel_colorspace_convert_component(
                pixels[i + 1], colorspace_src, colorspace_dst);
            pixels[i + 2] = sixel_colorspace_convert_component(
                pixels[i + 2], colorspace_src, colorspace_dst);
        }
        break;
    case SIXEL_PIXELFORMAT_ARGB8888:
        if (size % 4 != 0) {
            sixel_helper_set_additional_message(
                "sixel_helper_convert_colorspace: invalid data size.");
            return SIXEL_BAD_INPUT;
        }
        for (i = 0; i + 3 < size; i += 4) {
            pixels[i + 1] = sixel_colorspace_convert_component(
                pixels[i + 1], colorspace_src, colorspace_dst);
            pixels[i + 2] = sixel_colorspace_convert_component(
                pixels[i + 2], colorspace_src, colorspace_dst);
            pixels[i + 3] = sixel_colorspace_convert_component(
                pixels[i + 3], colorspace_src, colorspace_dst);
        }
        break;
    case SIXEL_PIXELFORMAT_BGRA8888:
        if (size % 4 != 0) {
            sixel_helper_set_additional_message(
                "sixel_helper_convert_colorspace: invalid data size.");
            return SIXEL_BAD_INPUT;
        }
        for (i = 0; i + 3 < size; i += 4) {
            pixels[i + 0] = sixel_colorspace_convert_component(
                pixels[i + 0], colorspace_src, colorspace_dst);
            pixels[i + 1] = sixel_colorspace_convert_component(
                pixels[i + 1], colorspace_src, colorspace_dst);
            pixels[i + 2] = sixel_colorspace_convert_component(
                pixels[i + 2], colorspace_src, colorspace_dst);
        }
        break;
    case SIXEL_PIXELFORMAT_ABGR8888:
        if (size % 4 != 0) {
            sixel_helper_set_additional_message(
                "sixel_helper_convert_colorspace: invalid data size.");
            return SIXEL_BAD_INPUT;
        }
        for (i = 0; i + 3 < size; i += 4) {
            pixels[i + 1] = sixel_colorspace_convert_component(
                pixels[i + 1], colorspace_src, colorspace_dst);
            pixels[i + 2] = sixel_colorspace_convert_component(
                pixels[i + 2], colorspace_src, colorspace_dst);
            pixels[i + 3] = sixel_colorspace_convert_component(
                pixels[i + 3], colorspace_src, colorspace_dst);
        }
        break;
    case SIXEL_PIXELFORMAT_G8:
        for (i = 0; i < size; ++i) {
            pixels[i] = sixel_colorspace_convert_component(
                pixels[i], colorspace_src, colorspace_dst);
        }
        break;
    case SIXEL_PIXELFORMAT_GA88:
        if (size % 2 != 0) {
            sixel_helper_set_additional_message(
                "sixel_helper_convert_colorspace: invalid data size.");
            return SIXEL_BAD_INPUT;
        }
        for (i = 0; i + 1 < size; i += 2) {
            pixels[i + 0] = sixel_colorspace_convert_component(
                pixels[i + 0], colorspace_src, colorspace_dst);
        }
        break;
    case SIXEL_PIXELFORMAT_AG88:
        if (size % 2 != 0) {
            sixel_helper_set_additional_message(
                "sixel_helper_convert_colorspace: invalid data size.");
            return SIXEL_BAD_INPUT;
        }
        for (i = 0; i + 1 < size; i += 2) {
            pixels[i + 1] = sixel_colorspace_convert_component(
                pixels[i + 1], colorspace_src, colorspace_dst);
        }
        break;
    default:
        sixel_helper_set_additional_message(
            "sixel_helper_convert_colorspace: unsupported pixelformat.");
        return SIXEL_BAD_INPUT;
    }

    return SIXEL_OK;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
