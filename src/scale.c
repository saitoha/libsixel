/*
 * SPDX-License-Identifier: MIT
 *
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

#if HAVE_ERRNO_H
# include <errno.h>
#endif  /* HAVE_ERRNO_H */
#if HAVE_LIMITS_H
# include <limits.h>
#endif  /* HAVE_LIMITS_H */
#if HAVE_STRING_H
# include <string.h>
#endif  /* HAVE_STRING_H */
#if HAVE_STDINT_H
# include <stdint.h>
#endif  /* HAVE_STDINT_H */

#if HAVE_MATH_H
# define _USE_MATH_DEFINES  /* for MSVC */
# include <math.h>
#endif  /* HAVE_MATH_H */
#ifndef M_PI
# define M_PI 3.14159265358979323846
#endif

#include <sixel.h>

#if SIXEL_ENABLE_THREADS
# include "sixel_threads_config.h"
# include "threadpool.h"
#endif

#if defined(HAVE_SSE2)
# if defined(__SSE2__)
#  if defined(HAVE_EMMINTRIN_H)
#   include <emmintrin.h>
#   define SIXEL_USE_SSE2 1
#  endif
# endif
#elif defined(HAVE_NEON)
# if (defined(__ARM_NEON) || defined(__ARM_NEON__))
#  if defined(HAVE_ARM_NEON_H)
#   include <arm_neon.h>
#   define SIXEL_USE_NEON 1
#  endif
# endif
#endif

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

/*
 * Two-pass separable filter helpers. Each function processes a single row so
 * the caller may invoke them serially or from a threadpool worker.
 */
static void
scale_horizontal_row(
    unsigned char *tmp,
    unsigned char const *src,
    int const srcw,
    int const dstw,
    int const depth,
    int const y,
    resample_fn_t const f_resample,
    double const n)
{
    int w;
    int x;
    int i;
    int pos;
    int x_first;
    int x_last;
    double center_x;
    double diff_x;
    double weight;
    double total;
    double offsets[8];

    for (w = 0; w < dstw; w++) {
        total = 0.0;
        for (i = 0; i < depth; i++) {
            offsets[i] = 0;
        }

        if (dstw >= srcw) {
            center_x = (w + 0.5) * srcw / dstw;
            x_first = MAX(center_x - n, 0);
            x_last = MIN(center_x + n, srcw - 1);
        } else {
            center_x = w + 0.5;
            x_first = MAX(floor((center_x - n) * srcw / dstw), 0);
            x_last = MIN(floor((center_x + n) * srcw / dstw), srcw - 1);
        }

#if defined(SIXEL_USE_SSE2) || defined(SIXEL_USE_NEON)
        if (depth == 3) {
#if defined(SIXEL_USE_SSE2)
            __m128 acc = _mm_setzero_ps();
#elif defined(SIXEL_USE_NEON)
            float32x4_t acc = vdupq_n_f32(0.0f);
#endif
            for (x = x_first; x <= x_last; x++) {
                diff_x = (dstw >= srcw)
                            ? (x + 0.5) - center_x
                            : (x + 0.5) * dstw / srcw - center_x;
                weight = f_resample(fabs(diff_x));
                pos = (y * srcw + x) * depth;
                const unsigned char *psrc = src + pos;
#if defined(SIXEL_USE_SSE2)
                unsigned int pixel = psrc[0]
                                   | (psrc[1] << 8)
                                   | (psrc[2] << 16);
                __m128i pixi = _mm_cvtsi32_si128((int)pixel);
                pixi = _mm_unpacklo_epi8(pixi, _mm_setzero_si128());
                pixi = _mm_unpacklo_epi16(pixi, _mm_setzero_si128());
                __m128 pixf = _mm_cvtepi32_ps(pixi);
                __m128 wv = _mm_set1_ps((float)weight);
                acc = _mm_add_ps(acc, _mm_mul_ps(pixf, wv));
#else /* NEON */
                uint32x4_t pix32 = {psrc[0], psrc[1], psrc[2], 0};
                float32x4_t pixf = vcvtq_f32_u32(pix32);
                float32x4_t wv = vdupq_n_f32((float)weight);
                acc = vmlaq_f32(acc, pixf, wv);
#endif
                total += weight;
            }
            if (total > 0.0) {
#if defined(SIXEL_USE_SSE2)
                __m128 scalev = _mm_set1_ps((float)(1.0 / total));
                acc = _mm_mul_ps(acc, scalev);
                __m128 minv = _mm_set1_ps(0.0f);
                __m128 maxv = _mm_set1_ps(255.0f);
                acc = _mm_max_ps(minv, _mm_min_ps(acc, maxv));
                __m128i acci = _mm_cvtps_epi32(acc);
                __m128i acc16 = _mm_packs_epi32(acci, _mm_setzero_si128());
                acc16 = _mm_packus_epi16(acc16, _mm_setzero_si128());
                pos = (y * dstw + w) * depth;
                unsigned int out = (unsigned int)_mm_cvtsi128_si32(acc16);
                tmp[pos + 0] = (unsigned char)out;
                tmp[pos + 1] = (unsigned char)(out >> 8);
                tmp[pos + 2] = (unsigned char)(out >> 16);
#else /* NEON */
                float32x4_t scalev = vdupq_n_f32((float)(1.0 / total));
                acc = vmulq_f32(acc, scalev);
                float32x4_t minv = vdupq_n_f32(0.0f);
                float32x4_t maxv = vdupq_n_f32(255.0f);
                acc = vmaxq_f32(minv, vminq_f32(acc, maxv));
                uint32x4_t acci = vcvtq_u32_f32(acc);
                uint16x4_t acc16 = vmovn_u32(acci);
                uint8x8_t acc8 = vmovn_u16(vcombine_u16(acc16, acc16));
                uint8_t outb[8];
                vst1_u8(outb, acc8);
                pos = (y * dstw + w) * depth;
                tmp[pos + 0] = outb[0];
                tmp[pos + 1] = outb[1];
                tmp[pos + 2] = outb[2];
#endif
            }
            continue;
        }
#endif /* SIMD paths */
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

        if (total > 0.0) {
            for (i = 0; i < depth; i++) {
                pos = (y * dstw + w) * depth + i;
                tmp[pos] = normalize(offsets[i], total);
            }
        }
    }
}

