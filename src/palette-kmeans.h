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

/*
 * Public entry points for the K-means palette builder and its tuning knobs.
 * Consumers include palette.c as well as tests that want to drive the
 * quantizer directly.  Keeping the declarations separate from palette.h keeps
 * the orchestrator lightweight and clarifies module ownership.
 */

#ifndef LIBSIXEL_PALETTE_KMEANS_H
#define LIBSIXEL_PALETTE_KMEANS_H

#include <stdint.h>

#include "palette.h"
#include "logger.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum sixel_kmeans_init_type {
    SIXEL_PALETTE_KMEANS_INIT_AUTO = 0,
    SIXEL_PALETTE_KMEANS_INIT_NONE,
    SIXEL_PALETTE_KMEANS_INIT_PCA,
} sixel_kmeans_init_type;

typedef enum sixel_kmeans_binning_mode {
    SIXEL_PALETTE_KMEANS_BINNING_AUTO = 0,
    SIXEL_PALETTE_KMEANS_BINNING_NONE,
    SIXEL_PALETTE_KMEANS_BINNING_HARD,
    SIXEL_PALETTE_KMEANS_BINNING_SOFT,
} sixel_kmeans_binning_mode;

typedef enum sixel_kmeans_mapping_mode {
    SIXEL_PALETTE_KMEANS_MAPPING_UNIFORM = 0,
    SIXEL_PALETTE_KMEANS_MAPPING_SRGB,
} sixel_kmeans_mapping_mode;

typedef enum sixel_kmeans_softdist_mode {
    SIXEL_PALETTE_KMEANS_SOFTDIST_TRILINEAR = 0,
} sixel_kmeans_softdist_mode;

typedef enum sixel_kmeans_feedback_mode {
    SIXEL_PALETTE_KMEANS_FEEDBACK_OFF = 0,
    SIXEL_PALETTE_KMEANS_FEEDBACK_ON,
} sixel_kmeans_feedback_mode;

typedef enum sixel_kmeans_prune_policy {
    SIXEL_PALETTE_KMEANS_PRUNE_AUTO = 0,
    SIXEL_PALETTE_KMEANS_PRUNE_NONE,
    SIXEL_PALETTE_KMEANS_PRUNE_HAMERLY,
    SIXEL_PALETTE_KMEANS_PRUNE_ELKAN,
    SIXEL_PALETTE_KMEANS_PRUNE_YINYANG,
} sixel_kmeans_prune_policy;

SIXELSTATUS
sixel_palette_build_kmeans(sixel_palette_t *palette,
                           unsigned char const *data,
                           unsigned int length,
                           int pixelformat,
                           sixel_allocator_t *allocator,
                           sixel_logger_t *logger,
                           int *job_seq,
                           char const *engine_name,
                           sixel_palette_telemetry_t *telemetry);

SIXELSTATUS
sixel_palette_build_kmeans_float32(sixel_palette_t *palette,
                                   float const *data,
                                   unsigned int length,
                                   int pixelformat,
                                   sixel_allocator_t *allocator,
                                   sixel_logger_t *logger,
                                   int *job_seq,
                                   char const *engine_name,
                                   sixel_palette_telemetry_t *telemetry);

unsigned int
sixel_palette_kmeans_iter_max(void);

double
sixel_palette_kmeans_threshold(void);

SIXEL_INTERNAL_API void
sixel_set_kmeans_threshold_override(int enabled,
                                    double threshold);

SIXEL_INTERNAL_API sixel_kmeans_init_type
sixel_get_kmeans_init_type(void);

SIXEL_INTERNAL_API void
sixel_set_kmeans_init_type_override(int enabled,
                                    sixel_kmeans_init_type init_type);

SIXEL_INTERNAL_API void
sixel_set_kmeans_binning_mode_override(int enabled,
                                       sixel_kmeans_binning_mode mode);

SIXEL_INTERNAL_API sixel_kmeans_binning_mode
sixel_get_kmeans_binning_mode(void);

