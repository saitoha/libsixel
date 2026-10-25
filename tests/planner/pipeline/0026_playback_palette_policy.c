/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/threading.h"
#include "playback_test_common.h"

typedef struct playback_0026_state {
    playback_test_buffer_t buffer;
    int expected_mode;
    int wrong_mode;
    int callbacks;
    int wrong_result;
} playback_0026_state_t;

/* Observe the policy at execution, so acceptance alone cannot pass. */
static int
playback_0026_write(char *data, int size, void *context)
{
    playback_0026_state_t *state;

    state = (playback_0026_state_t *)context;
    if (sixel_threading_policy_palette_mode() != state->expected_mode) {
        state->wrong_mode = 1;
    }
    return playback_test_write(data, size, &state->buffer);
}

static void
playback_0026_done(sixel_playback_result_t const *result, void *context)
{
    playback_0026_state_t *state;

    state = (playback_0026_state_t *)context;
    ++state->callbacks;
    if (result->status != SIXEL_OK ||
            result->sequence != (uint64_t)state->callbacks ||
            result->palette_sequence != result->sequence) {
        state->wrong_result = 1;
    }
}

/* Compare explicit frame with a requested session, then check detach.
 * Frames deliberately have multiframe=0, as public playback accepts RGB
 * frames independently of loader animation metadata. */
static int
playback_0026_compare(int encoder_mode, int config_mode, int budget)
{
    playback_0026_state_t state;
    playback_test_buffer_t reference;
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_playback_t *playback;
    sixel_playback_config_t config;
    sixel_frame_t *frame;
    SIXELSTATUS status;
    char policy[96];
    int pass;
    int index;
    int failed;

    memset(&state, 0, sizeof(state));
    memset(&reference, 0, sizeof(reference));
    encoder = NULL;
    output = NULL;
    playback = NULL;
    failed = 1;
    (void)snprintf(policy, sizeof(policy),
        "greedy:B%d:palette=%s:prepare=%s", budget,
        encoder_mode ? "latest" : "frame", budget == 1 ? "serial" : "ahead");
    encoder = playback_test_encoder(0, "1");
    if (encoder == NULL ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder,
                SIXEL_OPTFLAG_THREADING_POLICY, policy)) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder,
                SIXEL_OPTFLAG_LUT_POLICY, "none")) ||
            SIXEL_FAILED(sixel_output_new(&output, playback_0026_write,
                                          &state, NULL))) {
        goto end;
    }
    for (pass = 0; pass < 2; ++pass) {
        (void)sixel_playback_config_init(&config, sizeof(config));
        config.capacity = 2u;
        config.palette_mode = pass == 0 || config_mode < 0
            ? SIXEL_PLAYBACK_PALETTE_PER_FRAME : config_mode;
        config.done = playback_0026_done;
        config.done_context = &state;
        state.expected_mode = pass == 0 ? SIXEL_THREADING_PALETTE_FRAME
            : config_mode < 0 ? encoder_mode : config_mode;
        state.callbacks = 0;
        status = sixel_playback_new(&playback, encoder, output,
            pass != 0 && config_mode < 0 ? NULL : &config, NULL);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        for (index = 0; index < 3; ++index) {
            frame = playback_test_frame(index);
            if (frame == NULL) {
                goto end;
            }
            status = sixel_playback_submit(playback, frame,
                SIXEL_PLAYBACK_WAIT, -1, NULL);
            sixel_frame_unref(frame);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
        }
        status = sixel_playback_drain(playback, -1);
        sixel_playback_unref(playback);
        playback = NULL;
        if (SIXEL_FAILED(status) || state.wrong_mode || state.wrong_result ||
                state.buffer.size == 0u ||
                state.callbacks != (pass != 0 && config_mode < 0 ? 0 : 3)) {
            goto end;
        }
        if (pass == 0) {
            reference = state.buffer;
            memset(&state.buffer, 0, sizeof(state.buffer));
        }
    }
    if (reference.size != state.buffer.size ||
            memcmp(reference.bytes, state.buffer.bytes, reference.size)) {
        fprintf(stderr, "latest frame fallback differs\n");
        goto end;
    }

    /* Session overrides must not leak into subsequent direct encoding. */
    state.expected_mode = encoder_mode;
    frame = playback_test_frame(0);
    if (frame == NULL) {
        goto end;
    }
    status = sixel_encoder_encode_frame(encoder, frame, output);
    sixel_frame_unref(frame);
    failed = SIXEL_FAILED(status) || state.wrong_mode;
end:
    sixel_playback_unref(playback);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    free(state.buffer.bytes);
    free(reference.bytes);
    if (failed) {
        fprintf(stderr, "palette policy: encoder=%d config=%d budget=%d\n",
                encoder_mode, config_mode, budget);
    }
    return failed;
}

int
test_playback_0026(int argc, char **argv)
{
    int encoder_mode;
    int config_mode;
    int budget;

    (void)argc;
    (void)argv;
    for (budget = 1; budget <= 4; budget += 3) {
        for (encoder_mode = 0; encoder_mode <= 1; ++encoder_mode) {
            for (config_mode = -1; config_mode <= 1; ++config_mode) {
                if (playback_0026_compare(encoder_mode, config_mode,
                                          budget)) {
                    return EXIT_FAILURE;
                }
            }
        }
    }
    return EXIT_SUCCESS;
}
