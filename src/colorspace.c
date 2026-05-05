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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <math.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

#include <sixel.h>

#include "colorspace.h"
#include "cpu.h"
#include "loader-common.h"
#include "timeline-logger.h"
#include "threading.h"
#if SIXEL_ENABLE_THREADS
# include <6cells.h>
#endif
#include "compat_stub.h"

#if defined(_MSC_VER)
/*
 * Provide a C99-friendly alignment annotation for stack buffers used by
 * SIMD helpers.  This keeps the build compatible with compilers that do
 * not support C11 alignas.
 */
# define SIXEL_ALIGNAS(bytes) __declspec(align(bytes))
#elif defined(__GNUC__) || defined(__clang__)
# define SIXEL_ALIGNAS(bytes) __attribute__((aligned(bytes)))
#else
# define SIXEL_ALIGNAS(bytes)
#endif

/*
 * pixelformat layout
 */
typedef struct sixel_pixelformat_layout {
    int step;
    int index_r;
    int index_g;
    int index_b;
} sixel_pixelformat_layout_t;

static inline unsigned char
sixel_oklab_encode_L(double L);
static inline unsigned char
sixel_oklab_encode_ab(double value);
static inline double
sixel_oklab_decode_ab(unsigned char v);
static SIXELSTATUS
sixel_convert_pixels_via_linear(unsigned char *pixels,
                                size_t size,
                                int pixelformat,
                                int colorspace_src,
                                int colorspace_dst);

#if SIXEL_ENABLE_THREADS
# if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MSYS__) && \
        !defined(WITH_WINPTHREAD)
#  define SIXEL_COLORSPACE_USE_WIN32_ONCE 1
#  include <windows.h>
static INIT_ONCE sixel_colorspace_once = INIT_ONCE_STATIC_INIT;
static INIT_ONCE sixel_colorspace_parallel_min_pixels_once =
    INIT_ONCE_STATIC_INIT;
# else
#  include <pthread.h>
static pthread_once_t sixel_colorspace_once = PTHREAD_ONCE_INIT;
static pthread_once_t sixel_colorspace_parallel_min_pixels_once =
    PTHREAD_ONCE_INIT;
# endif
#endif

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

#if defined(SIXEL_USE_NEON)
static uint8x16x4_t sixel_neon_gamma_to_linear[4];
static uint8x16x4_t sixel_neon_linear_to_gamma[4];
static int sixel_neon_tables_initialized = 0;
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
#define SIXEL_CBR_LUT_SIZE 4096

/*
 * Cube-root lookup flattens repeated cbrt() calls inside OKLab/CIELAB
 * transforms.  SIMD lanes gather from this shared table so pow()-style math
 * stays confined to the one-time initialization.
 */

