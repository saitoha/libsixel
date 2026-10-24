/* SPDX-License-Identifier: MIT */
/* Independent destination grid values, ownership and encoder integration. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/cms.h"
#include "src/encoder.h"
#include "src/dither.h"
#include "src/palette.h"
#include "src/palette-snap-grid.h"
#include "src/palette-common-snap.h"
#include "tests/data/color-numeric/destination-snap.h"
#include "tests/data/color-numeric/paired-lut.h"

static int const destination_spaces[] = {
    SIXEL_COLORSPACE_GAMMA, SIXEL_COLORSPACE_LINEAR, SIXEL_COLORSPACE_OKLAB,
    SIXEL_COLORSPACE_CIELAB, SIXEL_COLORSPACE_DIN99D
};

static sixel_color_def_t *
destination_definition(int kind)
{
    static double const xy[8] = {
        .3127, .3290, .68, .32, .265, .69, .15, .06
    };
    static double const gamma[3] = {1.8, 2.2, 2.6};

    if (kind == 2) {
        return sixel_cdef_rgb(xy, SIXEL_COLOR_TRC_POWER, gamma);
    }
    return sixel_cdef_name(kind ? "dci-p3" : "adobe-rgb");
}

static int
destination_close(double const actual[3], double const expected[3])
{
    int channel;

    /* Independent chromaticities, D50 adaptation and opponent equations.
     * The 2e-5 typed-unit budget covers transforms and float stores, while
     * fixtures exclude near-tie candidates and wire half-bin boundaries. */
    for (channel = 0; channel < 3; ++channel) {
        if (!isfinite(actual[channel]) ||
                fabs(actual[channel] - expected[channel]) > 2e-5) {
            fprintf(stderr, "channel %d: %.17g expected %.17g\n",
                channel, actual[channel], expected[channel]);
            return 0;
        }
    }
    return 1;
}

static int
destination_grid(int kind, int strategy, int space)
{
    sixel_allocator_t *allocator;
    sixel_snap_grid_t *grid;
    sixel_color_def_t *definition;
    double actual[3];
    int fixture;
    int policy;
    int repeat;
    int ok;

    allocator = NULL;
    grid = NULL;
    definition = destination_definition(kind);
    ok = 0;
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL)) ||
            SIXEL_FAILED(sixel_snap_grid_new(&grid, strategy,
                destination_spaces[space], definition, allocator))) {
        goto end;
    }
    /* The caller can release its definition before any target is used. */
    sixel_cdef_free(definition);
    definition = NULL;
    for (repeat = 0; repeat < 2; ++repeat) {
        for (fixture = 0; fixture < 3; ++fixture) {
            for (policy = 0; policy < 2; ++policy) {
                if (SIXEL_FAILED(sixel_snap_grid_target(grid,
                        destination_snap_codes[kind][fixture][space][policy],
                        actual)) || !destination_close(actual,
                            destination_snap_values[kind][fixture][space]
                                [policy + 1]) ||
                        SIXEL_FAILED(sixel_snap_grid_project(grid,
                            destination_snap_values[kind][fixture][space][0],
                            policy == 0, .85, actual)) ||
                        !destination_close(actual,
                            destination_snap_values[kind][fixture][space]
                                [policy + 1])) {
                    fprintf(stderr, "grid %d/%d/%d fixture %d policy %d\n",
                        kind, strategy, space, fixture, policy);
                    goto end;
                }
            }
        }
    }
    ok = 1;
end:
    sixel_cdef_free(definition);
    sixel_snap_grid_destroy(grid);
    sixel_allocator_unref(allocator);
    return ok;
}

static int
destination_sink(char *data, int size, void *context)
{
    size_t *count;

    (void)data;
    count = context;
    *count += (size_t)size;
    return size;
}

