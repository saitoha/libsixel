/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/encoder.h"
#include "src/dither.h"
#include "playback_test_common.h"
#include "palette_integration_common.h"

#if SIXEL_ENABLE_THREADS
typedef struct integration_state integration_state_t;
typedef struct integration_gate {
    integration_state_t *state;
    int sequence;
    int published;
    int open;
} integration_gate_t;

struct integration_state {
    sixel_mutex_t mutex;
    sixel_cond_t cond;
    integration_gate_t gates[5];
    playback_test_buffer_t buffer;
    playback_test_buffer_t reference;
    sixel_playback_result_t results[5];
    size_t offsets[5];
    uint64_t chosen[5];
    int scenario;
    int completed;
    int failed;
    int emitted;
    int budget;
};

/* A timeout is a failure escape, never the source of the required order. */
static void
integration_wait(integration_state_t *state, int *value)
{
    while (!*value) {
        if (sixel_cond_timedwait(&state->cond, &state->mutex, 5000) != 0) {
            state->failed = 1;
            break;
        }
    }
}

static void
integration_before_publish(void *context)
{
    integration_gate_t *gate;
    integration_state_t *state;

    gate = context;
    state = gate->state;
    if (gate->sequence != 2 || state->scenario == 1 || state->scenario == 6) {
        return;
    }
    sixel_mutex_lock(&state->mutex);
    integration_wait(state, &gate->open);
    sixel_mutex_unlock(&state->mutex);
}

static void
integration_after_publish(void *context)
{
    integration_gate_t *gate;

    gate = context;
    sixel_mutex_lock(&gate->state->mutex);
    gate->published = 1;
    sixel_cond_broadcast(&gate->state->cond);
    sixel_mutex_unlock(&gate->state->mutex);
}

static void
integration_build(void *context, uint64_t sequence,
    sixel_palette_job_test_hooks_t *hooks)
{
    integration_state_t *state;

    state = context;
    hooks->context = &state->gates[sequence];
    hooks->before_publish = integration_before_publish;
    hooks->after_publish = integration_after_publish;
    /* AUTO retries frame 1 first, so its full-frame construction metadata
     * must not suppress the later adaptive retry of frame 2. */
    if (((state->scenario == 4 || state->scenario == 6) && sequence == 2) ||
            state->scenario == 7) {
        hooks->completion_status = SIXEL_BAD_ALLOCATION;
        hooks->completion_retryable = state->scenario == 7;
    }
    if (state->scenario == 5) {
        hooks->fail_thread_create = 1;
    }
}

static void
integration_before_select(void *context, uint64_t sequence,
    sixel_palette_store_job_t *own)
{
    integration_state_t *state;

    state = context;
    if (state->scenario == 1 || state->scenario == 5 ||
            (state->scenario >= 8 && sequence == 1)) {
        /* Complete own2 after preparation, before the delay/transaction. */
        if (sixel_palette_store_job_join(own, SIXEL_OK) != SIXEL_OK) {
            state->failed = 1;
        }
    }
    if (state->scenario == 6 && sequence == 2) {
        sixel_mutex_lock(&state->mutex);
        integration_wait(state, &state->gates[2].published);
        sixel_mutex_unlock(&state->mutex);
    }
    if (state->scenario == 2 && sequence == 2) {
        /* Future 3 publishes first; frame 2 must still select palette 1. */
        sixel_mutex_lock(&state->mutex);
        integration_wait(state, &state->gates[3].published);
        sixel_mutex_unlock(&state->mutex);
    }
}

static void
integration_selected(void *context, uint64_t sequence,
    sixel_palette_selection_t const *selection)
{
    integration_state_t *state;
    sixel_dither_t *dither;
    sixel_frame_t *frame;
    sixel_output_t *output;
    SIXELSTATUS status;

    state = context;
    state->chosen[sequence] = selection->sequence;
    if (!selection->published || selection->published >= selection->selected ||
            selection->sequence > sequence) {
        state->failed = 1;
    }
    if (sequence != 2) {
        return;
    }
    if (state->scenario >= 8) {
        /* Keep own2 in flight through actual application and encoding. */
        return;
    }
    /* Release own2 only after the transaction has irrevocably selected.
     * This also proves that finish joins it before reporting completion. */
    sixel_mutex_lock(&state->mutex);
    state->gates[2].open = 1;
    sixel_cond_broadcast(&state->cond);
    sixel_mutex_unlock(&state->mutex);
    if (state->scenario != 0) {
        return;
    }
    /* Independently apply the selected snapshot to frame 2. Compare actual
     * SIXEL bytes, not merely selection metadata or successful decoding. */
    frame = playback_test_frame(1);
    output = NULL;
    dither = NULL;
    status = sixel_palette_snapshot_clone(selection->snapshot, &dither);
    if (status == SIXEL_OK) {
        status = sixel_output_new(&output, playback_test_write,
            &state->reference, NULL);
    }
    if (status == SIXEL_OK && frame != NULL) {
        sixel_dither_set_diffusion_type(dither, SIXEL_DIFFUSE_FS);
        sixel_dither_set_optimize_palette(dither, 1);
        status = sixel_encode(sixel_frame_get_pixels(frame),
            sixel_frame_get_width(frame), sixel_frame_get_height(frame),
            3, dither, output);
    }
    state->failed |= status != SIXEL_OK || frame == NULL;
    sixel_output_unref(output);
    sixel_dither_unref(dither);
    sixel_frame_unref(frame);
}

