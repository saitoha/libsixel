/* SPDX-License-Identifier: MIT */
/* P3 snap must preserve typed targets, wire codes and borrowed source data. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"
#include "src/dither.h"
#include "src/palette.h"
#include "tests/data/color-numeric/p3-snap.h"

static int
p3_snap_sink(char *data, int size, void *context)
{
    size_t *count;

    (void)data;
    count = context;
    *count += (size_t)size;
    return size;
}

static int
p3_snap_pipeline(int model, int space, int policy, int mode)
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
        { '4', "full-frame" }, { '5', "none" }, { 'U', "display-p3" },
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
    int ok;
    SIXELSTATUS status;

    encoder = NULL;
    output = NULL;
    frame = NULL;
    ok = 0;
    emitted = 0U;
    automatic = mode == 4;
    partial = automatic ? 0 : mode % 2;
    rate = partial ? .25 : 1.0;
    memset(&view, 0, sizeof(view));
    for (index = 0; index < 6; ++index) {
        pixels[index] = (float)p3_snap_values[0][space][0][index % 3U];
    }
    memcpy(saved, pixels, sizeof(saved));
    snprintf(snap, sizeof(snap), "%s:timing=%s:rate=%s:grid=cache",
        automatic ? "auto" : policy ? "reversible" : "nearest",
        mode == 2 || mode == 3 ? "apply" : "once", partial ? ".25" : "1");
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, p3_snap_sink,
                &emitted, NULL)) ||
            SIXEL_FAILED(sixel_frame_new(&frame, NULL))) {
        goto end;
    }
    for (index = 0; index < sizeof(options) / sizeof(options[0]); ++index) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, options[index].flag,
                options[index].value))) {
            goto end;
        }
    }
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'Q', models[model])) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '_', snap)) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'X', names[space])) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'W', names[space])) ||
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
            memcmp(wire, p3_snap_wire[0][space][policy][partial], 3) != 0) {
        goto end;
    }
    for (channel = 0; channel < 3; ++channel) {
        expected = (1.0 - rate) * p3_snap_values[0][space][0][channel] +
            rate * p3_snap_values[0][space][policy + 1][channel];
        /* Same independent 2e-5 typed-unit budget as the grid tests.
         * Wire expectations separately exclude rounding boundaries. */
        if (!isfinite(view.entries[channel]) ||
                fabs(view.entries[channel] - expected) > 2e-5) {
            fprintf(stderr, "channel %d: %.9g expected %.12g\n",
                channel, (double)view.entries[channel], expected);
            goto end;
        }
    }
    ok = 1;
end:
    if (!ok) {
        fprintf(stderr, "P3 snap %d/%d/%d/%d: %s\n", model, space, policy,
            mode, sixel_helper_get_additional_message());
    }
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok;
}

/* Disjoint gray pairs make every Lloyd mean and Ward join independent of
 * seed order. Quarter-rate stages can then be counted by their actual values,
 * not merely by a trace saying that a policy was selected. */
static int
p3_snap_stage(char const *timing, int model)
{
    static char const *const models[] = {
        "kmeans:seed=1:iter=1:miniter=1:polish_iter=0", "center:seed=1",
        "medoids:seed=1:histbits=6:prune_mass=1:rare_keep=4", "heckbert"};
    static struct { int flag; char const *value; } const options[] = {
        { '=', "greedy:B1" }, { 'p', "2" }, { 'a', "off" }, { '~', "none" },
        { 'd', "none" }, { 't', "rgb" }, { '4', "full-frame" },
        { '5', "none" }, { 'U', "display-p3" },
        { 'X', "gamma" }, { 'W', "gamma" }
    };
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    sixel_dither_t *dither;
    sixel_palette_float32_entries_view_t view;
    float pixels[12];
    float saved[12];
    double expected;
    double target;
    size_t emitted;
    char snap[80];
    unsigned int index;
    unsigned int first;
    unsigned int row;
    int steps;
    int step;
    int merge;
    int refine;
    int ok;

    encoder = NULL;
    output = NULL;
    frame = NULL;
    emitted = 0U;
    ok = 0;
    refine = strcmp(timing, "merge-refinement") == 0;
    merge = refine || strcmp(timing, "premerge") == 0 ||
        strcmp(timing, "merge-iteration") == 0;
    steps = merge || strcmp(timing, "sample") == 0 ||
        strcmp(timing, "iteration") == 0 ? 2 : 1;
    for (index = 0; index < 12; ++index) {
        /* Medoids has a private candidate grid even with -5none. Separate
         * its points into distinct cells so this witness reaches Ward. */
        pixels[index] = model == 2
            ? (float)((index < 6 ? 96.875 : 198.875) +
                ((index / 3U) % 2U) * 5.0) / 255.0f
            : (float)((index < 6 ? 98.125 : 200.125) +
                ((index / 3U) % 2U) * .125) / 255.0f;
    }
    memcpy(saved, pixels, sizeof(saved));
    snprintf(snap, sizeof(snap), "nearest:timing=%s:rate=.25:grid=table",
        refine ? "merge-iteration" : timing);
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, p3_snap_sink,
                &emitted, NULL)) ||
            SIXEL_FAILED(sixel_frame_new(&frame, NULL))) {
        goto end;
    }
    for (index = 0; index < sizeof(options) / sizeof(options[0]); ++index) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, options[index].flag,
                options[index].value))) {
            goto end;
        }
    }
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'Q', models[model])) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '_', snap)) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'F',
                refine ? "ward:O2:L1" : merge ? "ward:O2:L0" : "none")) ||
            SIXEL_FAILED(sixel_frame_init_borrowed(frame, pixels, 4, 1,
                SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, -1)) ||
            SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame, output))) {
        goto end;
    }
    dither = (sixel_dither_t *)encoder->diagnostic_dither;
    if (dither == NULL || !dither->use_rgb101_palette || emitted == 0U ||
            memcmp(saved, pixels, sizeof(saved)) != 0 ||
            SIXEL_FAILED(dither->palette->vtbl->get_entries_float32(
                dither->palette, &view)) || view.entries == NULL ||
            view.entry_count != 2U) {
        goto end;
    }
    first = view.entries[0] < view.entries[3] ? 0U : 1U;
    for (row = 0; row < 2; ++row) {
        expected = ((double)pixels[row * 6] + pixels[row * 6 + 3]) * .5;
        target = (row == 0 ? 99.0 : 201.0) / 255.0;
        for (step = 0; step < steps; ++step) {
            /* Before joining medoid pairs, the two independently nearest
             * grid grays average to 99.5 / 201.5, not to 99 / 201. */
            expected = expected * .75 + .25 *
                (model == 2 && merge && step == 0 &&
                 strcmp(timing, "premerge") == 0
                 ? (row == 0 ? 99.5 : 201.5) / 255.0 : target);
        }
        for (index = 0; index < 3; ++index) {
            if (!isfinite(view.entries[((first + row) % 2) * 3 + index]) ||
                    fabs(view.entries[((first + row) % 2) * 3 + index] -
                        expected) > 2e-7) {
                fprintf(stderr, "%s entry %u: %.12g expected %.12g\n",
                    timing, row,
                    (double)view.entries[((first + row) % 2) * 3 + index],
                    expected);
                goto end;
            }
        }
    }
    ok = 1;
