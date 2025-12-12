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

#include "cpu.h"
#include "logger.h"
#include "compat_stub.h"
#include "threading.h"

#if SIXEL_ENABLE_THREADS
# include "threadpool.h"
#endif

#if defined(__GNUC__) && defined(__i386__)
/*
 * i386 callers may enter with only 4- or 8-byte stack alignment. Force
 * realignment for SSE2-heavy routines to avoid movaps spills to unaligned
 * stack slots when SIMD is enabled via SIXEL_SIMD_LEVEL. Mark affected
 * functions noinline so the prologue that performs realignment is not
 * dropped by inlining.
 */
# define SIXEL_ALIGN_STACK __attribute__((force_align_arg_pointer))
# define SIXEL_NO_INLINE __attribute__((noinline))
#else
# define SIXEL_ALIGN_STACK
# define SIXEL_NO_INLINE
#endif

#if defined(HAVE_IMMINTRIN_H) && \
    (defined(__x86_64__) || defined(_M_X64) || defined(__i386) || \
     defined(_M_IX86))
# define SIXEL_HAS_X86_INTRIN 1
# include <immintrin.h>
#endif

#if defined(__GNUC__) && !defined(__clang__)
/*
 * GCC reports a -Wpsabi note when __m512 parameters are present because the
 * calling convention changed in GCC 4.6. All callers and callees in this
 * translation unit share the same compiler, so suppress the note globally to
 * keep the output clean on AVX-512 builds.
 */
#pragma GCC diagnostic ignored "-Wpsabi"
#endif

#if defined(HAVE_SSE2)
/*
 * MSVC does not define __SSE2__ on x86/x64.  Instead, rely on the
 * architecture macros it provides so SIMD paths stay enabled after the
 * configure probe has validated SSE2 support.
 */
# if defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64) || \
    (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#  if defined(HAVE_EMMINTRIN_H)
#   include <emmintrin.h>
#   define SIXEL_USE_SSE2 1
#  endif
# endif
#endif

#if defined(SIXEL_HAS_X86_INTRIN)
# if defined(__GNUC__)
#  if !defined(__clang__)
#   define SIXEL_TARGET_AVX __attribute__((target("avx")))
#   define SIXEL_TARGET_AVX2 __attribute__((target("avx2")))
#   define SIXEL_TARGET_AVX512 __attribute__((target("avx512f")))
#   define SIXEL_USE_AVX 1
#  else
/*
 * clang rejects returning AVX vectors when the translation unit target
 * does not already include the corresponding ISA.  Guard runtime AVX
 * helpers with compile-time ISA availability to keep non-AVX builds
 * warning-free while still using AVX when the compiler enables it.
 */
#   define SIXEL_TARGET_AVX
#   define SIXEL_TARGET_AVX2
#   define SIXEL_TARGET_AVX512
#   if defined(__AVX__)
#    define SIXEL_USE_AVX 1
#   endif
#   if defined(__AVX2__)
#    define SIXEL_USE_AVX2 1
#   endif
#   if defined(__AVX512F__)
#    define SIXEL_USE_AVX512 1
#   endif
#  endif
# else
#  define SIXEL_TARGET_AVX
#  define SIXEL_TARGET_AVX2
#  define SIXEL_TARGET_AVX512
#  if defined(__AVX__)
#   define SIXEL_USE_AVX 1
#  endif
#  if defined(__AVX2__)
#   define SIXEL_USE_AVX2 1
#  endif
#  if defined(__AVX512F__)
#   define SIXEL_USE_AVX512 1
#  endif
# endif
#endif

#if defined(__GNUC__) && !defined(__clang__)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wpsabi"
# undef SIXEL_USE_AVX
# undef SIXEL_USE_AVX2
# undef SIXEL_USE_AVX512
#endif

#if defined(HAVE_NEON)
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

static int
sixel_scale_simd_level(void)
{
    static int simd_level = -2;

    if (simd_level == -2) {
        simd_level = sixel_cpu_simd_level();
#if defined(__i386__)
        /*
         * AVX and later widen the alignment requirement for stack spills to
         * 32 bytes. i386 stack realignment from force_align_arg_pointer only
         * guarantees 16-byte boundaries, so keep the runtime level capped at
         * SSE2 to avoid vmovaps faults when YMM locals spill.
         */
        if (simd_level > SIXEL_SIMD_LEVEL_SSE2) {
            simd_level = SIXEL_SIMD_LEVEL_SSE2;
        }
#endif
    }

    return simd_level;
}

static float
sixel_clamp_unit_f32(float value)
{
    /*
     * Resampling kernels with negative lobes can push linear RGB values
     * outside the unit interval. Clamp here so downstream conversions do
     * not collapse to black.
     */
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }

    return value;
}

#if defined(HAVE_IMMINTRIN_H)
#if defined(SIXEL_USE_AVX)
static SIXEL_ALIGN_STACK SIXEL_TARGET_AVX __m256
sixel_avx_load_rgb_ps(unsigned char const *psrc)
{
    __m128i pixi128;
    __m128 pixf128;
    __m256 pixf256;

    /*
     * Build the byte vector explicitly so the AVX path never accumulates
     * garbage data when widening to 32-bit lanes.
     */
    pixi128 = _mm_setr_epi8((char)psrc[0],
                            (char)psrc[1],
                            (char)psrc[2],
                            0,
                            0, 0, 0, 0,
                            0, 0, 0, 0,
                            0, 0, 0, 0);
    pixf128 = _mm_cvtepi32_ps(pixi128);
    pixf256 = _mm256_castps128_ps256(pixf128);
    pixf256 = _mm256_insertf128_ps(pixf256, _mm_setzero_ps(), 1);
    return pixf256;
}

static SIXEL_ALIGN_STACK SIXEL_TARGET_AVX void
sixel_avx_store_rgb_u8(__m256 acc, double total, unsigned char *dst)
{
    __m256 scalev;
    __m256 minv;
    __m256 maxv;
    __m256i acci;
    int out[8];

    scalev = _mm256_set1_ps((float)(1.0 / total));
    acc = _mm256_mul_ps(acc, scalev);
    minv = _mm256_set1_ps(0.0f);
    maxv = _mm256_set1_ps(255.0f);
    acc = _mm256_max_ps(minv, _mm256_min_ps(acc, maxv));
    acci = _mm256_cvtps_epi32(acc);
    _mm256_storeu_si256((__m256i *)out, acci);
    dst[0] = (unsigned char)out[0];
    dst[1] = (unsigned char)out[1];
    dst[2] = (unsigned char)out[2];
}

static SIXEL_ALIGN_STACK SIXEL_TARGET_AVX __m256
sixel_avx_zero_ps(void)
{
    return _mm256_setzero_ps();
}

static SIXEL_ALIGN_STACK SIXEL_TARGET_AVX __m256
sixel_avx_muladd_ps(__m256 acc, __m256 pix, float weight)
{
    __m256 wv;

    wv = _mm256_set1_ps(weight);
    return _mm256_add_ps(acc, _mm256_mul_ps(pix, wv));
}

static SIXEL_ALIGN_STACK SIXEL_TARGET_AVX __m256
sixel_avx_load_rgb_f32(float const *psrc)
{
    __m256 pixf;

    pixf = _mm256_set_ps(0.0f, 0.0f, 0.0f, 0.0f,
                         psrc[2], psrc[1], psrc[0], 0.0f);
    return pixf;
}

static SIXEL_ALIGN_STACK SIXEL_TARGET_AVX void
sixel_avx_store_rgb_f32(__m256 acc, double total, float *dst)
{
    __m256 scalev;
    __m256 minv;
    __m256 maxv;
    float out[8];

    scalev = _mm256_set1_ps((float)(1.0 / total));
    acc = _mm256_mul_ps(acc, scalev);
    minv = _mm256_set1_ps(0.0f);
    maxv = _mm256_set1_ps(1.0f);
    acc = _mm256_max_ps(minv, _mm256_min_ps(acc, maxv));
    _mm256_storeu_ps(out, acc);
    dst[0] = out[0];
    dst[1] = out[1];
    dst[2] = out[2];
}
#endif  /* SIXEL_USE_AVX */

