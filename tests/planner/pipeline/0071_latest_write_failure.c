/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/encoder.h"
#include "playback_test_common.h"

#if SIXEL_ENABLE_THREADS
typedef struct latest_write_state {
    sixel_mutex_t mutex;
    sixel_cond_t cond;
    sixel_playback_result_t results[3];
    uint64_t selected;
    int submitted;
    int completed;
    int release;
    int published;
    int failed;
    int writes_failed;
} latest_write_state_t;

/* Timeouts bound a broken test; barriers, not delays, establish ordering. */
static void
latest_write_wait(latest_write_state_t *state, int *value)
{
    while (!*value) {
        if (sixel_cond_timedwait(&state->cond, &state->mutex, 5000)) {
            state->failed = 1;
            break;
        }
    }
}

static void
latest_write_before_publish(void *context)
{
    latest_write_state_t *state;

    state = context;
    sixel_mutex_lock(&state->mutex);
    latest_write_wait(state, &state->release);
    sixel_mutex_unlock(&state->mutex);
}

static void
latest_write_after_publish(void *context)
{
    latest_write_state_t *state;

    state = context;
    sixel_mutex_lock(&state->mutex);
    state->published = 1;
    sixel_mutex_unlock(&state->mutex);
}

static void
latest_write_build(void *context, uint64_t sequence,
    sixel_palette_job_test_hooks_t *hooks)
{
    if (sequence == 2) {
        hooks->context = context;
        hooks->before_publish = latest_write_before_publish;
        hooks->after_publish = latest_write_after_publish;
    }
}

static void
latest_write_selected(void *context, uint64_t sequence,
    sixel_palette_selection_t const *selection)
{
    latest_write_state_t *state;

    state = context;
    sixel_mutex_lock(&state->mutex);
    state->selected = sequence;
    state->failed |= selection->sequence != 1 || sequence > 2;
    sixel_mutex_unlock(&state->mutex);
}

static int
latest_write(char *data, int size, void *context)
{
    latest_write_state_t *state;
    int fail;

    (void)data;
    state = context;
    sixel_mutex_lock(&state->mutex);
    fail = state->selected == 2;
    if (fail) {
        /* Queue frame 3 before failing frame 2, whose own build is still
         * held. Cleanup must join it and interrupt the queued admission. */
        latest_write_wait(state, &state->submitted);
        state->release = 1;
        ++state->writes_failed;
        sixel_cond_broadcast(&state->cond);
    }
    sixel_mutex_unlock(&state->mutex);
    return fail ? -1 : size;
}

static void
latest_write_done(sixel_playback_result_t const *result, void *context)
{
    latest_write_state_t *state;

    state = context;
    if (state->completed >= 3) {
        state->failed = 1;
        return;
    }
    state->results[state->completed++] = *result;
}
#endif

int
test_latest_write_0071(int argc, char **argv)
{
#if SIXEL_ENABLE_THREADS
    latest_write_state_t state;
    struct sixel_encoder_palette_test_hooks hooks;
    sixel_playback_config_t config;
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_playback_t *playback;
    sixel_frame_t *frame;
    SIXELSTATUS status;
    int index;
    int failed;

    (void)argc;
    (void)argv;
    memset(&state, 0, sizeof(state));
    memset(&hooks, 0, sizeof(hooks));
    encoder = NULL;
    output = NULL;
    playback = NULL;
    failed = 1;
    if (sixel_mutex_init(&state.mutex)) {
        return 1;
    }
    if (sixel_cond_init(&state.cond)) {
        sixel_mutex_destroy(&state.mutex);
        return 1;
    }
    encoder = playback_test_encoder(0, "2");
    if (encoder == NULL || sixel_encoder_setopt(encoder,
            SIXEL_OPTFLAG_THREADING_POLICY,
            "greedy:B2:palette=latest:prepare=serial:prepare_workers=1") ||
            sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_LUT_POLICY, "none") ||
            sixel_output_new(&output, latest_write, &state, NULL)) {
        goto end;
    }
    encoder->palette_sampling_policy = SIXEL_PALETTE_SAMPLING_FULL_FRAME;
    hooks.context = &state;
    hooks.build = latest_write_build;
    hooks.selected = latest_write_selected;
    encoder->palette_test_hooks = &hooks;
    (void)sixel_playback_config_init(&config, sizeof(config));
    config.palette_mode = SIXEL_PLAYBACK_PALETTE_LATEST;
    config.capacity = 4;
    config.done = latest_write_done;
    config.done_context = &state;
    if (sixel_playback_new(&playback, encoder, output, &config, NULL)) {
        goto end;
    }
    for (index = 0; index < 3; ++index) {
        frame = playback_test_frame(index);
        if (frame == NULL) {
            goto end;
        }
        status = sixel_playback_submit(playback, frame,
            SIXEL_PLAYBACK_WAIT, 10000, NULL);
        sixel_frame_unref(frame);
        if (status != SIXEL_OK) {
            goto end;
        }
    }
    sixel_mutex_lock(&state.mutex);
    state.submitted = 1;
    sixel_cond_broadcast(&state.cond);
    sixel_mutex_unlock(&state.mutex);
    status = sixel_playback_drain(playback, 10000);
    if (!SIXEL_FAILED(status) || status == SIXEL_TIMEOUT ||
            state.completed != 3 || state.failed || !state.published ||
            !state.writes_failed || state.results[0].status != SIXEL_OK ||
            state.results[1].status != status ||
            state.results[2].status != SIXEL_INTERRUPTED ||
            state.results[0].palette_sequence != 1 ||
            state.results[1].palette_sequence != 1 ||
            state.results[2].palette_sequence != 0) {
        goto end;
    }
    for (index = 0; index < 3; ++index) {
        if (state.results[index].sequence != (uint64_t)index + 1) {
            goto end;
        }
    }
    frame = playback_test_frame(3);
    if (frame == NULL) {
        goto end;
    }
    failed = sixel_playback_submit(playback, frame,
        SIXEL_PLAYBACK_NOWAIT, 0, NULL) != status ||
        sixel_playback_drain(playback, 10000) != status;
    sixel_frame_unref(frame);
end:
    sixel_mutex_lock(&state.mutex);
    state.release = state.submitted = 1;
    sixel_cond_broadcast(&state.cond);
    sixel_mutex_unlock(&state.mutex);
    sixel_playback_unref(playback);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    sixel_cond_destroy(&state.cond);
    sixel_mutex_destroy(&state.mutex);
    if (failed) {
        fprintf(stderr, "latest write failure cleanup/provenance failed\n");
    }
    return failed;
#else
    (void)argc;
    (void)argv;
    return 77;
#endif
}
