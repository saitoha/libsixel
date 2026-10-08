/* SPDX-License-Identifier: MIT */
/* AUTO admission: literal coordinates, actual solver and frame-local state. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"
#include "src/dither.h"
#include "src/palette.h"
#include "src/palette-kmeans.h"

typedef struct auto_capture {
    char data[2048];
    size_t size;
} auto_capture_t;

static int
auto_write(char *data, int size, void *context)
{
    auto_capture_t *capture;

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
test_auto_palette(int mode, int space)
{
    static struct { int flag; char const *value; } const options[] = {
        { '=', "1" }, { 'p', "2" }, { 'F', "none" }, { 'a', "off" },
        { '_', "none" }, { '~', "5bit" }, { 'd', "none" },
        { 't', "rgb" }, { '4', "full-frame" }, { '0', "hard" }
    };
    static char const *const spaces[] = {
        "gamma", "linear", "oklab", "cielab", "din99d"
    };
    static int const formats[] = {
        SIXEL_PIXELFORMAT_RGBFLOAT32, SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
        SIXEL_PIXELFORMAT_OKLABFLOAT32, SIXEL_PIXELFORMAT_CIELABFLOAT32,
        SIXEL_PIXELFORMAT_DIN99DFLOAT32
    };
    float pixels[9] = { -.25f, -.25f, -.25f, 1.25f, 1.25f, 1.25f };
    unsigned char bytes[6] = { 32, 32, 32, 224, 224, 224 };
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    sixel_dither_t *dither;
    sixel_palette_float32_entries_view_t view;
    auto_capture_t capture;
    SIXELSTATUS status;
    char const *binning;
    int byte_input;
    int byte_work;
    int p3;
    int reject;
    int solver;
    int passes;
    int pass;
    int count;
    int i;
    int j;
    int channel;
    int matched;
    int ok;
    double expected;
    double tolerance;

    encoder = NULL;
    output = NULL;
    frame = NULL;
    ok = 0;
    memset(&capture, 0, sizeof(capture));
    memset(&view, 0, sizeof(view));
    byte_input = mode == 4 || mode == 5 || mode == 14;
    byte_work = byte_input && mode != 5;
    p3 = mode == 2 || mode == 10 || mode == 11 || mode >= 15;
    reject = mode == 17;
    passes = mode == 9 ? 3 : 1;
    count = mode == 12 ? 3 : 2;
    if (space < 0 || space > 4) {
        return EXIT_FAILURE;
    }
    if (space == 3) {
        pixels[0] = -25.0f;
        pixels[3] = 125.0f;
        pixels[1] = pixels[2] = -160.0f;
        pixels[4] = pixels[5] = 160.0f;
    }
    if (mode == 12) {
        for (i = 0; i < 9; ++i) {
            pixels[i] = .5001f + (float)(i / 3) * .0001f;
        }
    }
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, auto_write,
                &capture, NULL))) {
        goto end;
    }
    for (i = 0; i < (int)(sizeof(options) / sizeof(options[0])); ++i) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, options[i].flag,
                options[i].value))) {
            goto end;
        }
    }
    if (((mode == 1 || mode == 16) &&
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'Q', "auto"))) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'X', spaces[space])) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'W', spaces[space])) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U',
                p3 ? "display-p3" : "gamma")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '.',
                mode == 5 ? "float32" : mode == 3 || byte_work
                ? "8bit" : "auto")) ||
            (mode == 10 &&
             SIXEL_FAILED(sixel_encoder_setopt(encoder, 'a', "corners"))) ||
            ((mode == 8 || mode == 11) &&
             SIXEL_FAILED(sixel_encoder_setopt(encoder, '_',
                mode == 8 ? "nearest:rate=0" : "nearest:rate=0.5"))) ||
            (mode == 12 &&
             SIXEL_FAILED(sixel_encoder_setopt(encoder, 'd', "fs")))) {
        goto end;
    }
    if (mode == 17 || mode == 18) {
        /* Inject pending model-specific state at the private admission
         * boundary. Only the resolved K-means consumer restricts feedback. */
        encoder->quantize_model_kmeans_feedback_mode =
            SIXEL_PALETTE_KMEANS_FEEDBACK_ON;
    }
    for (pass = 0; pass < passes; ++pass) {
        binning = mode == 6 ? "exact" : mode == 7 ? "soft"
            : mode == 14 || mode == 15 || mode == 17 ||
                (mode == 9 && pass == 1)
            ? "hard" : mode == 13 ? "auto" : "none";
        solver = strcmp(binning, "none") == 0 ||
            strcmp(binning, "auto") == 0 ? SIXEL_QUANTIZE_MODEL_MEDIANCUT
                                       : SIXEL_QUANTIZE_MODEL_KMEANS;
        /* Mode zero leaves binning omitted too. The three-pass case uses
         * one encoder to prove that a resolved consumer is not retained. */
        if (mode != 0 && SIXEL_FAILED(sixel_encoder_setopt(
                encoder, '5', binning))) {
            goto end;
        }
        memset(&capture, 0, sizeof(capture));
        if (SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
                SIXEL_FAILED(sixel_frame_init_borrowed(frame,
                    byte_input ? (void *)bytes : (void *)pixels, count, 1,
                    byte_input ? SIXEL_PIXELFORMAT_RGB888 : formats[space],
                    NULL, -1))) {
            goto end;
        }
        status = sixel_encoder_encode_frame(encoder, frame, output);
        if (reject) {
            /* A later solver rejection must not masquerade as preflight. */
            ok = SIXEL_FAILED(status) && capture.size == 0U &&
                strstr(sixel_helper_get_additional_message(),
                    "display-p3 does not support K-means feedback") != NULL;
            goto end;
        }
        dither = (sixel_dither_t *)encoder->diagnostic_dither;
        if (SIXEL_FAILED(status) || dither == NULL ||
                encoder->quantize_model != SIXEL_QUANTIZE_MODEL_AUTO ||
                dither->quantize_model != solver || dither->ncolors != 2 ||
                dither->use_rgb101_palette != !byte_work ||
                dither->pixelformat != (byte_work
                    ? SIXEL_PIXELFORMAT_RGB888 : formats[space])) {
            goto end;
        }
        if (byte_work) {
            if (strstr(capture.data, mode == 14
                    ? ";2;13;13;13" : ";2;14;14;14") == NULL ||
                    strstr(capture.data, mode == 14
                    ? ";2;88;88;88" : ";2;89;89;89") == NULL) {
                goto end;
            }
        } else if (mode == 12) {
            if (dither->origcolors != 3 ||
                    dither->method_for_diffuse != SIXEL_DIFFUSE_FS) {
                goto end;
            }
        } else {
            if (SIXEL_FAILED(dither->palette->vtbl->get_entries_float32(
                    dither->palette, &view)) || view.entries == NULL ||
                    view.entry_count != 2) {
                goto end;
            }
            /* Two colors need no reduction. Check the input coordinates
             * independently of the compatibility byte and RGB101 copies.
             * The small budget covers representation/centroid arithmetic. */
            for (i = 0; i < 2; ++i) {
                matched = 0;
                for (j = 0; j < 2; ++j) {
                    for (channel = 0; channel < 3; ++channel) {
                        expected = byte_input
                            ? bytes[i * 3 + channel] / 255.0
                            : pixels[i * 3 + channel];
                        if (mode == 11) {
                            expected = .5 * expected + .5 * (i == 1);
                        }
                        tolerance = 1e-7 + fabs(expected) * 5e-6;
                        if (!isfinite(view.entries[j * 3 + channel]) ||
                                fabs(view.entries[j * 3 + channel]
                                    - expected) > tolerance) {
                            break;
                        }
                    }
                    matched += channel == 3;
                }
                if (matched != 1) {
                    goto end;
                }
            }
            if (space == 0 &&
                    (strstr(capture.data, byte_input ? ";2;13;13;13"
                             : ";2;0;0;0") == NULL ||
                     strstr(capture.data, byte_input ? ";2;88;88;88"
                             : ";2;100;100;100") == NULL)) {
                goto end;
            }
        }
        sixel_frame_unref(frame);
        frame = NULL;
    }
    ok = 1;
end:
    if (!ok) {
        fprintf(stderr, "AUTO mode=%d space=%d: %s; %s\n", mode, space,
            sixel_helper_get_additional_message(), capture.data);
    }
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
