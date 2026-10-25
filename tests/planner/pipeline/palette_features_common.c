/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/encoder.h"
#include "src/dither.h"
#include "src/palette.h"
#include "playback_test_common.h"
#include "palette_features_common.h"

#if SIXEL_ENABLE_THREADS
enum { FEATURE_WIDTH = 16, FEATURE_HEIGHT = 12, FEATURE_PIXELS = 192 };
typedef struct feature_state feature_state_t;
typedef struct feature_gate {
    feature_state_t *state;
    int sequence;
    int open;
    int published;
} feature_gate_t;

struct feature_state {
    sixel_mutex_t mutex;
    sixel_cond_t cond;
    feature_gate_t gates[4];
    sixel_encoder_t *encoder;
    playback_test_buffer_t buffer;
    playback_test_buffer_t reference;
    size_t offset;
    unsigned char history[FEATURE_PIXELS * 2 * 3];
    unsigned char valid[FEATURE_PIXELS * 2];
    unsigned char captured[768];
    int captured_colors;
    uint64_t chosen[4];
    uint64_t attempts[4];
    int builds[4];
    int joined[4];
    int completed;
    int failed;
    int scenario;
    int floating;
    int error_skip;
    int painted;
    int kept;
};

static sixel_frame_t *feature_frame(feature_state_t *state, int sequence);

static void
feature_wait(feature_state_t *state, int *ready)
{
    while (!*ready) {
        if (sixel_cond_timedwait(&state->cond, &state->mutex, 5000) != 0) {
            state->failed = 1;
            break;
        }
    }
}

static void
feature_before_publish(void *context)
{
    feature_gate_t *gate;

    gate = context;
    sixel_mutex_lock(&gate->state->mutex);
    feature_wait(gate->state, &gate->open);
    sixel_mutex_unlock(&gate->state->mutex);
}

static void
feature_after_publish(void *context)
{
    feature_gate_t *gate;

    gate = context;
    sixel_mutex_lock(&gate->state->mutex);
    gate->published = 1;
    sixel_cond_broadcast(&gate->state->cond);
    sixel_mutex_unlock(&gate->state->mutex);
}

static void
feature_before_join(void *context)
{
    feature_gate_t *gate;
    feature_state_t *state;

    gate = context;
    state = gate->state;
    sixel_mutex_lock(&state->mutex);
    ++state->joined[gate->sequence];
    /* Frame 2 can publish only when its own fallback actually joins. */
    gate->open = 1;
    sixel_cond_broadcast(&state->cond);
    sixel_mutex_unlock(&state->mutex);
}

static void
feature_build(void *context, uint64_t sequence,
    sixel_palette_job_test_hooks_t *hooks)
{
    feature_state_t *state;

    state = context;
    hooks->context = &state->gates[sequence];
    hooks->before_publish = feature_before_publish;
    hooks->after_publish = feature_after_publish;
    hooks->before_join = feature_before_join;
    sixel_mutex_lock(&state->mutex);
    ++state->builds[sequence];
    sixel_mutex_unlock(&state->mutex);
}

static void
feature_before_select(void *context, uint64_t sequence,
    sixel_palette_store_job_t *own)
{
    feature_state_t *state;

    state = context;
    (void)own;
    if (state->scenario == 5 && sequence == 1) {
        /* Finish speculative frame 2 with its predicted reserved key, then
         * invalidate history after frame 1 emits to force pristine redo. */
        sixel_mutex_lock(&state->mutex);
        state->gates[2].open = 1;
        sixel_cond_broadcast(&state->cond);
        feature_wait(state, &state->gates[2].published);
        sixel_mutex_unlock(&state->mutex);
    }
    if (state->scenario == 6 && sequence == 2) {
        /* Publish future 3 before the byte-to-float transition rebases the
         * epoch. It must be retired and rebuilt, never selected afterward. */
        sixel_mutex_lock(&state->mutex);
        state->gates[3].open = 1;
        sixel_cond_broadcast(&state->cond);
        feature_wait(state, &state->gates[3].published);
        sixel_mutex_unlock(&state->mutex);
    }
}

