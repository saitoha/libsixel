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
    sixel_set_kmeans_seed_override(1, 1);
    sixel_set_kmeans_init_type_override(1, SIXEL_PALETTE_KMEANS_INIT_NONE);
    sixel_set_kmeans_iter_override(1, 1);
    sixel_set_kmeans_miniter_override(1, 1);
    sixel_set_kmeans_polish_iter_override(1, 0);
    sixel_set_kmeans_feedback_mode_override(1,
        SIXEL_PALETTE_KMEANS_FEEDBACK_OFF);
    sixel_set_kcenter_seed_override(1, 1);
    sixel_set_kmedoids_seed_override(1, 1);
    sixel_set_final_merge_target_factor_override(1, 2.0);
    sixel_set_final_merge_lloyd_iterations_override(1, 0);
}

static void
ordinary_reset(void)
{
    ordinary_fail_next = 0;
    sixel_set_kmeans_seed_override(0, 0);
    sixel_set_kmeans_init_type_override(0, SIXEL_PALETTE_KMEANS_INIT_AUTO);
    sixel_set_kmeans_iter_override(0, 0);
    sixel_set_kmeans_miniter_override(0, 0);
    sixel_set_kmeans_polish_iter_override(0, 0);
    sixel_set_kmeans_feedback_mode_override(0,
        SIXEL_PALETTE_KMEANS_FEEDBACK_OFF);
    sixel_set_kmeans_feedback_interval_override(0, 0);
    sixel_set_kmeans_feedback_slots_override(0, 0);
    sixel_set_kmeans_binbits_override(0, 0);
    sixel_set_palette_snap_override(NULL);
    sixel_set_kcenter_seed_override(0, 0);
    sixel_set_kmedoids_seed_override(0, 0);
    sixel_set_final_merge_target_factor_override(0, 0);
    sixel_set_final_merge_lloyd_iterations_override(0, 0);
}

static SIXELSTATUS
ordinary_build_rep(sixel_palette_t *palette, void *pixels, unsigned int count,
               int format, int model, int bin, int merge, int snap, int prefer,
               unsigned int colors, int representative,
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
    request.requested_colors = colors;
    request.method_for_rep = representative;
    request.method_for_largest = SIXEL_LARGE_NORM;
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

static SIXELSTATUS
ordinary_build(sixel_palette_t *palette, void *pixels, unsigned int count,
               int format, int model, int bin, int merge, int snap, int prefer,
               unsigned int colors,
               sixel_palette_build_attempt_t *attempt)
{
    return ordinary_build_rep(palette, pixels, count, format, model, bin,
        merge, snap, prefer, colors, SIXEL_REP_AVERAGE_PIXELS, attempt);
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
            0, prefer, 2, &attempt)) && ordinary_pair(palette, pixels) &&
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
            SIXEL_FINAL_MERGE_WARD, 0, 1, 2, &attempt)) &&
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
    /* K-means applies ITER to the Lloyd mean, then FINAL to that result.
     * Heckbert snaps the retained representative at ITER, then FINAL.
     * The discrete solvers have only FINAL. Both targets stay nearest to
     * the same byte codes, so two quarter approaches leave 9/16 residual. */
    if (model == SIXEL_QUANTIZE_MODEL_KMEANS ||
            model == SIXEL_QUANTIZE_MODEL_MEDIANCUT) {
        for (channel = 0; channel < 6; ++channel) {
            expected[channel] = (float)(.5625 * pixels[channel]
                + .4375 * targets[channel]);
        }
    }
    ok = SIXEL_SUCCEEDED(ordinary_build(palette, pixels, 2,
        SIXEL_PIXELFORMAT_RGBFLOAT32, model, SIXEL_PALETTE_BINNING_NONE,
        SIXEL_FINAL_MERGE_NONE, 1, 1, 2, &attempt)) &&
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
        0, 1, 2, &attempt);
    ok = status == (nonfinite
        ? (model == SIXEL_QUANTIZE_MODEL_MEDIANCUT
            ? SIXEL_BAD_ARGUMENT : SIXEL_BAD_INPUT) : SIXEL_BAD_ALLOCATION) &&
        attempt.quantizer_retry_count == 0 &&
        (nonfinite || ordinary_fail_hits == 1);
    if (reuse) {
        pixels[5] = 1.5f;
        ok = ok && SIXEL_SUCCEEDED(ordinary_build(palette, pixels, 2,
            SIXEL_PIXELFORMAT_RGBFLOAT32, model, SIXEL_PALETTE_BINNING_NONE,
            SIXEL_FINAL_MERGE_NONE, 0, 1, 2, &attempt)) &&
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
        SIXEL_FINAL_MERGE_NONE, 0, 0, 2, &attempt)) &&
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
        SIXEL_FINAL_MERGE_NONE, 0, 1, 2, &attempt)) &&
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

