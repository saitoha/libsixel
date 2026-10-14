/* SPDX-License-Identifier: MIT */
#ifndef SIXEL_ASSESSMENT_PERSISTENCE_H
#define SIXEL_ASSESSMENT_PERSISTENCE_H

#include <stddef.h>

typedef struct lsqa_persistence_result {
    double weighted_excess;
    double persistent_fraction;
    double max_age;
} lsqa_persistence_result_t;

/* Screen-coordinate diagnostic, not a motion-compensated perceptual metric.
 * Errors are constant over this interval. Ages start at zero and survive
 * adjacent intervals; error <= threshold resets that pixel's age. Inputs
 * must be finite/nonnegative, with positive pixels and duration. The caller
 * guarantees that accumulated ages remain representable. */
SIXEL_INTERNAL_API void
lsqa_persistence_step(double const *errors, double *ages, size_t pixels,
    double duration, double threshold, double ramp,
    lsqa_persistence_result_t *result);

#endif
