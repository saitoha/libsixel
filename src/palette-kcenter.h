/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
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
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/*
 * Public entry points for the k-center palette builder and its runtime knobs.
 * palette.c selects this solver when -Q center is active.
 */

#ifndef LIBSIXEL_PALETTE_KCENTER_H
#define LIBSIXEL_PALETTE_KCENTER_H

#include <stdint.h>

#include "palette.h"
#include "logger.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum sixel_kcenter_algo {
    SIXEL_PALETTE_KCENTER_ALGO_AUTO = 0,
    SIXEL_PALETTE_KCENTER_ALGO_FFT,
    SIXEL_PALETTE_KCENTER_ALGO_SWAP,
    SIXEL_PALETTE_KCENTER_ALGO_HYBRID,
} sixel_kcenter_algo_t;

typedef enum sixel_kcenter_profile {
    SIXEL_PALETTE_KCENTER_PROFILE_LEGACY = 0,
    SIXEL_PALETTE_KCENTER_PROFILE_SPEED,
    SIXEL_PALETTE_KCENTER_PROFILE_BALANCE,
    SIXEL_PALETTE_KCENTER_PROFILE_QUALITY,
} sixel_kcenter_profile_t;

typedef enum sixel_kcenter_auto_policy {
    SIXEL_PALETTE_KCENTER_AUTO_POLICY_LEGACY = 0,
    SIXEL_PALETTE_KCENTER_AUTO_POLICY_ADAPTIVE,
} sixel_kcenter_auto_policy_t;

typedef enum sixel_kcenter_candidate_policy {
    SIXEL_PALETTE_KCENTER_CANDIDATE_POLICY_LEGACY = 0,
    SIXEL_PALETTE_KCENTER_CANDIDATE_POLICY_HYBRID,
} sixel_kcenter_candidate_policy_t;

typedef enum sixel_kcenter_budget_policy {
    SIXEL_PALETTE_KCENTER_BUDGET_POLICY_LEGACY = 0,
    SIXEL_PALETTE_KCENTER_BUDGET_POLICY_ADAPTIVE,
} sixel_kcenter_budget_policy_t;

typedef enum sixel_kcenter_swap_update {
    SIXEL_PALETTE_KCENTER_SWAP_UPDATE_FULL = 0,
    SIXEL_PALETTE_KCENTER_SWAP_UPDATE_INCREMENTAL,
} sixel_kcenter_swap_update_t;

SIXELSTATUS
sixel_palette_build_kcenter(sixel_palette_t *palette,
                            unsigned char const *data,
                            unsigned int length,
                            int pixelformat,
                            sixel_allocator_t *allocator,
                            sixel_logger_t *logger,
                            int *job_seq,
                            char const *engine_name,
                            sixel_palette_telemetry_t *telemetry);

SIXELSTATUS
sixel_palette_build_kcenter_float32(sixel_palette_t *palette,
                                    float const *data,
                                    unsigned int length,
                                    int pixelformat,
                                    sixel_allocator_t *allocator,
                                    sixel_logger_t *logger,
                                    int *job_seq,
                                    char const *engine_name,
                                    sixel_palette_telemetry_t *telemetry);

SIXEL_INTERNAL_API void
sixel_set_kcenter_algo_override(int enabled,
                                sixel_kcenter_algo_t algo);

SIXEL_INTERNAL_API sixel_kcenter_algo_t
sixel_get_kcenter_algo(void);

SIXEL_INTERNAL_API void
sixel_set_kcenter_profile_override(int enabled,
                                   sixel_kcenter_profile_t profile);

SIXEL_INTERNAL_API sixel_kcenter_profile_t
sixel_get_kcenter_profile(void);

SIXEL_INTERNAL_API void
sixel_set_kcenter_seed_override(int enabled,
                                uint32_t seed);

SIXEL_INTERNAL_API uint32_t
sixel_get_kcenter_seed(void);

SIXEL_INTERNAL_API void
sixel_set_kcenter_restarts_override(int enabled,
                                    unsigned int restarts);

SIXEL_INTERNAL_API unsigned int
sixel_get_kcenter_restarts(void);

SIXEL_INTERNAL_API void
sixel_set_kcenter_iter_override(int enabled,
                                unsigned int iter_count);

SIXEL_INTERNAL_API unsigned int
sixel_get_kcenter_iter(void);

SIXEL_INTERNAL_API void
sixel_set_kcenter_histbits_override(int enabled,
                                    unsigned int histbits);

SIXEL_INTERNAL_API unsigned int
sixel_get_kcenter_histbits(void);

SIXEL_INTERNAL_API void
sixel_set_kcenter_point_budget_override(int enabled,
                                        unsigned int point_budget);

SIXEL_INTERNAL_API unsigned int
sixel_get_kcenter_point_budget(void);

SIXEL_INTERNAL_API void
sixel_set_kcenter_prune_mass_override(int enabled,
                                      double prune_mass);

SIXEL_INTERNAL_API double
sixel_get_kcenter_prune_mass(void);

SIXEL_INTERNAL_API void
sixel_set_kcenter_auto_policy_override(
    int enabled,
    sixel_kcenter_auto_policy_t policy);

SIXEL_INTERNAL_API sixel_kcenter_auto_policy_t
sixel_get_kcenter_auto_policy(void);

SIXEL_INTERNAL_API void
sixel_set_kcenter_auto_fft_threshold_override(
    int enabled,
    unsigned int threshold);

SIXEL_INTERNAL_API unsigned int
sixel_get_kcenter_auto_fft_threshold(void);

SIXEL_INTERNAL_API void
sixel_set_kcenter_candidate_policy_override(
    int enabled,
    sixel_kcenter_candidate_policy_t policy);

SIXEL_INTERNAL_API sixel_kcenter_candidate_policy_t
sixel_get_kcenter_candidate_policy(void);

SIXEL_INTERNAL_API void
sixel_set_kcenter_rare_keep_override(int enabled,
                                     unsigned int rare_keep);

SIXEL_INTERNAL_API unsigned int
sixel_get_kcenter_rare_keep(void);

SIXEL_INTERNAL_API void
sixel_set_kcenter_budget_policy_override(
    int enabled,
    sixel_kcenter_budget_policy_t policy);

SIXEL_INTERNAL_API sixel_kcenter_budget_policy_t
sixel_get_kcenter_budget_policy(void);

SIXEL_INTERNAL_API void
sixel_set_kcenter_budget_scale_override(int enabled,
                                        double budget_scale);

SIXEL_INTERNAL_API double
sixel_get_kcenter_budget_scale(void);

SIXEL_INTERNAL_API void
sixel_set_kcenter_swap_topk_override(int enabled,
                                     unsigned int swap_topk);

SIXEL_INTERNAL_API unsigned int
sixel_get_kcenter_swap_topk(void);

SIXEL_INTERNAL_API void
sixel_set_kcenter_swap_update_override(
    int enabled,
    sixel_kcenter_swap_update_t swap_update);

SIXEL_INTERNAL_API sixel_kcenter_swap_update_t
sixel_get_kcenter_swap_update(void);

SIXEL_INTERNAL_API void
sixel_set_kcenter_swap_patience_override(
    int enabled,
    unsigned int swap_patience);

SIXEL_INTERNAL_API unsigned int
sixel_get_kcenter_swap_patience(void);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_PALETTE_KCENTER_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