#if defined(SIXEL_USE_AVX2)
static SIXEL_ALIGN_STACK SIXEL_TARGET_AVX2 __m256
sixel_avx2_load_rgb_ps(unsigned char const *psrc)
{
    __m128i pixi128;
    __m256i pixi256;

    /*
     * Keep the unused bytes zeroed so widening to epi32 does not pull in
     * stack junk and bias every output channel toward white.
     */
    pixi128 = _mm_setr_epi8((char)psrc[0],
                            (char)psrc[1],
                            (char)psrc[2],
                            0,
                            0, 0, 0, 0,
                            0, 0, 0, 0,
                            0, 0, 0, 0);
    pixi256 = _mm256_cvtepu8_epi32(pixi128);
    return _mm256_cvtepi32_ps(pixi256);
}

static SIXEL_ALIGN_STACK SIXEL_TARGET_AVX2 void
sixel_avx2_store_rgb_u8(__m256 acc, double total, unsigned char *dst)
{
    __m256 scalev;
    __m256 minv;
    __m256 maxv;
    __m256i acci;
    int out[8];

    scalev = _mm256_set1_ps((float)(1.0 / total));
    acc = _mm256_mul_ps(acc, scalev);
    minv = _mm256_set1_ps(0.0f);
    maxv = _mm256_set1_ps(255.0f);
    acc = _mm256_max_ps(minv, _mm256_min_ps(acc, maxv));
    acci = _mm256_cvtps_epi32(acc);
    _mm256_storeu_si256((__m256i *)out, acci);
    dst[0] = (unsigned char)out[0];
    dst[1] = (unsigned char)out[1];
    dst[2] = (unsigned char)out[2];
}

static SIXEL_ALIGN_STACK SIXEL_TARGET_AVX2 __m256
sixel_avx2_zero_ps(void)
{
    return _mm256_setzero_ps();
}

static SIXEL_ALIGN_STACK SIXEL_TARGET_AVX2 __m256
sixel_avx2_muladd_ps(__m256 acc, __m256 pix, float weight)
{
    __m256 wv;

    wv = _mm256_set1_ps(weight);
    return _mm256_add_ps(acc, _mm256_mul_ps(pix, wv));
}

static SIXEL_ALIGN_STACK SIXEL_TARGET_AVX2 __m256
sixel_avx2_load_rgb_f32(float const *psrc)
{
    __m256 pixf;

    pixf = _mm256_set_ps(0.0f, 0.0f, 0.0f, 0.0f,
                         psrc[2], psrc[1], psrc[0], 0.0f);
    return pixf;
}

static SIXEL_ALIGN_STACK SIXEL_TARGET_AVX2 void
sixel_avx2_store_rgb_f32(__m256 acc, double total, float *dst)
{
    __m256 scalev;
    __m256 minv;
    __m256 maxv;
    float out[8];

    scalev = _mm256_set1_ps((float)(1.0 / total));
    acc = _mm256_mul_ps(acc, scalev);
    minv = _mm256_set1_ps(0.0f);
    maxv = _mm256_set1_ps(1.0f);
    acc = _mm256_max_ps(minv, _mm256_min_ps(acc, maxv));
    _mm256_storeu_ps(out, acc);
    dst[0] = out[0];
    dst[1] = out[1];
    dst[2] = out[2];
}
#endif  /* SIXEL_USE_AVX2 */

#if defined(SIXEL_USE_AVX512)
static SIXEL_ALIGN_STACK SIXEL_TARGET_AVX512 __m512
sixel_avx512_load_rgb_ps(unsigned char const *psrc)
{
    __m128i pixi128;
    __m512i pixi512;

    pixi128 = _mm_setr_epi8((char)psrc[0],
                            (char)psrc[1],
                            (char)psrc[2],
                            0,
                            0, 0, 0, 0,
                            0, 0, 0, 0,
                            0, 0, 0, 0);
    pixi512 = _mm512_cvtepu8_epi32(pixi128);
    return _mm512_cvtepi32_ps(pixi512);
}

static SIXEL_ALIGN_STACK SIXEL_TARGET_AVX512 void
sixel_avx512_store_rgb_u8(__m512 const *acc,
                          double total,
                          unsigned char *dst)
{
    __m512 scalev;
    __m512 minv;
    __m512 maxv;
    __m512 accv;
    __m512i acci;
    int out[16];

    scalev = _mm512_set1_ps((float)(1.0 / total));
    accv = _mm512_mul_ps(*acc, scalev);
    minv = _mm512_set1_ps(0.0f);
    maxv = _mm512_set1_ps(255.0f);
    accv = _mm512_max_ps(minv, _mm512_min_ps(accv, maxv));
    acci = _mm512_cvtps_epi32(accv);
    _mm512_storeu_si512((void *)out, acci);
    dst[0] = (unsigned char)out[0];
    dst[1] = (unsigned char)out[1];
    dst[2] = (unsigned char)out[2];
}

static SIXEL_ALIGN_STACK SIXEL_TARGET_AVX512 __m512
sixel_avx512_zero_ps(void)
{
    return _mm512_setzero_ps();
}

static SIXEL_ALIGN_STACK SIXEL_TARGET_AVX512 __m512
sixel_avx512_muladd_ps(__m512 acc, __m512 pix, float weight)
{
    __m512 wv;

    wv = _mm512_set1_ps(weight);
    return _mm512_add_ps(acc, _mm512_mul_ps(pix, wv));
}

static SIXEL_ALIGN_STACK SIXEL_TARGET_AVX512 __m512
sixel_avx512_load_rgb_f32(float const *psrc)
{
    __m512 pixf;

    pixf = _mm512_set_ps(0.0f, 0.0f, 0.0f, 0.0f,
                         0.0f, 0.0f, 0.0f, 0.0f,
                         0.0f, 0.0f, 0.0f, 0.0f,
                         psrc[2], psrc[1], psrc[0], 0.0f);
    return pixf;
}

static SIXEL_ALIGN_STACK SIXEL_TARGET_AVX512 void
sixel_avx512_store_rgb_f32(__m512 const *acc,
                           double total,
                           float *dst)
{
    __m512 scalev;
    __m512 minv;
    __m512 maxv;
    __m512 accv;
    float out[16];

    scalev = _mm512_set1_ps((float)(1.0 / total));
    accv = _mm512_mul_ps(*acc, scalev);
    minv = _mm512_set1_ps(0.0f);
    maxv = _mm512_set1_ps(1.0f);
    accv = _mm512_max_ps(minv, _mm512_min_ps(accv, maxv));
    _mm512_storeu_ps(out, accv);
    dst[0] = out[0];
    dst[1] = out[1];
    dst[2] = out[2];
}
#endif  /* SIXEL_USE_AVX512 */
#endif /* HAVE_IMMINTRIN_H */


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

