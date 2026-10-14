/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_PROGRESSIVE_PREDICT_H
#define LIBSIXEL_PROGRESSIVE_PREDICT_H

#include <sixel.h>
#include <stdint.h>

#define SIXEL_REPAIR_PREDICT_BANDS 16
#define SIXEL_REPAIR_PREDICT_SAMPLES 256

typedef struct sixel_repair_predict_point {
    uint64_t priority;
    size_t position;
    float target[3];
    double error;
} sixel_repair_predict_point_t;

typedef struct sixel_repair_predict_band {
    size_t population;
    size_t count;
    double sum;
    double maximum;
    /* Bottom-k position hashes, stored as a max heap. Unlike a fixed stride,
     * selection does not synchronize with a periodic image pattern. */
    sixel_repair_predict_point_t points[SIXEL_REPAIR_PREDICT_SAMPLES];
} sixel_repair_predict_band_t;

/* Private diagnostic state: exact residual-band populations with paired
 * target/error samples. This is a stratified empirical joint distribution,
 * not an exact color histogram. No coordinate clipping or color averaging. */
typedef struct sixel_repair_predict_model {
    double stop_squared;
    double upper[SIXEL_REPAIR_PREDICT_BANDS - 1];
    sixel_repair_predict_band_t bands[SIXEL_REPAIR_PREDICT_BANDS];
} sixel_repair_predict_model_t;

typedef struct sixel_repair_prediction {
    size_t samples;
    uint64_t distances;
    int exact;
    double selected;
    double painted;
    double gain;
    double excess_gain;
    double resolved;
    /* Actual sampled points give a lower bound; unsampled band maxima give
     * an upper bound. A sampled maximum is never a stopping certificate. */
    double maximum_lower;
    double maximum_upper;
} sixel_repair_prediction_t;

/* Owned by the opt-in observer. Feed each eligible audited pixel once, with
 * finite target/error and nonnegative error; reset at each new full audit. */
SIXEL_INTERNAL_API void
sixel_repair_predict_reset(sixel_repair_predict_model_t *model, double stop);
SIXEL_INTERNAL_API int
sixel_repair_predict_band(sixel_repair_predict_model_t const *model,
    double error);
SIXEL_INTERNAL_API void
sixel_repair_predict_add(sixel_repair_predict_model_t *model,
    size_t position, float const target[3], double error);

/* Pure no-dither, before-packet-fitting prediction for an actual projected
 * palette. Uses count/sample_count weights separately in each band. Lower
 * quotas use nested bottom-k subsets for matched sampling comparisons.
 * Quota zero instead shares a total of 256 samples equally between occupied
 * bands, redistributing unused slots from bands with smaller populations.
 * Estimates are deliberately not clipped: validation must expose sampling
 * error, including estimates outside physically possible aggregate bounds.
 * Only a complete audit may be used; the owner supplies its revision ID. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_repair_predict(sixel_repair_predict_model_t const *model,
    float const (*palette)[3], unsigned int colors, double threshold,
    size_t quota, sixel_repair_prediction_t *prediction);

#endif
