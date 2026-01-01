/*
 * SPDX-License-Identifier: MIT
 *
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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

/* STDC_HEADERS */
#include <stdlib.h>

#if HAVE_INTRIN_H && (defined(_WIN32) || defined(_MSC_VER))
/*
 * Avoid cpuid.h macro clashes on Windows toolchains. Restrict intrin.h to
 * Windows targets so platforms such as OpenBSD, whose Clang wrapper tries
 * include_next to a non-existent system header, skip it gracefully.
 */
# include <intrin.h>
#endif
#if HAVE_CPUID_H && !defined(_WIN32) && \
    (defined(__x86_64__) || defined(__i386))
# include <cpuid.h>
#endif
#if defined(HAVE_IMMINTRIN_H) && \
    (defined(__x86_64__) || defined(_M_X64) || defined(__i386) || \
     defined(_M_IX86)) && !defined(__COSMOPOLITAN__)
# include <immintrin.h>
#endif

#include "cpu.h"
#include "compat_stub.h"

static int simd_cached = -1;

static enum sixel_simd_level
sixel_cpu_env_cap(void)
{
    char const *env;

    env = sixel_compat_getenv("SIXEL_SIMD_LEVEL");
    if (env == NULL || env[0] == '\0') {
        return SIXEL_SIMD_LEVEL_NEON;
    }
    if (sixel_compat_strcasecmp(env, "auto") == 0) {
        return SIXEL_SIMD_LEVEL_NEON;
    }
    if (sixel_compat_strcasecmp(env, "none") == 0 ||
        sixel_compat_strcasecmp(env, "scalar") == 0) {
        return SIXEL_SIMD_LEVEL_SCALAR;
    }
    if (sixel_compat_strcasecmp(env, "sse2") == 0) {
        return SIXEL_SIMD_LEVEL_SSE2;
    }
    if (sixel_compat_strcasecmp(env, "avx") == 0) {
        return SIXEL_SIMD_LEVEL_AVX;
    }
    if (sixel_compat_strcasecmp(env, "avx2") == 0) {
        return SIXEL_SIMD_LEVEL_AVX2;
    }
    if (sixel_compat_strcasecmp(env, "avx512") == 0) {
        return SIXEL_SIMD_LEVEL_AVX512;
    }
    if (sixel_compat_strcasecmp(env, "neon") == 0) {
        return SIXEL_SIMD_LEVEL_NEON;
    }
    return SIXEL_SIMD_LEVEL_NEON;
}

static enum sixel_simd_level
sixel_cpu_detect_native(void)
{
    enum sixel_simd_level level;

    level = SIXEL_SIMD_LEVEL_SCALAR;
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386) || \
    defined(_M_IX86)
# if HAVE_BUILTIN_CPU_INIT && !defined(_MSC_VER) && !defined(__PCC__)
    __builtin_cpu_init();
# endif
# if HAVE_BUILTIN_CPU_SUPPORTS && !defined(_MSC_VER) && !defined(__PCC__)
    /*
     * clang-cl accepts __builtin_cpu_supports() but MSVC-style
     * toolchains do not ship the runtime helpers (e.g.
     * __cpu_indicator_init). Skip the builtin path there and use the
     * intrinsics-based detection instead.
     */
    if (__builtin_cpu_supports("avx512f")) {
        level = SIXEL_SIMD_LEVEL_AVX512;
    } else if (__builtin_cpu_supports("avx2")) {
        level = SIXEL_SIMD_LEVEL_AVX2;
    } else if (__builtin_cpu_supports("avx")) {
        level = SIXEL_SIMD_LEVEL_AVX;
    } else if (__builtin_cpu_supports("sse2")) {
        level = SIXEL_SIMD_LEVEL_SSE2;
    }
