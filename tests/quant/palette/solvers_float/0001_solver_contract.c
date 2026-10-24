/* SPDX-License-Identifier: MIT */
/* Preserve observed representatives and native coordinates across solvers. */
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
#include "src/palette-kmedoids.h"
#include "src/encoder.h"
#include "src/dither.h"
#include "tests/data/color-numeric/p3-palette.h"

static int const solver_formats[] = {
    SIXEL_PIXELFORMAT_RGBFLOAT32, SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
    SIXEL_PIXELFORMAT_OKLABFLOAT32, SIXEL_PIXELFORMAT_CIELABFLOAT32,
    SIXEL_PIXELFORMAT_DIN99DFLOAT32
};
static char const *const solver_spaces[] = {
    "gamma", "linear", "oklab", "cielab", "din99d"
};
static int solver_fail_once;
static int solver_failure_hits;

static void *
solver_malloc(size_t size)
{
    if (solver_fail_once) {
        solver_fail_once = 0;
        ++solver_failure_hits;
        return NULL;
    }
    return malloc(size);
}

static SIXELSTATUS
solver_build(sixel_palette_t *palette, float *samples, unsigned int count,
             unsigned int colors, int model, int space, int bin, int extended)
{
    sixel_palette_generate_request_t request;
    sixel_palette_build_attempt_t attempt;
    SIXELSTATUS status;
    SIXELSTATUS finish;

    memset(&request, 0, sizeof(request));
    memset(&attempt, 0, sizeof(attempt));
    request.data = samples;
    request.length = count * 3u * sizeof(float);
    request.pixelformat = solver_formats[space];
    request.requested_colors = colors;
    request.prefer_float32 = 1;
    request.quantize_model = model ? SIXEL_QUANTIZE_MODEL_KMEDOIDS
        : SIXEL_QUANTIZE_MODEL_KCENTER;
    request.final_merge_mode = SIXEL_FINAL_MERGE_NONE;
    request.lut_policy = SIXEL_LUT_POLICY_NONE;
    sixel_palette_binning_state_init(&attempt.binning, bin,
        bin == SIXEL_PALETTE_BINNING_AUTO
            ? SIXEL_PALETTE_POLICY_ORIGIN_AUTO
            : SIXEL_PALETTE_POLICY_ORIGIN_EXPLICIT);
    /* K-center consumes the already-resolved artifact contract; the
     * encoder tests below cover the actual top-level auto resolver. */
    if (!model) {
        bin = bin == SIXEL_PALETTE_BINNING_AUTO
            ? SIXEL_PALETTE_BINNING_HARD : bin;
        status = sixel_palette_binning_resolve(&attempt.binning, bin,
            bin == SIXEL_PALETTE_BINNING_HARD ? 5u : 0u,
            bin == SIXEL_PALETTE_BINNING_HARD
                ? SIXEL_PALETTE_BINNING_GRID_UNIFORM_256
                : SIXEL_PALETTE_BINNING_GRID_NONE,
            SIXEL_PALETTE_BINNING_KERNEL_NONE,
            bin == SIXEL_PALETTE_BINNING_NONE
                ? SIXEL_PALETTE_BINNING_BACKEND_DIRECT
                : SIXEL_PALETTE_BINNING_BACKEND_COMPACT_SPARSE,
            count, SIXEL_PALETTE_RESOLUTION_QUANTIZER_CAPABILITY);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }
    status = sixel_palette_build_attempt_begin(palette, &attempt);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    status = extended ? sixel_palette_generate_extended(palette, &request)
        : palette->vtbl->generate(palette, &request);
    finish = sixel_palette_build_attempt_finish(palette, &attempt);
    return SIXEL_FAILED(status) ? status : finish;
}

