/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_ENCODER_PALETTE_AUTO_H
#define LIBSIXEL_ENCODER_PALETTE_AUTO_H

#include <sixel.h>

/* Five ABBA blocks provide five local, order-balanced contrasts. Their
 * median tolerates two contaminated blocks; the MAD sets a relative noise
 * margin. The 3% floor admits modest stable gains while guarding against
 * the roughly 2% fixed-mode repeat variation.
 * Forty chosen frames bound retry latency; only decisive confirmations
 * double the interval, up to 320. These are heuristics, not confidence
 * intervals or promises about every workload. */
#define SIXEL_PALETTE_AUTO_K 5
#define SIXEL_PALETTE_AUTO_PROBE_FRAMES (4 * SIXEL_PALETTE_AUTO_K)
#define SIXEL_PALETTE_AUTO_M 40
#define SIXEL_PALETTE_AUTO_MAX_INTERVAL 320
#define SIXEL_PALETTE_AUTO_MARGIN 0.03

/* Only the ordered emission owner reads or changes this state. Preparation
 * receives a frozen mode, including predictable transitions within a probe. */
typedef struct sixel_palette_auto {
    double differences[SIXEL_PALETTE_AUTO_K];
    double block[4];
    int waits[SIXEL_PALETTE_AUTO_K];
    double measured;
    double mad;
    double margin;
    int count;
    int cursor;
    int step;
    int pairs;
    int elapsed;
    int reevaluate_interval;
    int evaluated;
    int chosen;
    int effective;
    int probe;
    unsigned int probes;
    unsigned int switches;
    unsigned int transitions;
    char const *decision;
} sixel_palette_auto_t;

SIXEL_INTERNAL_API void
sixel_palette_auto_init(sixel_palette_auto_t *state);
/* The wait is only max(0, palette_done - max(preprocess_done, emit_end)).
 * Preprocessing time alone must never trigger a latest probe. */
SIXEL_INTERNAL_API double
sixel_palette_auto_wait(double preprocess_done, double palette_done,
    double previous_emit_end);
/* Only the final probe completion needs a hold. Interior ABBA modes are
 * known before measuring the current interval, so preparation can overlap. */
SIXEL_INTERNAL_API int
sixel_palette_auto_boundary(sixel_palette_auto_t const *state, double wait);
SIXEL_INTERNAL_API int
sixel_palette_auto_next(sixel_palette_auto_t const *state);
/* Durations are inputs, allowing deterministic tests without a fake sleep.
 * An invalid/unavailable monotonic interval cannot influence a decision. */
SIXEL_INTERNAL_API void
sixel_palette_auto_observe(sixel_palette_auto_t *state, double interval,
    double wait);

#endif /* LIBSIXEL_ENCODER_PALETTE_AUTO_H */