# elif defined(_MSC_VER) && HAVE_INTRIN_H
    int cpu_info[4];
    int osxsave;
    int avx_capable;

    __cpuid(cpu_info, 1);
    osxsave = (cpu_info[2] & (1 << 27));
    avx_capable = (cpu_info[2] & (1 << 28));
    if (osxsave && avx_capable) {
        unsigned long long xcr0;

        xcr0 = _xgetbv(0);
        if ((xcr0 & 0x6) == 0x6) {
            int extended[4];

            __cpuidex(extended, 7, 0);
            if ((extended[1] & (1 << 16)) != 0) {
                level = SIXEL_SIMD_LEVEL_AVX512;
            } else if ((extended[1] & (1 << 5)) != 0) {
                level = SIXEL_SIMD_LEVEL_AVX2;
            } else {
                level = SIXEL_SIMD_LEVEL_AVX;
            }
        }
    }
    if (level == SIXEL_SIMD_LEVEL_SCALAR && (cpu_info[3] & (1 << 26))) {
        level = SIXEL_SIMD_LEVEL_SSE2;
    }
# elif HAVE_CPUID_H
    unsigned int eax;
    unsigned int ebx;
    unsigned int ecx;
    unsigned int edx;
    int osxsave;
    int avx_capable;

    if (__get_cpuid(1, &eax, &ebx, &ecx, &edx) != 0) {
        /*
         * DragonFlyBSD kernels expose AVX/AVX2/AVX512 bits even when the
         * OS has not enabled XSAVE. Guard against executing AVX-class
         * instructions by checking OSXSAVE and XCR0 before raising the
         * SIMD level. Fall back to SSE2 when the OS cannot preserve the
         * extended state.
         */
        osxsave = (ecx & (1U << 27)) != 0;
        avx_capable = (ecx & (1U << 28)) != 0;
#  if defined(HAVE_IMMINTRIN_H) && !defined(__COSMOPOLITAN__)
        /*
         * Cosmopolitan builds target very old x86 CPUs and omit -mxsave/-mavx
         * from the default flags. Skip the XGETBV probe there to avoid
         * triggering target-specific option mismatches during compilation.
         */
        if (osxsave != 0 && avx_capable != 0) {
            unsigned long long xcr0;

            xcr0 = _xgetbv(0);
            if ((xcr0 & 0x6) == 0x6) {
                if (__get_cpuid_max(0, NULL) >= 7 &&
                    __get_cpuid_count(7, 0, &eax, &ebx, &ecx,
                                      &edx) != 0) {
                    if ((ebx & (1U << 16)) != 0) {
                        level = SIXEL_SIMD_LEVEL_AVX512;
                    } else if ((ebx & (1U << 5)) != 0) {
                        level = SIXEL_SIMD_LEVEL_AVX2;
                    } else {
                        level = SIXEL_SIMD_LEVEL_AVX;
                    }
                } else {
                    level = SIXEL_SIMD_LEVEL_AVX;
                }
            }
        }
#  endif
        if (level == SIXEL_SIMD_LEVEL_SCALAR &&
            (edx & (1U << 26)) != 0) {
            level = SIXEL_SIMD_LEVEL_SSE2;
        }
    }
# endif
#elif defined(__aarch64__) || defined(__ARM_NEON) || defined(__ARM_NEON__)
    level = SIXEL_SIMD_LEVEL_NEON;
#endif
    return level;
}

static enum sixel_simd_level
sixel_cpu_min(enum sixel_simd_level lhs, enum sixel_simd_level rhs)
{
    if (lhs < rhs) {
        return lhs;
    }
    return rhs;
}

int
sixel_cpu_simd_level(void)
{
    enum sixel_simd_level env_cap;
    enum sixel_simd_level native;

    if (simd_cached >= 0) {
        return simd_cached;
    }

    env_cap = sixel_cpu_env_cap();
    native = sixel_cpu_detect_native();
    simd_cached = (int)sixel_cpu_min(env_cap, native);
    return simd_cached;
}

void
sixel_cpu_reset_simd_cache(void)
{
    simd_cached = -1;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
