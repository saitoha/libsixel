/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
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

#include "config.h"

#include <math.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

#include <sixel.h>

#include "colorspace.h"
#include "cpu.h"
#include "logger.h"
#include "threading.h"
#if SIXEL_ENABLE_THREADS
# include "threadpool.h"
#endif
#include "compat_stub.h"

#if defined(HAVE_IMMINTRIN_H) && \
    (defined(__x86_64__) || defined(_M_X64) || defined(__i386) || \
     defined(_M_IX86))
# define SIXEL_HAS_X86_INTRIN 1
# include <immintrin.h>
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
#   define SIXEL_TARGET_AVX2 __attribute__((target("avx2")))
#   define SIXEL_TARGET_AVX512 \
        __attribute__((target("avx512f,avx512bw")))
#   define SIXEL_USE_AVX2 1
#   define SIXEL_USE_AVX512 1
#  else
/*
 * clang rejects returning AVX vectors when the translation unit target
 * does not already include the corresponding ISA. Guard runtime AVX
 * helpers with compile-time ISA availability to keep non-AVX builds
 * warning-free while still using AVX when the compiler enables it.
 */
#   define SIXEL_TARGET_AVX2
#   define SIXEL_TARGET_AVX512
#   if defined(__AVX2__)
#    define SIXEL_USE_AVX2 1
#   endif
#   if defined(__AVX512F__) && defined(__AVX512BW__)
#    define SIXEL_USE_AVX512 1
#   endif
#  endif
# else
#  define SIXEL_TARGET_AVX2
#  define SIXEL_TARGET_AVX512
#  if defined(__AVX2__)
#   define SIXEL_USE_AVX2 1
#  endif
#  if defined(__AVX512F__) && defined(__AVX512BW__)
#   define SIXEL_USE_AVX512 1
#  endif
# endif
#endif

#if defined(HAVE_NEON)
# if (defined(__ARM_NEON) || defined(__ARM_NEON__))
#  if defined(HAVE_ARM_NEON_H)
#   include <arm_neon.h>
#   define SIXEL_USE_NEON 1
#  endif
# endif
#endif

#define SIXEL_COLORSPACE_LUT_SIZE 256
#define SIXEL_OKLAB_AB_OFFSET 0.5
#define SIXEL_OKLAB_AB_SCALE  255.0
#define SIXEL_CIELAB_AB_SCALE 128.0
#define SIXEL_CIELAB_L_SCALE  100.0
#define SIXEL_CIELAB_AB_LIMIT 1.5
#define SIXEL_DIN99D_L_SCALE  100.0
#define SIXEL_DIN99D_AB_RANGE 50.0
#define SIXEL_YUV_U_MAX  0.436
#define SIXEL_YUV_V_MAX  0.615

#if defined(__FMA__)
# define SIXEL_FMADD_PS256(a, b, c) _mm256_fmadd_ps((a), (b), (c))
# define SIXEL_FMADD_PS512(a, b, c) _mm512_fmadd_ps((a), (b), (c))
#else
# define SIXEL_FMADD_PS256(a, b, c) \
    _mm256_add_ps(_mm256_mul_ps((a), (b)), (c))
# define SIXEL_FMADD_PS512(a, b, c) \
    _mm512_add_ps(_mm512_mul_ps((a), (b)), (c))
#endif

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

#if (defined(SIXEL_USE_AVX512) && defined(__AVX512F__) && \
        defined(__AVX512BW__)) || \
        (defined(SIXEL_USE_AVX2) && defined(__AVX2__)) || \
        defined(SIXEL_USE_SSE2) || defined(SIXEL_USE_NEON)
static const float sixel_linear_srgb_to_smptec_matrix_f32[3][3] = {
    { 1.0651945f, -0.05539145f, -0.009975616f },
    { -0.019633066f, 1.0363870f, -0.016731962f },
    { 0.0016324889f, 0.0044134663f, 0.99419266f }
};

static const float sixel_linear_smptec_to_srgb_matrix_f32[3][3] = {
    { 0.93970484f, 0.050180361f, 0.010273410f },
    { 0.017775363f, 0.96577054f, 0.016431980f },
    { -0.0016219272f, -0.0043696986f, 1.0057515f }
};
#endif

#if defined(SIXEL_USE_NEON)
static uint8x16x4_t sixel_neon_gamma_to_linear[4];
static uint8x16x4_t sixel_neon_linear_to_gamma[4];
static int sixel_neon_tables_initialized = 0;
#endif

#if (defined(SIXEL_USE_AVX2) && defined(__AVX2__)) || \
        (defined(SIXEL_USE_AVX512) && defined(__AVX512F__) && \
         defined(__AVX512BW__))
static uint32_t sixel_avx_gamma_to_linear_lut32[SIXEL_COLORSPACE_LUT_SIZE];
static uint32_t sixel_avx_linear_to_gamma_lut32[SIXEL_COLORSPACE_LUT_SIZE];
static int sixel_avx_tables_initialized = 0;
static SIXELSTATUS sixel_colorspace_convert_avx512(unsigned char *pixels,
                                                   size_t size,
                                                   int pixelformat,
                                                   int colorspace_src,
                                                   int colorspace_dst);
static SIXELSTATUS sixel_colorspace_convert_avx2(unsigned char *pixels,
                                                 size_t size,
                                                 int pixelformat,
                                                 int colorspace_src,
                                                 int colorspace_dst);
#elif defined(SIXEL_USE_AVX2)
static __attribute__((unused)) SIXELSTATUS
sixel_colorspace_convert_avx2(unsigned char *pixels,
                              size_t size,
                              int pixelformat,
                              int colorspace_src,
                              int colorspace_dst)
{
    (void)pixels;
    (void)size;
    (void)pixelformat;
    (void)colorspace_src;
    (void)colorspace_dst;

    return SIXEL_BAD_INPUT;
}
#endif

#if defined(SIXEL_USE_AVX512) && \
        !(defined(__AVX512F__) && defined(__AVX512BW__))
static __attribute__((unused)) SIXELSTATUS
sixel_colorspace_convert_avx512(unsigned char *pixels,
                                size_t size,
                                int pixelformat,
                                int colorspace_src,
                                int colorspace_dst)
{
    (void)pixels;
    (void)size;
    (void)pixelformat;
    (void)colorspace_src;
    (void)colorspace_dst;

    return SIXEL_BAD_INPUT;
}
#endif

#if defined(SIXEL_USE_SSE2)
static SIXELSTATUS sixel_colorspace_convert_sse2(unsigned char *pixels,
                                                 size_t size,
                                                 int pixelformat,
                                                 int colorspace_src,
                                                 int colorspace_dst);
#endif

/*
 * Lookup tables keep the per-pixel pow() calls out of the byte conversion
 * hot paths.  The double variants carry normalised values for the general
 * decoder while the byte versions serve the SIMD accelerators.
 */
static unsigned char gamma_to_linear_lut[SIXEL_COLORSPACE_LUT_SIZE];
static unsigned char linear_to_gamma_lut[SIXEL_COLORSPACE_LUT_SIZE];
static double gamma_to_linear_double_lut[SIXEL_COLORSPACE_LUT_SIZE];
static double smptec_to_linear_double_lut[SIXEL_COLORSPACE_LUT_SIZE];
static unsigned char linear_to_smptec_lut[SIXEL_COLORSPACE_LUT_SIZE];
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

static inline double
sixel_srgb_unit_to_linear(double value)
{
    double x;

    x = sixel_clamp_unit(value);
    if (x <= 0.04045) {
        return x / 12.92;
    }

    return pow((x + 0.055) / 1.055, 2.4);
}

static inline double
sixel_linear_to_srgb_unit(double value)
{
    double y;

    if (value <= 0.0) {
        return 0.0;
    }
    if (value >= 1.0) {
        return 1.0;
    }

    if (value <= 0.0031308) {
        y = value * 12.92;
    } else {
        y = 1.055 * pow(value, 1.0 / 2.4) - 0.055;
    }

    return sixel_clamp_unit(y);
}

static inline double
sixel_smptec_unit_to_linear(double value)
{
    double x;

    x = sixel_clamp_unit(value);
    if (x <= 0.0) {
        return 0.0;
    }
    if (x >= 1.0) {
        return 1.0;
    }

    return pow(x, 2.2);
}

static inline double
sixel_linear_to_smptec_unit(double value)
{
    double y;

    if (value <= 0.0) {
        return 0.0;
    }
    if (value >= 1.0) {
        return 1.0;
    }

    y = pow(value, 1.0 / 2.2);
    return sixel_clamp_unit(y);
}

static inline double
sixel_oklab_clamp_ab(double value)
{
    double lower;
    double upper;

    lower = -SIXEL_OKLAB_AB_OFFSET;
    upper = SIXEL_OKLAB_AB_OFFSET;
    if (value < lower) {
        return lower;
    }
    if (value > upper) {
        return upper;
    }

    return value;
}

static inline double
sixel_cielab_clamp_ab(double value)
{
    if (value < -SIXEL_CIELAB_AB_LIMIT) {
        return -SIXEL_CIELAB_AB_LIMIT;
    }
    if (value > SIXEL_CIELAB_AB_LIMIT) {
        return SIXEL_CIELAB_AB_LIMIT;
    }

    return value;
}

static inline double
sixel_din99d_clamp_ab_norm(double value)
{
    if (value < -1.0) {
        return -1.0;
    }
    if (value > 1.0) {
        return 1.0;
    }

    return value;
}

static inline double
sixel_yuv_clamp_u(double value)
{
    if (value < -SIXEL_YUV_U_MAX) {
        return -SIXEL_YUV_U_MAX;
    }
    if (value > SIXEL_YUV_U_MAX) {
        return SIXEL_YUV_U_MAX;
    }

    return value;
}

static inline double
sixel_yuv_clamp_v(double value)
{
    if (value < -SIXEL_YUV_V_MAX) {
        return -SIXEL_YUV_V_MAX;
    }
    if (value > SIXEL_YUV_V_MAX) {
        return SIXEL_YUV_V_MAX;
    }

    return value;
}

#if 0
static inline double
sixel_din99d_clamp_ab(double value)
{
    if (value < -SIXEL_DIN99D_AB_RANGE) {
        return -SIXEL_DIN99D_AB_RANGE;
    }
    if (value > SIXEL_DIN99D_AB_RANGE) {
        return SIXEL_DIN99D_AB_RANGE;
    }

    return value;
}
#endif

#if defined(SIXEL_USE_NEON)
/*
 * SIMD lookup helpers accelerate the gamma/linear LUT path on NEON.
 * A four-way 64-entry table maps the 256 entry LUT without branches.
 */
static void
sixel_colorspace_fill_neon_table(uint8x16x4_t *table,
                                 const unsigned char *source)
{
    int index;

    for (index = 0; index < 4; ++index) {
        const unsigned char *chunk = source + (index * 16);

        table->val[index] = vld1q_u8(chunk);
    }
}

static void
sixel_colorspace_prepare_neon_tables(void)
{
    int block;

    if (sixel_neon_tables_initialized) {
        return;
    }

    for (block = 0; block < 4; ++block) {
        const unsigned char *gamma_src;
        const unsigned char *linear_src;

        gamma_src = gamma_to_linear_lut + (block * 64);
        linear_src = linear_to_gamma_lut + (block * 64);

        sixel_colorspace_fill_neon_table(
            &sixel_neon_gamma_to_linear[block],
            gamma_src);
        sixel_colorspace_fill_neon_table(
            &sixel_neon_linear_to_gamma[block],
            linear_src);
    }

    sixel_neon_tables_initialized = 1;
}

static inline uint8x16_t
sixel_colorspace_lookup_neon(uint8x16x4_t *table, uint8x16_t index)
{
    uint8x16_t block;
    uint8x16_t block_mask;
    uint8x16_t local_index;
    uint8x16_t result;
    uint8x16_t selection;

    block = vshrq_n_u8(index, 6);
    local_index = vandq_u8(index, vdupq_n_u8(0x3f));

    result = vdupq_n_u8(0);
    selection = vqtbl4q_u8(table[0], local_index);
    block_mask = vceqq_u8(block, vdupq_n_u8(0));
    result = vbslq_u8(block_mask, selection, result);

    selection = vqtbl4q_u8(table[1], local_index);
    block_mask = vceqq_u8(block, vdupq_n_u8(1));
    result = vbslq_u8(block_mask, selection, result);

    selection = vqtbl4q_u8(table[2], local_index);
    block_mask = vceqq_u8(block, vdupq_n_u8(2));
    result = vbslq_u8(block_mask, selection, result);

    selection = vqtbl4q_u8(table[3], local_index);
    block_mask = vceqq_u8(block, vdupq_n_u8(3));
    result = vbslq_u8(block_mask, selection, result);

    return result;
}

static inline uint8x16_t
sixel_colorspace_alpha_mask_neon(int pixelformat)
{
    static const uint8_t mask_rgba[16] = {
        0, 0, 0, 255, 0, 0, 0, 255,
        0, 0, 0, 255, 0, 0, 0, 255
    };
    static const uint8_t mask_bgra[16] = {
        0, 0, 0, 255, 0, 0, 0, 255,
        0, 0, 0, 255, 0, 0, 0, 255
    };
    static const uint8_t mask_argb[16] = {
        255, 0, 0, 0, 255, 0, 0, 0,
        255, 0, 0, 0, 255, 0, 0, 0
    };
    static const uint8_t mask_abgr[16] = {
        255, 0, 0, 0, 255, 0, 0, 0,
        255, 0, 0, 0, 255, 0, 0, 0
    };
    static const uint8_t mask_ga[16] = {
        0, 255, 0, 255, 0, 255, 0, 255,
        0, 255, 0, 255, 0, 255, 0, 255
    };
    static const uint8_t mask_ag[16] = {
        255, 0, 255, 0, 255, 0, 255, 0,
        255, 0, 255, 0, 255, 0, 255, 0
    };

    switch (pixelformat) {
    case SIXEL_PIXELFORMAT_RGBA8888:
        return vld1q_u8(mask_rgba);
    case SIXEL_PIXELFORMAT_BGRA8888:
        return vld1q_u8(mask_bgra);
    case SIXEL_PIXELFORMAT_ARGB8888:
        return vld1q_u8(mask_argb);
    case SIXEL_PIXELFORMAT_ABGR8888:
        return vld1q_u8(mask_abgr);
    case SIXEL_PIXELFORMAT_GA88:
        return vld1q_u8(mask_ga);
    case SIXEL_PIXELFORMAT_AG88:
        return vld1q_u8(mask_ag);
    default:
        return vdupq_n_u8(0);
    }
}

static void
sixel_colorspace_apply_neon(unsigned char *pixels,
                            size_t size,
                            int pixelformat,
                            const unsigned char *lut)
{
    uint8x16x4_t *table;
    uint8x16_t mask;
    size_t offset;
    size_t remaining;
    uint8_t mask_buffer[16];

    sixel_colorspace_prepare_neon_tables();

    if (lut == gamma_to_linear_lut) {
        table = sixel_neon_gamma_to_linear;
    } else {
        table = sixel_neon_linear_to_gamma;
    }

    mask = sixel_colorspace_alpha_mask_neon(pixelformat);
    vst1q_u8(mask_buffer, mask);

    offset = 0;
    remaining = size;
    while (remaining >= 16U) {
        uint8x16_t input = vld1q_u8(pixels + offset);
        uint8x16_t converted;
        uint8x16_t preserved;

        converted = sixel_colorspace_lookup_neon(table, input);
        preserved = vbslq_u8(mask, input, converted);
        vst1q_u8(pixels + offset, preserved);

        offset += 16U;
        remaining -= 16U;
    }

    while (remaining > 0U) {
        unsigned char original;
        unsigned char mapped;
        size_t mask_index;

        mask_index = offset % 16U;
        original = pixels[offset];
        mapped = lut[original];
        if (mask_buffer[mask_index] == 0U) {
            pixels[offset] = mapped;
        }

        ++offset;
        --remaining;
    }
}

#endif

/*
 * SIMD kernels share this LUT selector.  MinGW builds without SIMD support
 * do not reference it, so we hide the definition unless a SIMD path is
 * compiled in to avoid -Werror=unused-function.
 */
#if defined(SIXEL_USE_NEON) || defined(SIXEL_USE_SSE2) || \
        (defined(SIXEL_USE_AVX2) && defined(__AVX2__)) || \
        (defined(SIXEL_USE_AVX512) && defined(__AVX512F__) && \
         defined(__AVX512BW__))
static const unsigned char *
sixel_colorspace_select_lut(int colorspace_src, int colorspace_dst)
{
    if (colorspace_src == SIXEL_COLORSPACE_GAMMA &&
            colorspace_dst == SIXEL_COLORSPACE_LINEAR) {
        return gamma_to_linear_lut;
    }

    if (colorspace_src == SIXEL_COLORSPACE_LINEAR &&
            colorspace_dst == SIXEL_COLORSPACE_GAMMA) {
        return linear_to_gamma_lut;
    }

    return NULL;
}
#endif

#if (defined(SIXEL_USE_AVX2) && defined(__AVX2__)) || \
        (defined(SIXEL_USE_AVX512) && defined(__AVX512F__) && \
         defined(__AVX512BW__))
static inline const uint32_t *
sixel_colorspace_select_lut32(int colorspace_src, int colorspace_dst)
{
    if (colorspace_src == SIXEL_COLORSPACE_GAMMA &&
            colorspace_dst == SIXEL_COLORSPACE_LINEAR) {
        return sixel_avx_gamma_to_linear_lut32;
    }

    if (colorspace_src == SIXEL_COLORSPACE_LINEAR &&
            colorspace_dst == SIXEL_COLORSPACE_GAMMA) {
        return sixel_avx_linear_to_gamma_lut32;
    }

    return NULL;
}

#endif

#if defined(SIXEL_USE_SSE2)
static inline __m128i
sixel_colorspace_alpha_mask_sse2(int pixelformat)
{
    static const uint8_t mask_rgba[16] = {
        0, 0, 0, 255, 0, 0, 0, 255,
        0, 0, 0, 255, 0, 0, 0, 255
    };
    static const uint8_t mask_bgra[16] = {
        0, 0, 0, 255, 0, 0, 0, 255,
        0, 0, 0, 255, 0, 0, 0, 255
    };
    static const uint8_t mask_argb[16] = {
        255, 0, 0, 0, 255, 0, 0, 0,
        255, 0, 0, 0, 255, 0, 0, 0
    };
    static const uint8_t mask_abgr[16] = {
        255, 0, 0, 0, 255, 0, 0, 0,
        255, 0, 0, 0, 255, 0, 0, 0
    };
    static const uint8_t mask_ga[16] = {
        0, 255, 0, 255, 0, 255, 0, 255,
        0, 255, 0, 255, 0, 255, 0, 255
    };
    static const uint8_t mask_ag[16] = {
        255, 0, 255, 0, 255, 0, 255, 0,
        255, 0, 255, 0, 255, 0, 255, 0
    };

    switch (pixelformat) {
    case SIXEL_PIXELFORMAT_RGBA8888:
        return _mm_loadu_si128((const __m128i *)mask_rgba);
    case SIXEL_PIXELFORMAT_BGRA8888:
        return _mm_loadu_si128((const __m128i *)mask_bgra);
    case SIXEL_PIXELFORMAT_ARGB8888:
        return _mm_loadu_si128((const __m128i *)mask_argb);
    case SIXEL_PIXELFORMAT_ABGR8888:
        return _mm_loadu_si128((const __m128i *)mask_abgr);
    case SIXEL_PIXELFORMAT_GA88:
        return _mm_loadu_si128((const __m128i *)mask_ga);
    case SIXEL_PIXELFORMAT_AG88:
        return _mm_loadu_si128((const __m128i *)mask_ag);
    default:
        return _mm_setzero_si128();
    }
}

/*
 * SSE2 fallback that still relies on the LUT but performs masking in
 * vectors so alpha bytes are kept intact. The lookup itself expands a
 * 16-byte chunk to a temporary buffer to avoid SSSE3 pshufb usage.
 */
static void
sixel_colorspace_apply_sse2(unsigned char *pixels,
                            size_t size,
                            int pixelformat,
                            const unsigned char *lut)
{
    __m128i mask128;
    size_t offset;
    size_t remaining;
    uint8_t mask_buffer[16];
    unsigned char input_bytes[16];
    unsigned char mapped_bytes[16];
    int j;

    mask128 = sixel_colorspace_alpha_mask_sse2(pixelformat);
    _mm_storeu_si128((__m128i *)mask_buffer, mask128);

    offset = 0U;
    remaining = size;
    while (remaining >= 16U) {
        __m128i input;
        __m128i mapped;
        __m128i preserved;

        input = _mm_loadu_si128((const __m128i *)(pixels + offset));
        _mm_storeu_si128((__m128i *)input_bytes, input);

        for (j = 0; j < 16; ++j) {
            mapped_bytes[j] = lut[input_bytes[j]];
        }

        mapped = _mm_loadu_si128((const __m128i *)mapped_bytes);
        preserved = _mm_or_si128(_mm_and_si128(mask128, input),
                                 _mm_andnot_si128(mask128, mapped));

        _mm_storeu_si128((__m128i *)(pixels + offset), preserved);

        offset += 16U;
        remaining -= 16U;
    }

    while (remaining > 0U) {
        unsigned char original;
        unsigned char mapped_scalar;
        size_t mask_index;

        mask_index = offset % 16U;
        original = pixels[offset];
        mapped_scalar = lut[original];
        if (mask_buffer[mask_index] == 0U) {
            pixels[offset] = mapped_scalar;
        }

        ++offset;
        --remaining;
    }
}

