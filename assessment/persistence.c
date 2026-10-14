/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <sixel.h>
#include "persistence.h"

/* Integrate min(age / ramp, 1) exactly on each constant-error interval.
 * Splitting an interval at a reference or packet boundary must not change
 * its contribution. A frame-count counter would make variable FPS and
 * palette-only packets change the score without changing the display. */
SIXEL_INTERNAL_API void
lsqa_persistence_step(double const *errors, double *ages, size_t pixels,
    double duration, double threshold, double ramp,
    lsqa_persistence_result_t *result)
{
    size_t i;
    double remaining;
    double fraction;
    double weight;

    result->weighted_excess = 0;
    result->persistent_fraction = 0;
    result->max_age = 0;
    for (i = 0; i < pixels; ++i) {
        if (errors[i] <= threshold) {
            ages[i] = 0;
            continue;
        }
        remaining = ramp > ages[i] ? ramp - ages[i] : 0;
        if (remaining > duration) { remaining = duration; }
        fraction = remaining / duration;
        weight = 1.0 - fraction;
        if (remaining > 0) {
            /* Ratios avoid squaring timestamps or multiplying by duration,
             * either of which could overflow on valid long timelines. */
            weight += fraction * (ages[i] / ramp + 0.5 * (remaining / ramp));
        }
        result->weighted_excess +=
            ((errors[i] - threshold) / (double)pixels) * weight;
        result->persistent_fraction += (1.0 - fraction) / (double)pixels;
        ages[i] += duration;
        if (ages[i] > result->max_age) { result->max_age = ages[i]; }
    }
}
