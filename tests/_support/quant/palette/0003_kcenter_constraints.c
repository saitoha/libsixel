/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 *
 * Verify k-center palette constraints from TAP wrappers:
 * - seeded fft/swap/hybrid paths are reproducible
 * - swap radius does not increase with additional iterations
 * - swap update modes remain deterministic under identical seeds
 * - auto strategy selection follows quality/point-count branches
 * - profile=legacy keeps compatibility output
 * - perceptual adaptive strata keeps deterministic bin selection
 * - radius-constrained SSE polish never increases the final radius
 * - perceptual OKLab auto policy prefers hybrid where intended
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <sixel.h>

#include "src/dither.h"
#include "src/palette-kcenter.h"

#define TEST_SEED_WIDTH 128u
#define TEST_SEED_HEIGHT 1u
#define TEST_SEED_PIXELS (TEST_SEED_WIDTH * TEST_SEED_HEIGHT)
#define TEST_SEED_REQCOLORS 16

#define TEST_SWAP_WIDTH 512u
#define TEST_SWAP_HEIGHT 1u
#define TEST_SWAP_PIXELS (TEST_SWAP_WIDTH * TEST_SWAP_HEIGHT)
#define TEST_SWAP_REQCOLORS 16

#define TEST_AUTO_WIDTH 64u
#define TEST_AUTO_HEIGHT 64u
#define TEST_AUTO_PIXELS (TEST_AUTO_WIDTH * TEST_AUTO_HEIGHT)
#define TEST_AUTO_REQCOLORS 128

/*
 * Keep helper state reset explicit so each test case remains independent.
 * These defaults match encoder-side reset values.
 */
static void
test_reset_kcenter_overrides(void)
{
    sixel_set_kcenter_algo_override(0, SIXEL_PALETTE_KCENTER_ALGO_AUTO);
    sixel_set_kcenter_profile_override(0,
                                       SIXEL_PALETTE_KCENTER_PROFILE_LEGACY);
    sixel_set_kcenter_seed_override(0, 1u);
    sixel_set_kcenter_restarts_override(0, 1u);
    sixel_set_kcenter_init_seeds_override(0, 1u);
    sixel_set_kcenter_iter_override(0, 16u);
    sixel_set_kcenter_histbits_override(0, 5u);
    sixel_set_kcenter_point_budget_override(0, 0u);
    sixel_set_kcenter_auto_policy_override(
        0,
        SIXEL_PALETTE_KCENTER_AUTO_POLICY_LEGACY);
    sixel_set_kcenter_auto_fft_threshold_override(0, 2048u);
    sixel_set_kcenter_space_policy_override(
        0,
        SIXEL_PALETTE_KCENTER_SPACE_POLICY_LEGACY);
    sixel_set_kcenter_candidate_policy_override(
        0,
        SIXEL_PALETTE_KCENTER_CANDIDATE_POLICY_LEGACY);
    sixel_set_kcenter_rare_keep_override(0, 0u);
    sixel_set_kcenter_budget_policy_override(
        0,
        SIXEL_PALETTE_KCENTER_BUDGET_POLICY_LEGACY);
    sixel_set_kcenter_budget_scale_override(0, 1.0);
    sixel_set_kcenter_swap_topk_override(0, 1u);
    sixel_set_kcenter_swap_update_override(
        0,
        SIXEL_PALETTE_KCENTER_SWAP_UPDATE_FULL);
    sixel_set_kcenter_swap_patience_override(0, 0u);
    sixel_set_kcenter_swap_min_gain_override(0, 0.0);
    sixel_set_kcenter_prune_mass_override(0, 0.995);
}

static void
test_fill_seed_pixels(unsigned char *pixels,
                      unsigned int pixel_count)
{
    unsigned int index;

    index = 0u;
    if (pixels == NULL || pixel_count == 0u) {
        return;
    }

    for (index = 0u; index < pixel_count; ++index) {
        pixels[index * 3u + 0u] = (unsigned char)((index * 29u + 7u) & 0xffu);
        pixels[index * 3u + 1u] = (unsigned char)((index * 53u + 19u) & 0xffu);
        pixels[index * 3u + 2u] = (unsigned char)((index * 97u + 31u) & 0xffu);
    }
}

static void
test_fill_auto_pixels(unsigned char *pixels)
{
    unsigned int index;
    unsigned int r;
    unsigned int g;
    unsigned int b;

    index = 0u;
    r = 0u;
    g = 0u;
    b = 0u;
    if (pixels == NULL) {
        return;
    }

    for (index = 0u; index < TEST_AUTO_PIXELS; ++index) {
        r = (index >> 0u) & 0x0fu;
        g = (index >> 4u) & 0x0fu;
        b = (index >> 8u) & 0x0fu;
        pixels[index * 3u + 0u] = (unsigned char)(r * 17u);
        pixels[index * 3u + 1u] = (unsigned char)(g * 17u);
        pixels[index * 3u + 2u] = (unsigned char)(b * 17u);
    }
}

static void
test_fill_oklab_pixels(float *pixels)
{
    unsigned int index;
    unsigned int x;
    unsigned int y;
    int chroma_i;
    float lightness;
    float chroma_a;
    float chroma_b;

    index = 0u;
    x = 0u;
    y = 0u;
    chroma_i = 0;
    lightness = 0.0f;
    chroma_a = 0.0f;
    chroma_b = 0.0f;
    if (pixels == NULL) {
        return;
    }

    for (index = 0u; index < TEST_AUTO_PIXELS; ++index) {
        x = index % TEST_AUTO_WIDTH;
        y = index / TEST_AUTO_WIDTH;
        lightness = (float)((x * 5u + y * 3u) & 63u) / 63.0f;
        chroma_i = (int)((x * 11u + y * 7u) & 31u) - 16;
        chroma_a = (float)chroma_i / 40.0f;
        chroma_i = (int)((x * 13u + y * 9u) & 31u) - 16;
        chroma_b = (float)chroma_i / 40.0f;
        pixels[index * 3u + 0u] = lightness;
        pixels[index * 3u + 1u] = chroma_a;
        pixels[index * 3u + 2u] = chroma_b;
    }
}

/*
 * Copy palette entries through the non-deprecated palette object API.
 * The caller must release `*palette_out` with sixel_allocator_free().
 */
static int
test_copy_palette_from_kcenter(sixel_dither_t *dither,
                               sixel_allocator_t *allocator,
                               unsigned char **palette_out,
                               unsigned int *ncolors_out)
{
    SIXELSTATUS status;
    sixel_palette_entries_view_t palette_view;
    unsigned char *palette;
    size_t payload_size;

    status = SIXEL_FALSE;
    memset(&palette_view, 0, sizeof(palette_view));
    palette = NULL;
    payload_size = 0u;
    if (dither == NULL
            || allocator == NULL
            || palette_out == NULL
            || ncolors_out == NULL) {
        return 0;
    }
    *palette_out = NULL;
    *ncolors_out = 0u;

    if (dither->palette == NULL || dither->palette->vtbl == NULL ||
            dither->palette->vtbl->get_entries == NULL) {
        return 0;
    }

    status = dither->palette->vtbl->get_entries(dither->palette,
                                                &palette_view);
    if (SIXEL_FAILED(status) || palette_view.entries == NULL ||
            palette_view.depth != 3 || palette_view.entry_count == 0u) {
        return 0;
    }
    payload_size = (size_t)palette_view.entry_count * 3u;
    palette = (unsigned char *)sixel_allocator_malloc(allocator,
                                                      payload_size);
    if (palette == NULL) {
        return 0;
    }
    memcpy(palette, palette_view.entries, payload_size);

    *palette_out = palette;
    *ncolors_out = (unsigned int)palette_view.entry_count;
    return 1;
}

