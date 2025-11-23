/*
 * SPDX-License-Identifier: MIT
 *
 * Streaming-aware RGBA span filler used by the parallel decoder. On x86
 * the implementation emits non-temporal stores for long, aligned spans to
 * reduce cache pollution when writing large solid runs.
 */
#include "config.h"

#include <stddef.h>
#include <stdint.h>

#include "fillrun.h"

#if defined(__AVX2__)
# define SIXEL_FILLRUN_HAVE_AVX2 1
# include <immintrin.h>
#else
# define SIXEL_FILLRUN_HAVE_AVX2 0
#endif

#if defined(__SSE2__)
# define SIXEL_FILLRUN_HAVE_SSE2 1
# include <emmintrin.h>
#else
# define SIXEL_FILLRUN_HAVE_SSE2 0
#endif

static void
sixel_fillrun_store_rgba_scalar(unsigned char *dst, int repeat, uint32_t rgba)
{
    uint32_t *out;
    int i;

    out = (uint32_t *)dst;
    for (i = 0; i < repeat; ++i) {
        out[i] = rgba;
    }
}

#if SIXEL_FILLRUN_HAVE_AVX2
static void
sixel_fillrun_store_rgba_avx2(unsigned char *dst,
                              int repeat,
                              uint32_t rgba)
{
    __m256i packed;
    __m256i *out;
    int remaining;
    int words;

    out = (__m256i *)dst;
    packed = _mm256_set1_epi32((int)rgba);
    words = repeat / 8;
    remaining = repeat - words * 8;
    while (words-- > 0) {
        _mm256_stream_si256(out, packed);
        ++out;
    }
    _mm_sfence();
    if (remaining > 0) {
        sixel_fillrun_store_rgba_scalar((unsigned char *)out,
                                        remaining,
                                        rgba);
    }
}
#endif

#if SIXEL_FILLRUN_HAVE_SSE2
static void
sixel_fillrun_store_rgba_sse2(unsigned char *dst,
                              int repeat,
                              uint32_t rgba)
{
    __m128i packed;
    __m128i *out;
    int remaining;
    int words;

    out = (__m128i *)dst;
    packed = _mm_set1_epi32((int)rgba);
    words = repeat / 4;
    remaining = repeat - words * 4;
    while (words-- > 0) {
        _mm_stream_si128(out, packed);
        ++out;
    }
    _mm_sfence();
    if (remaining > 0) {
        sixel_fillrun_store_rgba_scalar((unsigned char *)out,
                                        remaining,
                                        rgba);
    }
}
#endif

void
sixel_fillrun_store_rgba(unsigned char *dst,
                         int repeat,
                         uint32_t rgba,
                         int use_non_temporal)
{
#if SIXEL_FILLRUN_HAVE_SSE2
    size_t align16;
#endif
#if SIXEL_FILLRUN_HAVE_AVX2
    size_t align32;
#endif

    if (dst == NULL || repeat <= 0) {
        return;
    }

#if SIXEL_FILLRUN_HAVE_SSE2
    align16 = ((size_t)dst) & 0x0fu;
#endif
#if SIXEL_FILLRUN_HAVE_AVX2
    align32 = ((size_t)dst) & 0x1fu;
#endif

    if (use_non_temporal) {
        /*
         * Prefer streaming stores when the destination is naturally
         * aligned. Otherwise fall back to scalar copies to avoid faults
         * or partial streaming stores on misaligned pointers.
         */
#if SIXEL_FILLRUN_HAVE_AVX2
        if (align32 == 0u && repeat >= 8) {
            sixel_fillrun_store_rgba_avx2(dst, repeat, rgba);
            return;
        }
#endif
#if SIXEL_FILLRUN_HAVE_SSE2
        if (align16 == 0u && repeat >= 4) {
            sixel_fillrun_store_rgba_sse2(dst, repeat, rgba);
            return;
        }
#endif
    }

    sixel_fillrun_store_rgba_scalar(dst, repeat, rgba);
}
