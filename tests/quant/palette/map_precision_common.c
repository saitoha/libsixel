/* SPDX-License-Identifier: MIT */
/* Map precision is independent of automatic source precision; the 8bit
 * precision policy rounds the managed map to bytes before application. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"
#include "src/dither.h"
#include "src/palette.h"

double const *test_map_p3_primary(int space);

typedef struct map_precision_capture {
    char data[8192];
    size_t size;
} map_precision_capture_t;

static int
map_precision_write(char *data, int size, void *context)
{
    map_precision_capture_t *capture;

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
test_map_precision_run(int mode, int space, char const *precision,
                   char const *quantizer, char const *loader,
                   char const *path, int animated)
{
    static struct { int flag; char const *value; } const options[] = {
        { '=', "1" }, { 'X', "linear" }, { 'F', "none" },
        { '_', "none" }, { '~', "5bit" }, { 't', "rgb" },
        { '4', "full-frame" }, { '5', "none" }, { '0', "hard" },
        { 'd', "none" }, { 'U', "gamma" }
    };
    static char const *const spaces[] = {
        "gamma", "linear", "oklab", "cielab", "din99d"
    };
    static char const managed[] =
        "builtin:cms_engine=builtin:cms_target=linear:prefer_8bit=0!";
    static char const unmanaged[] = "builtin:cms_engine=none!";
    unsigned char pixels[3] = { 255, 0, 0 };
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    sixel_dither_t *dither;
    sixel_palette_float32_entries_view_t view;
    map_precision_capture_t capture;
    unsigned char *decoded;
    unsigned char *palette;
    double const *expected;
    int width;
    int height;
    int colors;
    int native;
    int byte_policy;
    int passes;
    int pass;
    int i;
    int j;
    int selected;
    int matches;
    int requested_float;
    int requested_model;
    int ok;

    encoder = NULL;
    output = NULL;
    frame = NULL;
    decoded = NULL;
    palette = NULL;
    ok = 0;
    byte_policy = strcmp(precision, "8bit") == 0;
    expected = test_map_p3_primary(space);
    if (expected == NULL ||
            SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, map_precision_write,
                &capture, NULL))) {
        goto end;
    }
    for (i = 0; i < (int)(sizeof(options) / sizeof(options[0])); ++i) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder,
                options[i].flag, options[i].value))) {
            goto end;
        }
    }
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder, '.', precision)) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'Q', quantizer)) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'W', spaces[space])) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'L', loader)) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'm', path)) ||
            (mode == 4 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                'M', "gpl:-"))) ||
            (mode == 5 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                'U', "display-p3")))) {
        goto end;
    }
    requested_float = encoder->prefer_float32;
    requested_model = encoder->quantize_model;
    /* Isolate the map loader's precision requirement. Non-gamma clustering
     * independently requires native palette construction, even without CMS. */
    if ((mode == 1 || mode == 2 || mode == 3) &&
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'X', "gamma"))) {
        goto end;
    }
    passes = mode == 2 || mode == 3 ? 3 : mode == 8 ? 2 : 1;
    if (animated) {
        passes = 3;
        encoder->fstatic = 1;
        encoder->fignore_delay = 1;
    }
    for (pass = 0; pass < passes; ++pass) {
        native = mode != 1 && !byte_policy;
        if (mode == 9) {
            space = pass == 1 ? 4 : 0;
            expected = test_map_p3_primary(space);
            if (SIXEL_FAILED(sixel_encoder_setopt(encoder,
                    'W', spaces[space]))) {
                goto end;
            }
        }
        if (mode == 2 || mode == 3) {
            native = mode == 2 ? pass == 1 : pass != 1;
            /* Keep the same map path and encoder. Only loader work changes. */
            if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'L',
                    native ? managed : unmanaged))) {
                goto end;
            }
        }
        memset(&capture, 0, sizeof(capture));
        if (SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
                SIXEL_FAILED(sixel_frame_init_borrowed(frame, pixels,
                    1, 1, SIXEL_PIXELFORMAT_RGB888, NULL, -1))) {
            goto end;
        }
        if (animated || mode == 6 || (mode == 8 && pass == 1)) {
            sixel_frame_set_multiframe(frame, 1);
            sixel_frame_set_frame_no(frame, pass % 2);
            sixel_frame_set_loop_count(frame, pass / 2);
        }
        if (SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame,
                output)) || (mode != 9 && encoder->prefer_float32 != requested_float) ||
                encoder->quantize_model != requested_model) {
            goto end;
        }
        dither = (sixel_dither_t *)encoder->diagnostic_dither;
        if (dither == NULL || dither->use_rgb101_palette != native) {
            goto end;
        }
        /* A managed map promotes application, not source loading/planning. */
        if (!requested_float && space == 0 && mode != 5 &&
                encoder->planner.working_pixelformat !=
                    SIXEL_PIXELFORMAT_RGB888) {
            goto end;
        }
        memset(&view, 0, sizeof(view));
        if (SIXEL_FAILED(dither->palette->vtbl->get_entries_float32(
                dither->palette, &view))) {
            goto end;
        }
        /* 8bit applies the byte view only; no float coordinates remain. */
        if (byte_policy && view.entries != NULL) {
            goto end;
        }
        selected = -1;
        if (native) {
            if (view.entries == NULL || view.entry_count != 4) {
                goto end;
            }
            matches = 0;
            for (i = 0; i < 4; ++i) {
                for (j = 0; j < 3; ++j) {
                    /* Ideal primary vs quantized ICC: existing 3e-4 budget
                     * includes profile coefficients and float transforms. */
                    if (!isfinite(view.entries[3 * i + j]) ||
                            fabs(view.entries[3 * i + j] - expected[j])
                                > 3e-4) {
                        break;
                    }
                }
                if (j == 3) {
                    selected = i;
                    ++matches;
                }
            }
            if (matches != 1) {
                goto end;
            }
        }
        if (SIXEL_FAILED(sixel_decode_raw((unsigned char *)capture.data,
                (int)capture.size, &decoded, &width, &height,
                &palette, &colors, NULL)) || width != 1 || height != 1) {
            goto end;
        }
        /* sRGB red is strictly closest to this P3 primary among the four
         * fixture colors in all five metrics; byte narrowing cannot retain
         * the primary coordinates above. Wire clipping is a later boundary. */
        if (native && (selected < 0 || selected >= colors ||
                decoded[0] != selected || palette[selected * 3] != 255 ||
                palette[selected * 3 + 1] != 0 ||
                palette[selected * 3 + 2] != 0)) {
            goto end;
        }
        if (mode == 4 && (encoder->capture_ncolors != 4 ||
                encoder->capture_palette_size != 12 ||
                encoder->capture_palette[selected * 3] != 255 ||
                encoder->capture_palette[selected * 3 + 1] != 0 ||
                encoder->capture_palette[selected * 3 + 2] != 0)) {
            goto end;
        }
        free(decoded);
        free(palette);
        decoded = NULL;
        palette = NULL;
        sixel_frame_unref(frame);
        frame = NULL;
    }
    ok = 1;
