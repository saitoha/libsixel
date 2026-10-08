/* SPDX-License-Identifier: MIT */
/* Ordinary generate() must preserve coordinates without native admission. */
#include "ordinary_float_test_common.h"

static int ordinary_fail_next;
static int ordinary_fail_hits;
static int const ordinary_formats[] = {SIXEL_PIXELFORMAT_RGBFLOAT32,
    SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, SIXEL_PIXELFORMAT_OKLABFLOAT32,
    SIXEL_PIXELFORMAT_CIELABFLOAT32, SIXEL_PIXELFORMAT_DIN99DFLOAT32};

static void *
ordinary_malloc(size_t size)
{
    if (ordinary_fail_next) {
        ordinary_fail_next = 0;
        ++ordinary_fail_hits;
        return NULL;
    }
    return malloc(size);
}

static SIXELSTATUS
ordinary_new(void **object)
{
    sixel_allocator_t *allocator;
    SIXELSTATUS status;

    allocator = NULL;
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_palette_factory_new(allocator, object);
    }
    sixel_allocator_unref(allocator);
    return status;
}

static void
ordinary_options(int snap)
{
    sixel_palette_snap_options_t options;

    memset(&options, 0, sizeof(options));
    options.policy_override = options.timing_override = 1;
    options.approach_rate_override = 1;
    options.policy = snap ? SIXEL_PALETTE_SNAP_POLICY_NEAREST
        : SIXEL_PALETTE_SNAP_POLICY_NONE;
    options.timing = SIXEL_PALETTE_SNAP_TIMING_ALL;
    options.approach_rate = .25;
    sixel_set_palette_snap_override(&options);
    sixel_set_kcenter_seed_override(1, 1);
    sixel_set_kmedoids_seed_override(1, 1);
    sixel_set_final_merge_target_factor_override(1, 2.0);
    sixel_set_final_merge_lloyd_iterations_override(1, 0);
}

static void
ordinary_reset(void)
{
    ordinary_fail_next = 0;
    sixel_set_palette_snap_override(NULL);
    sixel_set_kcenter_seed_override(0, 0);
    sixel_set_kmedoids_seed_override(0, 0);
    sixel_set_final_merge_target_factor_override(0, 0);
    sixel_set_final_merge_lloyd_iterations_override(0, 0);
}

static SIXELSTATUS
ordinary_build(sixel_palette_t *palette, void *pixels, unsigned int count,
               int format, int model, int bin, int merge, int snap, int prefer,
               sixel_palette_build_attempt_t *attempt)
{
    sixel_palette_generate_request_t request;
    SIXELSTATUS status;
    SIXELSTATUS finish;

    memset(&request, 0, sizeof(request));
    memset(attempt, 0, sizeof(*attempt));
    request.data = pixels;
    request.length = count * 3u *
        (SIXEL_PIXELFORMAT_IS_FLOAT32(format) ? sizeof(float) : 1u);
    request.pixelformat = format;
    request.requested_colors = 2;
    request.prefer_float32 = prefer;
    request.quantize_model = model;
    request.final_merge_mode = merge;
    request.use_reversible = snap;
    request.lut_policy = SIXEL_LUT_POLICY_NONE;
    request.quality_mode = SIXEL_QUALITY_FULL;
    sixel_palette_binning_state_init(&attempt->binning, bin,
        SIXEL_PALETTE_POLICY_ORIGIN_EXPLICIT);
    if (model == SIXEL_QUANTIZE_MODEL_KCENTER) {
        status = sixel_palette_binning_resolve(&attempt->binning, bin,
            bin == SIXEL_PALETTE_BINNING_HARD ? 5u : 0u,
            bin == SIXEL_PALETTE_BINNING_HARD
                ? SIXEL_PALETTE_BINNING_GRID_UNIFORM_256
                : SIXEL_PALETTE_BINNING_GRID_NONE,
            SIXEL_PALETTE_BINNING_KERNEL_NONE,
            bin == SIXEL_PALETTE_BINNING_NONE
                ? SIXEL_PALETTE_BINNING_BACKEND_DIRECT
                : SIXEL_PALETTE_BINNING_BACKEND_COMPACT_SPARSE,
            count, SIXEL_PALETTE_RESOLUTION_EXPLICIT);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }
    status = sixel_palette_build_attempt_begin(palette, attempt);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    /* Deliberately use the ordinary entry point, not generate_extended(). */
    status = palette->vtbl->generate(palette, &request);
    finish = sixel_palette_build_attempt_finish(palette, attempt);
    if (SIXEL_SUCCEEDED(status) &&
            (attempt->quantize_model != model ||
             attempt->quantizer_retry_count != 0)) {
        return SIXEL_LOGIC_ERROR;
    }
    return SIXEL_FAILED(status) ? status : finish;
}

