/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/threading/encoder.md
 */
/* A two-way rendezvous proves stage overlap, not just worker creation. */
#include "../../_support/processing/gamut/gamut_filter_common.h"

typedef struct gamut_overlap_state {
    sixel_mutex_t mutex;
    sixel_cond_t cond;
    sixel_thread_identity_t caller;
    int producer_calls;
    int consumer_calls;
    int overlap;
    int budget_ok;
} gamut_overlap_state_t;

static SIXELSTATUS
gamut_overlap_apply(sixel_filter_t *filter, sixel_allocator_t *allocator,
    sixel_timeline_logger_t *logger)
{
    gamut_overlap_state_t *state;
    float *pixels;
    int producer, timeout, reached, i, count;

    (void)allocator;
    (void)logger;
    state = filter->userdata;
    producer = filter->kind == SIXEL_FILTER_KIND_COLORS;
    timeout = 0;
    sixel_mutex_lock(&state->mutex);
    state->budget_ok &= sixel_threads_resolve_fanout() == (producer ? 3 : 1);
    state->budget_ok &= sixel_thread_identity_equal(state->caller,
        sixel_thread_identity_current()) == producer;
    if (producer) {
        ++state->producer_calls;
    } else {
        ++state->consumer_calls;
    }
    sixel_cond_broadcast(&state->cond);
    reached = producer ? state->producer_calls != 2 ||
        state->consumer_calls >= 1 : state->producer_calls >= 2;
    while (!reached && !timeout) {
        timeout = sixel_cond_timedwait(&state->cond, &state->mutex, 2000);
        reached = producer ? state->consumer_calls >= 1
            : state->producer_calls >= 2;
    }
    state->overlap |= !producer && state->producer_calls >= 2 && !timeout;
    sixel_mutex_unlock(&state->mutex);
    if (timeout) {
        return SIXEL_LOGIC_ERROR;
    }
    count = sixel_frame_get_width(*filter->input.slot);
    pixels = (float *)sixel_frame_get_pixels(*filter->input.slot);
    for (i = 0; i < count * 3; ++i) {
        pixels[i] += 1.0f;
    }
    return SIXEL_OK;
}

int
test_gamut_0065_pipeline_overlap(int argc, char **argv)
{
    gamut_filter_fixture_t f = {0};
    gamut_overlap_state_t state;
    sixel_filter_t producer, consumer;
    sixel_thread_budget_scope_t scope;
    float *pixels;
    int ok;

    (void)argc;
    (void)argv;
    memset(&state, 0, sizeof(state));
    state.budget_ok = 1;
    state.caller = sixel_thread_identity_current();
    if (sixel_mutex_init(&state.mutex) != 0) {
        return EXIT_FAILURE;
    }
    if (sixel_cond_init(&state.cond) != 0) {
        sixel_mutex_destroy(&state.mutex);
        return EXIT_FAILURE;
    }
    pixels = calloc(262145 * 3, sizeof(float));
    ok = pixels != NULL && gamut_filter_init(&f, pixels, 262145,
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, SIXEL_GAMUT_RAY_TRACE);
    if (ok) {
        sixel_filter_init(&producer, "producer", SIXEL_FILTER_KIND_COLORS,
            gamut_overlap_apply, NULL, &state);
        sixel_filter_init(&consumer, "consumer", SIXEL_FILTER_KIND_GAMUT,
            gamut_overlap_apply, NULL, &state);
        producer.flags = consumer.flags = SIXEL_FILTER_FLAG_FLOAT32_TILES;
        producer.input = f.color->input;
        producer.output = f.color->output;
        consumer.input = f.gamut->input;
        consumer.output = f.gamut->output;
        /* This test needs four slots even on a two-CPU CI guest. Balanced
         * policy deliberately caps its budget at the performance CPUs. */
        ok = gamut_filter_scope(&scope, "greedy:global_budget=4");
        if (ok) {
            ok = sixel_filter_pipeline_run(&producer, &consumer,
                f.allocator, NULL) == SIXEL_OK && state.overlap &&
                state.budget_ok && state.producer_calls == 3 &&
                state.consumer_calls == 3 && pixels[0] == 2.0f &&
                pixels[262144 * 3] == 2.0f &&
                sixel_threads_resolve_fanout() == 4;
            sixel_thread_budget_leave(&scope);
        }
    }
    gamut_filter_free(&f);
    free(pixels);
    sixel_cond_destroy(&state.cond);
    sixel_mutex_destroy(&state.mutex);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