end:
    if (!ok) {
        fprintf(stderr, "map precision mode=%d space=%d: %s\n",
            mode, space, sixel_helper_get_additional_message());
    }
    free(decoded);
    free(palette);
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* Fail only the two-pixel RGB float work buffer, after setup is complete. */
static int map_precision_fail;
static int map_precision_failures;
static void *map_precision_index;
static int map_precision_index_frees;

static void *
map_precision_malloc(size_t size)
{
    void *result;

    if (map_precision_fail && size == 6 * sizeof(float)) {
        ++map_precision_failures;
        return NULL;
    }
    result = malloc(size);
    if (map_precision_fail && size == 2 * sizeof(sixel_index_t)) {
        map_precision_index = result;
    }
    return result;
}

static void
map_precision_free(void *pointer)
{
    if (pointer != NULL && pointer == map_precision_index) {
        ++map_precision_index_frees;
        map_precision_index = NULL;
    }
    free(pointer);
}

int
test_map_promotion(int fail, int diffuse)
{
    unsigned char pixels[6] = { 92, 92, 92, 91, 91, 91 };
    unsigned char entries[6] = { 88, 88, 88, 94, 94, 94 };
    float floats[6];
    sixel_allocator_t *allocator;
    sixel_dither_t *dither;
    sixel_index_t *indices;
    sixel_index_t borrowed[2] = { 123, 124 };
    sixel_palette_float32_entries_request_t request;
    int i;
    int ok;

    allocator = NULL;
    dither = NULL;
    indices = NULL;
    ok = 0;
    map_precision_fail = 0;
    map_precision_failures = 0;
    map_precision_index = NULL;
    map_precision_index_frees = 0;
    memset(&request, 0, sizeof(request));
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, map_precision_malloc,
            NULL, NULL, map_precision_free)) ||
            SIXEL_FAILED(sixel_dither_new(&dither, 2, allocator))) {
        goto end;
    }
    for (i = 0; i < 6; ++i) {
        floats[i] = (float)entries[i] / 255.0f;
    }
    sixel_dither_set_palette(dither, entries);
    request.entries = floats;
    request.colors = 2;
    request.depth = 3 * sizeof(float);
    if (SIXEL_FAILED(dither->palette->vtbl->init_entries_float32(
            dither->palette, &request))) {
        goto end;
    }
    dither->use_rgb101_palette = 1;
    dither->clamp_policy = SIXEL_DITHER_CLAMP_HARD;
    sixel_dither_set_pixelformat(dither, SIXEL_PIXELFORMAT_RGB888);
    sixel_dither_set_diffusion_type(dither,
        diffuse ? SIXEL_DIFFUSE_FS : SIXEL_DIFFUSE_NONE);
    sixel_dither_set_diffusion_scan(dither, SIXEL_SCAN_RASTER);
    sixel_dither_set_lut_policy(dither, SIXEL_LUT_POLICY_NONE);
    sixel_dither_set_optimize_palette(dither, 0);
    if (fail == 2) {
        dither->pipeline_index_buffer = borrowed;
        dither->pipeline_index_size = sizeof(borrowed);
        dither->pipeline_index_owned = 0;
    }
    map_precision_fail = fail;
    indices = sixel_dither_apply_palette(dither, pixels, 2, 1);
    if (fail) {
        ok = indices == NULL && map_precision_failures == 1 &&
            map_precision_index_frees == (fail == 2 ? 0 : 1) &&
            borrowed[0] == 123 && borrowed[1] == 124;
    } else {
        /* In byte units: 92 selects 94; FS adds (92-94)*7/16 to
         * the next 91, giving 90.125 < midpoint 91, so it selects 88.
         * The 0.875 margin is far larger than float rounding error. */
        ok = indices != NULL && indices[0] == 1 && indices[1] == 0;
    }
end:
    map_precision_fail = 0;
    if (!ok) {
        fprintf(stderr, "map promotion: fail=%d calls=%d result=%d\n",
            fail, map_precision_failures, indices != NULL);
    }
    if (indices != NULL && indices != borrowed) {
        sixel_allocator_free(allocator, indices);
    }
    sixel_dither_unref(dither);
    sixel_allocator_unref(allocator);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
test_map_precision(int mode, int space, char const *precision,
                   char const *quantizer, char const *loader,
                   char const *path)
{
    return test_map_precision_run(mode, space, precision, quantizer, loader,
        path, 0);
}

int
test_map_animation(int mode, int space, char const *path)
{
    return test_map_precision_run(mode, space, "auto", "kmeans:seed=1",
        "builtin:cms_engine=builtin:cms_target=linear:prefer_8bit=0!",
        path, 1);
}
