/*
 * Copyright (c) 2021-2025 libsixel developers. See `AUTHORS`.
 * Copyright (c) 2014-2016 Hayaki Saito
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
#include <stdlib.h>

#if HAVE_STRING_H
# include <string.h>
#endif  /* HAVE_STRING_H */

#if HAVE_MATH_H
# define _USE_MATH_DEFINES  /* for MSVC */
# include <math.h>
#endif  /* HAVE_MATH_H */
#ifndef M_PI
# define M_PI 3.14159265358979323846
#endif

#include <sixel.h>

#if !defined(MAX)
# define MAX(l, r) ((l) > (r) ? (l) : (r))
#endif
#if !defined(MIN)
#define MIN(l, r) ((l) < (r) ? (l) : (r))
#endif


#if 0
/* function Nearest Neighbor */
static double
nearest_neighbor(double const d)
{
    if (d <= 0.5) {
        return 1.0;
    }
    return 0.0;
}
#endif


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


static void
scale_without_resampling(
    unsigned char *dst,
    unsigned char const *src,
    int const srcw,
    int const srch,
    int const dstw,
    int const dsth,
    int const depth)
{
    int w;
    int h;
    int x;
    int y;
    int i;
    int pos;

    for (h = 0; h < dsth; h++) {
        for (w = 0; w < dstw; w++) {
            x = (long)w * srcw / dstw;
            y = (long)h * srch / dsth;
            for (i = 0; i < depth; i++) {
                pos = (y * srcw + x) * depth + i;
                dst[(h * dstw + w) * depth + i] = src[pos];
            }
        }
    }
}


typedef double (*resample_fn_t)(double const d);

static void
scale_with_resampling(
    unsigned char *dst,
    unsigned char const *src,
    int const srcw,
    int const srch,
    int const dstw,
    int const dsth,
    int const depth,
    resample_fn_t const f_resample,
    double n,
    sixel_allocator_t *allocator)
{
    /*
     * Resize an RGB image using an arbitrary resampling kernel.
     *
     * The filter is assumed to be separable, so we first convolve all
     * rows horizontally into a temporary buffer and then convolve the
     * intermediate image vertically.  This two-pass approach greatly
     * reduces the number of multiplications compared to evaluating the
     * 2-D kernel for every destination pixel directly.
     */
    int w;
    int h;
    int x;
    int y;
    int i;
    int pos;
    int x_first, x_last, y_first, y_last;
    double center_x, center_y;
    double diff_x, diff_y;
    double weight;
    double total;
    double offsets[8];
    unsigned char *tmp;

    /* allocate intermediate buffer for horizontally filtered rows */
    tmp = (unsigned char *)sixel_allocator_malloc(
        allocator, (size_t)(dstw * srch * depth));
    if (tmp == NULL) {
        return;                 /* give up if memory allocation fails */
    }

    /*
     * Horizontal pass
     */
    for (y = 0; y < srch; y++) {
        for (w = 0; w < dstw; w++) {
            /* reset accumulators for each destination pixel */
            total = 0.0;
            for (i = 0; i < depth; i++) {
                offsets[i] = 0;
            }

            /* determine which source pixels contribute to this column */
            if (dstw >= srcw) {
                /* up-scaling: source and destination share the same scale */
                center_x = (w + 0.5) * srcw / dstw;
                x_first = MAX(center_x - n, 0);
                x_last = MIN(center_x + n, srcw - 1);
            } else {
                /* down-scaling: map destination pixel back to source space */
                center_x = w + 0.5;
                x_first = MAX(floor((center_x - n) * srcw / dstw), 0);
                x_last = MIN(floor((center_x + n) * srcw / dstw), srcw - 1);
            }

            /* accumulate weighted source samples */
            for (x = x_first; x <= x_last; x++) {
                diff_x = (dstw >= srcw)
                            ? (x + 0.5) - center_x
                            : (x + 0.5) * dstw / srcw - center_x;
                weight = f_resample(fabs(diff_x));
                for (i = 0; i < depth; i++) {
                    pos = (y * srcw + x) * depth + i;
                    offsets[i] += src[pos] * weight;
                }
                total += weight;
            }

            /* normalize accumulated value and store into the temp buffer */
            if (total > 0.0) {
                for (i = 0; i < depth; i++) {
                    pos = (y * dstw + w) * depth + i;
                    tmp[pos] = normalize(offsets[i], total);
                }
            }
        }
    }

    /*
     * Vertical pass
     */
    for (h = 0; h < dsth; h++) {
        for (w = 0; w < dstw; w++) {
            total = 0.0;
            for (i = 0; i < depth; i++) {
                offsets[i] = 0;
            }

            /* determine contributing rows for this destination pixel */
            if (dsth >= srch) {
                center_y = (h + 0.5) * srch / dsth;
                y_first = MAX(center_y - n, 0);
                y_last = MIN(center_y + n, srch - 1);
            } else {
                center_y = h + 0.5;
                y_first = MAX(floor((center_y - n) * srch / dsth), 0);
                y_last = MIN(floor((center_y + n) * srch / dsth), srch - 1);
            }

            for (y = y_first; y <= y_last; y++) {
                diff_y = (dsth >= srch)
                            ? (y + 0.5) - center_y
                            : (y + 0.5) * dsth / srch - center_y;
                weight = f_resample(fabs(diff_y));
                for (i = 0; i < depth; i++) {
                    pos = (y * dstw + w) * depth + i;
                    offsets[i] += tmp[pos] * weight;
                }
                total += weight;
            }

            if (total > 0.0) {
                for (i = 0; i < depth; i++) {
                    pos = (h * dstw + w) * depth + i;
                    dst[pos] = normalize(offsets[i], total);
                }
            }
        }
    }

    /* clean up temporary storage */
    sixel_allocator_free(allocator, tmp);
}


