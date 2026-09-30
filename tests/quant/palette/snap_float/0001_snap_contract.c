/* SPDX-License-Identifier: MIT */
/* Native final snap: independent targets, partial range and actual dispatch. */
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"
#include "src/dither.h"
#include "src/palette.h"
#include "src/palette-common-snap.h"
#include "tests/data/color-numeric/snap-native.h"
#include "tests/data/color-numeric/cover-anchors.h"

static int const native_snap_formats[] = {
    SIXEL_PIXELFORMAT_RGBFLOAT32, SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
    SIXEL_PIXELFORMAT_OKLABFLOAT32, SIXEL_PIXELFORMAT_CIELABFLOAT32,
    SIXEL_PIXELFORMAT_DIN99DFLOAT32
};
static char const *const native_snap_spaces[] = {
    "gamma", "linear", "oklab", "cielab", "din99d"
};

typedef struct native_snap_capture {
    char data[2048];
    size_t size;
} native_snap_capture_t;

static int
native_snap_write(char *data, int size, void *context)
{
    native_snap_capture_t *capture;

    capture = context;
    if (size < 0 || (size_t)size >= sizeof(capture->data) - capture->size) {
        return -1;
    }
    memcpy(capture->data + capture->size, data, (size_t)size);
    capture->size += (size_t)size;
    capture->data[capture->size] = '\0';
    return size;
}

static int
native_snap_matches(float const *actual, int fixture, int space, int policy,
                    int partial)
{
    double rate;
    double expected;
    int channel;

    rate = partial ? .25 : 1.0;
    for (channel = 0; channel < 3; ++channel) {
        expected = (1 - rate) * snap_native_fixture[fixture][space][0][channel]
            + rate * snap_native_fixture[fixture][space][policy][channel];
        /* Independent typed equations start from the stored binary32 input.
         * 2e-5 native units covers matrix/TRC inverses and final storage;
         * neighboring safe targets differ by much more than this budget. */
        if (!isfinite(actual[channel]) ||
                fabs(actual[channel] - expected) > 2e-5) {
            fprintf(stderr, "channel %d: %.9g expected %.12g\n",
                channel, (double)actual[channel], expected);
            return 0;
        }
    }
    return 1;
}

static int
native_snap_values(int fixture, int space, int policy, int mode)
{
    sixel_palette_snap_options_t options;
    float entries[6];
    float saved[6];
    int channel;
    int ok;
    SIXELSTATUS status;

    memset(&options, 0, sizeof(options));
    options.policy_override = options.timing_override = 1;
    options.approach_rate_override = options.channel_factor_l_override = 1;
    options.policy = policy;
    options.approach_rate = mode == 1 ? .25 : 1;
    options.channel_factor_l = .85;
    for (channel = 0; channel < 6; ++channel) {
        entries[channel] = (float)snap_native_fixture[fixture][space][0]
            [channel % 3];
    }
    if (mode == 2) {
        options.policy = SIXEL_PALETTE_SNAP_POLICY_NONE;
    } else if (mode == 3) {
        options.approach_rate = 0;
    } else if (mode == 4 || mode == 5) {
        entries[5] = mode == 4 ? NAN : INFINITY;
    } else if (mode == 6) {
        options.timing = SIXEL_PALETTE_SNAP_TIMING_ALL;
    } else if (mode == 7) {
        entries[0] = -FLT_MAX;
        entries[1] = 98.125f / 255.0f;
        entries[2] = 0;
    }
    memcpy(saved, entries, sizeof(saved));
    sixel_set_palette_snap_override(&options);
    status = sixel_palette_snap_native(entries, 2, native_snap_formats[space]);
    ok = 0;
    if (mode >= 4 && mode <= 6) {
        ok = status == (mode == 6 ? SIXEL_BAD_ARGUMENT : SIXEL_BAD_INPUT) &&
            memcmp(saved, entries, sizeof(saved)) == 0;
        /* A rejected palette must not poison later calls on this thread. */
        options.timing = SIXEL_PALETTE_SNAP_TIMING_ONCE;
        entries[5] = saved[2];
        sixel_set_palette_snap_override(&options);
        ok = ok && SIXEL_SUCCEEDED(sixel_palette_snap_native(entries, 2,
            native_snap_formats[space]));
    } else if (SIXEL_SUCCEEDED(status)) {
        if (mode == 2 || mode == 3) {
            ok = memcmp(saved, entries, sizeof(saved)) == 0;
        } else if (mode == 7) {
            ok = entries[0] == 0 &&
                fabs(entries[1] - 99.0 / 255.0) < 1e-7 && entries[2] == 0;
        } else {
            ok = native_snap_matches(entries, fixture, space, policy, mode);
        }
    }
    sixel_set_palette_snap_override(NULL);
    return ok;
}