static int
destination_pipeline(int kind, int model, int space, int policy, int mode)
{
    static int const formats[] = {SIXEL_PIXELFORMAT_RGBFLOAT32,
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, SIXEL_PIXELFORMAT_OKLABFLOAT32,
        SIXEL_PIXELFORMAT_CIELABFLOAT32, SIXEL_PIXELFORMAT_DIN99DFLOAT32};
    static char const *const names[] = {
        "gamma", "linear", "oklab", "cielab", "din99d"};
    static char const *const models[] = {
        "kmeans:seed=1", "center:seed=1", "medoids:seed=1", "heckbert"};
    static struct { int flag; char const *value; } const options[] = {
        { '=', "greedy:B1" }, { 'p', "1" }, { 'F', "none" }, { 'a', "off" },
        { '~', "none" }, { 'd', "none" }, { 't', "rgb" },
        { '4', "full-frame" }, { '5', "none" },
        { '.', "auto" }
    };
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    sixel_dither_t *dither;
    sixel_palette_float32_entries_view_t view;
    unsigned char const *wire;
    unsigned int count;
    unsigned int index;
    float pixels[6];
    float saved[6];
    double expected;
    double rate;
    size_t emitted;
    char snap[80];
    int channel;
    int partial;
    int automatic;
    int pass;
    int ok;
    SIXELSTATUS status;

    encoder = NULL;
    output = NULL;
    frame = NULL;
    ok = 0;
    emitted = 0U;
    automatic = mode == 4;
    partial = automatic || mode == 5 ? 0 : mode % 2;
    rate = partial ? .25 : 1.0;
    memset(&view, 0, sizeof(view));
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, destination_sink,
                &emitted, NULL)) ||
            SIXEL_FAILED(sixel_frame_new(&frame, NULL))) {
        goto end;
    }
    for (pass = 0; pass < (mode == 5 ? 3 : 1); ++pass) {
        emitted = 0;
        if (pass > 0) {
            kind = 1 - kind;
        }
        for (index = 0; index < 6; ++index) {
            pixels[index] = (float)destination_snap_values[kind][0][space][0]
                [index % 3U];
        }
        memcpy(saved, pixels, sizeof(saved));
        snprintf(snap, sizeof(snap), "%s:timing=%s:rate=%s:grid=cache",
            automatic ? "auto" : policy ? "reversible" : "nearest",
            mode == 2 || mode == 3 ? "apply" : "once", partial ? ".25" : "1");
        for (index = 0; index < sizeof(options) / sizeof(options[0]); ++index) {
            if (SIXEL_FAILED(sixel_encoder_setopt(encoder, options[index].flag,
                    options[index].value))) {
                goto end;
            }
        }
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U',
                kind ? "dci-p3" : "adobe-rgb"))) {
            goto end;
        }
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'Q', models[model])) ||
                SIXEL_FAILED(sixel_encoder_setopt(encoder, '_', snap)) ||
                SIXEL_FAILED(sixel_encoder_setopt(encoder, 'X',
                    names[space])) ||
                SIXEL_FAILED(sixel_encoder_setopt(encoder, 'W',
                    names[space])) ||
                SIXEL_FAILED(sixel_frame_init_borrowed(frame, pixels, 2, 1,
                    formats[space], NULL, -1))) {
            goto end;
        }
        status = sixel_encoder_encode_frame(encoder, frame, output);
        dither = (sixel_dither_t *)encoder->diagnostic_dither;
        if (SIXEL_FAILED(status) || emitted == 0U || dither == NULL ||
                !dither->use_rgb101_palette ||
                dither->pixelformat != formats[space] ||
                memcmp(saved, pixels, sizeof(saved)) != 0 ||
                SIXEL_FAILED(dither->palette->vtbl->get_entries_float32(
                    dither->palette, &view)) || view.entries == NULL ||
                view.entry_count != 1U ||
                SIXEL_FAILED(sixel_palette_get_rgb101(dither->palette,
                    &wire, &count)) || count != 1U ||
                memcmp(wire,
                    destination_snap_wire[kind][0][space][policy][partial],
                    3) != 0) {
            goto end;
        }
        for (channel = 0; channel < 3; ++channel) {
            expected = (1.0 - rate) *
                destination_snap_values[kind][0][space][0][channel] +
                rate * destination_snap_values[kind][0][space][policy + 1]
                    [channel];
            /* Same independent 2e-5 typed-unit budget as the grid tests.
             * Wire expectations separately exclude rounding boundaries. */
            if (!isfinite(view.entries[channel]) ||
                    fabs(view.entries[channel] - expected) > 2e-5) {
                fprintf(stderr, "channel %d: %.9g expected %.12g\n",
                    channel, (double)view.entries[channel], expected);
                goto end;
            }
        }
    }
    ok = 1;