static SIXELSTATUS
sixel_colorspace_convert_sse2(unsigned char *pixels,
                              size_t size,
                              int pixelformat,
                              int colorspace_src,
                              int colorspace_dst)
{
    const unsigned char *lut;

    lut = sixel_colorspace_select_lut(colorspace_src, colorspace_dst);
    if (lut == NULL) {
        return SIXEL_BAD_INPUT;
    }

    switch (pixelformat) {
    case SIXEL_PIXELFORMAT_RGB888:
    case SIXEL_PIXELFORMAT_BGR888:
        if (size % 3U != 0U) {
            return SIXEL_BAD_INPUT;
        }
        sixel_colorspace_apply_sse2(pixels, size, pixelformat, lut);
        return SIXEL_OK;
    case SIXEL_PIXELFORMAT_RGBA8888:
    case SIXEL_PIXELFORMAT_BGRA8888:
    case SIXEL_PIXELFORMAT_ARGB8888:
    case SIXEL_PIXELFORMAT_ABGR8888:
        if (size % 4U != 0U) {
            return SIXEL_BAD_INPUT;
        }
        sixel_colorspace_apply_sse2(pixels, size, pixelformat, lut);
        return SIXEL_OK;
    case SIXEL_PIXELFORMAT_G8:
        sixel_colorspace_apply_sse2(pixels, size, pixelformat, lut);
        return SIXEL_OK;
    case SIXEL_PIXELFORMAT_GA88:
    case SIXEL_PIXELFORMAT_AG88:
        if (size % 2U != 0U) {
            return SIXEL_BAD_INPUT;
        }
        sixel_colorspace_apply_sse2(pixels, size, pixelformat, lut);
        return SIXEL_OK;
    default:
        break;
    }

    return SIXEL_BAD_INPUT;
}
#endif

#if defined(SIXEL_USE_AVX512) && defined(__AVX512F__) && \
        defined(__AVX512BW__)
/*
 * AVX512 path widens the LUT gather to 16 dword lanes, letting us reuse
 * the AVX2 tables while masking alpha bytes with SSE2 logic.
 */
static SIXEL_TARGET_AVX512 void
sixel_colorspace_apply_avx512(unsigned char *pixels,
                              size_t size,
                              int pixelformat,
                              const unsigned char *lut,
                              const uint32_t *lut32)
{
    __m128i mask128;
    size_t offset;
    size_t remaining;
    uint8_t mask_buffer[16];

    mask128 = sixel_colorspace_alpha_mask_sse2(pixelformat);
    _mm_storeu_si128((__m128i *)mask_buffer, mask128);

    offset = 0U;
    remaining = size;
    while (remaining >= 16U) {
        __m128i input;
        __m512i indices;
        __m512i mapped32;
        __m128i converted;
        __m128i preserved;

        input = _mm_loadu_si128((const __m128i *)(pixels + offset));
        indices = _mm512_cvtepu8_epi32(input);
        mapped32 = _mm512_i32gather_epi32(indices, (const int *)lut32, 4);
        converted = _mm512_cvtepi32_epi8(mapped32);

        preserved = _mm_or_si128(_mm_and_si128(mask128, input),
                                 _mm_andnot_si128(mask128, converted));

        _mm_storeu_si128((__m128i *)(pixels + offset), preserved);

        offset += 16U;
        remaining -= 16U;
    }

    while (remaining > 0U) {
        unsigned char original;
        unsigned char mapped;
        size_t mask_index;

        mask_index = offset % 16U;
        original = pixels[offset];
        mapped = lut[original];
        if (mask_buffer[mask_index] == 0U) {
            pixels[offset] = mapped;
        }

        ++offset;
        --remaining;
    }
}

static SIXEL_TARGET_AVX512 SIXELSTATUS
sixel_colorspace_convert_avx512(unsigned char *pixels,
                                size_t size,
                                int pixelformat,
                                int colorspace_src,
                                int colorspace_dst)
{
    const unsigned char *lut;
    const uint32_t *lut32;

    lut = sixel_colorspace_select_lut(colorspace_src, colorspace_dst);
    lut32 = sixel_colorspace_select_lut32(colorspace_src, colorspace_dst);
    if (lut == NULL || lut32 == NULL) {
        return SIXEL_BAD_INPUT;
    }

    switch (pixelformat) {
    case SIXEL_PIXELFORMAT_RGB888:
    case SIXEL_PIXELFORMAT_BGR888:
        if (size % 3U != 0U) {
            return SIXEL_BAD_INPUT;
        }
        sixel_colorspace_apply_avx512(pixels, size, pixelformat, lut, lut32);
        return SIXEL_OK;
    case SIXEL_PIXELFORMAT_RGBA8888:
    case SIXEL_PIXELFORMAT_BGRA8888:
    case SIXEL_PIXELFORMAT_ARGB8888:
    case SIXEL_PIXELFORMAT_ABGR8888:
        if (size % 4U != 0U) {
            return SIXEL_BAD_INPUT;
        }
        sixel_colorspace_apply_avx512(pixels, size, pixelformat, lut, lut32);
        return SIXEL_OK;
    case SIXEL_PIXELFORMAT_G8:
        sixel_colorspace_apply_avx512(pixels, size, pixelformat, lut, lut32);
        return SIXEL_OK;
    case SIXEL_PIXELFORMAT_GA88:
    case SIXEL_PIXELFORMAT_AG88:
        if (size % 2U != 0U) {
            return SIXEL_BAD_INPUT;
        }
        sixel_colorspace_apply_avx512(pixels, size, pixelformat, lut, lut32);
        return SIXEL_OK;
    default:
        break;
    }

    return SIXEL_BAD_INPUT;
}
#endif

#if defined(SIXEL_USE_AVX2) && defined(__AVX2__)

typedef struct sixel_colorspace_layout {
    int step;
    int index_r;
    int index_g;
    int index_b;
} sixel_colorspace_layout_t;

static void
sixel_colorspace_layout_pick(int pixelformat,
                             sixel_colorspace_layout_t *layout)
{
    switch (pixelformat) {
    case SIXEL_PIXELFORMAT_RGB888:
        layout->step = 3;
        layout->index_r = 0;
        layout->index_g = 1;
        layout->index_b = 2;
        break;
    case SIXEL_PIXELFORMAT_RGBA8888:
        layout->step = 4;
        layout->index_r = 0;
        layout->index_g = 1;
        layout->index_b = 2;
        break;
    case SIXEL_PIXELFORMAT_BGR888:
        layout->step = 3;
        layout->index_r = 2;
        layout->index_g = 1;
        layout->index_b = 0;
        break;
    case SIXEL_PIXELFORMAT_BGRA8888:
        layout->step = 4;
        layout->index_r = 2;
        layout->index_g = 1;
        layout->index_b = 0;
        break;
    case SIXEL_PIXELFORMAT_ARGB8888:
        layout->step = 4;
        layout->index_r = 1;
        layout->index_g = 2;
        layout->index_b = 3;
        break;
    case SIXEL_PIXELFORMAT_ABGR8888:
        layout->step = 4;
        layout->index_r = 3;
        layout->index_g = 2;
        layout->index_b = 1;
        break;
    default:
        layout->step = 0;
        layout->index_r = 0;
        layout->index_g = 0;
        layout->index_b = 0;
        break;
    }
}

static inline __m256
sixel_colorspace_clamp_unit_avx(__m256 value)
{
    __m256 zero;
    __m256 one;

    zero = _mm256_set1_ps(0.0f);
    one = _mm256_set1_ps(1.0f);

    value = _mm256_max_ps(value, zero);
    value = _mm256_min_ps(value, one);

    return value;
}

static inline __m256
sixel_colorspace_clamp_u_avx(__m256 value)
{
    __m256 min_u;
    __m256 max_u;

    min_u = _mm256_set1_ps((float)(-SIXEL_YUV_U_MAX));
    max_u = _mm256_set1_ps((float)SIXEL_YUV_U_MAX);

    value = _mm256_max_ps(value, min_u);
    value = _mm256_min_ps(value, max_u);

    return value;
}

static inline __m256
sixel_colorspace_clamp_v_avx(__m256 value)
{
    __m256 min_v;
    __m256 max_v;

    min_v = _mm256_set1_ps((float)(-SIXEL_YUV_V_MAX));
    max_v = _mm256_set1_ps((float)SIXEL_YUV_V_MAX);

    value = _mm256_max_ps(value, min_v);
    value = _mm256_min_ps(value, max_v);

    return value;
}

static inline __m256i
sixel_colorspace_pack_linear_bytes_avx(__m256 value)
{
    __m256 scaled;
    __m256i wide;
    __m256i packed16;
    __m256i packed8;

    scaled = _mm256_mul_ps(value, _mm256_set1_ps(255.0f));
    wide = _mm256_cvtps_epi32(scaled);

    packed16 = _mm256_packs_epi32(wide, wide);
    packed8 = _mm256_packus_epi16(packed16, packed16);

    return packed8;
}

static inline __m256i
sixel_colorspace_pack_gamma_bytes_avx(__m256 value)
{
    __m256i linear_bytes;
    __m128i mapped_low;
    __m128i mapped_high;
    alignas(32) unsigned char buffer[32];
    __m256i packed;

    linear_bytes = sixel_colorspace_pack_linear_bytes_avx(value);

    _mm256_store_si256((__m256i *)buffer, linear_bytes);

    mapped_low = _mm_set_epi8(
        linear_to_gamma_lut[buffer[15]],
        linear_to_gamma_lut[buffer[14]],
        linear_to_gamma_lut[buffer[13]],
        linear_to_gamma_lut[buffer[12]],
        linear_to_gamma_lut[buffer[11]],
        linear_to_gamma_lut[buffer[10]],
        linear_to_gamma_lut[buffer[9]],
        linear_to_gamma_lut[buffer[8]],
        linear_to_gamma_lut[buffer[7]],
        linear_to_gamma_lut[buffer[6]],
        linear_to_gamma_lut[buffer[5]],
        linear_to_gamma_lut[buffer[4]],
        linear_to_gamma_lut[buffer[3]],
        linear_to_gamma_lut[buffer[2]],
        linear_to_gamma_lut[buffer[1]],
        linear_to_gamma_lut[buffer[0]]);

    mapped_high = _mm_set_epi8(
        linear_to_gamma_lut[buffer[31]],
        linear_to_gamma_lut[buffer[30]],
        linear_to_gamma_lut[buffer[29]],
        linear_to_gamma_lut[buffer[28]],
        linear_to_gamma_lut[buffer[27]],
        linear_to_gamma_lut[buffer[26]],
        linear_to_gamma_lut[buffer[25]],
        linear_to_gamma_lut[buffer[24]],
        linear_to_gamma_lut[buffer[23]],
        linear_to_gamma_lut[buffer[22]],
        linear_to_gamma_lut[buffer[21]],
        linear_to_gamma_lut[buffer[20]],
        linear_to_gamma_lut[buffer[19]],
        linear_to_gamma_lut[buffer[18]],
        linear_to_gamma_lut[buffer[17]],
        linear_to_gamma_lut[buffer[16]]);

    packed = _mm256_castsi128_si256(mapped_low);
    packed = _mm256_inserti128_si256(packed, mapped_high, 1);

    return packed;
}

static void
sixel_colorspace_store_block(unsigned char *pixels,
                             size_t offset,
                             const sixel_colorspace_layout_t *layout,
                             const unsigned char *rbuf,
                             const unsigned char *gbuf,
                             const unsigned char *bbuf)
{
    size_t i;

    for (i = 0U; i < 8U; ++i) {
        size_t base;

        base = offset + i * (size_t)layout->step;
        pixels[base + (size_t)layout->index_r] = rbuf[i];
        pixels[base + (size_t)layout->index_g] = gbuf[i];
        pixels[base + (size_t)layout->index_b] = bbuf[i];
    }
}

static void
sixel_colorspace_load_block(const unsigned char *pixels,
                            size_t offset,
                            const sixel_colorspace_layout_t *layout,
                            int apply_gamma_lut,
                            __m256 *r,
                            __m256 *g,
                            __m256 *b)
{
    size_t i;
    float rbuf[8];
    float gbuf[8];
    float bbuf[8];

    for (i = 0U; i < 8U; ++i) {
        size_t base;

        base = offset + i * (size_t)layout->step;
        rbuf[i] = (float)pixels[base + (size_t)layout->index_r] / 255.0f;
        if (apply_gamma_lut) {
            gbuf[i] = (float)gamma_to_linear_lut[
                pixels[base + (size_t)layout->index_g]] / 255.0f;
            bbuf[i] = (float)gamma_to_linear_lut[
                pixels[base + (size_t)layout->index_b]] / 255.0f;
            rbuf[i] = (float)gamma_to_linear_lut[
                pixels[base + (size_t)layout->index_r]] / 255.0f;
        } else {
            gbuf[i] = (float)pixels[base + (size_t)layout->index_g] / 255.0f;
            bbuf[i] = (float)pixels[base + (size_t)layout->index_b] / 255.0f;
        }
    }

    *r = _mm256_loadu_ps(rbuf);
    *g = _mm256_loadu_ps(gbuf);
    *b = _mm256_loadu_ps(bbuf);
}

static void
sixel_colorspace_yuv_to_rgb_block(__m256 y,
                                  __m256 u,
                                  __m256 v,
                                  __m256 *r,
                                  __m256 *g,
                                  __m256 *b)
{
    __m256 u_term;
    __m256 v_term;
    __m256 g_u_term;
    __m256 g_v_term;

    u = sixel_colorspace_clamp_u_avx(u);
    v = sixel_colorspace_clamp_v_avx(v);

    v_term = _mm256_mul_ps(_mm256_set1_ps(1.13983f), v);
    u_term = _mm256_mul_ps(_mm256_set1_ps(2.03211f), u);

    *r = _mm256_add_ps(y, v_term);
    *b = _mm256_add_ps(y, u_term);

    g_u_term = _mm256_mul_ps(_mm256_set1_ps(-0.39465f), u);
    g_v_term = _mm256_mul_ps(_mm256_set1_ps(-0.58060f), v);
    *g = _mm256_add_ps(y, _mm256_add_ps(g_u_term, g_v_term));

    *r = sixel_colorspace_clamp_unit_avx(*r);
    *g = sixel_colorspace_clamp_unit_avx(*g);
    *b = sixel_colorspace_clamp_unit_avx(*b);
}

static void
sixel_colorspace_rgb_to_yuv_block(__m256 r,
                                  __m256 g,
                                  __m256 b,
                                  __m256 *y,
                                  __m256 *u,
                                  __m256 *v)
{
    __m256 y_r;
    __m256 y_g;
    __m256 y_b;
    __m256 u_r;
    __m256 u_g;
    __m256 u_b;
    __m256 v_r;
    __m256 v_g;
    __m256 v_b;

    r = sixel_colorspace_clamp_unit_avx(r);
    g = sixel_colorspace_clamp_unit_avx(g);
    b = sixel_colorspace_clamp_unit_avx(b);

    y_r = _mm256_mul_ps(_mm256_set1_ps(0.299f), r);
    y_g = _mm256_mul_ps(_mm256_set1_ps(0.587f), g);
    y_b = _mm256_mul_ps(_mm256_set1_ps(0.114f), b);
    *y = _mm256_add_ps(y_r, _mm256_add_ps(y_g, y_b));

    u_r = _mm256_mul_ps(_mm256_set1_ps(-0.14713f), r);
    u_g = _mm256_mul_ps(_mm256_set1_ps(-0.28886f), g);
    u_b = _mm256_mul_ps(_mm256_set1_ps(0.43600f), b);
    *u = _mm256_add_ps(u_r, _mm256_add_ps(u_g, u_b));

    v_r = _mm256_mul_ps(_mm256_set1_ps(0.61500f), r);
    v_g = _mm256_mul_ps(_mm256_set1_ps(-0.51499f), g);
    v_b = _mm256_mul_ps(_mm256_set1_ps(-0.10001f), b);
    *v = _mm256_add_ps(v_r, _mm256_add_ps(v_g, v_b));

    *y = sixel_colorspace_clamp_unit_avx(*y);
    *u = sixel_colorspace_clamp_u_avx(*u);
    *v = sixel_colorspace_clamp_v_avx(*v);
}

static SIXELSTATUS
sixel_colorspace_convert_yuv_avx2(unsigned char *pixels,
                                  size_t size,
                                  int pixelformat,
                                  int colorspace_src,
                                  int colorspace_dst)
{
    sixel_colorspace_layout_t layout;
    size_t offset;

    if (colorspace_src != SIXEL_COLORSPACE_YUV) {
        return SIXEL_BAD_INPUT;
    }

    if (colorspace_dst != SIXEL_COLORSPACE_GAMMA &&
            colorspace_dst != SIXEL_COLORSPACE_LINEAR) {
        return SIXEL_BAD_INPUT;
    }

    sixel_colorspace_layout_pick(pixelformat, &layout);
    if (layout.step == 0) {
        return SIXEL_BAD_INPUT;
    }

    if (size % (size_t)layout.step != 0U) {
        return SIXEL_BAD_INPUT;
    }

    offset = 0U;
    while (offset + (size_t)(layout.step * 8) <= size) {
        __m256 comp0;
        __m256 comp1;
        __m256 comp2;
        __m256 out0;
        __m256 out1;
        __m256 out2;
        __m256i packed0;
        __m256i packed1;
        __m256i packed2;
        alignas(32) unsigned char buf0[32];
        alignas(32) unsigned char buf1[32];
        alignas(32) unsigned char buf2[32];

        sixel_colorspace_load_block(pixels,
                                    offset,
                                    &layout,
                                    0,
                                    &comp0,
                                    &comp1,
                                    &comp2);

        sixel_colorspace_yuv_to_rgb_block(comp0,
                                          comp1,
                                          comp2,
                                          &out0,
                                          &out1,
                                          &out2);

        if (colorspace_dst == SIXEL_COLORSPACE_GAMMA) {
            packed0 = sixel_colorspace_pack_gamma_bytes_avx(out0);
            packed1 = sixel_colorspace_pack_gamma_bytes_avx(out1);
            packed2 = sixel_colorspace_pack_gamma_bytes_avx(out2);
        } else {
            packed0 = sixel_colorspace_pack_linear_bytes_avx(out0);
            packed1 = sixel_colorspace_pack_linear_bytes_avx(out1);
            packed2 = sixel_colorspace_pack_linear_bytes_avx(out2);
        }

        _mm256_store_si256((__m256i *)buf0, packed0);
        _mm256_store_si256((__m256i *)buf1, packed1);
        _mm256_store_si256((__m256i *)buf2, packed2);

        sixel_colorspace_store_block(pixels,
                                     offset,
                                     &layout,
                                     buf0,
                                     buf1,
                                     buf2);

        offset += (size_t)layout.step * 8U;
    }

    if (offset < size) {
        SIXELSTATUS status;

        status = sixel_convert_pixels_via_linear(pixels + offset,
                                                 size - offset,
                                                 pixelformat,
                                                 colorspace_src,
                                                 colorspace_dst);
        return status;
    }

    return SIXEL_OK;
}

static SIXEL_TARGET_AVX2 void
sixel_colorspace_apply_avx2(unsigned char *pixels,
                            size_t size,
                            int pixelformat,
                            const unsigned char *lut,
                            const uint32_t *lut32)
{
    __m128i mask128;
    size_t offset;
    size_t remaining;
    uint8_t mask_buffer[16];

    mask128 = sixel_colorspace_alpha_mask_sse2(pixelformat);
    _mm_storeu_si128((__m128i *)mask_buffer, mask128);

    offset = 0U;
    remaining = size;
    while (remaining >= 16U) {
        __m128i input;
        __m128i input_hi;
        __m256i index0;
        __m256i index1;
        __m256i mapped0;
        __m256i mapped1;
        __m256i packed16;
        __m256i packed8;
        __m128i converted;
        __m128i preserved;

        input = _mm_loadu_si128((const __m128i *)(pixels + offset));
        input_hi = _mm_srli_si128(input, 8);

        index0 = _mm256_cvtepu8_epi32(input);
        index1 = _mm256_cvtepu8_epi32(input_hi);

        mapped0 = _mm256_i32gather_epi32((const int *)lut32,
                                          index0, 4);
        mapped1 = _mm256_i32gather_epi32((const int *)lut32,
                                          index1, 4);

        packed16 = _mm256_packs_epi32(mapped0, mapped1);
        packed8 = _mm256_packus_epi16(packed16, packed16);

        converted = _mm256_castsi256_si128(packed8);

        preserved = _mm_or_si128(_mm_and_si128(mask128, input),
                                 _mm_andnot_si128(mask128, converted));

        _mm_storeu_si128((__m128i *)(pixels + offset), preserved);

        offset += 16U;
        remaining -= 16U;
    }

    while (remaining > 0U) {
        unsigned char original;
        unsigned char mapped;
        size_t mask_index;

        mask_index = offset % 16U;
        original = pixels[offset];
        mapped = lut[original];
        if (mask_buffer[mask_index] == 0U) {
            pixels[offset] = mapped;
        }

        ++offset;
        --remaining;
    }
}