static int
native_snap_encode(int model, int space, int policy, int mode)
{
    static struct { int flag; char const *value; } const options[] = {
        { '=', "1" }, { 'p', "1" }, { 'F', "none" }, { 'a', "off" },
        { '~', "none" }, { 'd', "none" }, { 't', "rgb" },
        { '4', "full-frame" }, { '5', "none" }, { 'U', "gamma" }
    };
    static char const *const models[] = {
        "kmeans:seed=1", "center:seed=1", "medoids:seed=1", "heckbert", "auto"
    };
    sixel_encoder_t *encoder;
    sixel_encoder_t *other;
    sixel_output_t *output;
    sixel_frame_t *frame;
    sixel_dither_t *dither;
    sixel_palette_float32_entries_view_t view;
    native_snap_capture_t capture;
    float pixels[6];
    float saved[6];
    float first[3];
    float second[3];
    sixel_palette_float32_entries_view_t other_view;
    unsigned char bytes[6] = { 90, 136, 182, 90, 136, 182 };
    unsigned char const *wire;
    unsigned int count;
    unsigned int n;
    char snap[64];
    int byte;
    int partial;
    int ok;
    SIXELSTATUS status;

    encoder = NULL;
    other = NULL;
    output = NULL;
    frame = NULL;
    ok = 0;
    byte = mode == 2;
    partial = mode == 1 || mode == 7 || mode == 9;
    memset(&capture, 0, sizeof(capture));
    for (n = 0; n < 6; ++n) {
        pixels[n] = (float)snap_native_fixture[0][space][0][n % 3];
    }
    memcpy(saved, pixels, sizeof(saved));
    snprintf(snap, sizeof(snap), "%s:timing=once:rate=%s",
        policy == 1 ? "nearest" : "reversible", partial ? "0.25" : "1");
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, native_snap_write,
                &capture, NULL)) ||
            SIXEL_FAILED(sixel_frame_new(&frame, NULL))) {
        goto end;
    }
    for (n = 0; n < sizeof(options) / sizeof(options[0]); ++n) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, options[n].flag,
                options[n].value))) {
            goto end;
        }
    }
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'Q', models[model])) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '_', snap)) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '.',
                byte ? "8bit" : "float32")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'X',
                native_snap_spaces[space])) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'W',
                native_snap_spaces[space])) ||
            SIXEL_FAILED(sixel_frame_init_borrowed(frame,
                byte ? (void *)bytes : (void *)pixels, 2, 1,
                byte ? SIXEL_PIXELFORMAT_RGB888 : native_snap_formats[space],
                NULL, -1))) {
        goto end;
    }
    if (mode >= 6 &&
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U',
                mode < 8 ? "linear" : "smpte-c"))) {
        goto end;
    }
    if (mode == 3 || mode == 4) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U', "display-p3"))) {
            goto end;
        }
        status = sixel_encoder_encode_frame(encoder, frame, output);
        if (status != SIXEL_BAD_ARGUMENT || capture.size != 0) {
            goto end;
        }
        if (mode == 3) {
            ok = 1;
            goto end;
        }
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U', "gamma"))) {
            goto end;
        }
    }
    status = sixel_encoder_encode_frame(encoder, frame, output);
    dither = (sixel_dither_t *)encoder->diagnostic_dither;
    if (SIXEL_FAILED(status) || dither == NULL ||
            memcmp(saved, pixels, sizeof(saved)) != 0) {
        goto end;
    }
    if (byte) {
        ok = dither->use_rgb101_palette == 0 &&
            dither->pixelformat == SIXEL_PIXELFORMAT_RGB888;
        goto end;
    }
    if (!dither->use_rgb101_palette ||
            SIXEL_FAILED(dither->palette->vtbl->get_entries_float32(
                dither->palette, &view)) || view.entries == NULL ||
            view.entry_count != 1 ||
            SIXEL_FAILED(sixel_palette_get_rgb101(dither->palette,
                &wire, &count)) || count != 1) {
        goto end;
    }
    /* The wire oracle has >0.02 percentage-code distance to a rounding
     * boundary, independent of the native-component comparison budget. */
    ok = native_snap_matches(view.entries, 0, space, policy, partial) &&
        memcmp(wire, mode >= 6
            ? snap_native_output[space][mode >= 8][policy - 1][partial]
            : snap_native_wire[0][space][policy - 1][partial], 3) == 0;
    if (ok && mode == 5) {
        memcpy(first, view.entries, sizeof(first));
        ok = 0;
        if (SIXEL_FAILED(sixel_encoder_new(&other, NULL))) {
            goto end;
        }
        for (n = 0; n < sizeof(options) / sizeof(options[0]); ++n) {
            if (SIXEL_FAILED(sixel_encoder_setopt(other, options[n].flag,
                    options[n].value))) {
                goto end;
            }
        }
        if (SIXEL_FAILED(sixel_encoder_setopt(other, '.', "float32")) ||
                SIXEL_FAILED(sixel_encoder_setopt(other, '_',
                    "reversible:timing=once:rate=0.25")) ||
                SIXEL_FAILED(sixel_encoder_encode_frame(other, frame,
                    output))) {
            goto end;
        }
        dither = (sixel_dither_t *)other->diagnostic_dither;
        if (dither == NULL || !dither->use_rgb101_palette ||
                SIXEL_FAILED(dither->palette->vtbl->get_entries_float32(
                    dither->palette, &other_view)) ||
                other_view.entries == NULL ||
                !native_snap_matches(other_view.entries, 0, 0, 2, 1) ||
                memcmp(first, view.entries, sizeof(first)) != 0) {
            goto end;
        }
        memcpy(second, other_view.entries, sizeof(second));
        if (SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame, output))) {
            goto end;
        }
        dither = (sixel_dither_t *)encoder->diagnostic_dither;
        ok = dither != NULL && dither->use_rgb101_palette &&
            SIXEL_SUCCEEDED(dither->palette->vtbl->get_entries_float32(
                dither->palette, &view)) && view.entries != NULL &&
            native_snap_matches(view.entries, 0, 0, 1, 0) &&
            memcmp(second, other_view.entries, sizeof(second)) == 0;
    }