static void
integration_emitted(void *context, uint64_t sequence,
    sixel_encoding_planner_t const *planner, sixel_dither_t const *dither)
{
    integration_state_t *state;
    int emit;
    int dither_threads;
    int encode_threads;

    state = context;
    if (state->scenario < 8) {
        return;
    }
    /* Frame 1 joins its own build during selection and recovers all N.
     * Frame 2 keeps P=N/2 at the barrier, leaving exactly M=N-P and Q=0. */
    emit = sequence == 1 ? state->budget : state->budget / 2;
    dither_threads = dither->pipeline_last_dither_threads;
    encode_threads = dither->pipeline_last_encode_threads;
    state->failed |= planner->total_threads != state->budget ||
        planner->prepare_threads != state->budget ||
        planner->emit_threads != emit || planner->main_threads != emit ||
        planner->palette_threads != state->budget - emit ||
        planner->next_prepare_threads != 0 || sixel_threads_resolve() != emit;
    state->failed |= dither_threads < 1 || encode_threads < 1 ||
        dither_threads > emit || encode_threads > emit ||
        planner->pipeline_dither_threads != dither_threads ||
        planner->pipeline_encode_threads != encode_threads;
    if (dither->pipeline_last_mode == SIXEL_DITHER_ENCODE_PIPELINE) {
        state->failed |= dither_threads + encode_threads > emit;
    } else {
        state->failed |= dither->pipeline_last_mode !=
            SIXEL_DITHER_ENCODE_STAGED || dither_threads <= 1;
    }
    if (state->scenario == 9) {
        state->failed |= dither_threads != 1 || encode_threads != 2;
    }
    fprintf(stderr, "latest emit trace: frame=%llu budget=%d emit=%d "
        "dither=%d encode=%d\n", (unsigned long long)sequence,
        planner->total_threads, planner->emit_threads,
        dither_threads, encode_threads);
    ++state->emitted;
    if (sequence == 2) {
        sixel_mutex_lock(&state->mutex);
        state->failed |= state->gates[2].published ||
            state->gates[2].open || state->chosen[2] != 1;
        state->gates[2].open = 1;
        sixel_cond_broadcast(&state->cond);
        sixel_mutex_unlock(&state->mutex);
    }
}

static void
integration_done(sixel_playback_result_t const *result, void *context)
{
    integration_state_t *state;
    int sequence;

    state = context;
    sequence = ++state->completed;
    if (sequence > 4 || result->sequence != (uint64_t)sequence) {
        state->failed = 1;
        return;
    }
    state->results[sequence] = *result;
    state->offsets[sequence] = state->buffer.size;
}

static sixel_frame_t *
integration_frame(int sequence, int scenario)
{
    sixel_frame_t *frame;
    unsigned char *pixels;
    unsigned char *palette;

    if (scenario != 3 || sequence != 3) {
        return playback_test_frame(sequence - 1);
    }
    pixels = malloc(8 * 8);
    palette = malloc(6);
    frame = NULL;
    if (pixels == NULL || palette == NULL ||
            sixel_frame_new(&frame, NULL) != SIXEL_OK) {
        free(pixels);
        free(palette);
        return NULL;
    }
    memset(pixels, 1, 64);
    memset(palette, 0, 6);
    palette[4] = 255;
    if (sixel_frame_init(frame, pixels, 8, 8, SIXEL_PIXELFORMAT_PAL8,
            palette, 2) != SIXEL_OK) {
        sixel_frame_unref(frame);
        return NULL;
    }
    return frame;
}
#endif