static int
solver_values(sixel_palette_t *palette, int model, int space, int bin,
              int reduced)
{
    float const values[4] = { -4.0f, -2.0f, 2.0f, 4.0f };
    float samples[12];
    sixel_palette_float32_entries_view_t view;
    unsigned int axis;
    unsigned int row;
    unsigned int channel;
    unsigned int expected;
    unsigned int found;
    unsigned int colors;
    SIXELSTATUS status;
    int match;

    colors = reduced ? 2u : 4u;
    /* A one-axis excursion detects saturation on each opponent channel
     * too. The reduced case actually optimizes rather than exporting all
     * candidates. Every representative must remain an observed input. */
    for (axis = 0; axis < 3; ++axis) {
        for (row = 0; row < 4; ++row) {
            for (channel = 0; channel < 3; ++channel) {
                samples[row * 3 + channel] = channel == axis
                    ? values[row] : .125f;
            }
        }
        memset(&view, 0, sizeof(view));
        status = solver_build(palette, samples, 4, colors, model,
            space, bin, 1);
        if (SIXEL_FAILED(status) ||
                SIXEL_FAILED(palette->vtbl->get_entries_float32(palette,
                    &view)) || view.entries == NULL ||
                view.entry_count != colors) {
            fprintf(stderr, "axis %u status %d count %u expected %u\n",
                axis, status, view.entry_count, colors);
            return 0;
        }
        found = 0;
        for (row = 0; row < colors; ++row) {
            match = 0;
            for (expected = 0; expected < 4; ++expected) {
                for (channel = 0; channel < 3; ++channel) {
                    /* Allow scaled-double round trips through a few
                     * binary32 ULPs; clipping and byte projection fail. */
                    if (!isfinite(view.entries[row * 3 + channel]) ||
                            fabs(view.entries[row * 3 + channel] -
                                samples[expected * 3 + channel]) > 2e-6) {
                        break;
                    }
                }
                if (channel == 3) {
                    match = 1;
                    found |= 1u << expected;
                }
            }
            if (!match) {
                fprintf(stderr, "axis %u row %u: %.9g %.9g %.9g\n", axis,
                    row, (double)view.entries[row * 3],
                    (double)view.entries[row * 3 + 1],
                    (double)view.entries[row * 3 + 2]);
                return 0;
            }
        }
        if (!reduced && found != 15u) {
            return 0;
        }
    }
    return 1;
}

static int
solver_pruned(sixel_palette_t *palette, int space)
{
    float samples[384];
    sixel_palette_float32_entries_view_t view;
    SIXELSTATUS status;
    unsigned int row;
    double position;
    int ok;

    /* Force hybrid candidate reduction and addresses far outside the
     * unsigned-int range. Source coordinates are exact binary32 values;
     * representative membership, not optimizer ordering, is the oracle. */
    for (row = 0; row < 128u; ++row) {
        samples[row * 3] = (float)((int)row - 64) * 1073741824.0f;
        samples[row * 3 + 1] = .125f;
        samples[row * 3 + 2] = .125f;
    }
    sixel_set_kcenter_point_budget_override(1, 64u);
    sixel_set_kcenter_candidate_policy_override(1,
        SIXEL_PALETTE_KCENTER_CANDIDATE_POLICY_HYBRID);
    sixel_set_kcenter_space_policy_override(1,
        SIXEL_PALETTE_KCENTER_SPACE_POLICY_PERCEPTUAL);
    sixel_set_kcenter_budget_policy_override(1,
        SIXEL_PALETTE_KCENTER_BUDGET_POLICY_LEGACY);
    sixel_set_kcenter_prune_mass_override(1, 1.0);
    status = solver_build(palette, samples, 128, 2, 0, space,
        SIXEL_PALETTE_BINNING_NONE, 1);
    sixel_set_kcenter_point_budget_override(0, 0u);
    sixel_set_kcenter_candidate_policy_override(0,
        SIXEL_PALETTE_KCENTER_CANDIDATE_POLICY_LEGACY);
    sixel_set_kcenter_space_policy_override(0,
        SIXEL_PALETTE_KCENTER_SPACE_POLICY_LEGACY);
    sixel_set_kcenter_budget_policy_override(0,
        SIXEL_PALETTE_KCENTER_BUDGET_POLICY_LEGACY);
    sixel_set_kcenter_prune_mass_override(0, 0.0);
    ok = SIXEL_SUCCEEDED(status) &&
        SIXEL_SUCCEEDED(palette->vtbl->get_entries_float32(palette, &view)) &&
        view.entry_count == 2 && view.entries != NULL;
    for (row = 0; ok && row < 2u; ++row) {
        position = (double)view.entries[row * 3] / 1073741824.0;
        ok = isfinite(position) && position >= -64.0 && position <= 63.0 &&
            fabs(position - floor(position + .5)) < 2e-6 &&
            fabs(view.entries[row * 3 + 1] - .125) < 2e-6 &&
            fabs(view.entries[row * 3 + 2] - .125) < 2e-6;
    }
    return ok && view.entries[0] != view.entries[3];
}

