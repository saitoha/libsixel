/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/encoder.h"
#include "playback_test_common.h"

typedef struct fallback_state {
    int callbacks;
    int failed;
} fallback_state_t;

static void
fallback_done(sixel_playback_result_t const *result, void *context)
{
    fallback_state_t *state;

    state = context;
    ++state->callbacks;
    state->failed |= result->status != SIXEL_OK ||
        result->sequence != (uint64_t)state->callbacks ||
        result->palette_sequence != result->sequence;
}

static void
fallback_selected(void *context, uint64_t sequence,
    sixel_palette_selection_t const *selection)
{
    fallback_state_t *state;

    state = context;
    (void)sequence;
    (void)selection;
    state->failed = 1;
}

/* These valid feature requests must execute successfully with the same
 * bytes as frame mode; a successful latest selection is a wrong route. */
static int
fallback_compare(int feature)
{
    static int const flags[] = {
        SIXEL_OPTFLAG_GPU_POLICY, SIXEL_OPTFLAG_BUILTIN_PALETTE,
        SIXEL_OPTFLAG_HIGH_COLOR
    };
    static char const *const values[] = { "auto", "xterm256", NULL };
    playback_test_buffer_t buffers[2];
    fallback_state_t state;
    struct sixel_encoder_palette_test_hooks hooks;
    sixel_playback_config_t config;
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_playback_t *playback;
    sixel_frame_t *frame;
    SIXELSTATUS status;
    int pass, index, failed;

    memset(buffers, 0, sizeof(buffers));
    memset(&hooks, 0, sizeof(hooks));
    encoder = NULL;
    output = NULL;
    playback = NULL;
    failed = 1;
    for (pass = 0; pass < 2; ++pass) {
        memset(&state, 0, sizeof(state));
        if (sixel_encoder_new(&encoder, NULL) ||
                sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_THREADING_POLICY,
                    "greedy:B2") ||
                sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_LUT_POLICY,
                    "none") ||
                sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_QUANTIZE_MODEL,
                    "heckbert") ||
                sixel_encoder_setopt(encoder, flags[feature],
                    values[feature]) ||
                sixel_output_new(&output, playback_test_write,
                    &buffers[pass], NULL)) goto end;
        hooks.selected = fallback_selected;
        hooks.context = &state;
        encoder->palette_test_hooks = &hooks;
        (void)sixel_playback_config_init(&config, sizeof(config));
        config.palette_mode = pass ? SIXEL_PLAYBACK_PALETTE_LATEST
                                  : SIXEL_PLAYBACK_PALETTE_PER_FRAME;
        config.done = fallback_done;
        config.done_context = &state;
        if (sixel_playback_new(&playback, encoder, output, &config, NULL)) {
            goto end;
        }
        for (index = 0; index < 2; ++index) {
            frame = playback_test_frame(index);
            if (frame == NULL) goto end;
            status = sixel_frame_clip(frame, 0, 0, 16, 12);
            if (status != SIXEL_OK) {
                sixel_frame_unref(frame);
                goto end;
            }
            sixel_frame_set_frame_no(frame, index);
            status = sixel_playback_submit(playback, frame,
                SIXEL_PLAYBACK_WAIT, 10000, NULL);
            sixel_frame_unref(frame);
            if (status != SIXEL_OK) goto end;
        }
        if (sixel_playback_drain(playback, 10000) || state.failed ||
                state.callbacks != 2) goto end;
        sixel_playback_unref(playback);
        playback = NULL;
        sixel_output_unref(output);
        output = NULL;
        sixel_encoder_unref(encoder);
        encoder = NULL;
    }
    failed = !buffers[0].size || buffers[0].size != buffers[1].size ||
        memcmp(buffers[0].bytes, buffers[1].bytes, buffers[0].size);
end:
    if (failed) fprintf(stderr, "feature fallback %d differs: %s\n",
        feature, sixel_helper_get_additional_message());
    sixel_playback_unref(playback);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    free(buffers[0].bytes);
    free(buffers[1].bytes);
    return failed;
}

int
test_palette_features_0070(int argc, char **argv)
{
    int feature;

    (void)argc;
    (void)argv;
    for (feature = 0; feature < 3; ++feature) {
        if (fallback_compare(feature)) return 1;
    }
    return 0;
}