/* All references below use literal input coordinates or their arithmetic
 * means. The 2e-6 budget covers affine bin mapping, double accumulation and
 * float storage; it rejects byte reconstruction and premature clipping. */
static int
ordinary_single(sixel_palette_t *palette, double const expected[3])
{
    sixel_palette_float32_entries_view_t view;
    unsigned int channel;

    memset(&view, 0, sizeof(view));
    if (SIXEL_FAILED(palette->vtbl->get_entries_float32(palette, &view)) ||
            view.entries == NULL || view.entry_count != 1) {
        return 0;
    }
    for (channel = 0; channel < 3; ++channel) {
        if (!isfinite(view.entries[channel]) ||
                fabs(view.entries[channel] - expected[channel]) > 2e-6) {
            fprintf(stderr, "mean %.9g expected %.9g\n",
                (double)view.entries[channel], expected[channel]);
            return 0;
        }
    }
    return 1;
}

int
ordinary_kmeans_mean(int polish)
{
    float pixels[12] = {.125f, -.25f, 1.25f, .15625f, -.25f, 1.25f,
        .1875f, -.25f, 1.25f, 1.125f, -.25f, 1.25f};
    double const expected[3] = {.3984375, -.25, 1.25};
    sixel_palette_build_attempt_t attempt;
    sixel_palette_t *palette;
    void *object;
    int space;
    int ok;

    object = NULL;
    ok = 1;
    ordinary_options(0);
    sixel_set_final_merge_target_factor_override(1, 3);
    sixel_set_final_merge_lloyd_iterations_override(1, polish);
    sixel_set_kmeans_binbits_override(1, 4);
    if (SIXEL_FAILED(ordinary_new(&object))) {
        ordinary_reset();
        return 0;
    }
    palette = object;
    for (space = 0; space < 5; ++space) {
        ok = ok && SIXEL_SUCCEEDED(ordinary_build(palette, pixels, 4,
            ordinary_formats[space], SIXEL_QUANTIZE_MODEL_KMEANS,
            SIXEL_PALETTE_BINNING_SOFT, SIXEL_FINAL_MERGE_WARD,
            0, 1, 1, &attempt)) && ordinary_single(palette, expected) &&
            attempt.binning.policy.effective == SIXEL_PALETTE_BINNING_SOFT &&
            attempt.binning.policy.phase == SIXEL_PALETTE_POLICY_EXECUTED;
    }
    palette->vtbl->unref(palette);
    ordinary_reset();
    return ok;
}

int
ordinary_kmeans_empty(void)
{
    float pixels[18] = {-2, .125f, 1.25f, -2, .125f, 1.25f,
        -2, .125f, 1.25f, -2, .125f, 1.25f,
        -2, .125f, 1.25f, -2, .125f, 1.25f};
    sixel_palette_build_attempt_t attempt;
    sixel_palette_t *palette;
    void *object;
    int ok;

    object = NULL;
    ordinary_options(0);
    if (SIXEL_FAILED(ordinary_new(&object))) {
        ordinary_reset();
        return 0;
    }
    palette = object;
    /* Identical seeds leave one cluster empty. Moving one source into it
     * must subtract its signed moment without erasing the donor's sum. */
    ok = SIXEL_SUCCEEDED(ordinary_build(palette, pixels, 6,
        SIXEL_PIXELFORMAT_RGBFLOAT32, SIXEL_QUANTIZE_MODEL_KMEANS,
        SIXEL_PALETTE_BINNING_NONE, SIXEL_FINAL_MERGE_NONE,
        0, 1, 2, &attempt)) && ordinary_pair(palette, pixels);
    palette->vtbl->unref(palette);
    ordinary_reset();
    return ok;
}

