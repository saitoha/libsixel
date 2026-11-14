/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_REFRESH_SPATIAL_H
#define LIBSIXEL_REFRESH_SPATIAL_H

#include <sixel.h>
#include <stdint.h>
#include "delta-residual.h"

/* One independent producer owns its work buffers. The encoder owns the
 * immutable mask snapshot consumed by all CPU/GPU application workers. */
typedef struct sixel_refresh_spatial sixel_refresh_spatial_t;

SIXEL_INTERNAL_API void
sixel_refresh_spatial_free(sixel_refresh_spatial_t *state);

/* Optional deterministic test barriers. Callbacks run without the queue
 * mutex; their storage must outlive the producer, including reset/drain. */
typedef struct sixel_refresh_spatial_hooks {
    void (*before)(void *, uint64_t);
    void (*published)(void *, uint64_t);
    void *context;
} sixel_refresh_spatial_hooks_t;

/* Submit at most every four encoded frames, replacing pending work. Never
 * wait for map generation. Missing or older-than-eight-frame maps use A1.
 * A reset (frame zero) drains the old producer before accepting new input. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_refresh_spatial_prepare(sixel_refresh_spatial_t **state,
    sixel_allocator_t *allocator, unsigned char const *rgb,
    sixel_delta_residual_t *schedule, unsigned int period,
    unsigned int strength, unsigned int const *band_periods,
    sixel_refresh_spatial_hooks_t const *hooks);

/* Extend the low band to 16J (G1/G3/G4) or 32J (G2) in active cells.
 * G3/G4 also extend the middle band to 8J, with a luma gate for G4.
 * PERIOD is the same base J used to build the A1 threshold tile. */
SIXEL_INTERNAL_API void
sixel_refresh_spatial_apply(sixel_delta_residual_t *schedule,
    uint32_t const activity[128], unsigned int period, unsigned int strength,
    unsigned int const *band_periods);

/* Pure map builder, also used by fixtures. Scratch covers 2*w*h plus two
 * ceil(w/4)*ceil(h/4) byte planes. The caller checks dimensions/allocation. */
SIXEL_INTERNAL_API void
sixel_refresh_spatial_build(unsigned char const *rgb, int w, int h,
    unsigned char *scratch, uint32_t activity[128]);

#endif