static int
test_build_kcenter_palette_with_options_format(
    unsigned char const *pixels,
    unsigned int width,
    unsigned int height,
    unsigned int reqcolors,
    int quality_mode,
    sixel_kcenter_algo_t algo,
    uint32_t seed,
    unsigned int restarts,
    unsigned int iter_count,
    unsigned int histbits,
    unsigned int point_budget,
    double prune_mass,
    int use_profile,
    sixel_kcenter_profile_t profile,
    int use_auto_policy,
    sixel_kcenter_auto_policy_t auto_policy,
    int use_auto_fft_threshold,
    unsigned int auto_fft_threshold,
    int use_candidate_policy,
    sixel_kcenter_candidate_policy_t candidate_policy,
    int use_rare_keep,
    unsigned int rare_keep,
    int use_budget_policy,
    sixel_kcenter_budget_policy_t budget_policy,
    int use_budget_scale,
    double budget_scale,
    int use_swap_topk,
    unsigned int swap_topk,
    int use_swap_update,
    sixel_kcenter_swap_update_t swap_update,
    int use_swap_patience,
    unsigned int swap_patience,
    int input_pixelformat,
    int prefer_float32,
    unsigned char **palette_out,
    unsigned int *ncolors_out,
    sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    sixel_dither_t *dither;
    int ok;

    status = SIXEL_FALSE;
    dither = NULL;
    ok = 0;
    if (pixels == NULL
            || width == 0u
            || height == 0u
            || reqcolors == 0u
            || palette_out == NULL
            || ncolors_out == NULL
            || allocator == NULL) {
        return 0;
    }

    *palette_out = NULL;
    *ncolors_out = 0u;
    status = sixel_dither_new(&dither, (int)reqcolors, allocator);
    if (SIXEL_FAILED(status) || dither == NULL) {
        goto end;
    }

    dither->quantize_model = SIXEL_QUANTIZE_MODEL_KCENTER;
    dither->final_merge_mode = SIXEL_FINAL_MERGE_NONE;
    dither->prefer_float32 = prefer_float32 ? 1 : 0;

    sixel_set_kcenter_algo_override(1, algo);
    sixel_set_kcenter_seed_override(1, seed);
    sixel_set_kcenter_restarts_override(1, restarts);
    sixel_set_kcenter_iter_override(1, iter_count);
    sixel_set_kcenter_histbits_override(1, histbits);
    sixel_set_kcenter_point_budget_override(1, point_budget);
    sixel_set_kcenter_prune_mass_override(1, prune_mass);
    if (use_profile) {
        sixel_set_kcenter_profile_override(1, profile);
    }
    if (use_auto_policy) {
        sixel_set_kcenter_auto_policy_override(1, auto_policy);
    }
    if (use_auto_fft_threshold) {
        sixel_set_kcenter_auto_fft_threshold_override(1, auto_fft_threshold);
    }
    if (use_candidate_policy) {
        sixel_set_kcenter_candidate_policy_override(1, candidate_policy);
    }
    if (use_rare_keep) {
        sixel_set_kcenter_rare_keep_override(1, rare_keep);
    }
    if (use_budget_policy) {
        sixel_set_kcenter_budget_policy_override(1, budget_policy);
    }
    if (use_budget_scale) {
        sixel_set_kcenter_budget_scale_override(1, budget_scale);
    }
    if (use_swap_topk) {
        sixel_set_kcenter_swap_topk_override(1, swap_topk);
    }
    if (use_swap_update) {
        sixel_set_kcenter_swap_update_override(1, swap_update);
    }
    if (use_swap_patience) {
        sixel_set_kcenter_swap_patience_override(1, swap_patience);
    }

    status = sixel_dither_initialize(dither,
                                     (unsigned char *)pixels,
                                     (int)width,
                                     (int)height,
                                     input_pixelformat,
                                     SIXEL_LARGE_AUTO,
                                     SIXEL_REP_AUTO,
                                     quality_mode);
    test_reset_kcenter_overrides();
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (!test_copy_palette_from_kcenter(dither,
                                        allocator,
                                        palette_out,
                                        ncolors_out)) {
        goto end;
    }
    ok = 1;

end:
    if (dither != NULL) {
        sixel_dither_unref(dither);
    }
    if (!ok) {
        test_reset_kcenter_overrides();
    }
    return ok;
}

/*
 * Keep the legacy RGB888 wrapper as a macro to avoid pcc ICE in the helper
 * function definition with a very long argument list.
 */
#define test_build_kcenter_palette_with_options(                         \
    pixels,                                                              \
    width,                                                               \
    height,                                                              \
    reqcolors,                                                           \
    quality_mode,                                                        \
    algo,                                                                \
    seed,                                                                \
    restarts,                                                            \
    iter_count,                                                          \
    histbits,                                                            \
    point_budget,                                                        \
    prune_mass,                                                          \
    use_profile,                                                         \
    profile,                                                             \
    use_auto_policy,                                                     \
    auto_policy,                                                         \
    use_auto_fft_threshold,                                              \
    auto_fft_threshold,                                                  \
    use_candidate_policy,                                                \
    candidate_policy,                                                    \
    use_rare_keep,                                                       \
    rare_keep,                                                           \
    use_budget_policy,                                                   \
    budget_policy,                                                       \
    use_budget_scale,                                                    \
    budget_scale,                                                        \
    use_swap_topk,                                                       \
    swap_topk,                                                           \
    use_swap_update,                                                     \
    swap_update,                                                         \
    use_swap_patience,                                                   \
    swap_patience,                                                       \
    palette_out,                                                         \
    ncolors_out,                                                         \
    allocator)                                                           \
    test_build_kcenter_palette_with_options_format(                      \
        pixels,                                                          \
        width,                                                           \
        height,                                                          \
        reqcolors,                                                       \
        quality_mode,                                                    \
        algo,                                                            \
        seed,                                                            \
        restarts,                                                        \
        iter_count,                                                      \
        histbits,                                                        \
        point_budget,                                                    \
        prune_mass,                                                      \
        use_profile,                                                     \
        profile,                                                         \
        use_auto_policy,                                                 \
        auto_policy,                                                     \
        use_auto_fft_threshold,                                          \
        auto_fft_threshold,                                              \
        use_candidate_policy,                                            \
        candidate_policy,                                                \
        use_rare_keep,                                                   \
        rare_keep,                                                       \
        use_budget_policy,                                               \
        budget_policy,                                                   \
        use_budget_scale,                                                \
        budget_scale,                                                    \
        use_swap_topk,                                                   \
        swap_topk,                                                       \
        use_swap_update,                                                 \
        swap_update,                                                     \
        use_swap_patience,                                               \
        swap_patience,                                                   \
        SIXEL_PIXELFORMAT_RGB888,                                        \
        0,                                                               \
        palette_out,                                                     \
        ncolors_out,                                                     \
        allocator)

static int
test_build_kcenter_palette(unsigned char const *pixels,
                           unsigned int width,
                           unsigned int height,
                           unsigned int reqcolors,
                           int quality_mode,
                           sixel_kcenter_algo_t algo,
                           uint32_t seed,
                           unsigned int restarts,
                           unsigned int iter_count,
                           unsigned int histbits,
                           unsigned int point_budget,
                           double prune_mass,
                           unsigned char **palette_out,
                           unsigned int *ncolors_out,
                           sixel_allocator_t *allocator)
{
    return test_build_kcenter_palette_with_options(
        pixels,
        width,
        height,
        reqcolors,
        quality_mode,
        algo,
        seed,
        restarts,
        iter_count,
        histbits,
        point_budget,
        prune_mass,
        0,
        SIXEL_PALETTE_KCENTER_PROFILE_LEGACY,
        0,
        SIXEL_PALETTE_KCENTER_AUTO_POLICY_LEGACY,
        0,
        2048u,
        0,
        SIXEL_PALETTE_KCENTER_CANDIDATE_POLICY_LEGACY,
        0,
        0u,
        0,
        SIXEL_PALETTE_KCENTER_BUDGET_POLICY_LEGACY,
        0,
        1.0,
        0,
        1u,
        0,
        SIXEL_PALETTE_KCENTER_SWAP_UPDATE_FULL,
        0,
        0u,
        palette_out,
        ncolors_out,
        allocator);
}