static int
ordinary_pair(sixel_palette_t *palette, float const *expected)
{
    sixel_palette_float32_entries_view_t view;
    unsigned int first;
    unsigned int row;
    unsigned int channel;
    double value;

    memset(&view, 0, sizeof(view));
    if (SIXEL_FAILED(palette->vtbl->get_entries_float32(palette, &view)) ||
            view.entries == NULL || view.entry_count != 2 ||
            view.depth != 3 * (int)sizeof(float)) {
        return 0;
    }
    first = view.entries[0] < view.entries[3] ? 0u : 1u;
    for (row = 0; row < 2; ++row) {
        for (channel = 0; channel < 3; ++channel) {
            value = view.entries[((first + row) % 2) * 3 + channel];
            /* Fixed affine scale/unscale and float export on small inputs.
             * 2e-6 is far smaller than either a clamp or a byte step. */
            if (!isfinite(value) ||
                    fabs(value - expected[row * 3 + channel]) > 2e-6) {
                fprintf(stderr, "coordinate %.9g expected %.9g\n", value,
                    (double)expected[row * 3 + channel]);
                return 0;
            }
        }
    }
    return 1;
}

int
ordinary_float_sources(int model, int bin, int prefer)
{
    float pixels[6] = {-.75f, -2.25f, 3.25f, -.25f, -2.f, 3.5f};
    float saved[6];
    sixel_palette_build_attempt_t attempt;
    sixel_palette_t *palette;
    void *object;
    int space;
    int ok;

    object = NULL;
    palette = NULL;
    ok = 1;
    memcpy(saved, pixels, sizeof(saved));
    ordinary_options(0);
    if (SIXEL_FAILED(ordinary_new(&object))) {
        ordinary_reset();
        return 0;
    }
    palette = object;
    for (space = 0; space < 5; ++space) {
        ok = ok && SIXEL_SUCCEEDED(ordinary_build(palette, pixels, 2,
            ordinary_formats[space], model, bin, SIXEL_FINAL_MERGE_NONE,
            0, prefer, &attempt)) && ordinary_pair(palette, pixels) &&
            memcmp(saved, pixels, sizeof(saved)) == 0;
    }
    palette->vtbl->unref(palette);
    ordinary_reset();
    return ok;
}

int
ordinary_float_ward(int model)
{
    float pixels[12] = {-.5f, -.75f, 1.75f, -.25f, -.75f, 1.75f,
        1.25f, -.75f, 1.75f, 1.5f, -.75f, 1.75f};
    float expected[6] = {-.375f, -.75f, 1.75f, 1.375f, -.75f, 1.75f};
    sixel_palette_build_attempt_t attempt;
    sixel_palette_t *palette;
    void *object;
    int space;
    int ok;

    object = NULL;
    palette = NULL;
    ok = 1;
    ordinary_options(0);
    if (SIXEL_FAILED(ordinary_new(&object))) {
        ordinary_reset();
        return 0;
    }
    palette = object;
    /* Four initial representatives merge into the two unique closest pairs.
     * Their independently specified arithmetic means keep all signed axes. */
    for (space = 0; space < 5; ++space) {
        ok = ok && SIXEL_SUCCEEDED(ordinary_build(palette, pixels, 4,
            ordinary_formats[space], model, SIXEL_PALETTE_BINNING_NONE,
            SIXEL_FINAL_MERGE_WARD, 0, 1, &attempt)) &&
            ordinary_pair(palette, expected);
    }
    palette->vtbl->unref(palette);
    ordinary_reset();
    return ok;
}

int
ordinary_float_snap(int model)
{
    float pixels[6] = {-.25f, 1.25f, .38480392098426819f, .5f, .5f, .5f};
    float expected[6];
    double const targets[6] = {0, 1, 99.0 / 255, 128.0 / 255,
        128.0 / 255, 128.0 / 255};
    sixel_palette_build_attempt_t attempt;
    sixel_palette_t *palette;
    void *object;
    int channel;
    int ok;

    object = NULL;
    palette = NULL;
    ordinary_options(1);
    for (channel = 0; channel < 6; ++channel) {
        expected[channel] = (float)(.75 * pixels[channel]
            + .25 * targets[channel]);
    }
    if (SIXEL_FAILED(ordinary_new(&object))) {
        ordinary_reset();
        return 0;
    }
    palette = object;
    /* These solvers currently have a final hook only, even for timing ALL.
     * Preservation must not suppress that requested hook or preclip it. */
    ok = SIXEL_SUCCEEDED(ordinary_build(palette, pixels, 2,
        SIXEL_PIXELFORMAT_RGBFLOAT32, model, SIXEL_PALETTE_BINNING_NONE,
        SIXEL_FINAL_MERGE_NONE, 1, 1, &attempt)) &&
        ordinary_pair(palette, expected);
    palette->vtbl->unref(palette);
    ordinary_reset();
    return ok;
}

