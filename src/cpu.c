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
#if defined(HAVE_IMMINTRIN_H) && defined(ENABLE_XSAVE_PROBE) && \
    (defined(__x86_64__) || defined(_M_X64) || defined(__i386) || \
     defined(_M_IX86))
# include <immintrin.h>
#endif

#include "cpu.h"
#include "compat_stub.h"

/*
 * The SIMD cache is read on hot paths, so keep the fast path lock-free.
 *
 * States:
 * - >=0: resolved SIMD level
 * - -1: unresolved, compute and publish
 */
static int simd_cached = -1;

#if defined(__GNUC__) || defined(__clang__)
# define SIXEL_CPU_CACHE_LOAD() \
    __atomic_load_n(&simd_cached, __ATOMIC_ACQUIRE)
# define SIXEL_CPU_CACHE_CAS(expected_ptr, desired) \
    __atomic_compare_exchange_n(&simd_cached, (expected_ptr), (desired), \
                                0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)
# define SIXEL_CPU_CACHE_STORE(value) \
    __atomic_store_n(&simd_cached, (value), __ATOMIC_RELEASE)
#elif defined(_MSC_VER)
# define SIXEL_CPU_CACHE_LOAD() \
    ((int)_InterlockedCompareExchange((volatile long *)&simd_cached, 0L, 0L))
# define SIXEL_CPU_CACHE_CAS(expected_ptr, desired) \
    (_InterlockedCompareExchange((volatile long *)&simd_cached, \
                                 (long)(desired), \
                                 (long)(*(expected_ptr))) \
     == (long)(*(expected_ptr)))
# define SIXEL_CPU_CACHE_STORE(value) \
    (void)_InterlockedExchange((volatile long *)&simd_cached, (long)(value))
#else
# define SIXEL_CPU_CACHE_LOAD() (simd_cached)
# define SIXEL_CPU_CACHE_CAS(expected_ptr, desired) \
    ((simd_cached == *(expected_ptr)) \
         ? ((simd_cached = (desired)), 1) \
         : 0)
# define SIXEL_CPU_CACHE_STORE(value) do { simd_cached = (value); } while (0)
#endif

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
    if (sixel_compat_strcasecmp(env, "neon") == 0) {
        return SIXEL_SIMD_LEVEL_NEON;
    }
    return SIXEL_SIMD_LEVEL_NEON;
}

static enum sixel_simd_level
sixel_cpu_detect_native(void)
{
    enum sixel_simd_level level;
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386) || \
    defined(_M_IX86)
# if defined(_MSC_VER) && HAVE_INTRIN_H
    int cpu_info[4];
#  if defined(ENABLE_XSAVE_PROBE)
    unsigned long long xcr0;
#  endif
# elif HAVE_CPUID_H
#  if !HAVE_BUILTIN_CPU_SUPPORTS || defined(_MSC_VER) || defined(__PCC__)
    /*
     * Keep CPUID register storage only when the builtin CPU probe is
     * unavailable, so builds using __builtin_cpu_supports() avoid
     * unused-variable warnings.
     */
    unsigned int eax;
    unsigned int ebx;
    unsigned int ecx;
    unsigned int edx;
#  endif
#  if defined(ENABLE_XSAVE_PROBE) && defined(HAVE_IMMINTRIN_H)
    unsigned long long xcr0;
#  endif
# endif
# if defined(ENABLE_XSAVE_PROBE) && \
     ((defined(_MSC_VER) && HAVE_INTRIN_H) || \
      (HAVE_CPUID_H && !(HAVE_BUILTIN_CPU_SUPPORTS && \
       !defined(_MSC_VER) && !defined(__PCC__))))
    int osxsave;
    int avx_capable;
# endif
#endif

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
    if (__builtin_cpu_supports("avx")) {
        level = SIXEL_SIMD_LEVEL_AVX;
    } else if (__builtin_cpu_supports("sse2")) {
        level = SIXEL_SIMD_LEVEL_SSE2;
    }
# elif defined(_MSC_VER) && HAVE_INTRIN_H
    __cpuid(cpu_info, 1);
#  if defined(ENABLE_XSAVE_PROBE)
    osxsave = (cpu_info[2] & (1 << 27));
    avx_capable = (cpu_info[2] & (1 << 28));
    if (osxsave && avx_capable) {
        xcr0 = _xgetbv(0);
        if ((xcr0 & 0x6) == 0x6) {
            level = SIXEL_SIMD_LEVEL_AVX;
        }
    }
#  endif
    if (level == SIXEL_SIMD_LEVEL_SCALAR && (cpu_info[3] & (1 << 26))) {
        level = SIXEL_SIMD_LEVEL_SSE2;
    }
# elif HAVE_CPUID_H
    if (__get_cpuid(1, &eax, &ebx, &ecx, &edx) != 0) {
        /*
         * DragonFlyBSD kernels expose AVX bits even when the
         * OS has not enabled XSAVE. Guard against executing AVX-class
         * instructions by checking OSXSAVE and XCR0 before raising the
         * SIMD level. Fall back to SSE2 when the OS cannot preserve the
         * extended state. The XGETBV probe is optional so toolchains that
         * lack the intrinsic still succeed.
         */
#  if defined(HAVE_IMMINTRIN_H)
#   if defined(ENABLE_XSAVE_PROBE)
        osxsave = (ecx & (1U << 27)) != 0;
        avx_capable = (ecx & (1U << 28)) != 0;
        /*
         * The XGETBV probe is optional. It is only enabled when the
         * toolchain can emit the intrinsic without extra target flags (see
         * ENABLE_XSAVE_PROBE/HAVE_XGETBV_INTRIN). Toolchains that cannot
         * support it will skip this block and fall back to SSE2.
         */
        if (osxsave != 0 && avx_capable != 0) {
            xcr0 = _xgetbv(0);
            if ((xcr0 & 0x6) == 0x6) {
                level = SIXEL_SIMD_LEVEL_AVX;
            }
        }
#   endif
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
    int cached;
    int expected;
    int resolved;
    enum sixel_simd_level env_cap;
    enum sixel_simd_level native;

    cached = SIXEL_CPU_CACHE_LOAD();
    if (cached >= 0) {
        return cached;
    }

    env_cap = sixel_cpu_env_cap();
    native = sixel_cpu_detect_native();
    resolved = (int)sixel_cpu_min(env_cap, native);
    expected = -1;
    if (SIXEL_CPU_CACHE_CAS(&expected, resolved)) {
        return resolved;
    }

    cached = SIXEL_CPU_CACHE_LOAD();
    if (cached >= 0) {
        return cached;
    }

    SIXEL_CPU_CACHE_STORE(resolved);
    return resolved;
}

void
sixel_cpu_reset_simd_cache(void)
{
    SIXEL_CPU_CACHE_STORE(-1);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