SIXEL_INTERNAL_API void
sixel_set_kmeans_binbits_override(int enabled,
                                  unsigned int bits);

SIXEL_INTERNAL_API unsigned int
sixel_get_kmeans_binbits(void);

SIXEL_INTERNAL_API void
sixel_set_kmeans_mapping_mode_override(int enabled,
                                       sixel_kmeans_mapping_mode mode);

SIXEL_INTERNAL_API sixel_kmeans_mapping_mode
sixel_get_kmeans_mapping_mode(void);

SIXEL_INTERNAL_API void
sixel_set_kmeans_softdist_mode_override(int enabled,
                                        sixel_kmeans_softdist_mode mode);

SIXEL_INTERNAL_API sixel_kmeans_softdist_mode
sixel_get_kmeans_softdist_mode(void);

SIXEL_INTERNAL_API void
sixel_set_kmeans_autoratio_override(int enabled,
                                    unsigned int ratio);

SIXEL_INTERNAL_API unsigned int
sixel_get_kmeans_autoratio(void);

SIXEL_INTERNAL_API void
sixel_set_kmeans_seed_override(int enabled,
                               uint32_t seed);

SIXEL_INTERNAL_API uint32_t
sixel_get_kmeans_seed(void);

SIXEL_INTERNAL_API int
sixel_get_kmeans_seed_enabled(void);

SIXEL_INTERNAL_API void
sixel_set_kmeans_restarts_override(int enabled,
                                   unsigned int restarts);

SIXEL_INTERNAL_API unsigned int
sixel_get_kmeans_restarts(void);

SIXEL_INTERNAL_API void
sixel_set_kmeans_iter_override(int enabled,
                               unsigned int iter_count);

SIXEL_INTERNAL_API unsigned int
sixel_get_kmeans_iter(void);

SIXEL_INTERNAL_API int
sixel_get_kmeans_iter_enabled(void);

SIXEL_INTERNAL_API void
sixel_set_kmeans_miniter_override(int enabled,
                                  unsigned int miniter);

SIXEL_INTERNAL_API unsigned int
sixel_get_kmeans_miniter(void);

SIXEL_INTERNAL_API void
sixel_set_kmeans_polish_iter_override(int enabled,
                                      unsigned int polish_iter);

SIXEL_INTERNAL_API unsigned int
sixel_get_kmeans_polish_iter(void);

SIXEL_INTERNAL_API void
sixel_set_kmeans_feedback_mode_override(int enabled,
                                        sixel_kmeans_feedback_mode mode);

SIXEL_INTERNAL_API sixel_kmeans_feedback_mode
sixel_get_kmeans_feedback_mode(void);

SIXEL_INTERNAL_API void
sixel_set_kmeans_prune_policy_override(int enabled,
                                       sixel_kmeans_prune_policy policy);

SIXEL_INTERNAL_API sixel_kmeans_prune_policy
sixel_get_kmeans_prune_policy(void);

SIXEL_INTERNAL_API void
sixel_set_kmeans_feedback_slots_override(int enabled,
                                         unsigned int slots);

SIXEL_INTERNAL_API unsigned int
sixel_get_kmeans_feedback_slots(void);

SIXEL_INTERNAL_API void
sixel_set_kmeans_feedback_interval_override(int enabled,
                                            unsigned int interval);

SIXEL_INTERNAL_API unsigned int
sixel_get_kmeans_feedback_interval(void);

SIXELSTATUS
sixel_kmeans_choose_initial_centroids(double *centers,
                                      unsigned int k,
                                      double const *samples,
                                      double const *weights,
                                      unsigned int sample_count,
                                      int use_reversible,
                                      int pixelformat,
                                      double *distance_cache,
                                      sixel_allocator_t *allocator,
                                      sixel_kmeans_init_type init_type,
                                      uint32_t *rng_state);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_PALETTE_KMEANS_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
