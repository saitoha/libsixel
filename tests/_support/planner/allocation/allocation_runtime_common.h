/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_TEST_ALLOCATION_RUNTIME_COMMON_H
#define LIBSIXEL_TEST_ALLOCATION_RUNTIME_COMMON_H

typedef struct allocation_runtime_expected {
    int threads;
    int dither;
    int encode;
    int band_height;
    int overlap;
    int mode;
    int band_jobs;
    int effective_method;
    char const *diffusion;
} allocation_runtime_expected_t;

int allocation_runtime_cases(allocation_runtime_expected_t const *expected,
                              int count, int sparse, char const *path);

#endif
