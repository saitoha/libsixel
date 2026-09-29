/* SPDX-License-Identifier: MIT */
/* Keep signed Ward means and native float palettes across final merge. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/palette.h"
#include "src/palette-kcenter.h"
#include "src/palette-kmeans.h"
#include "src/palette-common-merge.h"
#include "src/palette-kmedoids.h"
#include "src/encoder.h"
#include "src/dither.h"

static int const ward_formats[] = {
    SIXEL_PIXELFORMAT_RGBFLOAT32, SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
    SIXEL_PIXELFORMAT_OKLABFLOAT32, SIXEL_PIXELFORMAT_CIELABFLOAT32,
    SIXEL_PIXELFORMAT_DIN99DFLOAT32
};
static char const *const ward_spaces[] = {
    "gamma", "linear", "oklab", "cielab", "din99d"
};
static int const ward_models[] = {
    SIXEL_QUANTIZE_MODEL_KMEANS, SIXEL_QUANTIZE_MODEL_KCENTER,
    SIXEL_QUANTIZE_MODEL_KMEDOIDS
};
static char const *const ward_quantizers[] = {
    "kmeans:seed=1", "center:seed=1", "medoids:seed=1"
};
static int ward_fail;
static int ward_fail_cluster;

static void *
ward_malloc(size_t size)
{
    if (ward_fail_cluster && size == 3u * 4u * sizeof(double)) {
        ward_fail_cluster = 0;
        return NULL;
    }
    if (ward_fail) {
        ward_fail = 0;
        return NULL;
    }
    return malloc(size);
}

static SIXELSTATUS
ward_build(sixel_palette_t *palette, float *samples, unsigned int count,
           int model, int space, int legacy)
{
    sixel_palette_generate_request_t request;
    sixel_palette_build_attempt_t attempt;
    SIXELSTATUS status;
    SIXELSTATUS finish;

    memset(&request, 0, sizeof(request));
    memset(&attempt, 0, sizeof(attempt));
    request.data = samples;
    request.length = count * 3u * sizeof(float);
    request.pixelformat = ward_formats[space];
    request.requested_colors = 2;
    request.prefer_float32 = 1;
    request.quantize_model = ward_models[model];
    request.final_merge_mode = SIXEL_FINAL_MERGE_WARD;
    request.lut_policy = SIXEL_LUT_POLICY_NONE;
    sixel_palette_binning_state_init(&attempt.binning,
        SIXEL_PALETTE_BINNING_NONE, SIXEL_PALETTE_POLICY_ORIGIN_EXPLICIT);
    if (model == 1) {
        status = sixel_palette_binning_resolve(&attempt.binning,
            SIXEL_PALETTE_BINNING_NONE, 0, SIXEL_PALETTE_BINNING_GRID_NONE,
            SIXEL_PALETTE_BINNING_KERNEL_NONE,
            SIXEL_PALETTE_BINNING_BACKEND_DIRECT, count,
            SIXEL_PALETTE_RESOLUTION_QUANTIZER_CAPABILITY);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }
    status = sixel_palette_build_attempt_begin(palette, &attempt);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    status = legacy ? palette->vtbl->generate(palette, &request)
        : sixel_palette_generate_extended(palette, &request);
    finish = sixel_palette_build_attempt_finish(palette, &attempt);
    return SIXEL_FAILED(status) ? status : finish;
}

static int
ward_values(sixel_palette_t *palette, int model, int space, int metric)
{
    float const values[6] = { -2, -1, -1, -1, 3, 3 };
    float const metric_values[9] = {
        -4, 0, .125f, 0, 0, .125f, 0, 1, .125f
    };
    float samples[18];
    float original[18];
    double expected[6];
    sixel_palette_float32_entries_view_t view;
    SIXELSTATUS status;
    unsigned int axis;
    unsigned int row;
    unsigned int channel;
    unsigned int other;
    int ok;

    /* Three provisional clusters have populations 1,3,2. Ward costs on
     * the varying axis are 3/4, 96/5, 50/3. Thus the first two merge to
     * -5/4, retaining the final center 3. Lloyd leaves this split stable.
     * Repeat on all axes to expose clipping of signed opponent channels. */
    for (axis = 0; axis < (metric ? 1u : 3u); ++axis) {
        for (row = 0; row < 6; ++row) {
            for (channel = 0; channel < 3; ++channel) {
                samples[row * 3 + channel] = channel == axis
                    ? values[row] : .125f;
            }
        }
        for (channel = 0; channel < 3; ++channel) {
            expected[channel] = channel == axis ? -1.25 : .125;
            expected[3 + channel] = channel == axis ? 3 : .125;
        }
        if (metric) {
            /* Gamma has equal nominal channel scales. Native distances
             * merge (0,0) with (0,1), cost 1/2. Using an expanded [-4,1]
             * x-address range instead would merge (-4,0) with (0,0). */
            memcpy(samples, metric_values, sizeof(metric_values));
            expected[0] = -4;
            expected[1] = 0;
            expected[2] = .125;
            expected[3] = 0;
            expected[4] = .5;
            expected[5] = .125;
        }
        memcpy(original, samples, sizeof(samples));
        status = ward_build(palette, samples, metric ? 3u : 6u,
            model, space, 0);
        memset(&view, 0, sizeof(view));
        if (SIXEL_FAILED(status) ||
                SIXEL_FAILED(palette->vtbl->get_entries_float32(palette,
                    &view)) || view.entries == NULL ||
                view.entry_count != 2 ||
                memcmp(original, samples, sizeof(samples)) != 0) {
            fprintf(stderr, "axis %u status %d entries %u\n",
                axis, status, view.entry_count);
            return 0;
        }
        ok = 0;
        for (row = 0; row < 2; ++row) {
            other = 1u - row;
            for (channel = 0; channel < 3; ++channel) {
                /* Affine double conversions and one float store have a
                 * small absolute budget rather than bit identity. */
                if (!isfinite(view.entries[row * 3 + channel]) ||
                        !isfinite(view.entries[other * 3 + channel]) ||
                        fabs(view.entries[row * 3 + channel] -
                            expected[channel]) > 2e-6 ||
                        fabs(view.entries[other * 3 + channel] -
                            expected[3 + channel]) > 2e-6) {
                    break;
                }
            }
            ok |= channel == 3;
        }
        if (!ok) {
            fprintf(stderr, "axis %u actual %.9g %.9g %.9g / "
                "%.9g %.9g %.9g\n", axis,
                (double)view.entries[0], (double)view.entries[1],
                (double)view.entries[2], (double)view.entries[3],
                (double)view.entries[4], (double)view.entries[5]);
            return 0;
        }
    }
    return 1;
}

