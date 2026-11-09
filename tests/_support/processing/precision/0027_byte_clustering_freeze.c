/* SPDX-License-Identifier: MIT */
/* A byte application request must not retain the float palette that
 * non-gamma clustering builds: the byte component route rejects an
 * authoritative float palette, and the capture stage of an independent
 * playback queue has no legacy byte adapter to fall back to. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/dither.h"
#include "src/palette.h"

#define FREEZE_COLORS 4
#define FREEZE_WIDTH  64
#define FREEZE_HEIGHT 48
#define FREEZE_FRAMES 4

/*
 * Each coordinate sits three tenths of a byte step above or below its
 * byte, so the frozen palette witnesses nearest-byte rounding: truncating
 * the same floats would move the six entries that sit below. Every value
 * stays inside the unit range, where no clamp can hide the arithmetic.
 */
static float const freeze_floats[FREEZE_COLORS * 3] = {
    0.0678431392f, 0.249803916f, 0.507058799f,
    0.783137262f, 0.130588233f, 0.375294119f,
    0.628627479f, 0.877254903f, 0.0286274515f,
    0.312549025f, 0.565882325f, 0.979215682f
};

static unsigned char const freeze_bytes[FREEZE_COLORS * 3] = {
    17, 64, 129,
    200, 33, 96,
    160, 224, 7,
    80, 144, 250
};

typedef struct freeze_playback {
    unsigned int completed;
    unsigned int failed;
    size_t written;
    SIXELSTATUS failure;
    char const *stage;
} freeze_playback_t;

static void
freeze_record(freeze_playback_t *state, char const *stage,
              SIXELSTATUS status)
{
    if (state->stage != NULL) {
        return;
    }
    state->stage = stage;
    state->failure = status;
}

static int
freeze_write(char *data, int size, void *context)
{
    freeze_playback_t *state;

    (void)data;
    state = context;
    if (size > 0) {
        state->written += (size_t)size;
    }
    return size;
}

static void
freeze_done(sixel_playback_result_t const *result, void *context)
{
    freeze_playback_t *state;

    state = context;
    ++state->completed;
    if (SIXEL_FAILED(result->status)) {
        ++state->failed;
        freeze_record(state, "completion", result->status);
    }
}

/* Install the byte view first: an explicit byte assignment is a precision
 * boundary that drops any float view, so the float palette follows it. */
static int
freeze_install_palette(sixel_dither_t *dither)
{
    sixel_palette_float32_entries_request_t request;

    sixel_dither_set_palette(dither, (unsigned char *)freeze_bytes);
    memset(&request, 0, sizeof(request));
    request.entries = freeze_floats;
    request.colors = FREEZE_COLORS;
    request.depth = (int)(3U * sizeof(float));
    return SIXEL_SUCCEEDED(dither->palette->vtbl->init_entries_float32(
        dither->palette, &request));
}

static int
freeze_has_float(sixel_dither_t *dither)
{
    sixel_palette_float32_entries_view_t view;

    memset(&view, 0, sizeof(view));
    if (SIXEL_FAILED(dither->palette->vtbl->get_entries_float32(
            dither->palette, &view))) {
        return (-1);
    }
    return view.entries != NULL;
}

/* The float view stays authoritative until a byte request freezes it, and
 * the component preparation declines for exactly as long. */
static int
freeze_palette_case(void)
{
    sixel_dither_t *dither;
    sixel_palette_entries_view_t bytes;
    int ok;
    int i;

    dither = NULL;
    ok = 0;
    if (SIXEL_FAILED(sixel_dither_new(&dither, FREEZE_COLORS, NULL))) {
        return 0;
    }
    if (!freeze_install_palette(dither) || freeze_has_float(dither) != 1) {
        goto end;
    }
    /* An automatic or float request keeps the constructed coordinates. */
    if (SIXEL_FAILED(sixel_dither_freeze_application_precision(dither)) ||
            freeze_has_float(dither) != 1 ||
            sixel_dither_prepare_indexed_components(dither, 0, 0) !=
                SIXEL_FALSE) {
        goto end;
    }
    /* Native output consumes those coordinates: it reports its own
     * conflict with a byte request instead of losing them here. */
    dither->apply_8bit = 1;
    dither->use_rgb101_palette = 1;
    if (SIXEL_FAILED(sixel_dither_freeze_application_precision(dither)) ||
            freeze_has_float(dither) != 1) {
        goto end;
    }
    dither->use_rgb101_palette = 0;
    if (SIXEL_FAILED(sixel_dither_freeze_application_precision(dither)) ||
            freeze_has_float(dither) != 0) {
        goto end;
    }
    memset(&bytes, 0, sizeof(bytes));
    if (SIXEL_FAILED(dither->palette->vtbl->get_entries(dither->palette,
            &bytes)) || bytes.entries == NULL || bytes.depth != 3 ||
            bytes.entry_count != FREEZE_COLORS) {
        goto end;
    }
    for (i = 0; i < FREEZE_COLORS * 3; ++i) {
        if (bytes.entries[i] != freeze_bytes[i]) {
            fprintf(stderr, "entry %d: byte %u, expected %u\n", i,
                (unsigned int)bytes.entries[i],
                (unsigned int)freeze_bytes[i]);
            goto end;
        }
    }
    /* The byte component route now accepts the frozen palette. */
    if (sixel_dither_prepare_indexed_components(dither, 0, 0) != SIXEL_OK) {
        goto end;
    }
    ok = 1;
end:
    if (!ok) {
        fprintf(stderr, "palette freeze: %s\n",
            sixel_helper_get_additional_message());
    }
    sixel_dither_unref(dither);
    return ok;
}