end:
    if (!ok) {
        fprintf(stderr, "stage %s model %d: %s\n", timing, model,
            sixel_helper_get_additional_message());
    }
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok;
}

/* Growing hard cover from gray introduces eight unsnapped sRGB corners.
 * Every corner must be projected after cover, then converted from -Xgamma
 * to -Wlinear. Moving this hook before cover cannot satisfy these values. */
static int
p3_snap_cover_values(int partial)
{
    static struct { int flag; char const *value; } const options[] = {
        { '=', "greedy:B1" }, { 'p', "16" }, { 'F', "none" },
        { 'a', "corners:Whard:V1" }, { '~', "none" }, { 'd', "none" },
        { 't', "rgb" }, { 'Q', "kmeans:seed=1" }, { 'U', "display-p3" },
        { '4', "full-frame" }, { '5', "none" },
        { 'X', "gamma" }, { 'W', "linear" }
    };
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    sixel_dither_t *dither;
    sixel_palette_float32_entries_view_t view;
    float pixels[6] = {.4f, .4f, .4f, .6f, .6f, .6f};
    size_t emitted;
    unsigned int index;
    unsigned int corner;
    unsigned int entry;
    int channel;
    int matches;
    int found;
    int ok;

    encoder = NULL;
    output = NULL;
    frame = NULL;
    emitted = 0;
    ok = 0;
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, p3_snap_sink,
                &emitted, NULL)) ||
            SIXEL_FAILED(sixel_frame_new(&frame, NULL))) {
        goto end;
    }
    for (index = 0; index < sizeof(options) / sizeof(options[0]); ++index) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, options[index].flag,
                options[index].value))) {
            goto end;
        }
    }
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder, '_', partial
            ? "nearest:timing=apply:rate=.25" : "nearest:timing=apply")) ||
            SIXEL_FAILED(sixel_frame_init_borrowed(frame, pixels, 2, 1,
                SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, -1)) ||
            SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame, output))) {
        goto end;
    }
    dither = (sixel_dither_t *)encoder->diagnostic_dither;
    if (dither == NULL || emitted == 0 ||
            dither->pixelformat != SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 ||
            SIXEL_FAILED(dither->palette->vtbl->get_entries_float32(
                dither->palette, &view)) || view.entries == NULL ||
            view.entry_count != 10) {
        goto end;
    }
    for (corner = 0; corner < 8; ++corner) {
        found = 0;
        for (entry = 0; entry < view.entry_count; ++entry) {
            matches = 1;
            for (channel = 0; channel < 3; ++channel) {
                matches = matches && isfinite(view.entries[entry*3+channel]) &&
                    fabs(view.entries[entry*3+channel] -
                        p3_snap_cover[partial][corner][channel]) < 2e-5;
            }
            found += matches;
        }
        if (found != 1) {
            fprintf(stderr, "cover corner %u: %d matches\n", corner, found);
            goto end;
        }
    }
    ok = 1;
end:
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok;
}

int
test_p3_snap_pipeline(int argc, char **argv)
{
    int model;
    int space;
    int policy;
    int mode;

    if (argc == 3 && strcmp(argv[1], "cover") == 0) {
        return !p3_snap_cover_values(atoi(argv[2]) != 0);
    }
    if (argc == 4 && strcmp(argv[1], "stage") == 0) {
        model = atoi(argv[3]);
        return model < 0 || model > 3 || !p3_snap_stage(argv[2], model);
    }
    if (argc != 5) {
        return 1;
    }
    model = atoi(argv[1]);
    space = atoi(argv[2]);
    policy = atoi(argv[3]);
    mode = atoi(argv[4]);
    if (model < 0 || model > 3 || space < 0 || space > 4 ||
            policy < 0 || policy > 1 || mode < 0 || mode > 4) {
        return 1;
    }
    return !p3_snap_pipeline(model, space, policy, mode);
}