int
ordinary_kmeans_feedback(int snap, int interval)
{
    float pixels[9] = {-4, 0, 0, -1, 0, 0, -.5f, 0, 0};
    double expected[3] = {-4, 0, 0};
    sixel_palette_build_attempt_t attempt;
    sixel_palette_t *palette;
    void *object;
    int ok;

    object = NULL;
    ordinary_options(snap);
    sixel_set_kmeans_init_type_override(1, SIXEL_PALETTE_KMEANS_INIT_PCA);
    sixel_set_kmeans_feedback_mode_override(1,
        SIXEL_PALETTE_KMEANS_FEEDBACK_ON);
    sixel_set_kmeans_feedback_interval_override(1, interval);
    sixel_set_kmeans_feedback_slots_override(1, 1);
    if (SIXEL_FAILED(ordinary_new(&object))) {
        ordinary_reset();
        return 0;
    }
    palette = object;
    /* One PCA center is the mean -11/6. Its largest squared residual is
     * at -4, even after the requested initial snap. Feedback must keep
     * separate negative bins and choose that literal source coordinate.
     * ITER and FINAL each remove one quarter of its distance from zero. */
    if (snap) {
        expected[0] = -2.25;
    } else if (interval > 1) {
        expected[0] = -11.0 / 6;
    }
    ok = SIXEL_SUCCEEDED(ordinary_build(palette, pixels, 3,
        SIXEL_PIXELFORMAT_RGBFLOAT32, SIXEL_QUANTIZE_MODEL_KMEANS,
        SIXEL_PALETTE_BINNING_NONE, SIXEL_FINAL_MERGE_NONE,
        snap, 1, 1, &attempt)) && ordinary_single(palette, expected);
    palette->vtbl->unref(palette);
    ordinary_reset();
    return ok;
}

int
ordinary_kmeans_unique(void)
{
    float pixels[12] = {.1000f, .5f, .5f, .1004f, .5f, .5f,
        .1006f, .5f, .5f, .1011f, .5f, .5f};
    float expected[6] = {.100333333333f, .5f, .5f, .1011f, .5f, .5f};
    sixel_palette_build_attempt_t attempt;
    sixel_palette_t *palette;
    void *object;
    int ok;

    object = NULL;
    ordinary_options(0);
    sixel_set_kmeans_seed_override(1, 4);
    if (SIXEL_FAILED(ordinary_new(&object))) {
        ordinary_reset();
        return 0;
    }
    palette = object;
    /* These four colors share the same RGB888 projection. Ward must still
     * merge four float seeds: the closest three have mean .100333333333 and the
     * fourth stays .1011. A byte unique-count shortcut skips this merge. */
    ok = SIXEL_SUCCEEDED(ordinary_build(palette, pixels, 4,
        SIXEL_PIXELFORMAT_RGBFLOAT32, SIXEL_QUANTIZE_MODEL_KMEANS,
        SIXEL_PALETTE_BINNING_NONE, SIXEL_FINAL_MERGE_WARD,
        0, 1, 2, &attempt)) && ordinary_pair(palette, expected);
    palette->vtbl->unref(palette);
    ordinary_reset();
    return ok;
}

/* Deliberately asymmetric populations distinguish box extrema, distinct-color
 * mean and occurrence mean. Binary fractions are exact source fixtures;
 * 2e-6 covers only scale/unscale and the final float store. */