static int
test_palette_equal(unsigned char const *lhs,
                   unsigned int lhs_colors,
                   unsigned char const *rhs,
                   unsigned int rhs_colors)
{
    if (lhs == NULL || rhs == NULL) {
        return 0;
    }
    if (lhs_colors != rhs_colors) {
        return 0;
    }
    if (memcmp(lhs, rhs, (size_t)lhs_colors * 3u) != 0) {
        return 0;
    }
    return 1;
}

static double
test_palette_radius_sq(unsigned char const *pixels,
                       unsigned int pixel_count,
                       unsigned char const *palette,
                       unsigned int palette_count)
{
    unsigned int pixel_index;
    unsigned int color_index;
    double best_distance;
    double distance;
    double dr;
    double dg;
    double db;
    double radius2;

    pixel_index = 0u;
    color_index = 0u;
    best_distance = 0.0;
    distance = 0.0;
    dr = 0.0;
    dg = 0.0;
    db = 0.0;
    radius2 = 0.0;
    if (pixels == NULL || palette == NULL
            || pixel_count == 0u || palette_count == 0u) {
        return 0.0;
    }

    for (pixel_index = 0u; pixel_index < pixel_count; ++pixel_index) {
        best_distance = 1.0e300;
        for (color_index = 0u; color_index < palette_count; ++color_index) {
            dr = (double)pixels[pixel_index * 3u + 0u]
                - (double)palette[color_index * 3u + 0u];
            dg = (double)pixels[pixel_index * 3u + 1u]
                - (double)palette[color_index * 3u + 1u];
            db = (double)pixels[pixel_index * 3u + 2u]
                - (double)palette[color_index * 3u + 2u];
            distance = dr * dr + dg * dg + db * db;
            if (distance < best_distance) {
                best_distance = distance;
            }
        }
        if (best_distance > radius2) {
            radius2 = best_distance;
        }
    }

    return radius2;
}

static int
test_run_seed_reproducibility_case(sixel_kcenter_algo_t algo)
{
    sixel_allocator_t *allocator;
    unsigned char pixels[TEST_SEED_PIXELS * 3u];
    unsigned char *palette_a;
    unsigned char *palette_b;
    unsigned int ncolors_a;
    unsigned int ncolors_b;
    int ok;

    allocator = NULL;
    palette_a = NULL;
    palette_b = NULL;
    ncolors_a = 0u;
    ncolors_b = 0u;
    ok = 0;

    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL))) {
        return 0;
    }

    test_fill_seed_pixels(pixels, TEST_SEED_PIXELS);
    if (!test_build_kcenter_palette(pixels,
                                    TEST_SEED_WIDTH,
                                    TEST_SEED_HEIGHT,
                                    TEST_SEED_REQCOLORS,
                                    SIXEL_QUALITY_AUTO,
                                    algo,
                                    23u,
                                    2u,
                                    16u,
                                    6u,
                                    256u,
                                    1.000,
                                    &palette_a,
                                    &ncolors_a,
                                    allocator)) {
        goto end;
    }
    if (!test_build_kcenter_palette(pixels,
                                    TEST_SEED_WIDTH,
                                    TEST_SEED_HEIGHT,
                                    TEST_SEED_REQCOLORS,
                                    SIXEL_QUALITY_AUTO,
                                    algo,
                                    23u,
                                    2u,
                                    16u,
                                    6u,
                                    256u,
                                    1.000,
                                    &palette_b,
                                    &ncolors_b,
                                    allocator)) {
        goto end;
    }
    if (!test_palette_equal(palette_a, ncolors_a, palette_b, ncolors_b)) {
        goto end;
    }
    ok = 1;

end:
    if (palette_b != NULL) {
        sixel_allocator_free(allocator, palette_b);
    }
    if (palette_a != NULL) {
        sixel_allocator_free(allocator, palette_a);
    }
    sixel_allocator_unref(allocator);
    return ok;
}

static int
test_run_swap_monotonic_radius_case(void)
{
    sixel_allocator_t *allocator;
    unsigned char pixels[TEST_SWAP_PIXELS * 3u];
    unsigned char *palette_iter1;
    unsigned char *palette_iter4;
    unsigned char *palette_iter16;
    unsigned int ncolors_iter1;
    unsigned int ncolors_iter4;
    unsigned int ncolors_iter16;
    double radius_iter1;
    double radius_iter4;
    double radius_iter16;
    int ok;

    allocator = NULL;
    palette_iter1 = NULL;
    palette_iter4 = NULL;
    palette_iter16 = NULL;
    ncolors_iter1 = 0u;
    ncolors_iter4 = 0u;
    ncolors_iter16 = 0u;
    radius_iter1 = 0.0;
    radius_iter4 = 0.0;
    radius_iter16 = 0.0;
    ok = 0;

    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL))) {
        return 0;
    }

    test_fill_seed_pixels(pixels, TEST_SWAP_PIXELS);
    if (!test_build_kcenter_palette(pixels,
                                    TEST_SWAP_WIDTH,
                                    TEST_SWAP_HEIGHT,
                                    TEST_SWAP_REQCOLORS,
                                    SIXEL_QUALITY_FULL,
                                    SIXEL_PALETTE_KCENTER_ALGO_SWAP,
                                    7u,
                                    1u,
                                    1u,
                                    6u,
                                    512u,
                                    1.000,
                                    &palette_iter1,
                                    &ncolors_iter1,
                                    allocator)) {
        goto end;
    }
    if (!test_build_kcenter_palette(pixels,
                                    TEST_SWAP_WIDTH,
                                    TEST_SWAP_HEIGHT,
                                    TEST_SWAP_REQCOLORS,
                                    SIXEL_QUALITY_FULL,
                                    SIXEL_PALETTE_KCENTER_ALGO_SWAP,
                                    7u,
                                    1u,
                                    4u,
                                    6u,
                                    512u,
                                    1.000,
                                    &palette_iter4,
                                    &ncolors_iter4,
                                    allocator)) {
        goto end;
    }
    if (!test_build_kcenter_palette(pixels,
                                    TEST_SWAP_WIDTH,
                                    TEST_SWAP_HEIGHT,
                                    TEST_SWAP_REQCOLORS,
                                    SIXEL_QUALITY_FULL,
                                    SIXEL_PALETTE_KCENTER_ALGO_SWAP,
                                    7u,
                                    1u,
                                    16u,
                                    6u,
                                    512u,
                                    1.000,
                                    &palette_iter16,
                                    &ncolors_iter16,
                                    allocator)) {
        goto end;
    }

    radius_iter1 = test_palette_radius_sq(pixels,
                                          TEST_SWAP_PIXELS,
                                          palette_iter1,
                                          ncolors_iter1);
    radius_iter4 = test_palette_radius_sq(pixels,
                                          TEST_SWAP_PIXELS,
                                          palette_iter4,
                                          ncolors_iter4);
    radius_iter16 = test_palette_radius_sq(pixels,
                                           TEST_SWAP_PIXELS,
                                           palette_iter16,
                                           ncolors_iter16);
    if (radius_iter4 > radius_iter1 + 1.0e-9) {
        goto end;
    }
    if (radius_iter16 > radius_iter4 + 1.0e-9) {
        goto end;
    }
    ok = 1;

end:
    if (palette_iter16 != NULL) {
        sixel_allocator_free(allocator, palette_iter16);
    }
    if (palette_iter4 != NULL) {
        sixel_allocator_free(allocator, palette_iter4);
    }
    if (palette_iter1 != NULL) {
        sixel_allocator_free(allocator, palette_iter1);
    }
    sixel_allocator_unref(allocator);
    return ok;
}