static unsigned char *
freeze_frame_pixels(int seed)
{
    unsigned char *pixels;
    unsigned char *pixel;
    int x;
    int y;

    pixels = malloc((size_t)FREEZE_WIDTH * FREEZE_HEIGHT * 3U);
    if (pixels == NULL) {
        return NULL;
    }
    for (y = 0; y < FREEZE_HEIGHT; ++y) {
        for (x = 0; x < FREEZE_WIDTH; ++x) {
            pixel = pixels + ((size_t)y * FREEZE_WIDTH + (size_t)x) * 3U;
            pixel[0] = (unsigned char)((x * 3 + seed * 7) & 0xff);
            pixel[1] = (unsigned char)((y * 5 + seed * 11) & 0xff);
            pixel[2] = (unsigned char)(((x ^ y) * 2 + seed * 3) & 0xff);
        }
    }
    return pixels;
}

/*
 * An independent playback queue resolves every frame through the component
 * route alone. A retained float palette would fail each capture with no
 * message, so this witnesses the construction-time freeze end to end.
 */
static int
freeze_playback_case(void)
{
    static struct { int flag; char const *value; } const options[] = {
        { SIXEL_OPTFLAG_PRECISION_POLICY, "8bit" },
        { SIXEL_OPTFLAG_WORKING_COLORSPACE, "gamma" },
        { SIXEL_OPTFLAG_CLUSTERING_COLORSPACE, "oklab" },
        { SIXEL_OPTFLAG_DIFFUSION, "none" },
        { SIXEL_OPTFLAG_LUT_POLICY, "5bit" },
        { SIXEL_OPTFLAG_UPDATE_POLICY, "delta" },
        { SIXEL_OPTFLAG_THREADING_POLICY, "balanced:prepare=ahead" },
        { SIXEL_OPTFLAG_GPU_POLICY, "auto" },
        { SIXEL_OPTFLAG_COLORS, "32" }
    };
    freeze_playback_t state;
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_playback_t *playback;
    sixel_playback_config_t config;
    sixel_frame_t *frame;
    unsigned char *pixels;
    SIXELSTATUS status;
    int ok;
    int i;

    encoder = NULL;
    output = NULL;
    playback = NULL;
    frame = NULL;
    ok = 0;
    memset(&state, 0, sizeof(state));
    state.failure = SIXEL_OK;
    state.stage = NULL;
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, freeze_write, &state,
                NULL))) {
        goto end;
    }
    for (i = 0; i < (int)(sizeof(options) / sizeof(options[0])); ++i) {
        status = sixel_encoder_setopt(encoder, options[i].flag,
            options[i].value);
        if (SIXEL_FAILED(status)) {
            freeze_record(&state, "setopt", status);
            goto end;
        }
    }
    if (SIXEL_FAILED(sixel_playback_config_init(&config, sizeof(config)))) {
        goto end;
    }
    config.capacity = 2;
    config.done = freeze_done;
    config.done_context = &state;
    status = sixel_playback_new(&playback, encoder, output, &config, NULL);
    if (SIXEL_FAILED(status)) {
        freeze_record(&state, "playback_new", status);
        goto end;
    }
    /* The frame owns its pixels, so every admission outlives this loop. */
    for (i = 0; i < FREEZE_FRAMES; ++i) {
        pixels = freeze_frame_pixels(i);
        if (pixels == NULL) {
            goto end;
        }
        if (SIXEL_FAILED(sixel_frame_new(&frame, NULL))) {
            free(pixels);
            goto end;
        }
        status = sixel_frame_init(frame, pixels, FREEZE_WIDTH,
            FREEZE_HEIGHT, SIXEL_PIXELFORMAT_RGB888, NULL, (-1));
        if (SIXEL_SUCCEEDED(status)) {
            status = sixel_playback_submit(playback, frame,
                SIXEL_PLAYBACK_WAIT, (-1), NULL);
        }
        if (SIXEL_FAILED(status)) {
            freeze_record(&state, "submit", status);
            goto end;
        }
        sixel_frame_unref(frame);
        frame = NULL;
    }
    status = sixel_playback_drain(playback, (-1));
    if (SIXEL_FAILED(status)) {
        freeze_record(&state, "drain", status);
        goto end;
    }
    if (state.completed != FREEZE_FRAMES || state.failed != 0 ||
            state.written == 0U) {
        goto end;
    }
    ok = 1;
end:
    if (!ok) {
        fprintf(stderr, "component playback: stage=%s status=%08x "
            "completed=%u failed=%u written=%lu: %s\n",
            state.stage != NULL ? state.stage : "assertion",
            (unsigned int)state.failure, state.completed, state.failed,
            (unsigned long)state.written,
            sixel_helper_get_additional_message());
    }
    sixel_frame_unref(frame);
    sixel_playback_unref(playback);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok;
}

int test_precision_0027(int argc, char **argv);

int
test_precision_0027(int argc, char **argv)
{
    int scenario;

    scenario = argc == 2 ? atoi(argv[1]) : (-1);
    if (scenario == 0) {
        return freeze_palette_case() ? 0 : 1;
    }
    if (scenario == 1) {
        return freeze_playback_case() ? 0 : 1;
    }
    return 1;
}
