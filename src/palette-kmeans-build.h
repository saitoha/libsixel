/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 */

/*
 * Cross-translation-unit interfaces used by palette-kmeans-build.c.
 * Public K-means entry points remain declared in palette-kmeans.h.
 */

#ifndef LIBSIXEL_PALETTE_KMEANS_BUILD_H
#define LIBSIXEL_PALETTE_KMEANS_BUILD_H

#include "logger.h"

#ifdef __cplusplus
extern "C" {
#endif

int
sixel_kmeans_override_lock_acquire(void);

void
sixel_kmeans_override_lock_release(int acquired);

int
sixel_palette_kmeans_log_start(sixel_logger_t *logger,
                               int *job_seq,
                               char const *engine_name,
                               char const *role,
                               char const *phase);

void
sixel_palette_kmeans_log_finish(sixel_logger_t *logger,
                                int job_id,
                                char const *engine_name,
                                char const *role,
                                char const *phase,
                                char const *detail);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_PALETTE_KMEANS_BUILD_H */

/* EOF */