static void
scale_vertical_row(
    unsigned char *dst,
    unsigned char const *tmp,
    int const dstw,
    int const dsth,
    int const depth,
    int const srch,
    int const h,
    resample_fn_t const f_resample,
    double const n)
{
    int w;
    int y;
    int i;
    int pos;
    int y_first;
    int y_last;
    double center_y;
    double diff_y;
    double weight;
    double total;
    double offsets[8];

    for (w = 0; w < dstw; w++) {
        total = 0.0;
        for (i = 0; i < depth; i++) {
            offsets[i] = 0;
        }

        if (dsth >= srch) {
            center_y = (h + 0.5) * srch / dsth;
            y_first = MAX(center_y - n, 0);
            y_last = MIN(center_y + n, srch - 1);
        } else {
            center_y = h + 0.5;
            y_first = MAX(floor((center_y - n) * srch / dsth), 0);
            y_last = MIN(floor((center_y + n) * srch / dsth), srch - 1);
        }

#if defined(SIXEL_USE_SSE2) || defined(SIXEL_USE_NEON)
        if (depth == 3) {
#if defined(SIXEL_USE_SSE2)
            __m128 acc = _mm_setzero_ps();
#elif defined(SIXEL_USE_NEON)
            float32x4_t acc = vdupq_n_f32(0.0f);
#endif
            for (y = y_first; y <= y_last; y++) {
                diff_y = (dsth >= srch)
                            ? (y + 0.5) - center_y
                            : (y + 0.5) * dsth / srch - center_y;
                weight = f_resample(fabs(diff_y));
                pos = (y * dstw + w) * depth;
                const unsigned char *psrc = tmp + pos;
#if defined(SIXEL_USE_SSE2)
                unsigned int pixel = psrc[0]
                                   | (psrc[1] << 8)
                                   | (psrc[2] << 16);
                __m128i pixi = _mm_cvtsi32_si128((int)pixel);
                pixi = _mm_unpacklo_epi8(pixi, _mm_setzero_si128());
                pixi = _mm_unpacklo_epi16(pixi, _mm_setzero_si128());
                __m128 pixf = _mm_cvtepi32_ps(pixi);
                __m128 wv = _mm_set1_ps((float)weight);
                acc = _mm_add_ps(acc, _mm_mul_ps(pixf, wv));
#else /* NEON */
                uint32x4_t pix32 = {psrc[0], psrc[1], psrc[2], 0};
                float32x4_t pixf = vcvtq_f32_u32(pix32);
                float32x4_t wv = vdupq_n_f32((float)weight);
                acc = vmlaq_f32(acc, pixf, wv);
#endif
                total += weight;
            }
            if (total > 0.0) {
#if defined(SIXEL_USE_SSE2)
                __m128 scalev = _mm_set1_ps((float)(1.0 / total));
                acc = _mm_mul_ps(acc, scalev);
                __m128 minv = _mm_set1_ps(0.0f);
                __m128 maxv = _mm_set1_ps(255.0f);
                acc = _mm_max_ps(minv, _mm_min_ps(acc, maxv));
                __m128i acci = _mm_cvtps_epi32(acc);
                __m128i acc16 = _mm_packs_epi32(acci, _mm_setzero_si128());
                acc16 = _mm_packus_epi16(acc16, _mm_setzero_si128());
                pos = (h * dstw + w) * depth;
                unsigned int out = (unsigned int)_mm_cvtsi128_si32(acc16);
                dst[pos + 0] = (unsigned char)out;
                dst[pos + 1] = (unsigned char)(out >> 8);
                dst[pos + 2] = (unsigned char)(out >> 16);
#else /* NEON */
                float32x4_t scalev = vdupq_n_f32((float)(1.0 / total));
                acc = vmulq_f32(acc, scalev);
                float32x4_t minv = vdupq_n_f32(0.0f);
                float32x4_t maxv = vdupq_n_f32(255.0f);
                acc = vmaxq_f32(minv, vminq_f32(acc, maxv));
                uint32x4_t acci = vcvtq_u32_f32(acc);
                uint16x4_t acc16 = vmovn_u32(acci);
                uint8x8_t acc8 = vmovn_u16(vcombine_u16(acc16, acc16));
                uint8_t outb[8];
                vst1_u8(outb, acc8);
                pos = (h * dstw + w) * depth;
                dst[pos + 0] = outb[0];
                dst[pos + 1] = outb[1];
                dst[pos + 2] = outb[2];
#endif
            }
            continue;
        }
#endif /* SIMD paths */
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

static void
scale_with_resampling_serial(
    unsigned char *dst,
    unsigned char const *src,
    int const srcw,
    int const srch,
    int const dstw,
    int const dsth,
    int const depth,
    resample_fn_t const f_resample,
    double const n,
    unsigned char *tmp)
{
    int y;
    int h;

    for (y = 0; y < srch; y++) {
        scale_horizontal_row(tmp, src, srcw, dstw, depth, y, f_resample, n);
    }

    for (h = 0; h < dsth; h++) {
        scale_vertical_row(dst, tmp, dstw, dsth, depth, srch, h,
                           f_resample, n);
    }
}

#if SIXEL_ENABLE_THREADS
typedef enum scale_parallel_pass {
    SCALE_PASS_HORIZONTAL = 0,
    SCALE_PASS_VERTICAL = 1
} scale_parallel_pass_t;

typedef struct scale_parallel_context {
    unsigned char *dst;
    unsigned char const *src;
    unsigned char *tmp;
    int srcw;
    int srch;
    int dstw;
    int dsth;
    int depth;
    resample_fn_t f_resample;
    double n;
    scale_parallel_pass_t pass;
} scale_parallel_context_t;

/*
 * Allow callers to raise the floor for parallel execution using
 * SIXEL_SCALE_PARALLEL_MIN_BYTES. The default of zero preserves the previous
 * eager behavior while permitting deployments to defer threading on tiny
 * inputs.
 */
static size_t
scale_parallel_min_bytes(void)
{
    static int initialized = 0;
    static size_t threshold = 0;
    char const *text;
    char *endptr;
    unsigned long long parsed;

    if (initialized) {
        return threshold;
    }

    initialized = 1;
    text = getenv("SIXEL_SCALE_PARALLEL_MIN_BYTES");
    if (text == NULL || text[0] == '\0') {
        return threshold;
    }

    errno = 0;
    parsed = strtoull(text, &endptr, 10);
    if (endptr == text || *endptr != '\0' || errno == ERANGE) {
        return threshold;
    }

    if (parsed > (unsigned long long)SIZE_MAX) {
        threshold = SIZE_MAX;
    } else {
        threshold = (size_t)parsed;
    }

    return threshold;
}

static int
scale_parallel_worker(tp_job_t job, void *userdata, void *workspace)
{
    scale_parallel_context_t *ctx;
    int index;

    (void)workspace;
    ctx = (scale_parallel_context_t *)userdata;
    if (ctx == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    index = job.band_index;
    if (ctx->pass == SCALE_PASS_HORIZONTAL) {
        if (index < 0 || index >= ctx->srch) {
            return SIXEL_BAD_ARGUMENT;
        }
        scale_horizontal_row(ctx->tmp,
                             ctx->src,
                             ctx->srcw,
                             ctx->dstw,
                             ctx->depth,
                             index,
                             ctx->f_resample,
                             ctx->n);
    } else {
        if (index < 0 || index >= ctx->dsth) {
            return SIXEL_BAD_ARGUMENT;
        }
        scale_vertical_row(ctx->dst,
                           ctx->tmp,
                           ctx->dstw,
                           ctx->dsth,
                           ctx->depth,
                           ctx->srch,
                           index,
                           ctx->f_resample,
                           ctx->n);
    }

    return SIXEL_OK;
}

/*
 * Parallel path mirrors the encoder and dither thread selection through
 * sixel_threads_resolve(). Rows become individual jobs for both passes so the
 * caller can saturate the threadpool without altering the filtering math.
 */
static int
scale_with_resampling_parallel(
    unsigned char *dst,
    unsigned char const *src,
    int const srcw,
    int const srch,
    int const dstw,
    int const dsth,
    int const depth,
    resample_fn_t const f_resample,
    double const n,
    unsigned char *tmp)
{
    scale_parallel_context_t ctx;
    threadpool_t *pool;
    tp_job_t job;
    size_t image_bytes;
    int threads;
    int queue_depth;
    int y;
    int rc;

    image_bytes = (size_t)srcw * (size_t)srch * (size_t)depth;
    if (image_bytes < scale_parallel_min_bytes()) {
        return SIXEL_BAD_ARGUMENT;
    }

    threads = sixel_threads_resolve();
    if (threads < 2) {
        return SIXEL_BAD_ARGUMENT;
    }

    ctx.dst = dst;
    ctx.src = src;
    ctx.tmp = tmp;
    ctx.srcw = srcw;
    ctx.srch = srch;
    ctx.dstw = dstw;
    ctx.dsth = dsth;
    ctx.depth = depth;
    ctx.f_resample = f_resample;
    ctx.n = n;

    queue_depth = threads * 3;
    if (queue_depth > srch) {
        queue_depth = srch;
    }
    if (queue_depth < 1) {
        queue_depth = 1;
    }

    ctx.pass = SCALE_PASS_HORIZONTAL;
    pool = threadpool_create(threads,
                             queue_depth,
                             0,
                             scale_parallel_worker,
                             &ctx);
    if (pool == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    for (y = 0; y < srch; y++) {
        job.band_index = y;
        threadpool_push(pool, job);
    }
    threadpool_finish(pool);
    rc = threadpool_get_error(pool);
    threadpool_destroy(pool);
    if (rc != SIXEL_OK) {
        return rc;
    }

    queue_depth = threads * 3;
    if (queue_depth > dsth) {
        queue_depth = dsth;
    }
    if (queue_depth < 1) {
        queue_depth = 1;
    }

    ctx.pass = SCALE_PASS_VERTICAL;
    pool = threadpool_create(threads,
                             queue_depth,
                             0,
                             scale_parallel_worker,
                             &ctx);
    if (pool == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    for (y = 0; y < dsth; y++) {
        job.band_index = y;
        threadpool_push(pool, job);
    }
    threadpool_finish(pool);
    rc = threadpool_get_error(pool);
    threadpool_destroy(pool);

    return rc;
}
#endif /* SIXEL_ENABLE_THREADS */

/*
 * Allocate shared scratch storage and attempt the parallel pipeline first so
 * larger inputs benefit from threading while smaller ones retain the serial
 * behavior.
 */
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
    unsigned char *tmp;
    size_t tmp_size;
#if SIXEL_ENABLE_THREADS
    int rc;
#endif

    tmp_size = (size_t)dstw * (size_t)srch * (size_t)depth;
    tmp = (unsigned char *)sixel_allocator_malloc(allocator, tmp_size);
    if (tmp == NULL) {
        return;
    }

#if SIXEL_ENABLE_THREADS
    rc = scale_with_resampling_parallel(dst,
                                        src,
                                        srcw,
                                        srch,
                                        dstw,
                                        dsth,
                                        depth,
                                        f_resample,
                                        n,
                                        tmp);
    if (rc == SIXEL_OK) {
        sixel_allocator_free(allocator, tmp);
        return;
    }
#endif

    scale_with_resampling_serial(dst,
                                 src,
                                 srcw,
                                 srch,
                                 dstw,
                                 dsth,
                                 depth,
                                 f_resample,
                                 n,
                                 tmp);

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

SIXELAPI int
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