static int
test_run_swap_update_consistency_case(void)
{
    sixel_allocator_t *allocator;
    unsigned char pixels[TEST_SWAP_PIXELS * 3u];
    unsigned char *palette_full;
    unsigned char *palette_incremental;
    unsigned int ncolors_full;
    unsigned int ncolors_incremental;
    unsigned int iter_cases[3];
    unsigned int topk_cases[3];
    unsigned int restart_cases[3];
    uint32_t seed_cases[3];
    unsigned int case_index;
    int ok;

    allocator = NULL;
    palette_full = NULL;
    palette_incremental = NULL;
    ncolors_full = 0u;
    ncolors_incremental = 0u;
    iter_cases[0] = 16u;
    iter_cases[1] = 48u;
    iter_cases[2] = 64u;
    topk_cases[0] = 4u;
    topk_cases[1] = 8u;
    topk_cases[2] = 12u;
    restart_cases[0] = 1u;
    restart_cases[1] = 2u;
    restart_cases[2] = 3u;
    seed_cases[0] = 7u;
    seed_cases[1] = 29u;
    seed_cases[2] = 61u;
    case_index = 0u;
    ok = 0;

    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL))) {
        return 0;
    }

    test_fill_seed_pixels(pixels, TEST_SWAP_PIXELS);
    for (case_index = 0u; case_index < 3u; ++case_index) {
        if (!test_build_kcenter_palette_with_options(
                pixels,
                TEST_SWAP_WIDTH,
                TEST_SWAP_HEIGHT,
                TEST_SWAP_REQCOLORS,
                SIXEL_QUALITY_FULL,
                SIXEL_PALETTE_KCENTER_ALGO_SWAP,
                seed_cases[case_index],
                restart_cases[case_index],
                iter_cases[case_index],
                6u,
                512u,
                1.000,
                0,
                SIXEL_PALETTE_KCENTER_PROFILE_LEGACY,
                0,
                SIXEL_PALETTE_KCENTER_AUTO_POLICY_LEGACY,
                0,
                2048u,
                0,
                SIXEL_PALETTE_KCENTER_CANDIDATE_POLICY_LEGACY,
                0,
                0u,
                0,
                SIXEL_PALETTE_KCENTER_BUDGET_POLICY_LEGACY,
                0,
                1.0,
                1,
                topk_cases[case_index],
                1,
                SIXEL_PALETTE_KCENTER_SWAP_UPDATE_FULL,
                0,
                0u,
                &palette_full,
                &ncolors_full,
                allocator)) {
            goto end;
        }
        if (!test_build_kcenter_palette_with_options(
                pixels,
                TEST_SWAP_WIDTH,
                TEST_SWAP_HEIGHT,
                TEST_SWAP_REQCOLORS,
                SIXEL_QUALITY_FULL,
                SIXEL_PALETTE_KCENTER_ALGO_SWAP,
                seed_cases[case_index],
                restart_cases[case_index],
                iter_cases[case_index],
                6u,
                512u,
                1.000,
                0,
                SIXEL_PALETTE_KCENTER_PROFILE_LEGACY,
                0,
                SIXEL_PALETTE_KCENTER_AUTO_POLICY_LEGACY,
                0,
                2048u,
                0,
                SIXEL_PALETTE_KCENTER_CANDIDATE_POLICY_LEGACY,
                0,
                0u,
                0,
                SIXEL_PALETTE_KCENTER_BUDGET_POLICY_LEGACY,
                0,
                1.0,
                1,
                topk_cases[case_index],
                1,
                SIXEL_PALETTE_KCENTER_SWAP_UPDATE_INCREMENTAL,
                0,
                0u,
                &palette_incremental,
                &ncolors_incremental,
                allocator)) {
            goto end;
        }
        if (!test_palette_equal(palette_full,
                                ncolors_full,
                                palette_incremental,
                                ncolors_incremental)) {
            goto end;
        }
        sixel_allocator_free(allocator, palette_incremental);
        palette_incremental = NULL;
        sixel_allocator_free(allocator, palette_full);
        palette_full = NULL;
    }
    ok = 1;

end:
    if (palette_incremental != NULL) {
        sixel_allocator_free(allocator, palette_incremental);
    }
    if (palette_full != NULL) {
        sixel_allocator_free(allocator, palette_full);
    }
    sixel_allocator_unref(allocator);
    return ok;
}

static int
test_run_swap_topk_monotonic_radius_case(void)
{
    sixel_allocator_t *allocator;
    unsigned char pixels[TEST_SWAP_PIXELS * 3u];
    unsigned char *palette_iter1;
    unsigned char *palette_iter4;
    unsigned char *palette_iter16;
    unsigned int ncolors_iter1;
    unsigned int ncolors_iter4;
    unsigned int ncolors_iter16;
    double radius_iter1;
    double radius_iter4;
    double radius_iter16;
    int ok;

    allocator = NULL;
    palette_iter1 = NULL;
    palette_iter4 = NULL;
    palette_iter16 = NULL;
    ncolors_iter1 = 0u;
    ncolors_iter4 = 0u;
    ncolors_iter16 = 0u;
    radius_iter1 = 0.0;
    radius_iter4 = 0.0;
    radius_iter16 = 0.0;
    ok = 0;

    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL))) {
        return 0;
    }

    test_fill_seed_pixels(pixels, TEST_SWAP_PIXELS);
    if (!test_build_kcenter_palette_with_options(
            pixels,
            TEST_SWAP_WIDTH,
            TEST_SWAP_HEIGHT,
            TEST_SWAP_REQCOLORS,
            SIXEL_QUALITY_FULL,
            SIXEL_PALETTE_KCENTER_ALGO_SWAP,
            7u,
            1u,
            1u,
            6u,
            512u,
            1.000,
            0,
            SIXEL_PALETTE_KCENTER_PROFILE_LEGACY,
            0,
            SIXEL_PALETTE_KCENTER_AUTO_POLICY_LEGACY,
            0,
            2048u,
            0,
            SIXEL_PALETTE_KCENTER_CANDIDATE_POLICY_LEGACY,
            0,
            0u,
            0,
            SIXEL_PALETTE_KCENTER_BUDGET_POLICY_LEGACY,
            0,
            1.0,
            1,
            4u,
            1,
            SIXEL_PALETTE_KCENTER_SWAP_UPDATE_INCREMENTAL,
            0,
            0u,
            &palette_iter1,
            &ncolors_iter1,
            allocator)) {
        goto end;
    }
    if (!test_build_kcenter_palette_with_options(
            pixels,
            TEST_SWAP_WIDTH,
            TEST_SWAP_HEIGHT,
            TEST_SWAP_REQCOLORS,
            SIXEL_QUALITY_FULL,
            SIXEL_PALETTE_KCENTER_ALGO_SWAP,
            7u,
            1u,
            4u,
            6u,
            512u,
            1.000,
            0,
            SIXEL_PALETTE_KCENTER_PROFILE_LEGACY,
            0,
            SIXEL_PALETTE_KCENTER_AUTO_POLICY_LEGACY,
            0,
            2048u,
            0,
            SIXEL_PALETTE_KCENTER_CANDIDATE_POLICY_LEGACY,
            0,
            0u,
            0,
            SIXEL_PALETTE_KCENTER_BUDGET_POLICY_LEGACY,
            0,
            1.0,
            1,
            4u,
            1,
            SIXEL_PALETTE_KCENTER_SWAP_UPDATE_INCREMENTAL,
            0,
            0u,
            &palette_iter4,
            &ncolors_iter4,
            allocator)) {
        goto end;
    }
    if (!test_build_kcenter_palette_with_options(
            pixels,
            TEST_SWAP_WIDTH,
            TEST_SWAP_HEIGHT,
            TEST_SWAP_REQCOLORS,
            SIXEL_QUALITY_FULL,
            SIXEL_PALETTE_KCENTER_ALGO_SWAP,
            7u,
            1u,
            16u,
            6u,
            512u,
            1.000,
            0,
            SIXEL_PALETTE_KCENTER_PROFILE_LEGACY,
            0,
            SIXEL_PALETTE_KCENTER_AUTO_POLICY_LEGACY,
            0,
            2048u,
            0,
            SIXEL_PALETTE_KCENTER_CANDIDATE_POLICY_LEGACY,
            0,
            0u,
            0,
            SIXEL_PALETTE_KCENTER_BUDGET_POLICY_LEGACY,
            0,
            1.0,
            1,
            4u,
            1,
            SIXEL_PALETTE_KCENTER_SWAP_UPDATE_INCREMENTAL,
            0,
            0u,
            &palette_iter16,
            &ncolors_iter16,
            allocator)) {
        goto end;
    }

    radius_iter1 = test_palette_radius_sq(pixels,
                                          TEST_SWAP_PIXELS,
                                          palette_iter1,
                                          ncolors_iter1);
    radius_iter4 = test_palette_radius_sq(pixels,
                                          TEST_SWAP_PIXELS,
                                          palette_iter4,
                                          ncolors_iter4);
    radius_iter16 = test_palette_radius_sq(pixels,
                                           TEST_SWAP_PIXELS,
                                           palette_iter16,
                                           ncolors_iter16);
    if (radius_iter4 > radius_iter1 + 1.0e-9) {
        goto end;
    }
    if (radius_iter16 > radius_iter4 + 1.0e-9) {
        goto end;
    }
    ok = 1;