int
palette_integration_test(int scenario)
{
#if SIXEL_ENABLE_THREADS
    integration_state_t state;
    struct sixel_encoder_palette_test_hooks hooks;
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_playback_t *playback;
    sixel_playback_config_t config;
    sixel_playback_ticket_t ticket;
    sixel_frame_t *frame;
    SIXELSTATUS status;
    uint64_t expected;
    int index;
    int count;
    int failed;
    int pass;

    memset(&state, 0, sizeof(state));
    memset(&hooks, 0, sizeof(hooks));
    encoder = NULL;
    output = NULL;
    playback = NULL;
    failed = 1;
    count = scenario == 3 ? 4 : scenario == 2 ? 3 : 2;
    state.scenario = scenario;
    if (sixel_mutex_init(&state.mutex) != 0) {
        return 1;
    }
    if (sixel_cond_init(&state.cond) != 0) {
        sixel_mutex_destroy(&state.mutex);
        return 1;
    }
    encoder = playback_test_encoder(0, "4");
    if (encoder == NULL || sixel_encoder_setopt(encoder,
            SIXEL_OPTFLAG_THREADING_POLICY,
            scenario == 2 ? "greedy:B4:palette=latest:prepare=ahead"
                         : "greedy:B2:palette=latest:prepare_workers=1") ||
            sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_LUT_POLICY, "none") ||
            sixel_output_new(&output, playback_test_write, &state.buffer,
                NULL) != SIXEL_OK) {
        goto end;
    }
    /* Cover both sampling boundaries with the same actual reuse oracle. */
    encoder->palette_sampling_policy = scenario == 7
        ? SIXEL_PALETTE_SAMPLING_AUTO : scenario == 0
        ? SIXEL_PALETTE_SAMPLING_ADAPTIVE_GRID
        : SIXEL_PALETTE_SAMPLING_FULL_FRAME;
    hooks.build = integration_build;
    hooks.before_select = integration_before_select;
    hooks.selected = integration_selected;
    hooks.emitted = integration_emitted;
    hooks.context = &state;
    encoder->palette_test_hooks = &hooks;
    (void)sixel_playback_config_init(&config, sizeof(config));
    config.palette_mode = SIXEL_PLAYBACK_PALETTE_LATEST;
    config.capacity = scenario == 2 ? 16 : scenario == 3 ? 2 : 1;
    config.done = integration_done;
    config.done_context = &state;
    /* Reattach verifies that sequence/store state cannot cross sessions. */
    for (pass = 0; pass < 2; ++pass) {
        if (scenario >= 8) {
            state.budget = scenario == 8 && pass == 1 ? 4 : 8;
            status = sixel_encoder_setopt(encoder,
                SIXEL_OPTFLAG_THREADING_POLICY, scenario == 9
                    ? "greedy:B8:palette=latest:prepare=serial:"
                      "dither=1:encode=2"
                    : pass == 0 ? "greedy:B8:palette=latest:prepare=serial"
                                : "greedy:B4:palette=latest:prepare=serial");
            if (status != SIXEL_OK) {
                goto end;
            }
            encoder->verbose = 1;
        }
        state.completed = 0;
        state.emitted = 0;
        state.buffer.size = 0;
        state.reference.size = 0;
        memset(state.chosen, 0, sizeof(state.chosen));
        for (index = 0; index < 5; ++index) {
            state.gates[index].state = &state;
            state.gates[index].sequence = index;
            state.gates[index].published = 0;
            state.gates[index].open = scenario == 5;
        }
        status = sixel_playback_new(&playback, encoder, output, &config, NULL);
        if (status != SIXEL_OK) {
            goto end;
        }
        for (index = 1; index <= count; ++index) {
            frame = integration_frame(index, scenario);
            if (frame == NULL) {
                goto end;
            }
            if (scenario == 1) {
                sixel_frame_set_delay(frame, 2);
            }
            status = sixel_playback_reserve(playback, SIXEL_PLAYBACK_WAIT,
                10000, &ticket);
            if (status == SIXEL_OK) {
                status = sixel_playback_submit_reserved(playback, &ticket,
                    frame, NULL);
            }
            sixel_frame_unref(frame);
            if (status != SIXEL_OK) {
                goto end;
            }
        }
        status = sixel_playback_drain(playback, 10000);
        sixel_playback_unref(playback);
        playback = NULL;
        if (status != (scenario == 4 || scenario == 6
                       ? SIXEL_BAD_ALLOCATION : SIXEL_OK) ||
                state.completed != count || state.failed) {
            goto end;
        }
        if (scenario >= 8 && state.emitted != count) {
            goto end;
        }
        for (index = 1; index <= count; ++index) {
            expected = index == 2 && scenario == 6 ? 0
                : index == 2 && scenario != 1 && scenario != 5 ? 1
                : (uint64_t)index;
            if (state.results[index].palette_sequence != expected ||
                    state.results[index].status !=
                        ((scenario == 4 || scenario == 6) && index == 2
                            ? SIXEL_BAD_ALLOCATION
                                                    : SIXEL_OK)) {
                goto end;
            }
        }
        if (scenario == 0 &&
                (state.offsets[2] - state.offsets[1] != state.reference.size ||
                 memcmp(state.buffer.bytes + state.offsets[1],
                    state.reference.bytes, state.reference.size))) {
            goto end;
        }
        if (!state.gates[2].published ||
                (scenario == 6 ? state.offsets[2] != state.offsets[1]
                               : state.offsets[2] <= state.offsets[1])) {
            goto end;
        }
    }
    failed = 0;
end:
    if (failed) {
        fprintf(stderr, "latest integration scenario %d failed (%d callbacks, "
            "chosen %llu/%llu/%llu/%llu)\n", scenario, state.completed,
            (unsigned long long)state.chosen[1],
            (unsigned long long)state.chosen[2],
            (unsigned long long)state.chosen[3],
            (unsigned long long)state.chosen[4]);
    }
    sixel_mutex_lock(&state.mutex);
    state.gates[2].open = 1;
    sixel_cond_broadcast(&state.cond);
    sixel_mutex_unlock(&state.mutex);
    sixel_playback_unref(playback);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    sixel_cond_destroy(&state.cond);
    sixel_mutex_destroy(&state.mutex);
    free(state.buffer.bytes);
    free(state.reference.bytes);
    return failed;
#else
    (void)scenario;
    return 0;
#endif
}
