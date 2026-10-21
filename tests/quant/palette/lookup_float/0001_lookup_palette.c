/* SPDX-License-Identifier: MIT */
/* Selected float palette dispatch, wire cache and worker lifecycle. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"
#include "src/dither.h"
#include "src/palette.h"

static char const *const lookup_options[] = {
    "certlut", "5bit", "6bit", "auto", "default", "vptree", "rbc",
    "mahalanobis", "eytzinger", "fhedt", "delaunay"
};
static char const *const lookup_spaces[] = {
    "gamma", "linear", "oklab", "cielab", "din99d"
};
static int const lookup_formats[] = {
    SIXEL_PIXELFORMAT_RGBFLOAT32, SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
    SIXEL_PIXELFORMAT_OKLABFLOAT32, SIXEL_PIXELFORMAT_CIELABFLOAT32,
    SIXEL_PIXELFORMAT_DIN99DFLOAT32
};
static int const lookup_codes[] = {
    SIXEL_LUT_POLICY_CERTLUT, SIXEL_LUT_POLICY_5BIT, SIXEL_LUT_POLICY_6BIT,
    SIXEL_LUT_POLICY_AUTO, SIXEL_LUT_POLICY_CERTLUT,
    SIXEL_LUT_POLICY_VPTREE, SIXEL_LUT_POLICY_RBC,
    SIXEL_LUT_POLICY_MAHALANOBIS, SIXEL_LUT_POLICY_EYTZINGER,
    SIXEL_LUT_POLICY_FHEDT, SIXEL_LUT_POLICY_DELAUNAY
};

typedef struct lookup_capture {
    char data[2048];
    size_t size;
} lookup_capture_t;

static int
lookup_write(char *data, int size, void *context)
{
    lookup_capture_t *capture;

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
lookup_encode(int policy, int space, int p3, int byte,
              char const *solver)
{
    static struct { int flag; char const *value; } const options[] = {
        { '=', "1" }, { 'p', "2" }, { 'Q', "kmeans:seed=1" },
        { 'F', "none" }, { 'a', "off" }, { '_', "none" },
        { 'd', "none" }, { 't', "rgb" }, { '5', "none" },
        { '4', "full-frame" }, { 'X', "gamma" }
    };
    float pixels[6] = { -.25f, -.25f, -.25f, 1.25f, 1.25f, 1.25f };
    unsigned char bytes[6] = { 32, 32, 32, 224, 224, 224 };
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    sixel_dither_t *dither;
    sixel_palette_float32_entries_view_t view;
    lookup_capture_t capture;
    int n;
    int ok;

    encoder = NULL;
    output = NULL;
    frame = NULL;
    ok = 0;
    memset(&view, 0, sizeof(view));
    memset(&capture, 0, sizeof(capture));
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, lookup_write,
                &capture, NULL))) {
        goto end;
    }
    for (n = 0; n < (int)(sizeof(options) / sizeof(options[0])); ++n) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, options[n].flag,
                options[n].value))) {
            goto end;
        }
    }
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'Q', solver)) ||
            (policy != 4 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                '~', lookup_options[policy]))) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '.',
                byte ? "8bit" : "auto")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'W',
                lookup_spaces[space])) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U',
                p3 ? "display-p3" : "gamma")) ||
            SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
            SIXEL_FAILED(sixel_frame_init_borrowed(frame,
                byte ? (void *)bytes : (void *)pixels, 2, 1,
                byte ? SIXEL_PIXELFORMAT_RGB888
                     : SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, -1)) ||
            SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame,
                output))) {
        goto end;
    }
    dither = (sixel_dither_t *)encoder->diagnostic_dither;
    if (dither == NULL) {
        goto end;
    }
    if (byte) {
        ok = dither->use_rgb101_palette == 0 &&
            dither->pixelformat == SIXEL_PIXELFORMAT_RGB888 &&
            strstr(capture.data, ";2;13;13;13") != NULL &&
            strstr(capture.data, ";2;88;88;88") != NULL;
        goto end;
    }
    /* Neutral endpoints serialize as black and white in both output
     * definitions. They are NOT a canonical float black/white palette. */
    ok = dither->use_rgb101_palette != 0 &&
        dither->pixelformat == lookup_formats[space] &&
        strstr(capture.data, ";2;0;0;0") != NULL &&
        strstr(capture.data, ";2;100;100;100") != NULL &&
        SIXEL_SUCCEEDED(dither->palette->vtbl->get_entries_float32(
            dither->palette, &view)) && view.entries != NULL &&
        view.entry_count == 2;
    if (ok && space == 0) {
        /* Exact input endpoints require no gamma transform. A small
         * absolute budget covers centroid stores, not byte projection. */
        ok = isfinite(view.entries[0]) && isfinite(view.entries[3]) &&
            ((fabs(view.entries[0] + .25) < 2e-6 &&
              fabs(view.entries[3] - 1.25) < 2e-6) ||
             (fabs(view.entries[3] + .25) < 2e-6 &&
              fabs(view.entries[0] - 1.25) < 2e-6));
    }
