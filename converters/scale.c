/*
 * Copyright (c) 2014 Hayaki Saito
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
#include "malloc_stub.h"

#define _USE_MATH_DEFINES  /* for MSVC */
#include <math.h>
#ifndef M_PI
# define M_PI 3.14159265358979323846
#endif

#include <stdlib.h>

#include "scale.h"

#if !defined(MAX)
# define MAX(l, r) ((l) > (r) ? (l) : (r))
#endif
#if !defined(MIN)
#define MIN(l, r) ((l) < (r) ? (l) : (r))
#endif


/* function Nearest Neighbor */
static double
nearest_neighbor(double const d)
{
    if (d <= 0.5) {
        return 1.0;
    }
    return 0.0;
}


/* function Bi-linear */
static double
bilinear(double const d)
{
    if (d < 1.0) {
        return 1.0 - d;
    }
    return 0.0;
}


/* function Welsh */
static double
welsh(double const d)
{
    if (d < 1.0) {
        return 1.0 - d * d;
    }
    return 0.0;
}


/* function Bi-cubic */
static double
bicubic(double const d)
{
    if (d <= 1.0) {
        return 1.0 + (d - 2.0) * d * d;
    }
    if (d <= 2.0) {
        return 4.0 + d * (-8.0 + d * (5.0 - d));
    }
    return 0.0;
}


/* function sinc
 * sinc(x) = sin(PI * x) / (PI * x)
 */
static double
sinc(double const x)
{
    return sin(M_PI * x) / (M_PI * x);
}


/* function Lanczos-2
 * Lanczos(x) = sinc(x) * sinc(x / 2) , |x| <= 2
 *            = 0, |x| > 2
 */
static double
lanczos2(double const d)
{
    if (d == 0.0) {
        return 1.0;
    }
    if (d < 2.0) {
        return sinc(d) * sinc(d / 2.0);
    }
    return 0.0;
}


/* function Lanczos-3
 * Lanczos(x) = sinc(x) * sinc(x / 3) , |x| <= 3
 *            = 0, |x| > 3
 */
static double
lanczos3(double const d)
{
    if (d == 0.0) {
        return 1.0;
    }
    if (d < 3.0) {
        return sinc(d) * sinc(d / 3.0);
    }
    return 0.0;
}

/* function Lanczos-4
 * Lanczos(x) = sinc(x) * sinc(x / 4) , |x| <= 4
 *            = 0, |x| > 4
 */
static double
lanczos4(double const d)
{
    if (d == 0.0) {
        return 1.0;
    }
    if (d < 4.0) {
        return sinc(d) * sinc(d / 4.0);
    }
    return 0.0;
}


static double
gaussian(double const d)
{
    return exp(-2.0 * d * d) * sqrt(2.0 / M_PI);
}


static double
hanning(double const d)
{
    return 0.5 + 0.5 * cos(d * M_PI);
}


static double
hamming(const double d)
{
    return 0.54 + 0.46 * cos(d * M_PI);
}

static unsigned char
normalize(double x, double total)
{
    int result;

    result = floor(x / total);
    if (result > 255) {
        return 0xff;
    }
    if (result < 0) {
        return 0x00;
    }
    return (unsigned char)result;
}


unsigned char *
LSS_scale(unsigned char const *pixels,
          int srcx, int srcy, int depth,
          int destx, int desty,
          enum methodForResampling const methodForResampling)
{
    unsigned char *result;
    double *offsets;
    int i, index;
    double n;
    int h, w;
    int y, x;
    int x_first, x_last, y_first, y_last;
    double center_x, center_y;
    double diff_x, diff_y;
    double weight;
    double total;
    double (*f_resample)(double const d);

    result = malloc(destx * desty * depth);
    offsets = malloc(sizeof(*offsets) * depth);

    /* choose re-sampling strategy */
    switch (methodForResampling) {
#if 0
    case RES_NEAREST:
        f_resample = nearest_neighbor;
        n = 1.0;
        break;
#endif
    case RES_GAUSSIAN:
        f_resample = gaussian;
        n = 1.0;
        break;
    case RES_HANNING:
        f_resample = hanning;
        n = 1.0;
        break;
    case RES_HAMMING:
        f_resample = hamming;
        n = 1.0;
        break;
    case RES_BILINEAR:
        f_resample = bilinear;
        n = 1.0;
        break;
    case RES_WELSH:
        f_resample = welsh;
        n = 1.0;
        break;
    case RES_BICUBIC:
        f_resample = bicubic;
        n = 2.0;
        break;
    case RES_LANCZOS2:
        f_resample = lanczos2;
        n = 3.0;
        break;
    case RES_LANCZOS3:
        f_resample = lanczos3;
        n = 3.0;
        break;
    case RES_LANCZOS4:
        f_resample = lanczos4;
        n = 4.0;
        break;
    default:
        f_resample = bilinear;
        n = 1.0;
        break;
    }


    if (methodForResampling == RES_NEAREST) {
        for (h = 0; h < desty; h++) {
            for (w = 0; w < destx; w++) {
                x = w * srcx / destx;
                y = h * srcy / desty;
                for (i = 0; i < depth; i++) {
                    index = (y * srcx + x) * depth + i;
                    result[(h * destx + w) * depth + i] = pixels[index];
                }
            }
        }
    } else {
        for (h = 0; h < desty; h++) {
            for (w = 0; w < destx; w++) {
                total = 0.0;
                for (i = 0; i < depth; i++) {
                    offsets[i] = 0;
                }

                /* retrieve range of affected pixels */
                if (destx >= srcx) {
                    center_x = (w + 0.5) * srcx / destx;
                    x_first = MAX(center_x - n, 0);
                    x_last = MIN(center_x + n, srcx - 1);
                } else {
                    center_x = w + 0.5;
                    x_first = MAX(floor((center_x - n) * srcx / destx), 0);
                    x_last = MIN(floor((center_x + n) * srcx / destx), srcx - 1);
                }
                if (desty >= srcy) {
                    center_y = (h + 0.5) * srcy / desty;
                    y_first = MAX(center_y - n, 0);
                    y_last = MIN(center_y + n, srcy - 1);
                } else {
                    center_y = h + 0.5;
                    y_first = MAX(floor((center_y - n) * srcy / desty), 0);
                    y_last = MIN(floor((center_y + n) * srcy / desty), srcy - 1);
                }

                /* accumerate weights of affected pixels */
                for (y = y_first; y <= y_last; y++) {
                    for (x = x_first; x <= x_last; x++) {
                        if (destx >= srcx) {
                            diff_x = (x + 0.5) - center_x;
                        } else {
                            diff_x = (x + 0.5) * destx / srcx - center_x;
                        }
                        if (desty >= srcy) {
                            diff_y = (y + 0.5) - center_y;
                        } else {
                            diff_y = (y + 0.5) * desty / srcy - center_y;
                        }
                        weight = f_resample(fabs(diff_x)) * f_resample(fabs(diff_y));
                        for (i = 0; i < depth; i++) {
                            index = (y * srcx + x) * depth + i;
                            offsets[i] += pixels[index] * weight;
                        }
                        total += weight;
                    }
                }

                /* normalize */
                if (total > 0.0) {
                    for (i = 0; i < depth; i++) {
                        index = (h * destx + w) * depth + i;
                        result[index] = normalize(offsets[i], total);
                    }
                }
            }
        }
    }
    free(offsets);
    return result;
}

/* emacs, -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set expandtab ts=4 : */
/* EOF */
