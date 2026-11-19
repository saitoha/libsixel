/* SPDX-License-Identifier: MIT */
/* Compile an isolated AVX2 function without changing the baseline ISA. */
#if (!defined(__GNUC__) && !defined(__clang__)) || defined(__PCC__)
# error No supported per-function target attribute
#endif
#if defined(__has_attribute)
# if !__has_attribute(target)
#  error No per-function target attribute
# endif
#endif
#include <immintrin.h>

__attribute__((target("avx2"))) void
sixel_probe_avx2(void *dst, void const *src)
{
    __m128i low;
    __m256i value;

    low = _mm_loadu_si128((__m128i const *)src);
    value = _mm256_inserti128_si256(_mm256_castsi128_si256(low), low, 1);
    _mm256_storeu_si256((__m256i *)dst, _mm256_abs_epi16(value));
}