static int
ward_common(sixel_allocator_t *allocator, int mode)
{
    unsigned long weights[3] = { 1, 3, 2 };
    double sums[9] = { -510, 32, 765, -765, 96, 2295, 1530, 64, 1530 };
    double original[9];
    int count;
    int ok;

    memcpy(original, sums, sizeof(sums));
    ward_fail = mode == 2;
    count = sixel_palette_apply_merge(weights, sums, 3, 3, 2,
        SIXEL_FINAL_MERGE_WARD, 0, SIXEL_PIXELFORMAT_RGBFLOAT32,
        mode != 1, allocator);
    if (mode == 2) {
        return count < 0 && !ward_fail &&
            memcmp(original, sums, sizeof(sums)) == 0 &&
            weights[0] == 1 && weights[1] == 3 && weights[2] == 2;
    }
    /* The selected pair has sum (-1275,128,3060), weight 4; the third
     * remains (1530,64,1530), weight 2. Legacy clamps means to [0,255]. */
    ok = count == 2 && weights[0] == 4 && weights[1] == 2 &&
        weights[2] == 0 && sums[1] == 128 && sums[4] == 64;
    if (mode == 1) {
        return ok && sums[0] == 0 && sums[2] == 1020 &&
            sums[3] == 510 && sums[5] == 510;
    }
    return ok && sums[0] == -1275 && sums[2] == 3060 &&
        sums[3] == 1530 && sums[5] == 1530;
}

static int
ward_empty(sixel_palette_t *palette)
{
    float samples[18];
    sixel_palette_float32_entries_view_t view;
    SIXELSTATUS status;
    unsigned int row;

    /* Request two clusters from one repeated negative color. Assignments
     * leave a center empty; moving a source point to it must subtract a
     * negative contribution without zeroing the remaining signed sum. */
    for (row = 0; row < 6; ++row) {
        samples[row * 3] = -2;
        samples[row * 3 + 1] = .125f;
        samples[row * 3 + 2] = 1.25f;
    }
    status = ward_build(palette, samples, 6, 0, 0, 0);
    memset(&view, 0, sizeof(view));
    if (SIXEL_FAILED(status) ||
            SIXEL_FAILED(palette->vtbl->get_entries_float32(palette,
                &view)) || view.entries == NULL || view.entry_count != 2) {
        return 0;
    }
    for (row = 0; row < 2; ++row) {
        if (view.entries[row * 3] != -2 ||
                view.entries[row * 3 + 1] != .125f ||
                view.entries[row * 3 + 2] != 1.25f) {
            fprintf(stderr, "empty repair row %u: %.9g %.9g %.9g\n", row,
                (double)view.entries[row * 3],
                (double)view.entries[row * 3 + 1],
                (double)view.entries[row * 3 + 2]);
            return 0;
        }
    }
    return 1;
}