static void
feature_selected(void *context, uint64_t sequence,
    sixel_palette_selection_t const *selection)
{
    feature_state_t *state;
    sixel_palette_snapshot_profile_t const *profile;
    sixel_dither_t *copy;
    sixel_frame_t *frame;
    sixel_output_t *output;
    SIXELSTATUS status;
    sixel_palette_entries_view_t view;

    state = context;
    state->chosen[sequence] = selection->sequence;
    state->attempts[sequence] = selection->attempt;
    profile = sixel_palette_snapshot_profile(selection->snapshot);
    state->failed |= !selection->published ||
        selection->published >= selection->selected ||
        selection->sequence > sequence;
    if (sequence == 2 && state->scenario != 7 && state->scenario != 5) {
        state->failed |= selection->sequence != 2 || !state->joined[2];
    }
    if (state->scenario != 7) {
        state->failed |= profile->keycolor_reserved !=
            (sequence > 1 && !(state->scenario == 5 && sequence == 2));
    }
    if (sequence == 3 && state->scenario != 5 && state->scenario != 6) {
        state->failed |= selection->sequence != 2;
    }
    if (state->scenario == 3 || state->scenario == 8) {
        copy = NULL;
        if (sixel_palette_snapshot_clone(selection->snapshot, &copy) ||
                copy->palette->vtbl->get_entries(copy->palette, &view)) {
            state->failed = 1;
        } else {
            state->failed |= copy->use_rgb101_palette !=
                (state->scenario == 3);
            state->captured_colors = (int)view.entry_count;
            memcpy(state->captured, view.entries, view.entry_count * 3U);
        }
        sixel_dither_unref(copy);
    }
    if ((state->scenario == 1 || state->scenario == 2) && sequence == 3) {
        /* Independently bind the selected palette to the current target,
         * retained wire colors, zero submission limit and requested error
         * mode. Compare application bytes, not fields cleared by encode. */
        copy = NULL;
        output = NULL;
        frame = feature_frame(state, 3);
        status = sixel_palette_snapshot_clone(selection->snapshot, &copy);
        if (status == SIXEL_OK) {
            status = sixel_output_new(&output, playback_test_write,
                &state->reference, NULL);
        }
        if (status == SIXEL_OK && frame != NULL) {
            sixel_output_set_palette_type(output, state->scenario == 2
                ? SIXEL_PALETTETYPE_HLS : SIXEL_PALETTETYPE_RGB);
            sixel_dither_set_diffusion_type(copy, SIXEL_DIFFUSE_FS);
            sixel_dither_set_optimize_palette(copy, 1);
            sixel_dither_set_pipeline_accumulation_buffer_hint(copy,
                state->history, FEATURE_PIXELS * 3, state->valid,
                FEATURE_PIXELS, FEATURE_WIDTH, FEATURE_HEIGHT, 0, 0,
                FEATURE_WIDTH, FEATURE_HEIGHT, copy->keycolor, 1, 0,
                state->encoder->sixdelta_error_mode);
            status = sixel_encode(sixel_frame_get_pixels(frame),
                FEATURE_WIDTH, FEATURE_HEIGHT,
                sixel_helper_compute_depth(sixel_frame_get_pixelformat(frame)),
                copy, output);
        }
        state->failed |= status != SIXEL_OK || frame == NULL;
        sixel_output_unref(output);
        sixel_dither_unref(copy);
        sixel_frame_unref(frame);
    }
}

