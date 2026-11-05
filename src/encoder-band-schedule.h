/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_ENCODER_BAND_SCHEDULE_H
#define LIBSIXEL_ENCODER_BAND_SCHEDULE_H

/* EncoderCore owns this history, independently of the thread pool. The target
 * bounds one job's estimated work; it is not a video frame-rate assumption.
 * Invalid clocks and failed frames return to single-band admission. */
typedef struct sixel_band_schedule {
    double seconds_per_band;
    int quantum;
    int light_frames;
} sixel_band_schedule_t;

static void
sixel_band_schedule_observe(sixel_band_schedule_t *schedule,
    double seconds, int bands)
{
    double sample;
    double target;
    int desired;

    target = 0.000200;
    if (!(seconds > 0.0) || seconds > 60.0 || bands < 1) {
        schedule->seconds_per_band = 0.0;
        schedule->quantum = 1;
        schedule->light_frames = 0;
        return;
    }
    sample = seconds / bands;
    if (!(schedule->seconds_per_band > 0.0)) {
        schedule->seconds_per_band = sample;
    } else {
        schedule->seconds_per_band +=
            (sample - schedule->seconds_per_band) * 0.25;
    }
    if (schedule->quantum < 1) schedule->quantum = 1;
    /* A sudden expensive scene shrinks immediately. Expansion requires two
     * light observations and headroom, so alternating costs do not chatter. */
    desired = schedule->quantum;
    while (desired > 1 && (sample * desired > target * 1.2 ||
            schedule->seconds_per_band * desired > target)) {
        desired /= 2;
    }
    if (desired < schedule->quantum) {
        schedule->quantum = desired;
        schedule->light_frames = 0;
    } else if (desired < 4 &&
            sample * (desired * 2) < target * 0.8 &&
            schedule->seconds_per_band * (desired * 2) < target * 0.8) {
        if (++schedule->light_frames == 2) {
            schedule->quantum *= 2;
            schedule->light_frames = 0;
        }
    } else {
        schedule->light_frames = 0;
    }
}

static int
sixel_band_schedule_span(sixel_band_schedule_t const *schedule,
    int ready, int remaining, int workers, int queue_bands)
{
    int span;

    span = schedule->quantum;
    if (span != 2 && span != 4) span = 1;
    /* Never hold published bands for a future producer callback. At the tail,
     * retain at least one job per worker instead of stranding available CPUs.
     * Division avoids overflow for large explicit worker reservations. */
    while (span > 1 && (span > ready || span > queue_bands ||
            remaining / span < workers)) {
        span /= 2;
    }
    return span;
}

#endif
