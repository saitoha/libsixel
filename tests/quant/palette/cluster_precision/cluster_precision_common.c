/* SPDX-License-Identifier: MIT */
/* A float clustering view must not return through a byte working palette. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"
#include "src/dither.h"
#include "src/palette.h"

typedef struct cluster_capture {
    char data[4096];
    size_t size;
} cluster_capture_t;

static int
cluster_write(char *data, int size, void *context)
{
    cluster_capture_t *capture;

    capture = context;
    if (size < 0 || (size_t)size >= sizeof(capture->data) - capture->size) {
        return -1;
    }
    memcpy(capture->data + capture->size, data, (size_t)size);
    capture->size += (size_t)size;
    capture->data[capture->size] = '\0';
    return size;
}

int
test_cluster_precision(int mode)
{
    static struct { int flag; char const *value; } const options[] = {
        { '=', "1" }, { 'F', "none" }, { 'a', "off" }, { '_', "none" },
        { '~', "none" }, { 'd', "none" }, { 't', "rgb" },
        { '4', "full-frame" }, { '5', "none" }, { 'W', "gamma" }
    };
    static char const *const spaces[] = {
        "linear", "oklab", "cielab", "din99d", "gamma"
    };
    /* Independent equal-population means of sRGB red and blue, calculated
     * in the named clustering geometry, then transformed to signed sRGB.
     * Linear: encode(.5, 0, .5). Oklab uses the published forward matrices
     * and their independently solved inverses. Lab uses D65, the rounded
     * XYZ matrix and L/100,a/128,b/128; DIN99d uses the corrected XYZ basis.
     * Float storage plus rounded inverse matrices warrant 8e-5 absolute
     * tolerance, not bit equality. This is far below a byte step and the
     * CIELAB negative-green witness. No production transform is the oracle. */
    static double const expected[4][3] = {
        { .7353569830524495, 0, .7353569830524495 },
        { .5504410622883958, .3256206832141300, .6365006995136325 },
        { .7902415773292379, -.1423675952850459, .5352759010687180 },
        { .5628849061899642, .3109729000328108, .4723033791015568 }
    };
    static char const *const wire[] = {
        ";2;74;0;74", ";2;55;33;64", ";2;79;0;54", ";2;56;31;47"
    };
    unsigned char pixels[6] = { 255, 0, 0, 0, 0, 255 };
    unsigned char original[6];
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    sixel_dither_t *dither;
    sixel_palette_float32_entries_view_t view;
    cluster_capture_t capture;
    char const *solver;
    int space;
    int pass;
    int count;
    int native;
    int model;
    int i;
    int c;
    int found;
    int ok;
    double value;
    SIXELSTATUS status;

    encoder = NULL;
    output = NULL;
    frame = NULL;
    ok = 0;
    memcpy(original, pixels, sizeof(pixels));
    memset(&capture, 0, sizeof(capture));
    memset(&view, 0, sizeof(view));
    solver = mode == 4 ? "heckbert" : mode == 5 ? "center"
        : mode == 6 ? "medoids" : mode == 7 ? "auto" : "kmeans:seed=1";
    count = mode == 5 || mode == 6 ? 2 : 1;
    model = mode == 4 || mode == 7 ? SIXEL_QUANTIZE_MODEL_MEDIANCUT
        : mode == 5 ? SIXEL_QUANTIZE_MODEL_KCENTER
        : mode == 6 ? SIXEL_QUANTIZE_MODEL_KMEDOIDS
        : SIXEL_QUANTIZE_MODEL_KMEANS;
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, cluster_write,
                &capture, NULL))) {
        goto end;
    }
    for (i = 0; i < (int)(sizeof(options) / sizeof(options[0])); ++i) {
        if (mode == 9 && options[i].flag == 'a') {
            continue;
        }
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, options[i].flag,
                options[i].value))) {
            goto end;
        }
    }
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'Q', solver)) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '.',
                mode == 10 ? "8bit" : "auto")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder,
                mode == 9 ? 'b' : 'p', mode == 9 ? "gray8"
                : count == 2 ? "2" : "1")) ||
            (mode == 12 && SIXEL_FAILED(sixel_encoder_setopt(
                encoder, 'd', "fs"))) ||
            (mode == 13 && SIXEL_FAILED(sixel_encoder_setopt(
                encoder, 'U', "linear")))) {
        goto end;
    }
    for (pass = 0; pass < (mode == 11 ? 3 : 1); ++pass) {
        space = mode < 4 ? mode : mode == 8 || mode == 12 ? 4 : 0;
        if (mode == 11) {
            space = pass == 1 ? 4 : pass == 2 ? 2 : 0;
        }
        /* precision-policy=8bit rounds the float clustering palette. */
        native = space != 4 && mode != 9 && mode != 10;
        memset(&capture, 0, sizeof(capture));
        memset(&view, 0, sizeof(view));
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'X', spaces[space])) ||
                SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
                SIXEL_FAILED(sixel_frame_init_borrowed(frame, pixels, 2, 1,
                    SIXEL_PIXELFORMAT_RGB888, NULL, -1))) {
            goto end;
        }
        status = sixel_encoder_encode_frame(encoder, frame, output);
        dither = (sixel_dither_t *)encoder->diagnostic_dither;
        if (SIXEL_FAILED(status) || dither == NULL ||
                dither->use_rgb101_palette != native ||
                (mode != 9 && dither->quantize_model != model) ||
                dither->pixelformat != SIXEL_PIXELFORMAT_RGB888 ||
                encoder->prefer_float32 != 0 ||
                sixel_frame_get_pixelformat(frame) !=
                    SIXEL_PIXELFORMAT_RGB888 ||
                memcmp(pixels, original, sizeof(pixels)) != 0 ||
                SIXEL_FAILED(dither->palette->vtbl->get_entries_float32(
                    dither->palette, &view))) {
            goto end;
        }
        if (native) {
            if (view.entries == NULL || view.entry_count != (size_t)count) {
                goto end;
            }
            if (count == 1) {
                for (c = 0; c < 3; ++c) {
                    if (!isfinite(view.entries[c]) ||
                            fabs(view.entries[c] - expected[space][c]) >
                                8e-5) {
                        goto end;
                    }
                }
                if (strstr(capture.data, mode == 13 ? ";2;50;0;50"
                        : wire[space]) == NULL) {
                    goto end;
                }
            } else {
                /* A two-slot palette must retain both exact input colors;
                 * order is solver-owned, so compare the unordered entries. */
                found = 0;
                for (i = 0; i < 2; ++i) {
                    value = view.entries[i * 3];
                    if (fabs(value - 1) < 1e-6 &&
                            fabs(view.entries[i * 3 + 1]) < 1e-6 &&
                            fabs(view.entries[i * 3 + 2]) < 1e-6) {
                        found |= 1;
                    }
                    if (fabs(value) < 1e-6 &&
                            fabs(view.entries[i * 3 + 1]) < 1e-6 &&
                            fabs(view.entries[i * 3 + 2] - 1) < 1e-6) {
                        found |= 2;
                    }
                }
                if (found != 3) {
                    goto end;
                }
            }
        } else if (mode != 12 && view.entries != NULL) {
            goto end;
        }
        sixel_frame_unref(frame);
        frame = NULL;
    }
    ok = 1;
end:
    if (!ok) {
        fprintf(stderr, "cluster precision mode=%d: %s; wire=%s\n", mode,
            sixel_helper_get_additional_message(), capture.data);
        if (view.entries != NULL) {
            fprintf(stderr, "working=(%.9g, %.9g, %.9g)\n",
                view.entries[0], view.entries[1], view.entries[2]);
        }
    }
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