static SIXEL_TARGET_AVX2 SIXELSTATUS
sixel_colorspace_convert_avx2(unsigned char *pixels,
                              size_t size,
                              int pixelformat,
                              int colorspace_src,
                              int colorspace_dst)
{
    const unsigned char *lut;
    const uint32_t *lut32;

    if (colorspace_src == SIXEL_COLORSPACE_YUV ||
            colorspace_dst == SIXEL_COLORSPACE_YUV) {
        return sixel_colorspace_convert_yuv_avx2(pixels,
                                                 size,
                                                 pixelformat,
                                                 colorspace_src,
                                                 colorspace_dst);
    }

    lut = sixel_colorspace_select_lut(colorspace_src, colorspace_dst);
    lut32 = sixel_colorspace_select_lut32(colorspace_src, colorspace_dst);
    if (lut == NULL || lut32 == NULL) {
        return SIXEL_BAD_INPUT;
    }

    switch (pixelformat) {
    case SIXEL_PIXELFORMAT_RGB888:
    case SIXEL_PIXELFORMAT_BGR888:
        if (size % 3U != 0U) {
            return SIXEL_BAD_INPUT;
        }
        sixel_colorspace_apply_avx2(pixels, size, pixelformat, lut, lut32);
        return SIXEL_OK;
    case SIXEL_PIXELFORMAT_RGBA8888:
    case SIXEL_PIXELFORMAT_BGRA8888:
    case SIXEL_PIXELFORMAT_ARGB8888:
    case SIXEL_PIXELFORMAT_ABGR8888:
        if (size % 4U != 0U) {
            return SIXEL_BAD_INPUT;
        }
        sixel_colorspace_apply_avx2(pixels, size, pixelformat, lut, lut32);
        return SIXEL_OK;
    case SIXEL_PIXELFORMAT_G8:
        sixel_colorspace_apply_avx2(pixels, size, pixelformat, lut, lut32);
        return SIXEL_OK;
    case SIXEL_PIXELFORMAT_GA88:
    case SIXEL_PIXELFORMAT_AG88:
        if (size % 2U != 0U) {
            return SIXEL_BAD_INPUT;
        }
        sixel_colorspace_apply_avx2(pixels, size, pixelformat, lut, lut32);
        return SIXEL_OK;
    default:
        break;
    }

    return SIXEL_BAD_INPUT;
}
#endif

#if defined(SIXEL_USE_NEON)

static int
sixel_colorspace_neon_supported_format(int pixelformat)
{
    switch (pixelformat) {
    case SIXEL_PIXELFORMAT_RGB888:
    case SIXEL_PIXELFORMAT_BGR888:
    case SIXEL_PIXELFORMAT_RGBA8888:
    case SIXEL_PIXELFORMAT_BGRA8888:
    case SIXEL_PIXELFORMAT_ARGB8888:
    case SIXEL_PIXELFORMAT_ABGR8888:
    case SIXEL_PIXELFORMAT_G8:
    case SIXEL_PIXELFORMAT_GA88:
    case SIXEL_PIXELFORMAT_AG88:
        return 1;
    default:
        return 0;
    }
}

static SIXELSTATUS
sixel_colorspace_convert_neon(unsigned char *pixels,
                              size_t size,
                              int pixelformat,
                              int colorspace_src,
                              int colorspace_dst)
{
    const unsigned char *lut;

    lut = sixel_colorspace_select_lut(colorspace_src, colorspace_dst);
    if (lut == NULL) {
        return SIXEL_BAD_INPUT;
    }

    switch (pixelformat) {
    case SIXEL_PIXELFORMAT_RGB888:
    case SIXEL_PIXELFORMAT_BGR888:
        if (size % 3U != 0U) {
            return SIXEL_BAD_INPUT;
        }
        sixel_colorspace_apply_neon(pixels, size, pixelformat, lut);
        return SIXEL_OK;
    case SIXEL_PIXELFORMAT_RGBA8888:
    case SIXEL_PIXELFORMAT_BGRA8888:
    case SIXEL_PIXELFORMAT_ARGB8888:
    case SIXEL_PIXELFORMAT_ABGR8888:
        if (size % 4U != 0U) {
            return SIXEL_BAD_INPUT;
        }
        sixel_colorspace_apply_neon(pixels, size, pixelformat, lut);
        return SIXEL_OK;
    case SIXEL_PIXELFORMAT_G8:
        sixel_colorspace_apply_neon(pixels, size, pixelformat, lut);
        return SIXEL_OK;
    case SIXEL_PIXELFORMAT_GA88:
    case SIXEL_PIXELFORMAT_AG88:
        if (size % 2U != 0U) {
            return SIXEL_BAD_INPUT;
        }
        sixel_colorspace_apply_neon(pixels, size, pixelformat, lut);
        return SIXEL_OK;
    default:
        break;
    }

    return SIXEL_BAD_INPUT;
}
#endif

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
    return gamma_to_linear_double_lut[(int)v];
}

static inline unsigned char
sixel_linear_double_to_srgb(double v)
{
    int index;

    index = (int)(sixel_clamp_unit(v) * 255.0 + 0.5);
    if (index < 0) {
        index = 0;
    } else if (index >= SIXEL_COLORSPACE_LUT_SIZE) {
        index = SIXEL_COLORSPACE_LUT_SIZE - 1;
    }

    return linear_to_gamma_lut[index];
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
    return smptec_to_linear_double_lut[(int)v];
}

static inline unsigned char
sixel_linear_double_to_smptec(double v)
{
    int index;

    index = (int)(sixel_clamp_unit(v) * 255.0 + 0.5);
    if (index < 0) {
        index = 0;
    } else if (index >= SIXEL_COLORSPACE_LUT_SIZE) {
        index = SIXEL_COLORSPACE_LUT_SIZE - 1;
    }

    return linear_to_smptec_lut[index];
}

static inline double
sixel_cielab_f(double t)
{
    double delta;

    delta = 6.0 / 29.0;
    if (t > delta * delta * delta) {
        return cbrt(t);
    }

    return (t / (3.0 * delta * delta)) + (4.0 / 29.0);
}