end:
    if (!ok) {
        fprintf(stderr, "destination snap %d/%d/%d/%d: %s\n",
            model, space, policy,
            mode, sixel_helper_get_additional_message());
    }
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok;
}

/* Identical RGB101 keys must retain the destination of each live cache. */
static int
destination_isolation(void)
{
    sixel_allocator_t *allocator;
    sixel_color_def_t *definition;
    sixel_snap_grid_t *grids[2] = {NULL, NULL};
    double actual[3];
    int kind;
    int pass;
    int ok;

    allocator = NULL;
    definition = NULL;
    ok = 0;
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL))) {
        goto end;
    }
    for (kind = 0; kind < 2; ++kind) {
        definition = destination_definition(kind);
        if (SIXEL_FAILED(sixel_snap_grid_new(&grids[kind],
                SIXEL_SNAP_GRID_CACHE, SIXEL_COLORSPACE_LINEAR,
                definition, allocator))) {
            goto end;
        }
        sixel_cdef_free(definition);
        definition = NULL;
    }
    if (destination_snap_codes[0][0][1][1] !=
            destination_snap_codes[1][0][1][1]) {
        goto end;
    }
    for (pass = 0; pass < 4; ++pass) {
        kind = pass % 2;
        if (SIXEL_FAILED(sixel_snap_grid_target(grids[kind],
                destination_snap_codes[kind][0][1][1], actual)) ||
                !destination_close(actual,
                    destination_snap_values[kind][0][1][2])) {
            goto end;
        }
    }
    ok = 1;
end:
    sixel_cdef_free(definition);
    sixel_snap_grid_destroy(grids[0]);
    sixel_snap_grid_destroy(grids[1]);
    sixel_allocator_unref(allocator);
    return ok;
}

/* Nonanalytic models cannot silently acquire the additive table contract. */
static int
destination_invalid(void)
{
    sixel_allocator_t *allocator;
    sixel_color_def_t *definition;
    sixel_color_def_t *icc;
    sixel_color_def_t *linear;
    sixel_snap_grid_t *grid;
    unsigned char *bytes;
    size_t length;
    double input[3] = {NAN, 0.0, 0.0};
    double result[3] = {3.0, 4.0, 5.0};
    double saved[3];
    int ok;

    allocator = NULL;
    definition = destination_definition(0);
    icc = linear = NULL;
    grid = NULL;
    bytes = NULL;
    length = 0;
    ok = 0;
    memcpy(saved, result, sizeof(saved));
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL)) ||
            SIXEL_FAILED(sixel_cdef_export_icc(definition, &bytes, &length,
                allocator))) {
        goto end;
    }
    icc = sixel_cdef_icc(color_paired_lut, sizeof(color_paired_lut));
    if (icc == NULL || sixel_snap_grid_new(&grid, SIXEL_SNAP_GRID_TABLE,
            SIXEL_COLORSPACE_LINEAR, NULL, allocator) != SIXEL_BAD_ARGUMENT ||
            grid != NULL || sixel_snap_grid_new(&grid, SIXEL_SNAP_GRID_TABLE,
                SIXEL_COLORSPACE_LINEAR, icc, allocator) !=
                    SIXEL_BAD_ARGUMENT || grid != NULL) {
        goto end;
    }
    linear = sixel_cdef_linearized(icc);
    if (linear != NULL || sixel_cdef_transfer(definition, input, result, 0) ||
            memcmp(result, saved, sizeof(saved)) != 0) {
        goto end;
    }
    input[0] = 0.5;
    if (sixel_cdef_transfer(icc, input, result, 0) ||
            sixel_cdef_transfer(definition, input, result, 2) ||
            memcmp(result, saved, sizeof(saved)) != 0) {
        goto end;
    }
    ok = SIXEL_SUCCEEDED(sixel_snap_grid_new(&grid, SIXEL_SNAP_GRID_TABLE,
        SIXEL_COLORSPACE_LINEAR, definition, allocator));
end:
    sixel_cdef_free(linear);
    sixel_cdef_free(icc);
    sixel_cdef_free(definition);
    sixel_snap_grid_destroy(grid);
    if (allocator != NULL) {
        sixel_allocator_free(allocator, bytes);
        sixel_allocator_unref(allocator);
    }
    return ok;
}