end:
    if (palette_iter16 != NULL) {
        sixel_allocator_free(allocator, palette_iter16);
    }
    if (palette_iter4 != NULL) {
        sixel_allocator_free(allocator, palette_iter4);
    }
    if (palette_iter1 != NULL) {
        sixel_allocator_free(allocator, palette_iter1);
    }
    sixel_allocator_unref(allocator);
    return ok;
}

static int
test_run_auto_strategy_selection_case(void)
{
    sixel_allocator_t *allocator;
    unsigned char pixels[TEST_AUTO_PIXELS * 3u];
    unsigned char *low_auto_palette;
    unsigned char *low_fft_palette;
    unsigned char *full_auto_palette;
    unsigned char *full_hybrid_palette;
    unsigned int low_auto_colors;
    unsigned int low_fft_colors;
    unsigned int full_auto_colors;
    unsigned int full_hybrid_colors;
    int ok;

    allocator = NULL;
    low_auto_palette = NULL;
    low_fft_palette = NULL;
    full_auto_palette = NULL;
    full_hybrid_palette = NULL;
    low_auto_colors = 0u;
    low_fft_colors = 0u;
    full_auto_colors = 0u;
    full_hybrid_colors = 0u;
    ok = 0;

    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL))) {
        return 0;
    }

    test_fill_auto_pixels(pixels);
    if (!test_build_kcenter_palette(pixels,
                                    TEST_AUTO_WIDTH,
                                    TEST_AUTO_HEIGHT,
                                    TEST_AUTO_REQCOLORS,
                                    SIXEL_QUALITY_LOW,
                                    SIXEL_PALETTE_KCENTER_ALGO_AUTO,
                                    41u,
                                    1u,
                                    16u,
                                    6u,
                                    4096u,
                                    1.000,
                                    &low_auto_palette,
                                    &low_auto_colors,
                                    allocator)) {
        goto end;
    }
    if (!test_build_kcenter_palette(pixels,
                                    TEST_AUTO_WIDTH,
                                    TEST_AUTO_HEIGHT,
                                    TEST_AUTO_REQCOLORS,
                                    SIXEL_QUALITY_LOW,
                                    SIXEL_PALETTE_KCENTER_ALGO_FFT,
                                    41u,
                                    1u,
                                    16u,
                                    6u,
                                    4096u,
                                    1.000,
                                    &low_fft_palette,
                                    &low_fft_colors,
                                    allocator)) {
        goto end;
    }
    if (!test_build_kcenter_palette(pixels,
                                    TEST_AUTO_WIDTH,
                                    TEST_AUTO_HEIGHT,
                                    TEST_AUTO_REQCOLORS,
                                    SIXEL_QUALITY_FULL,
                                    SIXEL_PALETTE_KCENTER_ALGO_AUTO,
                                    41u,
                                    1u,
                                    16u,
                                    6u,
                                    4096u,
                                    1.000,
                                    &full_auto_palette,
                                    &full_auto_colors,
                                    allocator)) {
        goto end;
    }
    if (!test_build_kcenter_palette(pixels,
                                    TEST_AUTO_WIDTH,
                                    TEST_AUTO_HEIGHT,
                                    TEST_AUTO_REQCOLORS,
                                    SIXEL_QUALITY_FULL,
                                    SIXEL_PALETTE_KCENTER_ALGO_HYBRID,
                                    41u,
                                    1u,
                                    16u,
                                    6u,
                                    4096u,
                                    1.000,
                                    &full_hybrid_palette,
                                    &full_hybrid_colors,
                                    allocator)) {
        goto end;
    }

    if (!test_palette_equal(low_auto_palette,
                            low_auto_colors,
                            low_fft_palette,
                            low_fft_colors)) {
        goto end;
    }
    if (!test_palette_equal(full_auto_palette,
                            full_auto_colors,
                            full_hybrid_palette,
                            full_hybrid_colors)) {
        goto end;
    }
    ok = 1;

end:
    if (full_hybrid_palette != NULL) {
        sixel_allocator_free(allocator, full_hybrid_palette);
    }
    if (full_auto_palette != NULL) {
        sixel_allocator_free(allocator, full_auto_palette);
    }
    if (low_fft_palette != NULL) {
        sixel_allocator_free(allocator, low_fft_palette);
    }
    if (low_auto_palette != NULL) {
        sixel_allocator_free(allocator, low_auto_palette);
    }
    sixel_allocator_unref(allocator);
    return ok;
}

static int
test_run_auto_policy_adaptive_selection_case(void)
{
    sixel_allocator_t *allocator;
    unsigned char pixels[TEST_AUTO_PIXELS * 3u];
    unsigned char *low_auto_palette;
    unsigned char *low_fft_palette;
    unsigned char *full_auto_palette;
    unsigned char *full_hybrid_palette;
    unsigned int low_auto_colors;
    unsigned int low_fft_colors;
    unsigned int full_auto_colors;
    unsigned int full_hybrid_colors;
    int ok;

    allocator = NULL;
    low_auto_palette = NULL;
    low_fft_palette = NULL;
    full_auto_palette = NULL;
    full_hybrid_palette = NULL;
    low_auto_colors = 0u;
    low_fft_colors = 0u;
    full_auto_colors = 0u;
    full_hybrid_colors = 0u;
    ok = 0;

    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL))) {
        return 0;
    }

    test_fill_auto_pixels(pixels);
    if (!test_build_kcenter_palette_with_options(
            pixels,
            TEST_AUTO_WIDTH,
            TEST_AUTO_HEIGHT,
            TEST_AUTO_REQCOLORS,
            SIXEL_QUALITY_LOW,
            SIXEL_PALETTE_KCENTER_ALGO_AUTO,
            41u,
            1u,
            16u,
            6u,
            4096u,
            1.000,
            0,
            SIXEL_PALETTE_KCENTER_PROFILE_LEGACY,
            1,
            SIXEL_PALETTE_KCENTER_AUTO_POLICY_ADAPTIVE,
            1,
            1024u,
            0,
            SIXEL_PALETTE_KCENTER_CANDIDATE_POLICY_LEGACY,
            0,
            0u,
            0,
            SIXEL_PALETTE_KCENTER_BUDGET_POLICY_LEGACY,
            0,
            1.0,
            0,
            1u,
            0,
            SIXEL_PALETTE_KCENTER_SWAP_UPDATE_FULL,
            0,
            0u,
            &low_auto_palette,
            &low_auto_colors,
            allocator)) {
        goto end;
    }
    if (!test_build_kcenter_palette(pixels,
                                    TEST_AUTO_WIDTH,
                                    TEST_AUTO_HEIGHT,
                                    TEST_AUTO_REQCOLORS,
                                    SIXEL_QUALITY_LOW,
                                    SIXEL_PALETTE_KCENTER_ALGO_FFT,
                                    41u,
                                    1u,
                                    16u,
                                    6u,
                                    4096u,
                                    1.000,
                                    &low_fft_palette,
                                    &low_fft_colors,
                                    allocator)) {
        goto end;
    }
    if (!test_build_kcenter_palette_with_options(
            pixels,
            TEST_AUTO_WIDTH,
            TEST_AUTO_HEIGHT,
            TEST_AUTO_REQCOLORS,
            SIXEL_QUALITY_FULL,
            SIXEL_PALETTE_KCENTER_ALGO_AUTO,
            41u,
            1u,
            16u,
            6u,
            4096u,
            1.000,
            0,
            SIXEL_PALETTE_KCENTER_PROFILE_LEGACY,
            1,
            SIXEL_PALETTE_KCENTER_AUTO_POLICY_ADAPTIVE,
            1,
            16384u,
            0,
            SIXEL_PALETTE_KCENTER_CANDIDATE_POLICY_LEGACY,
            0,
            0u,
            0,
            SIXEL_PALETTE_KCENTER_BUDGET_POLICY_LEGACY,
            0,
            1.0,
            0,
            1u,
            0,
            SIXEL_PALETTE_KCENTER_SWAP_UPDATE_FULL,
            0,
            0u,
            &full_auto_palette,
            &full_auto_colors,
            allocator)) {
        goto end;
    }
    if (!test_build_kcenter_palette(pixels,
                                    TEST_AUTO_WIDTH,
                                    TEST_AUTO_HEIGHT,
                                    TEST_AUTO_REQCOLORS,
                                    SIXEL_QUALITY_FULL,
                                    SIXEL_PALETTE_KCENTER_ALGO_HYBRID,
                                    41u,
                                    1u,
                                    16u,
                                    6u,
                                    4096u,
                                    1.000,
                                    &full_hybrid_palette,
                                    &full_hybrid_colors,
                                    allocator)) {
        goto end;
    }

    if (!test_palette_equal(low_auto_palette,
                            low_auto_colors,
                            low_fft_palette,
                            low_fft_colors)) {
        goto end;
    }
    if (!test_palette_equal(full_auto_palette,
                            full_auto_colors,
                            full_hybrid_palette,
                            full_hybrid_colors)) {
        goto end;
    }
    ok = 1;