static void
feature_emitted(void *context, uint64_t sequence,
    sixel_encoding_planner_t const *planner, sixel_dither_t const *dither)
{
    feature_state_t *state;
    sixel_encoder_t *encoder;
    unsigned char *decoded;
    size_t length;
    size_t plane_count;
    int width, height, i, c, pos, origin, paint, valid;
    unsigned char expected;

    state = context;
    encoder = state->encoder;
    (void)planner;
    if (sequence == 3 && state->scenario != 5 && state->scenario != 6) {
        sixel_mutex_lock(&state->mutex);
        state->failed |= state->gates[3].published;
        sixel_mutex_unlock(&state->mutex);
    }
    if (state->scenario == 7) {
        /* The selected palette's two-color histogram says nothing about
         * the current gradient: it must retain the requested FS diffusion. */
        state->failed |= sequence > 1 &&
            dither->method_for_diffuse != SIXEL_DIFFUSE_FS;
    } else {
        decoded = NULL;
        length = state->buffer.size - state->offset;
        if (sixel_decode_direct(state->buffer.bytes + state->offset,
                (int)length, &decoded, &width, &height, NULL) ||
                width != FEATURE_WIDTH || height != FEATURE_HEIGHT) {
            state->failed = 1;
            free(decoded);
            return;
        }
        origin = state->scenario == 4 && sequence > 1 ? FEATURE_WIDTH : 0;
        plane_count = state->scenario == 4 ? FEATURE_PIXELS * 2
                                          : FEATURE_PIXELS;
        state->failed |= !encoder->accumulation_valid;
        for (i = 0; i < FEATURE_PIXELS; ++i) {
            pos = (i / FEATURE_WIDTH) *
                (state->scenario == 4 ? FEATURE_WIDTH * 2 : FEATURE_WIDTH) +
                i % FEATURE_WIDTH + origin;
            paint = decoded[i * 4 + 3] != 0;
            valid = paint || state->valid[pos];
            state->painted += paint;
            state->kept += !paint;
            state->failed |= encoder->accumulation_valid_mask[pos] != valid;
            for (c = 0; c < 3; ++c) {
                expected = paint ? decoded[i * 4 + c]
                                 : state->history[pos * 3 + c];
                if (valid && encoder->accumulation_pixels[pos * 3 + c]
                        != expected) {
                    fprintf(stderr, "wire/history: n=%llu pixel=%d c=%d "
                        "actual=%u expected=%u paint=%d\n",
                        (unsigned long long)sequence, i, c,
                        encoder->accumulation_pixels[pos * 3 + c],
                        expected, paint);
                    state->failed = 1;
                }
            }
            if (state->scenario == 4 && sequence > 1 && i == (int)sequence) {
                state->failed |= paint;
            }
        }
        memcpy(state->history, encoder->accumulation_pixels, plane_count * 3);
        memcpy(state->valid, encoder->accumulation_valid_mask, plane_count);
        free(decoded);
        if (sequence == 3 && (state->scenario == 1 || state->scenario == 2)) {
            state->failed |= length != state->reference.size ||
                memcmp(state->buffer.bytes + state->offset,
                    state->reference.bytes, state->reference.size);
        }
    }
    if (state->scenario == 3 || state->scenario == 8) {
        state->failed |= !encoder->capture_valid ||
            encoder->capture_ncolors != state->captured_colors ||
            memcmp(encoder->capture_palette, state->captured,
                (size_t)state->captured_colors * 3);
    }
    state->offset = state->buffer.size;
    if (state->scenario == 5 && sequence == 1) {
        /* Internal test hook owns the operation span; no public setter can
         * enter the attached encoder here. The prepared request is frozen. */
        encoder->accumulation_valid = 0;
        memset(state->valid, 0, sizeof(state->valid));
    }
    sixel_mutex_lock(&state->mutex);
    if (sequence >= 2) {
        state->gates[sequence].open = 1;
    }
    sixel_cond_broadcast(&state->cond);
    sixel_mutex_unlock(&state->mutex);
}

static void
feature_done(sixel_playback_result_t const *result, void *context)
{
    feature_state_t *state;

    state = context;
    ++state->completed;
    state->failed |= result->sequence != (uint64_t)state->completed ||
        result->status != SIXEL_OK ||
        result->palette_sequence != state->chosen[result->sequence];
    /* Finish joins the late own result; export/history must still describe
     * the palette that was actually applied, not that completed builder. */
    if (state->scenario == 3 || state->scenario == 8) {
        state->failed |= memcmp(state->encoder->capture_palette,
            state->captured, (size_t)state->captured_colors * 3);
    }
}

