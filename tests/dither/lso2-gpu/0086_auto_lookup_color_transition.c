/* SPDX-License-Identifier: MIT */
#include "tests/_support/lso2-gpu/rendezvous.h"
#include <stdlib.h>
#include <string.h>

#if SIXEL_ENABLE_THREADS && SIXEL_ENCODER_PREPARE_AHEAD_SUPPORTED \
    && SIXEL_ATOMIC_U32_LOCK_FREE
struct auto_lookup_transition {
    sixel_playback_t *playback;
    int count;
    int failed;
    int solves;
};

static void
auto_lookup_transition_done(sixel_playback_result_t const *result, void *priv)
{
    struct auto_lookup_transition *state;
    sixel_playback_pipeline_sample_t sample;
    unsigned int required;

    state = priv;
    memset(&sample, 0, sizeof(sample));
    sample.struct_size = sizeof(sample);
    required = SIXEL_PLAYBACK_PIPELINE_INDEPENDENT |
        SIXEL_PLAYBACK_PIPELINE_RESOLVE | SIXEL_PLAYBACK_PIPELINE_ENCODE;
    ++state->count;
    state->failed |= result->status != SIXEL_OK ||
        result->sequence != (uint64_t)state->count ||
        sixel_playback_get_pipeline_sample(state->playback,
            result->sequence, &sample) != SIXEL_OK ||
        (sample.valid & required) != required;
}

static void
auto_lookup_transition_solved(void *priv, uint64_t sequence)
{
    struct auto_lookup_transition *state;

    state = priv;
    /* The exact palette in the middle needs no independent solve. */
    state->solves |= sequence == 1 ? 1 : sequence == 3 ? 2 : 4;
}

static int
auto_lookup_transition_run(char const *diffusion, int ahead,
                           playback_test_buffer_t *buffer)
{
    struct auto_lookup_transition state = { 0 };
    struct sixel_encoder_solve_test_hooks hooks = { 0 };
    sixel_playback_config_t config;
    sixel_playback_options_t options;
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    unsigned char solid[16 * 16 * 3];
    int i, ok;

    encoder = NULL;
    output = NULL;
    frame = NULL;
    ok = 0;
    memset(solid, 128, sizeof(solid));
    AHEAD_CHECK(sixel_encoder_new(&encoder, NULL) == SIXEL_OK);
    AHEAD_CHECK(sixel_encoder_setopt(encoder, '=', ahead
        ? "balanced:B4:prepare=ahead" : "balanced:B4:prepare=serial")
        == SIXEL_OK);
    AHEAD_CHECK(sixel_encoder_setopt(encoder, 'G', "force") == SIXEL_OK);
    AHEAD_CHECK(sixel_encoder_setopt(encoder, 'Z', "delta:T0") == SIXEL_OK);
    AHEAD_CHECK(sixel_encoder_setopt(encoder, 'd', diffusion) == SIXEL_OK);
    AHEAD_CHECK(sixel_encoder_setopt(encoder, '~', "auto") == SIXEL_OK);
    AHEAD_CHECK(sixel_encoder_setopt(encoder, 'p', "64") == SIXEL_OK);
    AHEAD_CHECK(sixel_encoder_set_6delta_plane_size(encoder, 24, 24)
        == SIXEL_OK);
    AHEAD_CHECK(sixel_output_new(&output, playback_test_write, buffer, NULL)
        == SIXEL_OK);
    if (ahead) {
        hooks.completed = auto_lookup_transition_solved;
        hooks.context = &state;
        encoder->solve_test_hooks = &hooks;
        AHEAD_CHECK(sixel_playback_config_init(&config, sizeof(config))
            == SIXEL_OK);
        config.capacity = 2;
        config.done = auto_lookup_transition_done;
        config.done_context = &state;
        AHEAD_CHECK(sixel_playback_new(&state.playback, encoder, output,
            &config, NULL) == SIXEL_OK);
    }
    AHEAD_CHECK(sixel_playback_options_init(&options, sizeof(options))
        == SIXEL_OK);
    options.flags = SIXEL_PLAYBACK_OPTION_ORIGIN;
    for (i = 0; i < 3; ++i) {
        if (i == 1) {
            AHEAD_CHECK(sixel_frame_new(&frame, NULL) == SIXEL_OK);
            AHEAD_CHECK(sixel_frame_init_borrowed(frame, solid, 16, 16,
                SIXEL_PIXELFORMAT_RGB888, NULL, -1) == SIXEL_OK);
        } else {
            frame = playback_test_small_frame(i);
            AHEAD_CHECK(frame != NULL);
        }
        options.origin_x = i * 2;
        options.origin_y = i;
        if (!ahead) {
            AHEAD_CHECK(sixel_encoder_set_6delta_plane_origin(encoder,
                options.origin_x, options.origin_y) == SIXEL_OK);
        }
        AHEAD_CHECK((ahead ? sixel_playback_submit(state.playback, frame,
            SIXEL_PLAYBACK_WAIT, 5000, &options)
            : sixel_encoder_encode_frame(encoder, frame, output)) == SIXEL_OK);
        sixel_frame_unref(frame);
        frame = NULL;
    }
    if (ahead) {
        AHEAD_CHECK(sixel_playback_drain(state.playback, 5000) == SIXEL_OK);
        AHEAD_CHECK(!state.failed && state.count == 3 && state.solves == 3);
    }
    ok = 1;
end:
    if (state.playback != NULL) sixel_playback_unref(state.playback);
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return !ok;
}

static int
auto_lookup_transition_matches(char const *diffusion)
{
    playback_test_buffer_t direct = { 0 }, queued = { 0 };
    int ok;

    ok = 0;
    AHEAD_CHECK(!auto_lookup_transition_run(diffusion, 0, &direct));
    AHEAD_CHECK(!auto_lookup_transition_run(diffusion, 1, &queued));
    AHEAD_CHECK(direct.size > 0 && direct.size == queued.size &&
        memcmp(direct.bytes, queued.bytes, direct.size) == 0);
    ok = 1;
end:
    free(direct.bytes);
    free(queued.bytes);
    return !ok;
}
#endif

int
test_dither_lso2_gpu_0086(int argc, char **argv)
{
#if SIXEL_ENABLE_THREADS && SIXEL_ENCODER_PREPARE_AHEAD_SUPPORTED \
    && SIXEL_ATOMIC_U32_LOCK_FREE
    int device;

    (void)argc;
    (void)argv;
    device = lso2_gpu_rendezvous_device();
    if (device != 0) return device;
    return auto_lookup_transition_matches("lso2") ||
        auto_lookup_transition_matches("fs");
#else
    (void)argc;
    (void)argv;
    return 77;
#endif
}
