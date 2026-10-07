/* SPDX-License-Identifier: MIT */
/* Image-map clustering retains float means independently of source bytes. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"
#include "src/dither.h"
#include "src/palette.h"

typedef struct map_cluster_capture {
    char data[4096];
    size_t size;
} map_cluster_capture_t;

static int
map_cluster_write(char *data, int size, void *context)
{
    map_cluster_capture_t *capture;

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
test_map_cluster(int mode, char const *path)
{
    static struct { int flag; char const *value; } const options[] = {
        { '=', "1" }, { 'F', "none" }, { '_', "none" },
        { '~', "none" }, { 'd', "none" }, { 't', "rgb" },
        { '4', "full-frame" }, { '5', "none" }, { 'W', "gamma" },
        { 'L', "builtin:cms_engine=none!" }, { '0', "hard" }
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
    /* The closest RGB101 half-step is >2.7e-4 away (Lab blue), larger
     * than the propagated 8e-5 coordinate budget. */
    static char const *const wire[] = {
        ";2;74;0;74", ";2;55;33;64", ";2;79;0;54", ";2;56;31;47"
    };
    /* Distinct source colors prove that construction uses the map pixels. */
    unsigned char pixels[6] = { 24, 192, 80, 225, 40, 192 };
    unsigned char original[6];
    static unsigned char const structured[12] = {
        88, 88, 88, 94, 94, 94, 88, 88, 88, 255, 0, 0
    };
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    sixel_dither_t *dither;
    sixel_dither_t *cached;
    sixel_dither_t *fixed;
    sixel_palette_float32_entries_view_t view;
    sixel_palette_entries_view_t bytes;
    map_cluster_capture_t capture;
    char const *solver;
    int space;
    int pass;
    int count;
    int native;
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
    cached = NULL;
    fixed = NULL;
    memcpy(original, pixels, sizeof(pixels));
    memset(&capture, 0, sizeof(capture));
    memset(&view, 0, sizeof(view));
    solver = mode == 4 ? "heckbert" : mode == 5 ? "center"
        : mode == 6 ? "medoids" : mode == 7 ? "auto" : "kmeans:seed=1";
    count = mode == 5 || mode == 6 ? 2 : 1;
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, map_cluster_write,
                &capture, NULL))) {
        goto end;
    }
    for (i = 0; i < (int)(sizeof(options) / sizeof(options[0])); ++i) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, options[i].flag,
                options[i].value))) {
            goto end;
        }
    }
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'Q', solver)) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '.',
                mode == 8 ? "8bit" : "auto")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'm', path))) {
        goto end;
    }
    /* The CLI rejects -p with -m. Specify a small capacity at the internal
     * builder boundary to force reduction of two occupied map colors. */
    encoder->fstatic = 1;
    encoder->fignore_delay = 1;
    for (pass = 0; pass < (mode >= 11 ? 3 : 1); ++pass) {
        space = mode < 4 ? mode : mode == 9 ? 4 : 0;
        if (mode == 11) {
            space = pass == 1 ? 4 : pass == 2 ? 2 : 0;
        }
        native = space != 4 && mode != 10;
        memset(&capture, 0, sizeof(capture));
        memset(&view, 0, sizeof(view));
        encoder->reqcolors = -1;
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'X', spaces[space])) ||
                SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
                SIXEL_FAILED(sixel_frame_init_borrowed(frame, pixels, 2, 1,
                    SIXEL_PIXELFORMAT_RGB888, NULL, -1))) {
            goto end;
        }
        if (mode == 12) {
            sixel_frame_set_multiframe(frame, 1);
            sixel_frame_set_frame_no(frame, pass % 2);
            sixel_frame_set_loop_count(frame, pass / 2);
        }
        encoder->reqcolors = count;
        status = sixel_encoder_encode_frame(encoder, frame, output);
        dither = (sixel_dither_t *)encoder->diagnostic_dither;
        if (SIXEL_FAILED(status) || dither == NULL ||
                dither->use_rgb101_palette != native ||
                dither->pixelformat != SIXEL_PIXELFORMAT_RGB888 ||
                encoder->prefer_float32 != 0 ||
                encoder->planner.working_pixelformat !=
                    SIXEL_PIXELFORMAT_RGB888 ||
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
                if (strstr(capture.data, wire[space]) == NULL) {
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
        } else {
            memset(&bytes, 0, sizeof(bytes));
            if (view.entries != NULL ||
                    (mode == 10 &&
                     (SIXEL_FAILED(dither->palette->vtbl->get_entries(
                        dither->palette, &bytes)) ||
                      bytes.entries == NULL || bytes.entry_count != 4 ||
                      memcmp(bytes.entries, structured,
                          sizeof(structured)) != 0)) ||
                    (mode != 10 &&
                     strstr(capture.data, ";2;50;0;50") == NULL)) {
                goto end;
            }
        }
        if (mode == 12) {
            /* Both the immutable map source and its application survive
             * unchanged frames and loop replay; no per-frame reconstruction. */
            if (encoder->fixed_palette_source == NULL ||
                    encoder->dither_cache != dither ||
                    (pass != 0 && (cached != dither ||
                     fixed != encoder->fixed_palette_source))) {
                goto end;
            }
            cached = dither;
            fixed = encoder->fixed_palette_source;
        }
        sixel_frame_unref(frame);
        frame = NULL;
    }
    ok = 1;
end:
    if (!ok) {
        fprintf(stderr, "map clustering mode=%d: %s; wire=%s\n", mode,
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