static sixel_frame_t *
feature_frame(feature_state_t *state, int sequence)
{
    sixel_frame_t *frame;
    sixel_frame_interface_t *iface;
    sixel_frame_transparency_t transparency;
    unsigned char *pixels;
    unsigned char *mask;
    float *floats;
    int floating, i, c, value;

    frame = NULL;
    floating = state->floating || (state->scenario == 6 && sequence > 1);
    pixels = malloc(FEATURE_PIXELS * 3U * (floating ? sizeof(float) : 1U));
    if (pixels == NULL) return NULL;
    floats = (float *)pixels;
    for (i = 0; i < FEATURE_PIXELS; ++i) {
        for (c = 0; c < 3; ++c) {
            /* Black is a real paintable color, distinct from the reserved
             * zero-filled key. Frame 3 changes targets with an old palette. */
            value = i % 4 == 0 ? 0 : (i % 4) * (c + 1) * 23;
            if (sequence == 3 && i % 5 == 1) value = 127 + c * 11;
            if (state->scenario == 7) {
                value = sequence == 1 ? (i % 2) * 255
                                      : (i * 13 + c * 19) % 256;
            }
            if (floating) floats[i * 3 + c] = value / 255.0f;
            else pixels[i * 3 + c] = (unsigned char)value;
        }
    }
    if (sixel_frame_new(&frame, NULL) ||
            sixel_frame_init(frame, pixels, FEATURE_WIDTH, FEATURE_HEIGHT,
                floating ? SIXEL_PIXELFORMAT_RGBFLOAT32
                         : SIXEL_PIXELFORMAT_RGB888, NULL, -1)) {
        if (frame == NULL) free(pixels);
        sixel_frame_unref(frame);
        return NULL;
    }
    if (state->scenario == 4 && sequence > 1) {
        mask = calloc(FEATURE_PIXELS, 1);
        if (mask == NULL) {
            sixel_frame_unref(frame);
            return NULL;
        }
        mask[sequence] = 1;
        memset(&transparency, 0, sizeof(transparency));
        transparency.transparent = -1;
        transparency.alpha_zero_is_transparent = 1;
        transparency.transparent_mask = mask;
        transparency.transparent_mask_size = FEATURE_PIXELS;
        iface = (sixel_frame_interface_t *)frame;
        if (iface->vtbl->set_transparency(iface, &transparency)) {
            sixel_frame_unref(frame);
            return NULL;
        }
    }
    return frame;
}