static int
ward_unique(sixel_palette_t *palette)
{
    float samples[18] = {
        -0.0f, .125f, 1.25f, 0.0f, .125f, 1.25f,
        -2, .125f, 1.25f, -0.0f, .125f, 1.25f,
        -2, .125f, 1.25f, -2, .125f, 1.25f
    };
    sixel_palette_float32_entries_view_t view;
    SIXELSTATUS status;
    int ok;

    /* Equal signed zeros must hash together. Both remaining native colors
     * fit K=2, so allocating three provisional merge clusters is an error. */
    ward_fail_cluster = 1;
    status = ward_build(palette, samples, 6, 0, 0, 0);
    memset(&view, 0, sizeof(view));
    ok = SIXEL_SUCCEEDED(status) && ward_fail_cluster &&
        SIXEL_SUCCEEDED(palette->vtbl->get_entries_float32(palette, &view))
        && view.entry_count == 2 && view.entries != NULL;
    ward_fail_cluster = 0;
    if (!ok) {
        return 0;
    }
    return ((view.entries[0] == 0 && view.entries[3] == -2) ||
            (view.entries[0] == -2 && view.entries[3] == 0)) &&
        view.entries[1] == .125f && view.entries[2] == 1.25f &&
        view.entries[4] == .125f && view.entries[5] == 1.25f;
}

static int
ward_failure(sixel_palette_t *palette, int model)
{
    float samples[18] = {
        -2, .125f, .125f, -1, .125f, .125f, -1, .125f, .125f,
        -1, .125f, .125f, 3, .125f, .125f, 3, .125f, .125f
    };
    SIXELSTATUS status;

    ward_fail_cluster = 1;
    status = ward_build(palette, samples, 6, model, 0, 0);
    if (status != SIXEL_BAD_ALLOCATION || ward_fail_cluster ||
            strcmp(sixel_helper_get_additional_message(),
                "final palette merge: cannot allocate cluster workspace.")
                != 0) {
        fprintf(stderr, "merge failure status=%d pending=%d message=%s\n",
            status, ward_fail_cluster, sixel_helper_get_additional_message());
        return 0;
    }
    /* A failed selected build must clear its attempt context and permit
     * the next valid build, rather than silently retrying a byte solver. */
    return ward_values(palette, model, 0, 0);
}

typedef struct ward_capture {
    char data[4096];
    size_t size;
} ward_capture_t;