static int
solver_representative(sixel_palette_t *palette, int model)
{
    float samples[12] = {
        1.25f, .125f, -.25f, 1.25f, .125f, -.25f,
        1.25f, .125f, -.25f, 1.251953125f, .125f, -.25f
    };
    sixel_palette_float32_entries_view_t view;
    double expected;

    /* One bin: K-center chooses its population mean, whereas K-medoids
     * replaces its private bin mean by the nearest observed input. */
    expected = model ? 1.25 : 1.25048828125;
    return SIXEL_SUCCEEDED(solver_build(palette, samples, 4, 1, model,
            0, model ? SIXEL_PALETTE_BINNING_NONE
                     : SIXEL_PALETTE_BINNING_HARD, 1)) &&
        SIXEL_SUCCEEDED(palette->vtbl->get_entries_float32(palette, &view)) &&
        view.entry_count == 1 && view.entries != NULL &&
        fabs(view.entries[0] - expected) < 5e-7 &&
        fabs(view.entries[1] - .125) < 5e-7 &&
        fabs(view.entries[2] + .25) < 5e-7;
}

static int
solver_recover(sixel_palette_t *palette, int model, int invalid)
{
    float samples[3] = { 1.25f, -.25f, .125f };
    sixel_palette_float32_entries_view_t view;
    SIXELSTATUS status;
    int n;

    if (invalid) {
        for (n = 0; n < 3; ++n) {
            samples[0] = n == 0 ? NAN : n == 1 ? INFINITY : -INFINITY;
            if (SIXEL_SUCCEEDED(solver_build(palette, samples, 1, 1,
                    model, 0, SIXEL_PALETTE_BINNING_NONE, 1))) {
                return 0;
            }
        }
        samples[0] = 1.25f;
    } else {
        solver_fail_once = 1;
        solver_failure_hits = 0;
        status = solver_build(palette, samples, 1, 1, model, 0,
            SIXEL_PALETTE_BINNING_NONE, 1);
        if (status != SIXEL_BAD_ALLOCATION || solver_failure_hits != 1) {
            return 0;
        }
    }
    /* Both ordinary and opt-in float attempts preserve source coordinates.
     * Failure must leave the object reusable through either entry point. */
    if (SIXEL_FAILED(solver_build(palette, samples, 1, 1, model, 0,
            SIXEL_PALETTE_BINNING_NONE, 0)) ||
            SIXEL_FAILED(palette->vtbl->get_entries_float32(palette,
                &view)) || view.entries == NULL ||
            view.entry_count != 1) {
        return 0;
    }
    for (n = 0; n < 3; ++n) {
        if (!isfinite(view.entries[n]) ||
                fabs(view.entries[n] - samples[n]) > 5e-7) {
            return 0;
        }
    }
    if (SIXEL_FAILED(solver_build(palette, samples, 1, 1, model, 0,
            SIXEL_PALETTE_BINNING_NONE, 1)) ||
            SIXEL_FAILED(palette->vtbl->get_entries_float32(palette,
                &view)) || view.entry_count != 1 || view.entries == NULL) {
        return 0;
    }
    for (n = 0; n < 3; ++n) {
        if (!isfinite(view.entries[n]) ||
                fabs(view.entries[n] - samples[n]) > 5e-7) {
            return 0;
        }
    }
    return 1;
}

typedef struct solver_capture {
    char data[4096];
    size_t size;
} solver_capture_t;