static int
feature_run(int scenario, int floating, int error_skip)
{
    feature_state_t state;
    struct sixel_encoder_palette_test_hooks hooks;
    sixel_playback_config_t config;
    sixel_playback_options_t options;
    sixel_playback_t *playback;
    sixel_output_t *output;
    sixel_frame_t *frame;
    SIXELSTATUS status;
    int i, failed;

    memset(&state, 0, sizeof(state));
    memset(&hooks, 0, sizeof(hooks));
    state.scenario = scenario;
    state.floating = floating;
    state.error_skip = error_skip;
    playback = NULL;
    output = NULL;
    failed = 1;
    status = SIXEL_OK;
    if (sixel_mutex_init(&state.mutex)) return 1;
    if (sixel_cond_init(&state.cond)) {
        sixel_mutex_destroy(&state.mutex);
        return 1;
    }
    state.encoder = playback_test_encoder(scenario != 7, "4");
    if (state.encoder == NULL ||
            sixel_encoder_setopt(state.encoder, SIXEL_OPTFLAG_THREADING_POLICY,
                scenario == 5 || scenario == 6
                    ? "greedy:B4:palette=latest:prepare=ahead"
                    : "greedy:B2:palette=latest:prepare=serial") ||
            sixel_encoder_setopt(state.encoder, SIXEL_OPTFLAG_LUT_POLICY,
                "none") ||
            sixel_output_new(&output, playback_test_write, &state.buffer,
                NULL)) goto end;
    state.encoder->reqcolors = 4;
    state.encoder->palette_sampling_policy = scenario == 0
        ? SIXEL_PALETTE_SAMPLING_ADAPTIVE_GRID
        : SIXEL_PALETTE_SAMPLING_FULL_FRAME;
    state.encoder->sixdelta_threshold = 12;
    state.encoder->sixdelta_error_mode = error_skip ? SIXEL_6DELTA_ERROR_SKIP
                                                  : SIXEL_6DELTA_ERROR_DIFFUSE;
    if (scenario == 2) {
        state.encoder->palette_type = SIXEL_PALETTETYPE_HLS;
        sixel_output_set_palette_type(output, SIXEL_PALETTETYPE_HLS);
    }
    if (scenario == 3 && sixel_encoder_setopt(state.encoder,
            SIXEL_OPTFLAG_OUTPUT_COLORSPACE, "gamma")) goto end;
    if (scenario == 3 || scenario == 8) {
        if (sixel_encoder_setopt(state.encoder, SIXEL_OPTFLAG_MAPFILE_OUTPUT,
                "pal:-")) goto end;
    }
    if (scenario == 4 && sixel_encoder_set_6delta_plane_size(state.encoder,
            FEATURE_WIDTH * 2, FEATURE_HEIGHT)) goto end;
    hooks.context = &state;
    hooks.build = feature_build;
    hooks.before_select = feature_before_select;
    hooks.selected = feature_selected;
    hooks.emitted = feature_emitted;
    state.encoder->palette_test_hooks = &hooks;
    for (i = 1; i <= 3; ++i) {
        state.gates[i].state = &state;
        state.gates[i].sequence = i;
        state.gates[i].open = i == 1;
    }
    (void)sixel_playback_config_init(&config, sizeof(config));
    config.palette_mode = SIXEL_PLAYBACK_PALETTE_LATEST;
    config.capacity = 3;
    config.done = feature_done;
    config.done_context = &state;
    if (sixel_playback_new(&playback, state.encoder, output, &config, NULL)) {
        goto end;
    }
    for (i = 1; i <= 3; ++i) {
        frame = feature_frame(&state, i);
        if (frame == NULL) goto end;
        (void)sixel_playback_options_init(&options, sizeof(options));
        options.flags = SIXEL_PLAYBACK_OPTION_THRESHOLD_LIMIT;
        options.threshold_limit = i == 3 ? 0 : 12;
        if (scenario == 4) {
            options.flags |= SIXEL_PLAYBACK_OPTION_ORIGIN;
            options.origin_x = i > 1 ? FEATURE_WIDTH : 0;
        }
        status = sixel_playback_submit(playback, frame, SIXEL_PLAYBACK_WAIT,
            10000, &options);
        sixel_frame_unref(frame);
        if (status != SIXEL_OK) goto end;
    }
    status = sixel_playback_drain(playback, 10000);
    if (status != SIXEL_OK || state.failed || state.completed != 3 ||
            (scenario != 7 && (!state.painted || !state.kept))) goto end;
    if (scenario == 5 && (state.builds[2] != 2 || state.chosen[2] != 2)) {
        goto end;
    }
    if (scenario == 6 && (state.chosen[2] != 2 || state.chosen[3] != 3 ||
            state.builds[3] != 2)) goto end;
    failed = 0;
end:
    if (failed) {
        fprintf(stderr, "feature=%d float=%d skip=%d status=%x failed=%d "
            "done=%d chosen=%llu/%llu/%llu builds2=%d paints=%d keeps=%d %s\n",
            scenario, floating, error_skip, status, state.failed,
            state.completed, (unsigned long long)state.chosen[1],
            (unsigned long long)state.chosen[2],
            (unsigned long long)state.chosen[3], state.builds[2],
            state.painted, state.kept, sixel_helper_get_additional_message());
    }
    sixel_mutex_lock(&state.mutex);
    for (i = 1; i <= 3; ++i) state.gates[i].open = 1;
    sixel_cond_broadcast(&state.cond);
    sixel_mutex_unlock(&state.mutex);
    sixel_playback_unref(playback);
    sixel_output_unref(output);
    sixel_encoder_unref(state.encoder);
    free(state.buffer.bytes);
    free(state.reference.bytes);
    sixel_cond_destroy(&state.cond);
    sixel_mutex_destroy(&state.mutex);
    return failed;
}
#endif

int
palette_features_test(int scenario)
{
#if SIXEL_ENABLE_THREADS
    return feature_run(scenario, scenario == 3, 0) ||
        ((scenario == 1 || scenario == 2) && feature_run(scenario, 1, 1));
#else
    (void)scenario;
    return 0;
#endif
}