static inline double
sixel_cielab_f_inv(double t)
{
    double delta;

    delta = 6.0 / 29.0;
    if (t > delta) {
        return t * t * t;
    }

    return 3.0 * delta * delta * (t - (4.0 / 29.0));
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

static inline unsigned char
sixel_cielab_encode_L(double L)
{
    double clamped;

    clamped = sixel_clamp_unit(L);
    return sixel_colorspace_clamp((int)(clamped * 255.0 + 0.5));
}

static inline unsigned char
sixel_cielab_encode_ab(double value)
{
    double shifted;
    double normalized;

    shifted = sixel_cielab_clamp_ab(value);
    normalized = (shifted / (2.0 * SIXEL_CIELAB_AB_LIMIT)) + 0.5;
    return sixel_colorspace_clamp((int)(normalized * 255.0 + 0.5));
}

static inline double
sixel_cielab_decode_ab(unsigned char v)
{
    double encoded;

    encoded = (double)v / 255.0;
    return (encoded - 0.5) * (2.0 * SIXEL_CIELAB_AB_LIMIT);
}

static inline unsigned char
sixel_din99d_encode_L(double L)
{
    double clamped;

    clamped = sixel_clamp_unit(L);
    return sixel_colorspace_clamp((int)(clamped * 255.0 + 0.5));
}

static inline unsigned char
sixel_din99d_encode_ab(double value)
{
    double normalized;

    normalized = (sixel_din99d_clamp_ab_norm(value) / 2.0) + 0.5;
    return sixel_colorspace_clamp((int)(normalized * 255.0 + 0.5));
}

static inline double
sixel_din99d_decode_ab(unsigned char v)
{
    double encoded;

    encoded = (double)v / 255.0;
    return sixel_din99d_clamp_ab_norm((encoded - 0.5) * 2.0);
}

static inline unsigned char
sixel_yuv_encode_y(double Y)
{
    double clamped;

    clamped = sixel_clamp_unit(Y);
    return sixel_colorspace_clamp((int)(clamped * 255.0 + 0.5));
}

static inline unsigned char
sixel_yuv_encode_u(double value)
{
    double normalized;

    normalized = (sixel_yuv_clamp_u(value) / (2.0 * SIXEL_YUV_U_MAX)) + 0.5;
    return sixel_colorspace_clamp((int)(normalized * 255.0 + 0.5));
}

static inline unsigned char
sixel_yuv_encode_v(double value)
{
    double normalized;

    normalized = (sixel_yuv_clamp_v(value) / (2.0 * SIXEL_YUV_V_MAX)) + 0.5;
    return sixel_colorspace_clamp((int)(normalized * 255.0 + 0.5));
}

static inline double
sixel_yuv_decode_u(unsigned char v)
{
    double encoded;

    encoded = (double)v / 255.0;
    return (encoded - 0.5) * (2.0 * SIXEL_YUV_U_MAX);
}

static inline double
sixel_yuv_decode_v(unsigned char v)
{
    double encoded;

    encoded = (double)v / 255.0;
    return (encoded - 0.5) * (2.0 * SIXEL_YUV_V_MAX);
}

static void
sixel_linear_to_cielab(double r, double g, double b,
                       double *L, double *A, double *B)
{
    const double Xn = 0.95047;
    const double Yn = 1.00000;
    const double Zn = 1.08883;
    double X;
    double Y;
    double Z;
    double fx;
    double fy;
    double fz;
    double L_component;
    double a_component;
    double b_component;

    X = 0.4124564 * r + 0.3575761 * g + 0.1804375 * b;
    Y = 0.2126729 * r + 0.7151522 * g + 0.0721750 * b;
    Z = 0.0193339 * r + 0.1191920 * g + 0.9503041 * b;

    fx = sixel_cielab_f(X / Xn);
    fy = sixel_cielab_f(Y / Yn);
    fz = sixel_cielab_f(Z / Zn);

    L_component = 116.0 * fy - 16.0;
    a_component = 500.0 * (fx - fy);
    b_component = 200.0 * (fy - fz);

    *L = sixel_clamp_unit(L_component / SIXEL_CIELAB_L_SCALE);
    *A = sixel_cielab_clamp_ab(a_component / SIXEL_CIELAB_AB_SCALE);
    *B = sixel_cielab_clamp_ab(b_component / SIXEL_CIELAB_AB_SCALE);
}

static void
sixel_cielab_to_linear(double L, double A, double B,
                       double *r, double *g, double *b)
{
    const double Xn = 0.95047;
    const double Yn = 1.00000;
    const double Zn = 1.08883;
    double L_component;
    double a_component;
    double b_component;
    double fx;
    double fy;
    double fz;
    double X;
    double Y;
    double Z;

    L_component = sixel_clamp_unit(L) * SIXEL_CIELAB_L_SCALE;
    a_component = sixel_cielab_clamp_ab(A) * SIXEL_CIELAB_AB_SCALE;
    b_component = sixel_cielab_clamp_ab(B) * SIXEL_CIELAB_AB_SCALE;

    fy = (L_component + 16.0) / 116.0;
    fx = fy + (a_component / 500.0);
    fz = fy - (b_component / 200.0);

    X = Xn * sixel_cielab_f_inv(fx);
    Y = Yn * sixel_cielab_f_inv(fy);
    Z = Zn * sixel_cielab_f_inv(fz);

    *r = 3.2404542 * X - 1.5371385 * Y - 0.4985314 * Z;
    *g = -0.9692660 * X + 1.8760108 * Y + 0.0415560 * Z;
    *b = 0.0556434 * X - 0.2040259 * Y + 1.0572252 * Z;

    *r = sixel_clamp_unit(*r);
    *g = sixel_clamp_unit(*g);
    *b = sixel_clamp_unit(*b);
}

static void
sixel_cielab_to_din99d(double L,
                       double a,
                       double b,
                       double *L99d,
                       double *A99d,
                       double *B99d)
{
    /* Convert from CIELAB to DIN99d using Cui et al. (2002) parameters. */
    const double c1 = 325.22;
    const double c2 = 0.0036;
    const double c3 = 50.0;
    const double c4 = 1.14;
    const double c5 = 22.5;
    const double c6 = 0.06;
    const double c7 = 50.0;
    const double c8 = 1.0;
    const double rad_per_degree = 3.14159265358979323846 / 180.0;
    double radians_c3;
    double radians_c7;
    double e;
    double f;
    double G;
    double h_ef;
    double C99;

    radians_c3 = c3 * rad_per_degree;
    radians_c7 = c7 * rad_per_degree;

    e = cos(radians_c3) * a + sin(radians_c3) * b;
    f = c4 * (-sin(radians_c3) * a + cos(radians_c3) * b);
    G = sqrt(e * e + f * f);
    h_ef = atan2(f, e) + radians_c7;

    C99 = c5 * (log1p(c6 * G)) / (c8);

    *A99d = C99 * cos(h_ef);
    *B99d = C99 * sin(h_ef);
    *L99d = c1 * log1p(c2 * L);
}

static void
sixel_din99d_to_cielab(double L99d,
                       double A99d,
                       double B99d,
                       double *L,
                       double *a,
                       double *b)
{
    /* Convert from DIN99d back to absolute CIELAB coordinates. */
    const double c1 = 325.22;
    const double c2 = 0.0036;
    const double c3 = 50.0;
    const double c4 = 1.14;
    const double c5 = 22.5;
    const double c6 = 0.06;
    const double c7 = 50.0;
    const double c8 = 1.0;
    const double rad_per_degree = 3.14159265358979323846 / 180.0;
    double radians_c3;
    double radians_c7;
    double h99;
    double C99;
    double G;
    double e;
    double f;

    radians_c3 = c3 * rad_per_degree;
    radians_c7 = c7 * rad_per_degree;

    h99 = atan2(B99d, A99d) - radians_c7;
    C99 = hypot(A99d, B99d);
    G = expm1((c8 / c5) * C99) / c6;

    e = G * cos(h99);
    f = G * sin(h99);

    *a = e * cos(radians_c3) - (f / c4) * sin(radians_c3);
    *b = e * sin(radians_c3) + (f / c4) * cos(radians_c3);
    *L = expm1(L99d / c1) / c2;
}

static void
sixel_linear_to_din99d(double r,
                       double g,
                       double b,
                       double *L99d_norm,
                       double *A99d_norm,
                       double *B99d_norm)
{
    double L;
    double A;
    double B;
    double L_star;
    double a_star;
    double b_star;
    double L99d;
    double A99d;
    double B99d;

    sixel_linear_to_cielab(r, g, b, &L, &A, &B);

    L_star = L * SIXEL_CIELAB_L_SCALE;
    a_star = A * SIXEL_CIELAB_AB_SCALE;
    b_star = B * SIXEL_CIELAB_AB_SCALE;

    sixel_cielab_to_din99d(L_star, a_star, b_star, &L99d, &A99d, &B99d);

    *L99d_norm = sixel_clamp_unit(L99d / SIXEL_DIN99D_L_SCALE);
    *A99d_norm = sixel_din99d_clamp_ab_norm(
        A99d / SIXEL_DIN99D_AB_RANGE);
    *B99d_norm = sixel_din99d_clamp_ab_norm(
        B99d / SIXEL_DIN99D_AB_RANGE);
}

static void
sixel_din99d_to_linear(double L99d_norm,
                       double A99d_norm,
                       double B99d_norm,
                       double *r,
                       double *g,
                       double *b)
{
    double L_star;
    double a_star;
    double b_star;
    double L;
    double A;
    double B;

    L = sixel_clamp_unit(L99d_norm) * SIXEL_DIN99D_L_SCALE;
    A = sixel_din99d_clamp_ab_norm(A99d_norm) * SIXEL_DIN99D_AB_RANGE;
    B = sixel_din99d_clamp_ab_norm(B99d_norm) * SIXEL_DIN99D_AB_RANGE;

    sixel_din99d_to_cielab(L, A, B, &L_star, &a_star, &b_star);

    L_star = sixel_clamp_unit(L_star / SIXEL_CIELAB_L_SCALE);
    a_star = sixel_cielab_clamp_ab(a_star / SIXEL_CIELAB_AB_SCALE);
    b_star = sixel_cielab_clamp_ab(b_star / SIXEL_CIELAB_AB_SCALE);

    sixel_cielab_to_linear(L_star, a_star, b_star, r, g, b);
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
sixel_smptec_to_linear_scalar(float *r, float *g, float *b)
{
    double rs;
    double gs;
    double bs;
    double r_lin;
    double g_lin;
    double b_lin;

    rs = sixel_clamp_unit((double)*r);
    gs = sixel_clamp_unit((double)*g);
    bs = sixel_clamp_unit((double)*b);

    r_lin = sixel_linear_smptec_to_srgb_matrix[0][0] * rs
          + sixel_linear_smptec_to_srgb_matrix[0][1] * gs
          + sixel_linear_smptec_to_srgb_matrix[0][2] * bs;
    g_lin = sixel_linear_smptec_to_srgb_matrix[1][0] * rs
          + sixel_linear_smptec_to_srgb_matrix[1][1] * gs
          + sixel_linear_smptec_to_srgb_matrix[1][2] * bs;
    b_lin = sixel_linear_smptec_to_srgb_matrix[2][0] * rs
          + sixel_linear_smptec_to_srgb_matrix[2][1] * gs
          + sixel_linear_smptec_to_srgb_matrix[2][2] * bs;

    *r = (float)sixel_clamp_unit(r_lin);
    *g = (float)sixel_clamp_unit(g_lin);
    *b = (float)sixel_clamp_unit(b_lin);
}

static void
sixel_linear_to_smptec_scalar(float *r, float *g, float *b)
{
    double r_lin;
    double g_lin;
    double b_lin;
    double sr;
    double sg;
    double sb;

    r_lin = sixel_clamp_unit((double)*r);
    g_lin = sixel_clamp_unit((double)*g);
    b_lin = sixel_clamp_unit((double)*b);

    sr = sixel_linear_srgb_to_smptec_matrix[0][0] * r_lin
       + sixel_linear_srgb_to_smptec_matrix[0][1] * g_lin
       + sixel_linear_srgb_to_smptec_matrix[0][2] * b_lin;
    sg = sixel_linear_srgb_to_smptec_matrix[1][0] * r_lin
       + sixel_linear_srgb_to_smptec_matrix[1][1] * g_lin
       + sixel_linear_srgb_to_smptec_matrix[1][2] * b_lin;
    sb = sixel_linear_srgb_to_smptec_matrix[2][0] * r_lin
       + sixel_linear_srgb_to_smptec_matrix[2][1] * g_lin
       + sixel_linear_srgb_to_smptec_matrix[2][2] * b_lin;

    *r = (float)sixel_clamp_unit(sr);
    *g = (float)sixel_clamp_unit(sg);
    *b = (float)sixel_clamp_unit(sb);
}

static void
sixel_linear_to_yuv(double r, double g, double b,
                    double *y, double *u, double *v)
{
    double y_component;
    double u_component;
    double v_component;

    y_component = 0.299 * r + 0.587 * g + 0.114 * b;
    u_component = -0.14713 * r - 0.28886 * g + 0.43600 * b;
    v_component = 0.61500 * r - 0.51499 * g - 0.10001 * b;

    *y = sixel_clamp_unit(y_component);
    *u = sixel_yuv_clamp_u(u_component);
    *v = sixel_yuv_clamp_v(v_component);
}

static void
sixel_yuv_to_linear(double y, double u, double v,
                    double *r, double *g, double *b)
{
    double y_clamped;
    double u_clamped;
    double v_clamped;
    double r_lin;
    double g_lin;
    double b_lin;

    y_clamped = sixel_clamp_unit(y);
    u_clamped = sixel_yuv_clamp_u(u);
    v_clamped = sixel_yuv_clamp_v(v);

    r_lin = y_clamped + 1.13983 * v_clamped;
    g_lin = y_clamped - 0.39465 * u_clamped - 0.58060 * v_clamped;
    b_lin = y_clamped + 2.03211 * u_clamped;

    *r = sixel_clamp_unit(r_lin);
    *g = sixel_clamp_unit(g_lin);
    *b = sixel_clamp_unit(b_lin);
}

/*
 * SIMD helpers below operate on interleaved RGB float triplets. Channels are
 * gathered with byte offsets, multiplied by a 3x3 matrix, and scattered back
 * into the same AoS layout after clamping to the destination domain.
 */
#if defined(SIXEL_USE_AVX512) && defined(__AVX512F__) && \
        defined(__AVX512BW__)
static SIXEL_TARGET_AVX512 size_t
sixel_linear_to_yuv_avx512(float *pixels, size_t pixel_total)
{
    size_t index;
    size_t processed;
    const __m512 zero = _mm512_set1_ps(0.0f);
    const __m512 u_min = _mm512_set1_ps(-SIXEL_YUV_U_MAX);
    const __m512 u_max = _mm512_set1_ps(SIXEL_YUV_U_MAX);
    const __m512 v_min = _mm512_set1_ps(-SIXEL_YUV_V_MAX);
    const __m512 v_max = _mm512_set1_ps(SIXEL_YUV_V_MAX);
    const __m512 m00 = _mm512_set1_ps(0.299f);
    const __m512 m01 = _mm512_set1_ps(0.587f);
    const __m512 m02 = _mm512_set1_ps(0.114f);
    const __m512 m10 = _mm512_set1_ps(-0.14713f);
    const __m512 m11 = _mm512_set1_ps(-0.28886f);
    const __m512 m12 = _mm512_set1_ps(0.43600f);
    const __m512 m20 = _mm512_set1_ps(0.61500f);
    const __m512 m21 = _mm512_set1_ps(-0.51499f);
    const __m512 m22 = _mm512_set1_ps(-0.10001f);
    const __m512i idx_r = _mm512_setr_epi32(
        0, 12, 24, 36, 48, 60, 72, 84,
        96, 108, 120, 132, 144, 156, 168, 180);
    const __m512i idx_g = _mm512_setr_epi32(
        4, 16, 28, 40, 52, 64, 76, 88,
        100, 112, 124, 136, 148, 160, 172, 184);
    const __m512i idx_b = _mm512_setr_epi32(
        8, 20, 32, 44, 56, 68, 80, 92,
        104, 116, 128, 140, 152, 164, 176, 188);

    processed = pixel_total - (pixel_total % 16U);
    for (index = 0U; index < processed; index += 16U) {
        const char *base_char;
        const float *base;
        __m512 r;
        __m512 g;
        __m512 b;
        __m512 y;
        __m512 u;
        __m512 v;

        base_char = (const char *)(pixels + index * 3U);
        base = (const float *)base_char;

        r = _mm512_i32gather_ps(idx_r, base, 1);
        g = _mm512_i32gather_ps(idx_g, base, 1);
        b = _mm512_i32gather_ps(idx_b, base, 1);

        y = SIXEL_FMADD_PS512(g, m01, _mm512_mul_ps(r, m00));
        y = SIXEL_FMADD_PS512(b, m02, y);

        u = SIXEL_FMADD_PS512(g, m11, _mm512_mul_ps(r, m10));
        u = SIXEL_FMADD_PS512(b, m12, u);

        v = SIXEL_FMADD_PS512(g, m21, _mm512_mul_ps(r, m20));
        v = SIXEL_FMADD_PS512(b, m22, v);

        y = _mm512_min_ps(_mm512_set1_ps(1.0f),
                          _mm512_max_ps(zero, y));
        u = _mm512_min_ps(u_max, _mm512_max_ps(u_min, u));
        v = _mm512_min_ps(v_max, _mm512_max_ps(v_min, v));

        _mm512_i32scatter_ps((float *)base, idx_r, y, 1);
        _mm512_i32scatter_ps((float *)base, idx_g, u, 1);
        _mm512_i32scatter_ps((float *)base, idx_b, v, 1);
    }

    return processed;
}
#endif

#if defined(SIXEL_USE_AVX2) && defined(__AVX2__)
static SIXEL_TARGET_AVX2 size_t
sixel_linear_to_yuv_avx2(float *pixels, size_t pixel_total)
{
    size_t index;
    size_t processed;
    const __m256 zero = _mm256_set1_ps(0.0f);
    const __m256 y_max = _mm256_set1_ps(1.0f);
    const __m256 u_min = _mm256_set1_ps(-SIXEL_YUV_U_MAX);
    const __m256 u_max = _mm256_set1_ps(SIXEL_YUV_U_MAX);
    const __m256 v_min = _mm256_set1_ps(-SIXEL_YUV_V_MAX);
    const __m256 v_max = _mm256_set1_ps(SIXEL_YUV_V_MAX);
    const __m256 m00 = _mm256_set1_ps(0.299f);
    const __m256 m01 = _mm256_set1_ps(0.587f);
    const __m256 m02 = _mm256_set1_ps(0.114f);
    const __m256 m10 = _mm256_set1_ps(-0.14713f);
    const __m256 m11 = _mm256_set1_ps(-0.28886f);
    const __m256 m12 = _mm256_set1_ps(0.43600f);
    const __m256 m20 = _mm256_set1_ps(0.61500f);
    const __m256 m21 = _mm256_set1_ps(-0.51499f);
    const __m256 m22 = _mm256_set1_ps(-0.10001f);
    const __m256i idx_r = _mm256_setr_epi32(
        0, 12, 24, 36, 48, 60, 72, 84);
    const __m256i idx_g = _mm256_setr_epi32(
        4, 16, 28, 40, 52, 64, 76, 88);
    const __m256i idx_b = _mm256_setr_epi32(
        8, 20, 32, 44, 56, 68, 80, 92);
    const char *base_char;
    const float *base;
    float *store;
    __m256 r;
    __m256 g;
    __m256 b;
    __m256 y;
    __m256 u;
    __m256 v;
    __m128 y_lo;
    __m128 y_hi;
    __m128 u_lo;
    __m128 u_hi;
    __m128 v_lo;
    __m128 v_hi;
    __m128 yu_lo;
    __m128 yu_hi;
    __m128 uv_lo;
    __m128 uv_hi;
    __m128 vy_hi;
    __m128 store0;
    __m128 store1;
    __m128 store2;
    __m128 store3;
    __m128 store4;
    __m128 store5;

    processed = pixel_total - (pixel_total % 8U);
    for (index = 0U; index < processed; index += 8U) {
        base_char = (const char *)(pixels + index * 3U);
        base = (const float *)base_char;
        store = (float *)base_char;

        r = _mm256_i32gather_ps(base, idx_r, 1);
        g = _mm256_i32gather_ps(base, idx_g, 1);
        b = _mm256_i32gather_ps(base, idx_b, 1);

        y = SIXEL_FMADD_PS256(g, m01, _mm256_mul_ps(r, m00));
        y = SIXEL_FMADD_PS256(b, m02, y);

        u = SIXEL_FMADD_PS256(g, m11, _mm256_mul_ps(r, m10));
        u = SIXEL_FMADD_PS256(b, m12, u);

        v = SIXEL_FMADD_PS256(g, m21, _mm256_mul_ps(r, m20));
        v = SIXEL_FMADD_PS256(b, m22, v);

        y = _mm256_min_ps(y_max, _mm256_max_ps(zero, y));
        u = _mm256_min_ps(u_max, _mm256_max_ps(u_min, u));
        v = _mm256_min_ps(v_max, _mm256_max_ps(v_min, v));

        y_lo = _mm256_castps256_ps128(y);
        y_hi = _mm256_extractf128_ps(y, 1);
        u_lo = _mm256_castps256_ps128(u);
        u_hi = _mm256_extractf128_ps(u, 1);
        v_lo = _mm256_castps256_ps128(v);
        v_hi = _mm256_extractf128_ps(v, 1);

        yu_lo = _mm_unpacklo_ps(y_lo, u_lo);
        yu_hi = _mm_unpackhi_ps(y_lo, u_lo);
        uv_lo = _mm_unpacklo_ps(u_lo, v_lo);
        uv_hi = _mm_unpackhi_ps(u_lo, v_lo);
        vy_hi = _mm_unpackhi_ps(v_lo, y_lo);

        store0 = _mm_movelh_ps(yu_lo, v_lo);
        store0 = _mm_blend_ps(store0,
                              _mm_shuffle_ps(y_lo, y_lo,
                                             _MM_SHUFFLE(1, 1, 1, 1)),
                              0x8);
        store1 = _mm_shuffle_ps(uv_lo, yu_hi, 0x4E);
        store2 = _mm_shuffle_ps(vy_hi, uv_hi, 0xEC);

        _mm_storeu_ps(store, store0);
        _mm_storeu_ps(store + 4U, store1);
        _mm_storeu_ps(store + 8U, store2);

        yu_lo = _mm_unpacklo_ps(y_hi, u_hi);
        yu_hi = _mm_unpackhi_ps(y_hi, u_hi);
        uv_lo = _mm_unpacklo_ps(u_hi, v_hi);
        uv_hi = _mm_unpackhi_ps(u_hi, v_hi);
        vy_hi = _mm_unpackhi_ps(v_hi, y_hi);

        store3 = _mm_movelh_ps(yu_lo, v_hi);
        store3 = _mm_blend_ps(store3,
                              _mm_shuffle_ps(y_hi, y_hi,
                                             _MM_SHUFFLE(1, 1, 1, 1)),
                              0x8);
        store4 = _mm_shuffle_ps(uv_lo, yu_hi, 0x4E);
        store5 = _mm_shuffle_ps(vy_hi, uv_hi, 0xEC);

        _mm_storeu_ps(store + 12U, store3);
        _mm_storeu_ps(store + 16U, store4);
        _mm_storeu_ps(store + 20U, store5);
    }

    return processed;
}
#endif

#if defined(SIXEL_USE_SSE2)
static size_t
sixel_linear_to_yuv_sse2(float *pixels, size_t pixel_total)
{
    size_t index;
    size_t processed;
    const __m128 zero = _mm_set1_ps(0.0f);
    const __m128 y_max = _mm_set1_ps(1.0f);
    const __m128 u_min = _mm_set1_ps(-SIXEL_YUV_U_MAX);
    const __m128 u_max = _mm_set1_ps(SIXEL_YUV_U_MAX);
    const __m128 v_min = _mm_set1_ps(-SIXEL_YUV_V_MAX);
    const __m128 v_max = _mm_set1_ps(SIXEL_YUV_V_MAX);
    const __m128 m00 = _mm_set1_ps(0.299f);
    const __m128 m01 = _mm_set1_ps(0.587f);
    const __m128 m02 = _mm_set1_ps(0.114f);
    const __m128 m10 = _mm_set1_ps(-0.14713f);
    const __m128 m11 = _mm_set1_ps(-0.28886f);
    const __m128 m12 = _mm_set1_ps(0.43600f);
    const __m128 m20 = _mm_set1_ps(0.61500f);
    const __m128 m21 = _mm_set1_ps(-0.51499f);
    const __m128 m22 = _mm_set1_ps(-0.10001f);
    float y_out[4];
    float u_out[4];
    float v_out[4];

    processed = pixel_total - (pixel_total % 4U);
    for (index = 0U; index < processed; index += 4U) {
        float *base;
        __m128 r;
        __m128 g;
        __m128 b;
        __m128 y;
        __m128 u;
        __m128 v;

        base = pixels + index * 3U;

        r = _mm_set_ps(base[9], base[6], base[3], base[0]);
        g = _mm_set_ps(base[10], base[7], base[4], base[1]);
        b = _mm_set_ps(base[11], base[8], base[5], base[2]);

        y = _mm_add_ps(_mm_mul_ps(r, m00), _mm_mul_ps(g, m01));
        y = _mm_add_ps(y, _mm_mul_ps(b, m02));

        u = _mm_add_ps(_mm_mul_ps(r, m10), _mm_mul_ps(g, m11));
        u = _mm_add_ps(u, _mm_mul_ps(b, m12));

        v = _mm_add_ps(_mm_mul_ps(r, m20), _mm_mul_ps(g, m21));
        v = _mm_add_ps(v, _mm_mul_ps(b, m22));

        y = _mm_min_ps(y_max, _mm_max_ps(zero, y));
        u = _mm_min_ps(u_max, _mm_max_ps(u_min, u));
        v = _mm_min_ps(v_max, _mm_max_ps(v_min, v));

        _mm_storeu_ps(y_out, y);
        _mm_storeu_ps(u_out, u);
        _mm_storeu_ps(v_out, v);

        base[0] = y_out[0];
        base[1] = u_out[0];
        base[2] = v_out[0];
        base[3] = y_out[1];
        base[4] = u_out[1];
        base[5] = v_out[1];
        base[6] = y_out[2];
        base[7] = u_out[2];
        base[8] = v_out[2];
        base[9] = y_out[3];
        base[10] = u_out[3];
        base[11] = v_out[3];
    }

    return processed;
}
#endif

#if defined(SIXEL_USE_NEON)
static size_t
sixel_linear_to_yuv_neon(float *pixels, size_t pixel_total)
{
    size_t index;
    size_t processed;
    const float32x4_t zero = vdupq_n_f32(0.0f);
    const float32x4_t y_max = vdupq_n_f32(1.0f);
    const float32x4_t u_min = vdupq_n_f32(-SIXEL_YUV_U_MAX);
    const float32x4_t u_max = vdupq_n_f32(SIXEL_YUV_U_MAX);
    const float32x4_t v_min = vdupq_n_f32(-SIXEL_YUV_V_MAX);
    const float32x4_t v_max = vdupq_n_f32(SIXEL_YUV_V_MAX);
    const float32x4_t m00 = vdupq_n_f32(0.299f);
    const float32x4_t m01 = vdupq_n_f32(0.587f);
    const float32x4_t m02 = vdupq_n_f32(0.114f);
    const float32x4_t m10 = vdupq_n_f32(-0.14713f);
    const float32x4_t m11 = vdupq_n_f32(-0.28886f);
    const float32x4_t m12 = vdupq_n_f32(0.43600f);
    const float32x4_t m20 = vdupq_n_f32(0.61500f);
    const float32x4_t m21 = vdupq_n_f32(-0.51499f);
    const float32x4_t m22 = vdupq_n_f32(-0.10001f);

    processed = pixel_total - (pixel_total % 4U);
    for (index = 0U; index < processed; index += 4U) {
        float32x4x3_t rgb;
        float32x4_t y;
        float32x4_t u;
        float32x4_t v;
        float32x4x3_t out;

        rgb = vld3q_f32(pixels + index * 3U);

        y = vmlaq_f32(vmulq_f32(rgb.val[0], m00), rgb.val[1], m01);
        y = vmlaq_f32(y, rgb.val[2], m02);

        u = vmlaq_f32(vmulq_f32(rgb.val[0], m10), rgb.val[1], m11);
        u = vmlaq_f32(u, rgb.val[2], m12);

        v = vmlaq_f32(vmulq_f32(rgb.val[0], m20), rgb.val[1], m21);
        v = vmlaq_f32(v, rgb.val[2], m22);

        y = vminq_f32(y_max, vmaxq_f32(zero, y));
        u = vminq_f32(u_max, vmaxq_f32(u_min, u));
        v = vminq_f32(v_max, vmaxq_f32(v_min, v));

        out.val[0] = y;
        out.val[1] = u;
        out.val[2] = v;

        vst3q_f32(pixels + index * 3U, out);
    }

    return processed;
}
#endif

#if defined(SIXEL_USE_AVX512) && defined(__AVX512F__) && \
        defined(__AVX512BW__)
static SIXEL_TARGET_AVX512 size_t
sixel_yuv_to_linear_avx512(float *pixels, size_t pixel_total)
{
    size_t index;
    size_t processed;
    const __m512 zero = _mm512_set1_ps(0.0f);
    const __m512 one = _mm512_set1_ps(1.0f);
    const __m512 y_min = _mm512_set1_ps(0.0f);
    const __m512 y_max = _mm512_set1_ps(1.0f);
    const __m512 u_min = _mm512_set1_ps(-SIXEL_YUV_U_MAX);
    const __m512 u_max = _mm512_set1_ps(SIXEL_YUV_U_MAX);
    const __m512 v_min = _mm512_set1_ps(-SIXEL_YUV_V_MAX);
    const __m512 v_max = _mm512_set1_ps(SIXEL_YUV_V_MAX);
    const __m512 m10 = _mm512_set1_ps(-0.39465f);
    const __m512 m11 = _mm512_set1_ps(-0.58060f);
    const __m512 m21 = _mm512_set1_ps(2.03211f);
    const __m512 m20 = _mm512_set1_ps(1.13983f);
    const __m512i idx_r = _mm512_setr_epi32(
        0, 12, 24, 36, 48, 60, 72, 84,
        96, 108, 120, 132, 144, 156, 168, 180);
    const __m512i idx_g = _mm512_setr_epi32(
        4, 16, 28, 40, 52, 64, 76, 88,
        100, 112, 124, 136, 148, 160, 172, 184);
    const __m512i idx_b = _mm512_setr_epi32(
        8, 20, 32, 44, 56, 68, 80, 92,
        104, 116, 128, 140, 152, 164, 176, 188);

    processed = pixel_total - (pixel_total % 16U);
    for (index = 0U; index < processed; index += 16U) {
        const char *base_char;
        const float *base;
        __m512 y;
        __m512 u;
        __m512 v;
        __m512 r;
        __m512 g;
        __m512 b;

        base_char = (const char *)(pixels + index * 3U);
        base = (const float *)base_char;

        y = _mm512_i32gather_ps(idx_r, base, 1);
        u = _mm512_i32gather_ps(idx_g, base, 1);
        v = _mm512_i32gather_ps(idx_b, base, 1);

        y = _mm512_min_ps(y_max, _mm512_max_ps(y_min, y));
        u = _mm512_min_ps(u_max, _mm512_max_ps(u_min, u));
        v = _mm512_min_ps(v_max, _mm512_max_ps(v_min, v));

        r = SIXEL_FMADD_PS512(v, m20, y);
        g = SIXEL_FMADD_PS512(v, m11, y);
        g = SIXEL_FMADD_PS512(u, m10, g);
        b = SIXEL_FMADD_PS512(u, m21, y);

        r = _mm512_min_ps(one, _mm512_max_ps(zero, r));
        g = _mm512_min_ps(one, _mm512_max_ps(zero, g));
        b = _mm512_min_ps(one, _mm512_max_ps(zero, b));

        _mm512_i32scatter_ps((float *)base, idx_r, r, 1);
        _mm512_i32scatter_ps((float *)base, idx_g, g, 1);
        _mm512_i32scatter_ps((float *)base, idx_b, b, 1);
    }

    return processed;
}
#endif

#if defined(SIXEL_USE_AVX2) && defined(__AVX2__)
static SIXEL_TARGET_AVX2 size_t
sixel_yuv_to_linear_avx2(float *pixels, size_t pixel_total)
{
    size_t index;
    size_t processed;
    const __m256 zero = _mm256_set1_ps(0.0f);
    const __m256 one = _mm256_set1_ps(1.0f);
    const __m256 y_min = _mm256_set1_ps(0.0f);
    const __m256 y_max = _mm256_set1_ps(1.0f);
    const __m256 u_min = _mm256_set1_ps(-SIXEL_YUV_U_MAX);
    const __m256 u_max = _mm256_set1_ps(SIXEL_YUV_U_MAX);
    const __m256 v_min = _mm256_set1_ps(-SIXEL_YUV_V_MAX);
    const __m256 v_max = _mm256_set1_ps(SIXEL_YUV_V_MAX);
    const __m256 m10 = _mm256_set1_ps(-0.39465f);
    const __m256 m11 = _mm256_set1_ps(-0.58060f);
    const __m256 m21 = _mm256_set1_ps(2.03211f);
    const __m256 m20 = _mm256_set1_ps(1.13983f);
    const __m256i idx_r = _mm256_setr_epi32(
        0, 12, 24, 36, 48, 60, 72, 84);
    const __m256i idx_g = _mm256_setr_epi32(
        4, 16, 28, 40, 52, 64, 76, 88);
    const __m256i idx_b = _mm256_setr_epi32(
        8, 20, 32, 44, 56, 68, 80, 92);
    const char *base_char;
    const float *base;
    float *store;
    __m256 y;
    __m256 u;
    __m256 v;
    __m256 r;
    __m256 g;
    __m256 b;
    __m128 r_lo;
    __m128 r_hi;
    __m128 g_lo;
    __m128 g_hi;
    __m128 b_lo;
    __m128 b_hi;
    __m128 rg_lo;
    __m128 rg_hi;
    __m128 gb_lo;
    __m128 gb_hi;
    __m128 br_hi;
    __m128 store0;
    __m128 store1;
    __m128 store2;
    __m128 store3;
    __m128 store4;
    __m128 store5;

    processed = pixel_total - (pixel_total % 8U);
    for (index = 0U; index < processed; index += 8U) {
        base_char = (const char *)(pixels + index * 3U);
        base = (const float *)base_char;
        store = (float *)base_char;

        y = _mm256_i32gather_ps(base, idx_r, 1);
        u = _mm256_i32gather_ps(base, idx_g, 1);
        v = _mm256_i32gather_ps(base, idx_b, 1);

        y = _mm256_min_ps(y_max, _mm256_max_ps(y_min, y));
        u = _mm256_min_ps(u_max, _mm256_max_ps(u_min, u));
        v = _mm256_min_ps(v_max, _mm256_max_ps(v_min, v));

        r = SIXEL_FMADD_PS256(v, m20, y);
        g = SIXEL_FMADD_PS256(v, m11, y);
        g = SIXEL_FMADD_PS256(u, m10, g);
        b = SIXEL_FMADD_PS256(u, m21, y);

        r = _mm256_min_ps(one, _mm256_max_ps(zero, r));
        g = _mm256_min_ps(one, _mm256_max_ps(zero, g));
        b = _mm256_min_ps(one, _mm256_max_ps(zero, b));

        r_lo = _mm256_castps256_ps128(r);
        r_hi = _mm256_extractf128_ps(r, 1);
        g_lo = _mm256_castps256_ps128(g);
        g_hi = _mm256_extractf128_ps(g, 1);
        b_lo = _mm256_castps256_ps128(b);
        b_hi = _mm256_extractf128_ps(b, 1);

        rg_lo = _mm_unpacklo_ps(r_lo, g_lo);
        rg_hi = _mm_unpackhi_ps(r_lo, g_lo);
        gb_lo = _mm_unpacklo_ps(g_lo, b_lo);
        gb_hi = _mm_unpackhi_ps(g_lo, b_lo);
        br_hi = _mm_unpackhi_ps(b_lo, r_lo);

        store0 = _mm_movelh_ps(rg_lo, b_lo);
        store0 = _mm_blend_ps(store0,
                              _mm_shuffle_ps(r_lo, r_lo,
                                             _MM_SHUFFLE(1, 1, 1, 1)),
                              0x8);
        store1 = _mm_shuffle_ps(gb_lo, rg_hi, 0x4E);
        store2 = _mm_shuffle_ps(br_hi, gb_hi, 0xEC);

        _mm_storeu_ps(store, store0);
        _mm_storeu_ps(store + 4U, store1);
        _mm_storeu_ps(store + 8U, store2);

        rg_lo = _mm_unpacklo_ps(r_hi, g_hi);
        rg_hi = _mm_unpackhi_ps(r_hi, g_hi);
        gb_lo = _mm_unpacklo_ps(g_hi, b_hi);
        gb_hi = _mm_unpackhi_ps(g_hi, b_hi);
        br_hi = _mm_unpackhi_ps(b_hi, r_hi);

        store3 = _mm_movelh_ps(rg_lo, b_hi);
        store3 = _mm_blend_ps(store3,
                              _mm_shuffle_ps(r_hi, r_hi,
                                             _MM_SHUFFLE(1, 1, 1, 1)),
                              0x8);
        store4 = _mm_shuffle_ps(gb_lo, rg_hi, 0x4E);
        store5 = _mm_shuffle_ps(br_hi, gb_hi, 0xEC);

        _mm_storeu_ps(store + 12U, store3);
        _mm_storeu_ps(store + 16U, store4);
        _mm_storeu_ps(store + 20U, store5);
    }

    return processed;
}
#endif

#if defined(SIXEL_USE_SSE2)
static size_t
sixel_yuv_to_linear_sse2(float *pixels, size_t pixel_total)
{
    size_t index;
    size_t processed;
    const __m128 zero = _mm_set1_ps(0.0f);
    const __m128 one = _mm_set1_ps(1.0f);
    const __m128 y_min = _mm_set1_ps(0.0f);
    const __m128 y_max = _mm_set1_ps(1.0f);
    const __m128 u_min = _mm_set1_ps(-SIXEL_YUV_U_MAX);
    const __m128 u_max = _mm_set1_ps(SIXEL_YUV_U_MAX);
    const __m128 v_min = _mm_set1_ps(-SIXEL_YUV_V_MAX);
    const __m128 v_max = _mm_set1_ps(SIXEL_YUV_V_MAX);
    const __m128 m10 = _mm_set1_ps(-0.39465f);
    const __m128 m11 = _mm_set1_ps(-0.58060f);
    const __m128 m21 = _mm_set1_ps(2.03211f);
    const __m128 m20 = _mm_set1_ps(1.13983f);
    float r_out[4];
    float g_out[4];
    float b_out[4];

    processed = pixel_total - (pixel_total % 4U);
    for (index = 0U; index < processed; index += 4U) {
        float *base;
        __m128 y;
        __m128 u;
        __m128 v;
        __m128 r;
        __m128 g;
        __m128 b;

        base = pixels + index * 3U;

        y = _mm_set_ps(base[9], base[6], base[3], base[0]);
        u = _mm_set_ps(base[10], base[7], base[4], base[1]);
        v = _mm_set_ps(base[11], base[8], base[5], base[2]);

        y = _mm_min_ps(y_max, _mm_max_ps(y_min, y));
        u = _mm_min_ps(u_max, _mm_max_ps(u_min, u));
        v = _mm_min_ps(v_max, _mm_max_ps(v_min, v));

        r = _mm_add_ps(_mm_mul_ps(v, m20), y);
        g = _mm_add_ps(_mm_mul_ps(v, m11), y);
        g = _mm_add_ps(g, _mm_mul_ps(u, m10));
        b = _mm_add_ps(_mm_mul_ps(u, m21), y);

        r = _mm_min_ps(one, _mm_max_ps(zero, r));
        g = _mm_min_ps(one, _mm_max_ps(zero, g));
        b = _mm_min_ps(one, _mm_max_ps(zero, b));

        _mm_storeu_ps(r_out, r);
        _mm_storeu_ps(g_out, g);
        _mm_storeu_ps(b_out, b);

        base[0] = r_out[0];
        base[1] = g_out[0];
        base[2] = b_out[0];
        base[3] = r_out[1];
        base[4] = g_out[1];
        base[5] = b_out[1];
        base[6] = r_out[2];
        base[7] = g_out[2];
        base[8] = b_out[2];
        base[9] = r_out[3];
        base[10] = g_out[3];
        base[11] = b_out[3];
    }

    return processed;
}
#endif

#if defined(SIXEL_USE_NEON)
static size_t
sixel_yuv_to_linear_neon(float *pixels, size_t pixel_total)
{
    size_t index;
    size_t processed;
    const float32x4_t zero = vdupq_n_f32(0.0f);
    const float32x4_t one = vdupq_n_f32(1.0f);
    const float32x4_t y_min = vdupq_n_f32(0.0f);
    const float32x4_t y_max = vdupq_n_f32(1.0f);
    const float32x4_t u_min = vdupq_n_f32(-SIXEL_YUV_U_MAX);
    const float32x4_t u_max = vdupq_n_f32(SIXEL_YUV_U_MAX);
    const float32x4_t v_min = vdupq_n_f32(-SIXEL_YUV_V_MAX);
    const float32x4_t v_max = vdupq_n_f32(SIXEL_YUV_V_MAX);
    const float32x4_t m10 = vdupq_n_f32(-0.39465f);
    const float32x4_t m11 = vdupq_n_f32(-0.58060f);
    const float32x4_t m21 = vdupq_n_f32(2.03211f);
    const float32x4_t m20 = vdupq_n_f32(1.13983f);

    processed = pixel_total - (pixel_total % 4U);
    for (index = 0U; index < processed; index += 4U) {
        float32x4x3_t yuv;
        float32x4_t r;
        float32x4_t g;
        float32x4_t b;
        float32x4x3_t out;

        yuv = vld3q_f32(pixels + index * 3U);

        yuv.val[0] = vminq_f32(y_max, vmaxq_f32(y_min, yuv.val[0]));
        yuv.val[1] = vminq_f32(u_max, vmaxq_f32(u_min, yuv.val[1]));
        yuv.val[2] = vminq_f32(v_max, vmaxq_f32(v_min, yuv.val[2]));

        r = vmlaq_f32(yuv.val[0], yuv.val[2], m20);
        g = vmlaq_f32(yuv.val[0], yuv.val[2], m11);
        g = vmlaq_f32(g, yuv.val[1], m10);
        b = vmlaq_f32(yuv.val[0], yuv.val[1], m21);

        r = vminq_f32(one, vmaxq_f32(zero, r));
        g = vminq_f32(one, vmaxq_f32(zero, g));
        b = vminq_f32(one, vmaxq_f32(zero, b));

        out.val[0] = r;
        out.val[1] = g;
        out.val[2] = b;

        vst3q_f32(pixels + index * 3U, out);
    }

    return processed;
}
#endif
#if defined(SIXEL_USE_AVX512) && defined(__AVX512F__) && \
        defined(__AVX512BW__)
static SIXEL_TARGET_AVX512 size_t
sixel_smptec_to_linear_avx512(float *pixels, size_t pixel_total)
{
    size_t index;
    size_t processed;
    const __m512 zero = _mm512_set1_ps(0.0f);
    const __m512 one = _mm512_set1_ps(1.0f);
    const __m512 m00 = _mm512_set1_ps(
        sixel_linear_smptec_to_srgb_matrix_f32[0][0]);
    const __m512 m01 = _mm512_set1_ps(
        sixel_linear_smptec_to_srgb_matrix_f32[0][1]);
    const __m512 m02 = _mm512_set1_ps(
        sixel_linear_smptec_to_srgb_matrix_f32[0][2]);
    const __m512 m10 = _mm512_set1_ps(
        sixel_linear_smptec_to_srgb_matrix_f32[1][0]);
    const __m512 m11 = _mm512_set1_ps(
        sixel_linear_smptec_to_srgb_matrix_f32[1][1]);
    const __m512 m12 = _mm512_set1_ps(
        sixel_linear_smptec_to_srgb_matrix_f32[1][2]);
    const __m512 m20 = _mm512_set1_ps(
        sixel_linear_smptec_to_srgb_matrix_f32[2][0]);
    const __m512 m21 = _mm512_set1_ps(
        sixel_linear_smptec_to_srgb_matrix_f32[2][1]);
    const __m512 m22 = _mm512_set1_ps(
        sixel_linear_smptec_to_srgb_matrix_f32[2][2]);
    const __m512i idx_r = _mm512_setr_epi32(
        0, 12, 24, 36, 48, 60, 72, 84,
        96, 108, 120, 132, 144, 156, 168, 180);
    const __m512i idx_g = _mm512_setr_epi32(
        4, 16, 28, 40, 52, 64, 76, 88,
        100, 112, 124, 136, 148, 160, 172, 184);
    const __m512i idx_b = _mm512_setr_epi32(
        8, 20, 32, 44, 56, 68, 80, 92,
        104, 116, 128, 140, 152, 164, 176, 188);

    processed = pixel_total - (pixel_total % 16U);
    for (index = 0U; index < processed; index += 16U) {
        const char *base_char;
        const float *base;
        __m512 r;
        __m512 g;
        __m512 b;
        __m512 r_lin;
        __m512 g_lin;
        __m512 b_lin;

        base_char = (const char *)(pixels + index * 3U);
        base = (const float *)base_char;

        r = _mm512_i32gather_ps(idx_r, base, 1);
        g = _mm512_i32gather_ps(idx_g, base, 1);
        b = _mm512_i32gather_ps(idx_b, base, 1);

        r_lin = SIXEL_FMADD_PS512(g, m01, _mm512_mul_ps(r, m00));
        r_lin = SIXEL_FMADD_PS512(b, m02, r_lin);

        g_lin = SIXEL_FMADD_PS512(g, m11, _mm512_mul_ps(r, m10));
        g_lin = SIXEL_FMADD_PS512(b, m12, g_lin);

        b_lin = SIXEL_FMADD_PS512(g, m21, _mm512_mul_ps(r, m20));
        b_lin = SIXEL_FMADD_PS512(b, m22, b_lin);

        r_lin = _mm512_min_ps(one, _mm512_max_ps(zero, r_lin));
        g_lin = _mm512_min_ps(one, _mm512_max_ps(zero, g_lin));
        b_lin = _mm512_min_ps(one, _mm512_max_ps(zero, b_lin));

        _mm512_i32scatter_ps((float *)base, idx_r, r_lin, 1);
        _mm512_i32scatter_ps((float *)base, idx_g, g_lin, 1);
        _mm512_i32scatter_ps((float *)base, idx_b, b_lin, 1);
    }

    return processed;
}

static SIXEL_TARGET_AVX512 size_t
sixel_linear_to_smptec_avx512(float *pixels, size_t pixel_total)
{
    size_t index;
    size_t processed;
    const __m512 zero = _mm512_set1_ps(0.0f);
    const __m512 one = _mm512_set1_ps(1.0f);
    const __m512 m00 = _mm512_set1_ps(
        sixel_linear_srgb_to_smptec_matrix_f32[0][0]);
    const __m512 m01 = _mm512_set1_ps(
        sixel_linear_srgb_to_smptec_matrix_f32[0][1]);
    const __m512 m02 = _mm512_set1_ps(
        sixel_linear_srgb_to_smptec_matrix_f32[0][2]);
    const __m512 m10 = _mm512_set1_ps(
        sixel_linear_srgb_to_smptec_matrix_f32[1][0]);
    const __m512 m11 = _mm512_set1_ps(
        sixel_linear_srgb_to_smptec_matrix_f32[1][1]);
    const __m512 m12 = _mm512_set1_ps(
        sixel_linear_srgb_to_smptec_matrix_f32[1][2]);
    const __m512 m20 = _mm512_set1_ps(
        sixel_linear_srgb_to_smptec_matrix_f32[2][0]);
    const __m512 m21 = _mm512_set1_ps(
        sixel_linear_srgb_to_smptec_matrix_f32[2][1]);
    const __m512 m22 = _mm512_set1_ps(
        sixel_linear_srgb_to_smptec_matrix_f32[2][2]);
    const __m512i idx_r = _mm512_setr_epi32(
        0, 12, 24, 36, 48, 60, 72, 84,
        96, 108, 120, 132, 144, 156, 168, 180);
    const __m512i idx_g = _mm512_setr_epi32(
        4, 16, 28, 40, 52, 64, 76, 88,
        100, 112, 124, 136, 148, 160, 172, 184);
    const __m512i idx_b = _mm512_setr_epi32(
        8, 20, 32, 44, 56, 68, 80, 92,
        104, 116, 128, 140, 152, 164, 176, 188);

    processed = pixel_total - (pixel_total % 16U);
    for (index = 0U; index < processed; index += 16U) {
        const char *base_char;
        const float *base;
        __m512 r;
        __m512 g;
        __m512 b;
        __m512 sr;
        __m512 sg;
        __m512 sb;

        base_char = (const char *)(pixels + index * 3U);
        base = (const float *)base_char;

        r = _mm512_i32gather_ps(idx_r, base, 1);
        g = _mm512_i32gather_ps(idx_g, base, 1);
        b = _mm512_i32gather_ps(idx_b, base, 1);

        sr = SIXEL_FMADD_PS512(g, m01, _mm512_mul_ps(r, m00));
        sr = SIXEL_FMADD_PS512(b, m02, sr);

        sg = SIXEL_FMADD_PS512(g, m11, _mm512_mul_ps(r, m10));
        sg = SIXEL_FMADD_PS512(b, m12, sg);

        sb = SIXEL_FMADD_PS512(g, m21, _mm512_mul_ps(r, m20));
        sb = SIXEL_FMADD_PS512(b, m22, sb);

        sr = _mm512_min_ps(one, _mm512_max_ps(zero, sr));
        sg = _mm512_min_ps(one, _mm512_max_ps(zero, sg));
        sb = _mm512_min_ps(one, _mm512_max_ps(zero, sb));

        _mm512_i32scatter_ps((float *)base, idx_r, sr, 1);
        _mm512_i32scatter_ps((float *)base, idx_g, sg, 1);
        _mm512_i32scatter_ps((float *)base, idx_b, sb, 1);
    }

    return processed;
}
#endif

#if defined(SIXEL_USE_AVX2) && defined(__AVX2__)
static SIXEL_TARGET_AVX2 size_t
sixel_smptec_to_linear_avx2(float *pixels, size_t pixel_total)
{
    size_t index;
    size_t processed;
    const __m256 zero = _mm256_set1_ps(0.0f);
    const __m256 one = _mm256_set1_ps(1.0f);
    const __m256 m00 = _mm256_set1_ps(
        sixel_linear_smptec_to_srgb_matrix_f32[0][0]);
    const __m256 m01 = _mm256_set1_ps(
        sixel_linear_smptec_to_srgb_matrix_f32[0][1]);
    const __m256 m02 = _mm256_set1_ps(
        sixel_linear_smptec_to_srgb_matrix_f32[0][2]);
    const __m256 m10 = _mm256_set1_ps(
        sixel_linear_smptec_to_srgb_matrix_f32[1][0]);
    const __m256 m11 = _mm256_set1_ps(
        sixel_linear_smptec_to_srgb_matrix_f32[1][1]);
    const __m256 m12 = _mm256_set1_ps(
        sixel_linear_smptec_to_srgb_matrix_f32[1][2]);
    const __m256 m20 = _mm256_set1_ps(
        sixel_linear_smptec_to_srgb_matrix_f32[2][0]);
    const __m256 m21 = _mm256_set1_ps(
        sixel_linear_smptec_to_srgb_matrix_f32[2][1]);
    const __m256 m22 = _mm256_set1_ps(
        sixel_linear_smptec_to_srgb_matrix_f32[2][2]);
    const __m256i idx_r = _mm256_setr_epi32(
        0, 12, 24, 36, 48, 60, 72, 84);
    const __m256i idx_g = _mm256_setr_epi32(
        4, 16, 28, 40, 52, 64, 76, 88);
    const __m256i idx_b = _mm256_setr_epi32(
        8, 20, 32, 44, 56, 68, 80, 92);
    const char *base_char;
    const float *base;
    float *store;
    __m256 r;
    __m256 g;
    __m256 b;
    __m256 r_lin;
    __m256 g_lin;
    __m256 b_lin;
    __m128 sr_lo;
    __m128 sg_lo;
    __m128 sb_lo;
    __m128 sr_hi;
    __m128 sg_hi;
    __m128 sb_hi;
    __m128 rg_lo;
    __m128 rg_hi;
    __m128 gb_lo;
    __m128 gb_hi;
    __m128 br_hi;
    __m128 store0;
    __m128 store1;
    __m128 store2;
    __m128 store3;
    __m128 store4;
    __m128 store5;

    processed = pixel_total - (pixel_total % 8U);
    for (index = 0U; index < processed; index += 8U) {
        base_char = (const char *)(pixels + index * 3U);
        base = (const float *)base_char;
        store = (float *)base_char;

        r = _mm256_i32gather_ps(base, idx_r, 1);
        g = _mm256_i32gather_ps(base, idx_g, 1);
        b = _mm256_i32gather_ps(base, idx_b, 1);

        r_lin = SIXEL_FMADD_PS256(g, m01, _mm256_mul_ps(r, m00));
        r_lin = SIXEL_FMADD_PS256(b, m02, r_lin);

        g_lin = SIXEL_FMADD_PS256(g, m11, _mm256_mul_ps(r, m10));
        g_lin = SIXEL_FMADD_PS256(b, m12, g_lin);

        b_lin = SIXEL_FMADD_PS256(g, m21, _mm256_mul_ps(r, m20));
        b_lin = SIXEL_FMADD_PS256(b, m22, b_lin);

        r_lin = _mm256_min_ps(one, _mm256_max_ps(zero, r_lin));
        g_lin = _mm256_min_ps(one, _mm256_max_ps(zero, g_lin));
        b_lin = _mm256_min_ps(one, _mm256_max_ps(zero, b_lin));

        sr_lo = _mm256_castps256_ps128(r_lin);
        sr_hi = _mm256_extractf128_ps(r_lin, 1);
        sg_lo = _mm256_castps256_ps128(g_lin);
        sg_hi = _mm256_extractf128_ps(g_lin, 1);
        sb_lo = _mm256_castps256_ps128(b_lin);
        sb_hi = _mm256_extractf128_ps(b_lin, 1);

        /*
         * Re-interleave SoA vectors back into AoS layout using shuffle
         * and blend only. This avoids the temporary scalar arrays and
         * keeps the writeback path entirely vectorized.
         */
        rg_lo = _mm_unpacklo_ps(sr_lo, sg_lo);
        rg_hi = _mm_unpackhi_ps(sr_lo, sg_lo);
        gb_lo = _mm_unpacklo_ps(sg_lo, sb_lo);
        gb_hi = _mm_unpackhi_ps(sg_lo, sb_lo);
        br_hi = _mm_unpackhi_ps(sb_lo, sr_lo);

        store0 = _mm_movelh_ps(rg_lo, sb_lo);
        store0 = _mm_blend_ps(store0,
                              _mm_shuffle_ps(sr_lo, sr_lo,
                                             _MM_SHUFFLE(1, 1, 1, 1)),
                              0x8);
        store1 = _mm_shuffle_ps(gb_lo, rg_hi, 0x4E);
        store2 = _mm_shuffle_ps(br_hi, gb_hi, 0xEC);

        _mm_storeu_ps(store, store0);
        _mm_storeu_ps(store + 4U, store1);
        _mm_storeu_ps(store + 8U, store2);

        rg_lo = _mm_unpacklo_ps(sr_hi, sg_hi);
        rg_hi = _mm_unpackhi_ps(sr_hi, sg_hi);
        gb_lo = _mm_unpacklo_ps(sg_hi, sb_hi);
        gb_hi = _mm_unpackhi_ps(sg_hi, sb_hi);
        br_hi = _mm_unpackhi_ps(sb_hi, sr_hi);

        store3 = _mm_movelh_ps(rg_lo, sb_hi);
        store3 = _mm_blend_ps(store3,
                              _mm_shuffle_ps(sr_hi, sr_hi,
                                             _MM_SHUFFLE(1, 1, 1, 1)),
                              0x8);
        store4 = _mm_shuffle_ps(gb_lo, rg_hi, 0x4E);
        store5 = _mm_shuffle_ps(br_hi, gb_hi, 0xEC);

        _mm_storeu_ps(store + 12U, store3);
        _mm_storeu_ps(store + 16U, store4);
        _mm_storeu_ps(store + 20U, store5);
    }

    return processed;
}

static SIXEL_TARGET_AVX2 size_t
sixel_linear_to_smptec_avx2(float *pixels, size_t pixel_total)
{
    size_t index;
    size_t processed;
    const __m256 zero = _mm256_set1_ps(0.0f);
    const __m256 one = _mm256_set1_ps(1.0f);
    const __m256 m00 = _mm256_set1_ps(
        sixel_linear_srgb_to_smptec_matrix_f32[0][0]);
    const __m256 m01 = _mm256_set1_ps(
        sixel_linear_srgb_to_smptec_matrix_f32[0][1]);
    const __m256 m02 = _mm256_set1_ps(
        sixel_linear_srgb_to_smptec_matrix_f32[0][2]);
    const __m256 m10 = _mm256_set1_ps(
        sixel_linear_srgb_to_smptec_matrix_f32[1][0]);
    const __m256 m11 = _mm256_set1_ps(
        sixel_linear_srgb_to_smptec_matrix_f32[1][1]);
    const __m256 m12 = _mm256_set1_ps(
        sixel_linear_srgb_to_smptec_matrix_f32[1][2]);
    const __m256 m20 = _mm256_set1_ps(
        sixel_linear_srgb_to_smptec_matrix_f32[2][0]);
    const __m256 m21 = _mm256_set1_ps(
        sixel_linear_srgb_to_smptec_matrix_f32[2][1]);
    const __m256 m22 = _mm256_set1_ps(
        sixel_linear_srgb_to_smptec_matrix_f32[2][2]);
    const __m256i idx_r = _mm256_setr_epi32(
        0, 12, 24, 36, 48, 60, 72, 84);
    const __m256i idx_g = _mm256_setr_epi32(
        4, 16, 28, 40, 52, 64, 76, 88);
    const __m256i idx_b = _mm256_setr_epi32(
        8, 20, 32, 44, 56, 68, 80, 92);
    const char *base_char;
    const float *base;
    float *store;
    __m256 r;
    __m256 g;
    __m256 b;
    __m256 sr;
    __m256 sg;
    __m256 sb;
    __m128 sr_lo;
    __m128 sg_lo;
    __m128 sb_lo;
    __m128 sr_hi;
    __m128 sg_hi;
    __m128 sb_hi;
    __m128 rg_lo;
    __m128 rg_hi;
    __m128 gb_lo;
    __m128 gb_hi;
    __m128 br_hi;
    __m128 store0;
    __m128 store1;
    __m128 store2;
    __m128 store3;
    __m128 store4;
    __m128 store5;

    processed = pixel_total - (pixel_total % 8U);
    for (index = 0U; index < processed; index += 8U) {
        base_char = (const char *)(pixels + index * 3U);
        base = (const float *)base_char;
        store = (float *)base_char;

        r = _mm256_i32gather_ps(base, idx_r, 1);
        g = _mm256_i32gather_ps(base, idx_g, 1);
        b = _mm256_i32gather_ps(base, idx_b, 1);

        sr = SIXEL_FMADD_PS256(g, m01, _mm256_mul_ps(r, m00));
        sr = SIXEL_FMADD_PS256(b, m02, sr);

        sg = SIXEL_FMADD_PS256(g, m11, _mm256_mul_ps(r, m10));
        sg = SIXEL_FMADD_PS256(b, m12, sg);

        sb = SIXEL_FMADD_PS256(g, m21, _mm256_mul_ps(r, m20));
        sb = SIXEL_FMADD_PS256(b, m22, sb);

        sr = _mm256_min_ps(one, _mm256_max_ps(zero, sr));
        sg = _mm256_min_ps(one, _mm256_max_ps(zero, sg));
        sb = _mm256_min_ps(one, _mm256_max_ps(zero, sb));

        sr_lo = _mm256_castps256_ps128(sr);
        sr_hi = _mm256_extractf128_ps(sr, 1);
        sg_lo = _mm256_castps256_ps128(sg);
        sg_hi = _mm256_extractf128_ps(sg, 1);
        sb_lo = _mm256_castps256_ps128(sb);
        sb_hi = _mm256_extractf128_ps(sb, 1);

        /*
         * Re-pack SoA lanes into AoS using shuffle and blend to keep
         * the entire writeback path vectorized without temporary
         * scalar buffers.
         */
        rg_lo = _mm_unpacklo_ps(sr_lo, sg_lo);
        rg_hi = _mm_unpackhi_ps(sr_lo, sg_lo);
        gb_lo = _mm_unpacklo_ps(sg_lo, sb_lo);
        gb_hi = _mm_unpackhi_ps(sg_lo, sb_lo);
        br_hi = _mm_unpackhi_ps(sb_lo, sr_lo);

        store0 = _mm_movelh_ps(rg_lo, sb_lo);
        store0 = _mm_blend_ps(store0,
                              _mm_shuffle_ps(sr_lo, sr_lo,
                                             _MM_SHUFFLE(1, 1, 1, 1)),
                              0x8);
        store1 = _mm_shuffle_ps(gb_lo, rg_hi, 0x4E);
        store2 = _mm_shuffle_ps(br_hi, gb_hi, 0xEC);

        _mm_storeu_ps(store, store0);
        _mm_storeu_ps(store + 4U, store1);
        _mm_storeu_ps(store + 8U, store2);

        rg_lo = _mm_unpacklo_ps(sr_hi, sg_hi);
        rg_hi = _mm_unpackhi_ps(sr_hi, sg_hi);
        gb_lo = _mm_unpacklo_ps(sg_hi, sb_hi);
        gb_hi = _mm_unpackhi_ps(sg_hi, sb_hi);
        br_hi = _mm_unpackhi_ps(sb_hi, sr_hi);

        store3 = _mm_movelh_ps(rg_lo, sb_hi);
        store3 = _mm_blend_ps(store3,
                              _mm_shuffle_ps(sr_hi, sr_hi,
                                             _MM_SHUFFLE(1, 1, 1, 1)),
                              0x8);
        store4 = _mm_shuffle_ps(gb_lo, rg_hi, 0x4E);
        store5 = _mm_shuffle_ps(br_hi, gb_hi, 0xEC);

        _mm_storeu_ps(store + 12U, store3);
        _mm_storeu_ps(store + 16U, store4);
        _mm_storeu_ps(store + 20U, store5);
    }

    return processed;
}
#endif

#if defined(SIXEL_USE_SSE2)
static size_t
sixel_smptec_to_linear_sse2(float *pixels, size_t pixel_total)
{
    size_t index;
    size_t processed;
    const __m128 zero = _mm_set1_ps(0.0f);
    const __m128 one = _mm_set1_ps(1.0f);
    const __m128 m00 = _mm_set1_ps(
        sixel_linear_smptec_to_srgb_matrix_f32[0][0]);
    const __m128 m01 = _mm_set1_ps(
        sixel_linear_smptec_to_srgb_matrix_f32[0][1]);
    const __m128 m02 = _mm_set1_ps(
        sixel_linear_smptec_to_srgb_matrix_f32[0][2]);
    const __m128 m10 = _mm_set1_ps(
        sixel_linear_smptec_to_srgb_matrix_f32[1][0]);
    const __m128 m11 = _mm_set1_ps(
        sixel_linear_smptec_to_srgb_matrix_f32[1][1]);
    const __m128 m12 = _mm_set1_ps(
        sixel_linear_smptec_to_srgb_matrix_f32[1][2]);
    const __m128 m20 = _mm_set1_ps(
        sixel_linear_smptec_to_srgb_matrix_f32[2][0]);
    const __m128 m21 = _mm_set1_ps(
        sixel_linear_smptec_to_srgb_matrix_f32[2][1]);
    const __m128 m22 = _mm_set1_ps(
        sixel_linear_smptec_to_srgb_matrix_f32[2][2]);
    float r_out[4];
    float g_out[4];
    float b_out[4];
    __m128 vec0;
    __m128 vec1;
    __m128 vec2;

    processed = pixel_total - (pixel_total % 4U);
    for (index = 0U; index < processed; index += 4U) {
        float *base;
        __m128 r;
        __m128 g;
        __m128 b;
        __m128 r_lin;
        __m128 g_lin;
        __m128 b_lin;

        base = pixels + index * 3U;

        /*
         * SSE2 lacks gathers, so load interleaved RGB triplets with
         * scalar addressing and pack them lane-wise. Lane order is
         * preserved (pixel0..pixel3) to reuse the scatter below.
         */
        r = _mm_set_ps(base[9], base[6], base[3], base[0]);
        g = _mm_set_ps(base[10], base[7], base[4], base[1]);
        b = _mm_set_ps(base[11], base[8], base[5], base[2]);

        r_lin = _mm_add_ps(_mm_mul_ps(r, m00), _mm_mul_ps(g, m01));
        r_lin = _mm_add_ps(r_lin, _mm_mul_ps(b, m02));

        g_lin = _mm_add_ps(_mm_mul_ps(r, m10), _mm_mul_ps(g, m11));
        g_lin = _mm_add_ps(g_lin, _mm_mul_ps(b, m12));

        b_lin = _mm_add_ps(_mm_mul_ps(r, m20), _mm_mul_ps(g, m21));
        b_lin = _mm_add_ps(b_lin, _mm_mul_ps(b, m22));

        r_lin = _mm_min_ps(one, _mm_max_ps(zero, r_lin));
        g_lin = _mm_min_ps(one, _mm_max_ps(zero, g_lin));
        b_lin = _mm_min_ps(one, _mm_max_ps(zero, b_lin));

        _mm_storeu_ps(r_out, r_lin);
        _mm_storeu_ps(g_out, g_lin);
        _mm_storeu_ps(b_out, b_lin);

        /*
         * Re-interleave SIMD lanes back into AoS layout with vector
         * stores to avoid the scalar scatter loop. Layout per store:
         *   vec0 -> [r0 g0 b0 r1]
         *   vec1 -> [g1 b1 r2 g2]
         *   vec2 -> [b2 r3 g3 b3]
         */
        vec0 = _mm_setr_ps(r_out[0], g_out[0], b_out[0], r_out[1]);
        vec1 = _mm_setr_ps(g_out[1], b_out[1], r_out[2], g_out[2]);
        vec2 = _mm_setr_ps(b_out[2], r_out[3], g_out[3], b_out[3]);

        _mm_storeu_ps(base, vec0);
        _mm_storeu_ps(base + 4U, vec1);
        _mm_storeu_ps(base + 8U, vec2);
    }

    return processed;
}

static size_t
sixel_linear_to_smptec_sse2(float *pixels, size_t pixel_total)
{
    size_t index;
    size_t processed;
    const __m128 zero = _mm_set1_ps(0.0f);
    const __m128 one = _mm_set1_ps(1.0f);
    const __m128 m00 = _mm_set1_ps(
        sixel_linear_srgb_to_smptec_matrix_f32[0][0]);
    const __m128 m01 = _mm_set1_ps(
        sixel_linear_srgb_to_smptec_matrix_f32[0][1]);
    const __m128 m02 = _mm_set1_ps(
        sixel_linear_srgb_to_smptec_matrix_f32[0][2]);
    const __m128 m10 = _mm_set1_ps(
        sixel_linear_srgb_to_smptec_matrix_f32[1][0]);
    const __m128 m11 = _mm_set1_ps(
        sixel_linear_srgb_to_smptec_matrix_f32[1][1]);
    const __m128 m12 = _mm_set1_ps(
        sixel_linear_srgb_to_smptec_matrix_f32[1][2]);
    const __m128 m20 = _mm_set1_ps(
        sixel_linear_srgb_to_smptec_matrix_f32[2][0]);
    const __m128 m21 = _mm_set1_ps(
        sixel_linear_srgb_to_smptec_matrix_f32[2][1]);
    const __m128 m22 = _mm_set1_ps(
        sixel_linear_srgb_to_smptec_matrix_f32[2][2]);
    float sr_out[4];
    float sg_out[4];
    float sb_out[4];
    __m128 vec0;
    __m128 vec1;
    __m128 vec2;

    processed = pixel_total - (pixel_total % 4U);
    for (index = 0U; index < processed; index += 4U) {
        float *base;
        __m128 r;
        __m128 g;
        __m128 b;
        __m128 sr;
        __m128 sg;
        __m128 sb;

        base = pixels + index * 3U;

        /*
         * Expand interleaved linear RGB into lane-aligned vectors to
         * reuse the same scatter order as the SMPTEC→linear path.
         */
        r = _mm_set_ps(base[9], base[6], base[3], base[0]);
        g = _mm_set_ps(base[10], base[7], base[4], base[1]);
        b = _mm_set_ps(base[11], base[8], base[5], base[2]);

        sr = _mm_add_ps(_mm_mul_ps(r, m00), _mm_mul_ps(g, m01));
        sr = _mm_add_ps(sr, _mm_mul_ps(b, m02));

        sg = _mm_add_ps(_mm_mul_ps(r, m10), _mm_mul_ps(g, m11));
        sg = _mm_add_ps(sg, _mm_mul_ps(b, m12));

        sb = _mm_add_ps(_mm_mul_ps(r, m20), _mm_mul_ps(g, m21));
        sb = _mm_add_ps(sb, _mm_mul_ps(b, m22));

        sr = _mm_min_ps(one, _mm_max_ps(zero, sr));
        sg = _mm_min_ps(one, _mm_max_ps(zero, sg));
        sb = _mm_min_ps(one, _mm_max_ps(zero, sb));

        _mm_storeu_ps(sr_out, sr);
        _mm_storeu_ps(sg_out, sg);
        _mm_storeu_ps(sb_out, sb);

        /*
         * Use three vector stores to re-pack SoA lanes into the AoS
         * buffer. This mirrors the SMPTEC→linear SSE2 path to keep
         * scatter symmetric and branch-free.
         */
        vec0 = _mm_setr_ps(sr_out[0], sg_out[0], sb_out[0], sr_out[1]);
        vec1 = _mm_setr_ps(sg_out[1], sb_out[1], sr_out[2], sg_out[2]);
        vec2 = _mm_setr_ps(sb_out[2], sr_out[3], sg_out[3], sb_out[3]);

        _mm_storeu_ps(base, vec0);
        _mm_storeu_ps(base + 4U, vec1);
        _mm_storeu_ps(base + 8U, vec2);
    }

    return processed;
}
#endif

#if defined(SIXEL_USE_NEON)
static size_t
sixel_smptec_to_linear_neon(float *pixels, size_t pixel_total)
{
    size_t index;
    size_t processed;
    const float32x4_t zero = vdupq_n_f32(0.0f);
    const float32x4_t one = vdupq_n_f32(1.0f);
    const float32x4_t m00 = vdupq_n_f32(
        sixel_linear_smptec_to_srgb_matrix_f32[0][0]);
    const float32x4_t m01 = vdupq_n_f32(
        sixel_linear_smptec_to_srgb_matrix_f32[0][1]);
    const float32x4_t m02 = vdupq_n_f32(
        sixel_linear_smptec_to_srgb_matrix_f32[0][2]);
    const float32x4_t m10 = vdupq_n_f32(
        sixel_linear_smptec_to_srgb_matrix_f32[1][0]);
    const float32x4_t m11 = vdupq_n_f32(
        sixel_linear_smptec_to_srgb_matrix_f32[1][1]);
    const float32x4_t m12 = vdupq_n_f32(
        sixel_linear_smptec_to_srgb_matrix_f32[1][2]);
    const float32x4_t m20 = vdupq_n_f32(
        sixel_linear_smptec_to_srgb_matrix_f32[2][0]);
    const float32x4_t m21 = vdupq_n_f32(
        sixel_linear_smptec_to_srgb_matrix_f32[2][1]);
    const float32x4_t m22 = vdupq_n_f32(
        sixel_linear_smptec_to_srgb_matrix_f32[2][2]);

    processed = pixel_total - (pixel_total % 4U);
    for (index = 0U; index < processed; index += 4U) {
        float32x4x3_t rgb;
        float32x4_t r_lin;
        float32x4_t g_lin;
        float32x4_t b_lin;
        float32x4x3_t out;

        rgb = vld3q_f32(pixels + index * 3U);

        r_lin = vmlaq_f32(vmulq_f32(rgb.val[0], m00),
                          rgb.val[1], m01);
        r_lin = vmlaq_f32(r_lin, rgb.val[2], m02);

        g_lin = vmlaq_f32(vmulq_f32(rgb.val[0], m10),
                          rgb.val[1], m11);
        g_lin = vmlaq_f32(g_lin, rgb.val[2], m12);

        b_lin = vmlaq_f32(vmulq_f32(rgb.val[0], m20),
                          rgb.val[1], m21);
        b_lin = vmlaq_f32(b_lin, rgb.val[2], m22);

        r_lin = vminq_f32(one, vmaxq_f32(zero, r_lin));
        g_lin = vminq_f32(one, vmaxq_f32(zero, g_lin));
        b_lin = vminq_f32(one, vmaxq_f32(zero, b_lin));

        out.val[0] = r_lin;
        out.val[1] = g_lin;
        out.val[2] = b_lin;

        vst3q_f32(pixels + index * 3U, out);
    }

    return processed;
}

static size_t
sixel_linear_to_smptec_neon(float *pixels, size_t pixel_total)
{
    size_t index;
    size_t processed;
    const float32x4_t zero = vdupq_n_f32(0.0f);
    const float32x4_t one = vdupq_n_f32(1.0f);
    const float32x4_t m00 = vdupq_n_f32(
        sixel_linear_srgb_to_smptec_matrix_f32[0][0]);
    const float32x4_t m01 = vdupq_n_f32(
        sixel_linear_srgb_to_smptec_matrix_f32[0][1]);
    const float32x4_t m02 = vdupq_n_f32(
        sixel_linear_srgb_to_smptec_matrix_f32[0][2]);
    const float32x4_t m10 = vdupq_n_f32(
        sixel_linear_srgb_to_smptec_matrix_f32[1][0]);
    const float32x4_t m11 = vdupq_n_f32(
        sixel_linear_srgb_to_smptec_matrix_f32[1][1]);
    const float32x4_t m12 = vdupq_n_f32(
        sixel_linear_srgb_to_smptec_matrix_f32[1][2]);
    const float32x4_t m20 = vdupq_n_f32(
        sixel_linear_srgb_to_smptec_matrix_f32[2][0]);
    const float32x4_t m21 = vdupq_n_f32(
        sixel_linear_srgb_to_smptec_matrix_f32[2][1]);
    const float32x4_t m22 = vdupq_n_f32(
        sixel_linear_srgb_to_smptec_matrix_f32[2][2]);

    processed = pixel_total - (pixel_total % 4U);
    for (index = 0U; index < processed; index += 4U) {
        float32x4x3_t rgb;
        float32x4_t sr;
        float32x4_t sg;
        float32x4_t sb;
        float32x4x3_t out;

        rgb = vld3q_f32(pixels + index * 3U);

        sr = vmlaq_f32(vmulq_f32(rgb.val[0], m00),
                       rgb.val[1], m01);
        sr = vmlaq_f32(sr, rgb.val[2], m02);

        sg = vmlaq_f32(vmulq_f32(rgb.val[0], m10),
                       rgb.val[1], m11);
        sg = vmlaq_f32(sg, rgb.val[2], m12);

        sb = vmlaq_f32(vmulq_f32(rgb.val[0], m20),
                       rgb.val[1], m21);
        sb = vmlaq_f32(sb, rgb.val[2], m22);

        sr = vminq_f32(one, vmaxq_f32(zero, sr));
        sg = vminq_f32(one, vmaxq_f32(zero, sg));
        sb = vminq_f32(one, vmaxq_f32(zero, sb));

        out.val[0] = sr;
        out.val[1] = sg;
        out.val[2] = sb;

        vst3q_f32(pixels + index * 3U, out);
    }

    return processed;
}
#endif

static void
sixel_smptec_to_linear_float_simd(float *pixels,
                                  size_t pixel_total,
                                  int simd_level)
{
    size_t processed;
    size_t index;

    processed = 0U;

    (void)simd_level;

#if defined(SIXEL_USE_AVX512) && defined(__AVX512F__) && \
        defined(__AVX512BW__)
    if (simd_level >= SIXEL_SIMD_LEVEL_AVX512) {
        processed = sixel_smptec_to_linear_avx512(pixels, pixel_total);
    }
#endif

#if defined(SIXEL_USE_AVX2) && defined(__AVX2__)
    if (processed < pixel_total &&
            simd_level >= SIXEL_SIMD_LEVEL_AVX2) {
        processed += sixel_smptec_to_linear_avx2(
            pixels + processed * 3U, pixel_total - processed);
    }
#endif

#if defined(SIXEL_USE_SSE2)
    if (processed < pixel_total &&
            simd_level >= SIXEL_SIMD_LEVEL_SSE2) {
        processed += sixel_smptec_to_linear_sse2(
            pixels + processed * 3U, pixel_total - processed);
    }
#endif

#if defined(SIXEL_USE_NEON)
    if (processed < pixel_total &&
            simd_level == SIXEL_SIMD_LEVEL_NEON) {
        processed += sixel_smptec_to_linear_neon(
            pixels + processed * 3U, pixel_total - processed);
    }
#endif

    for (index = processed; index < pixel_total; ++index) {
        float *pixel;

        pixel = pixels + index * 3U;
        sixel_smptec_to_linear_scalar(pixel + 0, pixel + 1, pixel + 2);
    }
}

static void
sixel_linear_to_smptec_float_simd(float *pixels,
                                  size_t pixel_total,
                                  int simd_level)
{
    size_t processed;
    size_t index;

    processed = 0U;

    (void)simd_level;

#if defined(SIXEL_USE_AVX512) && defined(__AVX512F__) && \
        defined(__AVX512BW__)
    if (simd_level >= SIXEL_SIMD_LEVEL_AVX512) {
        processed = sixel_linear_to_smptec_avx512(pixels, pixel_total);
    }
#endif

#if defined(SIXEL_USE_AVX2) && defined(__AVX2__)
    if (processed < pixel_total &&
            simd_level >= SIXEL_SIMD_LEVEL_AVX2) {
        processed += sixel_linear_to_smptec_avx2(
            pixels + processed * 3U, pixel_total - processed);
    }
#endif

#if defined(SIXEL_USE_SSE2)
    if (processed < pixel_total &&
            simd_level >= SIXEL_SIMD_LEVEL_SSE2) {
        processed += sixel_linear_to_smptec_sse2(
            pixels + processed * 3U, pixel_total - processed);
    }
#endif

#if defined(SIXEL_USE_NEON)
    if (processed < pixel_total &&
            simd_level == SIXEL_SIMD_LEVEL_NEON) {
        processed += sixel_linear_to_smptec_neon(
            pixels + processed * 3U, pixel_total - processed);
    }
#endif

    for (index = processed; index < pixel_total; ++index) {
        float *pixel;

        pixel = pixels + index * 3U;
        sixel_linear_to_smptec_scalar(pixel + 0, pixel + 1, pixel + 2);
    }
}

static void
sixel_linear_to_yuv_float_simd(float *pixels,
                               size_t pixel_total,
                               int simd_level)
{
    size_t processed;
    size_t index;

    processed = 0U;

    (void)simd_level;

#if defined(SIXEL_USE_AVX512) && defined(__AVX512F__) && \
        defined(__AVX512BW__)
    if (simd_level >= SIXEL_SIMD_LEVEL_AVX512) {
        processed = sixel_linear_to_yuv_avx512(pixels, pixel_total);
    }
#endif

#if defined(SIXEL_USE_AVX2) && defined(__AVX2__)
    if (processed < pixel_total &&
            simd_level >= SIXEL_SIMD_LEVEL_AVX2) {
        processed += sixel_linear_to_yuv_avx2(
            pixels + processed * 3U, pixel_total - processed);
    }
#endif

#if defined(SIXEL_USE_SSE2)
    if (processed < pixel_total &&
            simd_level >= SIXEL_SIMD_LEVEL_SSE2) {
        processed += sixel_linear_to_yuv_sse2(
            pixels + processed * 3U, pixel_total - processed);
    }
#endif

#if defined(SIXEL_USE_NEON)
    if (processed < pixel_total &&
            simd_level == SIXEL_SIMD_LEVEL_NEON) {
        processed += sixel_linear_to_yuv_neon(
            pixels + processed * 3U, pixel_total - processed);
    }
#endif

    for (index = processed; index < pixel_total; ++index) {
        double y_component;
        double u_component;
        double v_component;
        float *base;

        base = pixels + index * 3U;
        sixel_linear_to_yuv((double)base[0],
                            (double)base[1],
                            (double)base[2],
                            &y_component,
                            &u_component,
                            &v_component);
        base[0] = (float)y_component;
        base[1] = (float)u_component;
        base[2] = (float)v_component;
    }
}

static void
sixel_yuv_to_linear_float_simd(float *pixels,
                               size_t pixel_total,
                               int simd_level)
{
    size_t processed;
    size_t index;

    processed = 0U;

    (void)simd_level;

#if defined(SIXEL_USE_AVX512) && defined(__AVX512F__) && \
        defined(__AVX512BW__)
    if (simd_level >= SIXEL_SIMD_LEVEL_AVX512) {
        processed = sixel_yuv_to_linear_avx512(pixels, pixel_total);
    }
#endif

#if defined(SIXEL_USE_AVX2) && defined(__AVX2__)
    if (processed < pixel_total &&
            simd_level >= SIXEL_SIMD_LEVEL_AVX2) {
        processed += sixel_yuv_to_linear_avx2(
            pixels + processed * 3U, pixel_total - processed);
    }
#endif

#if defined(SIXEL_USE_SSE2)
    if (processed < pixel_total &&
            simd_level >= SIXEL_SIMD_LEVEL_SSE2) {
        processed += sixel_yuv_to_linear_sse2(
            pixels + processed * 3U, pixel_total - processed);
    }
#endif

#if defined(SIXEL_USE_NEON)
    if (processed < pixel_total &&
            simd_level == SIXEL_SIMD_LEVEL_NEON) {
        processed += sixel_yuv_to_linear_neon(
            pixels + processed * 3U, pixel_total - processed);
    }
#endif

    for (index = processed; index < pixel_total; ++index) {
        double r_lin;
        double g_lin;
        double b_lin;
        float *base;

        base = pixels + index * 3U;
        sixel_yuv_to_linear((double)base[0],
                            (double)base[1],
                            (double)base[2],
                            &r_lin,
                            &g_lin,
                            &b_lin);
        base[0] = (float)r_lin;
        base[1] = (float)g_lin;
        base[2] = (float)b_lin;
    }
}

static void
sixel_colorspace_init_tables(void)
{
    int i;
    double gamma_value;
    double linear_value;
    double converted;
    double smptec_value;
    double smptec_linear;

    if (tables_initialized) {
        return;
    }

    /*
     * Use the canonical sRGB transfer functions for the LUT to avoid
     * compounding approximation error.  The pow() calls are confined to
     * this one-time initialisation so the runtime conversion paths stay
     * unaffected while keeping the mapping faithful.
     */
    for (i = 0; i < SIXEL_COLORSPACE_LUT_SIZE; ++i) {
        gamma_value = (double)i / 255.0;
        if (gamma_value <= 0.04045) {
            converted = gamma_value / 12.92;
        } else {
            converted = pow((gamma_value + 0.055) / 1.055, 2.4);
        }
        gamma_to_linear_double_lut[i] = converted;
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

    /*
     * SMPTE-C curves are baked here so the runtime encoder/decoder can skip
     * repetitive pow() calls while keeping the mapping consistent with the
     * scalar reference formulas.
     */
    for (i = 0; i < SIXEL_COLORSPACE_LUT_SIZE; ++i) {
        smptec_value = (double)i / 255.0;
        smptec_linear = pow(smptec_value, 2.2);
        smptec_to_linear_double_lut[i] = sixel_clamp_unit(smptec_linear);
        converted = pow((double)i / 255.0, 1.0 / 2.2);
        linear_to_smptec_lut[i] =
            sixel_colorspace_clamp((int)(converted * 255.0 + 0.5));
    }

#if (defined(SIXEL_USE_AVX2) && defined(__AVX2__)) || \
        (defined(SIXEL_USE_AVX512) && defined(__AVX512F__) && \
         defined(__AVX512BW__))
    if (!sixel_avx_tables_initialized) {
        for (i = 0; i < SIXEL_COLORSPACE_LUT_SIZE; ++i) {
            sixel_avx_gamma_to_linear_lut32[i] =
                (uint32_t)gamma_to_linear_lut[i];
            sixel_avx_linear_to_gamma_lut32[i] =
                (uint32_t)linear_to_gamma_lut[i];
        }

        sixel_avx_tables_initialized = 1;
    }
#endif

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
    case SIXEL_COLORSPACE_CIELAB:
    {
        double L;
        double A;
        double B;

        L = (double)r8 / 255.0;
        A = sixel_cielab_decode_ab(g8);
        B = sixel_cielab_decode_ab(b8);
        sixel_cielab_to_linear(L, A, B, r_lin, g_lin, b_lin);
        break;
    }
    case SIXEL_COLORSPACE_DIN99D:
    {
        double L;
        double A;
        double B;

        L = (double)r8 / 255.0;
        A = sixel_din99d_decode_ab(g8);
        B = sixel_din99d_decode_ab(b8);
        sixel_din99d_to_linear(L, A, B, r_lin, g_lin, b_lin);
        break;
    }
    case SIXEL_COLORSPACE_YUV:
    {
        double y_component;
        double u_component;
        double v_component;

        y_component = (double)r8 / 255.0;
        u_component = sixel_yuv_decode_u(g8);
        v_component = sixel_yuv_decode_v(b8);
        sixel_yuv_to_linear(y_component,
                            u_component,
                            v_component,
                            r_lin,
                            g_lin,
                            b_lin);
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

/*
 * Float variant of the colorspace decoder so RGBFLOAT32 buffers can skip the
 * byte quantisation that the legacy helper performs.
 */
static void
sixel_decode_linear_from_colorspace_float(int colorspace,
                                          float r_value,
                                          float g_value,
                                          float b_value,
                                          double *r_lin,
                                          double *g_lin,
                                          double *b_lin)
{
    double r;
    double g;
    double b;

    r = (double)r_value;
    g = (double)g_value;
    b = (double)b_value;

    switch (colorspace) {
    case SIXEL_COLORSPACE_GAMMA:
        *r_lin = sixel_srgb_unit_to_linear(r);
        *g_lin = sixel_srgb_unit_to_linear(g);
        *b_lin = sixel_srgb_unit_to_linear(b);
        break;
    case SIXEL_COLORSPACE_LINEAR:
        *r_lin = sixel_clamp_unit(r);
        *g_lin = sixel_clamp_unit(g);
        *b_lin = sixel_clamp_unit(b);
        break;
    case SIXEL_COLORSPACE_OKLAB:
        sixel_oklab_to_linear(r, g, b, r_lin, g_lin, b_lin);
        break;
    case SIXEL_COLORSPACE_CIELAB:
        sixel_cielab_to_linear(r, g, b, r_lin, g_lin, b_lin);
        break;
    case SIXEL_COLORSPACE_DIN99D:
        sixel_din99d_to_linear(r, g, b, r_lin, g_lin, b_lin);
        break;
    case SIXEL_COLORSPACE_YUV:
        sixel_yuv_to_linear(sixel_clamp_unit(r),
                            sixel_yuv_clamp_u(g),
                            sixel_yuv_clamp_v(b),
                            r_lin,
                            g_lin,
                            b_lin);
        break;
    case SIXEL_COLORSPACE_SMPTEC:
    {
        double r_smptec;
        double g_smptec;
        double b_smptec;

        r_smptec = sixel_smptec_unit_to_linear(r);
        g_smptec = sixel_smptec_unit_to_linear(g);
        b_smptec = sixel_smptec_unit_to_linear(b);
        sixel_linear_smptec_to_srgb(r_smptec,
                                    g_smptec,
                                    b_smptec,
                                    r_lin,
                                    g_lin,
                                    b_lin);
        break;
    }
    default:
        *r_lin = sixel_clamp_unit(r);
        *g_lin = sixel_clamp_unit(g);
        *b_lin = sixel_clamp_unit(b);
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
    case SIXEL_COLORSPACE_CIELAB:
        sixel_linear_to_cielab(r_lin, g_lin, b_lin, &L, &A, &B);
        *r8 = sixel_cielab_encode_L(L);
        *g8 = sixel_cielab_encode_ab(A);
        *b8 = sixel_cielab_encode_ab(B);
        break;
    case SIXEL_COLORSPACE_DIN99D:
        sixel_linear_to_din99d(r_lin, g_lin, b_lin, &L, &A, &B);
        *r8 = sixel_din99d_encode_L(L);
        *g8 = sixel_din99d_encode_ab(A);
        *b8 = sixel_din99d_encode_ab(B);
        break;
    case SIXEL_COLORSPACE_YUV:
    {
        double y_component;
        double u_component;
        double v_component;

        sixel_linear_to_yuv(r_lin, g_lin, b_lin,
                            &y_component,
                            &u_component,
                            &v_component);
        *r8 = sixel_yuv_encode_y(y_component);
        *g8 = sixel_yuv_encode_u(u_component);
        *b8 = sixel_yuv_encode_v(v_component);
        break;
    }
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

static void
sixel_encode_linear_to_colorspace_float(int colorspace,
                                        double r_lin,
                                        double g_lin,
                                        double b_lin,
                                        float *r_value,
                                        float *g_value,
                                        float *b_value)
{
    double r;
    double g;
    double b;

    switch (colorspace) {
    case SIXEL_COLORSPACE_GAMMA:
        r = sixel_linear_to_srgb_unit(r_lin);
        g = sixel_linear_to_srgb_unit(g_lin);
        b = sixel_linear_to_srgb_unit(b_lin);
        break;
    case SIXEL_COLORSPACE_LINEAR:
        r = sixel_clamp_unit(r_lin);
        g = sixel_clamp_unit(g_lin);
        b = sixel_clamp_unit(b_lin);
        break;
    case SIXEL_COLORSPACE_OKLAB:
        sixel_linear_to_oklab(r_lin, g_lin, b_lin, &r, &g, &b);
        r = sixel_clamp_unit(r);
        g = sixel_oklab_clamp_ab(g);
        b = sixel_oklab_clamp_ab(b);
        break;
    case SIXEL_COLORSPACE_CIELAB:
        sixel_linear_to_cielab(r_lin, g_lin, b_lin, &r, &g, &b);
        r = sixel_clamp_unit(r);
        g = sixel_cielab_clamp_ab(g);
        b = sixel_cielab_clamp_ab(b);
        break;
    case SIXEL_COLORSPACE_DIN99D:
        sixel_linear_to_din99d(r_lin, g_lin, b_lin, &r, &g, &b);
        r = sixel_clamp_unit(r);
        g = sixel_din99d_clamp_ab_norm(g);
        b = sixel_din99d_clamp_ab_norm(b);
        break;
    case SIXEL_COLORSPACE_YUV:
        sixel_linear_to_yuv(r_lin, g_lin, b_lin, &r, &g, &b);
        break;
    case SIXEL_COLORSPACE_SMPTEC:
    {
        double r_smptec;
        double g_smptec;
        double b_smptec;

        sixel_linear_srgb_to_smptec(r_lin,
                                     g_lin,
                                     b_lin,
                                     &r_smptec,
                                     &g_smptec,
                                     &b_smptec);
        r = sixel_linear_to_smptec_unit(r_smptec);
        g = sixel_linear_to_smptec_unit(g_smptec);
        b = sixel_linear_to_smptec_unit(b_smptec);
        break;
    }
    default:
        r = sixel_clamp_unit(r_lin);
        g = sixel_clamp_unit(g_lin);
        b = sixel_clamp_unit(b_lin);
        break;
    }

    *r_value = (float)r;
    *g_value = (float)g;
    *b_value = (float)b;
}

typedef struct sixel_pixelformat_layout {
    int step;
    int index_r;
    int index_g;
    int index_b;
} sixel_pixelformat_layout_t;

static SIXELSTATUS
sixel_pixelformat_layout_init(int pixelformat,
                              sixel_pixelformat_layout_t *layout)
{
    SIXELSTATUS status;

    status = SIXEL_OK;
    switch (pixelformat) {
    case SIXEL_PIXELFORMAT_RGB888:
        layout->step = 3;
        layout->index_r = 0;
        layout->index_g = 1;
        layout->index_b = 2;
        break;
    case SIXEL_PIXELFORMAT_BGR888:
        layout->step = 3;
        layout->index_r = 2;
        layout->index_g = 1;
        layout->index_b = 0;
        break;
    case SIXEL_PIXELFORMAT_RGBA8888:
        layout->step = 4;
        layout->index_r = 0;
        layout->index_g = 1;
        layout->index_b = 2;
        break;
    case SIXEL_PIXELFORMAT_BGRA8888:
        layout->step = 4;
        layout->index_r = 2;
        layout->index_g = 1;
        layout->index_b = 0;
        break;
    case SIXEL_PIXELFORMAT_ARGB8888:
        layout->step = 4;
        layout->index_r = 1;
        layout->index_g = 2;
        layout->index_b = 3;
        break;
    case SIXEL_PIXELFORMAT_ABGR8888:
        layout->step = 4;
        layout->index_r = 3;
        layout->index_g = 2;
        layout->index_b = 1;
        break;
    case SIXEL_PIXELFORMAT_G8:
        layout->step = 1;
        layout->index_r = 0;
        layout->index_g = 0;
        layout->index_b = 0;
        break;
    case SIXEL_PIXELFORMAT_GA88:
        layout->step = 2;
        layout->index_r = 0;
        layout->index_g = 0;
        layout->index_b = 0;
        break;
    case SIXEL_PIXELFORMAT_AG88:
        layout->step = 2;
        layout->index_r = 1;
        layout->index_g = 1;
        layout->index_b = 1;
        break;
    default:
        status = SIXEL_BAD_INPUT;
        break;
    }

    return status;
}

static SIXELSTATUS
sixel_convert_pixels_via_linear_chunk(unsigned char *pixels,
                                      size_t pixel_total,
                                      sixel_pixelformat_layout_t const *layout,
                                      int colorspace_src,
                                      int colorspace_dst)
{
    size_t pixel_index;
    size_t offset;
    unsigned char *pr;
    unsigned char *pg;
    unsigned char *pb;
    double r_lin;
    double g_lin;
    double b_lin;

    for (pixel_index = 0U; pixel_index < pixel_total; ++pixel_index) {
        offset = pixel_index * (size_t)layout->step;
        pr = pixels + offset + (size_t)layout->index_r;
        pg = pixels + offset + (size_t)layout->index_g;
        pb = pixels + offset + (size_t)layout->index_b;

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

#if SIXEL_ENABLE_THREADS
typedef struct sixel_colorspace_parallel_context {
    float *pixels;
    size_t pixel_total;
    size_t chunk_pixels;
    int colorspace_src;
    int colorspace_dst;
    int simd_level;
    sixel_logger_t *logger;
} sixel_colorspace_parallel_context_t;

typedef struct sixel_colorspace_parallel_byte_context {
    unsigned char *pixels;
    size_t pixel_total;
    size_t chunk_pixels;
    sixel_pixelformat_layout_t layout;
    int colorspace_src;
    int colorspace_dst;
    sixel_logger_t *logger;
} sixel_colorspace_parallel_byte_context_t;

static int
sixel_colorspace_log_clamp(size_t value)
{
    if (value > (size_t)INT_MAX) {
        return INT_MAX;
    }

    return (int)value;
}

/*
 * Allow deployments to defer thread fan-out on tiny buffers via
 * SIXEL_COLORSPACE_PARALLEL_MIN_PIXELS. Defaults to 65537 pixels so the
 * colorspace conversion waits for moderately sized frames before fanning
 * out, but callers can override the behavior through the environment.
 */
static size_t
sixel_colorspace_parallel_min_pixels(void)
{
    static int initialized = 0;
    static size_t threshold = 65537;
    char const *text;
    char *endptr;
    unsigned long long parsed;

    if (initialized) {
        return threshold;
    }

    initialized = 1;
    text = sixel_compat_getenv("SIXEL_COLORSPACE_PARALLEL_MIN_PIXELS");
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
sixel_colorspace_parallel_worker_bytes(tp_job_t job,
                                       void *userdata,
                                       void *workspace)
{
    size_t start;
    size_t remaining;
    size_t end;
    sixel_colorspace_parallel_byte_context_t *ctx;
    sixel_logger_t *logger;
    int start_row;
    int end_row;
    int status;

    (void)workspace;

    ctx = (sixel_colorspace_parallel_byte_context_t *)userdata;
    logger = NULL;
    start = 0U;
    remaining = 0U;
    end = 0U;
    start_row = 0;
    end_row = 0;
    status = SIXEL_OK;
    if (ctx == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (job.band_index < 0) {
        return SIXEL_BAD_ARGUMENT;
    }

    start = (size_t)job.band_index * ctx->chunk_pixels;
    if (start >= ctx->pixel_total) {
        return SIXEL_OK;
    }

    remaining = ctx->pixel_total - start;
    if (remaining > ctx->chunk_pixels) {
        remaining = ctx->chunk_pixels;
    }

    end = start + remaining;
    logger = ctx->logger;
    if (logger != NULL && logger->active) {
        start_row = sixel_colorspace_log_clamp(start);
        end_row = sixel_colorspace_log_clamp(end);
        sixel_logger_logf(logger,
                          "worker",
                          "colorspace",
                          "start",
                          job.band_index,
                          start_row,
                          start_row,
                          end_row,
                          start_row,
                          end_row,
                          "chunk=%zu", remaining);
    }

    status = sixel_convert_pixels_via_linear_chunk(
        ctx->pixels + start * (size_t)ctx->layout.step,
        remaining,
        &ctx->layout,
        ctx->colorspace_src,
        ctx->colorspace_dst);

    if (logger != NULL && logger->active) {
        sixel_logger_logf(logger,
                          "worker",
                          "colorspace",
                          "finish",
                          job.band_index,
                          end_row,
                          start_row,
                          end_row,
                          start_row,
                          end_row,
                          "status=%d", status);
    }

    return status;
}
#endif

static SIXELSTATUS
sixel_convert_pixels_via_linear(unsigned char *pixels,
                                size_t size,
                                int pixelformat,
                                int colorspace_src,
                                int colorspace_dst)
{
    size_t pixel_total;
    SIXELSTATUS status;
    sixel_pixelformat_layout_t layout;
#if SIXEL_ENABLE_THREADS
    size_t job_count;
    size_t chunk_pixels;
    sixel_colorspace_parallel_byte_context_t ctx;
    threadpool_t *pool;
    tp_job_t job;
    int threads;
    int queue_depth;
    size_t job_index;
    int rc;
    sixel_logger_t logger;
    sixel_logger_t *logger_ref;
#endif

    if (colorspace_src == colorspace_dst) {
        return SIXEL_OK;
    }

    status = sixel_pixelformat_layout_init(pixelformat, &layout);
    if (SIXEL_FAILED(status)) {
        return SIXEL_BAD_INPUT;
    }

    if (size % (size_t)layout.step != 0U) {
        return SIXEL_BAD_INPUT;
    }

    pixel_total = size / (size_t)layout.step;
    status = SIXEL_OK;

#if SIXEL_ENABLE_THREADS
    logger_ref = NULL;
    rc = SIXEL_RUNTIME_ERROR;
    /*
     * Keep byte and float conversions aligned: respect the same
     * SIXEL_COLORSPACE_PARALLEL_MIN_PIXELS threshold and emit logger events
     * when SIXEL_PARALLEL_LOG_PATH is configured so traces show which path a
     * frame took.
     */
    sixel_logger_init(&logger);
    (void)sixel_logger_prepare_env(&logger);
    if (logger.active) {
        logger_ref = &logger;
        sixel_logger_logf(logger_ref,
                          "controller",
                          "colorspace",
                          "configure",
                          -1,
                          -1,
                          0,
                          sixel_colorspace_log_clamp(pixel_total),
                          0,
                          sixel_colorspace_log_clamp(pixel_total),
                          "pixels=%zu", pixel_total);
    }

    if (pixel_total >= sixel_colorspace_parallel_min_pixels()) {
        threads = sixel_threads_resolve();
        if (threads > 1) {
            chunk_pixels = (pixel_total + (size_t)threads - 1U)
                / (size_t)threads;
            if (chunk_pixels == 0U) {
                chunk_pixels = pixel_total;
            }

            ctx.pixels = pixels;
            ctx.pixel_total = pixel_total;
            ctx.chunk_pixels = chunk_pixels;
            ctx.layout = layout;
            ctx.colorspace_src = colorspace_src;
            ctx.colorspace_dst = colorspace_dst;
            ctx.logger = logger_ref;

            queue_depth = threads * 3;
            job_count = (pixel_total + chunk_pixels - 1U) / chunk_pixels;
            if (queue_depth > (int)job_count) {
                queue_depth = (int)job_count;
            }
            if (queue_depth < 1) {
                queue_depth = 1;
            }

            if (logger_ref != NULL) {
                sixel_logger_logf(logger_ref,
                                  "controller",
                                  "colorspace",
                                  "start",
                                  -1,
                                  -1,
                                  0,
                                  sixel_colorspace_log_clamp(pixel_total),
                                  0,
                                  sixel_colorspace_log_clamp(pixel_total),
                                  "threads=%d chunk=%zu jobs=%zu",
                                  threads,
                                  chunk_pixels,
                                  job_count);
            }

            pool = threadpool_create(threads,
                                     queue_depth,
                                     0,
                                     sixel_colorspace_parallel_worker_bytes,
                                     &ctx,
                                     NULL);
            if (pool != NULL) {
                for (job_index = 0U; job_index < job_count; ++job_index) {
                    job.band_index = (int)job_index;
                    threadpool_push(pool, job);
                }

                threadpool_finish(pool);
                rc = threadpool_get_error(pool);
                threadpool_destroy(pool);

                if (rc == SIXEL_OK) {
                    if (logger_ref != NULL) {
                        sixel_logger_logf(
                            logger_ref,
                            "controller",
                            "colorspace",
                            "finish",
                            -1,
                            -1,
                            0,
                            sixel_colorspace_log_clamp(pixel_total),
                            0,
                            sixel_colorspace_log_clamp(pixel_total),
                            "parallel finish threads=%d", threads);
                    }
                    status = SIXEL_OK;
                    goto end;
                }
            }

            if (logger_ref != NULL) {
                sixel_logger_logf(logger_ref,
                                  "controller",
                                  "colorspace",
                                  "fallback",
                                  -1,
                                  -1,
                                  0,
                                  sixel_colorspace_log_clamp(pixel_total),
                                  0,
                                  sixel_colorspace_log_clamp(pixel_total),
                                  "threadpool fallback rc=%d", rc);
            }
        } else if (logger_ref != NULL) {
            sixel_logger_logf(logger_ref,
                              "controller",
                              "colorspace",
                              "fallback",
                              -1,
                              -1,
                              0,
                              sixel_colorspace_log_clamp(pixel_total),
                              0,
                              sixel_colorspace_log_clamp(pixel_total),
                              "threads=%d", threads);
        }
    } else if (logger_ref != NULL) {
        sixel_logger_logf(logger_ref,
                          "controller",
                          "colorspace",
                          "fallback",
                          -1,
                          -1,
                          0,
                          sixel_colorspace_log_clamp(pixel_total),
                          0,
                          sixel_colorspace_log_clamp(pixel_total),
                          "below threshold=%zu",
                          sixel_colorspace_parallel_min_pixels());
    }
#endif

#if SIXEL_ENABLE_THREADS
    if (logger_ref != NULL) {
        sixel_logger_logf(logger_ref,
                          "worker",
                          "colorspace",
                          "start",
                          0,
                          0,
                          0,
                          sixel_colorspace_log_clamp(pixel_total),
                          0,
                          sixel_colorspace_log_clamp(pixel_total),
                          "serial chunk size=%zu", pixel_total);
    }
#endif

    status = sixel_convert_pixels_via_linear_chunk(pixels,
                                                   pixel_total,
                                                   &layout,
                                                   colorspace_src,
                                                   colorspace_dst);

#if SIXEL_ENABLE_THREADS
    if (logger_ref != NULL) {
        sixel_logger_logf(logger_ref,
                          "worker",
                          "colorspace",
                          "finish",
                          0,
                          sixel_colorspace_log_clamp(pixel_total),
                          0,
                          sixel_colorspace_log_clamp(pixel_total),
                          0,
                          sixel_colorspace_log_clamp(pixel_total),
                          "serial status=%d", status);
    }

end:
    sixel_logger_close(&logger);
#endif

    return status;
}

/*
 * Convert RGBFLOAT32 buffers in-place by round-tripping through linear space.
 * The general path keeps double intermediates for OKLab/CIELAB precision, and
 * the SMPTEC↔linear pair is specialized with SIMD float math to accelerate the
 * matrix multiply without changing the storage layout.
 */
static SIXELSTATUS
sixel_convert_pixels_via_linear_float_chunk(float *pixels,
                                            size_t pixel_total,
                                            int colorspace_src,
                                            int colorspace_dst,
                                            int simd_level)
{
    size_t index;
    size_t base;
    double r_lin;
    double g_lin;
    double b_lin;
    float *pr;
    float *pg;
    float *pb;

    if (colorspace_src == colorspace_dst) {
        return SIXEL_OK;
    }

    if (colorspace_src == SIXEL_COLORSPACE_SMPTEC &&
            colorspace_dst == SIXEL_COLORSPACE_LINEAR) {
        sixel_smptec_to_linear_float_simd(pixels, pixel_total, simd_level);
        return SIXEL_OK;
    }

    if (colorspace_src == SIXEL_COLORSPACE_LINEAR &&
            colorspace_dst == SIXEL_COLORSPACE_SMPTEC) {
        sixel_linear_to_smptec_float_simd(pixels, pixel_total, simd_level);
        return SIXEL_OK;
    }

    if (colorspace_src == SIXEL_COLORSPACE_LINEAR &&
            colorspace_dst == SIXEL_COLORSPACE_YUV) {
        sixel_linear_to_yuv_float_simd(pixels, pixel_total, simd_level);
        return SIXEL_OK;
    }

    if (colorspace_src == SIXEL_COLORSPACE_YUV &&
            colorspace_dst == SIXEL_COLORSPACE_LINEAR) {
        sixel_yuv_to_linear_float_simd(pixels, pixel_total, simd_level);
        return SIXEL_OK;
    }

    for (index = 0U; index < pixel_total; ++index) {
        base = index * 3U;
        pr = pixels + base + 0U;
        pg = pixels + base + 1U;
        pb = pixels + base + 2U;

        sixel_decode_linear_from_colorspace_float(colorspace_src,
                                                  *pr,
                                                  *pg,
                                                  *pb,
                                                  &r_lin,
                                                  &g_lin,
                                                  &b_lin);

        sixel_encode_linear_to_colorspace_float(colorspace_dst,
                                                r_lin,
                                                g_lin,
                                                b_lin,
                                                pr,
                                                pg,
                                                pb);
    }

    return SIXEL_OK;
}

#if SIXEL_ENABLE_THREADS
/*
 * Worker slices the pixel array into fixed chunks to keep writeback ranges
 * disjoint. Each job reuses the same SIMD level decision to avoid repeated
 * CPU feature detection.
 */
static int
sixel_colorspace_parallel_worker(tp_job_t job,
                                 void *userdata,
                                 void *workspace)
{
    sixel_colorspace_parallel_context_t *ctx;
    sixel_logger_t *logger;
    size_t start;
    size_t remaining;
    size_t end;
    int status;
    int start_row;
    int end_row;

    (void)workspace;

    ctx = (sixel_colorspace_parallel_context_t *)userdata;
    logger = NULL;
    start = 0U;
    remaining = 0U;
    end = 0U;
    status = SIXEL_OK;
    start_row = 0;
    end_row = 0;
    if (ctx == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (job.band_index < 0) {
        return SIXEL_BAD_ARGUMENT;
    }

    start = (size_t)job.band_index * ctx->chunk_pixels;
    if (start >= ctx->pixel_total) {
        return SIXEL_OK;
    }

    remaining = ctx->pixel_total - start;
    if (remaining > ctx->chunk_pixels) {
        remaining = ctx->chunk_pixels;
    }

    end = start + remaining;
    logger = ctx->logger;
    if (logger != NULL && logger->active) {
        start_row = sixel_colorspace_log_clamp(start);
        end_row = sixel_colorspace_log_clamp(end);
        sixel_logger_logf(logger,
                          "worker",
                          "colorspace",
                          "start",
                          job.band_index,
                          start_row,
                          start_row,
                          end_row,
                          start_row,
                          end_row,
                          "chunk=%zu", remaining);
    }

    status = sixel_convert_pixels_via_linear_float_chunk(
        ctx->pixels + start * 3U,
        remaining,
        ctx->colorspace_src,
        ctx->colorspace_dst,
        ctx->simd_level);

    if (logger != NULL && logger->active) {
        sixel_logger_logf(logger,
                          "worker",
                          "colorspace",
                          "finish",
                          job.band_index,
                          end_row,
                          start_row,
                          end_row,
                          start_row,
                          end_row,
                          "status=%d", status);
    }

    return status;
}
#endif

static SIXELSTATUS
sixel_convert_pixels_via_linear_float(float *pixels,
                                      size_t size,
                                      int colorspace_src,
                                      int colorspace_dst)
{
    size_t pixel_total;
    int simd_level;
    SIXELSTATUS status;
#if SIXEL_ENABLE_THREADS
    size_t job_count;
    size_t chunk_pixels;
    sixel_colorspace_parallel_context_t ctx;
    threadpool_t *pool;
    tp_job_t job;
    int threads;
    int queue_depth;
    size_t job_index;
    int rc;
    sixel_logger_t logger;
    sixel_logger_t *logger_ref;
#endif

    if (colorspace_src == colorspace_dst) {
        return SIXEL_OK;
    }

    if (size % (3U * sizeof(float)) != 0U) {
        return SIXEL_BAD_INPUT;
    }

    pixel_total = size / (3U * sizeof(float));
    simd_level = sixel_cpu_simd_level();
    status = SIXEL_OK;

#if SIXEL_ENABLE_THREADS
    logger_ref = NULL;
    rc = SIXEL_RUNTIME_ERROR;
    /*
     * Enable the timeline logger when SIXEL_PARALLEL_LOG_PATH points to a
     * writable sink. The controller emits a configure event even if the
     * call later falls back to the serial path so the timeline remains
     * continuous.
     */
    sixel_logger_init(&logger);
    (void)sixel_logger_prepare_env(&logger);
    if (logger.active) {
        logger_ref = &logger;
        sixel_logger_logf(logger_ref,
                          "controller",
                          "colorspace",
                          "configure",
                          -1,
                          -1,
                          0,
                          sixel_colorspace_log_clamp(pixel_total),
                          0,
                          sixel_colorspace_log_clamp(pixel_total),
                          "pixels=%zu simd=%d",
                          pixel_total,
                          simd_level);
    }

    if (pixel_total >= sixel_colorspace_parallel_min_pixels()) {
        threads = sixel_threads_resolve();
        if (threads > 1) {
            chunk_pixels = (pixel_total + (size_t)threads - 1U)
                / (size_t)threads;
            if (chunk_pixels == 0U) {
                chunk_pixels = pixel_total;
            }

            ctx.pixels = pixels;
            ctx.pixel_total = pixel_total;
            ctx.chunk_pixels = chunk_pixels;
            ctx.colorspace_src = colorspace_src;
            ctx.colorspace_dst = colorspace_dst;
            ctx.simd_level = simd_level;
            ctx.logger = logger_ref;

            queue_depth = threads * 3;
            job_count = (pixel_total + chunk_pixels - 1U) / chunk_pixels;
            if (queue_depth > (int)job_count) {
                queue_depth = (int)job_count;
            }
            if (queue_depth < 1) {
                queue_depth = 1;
            }

            if (logger_ref != NULL) {
                sixel_logger_logf(logger_ref,
                                  "controller",
                                  "colorspace",
                                  "start",
                                  -1,
                                  -1,
                                  0,
                                  sixel_colorspace_log_clamp(pixel_total),
                                  0,
                                  sixel_colorspace_log_clamp(pixel_total),
                                  "threads=%d chunk=%zu jobs=%zu",
                                  threads,
                                  chunk_pixels,
                                  job_count);
            }

            pool = threadpool_create(threads,
                                     queue_depth,
                                     0,
                                     sixel_colorspace_parallel_worker,
                                     &ctx,
                                     NULL);
            if (pool != NULL) {
                for (job_index = 0U; job_index < job_count; ++job_index) {
                    job.band_index = (int)job_index;
                    threadpool_push(pool, job);
                }

                threadpool_finish(pool);
                rc = threadpool_get_error(pool);
                threadpool_destroy(pool);

                if (rc == SIXEL_OK) {
                    if (logger_ref != NULL) {
                        sixel_logger_logf(
                            logger_ref,
                            "controller",
                            "colorspace",
                            "finish",
                            -1,
                            -1,
                            0,
                            sixel_colorspace_log_clamp(pixel_total),
                            0,
                            sixel_colorspace_log_clamp(pixel_total),
                            "parallel finish threads=%d", threads);
                    }
                    status = SIXEL_OK;
                    goto end;
                }
            }

            if (logger_ref != NULL) {
                sixel_logger_logf(logger_ref,
                                  "controller",
                                  "colorspace",
                                  "fallback",
                                  -1,
                                  -1,
                                  0,
                                  sixel_colorspace_log_clamp(pixel_total),
                                  0,
                                  sixel_colorspace_log_clamp(pixel_total),
                                  "threadpool fallback rc=%d", rc);
            }
        } else if (logger_ref != NULL) {
            sixel_logger_logf(logger_ref,
                              "controller",
                              "colorspace",
                              "fallback",
                              -1,
                              -1,
                              0,
                              sixel_colorspace_log_clamp(pixel_total),
                              0,
                              sixel_colorspace_log_clamp(pixel_total),
                              "threads=%d", threads);
        }
    } else if (logger_ref != NULL) {
        sixel_logger_logf(logger_ref,
                          "controller",
                          "colorspace",
                          "fallback",
                          -1,
                          -1,
                          0,
                          sixel_colorspace_log_clamp(pixel_total),
                          0,
                          sixel_colorspace_log_clamp(pixel_total),
                          "below threshold=%zu",
                          sixel_colorspace_parallel_min_pixels());
    }
#endif

#if SIXEL_ENABLE_THREADS
    if (logger_ref != NULL) {
        sixel_logger_logf(logger_ref,
                          "worker",
                          "colorspace",
                          "start",
                          0,
                          0,
                          0,
                          sixel_colorspace_log_clamp(pixel_total),
                          0,
                          sixel_colorspace_log_clamp(pixel_total),
                          "serial chunk size=%zu", pixel_total);
    }
#endif

    status = sixel_convert_pixels_via_linear_float_chunk(pixels,
                                                         pixel_total,
                                                         colorspace_src,
                                                         colorspace_dst,
                                                         simd_level);

#if SIXEL_ENABLE_THREADS
    if (logger_ref != NULL) {
        sixel_logger_logf(logger_ref,
                          "worker",
                          "colorspace",
                          "finish",
                          0,
                          sixel_colorspace_log_clamp(pixel_total),
                          0,
                          sixel_colorspace_log_clamp(pixel_total),
                          0,
                          sixel_colorspace_log_clamp(pixel_total),
                          "serial status=%d", status);
    }

end:
    sixel_logger_close(&logger);
#endif

    return status;
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
    case SIXEL_PIXELFORMAT_RGBFLOAT32:
    case SIXEL_PIXELFORMAT_LINEARRGBFLOAT32:
    case SIXEL_PIXELFORMAT_OKLABFLOAT32:
    case SIXEL_PIXELFORMAT_CIELABFLOAT32:
    case SIXEL_PIXELFORMAT_DIN99DFLOAT32:
    case SIXEL_PIXELFORMAT_YUVFLOAT32:
        return 1;
    default:
        break;
    }

    return 0;
}

static int
sixel_colorspace_supports_byte_format(int pixelformat)
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

static int
sixel_colorspace_supports_lut_pair(int colorspace_src, int colorspace_dst)
{
    if (colorspace_src == SIXEL_COLORSPACE_GAMMA &&
            colorspace_dst == SIXEL_COLORSPACE_LINEAR) {
        return 1;
    }

    if (colorspace_src == SIXEL_COLORSPACE_LINEAR &&
            colorspace_dst == SIXEL_COLORSPACE_GAMMA) {
        return 1;
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
    int simd_level;
    int byte_format_supported;
    int lut_pair_supported;
    int avx2_yuv_supported;

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

    /*
     * Fast paths rely on LUT-based byte formats.  Filter out unsupported
     * combinations early so we do not waste time probing SIMD kernels that
     * are guaranteed to fail for the current request.
     */
    byte_format_supported =
        sixel_colorspace_supports_byte_format(pixelformat);
    lut_pair_supported = sixel_colorspace_supports_lut_pair(colorspace_src,
                                                            colorspace_dst);
    avx2_yuv_supported = byte_format_supported &&
        colorspace_src == SIXEL_COLORSPACE_YUV &&
        (colorspace_dst == SIXEL_COLORSPACE_GAMMA ||
         colorspace_dst == SIXEL_COLORSPACE_LINEAR);

#if (defined(SIXEL_USE_AVX512) && defined(__AVX512F__) && \
        defined(__AVX512BW__)) || \
        (defined(SIXEL_USE_AVX2) && defined(__AVX2__)) || \
        defined(SIXEL_USE_SSE2) || defined(SIXEL_USE_NEON)
    simd_level = sixel_cpu_simd_level();
#else
    simd_level = SIXEL_SIMD_LEVEL_SCALAR;
#endif

#if (!defined(SIXEL_USE_AVX512) || !defined(__AVX512F__) || \
        !defined(__AVX512BW__)) && \
        (!defined(SIXEL_USE_AVX2) || !defined(__AVX2__)) && \
        !defined(SIXEL_USE_SSE2) && !defined(SIXEL_USE_NEON)
    (void)simd_level;
#endif

#if !defined(SIXEL_USE_AVX2) || !defined(__AVX2__)
    (void)avx2_yuv_supported;
#endif

    if (SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat)) {
        return sixel_convert_pixels_via_linear_float((float *)pixels,
                                                     size,
                                                     colorspace_src,
                                                     colorspace_dst);
    }

#if defined(SIXEL_USE_AVX512) && defined(__AVX512F__) && \
        defined(__AVX512BW__)
    if (simd_level >= SIXEL_SIMD_LEVEL_AVX512 &&
            byte_format_supported && lut_pair_supported) {
        SIXELSTATUS avx512_status;

        avx512_status = sixel_colorspace_convert_avx512(pixels,
                                                        size,
                                                        pixelformat,
                                                        colorspace_src,
                                                        colorspace_dst);
        if (avx512_status == SIXEL_OK) {
            return SIXEL_OK;
        }
    }
#endif

#if defined(SIXEL_USE_AVX2) && defined(__AVX2__)
    if (simd_level >= SIXEL_SIMD_LEVEL_AVX2 &&
            (avx2_yuv_supported || (byte_format_supported &&
            lut_pair_supported))) {
        SIXELSTATUS avx_status;

        avx_status = sixel_colorspace_convert_avx2(pixels,
                                                   size,
                                                   pixelformat,
                                                   colorspace_src,
                                                   colorspace_dst);
        if (avx_status == SIXEL_OK) {
            return SIXEL_OK;
        }
    }
#endif

#if defined(SIXEL_USE_SSE2)
    if (simd_level >= SIXEL_SIMD_LEVEL_SSE2 &&
            byte_format_supported && lut_pair_supported) {
        SIXELSTATUS sse_status;

        sse_status = sixel_colorspace_convert_sse2(pixels,
                                                   size,
                                                   pixelformat,
                                                   colorspace_src,
                                                   colorspace_dst);
        if (sse_status == SIXEL_OK) {
            return SIXEL_OK;
        }
    }
#endif

#if defined(SIXEL_USE_NEON)
    if (simd_level == SIXEL_SIMD_LEVEL_NEON &&
            byte_format_supported && lut_pair_supported &&
            sixel_colorspace_neon_supported_format(pixelformat)) {
        SIXELSTATUS neon_status;

        neon_status = sixel_colorspace_convert_neon(pixels,
                                                    size,
                                                    pixelformat,
                                                    colorspace_src,
                                                    colorspace_dst);
        if (neon_status == SIXEL_OK) {
            return SIXEL_OK;
        }
    }
#endif

    if (colorspace_src == SIXEL_COLORSPACE_OKLAB ||
            colorspace_dst == SIXEL_COLORSPACE_OKLAB ||
            colorspace_src == SIXEL_COLORSPACE_CIELAB ||
            colorspace_dst == SIXEL_COLORSPACE_CIELAB ||
            colorspace_src == SIXEL_COLORSPACE_DIN99D ||
            colorspace_dst == SIXEL_COLORSPACE_DIN99D ||
            colorspace_src == SIXEL_COLORSPACE_YUV ||
            colorspace_dst == SIXEL_COLORSPACE_YUV ||
            colorspace_src == SIXEL_COLORSPACE_SMPTEC ||
            colorspace_dst == SIXEL_COLORSPACE_SMPTEC) {
        SIXELSTATUS status = sixel_convert_pixels_via_linear(pixels,
                                                             size,
                                                             pixelformat,
                                                             colorspace_src,
                                                             colorspace_dst);
        if (SIXEL_FAILED(status)) {
            sixel_helper_set_additional_message(
                "sixel_helper_convert_colorspace: unsupported "
                "pixelformat for conversion.");
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