static int
solver_write(char *data, int size, void *context)
{
    solver_capture_t *capture;

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
solver_wire(int model, int x, int w, int p3, char const *mode)
{
    static struct { int flag; char const *value; } const options[] = {
        { 'p', "1" }, { 'F', "none" }, { 'a', "off" },
        { '_', "none" }, { '~', "none" }, { 'd', "none" },
        { 't', "rgb" }, { '5', "auto" }
    };
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    sixel_dither_t *diagnostic;
    solver_capture_t capture;
    float pixels[24];
    unsigned char bytes[6] = { 0, 0, 0, 255, 255, 255 };
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
    for (n = 0; n < 24; ++n) {
        pixels[n] = (float)color_p3_red_linear[n % 3];
    }
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, solver_write,
                &capture, NULL))) {
        goto end;
    }
    for (n = 0; n < (int)(sizeof(options) / sizeof(options[0])); ++n) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, options[n].flag,
                options[n].value))) {
            goto end;
        }
    }
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'Q', model
                ? "medoids:seed=1" : "center:seed=1")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '=',
                worker ? "balanced:B4" : "balanced:B1")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '.',
                byte ? "8bit" : "auto")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '4',
                worker ? "adaptive-grid" : "full-frame")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'X',
                solver_spaces[x])) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'W',
                solver_spaces[w])) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U',
                p3 ? "display-p3" : "gamma")) ||
            SIXEL_FAILED(sixel_frame_new(&frame, NULL))) {
        goto end;
    }
    if (strcmp(mode, "zero") == 0 &&
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '_',
                "reversible:rate=0"))) {
        goto end;
    }
    if (byte) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'p', "2")) ||
                SIXEL_FAILED(sixel_frame_init_borrowed(frame, bytes, 2, 1,
                    SIXEL_PIXELFORMAT_RGB888, NULL, -1))) {
            goto end;
        }
    } else if (SIXEL_FAILED(sixel_frame_init_borrowed(frame, pixels, 4, 2,
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
    /* The independently derived P3 red must return to (100,0,0).
     * In sRGB it clips only at the final output boundary to the same code.
     * Direct tests separately protect native representative precision. */
    ok = strstr(capture.data, ";2;100;0;0") != NULL &&
        diagnostic->use_rgb101_palette != 0 &&
        diagnostic->pixelformat == solver_formats[w];
    if (byte) {
        ok = diagnostic->use_rgb101_palette == 0 &&
            diagnostic->pixelformat == SIXEL_PIXELFORMAT_RGB888 &&
            strstr(capture.data, ";2;0;0;0") != NULL &&
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
test_palette_solvers_float(int argc, char **argv)
{
    sixel_allocator_t *allocator;
    sixel_palette_t *palette;
    void *object;
    int model;
    int space;
    int algo;
    int bin;
    int ok;

    if (argc != 6) {
        return EXIT_FAILURE;
    }
    model = atoi(argv[2]);
    space = atoi(argv[3]);
    algo = atoi(argv[4]);
    if (model < 0 || model > 1 || space < 0 || space > 4) {
        return EXIT_FAILURE;
    }
    if (strcmp(argv[1], "wire") == 0 || strcmp(argv[1], "worker") == 0 ||
            strcmp(argv[1], "byte") == 0 || strcmp(argv[1], "zero") == 0) {
        return solver_wire(model, space, algo, atoi(argv[5]), argv[1])
            ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    bin = strcmp(argv[5], "exact") == 0 ? SIXEL_PALETTE_BINNING_EXACT
        : strcmp(argv[5], "hard") == 0 ? SIXEL_PALETTE_BINNING_HARD
        : strcmp(argv[5], "auto") == 0 ? SIXEL_PALETTE_BINNING_AUTO
        : SIXEL_PALETTE_BINNING_NONE;
    allocator = NULL;
    palette = NULL;
    object = NULL;
    ok = 0;
    sixel_set_kcenter_algo_override(1, algo);
    sixel_set_kcenter_seed_override(1, 1u);
    sixel_set_kmedoids_algo_override(1, algo);
    sixel_set_kmedoids_seed_override(1, 1u);
    if (SIXEL_SUCCEEDED(sixel_allocator_new(&allocator, solver_malloc,
            NULL, NULL, NULL)) &&
            SIXEL_SUCCEEDED(sixel_palette_factory_new(allocator, &object))) {
        palette = object;
        if (strcmp(argv[1], "pruned") == 0) {
            ok = solver_pruned(palette, space);
        } else if (strcmp(argv[1], "representative") == 0) {
            ok = solver_representative(palette, model);
        } else if (strcmp(argv[1], "invalid") == 0 ||
                   strcmp(argv[1], "failure") == 0) {
            ok = solver_recover(palette, model,
                strcmp(argv[1], "invalid") == 0);
        } else {
            ok = solver_values(palette, model, space, bin,
                strcmp(argv[1], "reduced") == 0);
        }
    }
    if (!ok) {
        fprintf(stderr, "%s model=%d space=%d algo=%d bin=%s: %s\n",
            argv[1], model, space, algo, argv[5],
            sixel_helper_get_additional_message());
    }
    if (palette != NULL) {
        palette->vtbl->unref(palette);
    }
    sixel_allocator_unref(allocator);
    sixel_set_kcenter_algo_override(0, SIXEL_PALETTE_KCENTER_ALGO_AUTO);
    sixel_set_kcenter_seed_override(0, 0u);
    sixel_set_kmedoids_algo_override(0, SIXEL_PALETTE_KMEDOIDS_ALGO_AUTO);
    sixel_set_kmedoids_seed_override(0, 0u);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