SIXELAPI int
sixel_helper_scale_image(
    unsigned char       /* out */ *dst,
    unsigned char const /* in */  *src,                   /* source image data */
    int                 /* in */  srcw,                   /* source image width */
    int                 /* in */  srch,                   /* source image height */
    int                 /* in */  pixelformat,            /* one of enum pixelFormat */
    int                 /* in */  dstw,                   /* destination image width */
    int                 /* in */  dsth,                   /* destination image height */
    int                 /* in */  method_for_resampling,  /* one of methodForResampling */
    sixel_allocator_t   /* in */  *allocator)             /* allocator object */
{
    /*
     * Convert the source image to RGB24 if necessary and scale it to the
     * requested destination size.  The caller supplies an allocator used
     * for any temporary buffers required during conversion or filtering.
     */
    int const depth = sixel_helper_compute_depth(pixelformat);
    unsigned char *new_src = NULL;  /* optional converted source buffer */
    int nret;
    int new_pixelformat;

    /* ensure the scaler operates on RGB triples */
    if (depth != 3) {
        new_src = (unsigned char *)sixel_allocator_malloc(allocator,
                                                          (size_t)(srcw * srch * 3));
        if (new_src == NULL) {
            return (-1);
        }
        nret = sixel_helper_normalize_pixelformat(new_src,
                                                  &new_pixelformat,
                                                  src, pixelformat,
                                                  srcw, srch);
        if (nret != 0) {
            sixel_allocator_free(allocator, new_src);
            return (-1);
        }

        src = new_src;  /* use converted buffer from here on */
    } else {
        new_pixelformat = pixelformat;
    }

    /* choose re-sampling strategy */
    switch (method_for_resampling) {
    case SIXEL_RES_NEAREST:
        scale_without_resampling(dst, src, srcw, srch, dstw, dsth, depth);
        break;
    case SIXEL_RES_GAUSSIAN:
        scale_with_resampling(dst, src, srcw, srch, dstw, dsth, depth,
                              gaussian, 1.0, allocator);
        break;
    case SIXEL_RES_HANNING:
        scale_with_resampling(dst, src, srcw, srch, dstw, dsth, depth,
                              hanning, 1.0, allocator);
        break;
    case SIXEL_RES_HAMMING:
        scale_with_resampling(dst, src, srcw, srch, dstw, dsth, depth,
                              hamming, 1.0, allocator);
        break;
    case SIXEL_RES_WELSH:
        scale_with_resampling(dst, src, srcw, srch, dstw, dsth, depth,
                              welsh, 1.0, allocator);
        break;
    case SIXEL_RES_BICUBIC:
        scale_with_resampling(dst, src, srcw, srch, dstw, dsth, depth,
                              bicubic, 2.0, allocator);
        break;
    case SIXEL_RES_LANCZOS2:
        scale_with_resampling(dst, src, srcw, srch, dstw, dsth, depth,
                              lanczos2, 2.0, allocator);
        break;
    case SIXEL_RES_LANCZOS3:
        scale_with_resampling(dst, src, srcw, srch, dstw, dsth, depth,
                              lanczos3, 3.0, allocator);
        break;
    case SIXEL_RES_LANCZOS4:
        scale_with_resampling(dst, src, srcw, srch, dstw, dsth, depth,
                              lanczos4, 4.0, allocator);
        break;
    case SIXEL_RES_BILINEAR:
    default:
        scale_with_resampling(dst, src, srcw, srch, dstw, dsth, depth,
                              bilinear, 1.0, allocator);
        break;
    }

    /* release temporary copy created for pixel-format normalization */
    sixel_allocator_free(allocator, new_src);
    return 0;
}