/* All seven hooks must use the selected destination rather than sRGB/P3.
 * Backend-specific stage invocation/counts are covered by the P3 witnesses. */
static int
destination_stages(int kind)
{
    static int const timings[] = {
        SIXEL_PALETTE_SNAP_TIMING_ONCE, SIXEL_PALETTE_SNAP_TIMING_PREMERGE,
        SIXEL_PALETTE_SNAP_TIMING_MERGE_ITERATION,
        SIXEL_PALETTE_SNAP_TIMING_ITERATION, SIXEL_PALETTE_SNAP_TIMING_SEED,
        SIXEL_PALETTE_SNAP_TIMING_SAMPLE, SIXEL_PALETTE_SNAP_TIMING_APPLY
    };
    sixel_allocator_t *allocator;
    sixel_palette_snap_options_t options;
    float entries[3];
    double actual[3];
    double expected[3];
    int stage;
    int c;
    int ok;

    allocator = NULL;
    ok = 0;
    memset(&options, 0, sizeof(options));
    options.policy_override = options.timing_override = 1;
    options.policy = SIXEL_PALETTE_SNAP_POLICY_NEAREST;
    options.approach_rate_override = options.channel_factor_l_override = 1;
    options.approach_rate = .25;
    options.channel_factor_l = .85;
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL))) {
        goto end;
    }
    for (stage = 0; stage < 7; ++stage) {
        options.timing = timings[stage];
        sixel_set_palette_snap_override(&options);
        sixel_palette_snap_set_output(kind ? SIXEL_NAMED_COLOR_DCI_P3
            : SIXEL_NAMED_COLOR_ADOBE_RGB, 1, SIXEL_SNAP_GRID_TABLE);
        for (c = 0; c < 3; ++c) {
            entries[c] = (float)destination_snap_values[kind][2][1][0][c];
            expected[c] = .75 * entries[c] +
                .25 * destination_snap_values[kind][2][1][1][c];
        }
        if (SIXEL_FAILED(sixel_palette_snap_begin(
                SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
                SIXEL_QUANTIZE_MODEL_KMEANS, allocator)) ||
                SIXEL_FAILED(sixel_palette_snap_stage_entries(entries, 1,
                    SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
                    (enum sixel_palette_snap_stage)stage))) {
            goto end;
        }
        for (c = 0; c < 3; ++c) {
            actual[c] = entries[c];
        }
        if (!destination_close(actual, expected) ||
                SIXEL_FAILED(sixel_palette_snap_end())) {
            goto end;
        }
    }
    ok = 1;
end:
    (void)sixel_palette_snap_end();
    sixel_set_palette_snap_override(NULL);
    sixel_allocator_unref(allocator);
    return ok;
}

int
test_destination_snap(int argc, char **argv)
{
    int kind;
    int strategy;
    int space;
    int model;
    int policy;
    int mode;

    if (argc == 2 && strcmp(argv[1], "isolation") == 0) {
        return !destination_isolation();
    }
    if (argc == 2 && strcmp(argv[1], "invalid") == 0) {
        return !destination_invalid();
    }
    if (argc < 3) {
        return 1;
    }
    kind = atoi(argv[2]);
    if (kind < 0 || kind > 2) {
        return 1;
    }
    if (argc == 3 && strcmp(argv[1], "stages") == 0 && kind < 2) {
        return !destination_stages(kind);
    }
    if (argc == 5 && strcmp(argv[1], "grid") == 0) {
        strategy = atoi(argv[3]);
        space = atoi(argv[4]);
        if (strategy < 0 || strategy > 4 || space < 0 || space > 4) {
            return 1;
        }
        return !destination_grid(kind, strategy, space);
    }
    if (argc != 7 || strcmp(argv[1], "pipeline") != 0 || kind > 1) {
        return 1;
    }
    model = atoi(argv[3]);
    space = atoi(argv[4]);
    policy = atoi(argv[5]);
    mode = atoi(argv[6]);
    if (model < 0 || model > 3 || space < 0 || space > 4 ||
            policy < 0 || policy > 1 || mode < 0 || mode > 5) {
        return 1;
    }
    return !destination_pipeline(kind, model, space, policy, mode);
}
