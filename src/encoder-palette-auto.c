/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <string.h>
#include <math.h>
#include "encoder-palette-auto.h"

void
sixel_palette_auto_init(sixel_palette_auto_t *state)
{
    memset(state, 0, sizeof(*state));
    state->reevaluate_interval = SIXEL_PALETTE_AUTO_M;
    state->decision = "warmup";
}

double
sixel_palette_auto_wait(double preprocess_done, double palette_done,
    double previous_emit_end)
{
    double ready;

    ready = preprocess_done > previous_emit_end
        ? preprocess_done : previous_emit_end;
    return palette_done > ready && ready > 0.0
        ? palette_done - ready : 0.0;
}

static double
sixel_palette_auto_median(double const *values)
{
    double sorted[SIXEL_PALETTE_AUTO_K];
    double value;
    int i;
    int j;

    memcpy(sorted, values, sizeof(sorted));
    for (i = 1; i < SIXEL_PALETTE_AUTO_K; ++i) {
        value = sorted[i];
        for (j = i; j > 0 && sorted[j - 1] > value; --j) {
            sorted[j] = sorted[j - 1];
        }
        sorted[j] = value;
    }
    return sorted[SIXEL_PALETTE_AUTO_K / 2];
}

int
sixel_palette_auto_boundary(sixel_palette_auto_t const *state, double wait)
{
    int i;
    int waiting;

    if (state->probe) {
        return state->count + 1 >= SIXEL_PALETTE_AUTO_K;
    }
    if (state->count + 1 < SIXEL_PALETTE_AUTO_K ||
            (state->evaluated &&
             state->elapsed + 1 < state->reevaluate_interval)) {
        return 0;
    }
    waiting = wait > 0.0;
    for (i = 0; i < state->count; ++i) {
        if (i != state->cursor) {
            waiting |= state->waits[i];
        }
    }
    return state->chosen || waiting;
}

void
sixel_palette_auto_observe(sixel_palette_auto_t *state, double interval,
    double wait)
{
    int i;
    int waiting;

    state->decision = "sample";
    if (!(interval > 0.0) || !isfinite(interval)) {
        state->decision = "clock-unavailable";
        return;
    }
    state->intervals[state->cursor] = interval;
    state->waits[state->cursor] = wait > 0.0;
    state->cursor = (state->cursor + 1) % SIXEL_PALETTE_AUTO_K;
    if (state->count < SIXEL_PALETTE_AUTO_K) {
        ++state->count;
    }
    /* Saturate on long sessions while waiting for a usable frame window. */
    if (state->elapsed < state->reevaluate_interval) {
        ++state->elapsed;
    }
    if (state->count < SIXEL_PALETTE_AUTO_K) {
        return;
    }
    state->measured = sixel_palette_auto_median(state->intervals);
    if (state->probe) {
        /* A probe is a challenger. Inside the margin the incumbent wins,
         * including when it is latest and the challenger is frame. */
        if (state->measured <
                state->baseline * (1.0 - SIXEL_PALETTE_AUTO_MARGIN)) {
            state->chosen = state->effective;
            state->reevaluate_interval = SIXEL_PALETTE_AUTO_M;
            ++state->switches;
            state->decision = state->chosen ? "switch-latest" : "switch-frame";
        } else {
            state->decision = state->chosen ? "keep-latest" : "keep-frame";
            /* The initial comparison establishes the base cadence. Only
             * a subsequent confirmation doubles the chosen period. */
            if (state->evaluated && state->reevaluate_interval <
                    SIXEL_PALETTE_AUTO_MAX_INTERVAL) {
                state->reevaluate_interval *= 2;
            }
        }
        state->transitions += state->effective != state->chosen;
        state->effective = state->chosen;
        state->probe = 0;
        state->evaluated = 1;
        state->elapsed = 0;
        state->count = 0;
        state->cursor = 0;
        return;
    }
    if (state->evaluated && state->elapsed < state->reevaluate_interval) {
        return;
    }
    waiting = 0;
    for (i = 0; i < SIXEL_PALETTE_AUTO_K; ++i) {
        waiting |= state->waits[i];
    }
    if (!state->chosen && !waiting) {
        state->decision = "skip-no-wait";
        /* A no-wait reevaluation also confirms frame, without running a
         * challenger. Before the first comparison, keep observing the full
         * K-sample window; wait magnitude never shortens that decision. */
        if (state->evaluated && state->reevaluate_interval <
                SIXEL_PALETTE_AUTO_MAX_INTERVAL) {
            state->reevaluate_interval *= 2;
        }
        state->elapsed = 0;
        return;
    }
    state->baseline = state->measured;
    state->effective = !state->chosen;
    ++state->transitions;
    state->probe = 1;
    ++state->probes;
    state->count = 0;
    state->cursor = 0;
    state->decision = state->effective ? "probe-latest" : "probe-frame";
}
