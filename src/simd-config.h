/* SPDX-License-Identifier: MIT */
/*
 * SIMD kernels compiled into this build, as decided by the configure or
 * meson probes. Kernels test these macros instead of compiler or
 * architecture macros: MSVC defines neither __SSE2__ on x64 nor
 * __aarch64__ on ARM64, and --disable-simd must win on every compiler.
 * Include after config.h.
 */
#ifndef LIBSIXEL_SIMD_CONFIG_H
#define LIBSIXEL_SIMD_CONFIG_H

/* Fat objects share config.h. Each compiler receives its own target flag. */
#if defined(HAVE_SSE2) && HAVE_SSE2 && \
    defined(HAVE_EMMINTRIN_H) && HAVE_EMMINTRIN_H && \
    (!defined(SIXEL_SIMD_SHARED_CONFIG) || defined(SIXEL_SIMD_TARGET_X86))
# define SIXEL_SIMD_SSE2 1
#else
# define SIXEL_SIMD_SSE2 0
#endif

/* AVX2 kernels compile for run-time dispatch; SIXEL_TARGET_AVX2 is the
 * function attribute the probe needed (empty where none is needed). */
#if SIXEL_SIMD_SSE2 && defined(HAVE_AVX2_KERNELS) && HAVE_AVX2_KERNELS
# define SIXEL_SIMD_AVX2 1
#else
# define SIXEL_SIMD_AVX2 0
#endif

/* Any NEON: the common 32-bit and 64-bit intrinsic subset. */
#if defined(HAVE_NEON) && HAVE_NEON && \
    defined(HAVE_ARM_NEON_H) && HAVE_ARM_NEON_H && \
    (!defined(SIXEL_SIMD_SHARED_CONFIG) || defined(SIXEL_SIMD_TARGET_A64))
# define SIXEL_SIMD_NEON 1
#else
# define SIXEL_SIMD_NEON 0
#endif

/* AArch64 NEON: across-vector reductions and float64x2_t lanes. */
#if SIXEL_SIMD_NEON && defined(HAVE_NEON_A64) && HAVE_NEON_A64
# define SIXEL_SIMD_NEON_A64 1
#else
# define SIXEL_SIMD_NEON_A64 0
#endif

/* Kernels that reinterpret vector lanes as packed bytes need this order. */
#if defined(WORDS_BIGENDIAN) && WORDS_BIGENDIAN
# define SIXEL_SIMD_LITTLE_ENDIAN 0
#else
# define SIXEL_SIMD_LITTLE_ENDIAN 1
#endif

#endif /* LIBSIXEL_SIMD_CONFIG_H */