end:
    if (full_hybrid_palette != NULL) {
        sixel_allocator_free(allocator, full_hybrid_palette);
    }
    if (full_auto_palette != NULL) {
        sixel_allocator_free(allocator, full_auto_palette);
    }
    if (low_fft_palette != NULL) {
        sixel_allocator_free(allocator, low_fft_palette);
    }
    if (low_auto_palette != NULL) {
        sixel_allocator_free(allocator, low_auto_palette);
    }
    sixel_allocator_unref(allocator);
    return ok;
}

static int
test_run_profile_legacy_compatibility_case(void)
{
    sixel_allocator_t *allocator;
    unsigned char pixels[TEST_SEED_PIXELS * 3u];
    unsigned char *legacy_profile_palette;
    unsigned char *baseline_palette;
    unsigned int legacy_profile_colors;
    unsigned int baseline_colors;
    int ok;

    allocator = NULL;
    legacy_profile_palette = NULL;
    baseline_palette = NULL;
    legacy_profile_colors = 0u;
    baseline_colors = 0u;
    ok = 0;

    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL))) {
        return 0;
    }

    test_fill_seed_pixels(pixels, TEST_SEED_PIXELS);
    if (!test_build_kcenter_palette(pixels,
                                    TEST_SEED_WIDTH,
                                    TEST_SEED_HEIGHT,
                                    TEST_SEED_REQCOLORS,
                                    SIXEL_QUALITY_AUTO,
                                    SIXEL_PALETTE_KCENTER_ALGO_HYBRID,
                                    23u,
                                    2u,
                                    16u,
                                    6u,
                                    256u,
                                    1.000,
                                    &baseline_palette,
                                    &baseline_colors,
                                    allocator)) {
        goto end;
    }
    if (!test_build_kcenter_palette_with_options(
            pixels,
            TEST_SEED_WIDTH,
            TEST_SEED_HEIGHT,
            TEST_SEED_REQCOLORS,
            SIXEL_QUALITY_AUTO,
            SIXEL_PALETTE_KCENTER_ALGO_HYBRID,
            23u,
            2u,
            16u,
            6u,
            256u,
            1.000,
            1,
            SIXEL_PALETTE_KCENTER_PROFILE_LEGACY,
            1,
            SIXEL_PALETTE_KCENTER_AUTO_POLICY_LEGACY,
            1,
            2048u,
            1,
            SIXEL_PALETTE_KCENTER_CANDIDATE_POLICY_LEGACY,
            1,
            0u,
            1,
            SIXEL_PALETTE_KCENTER_BUDGET_POLICY_LEGACY,
            1,
            1.0,
            1,
            1u,
            1,
            SIXEL_PALETTE_KCENTER_SWAP_UPDATE_FULL,
            1,
            0u,
            &legacy_profile_palette,
            &legacy_profile_colors,
            allocator)) {
        goto end;
    }
    if (!test_palette_equal(baseline_palette,
                            baseline_colors,
                            legacy_profile_palette,
                            legacy_profile_colors)) {
        goto end;
    }
    ok = 1;

end:
    if (baseline_palette != NULL) {
        sixel_allocator_free(allocator, baseline_palette);
    }
    if (legacy_profile_palette != NULL) {
        sixel_allocator_free(allocator, legacy_profile_palette);
    }
    sixel_allocator_unref(allocator);
    return ok;
}

static int
test_run_init_seeds_nonworsening_case(void)
{
    sixel_allocator_t *allocator;
    unsigned char pixels[TEST_SWAP_PIXELS * 3u];
    unsigned char *palette_seed1;
    unsigned char *palette_seed4;
    unsigned int ncolors_seed1;
    unsigned int ncolors_seed4;
    double radius_seed1;
    double radius_seed4;
    int ok;

    allocator = NULL;
    palette_seed1 = NULL;
    palette_seed4 = NULL;
    ncolors_seed1 = 0u;
    ncolors_seed4 = 0u;
    radius_seed1 = 0.0;
    radius_seed4 = 0.0;
    ok = 0;

    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL))) {
        return 0;
    }

    test_fill_seed_pixels(pixels, TEST_SWAP_PIXELS);
    sixel_set_kcenter_init_seeds_override(1, 1u);
    if (!test_build_kcenter_palette(pixels,
                                    TEST_SWAP_WIDTH,
                                    TEST_SWAP_HEIGHT,
                                    TEST_SWAP_REQCOLORS,
                                    SIXEL_QUALITY_FULL,
                                    SIXEL_PALETTE_KCENTER_ALGO_HYBRID,
                                    13u,
                                    1u,
                                    16u,
                                    6u,
                                    512u,
                                    1.000,
                                    &palette_seed1,
                                    &ncolors_seed1,
                                    allocator)) {
        goto end;
    }

    sixel_set_kcenter_init_seeds_override(1, 4u);
    if (!test_build_kcenter_palette(pixels,
                                    TEST_SWAP_WIDTH,
                                    TEST_SWAP_HEIGHT,
                                    TEST_SWAP_REQCOLORS,
                                    SIXEL_QUALITY_FULL,
                                    SIXEL_PALETTE_KCENTER_ALGO_HYBRID,
                                    13u,
                                    1u,
                                    16u,
                                    6u,
                                    512u,
                                    1.000,
                                    &palette_seed4,
                                    &ncolors_seed4,
                                    allocator)) {
        goto end;
    }

    radius_seed1 = test_palette_radius_sq(pixels,
                                          TEST_SWAP_PIXELS,
                                          palette_seed1,
                                          ncolors_seed1);
    radius_seed4 = test_palette_radius_sq(pixels,
                                          TEST_SWAP_PIXELS,
                                          palette_seed4,
                                          ncolors_seed4);
    if (radius_seed4 > radius_seed1 + 1.0e-9) {
        goto end;
    }
    ok = 1;

end:
    if (palette_seed4 != NULL) {
        sixel_allocator_free(allocator, palette_seed4);
    }
    if (palette_seed1 != NULL) {
        sixel_allocator_free(allocator, palette_seed1);
    }
    sixel_allocator_unref(allocator);
    return ok;
}