#if HAVE_TESTS

static void
reference_scale(
    unsigned char *dst,
    unsigned char const *src,
    int const srcw,
    int const srch,
    int const dstw,
    int const dsth,
    int const depth)
{
    int w;
    int h;
    int x;
    int y;
    int i;
    int pos;

    for (h = 0; h < dsth; h++) {
        for (w = 0; w < dstw; w++) {
            x = (long)w * srcw / dstw;
            y = (long)h * srch / dsth;
            for (i = 0; i < depth; i++) {
                pos = (y * srcw + x) * depth + i;
                dst[(h * dstw + w) * depth + i] = src[pos];
            }
        }
    }
}

static int
test_without_resampling_case(
    int srcw,
    int srch,
    int dstw,
    int dsth,
    int depth)
{
    int nret = EXIT_FAILURE;
    size_t srcsize = (size_t)srcw * srch * depth;
    size_t dstsize = (size_t)dstw * dsth * depth;
    unsigned char *src = NULL;
    unsigned char *ref = NULL;
    unsigned char *out = NULL;
    size_t i;

    src = (unsigned char *)malloc(srcsize);
    ref = (unsigned char *)malloc(dstsize);
    out = (unsigned char *)malloc(dstsize);
    if (src == NULL || ref == NULL || out == NULL) {
        goto end;
    }

    for (i = 0; i < srcsize; ++i) {
        src[i] = (unsigned char)(i & 0xff);
    }

    reference_scale(ref, src, srcw, srch, dstw, dsth, depth);
    scale_without_resampling(out, src, srcw, srch, dstw, dsth, depth);

    if (memcmp(ref, out, dstsize) != 0) {
        goto end;
    }

    nret = EXIT_SUCCESS;

end:
    free(src);
    free(ref);
    free(out);
    return nret;
}

int
sixel_scale_tests_main(void)
{
    int nret = EXIT_FAILURE;
    size_t i;
    struct {
        int srcw;
        int srch;
        int dstw;
        int dsth;
        int depth;
    } cases[] = {
        {8, 4, 3, 7, 3},
        {13, 9, 17, 6, 4}
    };

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        nret = test_without_resampling_case(cases[i].srcw,
                                            cases[i].srch,
                                            cases[i].dstw,
                                            cases[i].dsth,
                                            cases[i].depth);
        if (nret != EXIT_SUCCESS) {
            goto end;
        }
    }

    nret = EXIT_SUCCESS;

end:
    return nret;
}

#endif /* HAVE_TESTS */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
