/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_ENCODER_PALETTE_AUTO_H
#define LIBSIXEL_ENCODER_PALETTE_AUTO_H

#include <sixel.h>

/* Five samples reject two isolated outliers. Confirmed choices back off
 * from forty to 320 frames, bounding probes to 5/(320+5) at the cap while
 * retaining periodic observations. Times and margin remain relative. */
#define SIXEL_PALETTE_AUTO_K 5
#define SIXEL_PALETTE_AUTO_M 40
#define SIXEL_PALETTE_AUTO_MAX_INTERVAL 320
#define SIXEL_PALETTE_AUTO_MARGIN 0.12

/* Only the ordered emission owner changes this state. Preparation borrows
 * effective, which is constant until that helper finishes. */
typedef struct sixel_palette_auto {
    double intervals[SIXEL_PALETTE_AUTO_K];
    int waits[SIXEL_PALETTE_AUTO_K];
    double baseline;
    double measured;
    int count;
    int cursor;
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
/* Do not speculate across a possible decision boundary: the next prepare
 * must use the resulting mode. This cost remains in the measured interval. */
SIXEL_INTERNAL_API int
sixel_palette_auto_boundary(sixel_palette_auto_t const *state, double wait);
/* Durations are inputs, allowing deterministic tests without a fake sleep.
 * An invalid/unavailable monotonic interval cannot influence a decision. */
SIXEL_INTERNAL_API void
sixel_palette_auto_observe(sixel_palette_auto_t *state, double interval,
    double wait);

#endif /* LIBSIXEL_ENCODER_PALETTE_AUTO_H */