static int
test_run_perceptual_strata_deterministic_case(void)
{
    sixel_allocator_t *allocator;
    float pixels[TEST_AUTO_PIXELS * 3u];
    unsigned char *palette_a;
    unsigned char *palette_b;
    unsigned int ncolors_a;
    unsigned int ncolors_b;
    int ok;

    allocator = NULL;
    palette_a = NULL;
    palette_b = NULL;
    ncolors_a = 0u;
    ncolors_b = 0u;
    ok = 0;

    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL))) {
        return 0;
    }

    test_fill_oklab_pixels(pixels);
    sixel_set_kcenter_space_policy_override(
        1,
        SIXEL_PALETTE_KCENTER_SPACE_POLICY_PERCEPTUAL);
    if (!test_build_kcenter_palette_with_options_format(
            (unsigned char const *)pixels,
            TEST_AUTO_WIDTH,
            TEST_AUTO_HEIGHT,
            TEST_AUTO_REQCOLORS,
            SIXEL_QUALITY_FULL,
            SIXEL_PALETTE_KCENTER_ALGO_HYBRID,
            97u,
            1u,
            16u,
            6u,
            4096u,
            1.000,
            1,
            SIXEL_PALETTE_KCENTER_PROFILE_BALANCE,
            1,
            SIXEL_PALETTE_KCENTER_AUTO_POLICY_ADAPTIVE,
            1,
            1024u,
            1,
            SIXEL_PALETTE_KCENTER_CANDIDATE_POLICY_HYBRID,
            1,
            32u,
            1,
            SIXEL_PALETTE_KCENTER_BUDGET_POLICY_ADAPTIVE,
            1,
            1.0,
            1,
            4u,
            1,
            SIXEL_PALETTE_KCENTER_SWAP_UPDATE_INCREMENTAL,
            1,
            2u,
            SIXEL_PIXELFORMAT_OKLABFLOAT32,
            1,
            &palette_a,
            &ncolors_a,
            allocator)) {
        goto end;
    }
    sixel_set_kcenter_space_policy_override(
        1,
        SIXEL_PALETTE_KCENTER_SPACE_POLICY_PERCEPTUAL);
    if (!test_build_kcenter_palette_with_options_format(
            (unsigned char const *)pixels,
            TEST_AUTO_WIDTH,
            TEST_AUTO_HEIGHT,
            TEST_AUTO_REQCOLORS,
            SIXEL_QUALITY_FULL,
            SIXEL_PALETTE_KCENTER_ALGO_HYBRID,
            97u,
            1u,
            16u,
            6u,
            4096u,
            1.000,
            1,
            SIXEL_PALETTE_KCENTER_PROFILE_BALANCE,
            1,
            SIXEL_PALETTE_KCENTER_AUTO_POLICY_ADAPTIVE,
            1,
            1024u,
            1,
            SIXEL_PALETTE_KCENTER_CANDIDATE_POLICY_HYBRID,
            1,
            32u,
            1,
            SIXEL_PALETTE_KCENTER_BUDGET_POLICY_ADAPTIVE,
            1,
            1.0,
            1,
            4u,
            1,
            SIXEL_PALETTE_KCENTER_SWAP_UPDATE_INCREMENTAL,
            1,
            2u,
            SIXEL_PIXELFORMAT_OKLABFLOAT32,
            1,
            &palette_b,
            &ncolors_b,
            allocator)) {
        goto end;
    }
    if (!test_palette_equal(palette_a, ncolors_a, palette_b, ncolors_b)) {
        goto end;
    }
    ok = 1;

end:
    if (palette_b != NULL) {
        sixel_allocator_free(allocator, palette_b);
    }
    if (palette_a != NULL) {
        sixel_allocator_free(allocator, palette_a);
    }
    sixel_allocator_unref(allocator);
    return ok;
}

static int
test_run_radius_constrained_sse_polish_case(void)
{
    sixel_allocator_t *allocator;
    float pixels[TEST_AUTO_PIXELS * 3u];
    unsigned char *palette;
    unsigned int ncolors;
    double pre_radius2;
    double post_radius2;
    int polish_applied;
    int ok;

    allocator = NULL;
    palette = NULL;
    ncolors = 0u;
    pre_radius2 = 0.0;
    post_radius2 = 0.0;
    polish_applied = 0;
    ok = 0;

    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL))) {
        return 0;
    }

    test_fill_oklab_pixels(pixels);
    sixel_set_kcenter_space_policy_override(
        1,
        SIXEL_PALETTE_KCENTER_SPACE_POLICY_PERCEPTUAL);
    if (!test_build_kcenter_palette_with_options_format(
            (unsigned char const *)pixels,
            TEST_AUTO_WIDTH,
            TEST_AUTO_HEIGHT,
            TEST_AUTO_REQCOLORS,
            SIXEL_QUALITY_FULL,
            SIXEL_PALETTE_KCENTER_ALGO_HYBRID,
            111u,
            1u,
            16u,
            6u,
            4096u,
            1.000,
            1,
            SIXEL_PALETTE_KCENTER_PROFILE_QUALITY,
            1,
            SIXEL_PALETTE_KCENTER_AUTO_POLICY_ADAPTIVE,
            1,
            1024u,
            1,
            SIXEL_PALETTE_KCENTER_CANDIDATE_POLICY_HYBRID,
            1,
            64u,
            1,
            SIXEL_PALETTE_KCENTER_BUDGET_POLICY_ADAPTIVE,
            1,
            1.0,
            1,
            6u,
            1,
            SIXEL_PALETTE_KCENTER_SWAP_UPDATE_INCREMENTAL,
            1,
            2u,
            SIXEL_PIXELFORMAT_OKLABFLOAT32,
            1,
            &palette,
            &ncolors,
            allocator)) {
        goto end;
    }

    polish_applied = sixel_get_kcenter_last_polish_applied();
    pre_radius2 = sixel_get_kcenter_last_polish_pre_radius2();
    post_radius2 = sixel_get_kcenter_last_polish_post_radius2();
    if (polish_applied == 0) {
        goto end;
    }
    if (post_radius2 > pre_radius2 + 1.0e-12) {
        goto end;
    }
    ok = 1;

end:
    if (palette != NULL) {
        sixel_allocator_free(allocator, palette);
    }
    sixel_allocator_unref(allocator);
    return ok;
}