end:
    if (!ok) {
        fprintf(stderr, "native snap model=%d space=%d policy=%d mode=%d: %s\n",
            model, space, policy, mode, sixel_helper_get_additional_message());
    }
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    sixel_encoder_unref(other);
    return ok;
}

/* A constant source separates snap from repair without a solver oracle.
 * Full/three-quarter snap moves it beyond the cover reach threshold. Soft
 * must append the original source after snapping; hard appends eight fixed
 * corners. A second snap or the reverse ordering fails independent values.
 * The fixed-size variant starts with two negative grays: their snapped
 * zeros fund a gamma midpoint plus the source cell mean (-0.24). */
static int
native_snap_cover(int model, int space, int policy, int mode)
{
    static struct { int flag; char const *value; } const options[] = {
        { '=', "1" }, { 'p', "1" }, { 'F', "none" }, { '~', "none" },
        { 'd', "none" }, { 't', "rgb" }, { '4', "full-frame" },
        { '5', "none" }, { 'U', "gamma" }
    };
    static char const *const models[] = {
        "kmeans:seed=1", "center:seed=1", "medoids:seed=1", "heckbert", "auto"
    };
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    sixel_dither_t *dither;
    sixel_palette_float32_entries_view_t view;
    native_snap_capture_t capture;
    float pixels[12];
    float saved[12];
    unsigned char bytes[12];
    unsigned char const *wire;
    unsigned int count;
    unsigned int n;
    unsigned int channel;
    double rate;
    double expected;
    int hard;
    int merge;
    int ok;
    char snap[80];
    SIXELSTATUS status;

    encoder = NULL;
    output = NULL;
    frame = NULL;
    ok = 0;
    hard = mode == 2 || mode == 3;
    merge = mode == 4;
    rate = mode == 1 || mode == 3 || merge || mode == 5 ? 1 : .75;
    memset(&capture, 0, sizeof(capture));
    memset(bytes, 90, sizeof(bytes));
    for (n = 0; n < 12; ++n) {
        pixels[n] = merge ? (n < 6 ? -.25f : -.23f)
            : (float)snap_native_fixture[0][space][0][n % 3];
    }
    memcpy(saved, pixels, sizeof(saved));
    snprintf(snap, sizeof(snap), "%s:timing=%s:rate=%s",
        policy == 1 ? "nearest" : "reversible",
        mode == 8 ? "all" : "once", rate == 1 ? "1" : "0.75");
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, native_snap_write,
                &capture, NULL)) ||
            SIXEL_FAILED(sixel_frame_new(&frame, NULL))) {
        goto end;
    }
    for (n = 0; n < sizeof(options) / sizeof(options[0]); ++n) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, options[n].flag,
                options[n].value))) {
            goto end;
        }
    }
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'Q', models[model])) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '_', snap)) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '.',
                mode == 6 ? "8bit" : "float32")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'a',
                mode == 7 ? "corners:V1" : merge ? "corners:Wsoft:V0" :
                hard ? "corners:Whard:V1" : "corners:Wsoft:V1")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'X',
                native_snap_spaces[space])) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'W',
                native_snap_spaces[space])) ||
            SIXEL_FAILED(sixel_frame_init_borrowed(frame,
                mode == 6 ? (void *)bytes : (void *)pixels, 2, 2,
                mode == 6 ? SIXEL_PIXELFORMAT_RGB888 :
                native_snap_formats[space], NULL, -1))) {
        goto end;
    }
    if (merge && SIXEL_FAILED(sixel_encoder_setopt(encoder, 'p', "2"))) {
        goto end;
    }
    if (mode == 5 &&
            (SIXEL_FAILED(sixel_encoder_setopt(encoder, '=', "4")) ||
             SIXEL_FAILED(sixel_encoder_setopt(encoder, '4',
                 "adaptive-grid")))) {
        goto end;
    }
    if (mode == 9) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U', "display-p3"))) {
            goto end;
        }
        status = sixel_encoder_encode_frame(encoder, frame, output);
        if (status != SIXEL_BAD_ARGUMENT || capture.size != 0 ||
                SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U', "gamma"))) {
            goto end;
        }
    }
    status = sixel_encoder_encode_frame(encoder, frame, output);
    dither = (sixel_dither_t *)encoder->diagnostic_dither;
    if (SIXEL_FAILED(status) || dither == NULL ||
            memcmp(saved, pixels, sizeof(saved)) != 0) {
        goto end;
    }
    if (mode >= 6 && mode <= 8) {
        ok = !dither->use_rgb101_palette &&
            (mode != 6 || dither->pixelformat == SIXEL_PIXELFORMAT_RGB888);
        goto end;
    }
    if (!dither->use_rgb101_palette ||
            SIXEL_FAILED(dither->palette->vtbl->get_entries_float32(
                dither->palette, &view)) || view.entries == NULL ||
            view.entry_count != (hard ? 9u : 2u) ||
            SIXEL_FAILED(sixel_palette_get_rgb101(dither->palette,
                &wire, &count)) || count != view.entry_count) {
        goto end;
    }
    for (n = 0; n < count; ++n) {
        for (channel = 0; channel < 3; ++channel) {
            expected = merge ? (n == 0 ? 0 : -.24)
                : n == 0 ? (1 - rate) *
                    snap_native_fixture[0][space][0][channel] + rate *
                    snap_native_fixture[0][space][policy][channel]
                : hard ? color_cover_anchors[space][n - 1][channel]
                : snap_native_fixture[0][space][0][channel];
            /* Same 2e-5 typed-coordinate budget as the independent snap
             * and cover oracles; wire anchors have no rounding ties. */
            if (!isfinite(view.entries[n * 3u + channel]) ||
                    fabs(view.entries[n * 3u + channel] - expected) > 2e-5) {
                fprintf(stderr, "entry %u channel %u: %.9g expected %.12g\n",
                    n, channel, (double)view.entries[n * 3u + channel],
                    expected);
                goto end;
            }
            if (n > 0 && wire[n * 3u + channel] != (merge ? 0 : hard
                    ? (int)(100 * color_cover_anchors[0][n - 1][channel])
                    : channel == 0 ? 0 : channel == 1 ? 100 : 38)) {
                goto end;
            }
        }
    }
    ok = 1;
end:
    if (!ok) {
        fprintf(stderr, "snap/cover %d %d %d %d: %s\n", model, space,
            policy, mode, sixel_helper_get_additional_message());
    }
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok;
}

int
test_native_snap(int argc, char **argv)
{
    if (argc != 6) {
        return 1;
    }
    return !(strcmp(argv[1], "cover") == 0
        ? native_snap_cover(atoi(argv[2]), atoi(argv[3]), atoi(argv[4]),
            atoi(argv[5]))
        : strcmp(argv[1], "encode") == 0
        ? native_snap_encode(atoi(argv[2]), atoi(argv[3]), atoi(argv[4]),
            atoi(argv[5]))
        : native_snap_values(atoi(argv[2]), atoi(argv[3]), atoi(argv[4]),
            atoi(argv[5])));
}