static int
ward_write(char *data, int size, void *context)
{
    ward_capture_t *capture;

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
ward_wire(int model, int x, int w, int p3, char const *mode)
{
    static struct { int flag; char const *value; } const options[] = {
        { 'p', "2" }, { 'F', "ward:O1.5:L0" }, { 'a', "off" },
        { '_', "none" }, { '~', "none" }, { 'd', "none" },
        { 't', "rgb" }, { '5', "auto" }
    };
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    sixel_dither_t *diagnostic;
    ward_capture_t capture;
    float pixels[18];
    unsigned char bytes[9] = { 0, 0, 0, 64, 64, 64, 255, 255, 255 };
    int worker;
    int byte;
    int ok;
    int n;

    encoder = NULL;
    output = NULL;
    frame = NULL;
    ok = 0;
    worker = strcmp(mode, "worker") == 0;
    byte = strcmp(mode, "byte") == 0;
    memset(&capture, 0, sizeof(capture));
    for (n = 0; n < 18; ++n) {
        pixels[n] = n < 6 ? -.5f : n < 12 ? -.25f : 1.25f;
    }
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, ward_write,
                &capture, NULL))) {
        goto end;
    }
    for (n = 0; n < (int)(sizeof(options) / sizeof(options[0])); ++n) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, options[n].flag,
                options[n].value))) {
            goto end;
        }
    }
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'Q',
                ward_quantizers[model])) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '=',
                worker ? "4" : "1")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '.',
                byte ? "8bit" : "auto")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '4',
                worker ? "adaptive-grid" : "full-frame")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'X',
                ward_spaces[x])) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'W',
                ward_spaces[w])) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U',
                p3 ? "display-p3" : "gamma")) ||
            SIXEL_FAILED(sixel_frame_new(&frame, NULL))) {
        goto end;
    }
    if (byte) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'p', "2")) ||
                SIXEL_FAILED(sixel_frame_init_borrowed(frame, bytes, 3, 1,
                    SIXEL_PIXELFORMAT_RGB888, NULL, -1))) {
            goto end;
        }
    } else if (SIXEL_FAILED(sixel_frame_init_borrowed(frame, pixels, 3, 2,
            SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, NULL, -1))) {
        goto end;
    }
    if (SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame, output))) {
        goto end;
    }
    diagnostic = (sixel_dither_t *)encoder->diagnostic_dither;
    if (diagnostic == NULL) {
        goto end;
    }
    /* The two negative neutral centers merge; only serialization clips
     * their weighted mean to black. The last center remains above one.
     * D65 neutral endpoints are shared by sRGB and Display P3. */
    ok = strstr(capture.data, ";2;100;100;100")
            != NULL && strstr(capture.data, ";2;0;0;0") != NULL &&
        diagnostic->use_rgb101_palette != 0 &&
        diagnostic->pixelformat == ward_formats[w];
    if (byte) {
        ok = diagnostic->use_rgb101_palette == 0 &&
            diagnostic->pixelformat == SIXEL_PIXELFORMAT_RGB888 &&
            strstr(capture.data, ";2;13;13;13") != NULL &&
            strstr(capture.data, ";2;100;100;100") != NULL;
    }
end:
    if (!ok) {
        fprintf(stderr, "%s model=%d X=%d W=%d p3=%d: %s; %s\n",
            mode, model, x, w, p3, capture.data,
            sixel_helper_get_additional_message());
    }
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok;
}

int
test_palette_merge_float(int argc, char **argv)
{
    sixel_allocator_t *allocator;
    sixel_palette_t *palette;
    void *object;
    int model;
    int space;
    int mode;
    int ok;

    if (argc != 5) {
        return EXIT_FAILURE;
    }
    model = atoi(argv[2]);
    space = atoi(argv[3]);
    mode = atoi(argv[4]);
    if (model < 0 || model > 2 || space < 0 || space > 4) {
        return EXIT_FAILURE;
    }
    if (strcmp(argv[1], "wire") == 0 ||
            strcmp(argv[1], "worker") == 0 ||
            strcmp(argv[1], "byte") == 0) {
        return ward_wire(model, space, space, mode, argv[1])
            ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    allocator = NULL;
    palette = NULL;
    object = NULL;
    ok = 0;
    sixel_set_final_merge_target_factor_override(1, 1.5);
    sixel_set_final_merge_lloyd_iterations_override(1, (unsigned int)mode);
    sixel_set_kmeans_seed_override(1, 1u);
    sixel_set_kcenter_seed_override(1, 1u);
    sixel_set_kmedoids_seed_override(1, 1u);
    if (SIXEL_SUCCEEDED(sixel_allocator_new(&allocator, ward_malloc,
            NULL, NULL, NULL)) &&
            SIXEL_SUCCEEDED(sixel_palette_factory_new(allocator, &object))) {
        palette = object;
        ok = strcmp(argv[1], "empty") == 0 ? ward_empty(palette)
            : strcmp(argv[1], "unique") == 0 ? ward_unique(palette)
            : strcmp(argv[1], "failure") == 0
            ? ward_failure(palette, model)
            : strcmp(argv[1], "common") == 0
            ? ward_common(allocator, mode)
            : ward_values(palette, model, space,
                strcmp(argv[1], "metric") == 0);
    }
    if (!ok) {
        fprintf(stderr, "%s model=%d space=%d mode=%d: %s\n", argv[1],
            model, space, mode, sixel_helper_get_additional_message());
    }
    if (palette != NULL) {
        palette->vtbl->unref(palette);
    }
    sixel_allocator_unref(allocator);
    sixel_set_final_merge_target_factor_override(0, 0);
    sixel_set_final_merge_lloyd_iterations_override(0, 0);
    sixel_set_kmeans_seed_override(0, 0);
    sixel_set_kcenter_seed_override(0, 0);
    sixel_set_kmedoids_seed_override(0, 0);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