static unsigned char gamma_to_linear_lut[SIXEL_COLORSPACE_LUT_SIZE];
static unsigned char linear_to_gamma_lut[SIXEL_COLORSPACE_LUT_SIZE];
static double gamma_to_linear_double_lut[SIXEL_COLORSPACE_LUT_SIZE];
static double smptec_to_linear_double_lut[SIXEL_COLORSPACE_LUT_SIZE];
static unsigned char linear_to_smptec_lut[SIXEL_COLORSPACE_LUT_SIZE];
static float sixel_cbrt_lut[SIXEL_CBR_LUT_SIZE];
static double sixel_cbrt_lut_double[SIXEL_CBR_LUT_SIZE];
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
sixel_colorspace_cbrt_lookup_double(double value)
{
    double scaled;
    int index;

    if (value <= 0.0) {
        return 0.0;
    }
    if (value >= 1.0) {
        return 1.0;
    }

    scaled = value * (double)(SIXEL_CBR_LUT_SIZE - 1);
    index = (int)(scaled + 0.5);
    if (index < 0) {
        index = 0;
    } else if (index >= SIXEL_CBR_LUT_SIZE) {
        index = SIXEL_CBR_LUT_SIZE - 1;
    }

    return (double)sixel_cbrt_lut[index];
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

static inline unsigned char
sixel_linear_double_to_srgb(double v);

static inline unsigned char
sixel_linear_double_to_byte(double v);

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
#if defined(SIXEL_USE_NEON) || defined(SIXEL_USE_SSE2)
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
        return sixel_colorspace_cbrt_lookup_double(t);
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

    l_ = sixel_colorspace_cbrt_lookup_double(l);
    m_ = sixel_colorspace_cbrt_lookup_double(m);
    s_ = sixel_colorspace_cbrt_lookup_double(s);

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
sixel_colorspace_build_tables(void)
{
    int i;
    double gamma_value;
    double linear_value;
    double converted;
    double smptec_value;
    double smptec_linear;
    double cbrt_input;

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

    for (i = 0; i < SIXEL_CBR_LUT_SIZE; ++i) {
        cbrt_input = (double)i / (double)(SIXEL_CBR_LUT_SIZE - 1);
        sixel_cbrt_lut[i] = (float)cbrt(cbrt_input);
        sixel_cbrt_lut_double[i] = (double)sixel_cbrt_lut[i];
    }


    tables_initialized = 1;
}

#if SIXEL_ENABLE_THREADS && defined(SIXEL_COLORSPACE_USE_WIN32_ONCE)
static BOOL CALLBACK
sixel_colorspace_build_once(PINIT_ONCE init_once,
                            PVOID parameter,
                            PVOID *context)
{
    (void)init_once;
    (void)parameter;
    (void)context;

    sixel_colorspace_build_tables();

    return TRUE;
}
#endif

static void
sixel_colorspace_init_tables(void)
{
#if SIXEL_ENABLE_THREADS
# if defined(SIXEL_COLORSPACE_USE_WIN32_ONCE)
    BOOL executed;

    executed = InitOnceExecuteOnce(&sixel_colorspace_once,
                                   sixel_colorspace_build_once,
                                   NULL,
                                   NULL);
    if (executed == FALSE) {
        /*
         * InitOnce failure keeps the code path live so fall back to a
         * direct build that mirrors the single-threaded path.  Table
         * contents remain immutable after completion.
         */
        sixel_colorspace_build_tables();
    }
# else
    int status;

    status = pthread_once(&sixel_colorspace_once,
                          sixel_colorspace_build_tables);
    if (status != 0) {
        /*
         * Defer to the plain builder when pthread_once reports an error so
         * callers still receive the precomputed tables.  pthread_once only
         * invokes the callback once per process even if multiple threads
         * race the entry point.
         */
        sixel_colorspace_build_tables();
    }
# endif
#else
    sixel_colorspace_build_tables();
#endif
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
    size_t processed;
    size_t pixel_index;
    size_t offset;
    unsigned char *pr;
    unsigned char *pg;
    unsigned char *pb;
    double r_lin;
    double g_lin;
    double b_lin;

    processed = 0U;

    for (pixel_index = processed; pixel_index < pixel_total; ++pixel_index) {
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

static int
sixel_colorspace_log_clamp(size_t value)
{
    if (value > (size_t)INT_MAX) {
        return INT_MAX;
    }

    return (int)value;
}

#if SIXEL_ENABLE_THREADS
typedef struct sixel_colorspace_parallel_context {
    float *pixels;
    size_t pixel_total;
    size_t chunk_pixels;
    int colorspace_src;
    int colorspace_dst;
    int simd_level;
    sixel_timeline_logger_t *logger;
} sixel_colorspace_parallel_context_t;

typedef struct sixel_colorspace_parallel_byte_context {
    unsigned char *pixels;
    size_t pixel_total;
    size_t chunk_pixels;
    sixel_pixelformat_layout_t layout;
    int colorspace_src;
    int colorspace_dst;
    sixel_timeline_logger_t *logger;
} sixel_colorspace_parallel_byte_context_t;

/*
 * Allow deployments to defer thread fan-out on tiny buffers via
 * SIXEL_COLORSPACE_PARALLEL_MIN_PIXELS. Defaults to 65537 pixels so the
 * colorspace conversion waits for moderately sized frames before fanning
 * out, but callers can override the behavior through the environment.
 */
static size_t
sixel_colorspace_parallel_min_pixels(void)
{
    static size_t threshold = 65537;
    static int initialized = 0;
    char const *text;
    char *endptr;
    unsigned long long parsed;

    if (initialized != 0) {
        return threshold;
    }

    text = sixel_compat_getenv("SIXEL_COLORSPACE_PARALLEL_MIN_PIXELS");
    if (text == NULL || text[0] == '\0') {
        initialized = 1;
        return threshold;
    }

    errno = 0;
    parsed = strtoull(text, &endptr, 10);
    if (endptr == text || *endptr != '\0' || errno == ERANGE) {
        initialized = 1;
        return threshold;
    }

    if (parsed > (unsigned long long)SIZE_MAX) {
        threshold = SIZE_MAX;
    } else {
        threshold = (size_t)parsed;
    }

    initialized = 1;
    return threshold;
}

static void
sixel_colorspace_parallel_min_pixels_init_once(void)
{
    (void)sixel_colorspace_parallel_min_pixels();
}

#if SIXEL_ENABLE_THREADS
# if defined(SIXEL_COLORSPACE_USE_WIN32_ONCE)
static BOOL CALLBACK
sixel_colorspace_parallel_min_pixels_once_cb(PINIT_ONCE init_once,
                                             PVOID parameter,
                                             PVOID *context)
{
    (void)init_once;
    (void)parameter;
    (void)context;

    sixel_colorspace_parallel_min_pixels_init_once();

    return TRUE;
}
# endif

static size_t
sixel_colorspace_parallel_min_pixels_cached(void)
{
# if defined(SIXEL_COLORSPACE_USE_WIN32_ONCE)
    BOOL executed;

    executed = InitOnceExecuteOnce(
        &sixel_colorspace_parallel_min_pixels_once,
        sixel_colorspace_parallel_min_pixels_once_cb,
        NULL,
        NULL);
    if (executed == FALSE) {
        sixel_colorspace_parallel_min_pixels_init_once();
    }
# else
    int status;

    status = pthread_once(&sixel_colorspace_parallel_min_pixels_once,
                          sixel_colorspace_parallel_min_pixels_init_once);
    if (status != 0) {
        sixel_colorspace_parallel_min_pixels_init_once();
    }
# endif

    return sixel_colorspace_parallel_min_pixels();
}
#else
static size_t
sixel_colorspace_parallel_min_pixels_cached(void)
{
    return sixel_colorspace_parallel_min_pixels();
}
#endif

static int
sixel_colorspace_parallel_worker_bytes(sixel_thread_pool_job_t job,
                                       void *userdata,
                                       void *workspace)
{
    size_t start;
    size_t remaining;
    size_t end;
    sixel_colorspace_parallel_byte_context_t *ctx;
    sixel_timeline_logger_t *logger;
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
    if (logger != NULL) {
        start_row = sixel_colorspace_log_clamp(start);
        end_row = sixel_colorspace_log_clamp(end);
        sixel_timeline_logger_logf(logger,
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

    if (logger != NULL) {
        sixel_timeline_logger_logf(logger,
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

static SIXELSTATUS
sixel_colorspace_create_pool(sixel_thread_pool_t **pool,
                             int threads,
                             int queue_depth,
                             sixel_thread_pool_worker_function_t worker,
                             void *userdata)
{
    sixel_threadpool_service_t *service;
    sixel_thread_pool_create_request_t request;
    void *service_object;
    SIXELSTATUS status;

    if (pool != NULL) {
        *pool = NULL;
    }
    if (pool == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    service = NULL;
    service_object = NULL;
    status = sixel_components_getservice("services/threadpool",
                                         &service_object);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    service = (sixel_threadpool_service_t *)service_object;
    if (service == NULL || service->vtbl == NULL ||
        service->vtbl->create_pool == NULL) {
        if (service != NULL && service->vtbl != NULL &&
            service->vtbl->unref != NULL) {
            service->vtbl->unref(service);
        }
        return SIXEL_BAD_ARGUMENT;
    }

    request.threads = threads;
    request.queue_size = queue_depth;
    request.workspace_size = 0U;
    request.worker = worker;
    request.userdata = userdata;
    request.workspace_cleanup = NULL;
    status = service->vtbl->create_pool(service, &request, pool);
    if (service->vtbl->unref != NULL) {
        service->vtbl->unref(service);
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
    sixel_timeline_logger_t *logger;
    sixel_timeline_logger_t *logger_ref;
#if SIXEL_ENABLE_THREADS
    size_t job_count;
    size_t chunk_pixels;
    sixel_colorspace_parallel_byte_context_t ctx;
    sixel_thread_pool_t *pool;
    sixel_thread_pool_job_t job;
    int threads;
    int queue_depth;
    size_t job_index;
    int rc;
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
    rc = SIXEL_RUNTIME_ERROR;
#endif
    logger_ref = NULL;
    /*
     * Keep byte and float conversions aligned: respect the same
     * SIXEL_COLORSPACE_PARALLEL_MIN_PIXELS threshold and emit logger events
     * when SIXEL_LOG_PATH is configured so traces show which path a
     * frame took even when threading is disabled.
     */
    logger = NULL;
    (void)sixel_timeline_logger_prepare_env(NULL, &logger);
    if (logger != NULL) {
        logger_ref = logger;
        sixel_timeline_logger_logf(logger_ref,
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

#if SIXEL_ENABLE_THREADS
    if (pixel_total >= sixel_colorspace_parallel_min_pixels_cached()) {
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
                sixel_timeline_logger_logf(logger_ref,
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

            rc = sixel_colorspace_create_pool(
                &pool,
                threads,
                queue_depth,
                sixel_colorspace_parallel_worker_bytes,
                &ctx);
            if (rc == SIXEL_OK && pool != NULL) {
                for (job_index = 0U; job_index < job_count; ++job_index) {
                    job.band_index = (int)job_index;
                    rc = pool->vtbl->push(pool, job);
                    if (rc != SIXEL_OK) {
                        break;
                    }
                }

                pool->vtbl->finish(pool);
                if (rc == SIXEL_OK) {
                    rc = pool->vtbl->get_error(pool);
                }
                pool->vtbl->unref(pool);

                if (rc == SIXEL_OK) {
                    if (logger_ref != NULL) {
                        sixel_timeline_logger_logf(
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
                sixel_timeline_logger_logf(logger_ref,
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
            sixel_timeline_logger_logf(logger_ref,
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
        sixel_timeline_logger_logf(logger_ref,
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
                          sixel_colorspace_parallel_min_pixels_cached());
    }
#endif

    if (logger_ref != NULL) {
        sixel_timeline_logger_logf(logger_ref,
                          "controller",
                          "colorspace",
                          "start",
                          -1,
                          -1,
                          0,
                          sixel_colorspace_log_clamp(pixel_total),
                          0,
                          sixel_colorspace_log_clamp(pixel_total),
                          "serial chunk size=%zu", pixel_total);
        sixel_timeline_logger_logf(logger_ref,
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

    status = sixel_convert_pixels_via_linear_chunk(pixels,
                                                   pixel_total,
                                                   &layout,
                                                   colorspace_src,
                                                   colorspace_dst);

    if (logger_ref != NULL) {
        sixel_timeline_logger_logf(logger_ref,
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
        sixel_timeline_logger_logf(logger_ref,
                          "controller",
                          "colorspace",
                          "finish",
                          -1,
                          -1,
                          0,
                          sixel_colorspace_log_clamp(pixel_total),
                          0,
                          sixel_colorspace_log_clamp(pixel_total),
                          "serial status=%d", status);
    }

#if SIXEL_ENABLE_THREADS
end:
#endif
    sixel_timeline_logger_unref(logger);

    return status;
}

/*
 * Convert RGBFLOAT32 buffers in-place by round-tripping through linear space.
 * The general path keeps double intermediates for OKLab/CIELAB precision.
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

    (void)simd_level;

    if (colorspace_src == colorspace_dst) {
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
sixel_colorspace_parallel_worker(sixel_thread_pool_job_t job,
                                 void *userdata,
                                 void *workspace)
{
    sixel_colorspace_parallel_context_t *ctx;
    sixel_timeline_logger_t *logger;
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
    if (logger != NULL) {
        start_row = sixel_colorspace_log_clamp(start);
        end_row = sixel_colorspace_log_clamp(end);
        sixel_timeline_logger_logf(logger,
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

    if (logger != NULL) {
        sixel_timeline_logger_logf(logger,
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
    sixel_timeline_logger_t *logger;
    sixel_timeline_logger_t *logger_ref;
#if SIXEL_ENABLE_THREADS
    size_t job_count;
    size_t chunk_pixels;
    sixel_colorspace_parallel_context_t ctx;
    sixel_thread_pool_t *pool;
    sixel_thread_pool_job_t job;
    int threads;
    int queue_depth;
    size_t job_index;
    int rc;
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
    rc = SIXEL_RUNTIME_ERROR;
#endif
    logger_ref = NULL;
    /*
     * Enable the timeline logger when SIXEL_LOG_PATH points to a
     * writable output. The controller emits a configure event even if the
     * call later falls back to the serial path so the timeline remains
     * continuous, including non-threaded builds.
     */
    logger = NULL;
    (void)sixel_timeline_logger_prepare_env(NULL, &logger);
    if (logger != NULL) {
        logger_ref = logger;
        sixel_timeline_logger_logf(logger_ref,
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

#if SIXEL_ENABLE_THREADS
    if (pixel_total >= sixel_colorspace_parallel_min_pixels_cached()) {
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
                sixel_timeline_logger_logf(logger_ref,
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

            rc = sixel_colorspace_create_pool(
                &pool,
                threads,
                queue_depth,
                sixel_colorspace_parallel_worker,
                &ctx);
            if (rc == SIXEL_OK && pool != NULL) {
                for (job_index = 0U; job_index < job_count; ++job_index) {
                    job.band_index = (int)job_index;
                    rc = pool->vtbl->push(pool, job);
                    if (rc != SIXEL_OK) {
                        break;
                    }
                }

                pool->vtbl->finish(pool);
                if (rc == SIXEL_OK) {
                    rc = pool->vtbl->get_error(pool);
                }
                pool->vtbl->unref(pool);

                if (rc == SIXEL_OK) {
                    if (logger_ref != NULL) {
                        sixel_timeline_logger_logf(
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
                sixel_timeline_logger_logf(logger_ref,
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
            sixel_timeline_logger_logf(logger_ref,
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
        sixel_timeline_logger_logf(logger_ref,
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
                          sixel_colorspace_parallel_min_pixels_cached());
    }
#endif

    if (logger_ref != NULL) {
        sixel_timeline_logger_logf(logger_ref,
                          "controller",
                          "colorspace",
                          "start",
                          -1,
                          -1,
                          0,
                          sixel_colorspace_log_clamp(pixel_total),
                          0,
                          sixel_colorspace_log_clamp(pixel_total),
                          "serial chunk size=%zu", pixel_total);
        sixel_timeline_logger_logf(logger_ref,
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

    status = sixel_convert_pixels_via_linear_float_chunk(pixels,
                                                         pixel_total,
                                                         colorspace_src,
                                                         colorspace_dst,
                                                         simd_level);

    if (logger_ref != NULL) {
        sixel_timeline_logger_logf(logger_ref,
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
        sixel_timeline_logger_logf(logger_ref,
                          "controller",
                          "colorspace",
                          "finish",
                          -1,
                          -1,
                          0,
                          sixel_colorspace_log_clamp(pixel_total),
                          0,
                          sixel_colorspace_log_clamp(pixel_total),
                          "serial status=%d", status);
    }

#if SIXEL_ENABLE_THREADS
end:
#endif
    sixel_timeline_logger_unref(logger);

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

    loader_timeline_optional_mark("post/colorspace");

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
#if defined(SIXEL_USE_SSE2) || defined(SIXEL_USE_NEON)
    simd_level = sixel_cpu_simd_level();
#else
    simd_level = SIXEL_SIMD_LEVEL_SCALAR;
#endif

#if !defined(SIXEL_USE_SSE2) && !defined(SIXEL_USE_NEON)
    /*
     * Suppress unused warnings when all SIMD paths are disabled at
     * compile-time.  These flags are consulted only by SIMD dispatch,
     * so scalar-only builds must explicitly mark them as unused.
     */
    (void)simd_level;
    (void)byte_format_supported;
    (void)lut_pair_supported;
#endif

    if (SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat)) {
        return sixel_convert_pixels_via_linear_float((float *)pixels,
                                                     size,
                                                     colorspace_src,
                                                     colorspace_dst);
    }


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
