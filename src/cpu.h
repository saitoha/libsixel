/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 */

#ifndef LIBSIXEL_CPU_H
#define LIBSIXEL_CPU_H

#ifdef __cplusplus
extern "C" {
#endif

enum sixel_simd_level {
    SIXEL_SIMD_LEVEL_SCALAR = 0,
    SIXEL_SIMD_LEVEL_SSE2,
    SIXEL_SIMD_LEVEL_AVX,
    SIXEL_SIMD_LEVEL_AVX2,
    SIXEL_SIMD_LEVEL_AVX512,
    SIXEL_SIMD_LEVEL_NEON
};

int
sixel_cpu_simd_level(void);

void
sixel_cpu_reset_simd_cache(void);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_CPU_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
