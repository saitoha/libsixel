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

/* A is the incumbent, B the challenger. Opposite adjacent-pair orders
 * reduce content-trend bias without comparing separated content windows. */
static int
sixel_palette_auto_mode(sixel_palette_auto_t const *state, int step)
{
    return state->chosen ^ (step % 4 == 1 || step % 4 == 2);
}

int
sixel_palette_auto_next(sixel_palette_auto_t const *state)
{
    return state->probe
        ? sixel_palette_auto_mode(state, state->step + 1) : state->chosen;
}

int
sixel_palette_auto_boundary(sixel_palette_auto_t const *state, double wait)
{
    (void)wait;
    return state->probe &&
        state->step + 1 == SIXEL_PALETTE_AUTO_PROBE_FRAMES;
}

static void
sixel_palette_auto_decide(sixel_palette_auto_t *state)
{
    double deviations[SIXEL_PALETTE_AUTO_K];
    int i;

    state->decision = "uncertain";
    if (state->pairs == SIXEL_PALETTE_AUTO_K) {
        state->measured = sixel_palette_auto_median(state->differences);
        for (i = 0; i < SIXEL_PALETTE_AUTO_K; ++i) {
            deviations[i] = fabs(state->differences[i] - state->measured);
        }
        state->mad = sixel_palette_auto_median(deviations);
        state->margin = SIXEL_PALETTE_AUTO_MARGIN + 2.0 * state->mad;
        if (state->measured > state->margin) {
            state->chosen = !state->chosen;
            state->reevaluate_interval = SIXEL_PALETTE_AUTO_M;
            ++state->switches;
            state->decision = state->chosen ? "switch-latest" : "switch-frame";
        } else if (state->measured < -state->margin) {
            state->decision = state->chosen ? "keep-latest" : "keep-frame";
            if (state->evaluated && state->reevaluate_interval <
                    SIXEL_PALETTE_AUTO_MAX_INTERVAL) {
                state->reevaluate_interval *= 2;
            }
        }
    }
    state->probe = 0;
    state->evaluated = 1;
    state->elapsed = 0;
    state->count = 0;
    state->cursor = 0;
}

void
sixel_palette_auto_observe(sixel_palette_auto_t *state, double interval,
    double wait)
{
    double first;
    double second;
    int i;
    int waiting;
    int next;

    state->decision = "sample";
    if (state->probe) {
        /* Even invalid clocks advance the preannounced schedule. A block
         * containing one cannot vote, and an incomplete probe cannot decide
         * or back off. No speculative job depends on clock validity. */
        state->block[state->step % 4] = interval;
        if (state->step % 4 == 3) {
            for (i = 0; i < 4; ++i) {
                if (!(state->block[i] > 0.0) || !isfinite(state->block[i])) {
                    break;
                }
            }
            if (i == 4) {
                /* Scale each pair before division to avoid overflow. Each
                 * signed contrast is 2*(A-B)/(A+B); averaging AB and BA
                 * balances order and keeps all comparisons dimensionless. */
                first = state->block[0] / fmax(state->block[0],
                    state->block[1]);
                second = state->block[1] / fmax(state->block[0],
                    state->block[1]);
                state->differences[state->pairs] =
                    (first - second) / (first + second);
                first = state->block[3] / fmax(state->block[3],
                    state->block[2]);
                second = state->block[2] / fmax(state->block[3],
                    state->block[2]);
                state->differences[state->pairs++] +=
                    (first - second) / (first + second);
            }
        }
        ++state->step;
        if (state->step == SIXEL_PALETTE_AUTO_PROBE_FRAMES) {
            sixel_palette_auto_decide(state);
        }
        next = state->probe
            ? sixel_palette_auto_mode(state, state->step) : state->chosen;
        state->transitions += state->effective != next;
        state->effective = next;
        return;
    }
    if (!(interval > 0.0) || !isfinite(interval)) {
        state->decision = "clock-unavailable";
        return;
    }
    state->waits[state->cursor] = wait > 0.0;
    state->cursor = (state->cursor + 1) % SIXEL_PALETTE_AUTO_K;
    if (state->count < SIXEL_PALETTE_AUTO_K) {
        ++state->count;
    }
    /* Saturate on long sessions while waiting for a usable frame window. */
    if (state->elapsed < state->reevaluate_interval) {
        ++state->elapsed;
    }
    if (state->count < SIXEL_PALETTE_AUTO_K ||
            (state->evaluated &&
             state->elapsed < state->reevaluate_interval)) {
        return;
    }
    waiting = 0;
    for (i = 0; i < SIXEL_PALETTE_AUTO_K; ++i) {
        waiting |= state->waits[i];
    }
    if (!state->chosen && !waiting) {
        state->decision = "skip-no-wait";
        /* No critical palette wait decisively rules out useful overlap.
         * Keep the initial window rolling until the first wait appears. */
        if (state->evaluated && state->reevaluate_interval <
                SIXEL_PALETTE_AUTO_MAX_INTERVAL) {
            state->reevaluate_interval *= 2;
        }
        state->elapsed = 0;
        return;
    }
    state->probe = 1;
    ++state->probes;
    state->step = 0;
    state->pairs = 0;
    state->measured = state->mad = state->margin = 0.0;
    state->decision = "probe-paired";
}