end:
    if (!ok) {
        fprintf(stderr, "lookup=%d W=%d p3=%d byte=%d: %s; %s\n",
            policy, space, p3, byte, capture.data,
            sixel_helper_get_additional_message());
    }
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok;
}

static int
lookup_setup(sixel_dither_t *dither, int policy, int shared)
{
    unsigned char bytes[9] = { 0, 0, 0, 128, 128, 128, 255, 255, 255 };
    float values[9] = { -.25f, -.25f, -.25f, .5f, .5f, .5f,
                       1.25f, 1.25f, 1.25f };
    sixel_palette_float32_entries_request_t request;

    sixel_dither_set_palette(dither, bytes);
    sixel_dither_set_diffusion_type(dither, SIXEL_DIFFUSE_FS);
    sixel_dither_set_lut_policy(dither, lookup_codes[policy]);
    sixel_dither_set_pixelformat(dither, SIXEL_PIXELFORMAT_RGBFLOAT32);
    dither->use_rgb101_palette = 1;
    dither->prefer_float32 = 1;
    /* Exercise implicit certlut with supplied entries (optimized=0), too.
     * Its class must not fall back to NONE just because no CLI override
     * enabled the old byte cache. */
    if (policy == 0) {
        dither->lut_policy_override = 0;
    }
    dither->lut_policy_shared_instance_override = 1;
    dither->lut_policy_shared_instance = shared;
    memset(&request, 0, sizeof(request));
    request.entries = values;
    request.colors = 3;
    request.depth = 3 * sizeof(float);
    return SIXEL_SUCCEEDED(dither->palette->vtbl->init_entries_float32(
        dither->palette, &request)) &&
        SIXEL_SUCCEEDED(sixel_palette_prepare_rgb101(dither->palette,
            SIXEL_COLORSPACE_GAMMA, SIXEL_PALETTE_OUTPUT_LEGACY)) &&
        SIXEL_SUCCEEDED(sixel_dither_set_clamp_policy(dither,
            SIXEL_DITHER_CLAMP_SOFT, 1.01));
}

