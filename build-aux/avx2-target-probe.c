/* SPDX-License-Identifier: MIT */
/* Compile an isolated AVX2 function without changing the baseline ISA.
 * The build system defines SIXEL_PROBE_TARGET_AVX2 as the function
 * attribute under test: GCC and clang need target("avx2"), while MSVC
 * accepts AVX2 intrinsics in any function and needs none. */
#if defined(__PCC__)
# error pcc does not ship usable x86 intrinsic headers
#endif
#include <immintrin.h>

SIXEL_PROBE_TARGET_AVX2 void
sixel_probe_avx2(void *dst, void const *src)
{
    __m128i low;
    __m256i value;

    low = _mm_loadu_si128((__m128i const *)src);
    value = _mm256_inserti128_si256(_mm256_castsi128_si256(low), low, 1);
    _mm256_storeu_si256((__m256i *)dst, _mm256_abs_epi16(value));
}