int
ordinary_float_failure(int model, int nonfinite, int reuse)
{
    float pixels[6] = {-.25f, 1.25f, .125f, .75f, -.5f, 1.5f};
    sixel_allocator_t *allocator;
    sixel_palette_t *palette;
    sixel_palette_build_attempt_t attempt;
    SIXELSTATUS status;
    void *object;
    int ok;

    allocator = NULL;
    palette = NULL;
    object = NULL;
    ok = 0;
    ordinary_options(0);
    ordinary_fail_hits = 0;
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, ordinary_malloc,
            NULL, NULL, NULL)) ||
            SIXEL_FAILED(sixel_palette_factory_new(allocator, &object))) {
        goto end;
    }
    palette = object;
    if (nonfinite) {
        pixels[5] = NAN;
    } else {
        ordinary_fail_next = 1;
    }
    status = ordinary_build(palette, pixels, 2, SIXEL_PIXELFORMAT_RGBFLOAT32,
        model, SIXEL_PALETTE_BINNING_NONE, SIXEL_FINAL_MERGE_NONE,
        0, 1, &attempt);
    ok = status == (nonfinite ? SIXEL_BAD_INPUT : SIXEL_BAD_ALLOCATION) &&
        attempt.quantizer_retry_count == 0 &&
        (nonfinite || ordinary_fail_hits == 1);
    if (reuse) {
        pixels[5] = 1.5f;
        ok = ok && SIXEL_SUCCEEDED(ordinary_build(palette, pixels, 2,
            SIXEL_PIXELFORMAT_RGBFLOAT32, model, SIXEL_PALETTE_BINNING_NONE,
            SIXEL_FINAL_MERGE_NONE, 0, 1, &attempt)) &&
            ordinary_pair(palette, pixels);
    }
end:
    if (palette != NULL) {
        palette->vtbl->unref(palette);
    }
    sixel_allocator_unref(allocator);
    ordinary_reset();
    return ok;
}

int
ordinary_float_byte(int model)
{
    unsigned char pixels[6] = {16, 32, 48, 224, 208, 192};
    sixel_palette_build_attempt_t attempt;
    sixel_palette_float32_entries_view_t fview;
    sixel_palette_entries_view_t view;
    sixel_palette_t *palette;
    void *object;
    int ok;

    object = NULL;
    palette = NULL;
    memset(&view, 0, sizeof(view));
    memset(&fview, 0, sizeof(fview));
    ordinary_options(0);
    if (SIXEL_FAILED(ordinary_new(&object))) {
        ordinary_reset();
        return 0;
    }
    palette = object;
    ok = SIXEL_SUCCEEDED(ordinary_build(palette, pixels, 2,
        SIXEL_PIXELFORMAT_RGB888, model, SIXEL_PALETTE_BINNING_NONE,
        SIXEL_FINAL_MERGE_NONE, 0, 0, &attempt)) &&
        SIXEL_SUCCEEDED(palette->vtbl->get_entries(palette, &view)) &&
        SIXEL_SUCCEEDED(palette->vtbl->get_entries_float32(palette, &fview)) &&
        fview.entries == NULL && view.entry_count == 2 && view.depth == 3 &&
        ((memcmp(view.entries, pixels, 6) == 0) ||
         (memcmp(view.entries, pixels + 3, 3) == 0 &&
          memcmp(view.entries + 3, pixels, 3) == 0));
    palette->vtbl->unref(palette);
    ordinary_reset();
    return ok;
}

int
ordinary_float_reduce(int model)
{
    float pixels[12] = {-.75f, -.5f, 1.5f, -.5f, -.5f, 1.5f,
        1.25f, -.5f, 1.5f, 1.5f, -.5f, 1.5f};
    sixel_palette_build_attempt_t attempt;
    sixel_palette_float32_entries_view_t view;
    sixel_palette_t *palette;
    void *object;
    unsigned int row;
    unsigned int source;
    int found;
    int ok;

    object = NULL;
    palette = NULL;
    memset(&view, 0, sizeof(view));
    ordinary_options(0);
    if (SIXEL_FAILED(ordinary_new(&object))) {
        ordinary_reset();
        return 0;
    }
    palette = object;
    ok = SIXEL_SUCCEEDED(ordinary_build(palette, pixels, 4,
        SIXEL_PIXELFORMAT_RGBFLOAT32, model, SIXEL_PALETTE_BINNING_NONE,
        SIXEL_FINAL_MERGE_NONE, 0, 1, &attempt)) &&
        SIXEL_SUCCEEDED(palette->vtbl->get_entries_float32(palette, &view)) &&
        view.entries != NULL && view.entry_count == 2;
    if (ok) {
        ok = view.entries[0] * view.entries[3] < 0;
        for (row = 0; row < 2; ++row) {
            found = 0;
            for (source = 0; source < 4; ++source) {
                found |= isfinite(view.entries[row * 3]) &&
                    fabs(view.entries[row * 3] - pixels[source * 3]) < 2e-6;
            }
            ok = ok && found && view.entries[row * 3 + 1] == -.5f &&
                view.entries[row * 3 + 2] == 1.5f;
        }
    }
    palette->vtbl->unref(palette);
    ordinary_reset();
    return ok;
}