static int
test_run_auto_perceptual_oklab_hybrid_preference_case(void)
{
    sixel_allocator_t *allocator;
    float pixels[TEST_AUTO_PIXELS * 3u];
    unsigned char *auto_perceptual_palette;
    unsigned char *auto_legacy_palette;
    unsigned char *hybrid_palette;
    unsigned char *fft_palette;
    unsigned int auto_perceptual_colors;
    unsigned int auto_legacy_colors;
    unsigned int hybrid_colors;
    unsigned int fft_colors;
    int ok;

    allocator = NULL;
    auto_perceptual_palette = NULL;
    auto_legacy_palette = NULL;
    hybrid_palette = NULL;
    fft_palette = NULL;
    auto_perceptual_colors = 0u;
    auto_legacy_colors = 0u;
    hybrid_colors = 0u;
    fft_colors = 0u;
    ok = 0;

    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL))) {
        return 0;
    }

    test_fill_oklab_pixels(pixels);
    sixel_set_kcenter_space_policy_override(
        1,
        SIXEL_PALETTE_KCENTER_SPACE_POLICY_PERCEPTUAL);
    if (!test_build_kcenter_palette_with_options_format(
            (unsigned char const *)pixels,
            TEST_AUTO_WIDTH,
            TEST_AUTO_HEIGHT,
            TEST_AUTO_REQCOLORS,
            SIXEL_QUALITY_AUTO,
            SIXEL_PALETTE_KCENTER_ALGO_AUTO,
            41u,
            1u,
            16u,
            6u,
            4096u,
            1.000,
            1,
            SIXEL_PALETTE_KCENTER_PROFILE_BALANCE,
            1,
            SIXEL_PALETTE_KCENTER_AUTO_POLICY_ADAPTIVE,
            1,
            1024u,
            1,
            SIXEL_PALETTE_KCENTER_CANDIDATE_POLICY_HYBRID,
            1,
            32u,
            1,
            SIXEL_PALETTE_KCENTER_BUDGET_POLICY_ADAPTIVE,
            1,
            1.0,
            1,
            4u,
            1,
            SIXEL_PALETTE_KCENTER_SWAP_UPDATE_INCREMENTAL,
            1,
            2u,
            SIXEL_PIXELFORMAT_OKLABFLOAT32,
            1,
            &auto_perceptual_palette,
            &auto_perceptual_colors,
            allocator)) {
        goto end;
    }
    sixel_set_kcenter_space_policy_override(
        1,
        SIXEL_PALETTE_KCENTER_SPACE_POLICY_PERCEPTUAL);
    if (!test_build_kcenter_palette_with_options_format(
            (unsigned char const *)pixels,
            TEST_AUTO_WIDTH,
            TEST_AUTO_HEIGHT,
            TEST_AUTO_REQCOLORS,
            SIXEL_QUALITY_AUTO,
            SIXEL_PALETTE_KCENTER_ALGO_HYBRID,
            41u,
            1u,
            16u,
            6u,
            4096u,
            1.000,
            1,
            SIXEL_PALETTE_KCENTER_PROFILE_BALANCE,
            1,
            SIXEL_PALETTE_KCENTER_AUTO_POLICY_ADAPTIVE,
            1,
            1024u,
            1,
            SIXEL_PALETTE_KCENTER_CANDIDATE_POLICY_HYBRID,
            1,
            32u,
            1,
            SIXEL_PALETTE_KCENTER_BUDGET_POLICY_ADAPTIVE,
            1,
            1.0,
            1,
            4u,
            1,
            SIXEL_PALETTE_KCENTER_SWAP_UPDATE_INCREMENTAL,
            1,
            2u,
            SIXEL_PIXELFORMAT_OKLABFLOAT32,
            1,
            &hybrid_palette,
            &hybrid_colors,
            allocator)) {
        goto end;
    }
    if (!test_palette_equal(auto_perceptual_palette,
                            auto_perceptual_colors,
                            hybrid_palette,
                            hybrid_colors)) {
        goto end;
    }

    sixel_set_kcenter_space_policy_override(
        1,
        SIXEL_PALETTE_KCENTER_SPACE_POLICY_LEGACY);
    if (!test_build_kcenter_palette_with_options_format(
            (unsigned char const *)pixels,
            TEST_AUTO_WIDTH,
            TEST_AUTO_HEIGHT,
            TEST_AUTO_REQCOLORS,
            SIXEL_QUALITY_AUTO,
            SIXEL_PALETTE_KCENTER_ALGO_AUTO,
            41u,
            1u,
            16u,
            6u,
            4096u,
            1.000,
            1,
            SIXEL_PALETTE_KCENTER_PROFILE_BALANCE,
            1,
            SIXEL_PALETTE_KCENTER_AUTO_POLICY_ADAPTIVE,
            1,
            1024u,
            1,
            SIXEL_PALETTE_KCENTER_CANDIDATE_POLICY_HYBRID,
            1,
            32u,
            1,
            SIXEL_PALETTE_KCENTER_BUDGET_POLICY_ADAPTIVE,
            1,
            1.0,
            1,
            4u,
            1,
            SIXEL_PALETTE_KCENTER_SWAP_UPDATE_INCREMENTAL,
            1,
            2u,
            SIXEL_PIXELFORMAT_OKLABFLOAT32,
            1,
            &auto_legacy_palette,
            &auto_legacy_colors,
            allocator)) {
        goto end;
    }
    sixel_set_kcenter_space_policy_override(
        1,
        SIXEL_PALETTE_KCENTER_SPACE_POLICY_LEGACY);
    if (!test_build_kcenter_palette_with_options_format(
            (unsigned char const *)pixels,
            TEST_AUTO_WIDTH,
            TEST_AUTO_HEIGHT,
            TEST_AUTO_REQCOLORS,
            SIXEL_QUALITY_AUTO,
            SIXEL_PALETTE_KCENTER_ALGO_FFT,
            41u,
            1u,
            16u,
            6u,
            4096u,
            1.000,
            1,
            SIXEL_PALETTE_KCENTER_PROFILE_BALANCE,
            1,
            SIXEL_PALETTE_KCENTER_AUTO_POLICY_ADAPTIVE,
            1,
            1024u,
            1,
            SIXEL_PALETTE_KCENTER_CANDIDATE_POLICY_HYBRID,
            1,
            32u,
            1,
            SIXEL_PALETTE_KCENTER_BUDGET_POLICY_ADAPTIVE,
            1,
            1.0,
            1,
            4u,
            1,
            SIXEL_PALETTE_KCENTER_SWAP_UPDATE_INCREMENTAL,
            1,
            2u,
            SIXEL_PIXELFORMAT_OKLABFLOAT32,
            1,
            &fft_palette,
            &fft_colors,
            allocator)) {
        goto end;
    }
    if (!test_palette_equal(auto_legacy_palette,
                            auto_legacy_colors,
                            fft_palette,
                            fft_colors)) {
        goto end;
    }
    ok = 1;

end:
    if (fft_palette != NULL) {
        sixel_allocator_free(allocator, fft_palette);
    }
    if (auto_legacy_palette != NULL) {
        sixel_allocator_free(allocator, auto_legacy_palette);
    }
    if (hybrid_palette != NULL) {
        sixel_allocator_free(allocator, hybrid_palette);
    }
    if (auto_perceptual_palette != NULL) {
        sixel_allocator_free(allocator, auto_perceptual_palette);
    }
    sixel_allocator_unref(allocator);
    return ok;
}

int
test_palette_0003_kcenter_constraints(int argc, char **argv)
{
    if (argc < 2) {
        return 1;
    }
    if (strcmp(argv[1], "seed-fft") == 0) {
        return test_run_seed_reproducibility_case(
            SIXEL_PALETTE_KCENTER_ALGO_FFT) ? 0 : 1;
    }
    if (strcmp(argv[1], "seed-swap") == 0) {
        return test_run_seed_reproducibility_case(
            SIXEL_PALETTE_KCENTER_ALGO_SWAP) ? 0 : 1;
    }
    if (strcmp(argv[1], "seed-hybrid") == 0) {
        return test_run_seed_reproducibility_case(
            SIXEL_PALETTE_KCENTER_ALGO_HYBRID) ? 0 : 1;
    }
    if (strcmp(argv[1], "swap-monotonic-radius") == 0) {
        return test_run_swap_monotonic_radius_case() ? 0 : 1;
    }
    if (strcmp(argv[1], "swap-update-consistency") == 0) {
        return test_run_swap_update_consistency_case() ? 0 : 1;
    }
    if (strcmp(argv[1], "swap-topk-monotonic-radius") == 0) {
        return test_run_swap_topk_monotonic_radius_case() ? 0 : 1;
    }
    if (strcmp(argv[1], "auto-strategy-selection") == 0) {
        return test_run_auto_strategy_selection_case() ? 0 : 1;
    }
    if (strcmp(argv[1], "auto-policy-adaptive-selection") == 0) {
        return test_run_auto_policy_adaptive_selection_case() ? 0 : 1;
    }
    if (strcmp(argv[1], "profile-legacy-compatibility") == 0) {
        return test_run_profile_legacy_compatibility_case() ? 0 : 1;
    }
    if (strcmp(argv[1], "init-seeds-nonworsening") == 0) {
        return test_run_init_seeds_nonworsening_case() ? 0 : 1;
    }
    if (strcmp(argv[1], "perceptual-strata-deterministic") == 0) {
        return test_run_perceptual_strata_deterministic_case() ? 0 : 1;
    }
    if (strcmp(argv[1], "radius-constrained-sse-polish") == 0) {
        return test_run_radius_constrained_sse_polish_case() ? 0 : 1;
    }
    if (strcmp(argv[1], "auto-perceptual-oklab-hybrid-preference") == 0) {
        return test_run_auto_perceptual_oklab_hybrid_preference_case()
            ? 0 : 1;
    }

    fprintf(stderr, "unknown kcenter test case: %s\n", argv[1]);
    return 1;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