int
ordinary_heckbert_representative(int representative, int polish)
{
    float pixels[15] = {-.5f, -2.f, 3.f, -.25f, -2.f, 3.f,
        -.25f, -2.f, 3.f, -.25f, -2.f, 3.f, 1.5f, -2.f, 3.f};
    double expected[3] = {0.0, -2.0, 3.0};
    sixel_palette_build_attempt_t attempt;
    sixel_palette_t *palette;
    void *object;
    int space;
    int ok;

    object = NULL;
    ok = 1;
    ordinary_options(0);
    expected[0] = representative == SIXEL_REP_CENTER_BOX ? .5
        : representative == SIXEL_REP_AVERAGE_COLORS ? .25 : .05;
    if (polish) {
        expected[0] = .05;
        sixel_set_final_merge_lloyd_iterations_override(1, 3);
    }
    if (SIXEL_FAILED(ordinary_new(&object))) {
        ordinary_reset();
        return 0;
    }
    palette = object;
    for (space = 0; space < 5; ++space) {
        ok = ok && SIXEL_SUCCEEDED(ordinary_build_rep(palette, pixels, 5,
            ordinary_formats[space], SIXEL_QUANTIZE_MODEL_MEDIANCUT,
            SIXEL_PALETTE_BINNING_NONE, polish ? SIXEL_FINAL_MERGE_WARD
                : SIXEL_FINAL_MERGE_NONE,
            0, 1, 1, representative, &attempt)) &&
            ordinary_single(palette, expected);
    }
    palette->vtbl->unref(palette);
    ordinary_reset();
    return ok;
}

int
ordinary_heckbert_once(void)
{
    float pixels[6] = {-.25f, 1.25f, .38480392098426819f, .5f, .5f, .5f};
    float expected[6];
    double const target[6] = {0, 1, 99.0 / 255, 128.0 / 255,
        128.0 / 255, 128.0 / 255};
    sixel_palette_snap_options_t options;
    sixel_palette_build_attempt_t attempt;
    sixel_palette_t *palette;
    void *object;
    int channel;
    int ok;

    object = NULL;
    ordinary_options(1);
    memset(&options, 0, sizeof(options));
    options.policy_override = options.timing_override = 1;
    options.approach_rate_override = 1;
    options.policy = SIXEL_PALETTE_SNAP_POLICY_NEAREST;
    options.timing = SIXEL_PALETTE_SNAP_TIMING_ONCE;
    options.approach_rate = .25;
    sixel_set_palette_snap_override(&options);
    for (channel = 0; channel < 6; ++channel) {
        expected[channel] = (float)(.75 * pixels[channel]
            + .25 * target[channel]);
    }
    if (SIXEL_FAILED(ordinary_new(&object))) {
        ordinary_reset();
        return 0;
    }
    palette = object;
    /* Exactly one quarter approach; histogram construction cannot snap or
     * quantize samples when only the final palette stage is requested. */
    ok = SIXEL_SUCCEEDED(ordinary_build(palette, pixels, 2,
        SIXEL_PIXELFORMAT_RGBFLOAT32, SIXEL_QUANTIZE_MODEL_MEDIANCUT,
        SIXEL_PALETTE_BINNING_NONE, SIXEL_FINAL_MERGE_NONE, 1, 1, 2,
        &attempt)) && ordinary_pair(palette, expected);
    palette->vtbl->unref(palette);
    ordinary_reset();
    return ok;
}

int
ordinary_heckbert_unique(void)
{
    float pixels[6] = {.25f, -.5f, 1.5f, .250244140625f, -.5f, 1.5f};
    sixel_palette_build_attempt_t attempt;
    sixel_palette_t *palette;
    void *object;
    sixel_palette_metadata_t metadata;
    int ok;

    object = NULL;
    memset(&metadata, 0, sizeof(metadata));
    ordinary_options(0);
    if (SIXEL_FAILED(ordinary_new(&object))) {
        ordinary_reset();
        return 0;
    }
    palette = object;
    ok = SIXEL_SUCCEEDED(ordinary_build(palette, pixels, 2,
        SIXEL_PIXELFORMAT_RGBFLOAT32, SIXEL_QUANTIZE_MODEL_MEDIANCUT,
        SIXEL_PALETTE_BINNING_NONE, SIXEL_FINAL_MERGE_NONE, 0, 1, 2,
        &attempt)) && ordinary_pair(palette, pixels) &&
        SIXEL_SUCCEEDED(palette->vtbl->get_metadata(palette, &metadata))
        && metadata.original_colors == 2;
    palette->vtbl->unref(palette);
    ordinary_reset();
    return ok;
}