static void
scale_without_resampling_float32(
    float *dst,
    float const *src,
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

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
/*
 * GCC emits a -Wpsabi note for __m512 parameters because the calling
 * convention changed in GCC 4.6. The functions only pass vectors between
 * helpers compiled with the same compiler, so suppress the noise locally.
 */
#pragma GCC diagnostic ignored "-Wpsabi"
#endif

/*
 * Two-pass separable filter helpers. Each function processes a single row so
 * the caller may invoke them serially or from a threadpool worker. On i386 we
 * also mark the functions noinline to ensure the stack-realigning prologue
 * from SIXEL_ALIGN_STACK is preserved under optimization.
 */
static SIXEL_ALIGN_STACK SIXEL_NO_INLINE void
scale_horizontal_row(
    unsigned char *tmp,
    unsigned char const *src,
    int const srcw,
    int const dstw,
    int const depth,
    int const y,
    resample_fn_t const f_resample,
    double const n,
    int const simd_level)
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
#if !defined(SIXEL_USE_AVX512) && !defined(SIXEL_USE_AVX2) && \
    !defined(SIXEL_USE_AVX) && !defined(SIXEL_USE_SSE2) && \
    !defined(SIXEL_USE_NEON)
    /*
     * No SIMD backends are compiled for this target, so the SIMD level gate
     * becomes a dead parameter. Silence -Wunused-parameter on 32-bit GCC
     * builds while keeping the signature identical across configurations.
     */
    (void)simd_level;
#endif
#if defined(SIXEL_USE_AVX512)
    __m512 acc512;
    __m512 pix512;
#endif
#if defined(SIXEL_USE_AVX2) || defined(SIXEL_USE_AVX)
    __m256 acc256;
#endif
#if defined(SIXEL_USE_SSE2)
    /*
     * __m128 locals remain on the stack. On i386 callers may arrive with
     * only 4- or 8-byte alignment, so movaps spills can fault when SSE2 is
     * forced. SIXEL_ALIGN_STACK realigns the frame on entry to keep the
     * SSE2 path consistent with the 16-byte guarantee on x86_64.
     */
    __m128 acc128;
    __m128 minv128;
    __m128 maxv128;
    __m128 scalev128;
    __m128 wv128;
    __m128 pixf128;
    __m128i pixi128;
    __m128i acci128;
    __m128i acc16_128;
    unsigned int pixel128;
#endif
#if defined(SIXEL_USE_NEON)
    float32x4_t acc_neon;
    float32x4_t minv_neon;
    float32x4_t maxv_neon;
    float32x4_t scalev_neon;
    float32x4_t wv_neon;
    float32x4_t pixf_neon;
    uint32x4_t pix32_neon;
    uint32x4_t acci_neon;
    uint16x4_t acc16_neon;
    uint8x8_t acc8_neon;
    uint8_t outb_neon[8];
#endif

    for (w = 0; w < dstw; w++) {
        total = 0.0;
        for (i = 0; i < depth; i++) {
            offsets[i] = 0;
        }

        if (dstw >= srcw) {
            center_x = (w + 0.5) * srcw / dstw;
            x_first = MAX((int)(center_x - n), 0);
            x_last = MIN((int)(center_x + n), srcw - 1);
        } else {
            center_x = w + 0.5;
            x_first = MAX((int)floor((center_x - n) * srcw / dstw), 0);
            x_last = MIN((int)floor((center_x + n) * srcw / dstw),
                         srcw - 1);
        }

#if defined(SIXEL_USE_AVX512)
        if (depth == 3 && simd_level >= SIXEL_SIMD_LEVEL_AVX512) {
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpsabi"
#endif
            acc512 = sixel_avx512_zero_ps();

            for (x = x_first; x <= x_last; x++) {
                diff_x = (dstw >= srcw)
                             ? (x + 0.5) - center_x
                             : (x + 0.5) * dstw / srcw - center_x;
                weight = f_resample(fabs(diff_x));
                pos = (y * srcw + x) * depth;
                pix512 = sixel_avx512_load_rgb_ps(src + pos);
                acc512 = sixel_avx512_muladd_ps(
                    acc512,
                    pix512,
                    (float)weight);
                total += weight;
            }
            if (total > 0.0) {
                pos = (y * dstw + w) * depth;
                sixel_avx512_store_rgb_u8(&acc512, total, tmp + pos);
            }
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
            continue;
        }
#endif
#if defined(SIXEL_USE_AVX2)
        if (depth == 3 && simd_level >= SIXEL_SIMD_LEVEL_AVX2) {
            acc256 = sixel_avx2_zero_ps();

            for (x = x_first; x <= x_last; x++) {
                diff_x = (dstw >= srcw)
                             ? (x + 0.5) - center_x
                             : (x + 0.5) * dstw / srcw - center_x;
                weight = f_resample(fabs(diff_x));
                pos = (y * srcw + x) * depth;
                acc256 = sixel_avx2_muladd_ps(
                    acc256,
                    sixel_avx2_load_rgb_ps(src + pos),
                    (float)weight);
                total += weight;
            }
            if (total > 0.0) {
                pos = (y * dstw + w) * depth;
                sixel_avx2_store_rgb_u8(acc256, total, tmp + pos);
            }
            continue;
        }
#endif
#if defined(SIXEL_USE_AVX)
        if (depth == 3 && simd_level >= SIXEL_SIMD_LEVEL_AVX) {
            acc256 = sixel_avx_zero_ps();

            for (x = x_first; x <= x_last; x++) {
                diff_x = (dstw >= srcw)
                             ? (x + 0.5) - center_x
                             : (x + 0.5) * dstw / srcw - center_x;
                weight = f_resample(fabs(diff_x));
                pos = (y * srcw + x) * depth;
                acc256 = sixel_avx_muladd_ps(
                    acc256,
                    sixel_avx_load_rgb_ps(src + pos),
                    (float)weight);
                total += weight;
            }
            if (total > 0.0) {
                pos = (y * dstw + w) * depth;
                sixel_avx_store_rgb_u8(acc256, total, tmp + pos);
            }
            continue;
        }
#endif
#if defined(SIXEL_USE_SSE2) || defined(SIXEL_USE_NEON)
        if (depth == 3
# if defined(SIXEL_USE_SSE2)
            && simd_level >= SIXEL_SIMD_LEVEL_SSE2
# elif defined(SIXEL_USE_NEON)
            && simd_level >= SIXEL_SIMD_LEVEL_NEON
# endif
            ) {
#if defined(SIXEL_USE_SSE2)
            acc128 = _mm_setzero_ps();
#elif defined(SIXEL_USE_NEON)
            acc_neon = vdupq_n_f32(0.0f);
#endif
            for (x = x_first; x <= x_last; x++) {
                diff_x = (dstw >= srcw)
                             ? (x + 0.5) - center_x
                             : (x + 0.5) * dstw / srcw - center_x;
                weight = f_resample(fabs(diff_x));
                pos = (y * srcw + x) * depth;
                const unsigned char *psrc = src + pos;
#if defined(SIXEL_USE_SSE2)
                pixel128 = psrc[0] | (psrc[1] << 8) | (psrc[2] << 16);
                pixi128 = _mm_cvtsi32_si128((int)pixel128);
                pixi128 = _mm_unpacklo_epi8(pixi128, _mm_setzero_si128());
                pixi128 = _mm_unpacklo_epi16(pixi128, _mm_setzero_si128());
                pixf128 = _mm_cvtepi32_ps(pixi128);
                wv128 = _mm_set1_ps((float)weight);
                acc128 = _mm_add_ps(acc128, _mm_mul_ps(pixf128, wv128));
#else /* NEON */
                pix32_neon = (uint32x4_t){psrc[0], psrc[1], psrc[2], 0};
                pixf_neon = vcvtq_f32_u32(pix32_neon);
                wv_neon = vdupq_n_f32((float)weight);
                acc_neon = vmlaq_f32(acc_neon, pixf_neon, wv_neon);
#endif
                total += weight;
            }
            if (total > 0.0) {
#if defined(SIXEL_USE_SSE2)
                scalev128 = _mm_set1_ps((float)(1.0 / total));
                acc128 = _mm_mul_ps(acc128, scalev128);
                minv128 = _mm_set1_ps(0.0f);
                maxv128 = _mm_set1_ps(255.0f);
                acc128 = _mm_max_ps(minv128, _mm_min_ps(acc128, maxv128));
                acci128 = _mm_cvtps_epi32(acc128);
                acc16_128 = _mm_packs_epi32(acci128, _mm_setzero_si128());
                acc16_128 = _mm_packus_epi16(acc16_128, _mm_setzero_si128());
                pos = (y * dstw + w) * depth;
                pixel128 = (unsigned int)_mm_cvtsi128_si32(acc16_128);
                tmp[pos + 0] = (unsigned char)pixel128;
                tmp[pos + 1] = (unsigned char)(pixel128 >> 8);
                tmp[pos + 2] = (unsigned char)(pixel128 >> 16);
#else /* NEON */
                scalev_neon = vdupq_n_f32((float)(1.0 / total));
                acc_neon = vmulq_f32(acc_neon, scalev_neon);
                minv_neon = vdupq_n_f32(0.0f);
                maxv_neon = vdupq_n_f32(255.0f);
                acc_neon = vmaxq_f32(minv_neon,
                                     vminq_f32(acc_neon, maxv_neon));
                acci_neon = vcvtq_u32_f32(acc_neon);
                acc16_neon = vmovn_u32(acci_neon);
                acc8_neon = vmovn_u16(vcombine_u16(acc16_neon, acc16_neon));

                vst1_u8(outb_neon, acc8_neon);
                pos = (y * dstw + w) * depth;
                tmp[pos + 0] = outb_neon[0];
                tmp[pos + 1] = outb_neon[1];
                tmp[pos + 2] = outb_neon[2];
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

static SIXEL_ALIGN_STACK SIXEL_NO_INLINE void
scale_vertical_row(
    unsigned char *dst,
    unsigned char const *tmp,
    int const dstw,
    int const dsth,
    int const depth,
    int const srch,
    int const h,
    resample_fn_t const f_resample,
    double const n,
    int const simd_level)
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
#if !defined(SIXEL_USE_AVX512) && !defined(SIXEL_USE_AVX2) && \
    !defined(SIXEL_USE_AVX) && !defined(SIXEL_USE_SSE2) && \
    !defined(SIXEL_USE_NEON)
    /*
     * When no SIMD implementations are present the runtime SIMD level does
     * not influence the algorithm. Mark it unused to keep 32-bit GCC quiet
     * without altering the interface shared with SIMD-enabled builds.
     */
    (void)simd_level;
#endif
#if defined(SIXEL_USE_AVX512)
    __m512 acc512;
    __m512 pix512;
#endif
#if defined(SIXEL_USE_AVX2) || defined(SIXEL_USE_AVX)
    __m256 acc256;
#endif
#if defined(SIXEL_USE_SSE2)
    __m128 acc128;
    __m128 minv128;
    __m128 maxv128;
    __m128 scalev128;
    __m128 wv128;
    __m128 pixf128;
    __m128i pixi128;
    __m128i acci128;
    __m128i acc16_128;
    unsigned int pixel128;
#endif
#if defined(SIXEL_USE_NEON)
    float32x4_t acc_neon;
    float32x4_t minv_neon;
    float32x4_t maxv_neon;
    float32x4_t scalev_neon;
    float32x4_t wv_neon;
    float32x4_t pixf_neon;
    uint32x4_t pix32_neon;
    uint32x4_t acci_neon;
    uint16x4_t acc16_neon;
    uint8x8_t acc8_neon;
    uint8_t outb_neon[8];
#endif

    for (w = 0; w < dstw; w++) {
        total = 0.0;
        for (i = 0; i < depth; i++) {
            offsets[i] = 0;
        }

        if (dsth >= srch) {
            center_y = (h + 0.5) * srch / dsth;
            y_first = MAX((int)(center_y - n), 0);
            y_last = MIN((int)(center_y + n), srch - 1);
        } else {
            center_y = h + 0.5;
            y_first = MAX((int)floor((center_y - n) * srch / dsth), 0);
            y_last = MIN((int)floor((center_y + n) * srch / dsth),
                         srch - 1);
        }

#if defined(SIXEL_USE_AVX512)
        if (depth == 3 && simd_level >= SIXEL_SIMD_LEVEL_AVX512) {
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpsabi"
#endif
            acc512 = sixel_avx512_zero_ps();

            for (y = y_first; y <= y_last; y++) {
                diff_y = (dsth >= srch)
                             ? (y + 0.5) - center_y
                             : (y + 0.5) * dsth / srch - center_y;
                weight = f_resample(fabs(diff_y));
                pos = (y * dstw + w) * depth;
                pix512 = sixel_avx512_load_rgb_ps(tmp + pos);
                acc512 = sixel_avx512_muladd_ps(
                    acc512,
                    pix512,
                    (float)weight);
                total += weight;
            }
            if (total > 0.0) {
                pos = (h * dstw + w) * depth;
                sixel_avx512_store_rgb_u8(&acc512, total, dst + pos);
            }
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
            continue;
        }
#endif
#if defined(SIXEL_USE_AVX2)
        if (depth == 3 && simd_level >= SIXEL_SIMD_LEVEL_AVX2) {
            acc256 = sixel_avx2_zero_ps();

            for (y = y_first; y <= y_last; y++) {
                diff_y = (dsth >= srch)
                             ? (y + 0.5) - center_y
                             : (y + 0.5) * dsth / srch - center_y;
                weight = f_resample(fabs(diff_y));
                pos = (y * dstw + w) * depth;
                acc256 = sixel_avx2_muladd_ps(
                    acc256,
                    sixel_avx2_load_rgb_ps(tmp + pos),
                    (float)weight);
                total += weight;
            }
            if (total > 0.0) {
                pos = (h * dstw + w) * depth;
                sixel_avx2_store_rgb_u8(acc256, total, dst + pos);
            }
            continue;
        }
#endif
#if defined(SIXEL_USE_AVX)
        if (depth == 3 && simd_level >= SIXEL_SIMD_LEVEL_AVX) {
            acc256 = sixel_avx_zero_ps();

            for (y = y_first; y <= y_last; y++) {
                diff_y = (dsth >= srch)
                             ? (y + 0.5) - center_y
                             : (y + 0.5) * dsth / srch - center_y;
                weight = f_resample(fabs(diff_y));
                pos = (y * dstw + w) * depth;
                acc256 = sixel_avx_muladd_ps(
                    acc256,
                    sixel_avx_load_rgb_ps(tmp + pos),
                    (float)weight);
                total += weight;
            }
            if (total > 0.0) {
                pos = (h * dstw + w) * depth;
                sixel_avx_store_rgb_u8(acc256, total, dst + pos);
            }
            continue;
        }
#endif
#if defined(SIXEL_USE_SSE2) || defined(SIXEL_USE_NEON)
        if (depth == 3
# if defined(SIXEL_USE_SSE2)
            && simd_level >= SIXEL_SIMD_LEVEL_SSE2
# elif defined(SIXEL_USE_NEON)
            && simd_level >= SIXEL_SIMD_LEVEL_NEON
# endif
            ) {
#if defined(SIXEL_USE_SSE2)
            acc128 = _mm_setzero_ps();
#elif defined(SIXEL_USE_NEON)
            acc_neon = vdupq_n_f32(0.0f);
#endif
            for (y = y_first; y <= y_last; y++) {
                diff_y = (dsth >= srch)
                             ? (y + 0.5) - center_y
                             : (y + 0.5) * dsth / srch - center_y;
                weight = f_resample(fabs(diff_y));
                pos = (y * dstw + w) * depth;
                const unsigned char *psrc = tmp + pos;
#if defined(SIXEL_USE_SSE2)
                pixel128 = psrc[0] | (psrc[1] << 8) | (psrc[2] << 16);
                pixi128 = _mm_cvtsi32_si128((int)pixel128);
                pixi128 = _mm_unpacklo_epi8(pixi128, _mm_setzero_si128());
                pixi128 = _mm_unpacklo_epi16(pixi128, _mm_setzero_si128());
                pixf128 = _mm_cvtepi32_ps(pixi128);
                wv128 = _mm_set1_ps((float)weight);
                acc128 = _mm_add_ps(acc128, _mm_mul_ps(pixf128, wv128));
#else /* NEON */
                pix32_neon = (uint32x4_t){psrc[0], psrc[1], psrc[2], 0};
                pixf_neon = vcvtq_f32_u32(pix32_neon);
                wv_neon = vdupq_n_f32((float)weight);
                acc_neon = vmlaq_f32(acc_neon, pixf_neon, wv_neon);
#endif
                total += weight;
            }
            if (total > 0.0) {
#if defined(SIXEL_USE_SSE2)
                scalev128 = _mm_set1_ps((float)(1.0 / total));
                acc128 = _mm_mul_ps(acc128, scalev128);
                minv128 = _mm_set1_ps(0.0f);
                maxv128 = _mm_set1_ps(255.0f);
                acc128 = _mm_max_ps(minv128, _mm_min_ps(acc128, maxv128));
                acci128 = _mm_cvtps_epi32(acc128);
                acc16_128 = _mm_packs_epi32(acci128, _mm_setzero_si128());
                acc16_128 = _mm_packus_epi16(acc16_128, _mm_setzero_si128());
                pos = (h * dstw + w) * depth;
                pixel128 = (unsigned int)_mm_cvtsi128_si32(acc16_128);
                dst[pos + 0] = (unsigned char)pixel128;
                dst[pos + 1] = (unsigned char)(pixel128 >> 8);
                dst[pos + 2] = (unsigned char)(pixel128 >> 16);
#else /* NEON */
                scalev_neon = vdupq_n_f32((float)(1.0 / total));
                acc_neon = vmulq_f32(acc_neon, scalev_neon);
                minv_neon = vdupq_n_f32(0.0f);
                maxv_neon = vdupq_n_f32(255.0f);
                acc_neon = vmaxq_f32(minv_neon,
                                     vminq_f32(acc_neon, maxv_neon));
                acci_neon = vcvtq_u32_f32(acc_neon);
                acc16_neon = vmovn_u32(acci_neon);
                acc8_neon = vmovn_u16(vcombine_u16(acc16_neon, acc16_neon));

                vst1_u8(outb_neon, acc8_neon);
                pos = (h * dstw + w) * depth;
                dst[pos + 0] = outb_neon[0];
                dst[pos + 1] = outb_neon[1];
                dst[pos + 2] = outb_neon[2];
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

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

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
    int simd_level;

    simd_level = sixel_scale_simd_level();
#if !defined(SIXEL_USE_AVX512) && !defined(SIXEL_USE_AVX2) && \
    !defined(SIXEL_USE_AVX) && !defined(SIXEL_USE_SSE2) && \
    !defined(SIXEL_USE_NEON)
    /*
     * GCC i686 builds can compile this function without any SIMD backends
     * enabled; consume the detection result to keep the signature stable
     * while avoiding an unused-but-set-variable warning.
     */
    (void)simd_level;
#endif

    for (y = 0; y < srch; y++) {
        scale_horizontal_row(tmp,
                             src,
                             srcw,
                             dstw,
                             depth,
                             y,
                             f_resample,
                             n,
                             simd_level);
    }

    for (h = 0; h < dsth; h++) {
        scale_vertical_row(dst,
                           tmp,
                           dstw,
                           dsth,
                           depth,
                           srch,
                           h,
                           f_resample,
                           n,
                           simd_level);
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
    int simd_level;
    int band_span;
    sixel_logger_t *logger;
} scale_parallel_context_t;

/*
 * Emit timeline entries for every band so downstream aggregation can compute
 * first/last activity windows per thread without losing information.
 */
static int
scale_parallel_should_log(scale_parallel_context_t const *ctx, int index)
{
    int span;

    if (ctx == NULL || ctx->logger == NULL || !ctx->logger->active) {
        return 0;
    }

    if (index < 0) {
        return 0;
    }

    if (ctx->pass == SCALE_PASS_HORIZONTAL) {
        span = ctx->srch;
    } else {
        span = ctx->dsth;
    }

    if (span <= 0 || index >= span) {
        return 0;
    }

    return 1;
}

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
    text = sixel_compat_getenv("SIXEL_SCALE_PARALLEL_MIN_BYTES");
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

/*
 * Choose the number of rows handled per threadpool job. We prefer an
 * environment override via SIXEL_PARALLEL_FACTOR so deployments can tune
 * queueing overhead. Otherwise derive a span from rows/threads and clamp to
 * [1, rows]. The value is cached after the first lookup.
 */
static int
scale_parallel_band_span(int rows, int threads)
{
    static int initialized = 0;
    static int env_span = 0;
    char const *text;
    char *endptr;
    long parsed;
    int span;

    if (rows <= 0) {
        return 1;
    }

    if (!initialized) {
        initialized = 1;
        text = sixel_compat_getenv("SIXEL_PARALLEL_FACTOR");
        if (text != NULL && text[0] != '\0') {
            errno = 0;
            parsed = strtol(text, &endptr, 10);
            if (endptr != text && *endptr == '\0' && errno != ERANGE &&
                parsed > 0 && parsed <= INT_MAX) {
                env_span = (int)parsed;
            }
        }
    }

    if (env_span > 0) {
        span = env_span;
    } else {
        span = rows / threads;
    }

    if (span < 1) {
        span = 1;
    }
    if (span > rows) {
        span = rows;
    }

    return span;
}

static int
scale_parallel_worker(tp_job_t job, void *userdata, void *workspace)
{
    scale_parallel_context_t *ctx;
    int index;
    char const *role;
    int y0;
    int y1;
    int in0;
    int in1;
    int limit;
    int y;

    (void)workspace;
    ctx = (scale_parallel_context_t *)userdata;
    if (ctx == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    role = "horizontal";
    y0 = 0;
    y1 = 0;
    in0 = 0;
    in1 = 0;
    index = job.band_index;
    limit = ctx->srch;
    if (ctx->pass == SCALE_PASS_HORIZONTAL) {
        limit = ctx->srch;
    } else {
        limit = ctx->dsth;
    }

    if (index < 0 || index >= limit) {
        return SIXEL_BAD_ARGUMENT;
    }

    y0 = index;
    y1 = index + ctx->band_span;
    if (y1 > limit) {
        y1 = limit;
    }

    if (ctx->pass == SCALE_PASS_HORIZONTAL) {
        in1 = ctx->dstw;
        if (scale_parallel_should_log(ctx, index)) {
            sixel_logger_logf(ctx->logger,
                              role,
                              "scale",
                              "start",
                              index,
                              y1 - 1,
                              y0,
                              y1,
                              in0,
                              in1,
                              "horizontal pass");
        }
        for (y = y0; y < y1; y++) {
            scale_horizontal_row(ctx->tmp,
                                 ctx->src,
                                 ctx->srcw,
                                 ctx->dstw,
                                 ctx->depth,
                                 y,
                                 ctx->f_resample,
                                 ctx->n,
                                 ctx->simd_level);
        }
    } else {
        role = "vertical";
        in1 = ctx->srch;
        if (scale_parallel_should_log(ctx, index)) {
            sixel_logger_logf(ctx->logger,
                              role,
                              "scale",
                              "start",
                              index,
                              y1 - 1,
                              y0,
                              y1,
                              in0,
                              in1,
                              "vertical pass");
        }
        for (y = y0; y < y1; y++) {
            scale_vertical_row(ctx->dst,
                               ctx->tmp,
                               ctx->dstw,
                               ctx->dsth,
                               ctx->depth,
                               ctx->srch,
                               y,
                               ctx->f_resample,
                               ctx->n,
                               ctx->simd_level);
        }
    }

    if (scale_parallel_should_log(ctx, index)) {
        sixel_logger_logf(ctx->logger,
                          role,
                          "scale",
                          "finish",
                          index,
                          y1 - 1,
                          y0,
                          y1,
                          in0,
                          in1,
                          "pass complete");
    }

    return SIXEL_OK;
}

/*
 * Parallel path mirrors the encoder and dither thread selection through
 * sixel_threads_resolve(). Rows are batched into jobs for both passes so the
 * caller can saturate the threadpool without altering the filtering math while
 * reducing queue overhead.
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
    unsigned char *tmp,
    sixel_logger_t *logger)
{
    scale_parallel_context_t ctx;
    threadpool_t *pool;
    tp_job_t job;
    size_t image_bytes;
    int threads;
    int queue_depth;
    int y;
    int rc;
    int logger_ready;
    int horizontal_span;
    int vertical_span;

    image_bytes = (size_t)srcw * (size_t)srch * (size_t)depth;
    if (image_bytes < scale_parallel_min_bytes()) {
        if (logger != NULL) {
            sixel_logger_logf(logger,
                              "controller",
                              "scale",
                              "skip",
                              -1,
                              -1,
                              0,
                              0,
                              0,
                              0,
                              "below threshold bytes=%zu",
                              image_bytes);
        }
        return SIXEL_BAD_ARGUMENT;
    }

    threads = sixel_threads_resolve();
    if (threads < 2) {
        if (logger != NULL) {
            sixel_logger_logf(logger,
                              "controller",
                              "scale",
                              "skip",
                              -1,
                              -1,
                              0,
                              0,
                              0,
                              0,
                              "threads=%d",
                              threads);
        }
        return SIXEL_BAD_ARGUMENT;
    }

    logger_ready = logger != NULL && logger->active;
    if (logger_ready) {
        sixel_logger_logf(logger,
                          "controller",
                          "scale",
                          "start",
                          -1,
                          -1,
                          0,
                          srch,
                          0,
                          dsth,
                          "parallel scale src=%dx%d dst=%dx%d",
                          srcw,
                          srch,
                          dstw,
                          dsth);
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
    ctx.simd_level = sixel_scale_simd_level();
    ctx.logger = logger_ready ? logger : NULL;

    /*
     * Batch rows to reduce queue churn. Prefer the environment override so
     * deployments can pin a consistent span; otherwise derive a default from
     * rows per thread.
     */
    horizontal_span = scale_parallel_band_span(srch, threads);
    vertical_span = scale_parallel_band_span(dsth, threads);

    queue_depth = threads * 3;
    if (queue_depth > srch) {
        queue_depth = srch;
    }
    if (queue_depth < 1) {
        queue_depth = 1;
    }

    ctx.pass = SCALE_PASS_HORIZONTAL;
    ctx.band_span = horizontal_span;
    if (logger_ready) {
        sixel_logger_logf(logger,
                          "controller",
                          "scale",
                          "pass_start",
                          -1,
                          0,
                          0,
                          srch,
                          0,
                          ctx.dstw,
                          "horizontal queue=%d threads=%d",
                          queue_depth,
                          threads);
    }
    pool = threadpool_create(threads,
                             queue_depth,
                             0,
                             scale_parallel_worker,
                             &ctx,
                             NULL);
    if (pool == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    for (y = 0; y < srch; y += horizontal_span) {
        job.band_index = y;
        threadpool_push(pool, job);
    }
    threadpool_finish(pool);
    rc = threadpool_get_error(pool);
    threadpool_destroy(pool);
    if (rc != SIXEL_OK) {
        return rc;
    }

    if (logger_ready) {
        sixel_logger_logf(logger,
                          "controller",
                          "scale",
                          "pass_finish",
                          -1,
                          srch - 1,
                          0,
                          srch,
                          0,
                          ctx.dstw,
                          "horizontal complete");
    }

    queue_depth = threads * 3;
    if (queue_depth > dsth) {
        queue_depth = dsth;
    }
    if (queue_depth < 1) {
        queue_depth = 1;
    }

    ctx.pass = SCALE_PASS_VERTICAL;
    ctx.band_span = vertical_span;
    if (logger_ready) {
        sixel_logger_logf(logger,
                          "controller",
                          "scale",
                          "pass_start",
                          -1,
                          0,
                          0,
                          dsth,
                          0,
                          ctx.srch,
                          "vertical queue=%d threads=%d",
                          queue_depth,
                          threads);
    }
    pool = threadpool_create(threads,
                             queue_depth,
                             0,
                             scale_parallel_worker,
                             &ctx,
                             NULL);
    if (pool == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    for (y = 0; y < dsth; y += vertical_span) {
        job.band_index = y;
        threadpool_push(pool, job);
    }
    threadpool_finish(pool);
    rc = threadpool_get_error(pool);
    threadpool_destroy(pool);

    if (logger_ready) {
        sixel_logger_logf(logger,
                          "controller",
                          "scale",
                          "pass_finish",
                          -1,
                          dsth - 1,
                          0,
                          dsth,
                          0,
                          ctx.srch,
                          "vertical complete rc=%d",
                          rc);
        sixel_logger_logf(logger,
                          "controller",
                          "scale",
                          "finish",
                          -1,
                          dsth - 1,
                          0,
                          dsth,
                          0,
                          ctx.srch,
                          "parallel scale status=%d",
                          rc);
    }

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
    sixel_logger_t logger;
    int logger_prepared;
#endif

#if SIXEL_ENABLE_THREADS
    sixel_logger_init(&logger);
    logger_prepared = 0;
    (void)sixel_logger_prepare_env(&logger);
    logger_prepared = logger.active;
#endif

    tmp_size = (size_t)dstw * (size_t)srch * (size_t)depth;
    tmp = (unsigned char *)sixel_allocator_malloc(allocator, tmp_size);
    if (tmp == NULL) {
#if SIXEL_ENABLE_THREADS
        if (logger_prepared) {
            sixel_logger_close(&logger);
        }
#endif
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
                                        tmp,
                                        logger_prepared
                                            ? &logger
                                            : NULL);
    if (rc == SIXEL_OK) {
        sixel_allocator_free(allocator, tmp);
        if (logger_prepared) {
            sixel_logger_close(&logger);
        }
        return;
    }

    if (logger_prepared) {
        sixel_logger_logf(&logger,
                          "controller",
                          "scale",
                          "fallback",
                          -1,
                          -1,
                          0,
                          dsth,
                          0,
                          srch,
                          "parallel rc=%d",
                          rc);
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
#if SIXEL_ENABLE_THREADS
    if (logger_prepared) {
        sixel_logger_close(&logger);
    }
#endif
}

/*
 * Floating-point scaler mirrors the byte-path SSE2 usage. Keep it noinline
 * on i386 so the SIXEL_ALIGN_STACK prologue stays in place when SSE2 locals
 * need to spill to the stack.
 */
static SIXEL_ALIGN_STACK SIXEL_NO_INLINE void
scale_with_resampling_float32(
    float *dst,
    float const *src,
    int const srcw,
    int const srch,
    int const dstw,
    int const dsth,
    int const depth,
    resample_fn_t const f_resample,
    double n,
    sixel_allocator_t *allocator)
{
    int w;
    int h;
    int x;
    int y;
    int i;
    int pos;
    int x_first;
    int x_last;
    int y_first;
    int y_last;
    double center_x;
    double center_y;
    double diff_x;
    double diff_y;
    double weight;
    double total;
    double offsets[8];
    float *tmp;
#if defined(SIXEL_USE_SSE2) || defined(SIXEL_USE_NEON)
    float vecbuf[4];
#endif
    int simd_level;
#if defined(SIXEL_USE_AVX512)
    __m512 acc512;
    __m512 pix512;
#endif
#if defined(SIXEL_USE_AVX2) || defined(SIXEL_USE_AVX)
    __m256 acc256;
#endif
#if defined(SIXEL_USE_SSE2)
    __m128 acc128;
    __m128 pixf128;
    __m128 wv128;
    __m128 scalev128;
    __m128 minv128;
    __m128 maxv128;
#elif defined(SIXEL_USE_NEON)
    float32x4_t acc_neon;
    float32x4_t pixf_neon;
    float32x4_t wv_neon;
    float32x4_t scalev_neon;
    float32x4_t minv_neon;
    float32x4_t maxv_neon;
#endif

    tmp = (float *)sixel_allocator_malloc(
        allocator,
        (size_t)(dstw * srch * depth * (int)sizeof(float)));
    if (tmp == NULL) {
        return;
    }

    simd_level = sixel_scale_simd_level();
#if !defined(SIXEL_USE_AVX512) && !defined(SIXEL_USE_AVX2) && \
    !defined(SIXEL_USE_AVX) && !defined(SIXEL_USE_SSE2) && \
    !defined(SIXEL_USE_NEON)
    /*
     * GCC i686 builds can reach this function with every SIMD backend
     * compiled out; acknowledge the detection result to avoid an unused
     * write while keeping the signature intact.
     */
    (void)simd_level;
#endif

    for (y = 0; y < srch; y++) {
        for (w = 0; w < dstw; w++) {
            total = 0.0;
            for (i = 0; i < depth; i++) {
                offsets[i] = 0.0;
            }

        if (dstw >= srcw) {
            center_x = (w + 0.5) * srcw / dstw;
            x_first = MAX((int)(center_x - n), 0);
            x_last = MIN((int)(center_x + n), srcw - 1);
        } else {
            center_x = w + 0.5;
            x_first = MAX((int)floor((center_x - n) * srcw / dstw), 0);
            x_last = MIN((int)floor((center_x + n) * srcw / dstw),
                         srcw - 1);
        }

#if defined(SIXEL_USE_AVX512)
            if (depth == 3 &&
                simd_level >= SIXEL_SIMD_LEVEL_AVX512) {
                acc512 = sixel_avx512_zero_ps();

                for (x = x_first; x <= x_last; x++) {
                    diff_x = (dstw >= srcw)
                                 ? (x + 0.5) - center_x
                                 : (x + 0.5) * srcw / dstw - center_x;
                    weight = f_resample(fabs(diff_x));
                    pos = (y * srcw + x) * depth;
                    pix512 = sixel_avx512_load_rgb_f32(src + pos);
                    acc512 = sixel_avx512_muladd_ps(
                        acc512,
                        pix512,
                        (float)weight);
                    total += weight;
                }
                if (total > 0.0) {
                    pos = (y * dstw + w) * depth;
                    sixel_avx512_store_rgb_f32(&acc512, total, tmp + pos);
                }
            } else
#endif
#if defined(SIXEL_USE_AVX2)
            if (depth == 3 && simd_level >= SIXEL_SIMD_LEVEL_AVX2) {
                acc256 = sixel_avx2_zero_ps();

                for (x = x_first; x <= x_last; x++) {
                    diff_x = (dstw >= srcw)
                                 ? (x + 0.5) - center_x
                                 : (x + 0.5) * srcw / dstw - center_x;
                    weight = f_resample(fabs(diff_x));
                    pos = (y * srcw + x) * depth;
                    acc256 = sixel_avx2_muladd_ps(
                        acc256,
                        sixel_avx2_load_rgb_f32(src + pos),
                        (float)weight);
                    total += weight;
                }
                if (total > 0.0) {
                    pos = (y * dstw + w) * depth;
                    sixel_avx2_store_rgb_f32(acc256, total, tmp + pos);
                }
            } else
#endif
#if defined(SIXEL_USE_AVX)
            if (depth == 3 && simd_level >= SIXEL_SIMD_LEVEL_AVX) {
                acc256 = sixel_avx_zero_ps();

                for (x = x_first; x <= x_last; x++) {
                    diff_x = (dstw >= srcw)
                                 ? (x + 0.5) - center_x
                                 : (x + 0.5) * srcw / dstw - center_x;
                    weight = f_resample(fabs(diff_x));
                    pos = (y * srcw + x) * depth;
                    acc256 = sixel_avx_muladd_ps(
                        acc256,
                        sixel_avx_load_rgb_f32(src + pos),
                        (float)weight);
                    total += weight;
                }
                if (total > 0.0) {
                    pos = (y * dstw + w) * depth;
                    sixel_avx_store_rgb_f32(acc256, total, tmp + pos);
                }
            } else
#endif
#if defined(SIXEL_USE_SSE2) || defined(SIXEL_USE_NEON)
            if (depth == 3
# if defined(SIXEL_USE_SSE2)
                && simd_level >= SIXEL_SIMD_LEVEL_SSE2
# elif defined(SIXEL_USE_NEON)
                && simd_level >= SIXEL_SIMD_LEVEL_NEON
# endif
                ) {
#if defined(SIXEL_USE_SSE2)
                acc128 = _mm_setzero_ps();
                minv128 = _mm_set1_ps(0.0f);
                maxv128 = _mm_set1_ps(1.0f);
#elif defined(SIXEL_USE_NEON)
                acc_neon = vdupq_n_f32(0.0f);
                minv_neon = vdupq_n_f32(0.0f);
                maxv_neon = vdupq_n_f32(1.0f);
#endif
                for (x = x_first; x <= x_last; x++) {
                    diff_x = (dstw >= srcw)
                                 ? (x + 0.5) - center_x
                                 : (x + 0.5) * srcw / dstw - center_x;
                    weight = f_resample(fabs(diff_x));
                    pos = (y * srcw + x) * depth;
                    const float *psrc = src + pos;
#if defined(SIXEL_USE_SSE2)
                    pixf128 = _mm_set_ps(
                        0.0f, psrc[2], psrc[1], psrc[0]);
                    wv128 = _mm_set1_ps((float)weight);
                    acc128 = _mm_add_ps(acc128,
                                        _mm_mul_ps(pixf128, wv128));
#else /* NEON */
                    /*
                     * Expand the RGB triple into a NEON vector without
                     * brace initialization to keep older toolchains
                     * happy.
                     */
                    pixf_neon = vdupq_n_f32(0.0f);
                    pixf_neon = vsetq_lane_f32(psrc[0], pixf_neon, 0);
                    pixf_neon = vsetq_lane_f32(psrc[1], pixf_neon, 1);
                    pixf_neon = vsetq_lane_f32(psrc[2], pixf_neon, 2);
                    wv_neon = vdupq_n_f32((float)weight);
                    acc_neon = vmlaq_f32(acc_neon, pixf_neon, wv_neon);
#endif
                    total += weight;
                }
                if (total > 0.0) {
#if defined(SIXEL_USE_SSE2)
                    scalev128 = _mm_set1_ps((float)(1.0 / total));
                    acc128 = _mm_mul_ps(acc128, scalev128);
                    acc128 = _mm_max_ps(minv128,
                                        _mm_min_ps(acc128, maxv128));
                    _mm_storeu_ps(vecbuf, acc128);
#else /* NEON */
                    scalev_neon = vdupq_n_f32(
                        (float)(1.0 / total));
                    acc_neon = vmulq_f32(acc_neon, scalev_neon);
                    acc_neon = vmaxq_f32(minv_neon,
                                         vminq_f32(acc_neon, maxv_neon));
                    vst1q_f32(vecbuf, acc_neon);
#endif
                    pos = (y * dstw + w) * depth;
                    tmp[pos + 0] = vecbuf[0];
                    tmp[pos + 1] = vecbuf[1];
                    tmp[pos + 2] = vecbuf[2];
                }
            } else
#endif
            {
                for (x = x_first; x <= x_last; x++) {
                    diff_x = (dstw >= srcw)
                                 ? (x + 0.5) - center_x
                                 : (x + 0.5) * srcw / dstw - center_x;
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
                        tmp[pos] = sixel_clamp_unit_f32(
                            (float)(offsets[i] / total));
                    }
                }
            }
        }
    }

    for (h = 0; h < dsth; h++) {
        for (w = 0; w < dstw; w++) {
            total = 0.0;
            for (i = 0; i < depth; i++) {
                offsets[i] = 0.0;
            }

            if (dsth >= srch) {
                center_y = (h + 0.5) * srch / dsth;
                y_first = MAX((int)(center_y - n), 0);
                y_last = MIN((int)(center_y + n), srch - 1);
            } else {
                center_y = h + 0.5;
                y_first = MAX((int)floor((center_y - n) * srch / dsth), 0);
                y_last = MIN((int)floor((center_y + n) * srch / dsth),
                             srch - 1);
            }

#if defined(SIXEL_USE_AVX512)
            if (depth == 3 &&
                simd_level >= SIXEL_SIMD_LEVEL_AVX512) {
                acc512 = sixel_avx512_zero_ps();

                for (y = y_first; y <= y_last; y++) {
                    diff_y = (dsth >= srch)
                                 ? (y + 0.5) - center_y
                                 : (y + 0.5) * dsth / srch - center_y;
                    weight = f_resample(fabs(diff_y));
                    pos = (y * dstw + w) * depth;
                    pix512 = sixel_avx512_load_rgb_f32(tmp + pos);
                    acc512 = sixel_avx512_muladd_ps(
                        acc512,
                        pix512,
                        (float)weight);
                    total += weight;
                }
                if (total > 0.0) {
                    pos = (h * dstw + w) * depth;
                    sixel_avx512_store_rgb_f32(&acc512, total, dst + pos);
                }
            } else
#endif
#if defined(SIXEL_USE_AVX2)
            if (depth == 3 && simd_level >= SIXEL_SIMD_LEVEL_AVX2) {
                acc256 = sixel_avx2_zero_ps();

                for (y = y_first; y <= y_last; y++) {
                    diff_y = (dsth >= srch)
                                 ? (y + 0.5) - center_y
                                 : (y + 0.5) * dsth / srch - center_y;
                    weight = f_resample(fabs(diff_y));
                    pos = (y * dstw + w) * depth;
                    acc256 = sixel_avx2_muladd_ps(
                        acc256,
                        sixel_avx2_load_rgb_f32(tmp + pos),
                        (float)weight);
                    total += weight;
                }
                if (total > 0.0) {
                    pos = (h * dstw + w) * depth;
                    sixel_avx2_store_rgb_f32(acc256, total, dst + pos);
                }
            } else
#endif
#if defined(SIXEL_USE_AVX)
            if (depth == 3 && simd_level >= SIXEL_SIMD_LEVEL_AVX) {
                acc256 = sixel_avx_zero_ps();

                for (y = y_first; y <= y_last; y++) {
                    diff_y = (dsth >= srch)
                                 ? (y + 0.5) - center_y
                                 : (y + 0.5) * dsth / srch - center_y;
                    weight = f_resample(fabs(diff_y));
                    pos = (y * dstw + w) * depth;
                    acc256 = sixel_avx_muladd_ps(
                        acc256,
                        sixel_avx_load_rgb_f32(tmp + pos),
                        (float)weight);
                    total += weight;
                }
                if (total > 0.0) {
                    pos = (h * dstw + w) * depth;
                    sixel_avx_store_rgb_f32(acc256, total, dst + pos);
                }
            } else
#endif
#if defined(SIXEL_USE_SSE2) || defined(SIXEL_USE_NEON)
            if (depth == 3
# if defined(SIXEL_USE_SSE2)
                && simd_level >= SIXEL_SIMD_LEVEL_SSE2
# elif defined(SIXEL_USE_NEON)
                && simd_level >= SIXEL_SIMD_LEVEL_NEON
# endif
                ) {
#if defined(SIXEL_USE_SSE2)
                acc128 = _mm_setzero_ps();
                minv128 = _mm_set1_ps(0.0f);
                maxv128 = _mm_set1_ps(1.0f);
#elif defined(SIXEL_USE_NEON)
                acc_neon = vdupq_n_f32(0.0f);
                minv_neon = vdupq_n_f32(0.0f);
                maxv_neon = vdupq_n_f32(1.0f);
#endif
                for (y = y_first; y <= y_last; y++) {
                    diff_y = (dsth >= srch)
                                 ? (y + 0.5) - center_y
                                 : (y + 0.5) * dsth / srch - center_y;
                    weight = f_resample(fabs(diff_y));
                    pos = (y * dstw + w) * depth;
                    const float *psrc = tmp + pos;
#if defined(SIXEL_USE_SSE2)
                    pixf128 = _mm_set_ps(
                        0.0f, psrc[2], psrc[1], psrc[0]);
                    wv128 = _mm_set1_ps((float)weight);
                    acc128 = _mm_add_ps(acc128,
                                        _mm_mul_ps(pixf128, wv128));
#else /* NEON */
                    /*
                     * Expand the RGB triple into a NEON vector without
                     * brace initialization to keep older toolchains
                     * happy.
                     */
                    pixf_neon = vdupq_n_f32(0.0f);
                    pixf_neon = vsetq_lane_f32(psrc[0], pixf_neon, 0);
                    pixf_neon = vsetq_lane_f32(psrc[1], pixf_neon, 1);
                    pixf_neon = vsetq_lane_f32(psrc[2], pixf_neon, 2);
                    wv_neon = vdupq_n_f32((float)weight);
                    acc_neon = vmlaq_f32(acc_neon, pixf_neon, wv_neon);
#endif
                    total += weight;
                }
                if (total > 0.0) {
#if defined(SIXEL_USE_SSE2)
                    scalev128 = _mm_set1_ps((float)(1.0 / total));
                    acc128 = _mm_mul_ps(acc128, scalev128);
                    acc128 = _mm_max_ps(minv128,
                                        _mm_min_ps(acc128, maxv128));
                    _mm_storeu_ps(vecbuf, acc128);
#else /* NEON */
                    scalev_neon = vdupq_n_f32(
                        (float)(1.0 / total));
                    acc_neon = vmulq_f32(acc_neon, scalev_neon);
                    acc_neon = vmaxq_f32(minv_neon,
                                         vminq_f32(acc_neon, maxv_neon));
                    vst1q_f32(vecbuf, acc_neon);
#endif
                    pos = (h * dstw + w) * depth;
                    dst[pos + 0] = vecbuf[0];
                    dst[pos + 1] = vecbuf[1];
                    dst[pos + 2] = vecbuf[2];
                }
            } else
#endif
            {
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
                        dst[pos] = sixel_clamp_unit_f32(
                            (float)(offsets[i] / total));
                    }
                }
            }
        }
    }

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

SIXELAPI int
sixel_helper_scale_image_float32(
    float             /* out */ *dst,
    float const       /* in */  *src,
    int               /* in */  srcw,
    int               /* in */  srch,
    int               /* in */  pixelformat,
    int               /* in */  dstw,
    int               /* in */  dsth,
    int               /* in */  method_for_resampling,
    sixel_allocator_t /* in */  *allocator)
{
    int depth;
    int depth_bytes;

    depth_bytes = sixel_helper_compute_depth(pixelformat);
    if (depth_bytes <= 0) {
        return (-1);
    }

    depth = depth_bytes / (int)sizeof(float);
    if (depth * (int)sizeof(float) != depth_bytes) {
        return (-1);
    }

    switch (method_for_resampling) {
    case SIXEL_RES_NEAREST:
        scale_without_resampling_float32(
            dst, src, srcw, srch, dstw, dsth, depth);
        break;
    case SIXEL_RES_GAUSSIAN:
        scale_with_resampling_float32(
            dst, src, srcw, srch, dstw, dsth, depth,
            gaussian, 1.0, allocator);
        break;
    case SIXEL_RES_HANNING:
        scale_with_resampling_float32(
            dst, src, srcw, srch, dstw, dsth, depth,
            hanning, 1.0, allocator);
        break;
    case SIXEL_RES_HAMMING:
        scale_with_resampling_float32(
            dst, src, srcw, srch, dstw, dsth, depth,
            hamming, 1.0, allocator);
        break;
    case SIXEL_RES_WELSH:
        scale_with_resampling_float32(
            dst, src, srcw, srch, dstw, dsth, depth,
            welsh, 1.0, allocator);
        break;
    case SIXEL_RES_BICUBIC:
        scale_with_resampling_float32(
            dst, src, srcw, srch, dstw, dsth, depth,
            bicubic, 2.0, allocator);
        break;
    case SIXEL_RES_LANCZOS2:
        scale_with_resampling_float32(
            dst, src, srcw, srch, dstw, dsth, depth,
            lanczos2, 2.0, allocator);
        break;
    case SIXEL_RES_LANCZOS3:
        scale_with_resampling_float32(
            dst, src, srcw, srch, dstw, dsth, depth,
            lanczos3, 3.0, allocator);
        break;
    case SIXEL_RES_LANCZOS4:
        scale_with_resampling_float32(
            dst, src, srcw, srch, dstw, dsth, depth,
            lanczos4, 4.0, allocator);
        break;
    case SIXEL_RES_BILINEAR:
    default:
        scale_with_resampling_float32(
            dst, src, srcw, srch, dstw, dsth, depth,
            bilinear, 1.0, allocator);
        break;
    }

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

#if defined(__GNUC__) && !defined(__clang__)
# pragma GCC diagnostic pop
#endif

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