static int
lookup_bands(int policy, int shared)
{
    sixel_dither_t *parallel;
    sixel_dither_t *serial;
    sixel_index_t *actual;
    sixel_index_t *band;
    sixel_palette_float32_entries_view_t view;
    unsigned char const *wire;
    unsigned int count;
    unsigned char const expected[9] = { 0, 0, 0, 50, 50, 50, 100, 100, 100 };
    float input[8 * 18 * 3];
    float working[8 * 18 * 3];
    int n;
    int y;
    int start;
    int rows;
    int ok;

    parallel = NULL;
    serial = NULL;
    actual = NULL;
    band = NULL;
    ok = 0;
    if (SIXEL_FAILED(sixel_dither_new(&parallel, 3, NULL)) ||
            SIXEL_FAILED(sixel_dither_new(&serial, 3, NULL)) ||
            !lookup_setup(parallel, policy, shared) ||
            !lookup_setup(serial, policy, 0)) {
        goto end;
    }
    parallel->pipeline_parallel_active = 1;
    parallel->pipeline_dither_threads = 2;
    parallel->pipeline_band_height = 6;
    parallel->pipeline_band_overlap = 2;
    for (n = 0; n < 8 * 18 * 3; ++n) {
        input[n] = (float)((n * 17 + 13) % 193 - 32) / 128;
    }
    memcpy(working, input, sizeof(input));
    actual = sixel_dither_apply_palette(parallel,
        (unsigned char *)working, 8, 18);
    if (actual == NULL) {
        fprintf(stderr, "parallel apply failed: %s\n",
            sixel_helper_get_additional_message());
        goto end;
    }
    /* Compare equal band geometries: whole-image FS has a different
     * error history and would not be a valid oracle for this contract.
     * The production geometry uses six-row bands; an even overlap also
     * preserves global serpentine parity in each isolated reference. */
    for (y = 0; y < 18; y += 6) {
        start = y == 0 ? 0 : y - 2;
        rows = y + 6 - start;
        memcpy(working, input + start * 8 * 3,
            (size_t)rows * 8 * 3 * sizeof(float));
        band = sixel_dither_apply_palette(serial,
            (unsigned char *)working, 8, rows);
        if (band == NULL || memcmp(actual + y * 8,
                band + (y - start) * 8, 48 * sizeof(*band)) != 0) {
            fprintf(stderr, "serial band y=%d start=%d rows=%d null=%d: "
                "%s\n", y, start, rows, band == NULL,
                sixel_helper_get_additional_message());
            if (band != NULL) {
                for (n = 0; n < 48; ++n) {
                    fprintf(stderr, "%d:%u/%u ", n,
                        (unsigned)actual[y * 8 + n],
                        (unsigned)band[(y - start) * 8 + n]);
                }
                fputc('\n', stderr);
            }
            goto end;
        }
        sixel_allocator_free(serial->allocator, band);
        band = NULL;
    }
    memset(&view, 0, sizeof(view));
    wire = NULL;
    count = 0;
    ok = SIXEL_SUCCEEDED(sixel_palette_get_rgb101(parallel->palette,
            &wire, &count)) && count == 3 &&
        memcmp(wire, expected, sizeof(expected)) == 0 &&
        SIXEL_SUCCEEDED(parallel->palette->vtbl->get_entries_float32(
            parallel->palette, &view)) && view.entries != NULL &&
        view.entry_count == 3 && view.entries[0] == -.25f &&
        view.entries[6] == 1.25f;
    if (!ok) {
        fprintf(stderr, "bands %d shared=%d: count=%u, entries=%u, "
            "endpoints=%g,%g; %s\n", policy, shared, count,
            view.entry_count, view.entries ? (double)view.entries[0] : 0,
            view.entries ? (double)view.entries[6] : 0,
            sixel_helper_get_additional_message());
        if (wire != NULL) {
            for (n = 0; n < 9; ++n) {
                fprintf(stderr, "%u ", (unsigned)wire[n]);
            }
            fputc('\n', stderr);
        }
    }
end:
    if (parallel != NULL) {
        sixel_allocator_free(parallel->allocator, actual);
    }
    if (serial != NULL) {
        sixel_allocator_free(serial->allocator, band);
    }
    sixel_dither_unref(parallel);
    sixel_dither_unref(serial);
    return ok;
}

int
test_lookup_palette(int argc, char **argv)
{
    int policy;
    int space;
    int p3;
    int ok;

    if (argc != 5 && argc != 6) {
        return EXIT_FAILURE;
    }
    policy = atoi(argv[2]);
    space = atoi(argv[3]);
    p3 = atoi(argv[4]);
    if (policy < 0 || policy > 10 || space < 0 || space > 4 ||
            p3 < 0 || p3 > 1) {
        return EXIT_FAILURE;
    }
    ok = strcmp(argv[1], "bands") == 0
        ? (policy < 3 || policy >= 5) && lookup_bands(policy, p3)
        : lookup_encode(policy, space, p3, strcmp(argv[1], "byte") == 0,
                        argc == 6 ? argv[5] : "kmeans:seed=1");
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
