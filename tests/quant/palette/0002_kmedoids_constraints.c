/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 *
 * Verify k-medoids palette constraints from TAP wrappers:
 * - medoid palette entries must be sampled from input pixels
 * - seeded randomized variants must keep deterministic output
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>

#include <sixel.h>

#include "src/dither.h"
#include "src/palette-kmedoids.h"

#define TEST_PIXEL_COUNT 12u
#define TEST_WIDTH 12
#define TEST_HEIGHT 1
#define TEST_REQCOLORS 4

SIXELSTATUS
sixel_kmedoids_test_pick_sample_indices(unsigned int point_count,
                                        unsigned int sample_size,
                                        uint32_t seed,
                                        unsigned int **indices_out,
                                        sixel_allocator_t *allocator);

void
sixel_kmedoids_test_assign_points(double const *points,
                                  double const *weights,
                                  unsigned int point_count,
                                  unsigned int const *medoids,
                                  unsigned int k,
                                  unsigned int *nearest_slot,
                                  double *nearest_dist,
                                  double *second_dist,
                                  unsigned int *second_slot,
                                  double *cost_out);

void
sixel_kmedoids_test_update_assignments_after_swap(
    double const *points,
    double const *weights,
    unsigned int point_count,
    unsigned int const *medoids,
    unsigned int k,
    unsigned int swapped_slot,
    unsigned int new_medoid,
    unsigned int *nearest_slot,
    double *nearest_dist,
    double *second_dist,
    unsigned int *second_slot,
    double *cost_out);

SIXELSTATUS
sixel_kmedoids_test_build_clarans_guided_sets(
    double const *weights,
    unsigned int point_count,
    unsigned int k,
    unsigned int const *nearest_slot,
    double const *nearest_dist,
    unsigned char const *flags,
    unsigned int hot_point_limit,
    unsigned int hot_slot_limit,
    unsigned int *hot_points,
    unsigned int *hot_point_count_out,
    unsigned int *hot_slots,
    unsigned int *hot_slot_count_out,
    sixel_allocator_t *allocator);

SIXELSTATUS
sixel_kmedoids_test_collect_samples(
    unsigned char const *data,
    unsigned int length,
    unsigned int channels,
    unsigned int pixel_stride,
    int input_is_float32,
    unsigned int histbits,
    unsigned int point_budget,
    unsigned int rare_keep,
    double prune_mass,
    uint32_t seed,
    double **samples_out,
    double **sample_weights_out,
    unsigned int *sample_count_out,
    unsigned int *visible_count_out,
    sixel_allocator_t *allocator);

double
sixel_kmedoids_test_swap_cost_cutoff_with_row(
    double const *points,
    double const *weights,
    unsigned int point_count,
    unsigned int const *nearest_slot,
    double const *nearest_dist,
    double const *second_dist,
    unsigned int replace_slot,
    unsigned int candidate_point,
    double *candidate_dist_row,
    unsigned int const *order,
    double cutoff,
    int *early_stop_out);

SIXELSTATUS
sixel_kmedoids_test_build_eval_order_residual(
    double const *weights,
    double const *nearest_dist,
    unsigned int point_count,
    unsigned int *order_out,
    sixel_allocator_t *allocator);

SIXELSTATUS
sixel_kmedoids_test_apply_eval_order_delta(
    double const *weights,
    double const *nearest_dist_before,
    double const *nearest_dist_after,
    unsigned int point_count,
    unsigned int const *changed_points,
    unsigned int changed_count,
    unsigned int delta_threshold,
    unsigned int *order_io,
    int *full_refresh_out,
    sixel_allocator_t *allocator);

SIXELSTATUS
sixel_kmedoids_test_build_clarans_slot_order(
    double const *weights,
    unsigned int const *nearest_slot,
    double const *nearest_dist,
    double const *second_dist,
    unsigned int point_count,
    unsigned int k,
    unsigned int slot,
    unsigned int *order_out,
    sixel_allocator_t *allocator);

SIXELSTATUS
sixel_kmedoids_test_clarans_candidate_batch_best(
    double const *points,
    double const *weights,
    unsigned int point_count,
    unsigned int const *nearest_slot,
    double const *nearest_dist,
    double const *second_dist,
    unsigned int k,
    unsigned int candidate,
    unsigned int const *slots,
    unsigned int slot_count,
    unsigned int const *slot_orders,
    double cutoff,
    unsigned int *best_slot_out,
    double *best_cost_out,
    unsigned int *evaluated_pairs_out,
    sixel_allocator_t *allocator);

SIXELSTATUS
sixel_kmedoids_test_pam_polish_cost(double const *points,
                                    double const *weights,
                                    unsigned int point_count,
                                    unsigned int const *initial_medoids,
                                    unsigned int k,
                                    double *before_cost_out,
                                    double *after_cost_out,
                                    unsigned int *iterations_out,
                                    sixel_allocator_t *allocator);

SIXELSTATUS
sixel_kmedoids_test_apply_clarans_guided_delta(
    double const *weights,
    unsigned int point_count,
    unsigned int k,
    unsigned int const *nearest_slot_before,
    double const *nearest_dist_before,
    unsigned char const *flags_before,
    unsigned int const *nearest_slot_after,
    double const *nearest_dist_after,
    unsigned char const *flags_after,
    unsigned int hot_point_limit,
    unsigned int hot_slot_limit,
    unsigned int const *changed_points,
    unsigned int changed_count,
    unsigned int old_medoid,
    unsigned int new_medoid,
    unsigned int *hot_points_out,
    unsigned int *hot_point_count_out,
    unsigned int *hot_slots_out,
    unsigned int *hot_slot_count_out,
    sixel_allocator_t *allocator);

void
sixel_kmedoids_test_reset_clarans_guided_full_build_count(void);

unsigned int
sixel_kmedoids_test_get_clarans_guided_full_build_count(void);

SIXELSTATUS
sixel_kmedoids_test_two_step_pam_polish_cost(
    double const *points,
    double const *weights,
    unsigned int point_count,
    unsigned int const *initial_medoids,
    unsigned int k,
    double *before_cost_out,
    double *after_first_cost_out,
    double *after_second_cost_out,
    unsigned int *first_iterations_out,
    unsigned int *second_iterations_out,
    sixel_allocator_t *allocator);

static unsigned char const g_test_pixels_rgb[TEST_PIXEL_COUNT * 3u] = {
    255u, 0u,   0u,
    255u, 0u,   0u,
    0u,   255u, 0u,
    0u,   255u, 0u,
    0u,   0u,   255u,
    255u, 255u, 0u,
    255u, 255u, 255u,
    0u,   0u,   0u,
    0u,   0u,   0u,
    128u, 64u,  192u,
    128u, 64u,  192u,
    32u,  200u, 120u
};

typedef struct test_guided_rank {
    unsigned int index;
    double score;
} test_guided_rank_t;

static int
test_guided_rank_is_better(double lhs_score,
                           unsigned int lhs_index,
                           double rhs_score,
                           unsigned int rhs_index)
{
    if (lhs_score > rhs_score) {
        return 1;
    }
    if (lhs_score < rhs_score) {
        return 0;
    }
    return lhs_index < rhs_index ? 1 : 0;
}

static int
test_palette_contains_color(unsigned char const *palette,
                            unsigned int color_count,
                            unsigned char const color[3])
{
    unsigned int index;

    index = 0u;
    if (palette == NULL || color == NULL) {
        return 0;
    }

    for (index = 0u; index < color_count; ++index) {
        if (palette[index * 3u + 0u] == color[0u]
                && palette[index * 3u + 1u] == color[1u]
                && palette[index * 3u + 2u] == color[2u]) {
            return 1;
        }
    }

    return 0;
}

static unsigned int
test_collect_unique_colors(unsigned char const *pixels,
                           unsigned int pixel_count,
                           unsigned char *colors_out,
                           unsigned int colors_capacity)
{
    unsigned int index;
    unsigned int unique_count;
    unsigned char color[3];

    index = 0u;
    unique_count = 0u;
    color[0u] = 0u;
    color[1u] = 0u;
    color[2u] = 0u;
    if (pixels == NULL || colors_out == NULL || colors_capacity == 0u) {
        return 0u;
    }

    for (index = 0u; index < pixel_count; ++index) {
        color[0u] = pixels[index * 3u + 0u];
        color[1u] = pixels[index * 3u + 1u];
        color[2u] = pixels[index * 3u + 2u];
        if (test_palette_contains_color(colors_out, unique_count, color)) {
            continue;
        }
        if (unique_count >= colors_capacity) {
            break;
        }
        colors_out[unique_count * 3u + 0u] = color[0u];
        colors_out[unique_count * 3u + 1u] = color[1u];
        colors_out[unique_count * 3u + 2u] = color[2u];
        ++unique_count;
    }

    return unique_count;
}

static int
test_verify_palette_subset(unsigned char const *palette,
                           unsigned int color_count,
                           unsigned char const *allowed,
                           unsigned int allowed_count)
{
    unsigned int index;
    unsigned char color[3];

    index = 0u;
    color[0u] = 0u;
    color[1u] = 0u;
    color[2u] = 0u;
    if (palette == NULL || allowed == NULL || allowed_count == 0u) {
        return 0;
    }

    for (index = 0u; index < color_count; ++index) {
        color[0u] = palette[index * 3u + 0u];
        color[1u] = palette[index * 3u + 1u];
        color[2u] = palette[index * 3u + 2u];
        if (!test_palette_contains_color(allowed, allowed_count, color)) {
            return 0;
        }
    }

    return 1;
}

static int
test_build_dither(sixel_kmedoids_algo_t algo,
                  uint32_t seed,
                  unsigned int iter_override,
                  int use_float32,
                  sixel_dither_t **dither_out,
                  sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    sixel_dither_t *dither;
    sixel_palette_t *palette_obj;
    float float_pixels[TEST_PIXEL_COUNT * 3u];
    unsigned int index;
    int pixelformat;

    status = SIXEL_FALSE;
    dither = NULL;
    palette_obj = NULL;
    index = 0u;
    pixelformat = SIXEL_PIXELFORMAT_RGB888;
    if (dither_out == NULL || allocator == NULL) {
        return 0;
    }
    *dither_out = NULL;

    status = sixel_dither_new(&dither, TEST_REQCOLORS, allocator);
    if (SIXEL_FAILED(status) || dither == NULL) {
        return 0;
    }

    dither->quantize_model = SIXEL_QUANTIZE_MODEL_KMEDOIDS;
    dither->final_merge_mode = SIXEL_FINAL_MERGE_NONE;
    dither->prefer_float32 = use_float32 ? 1 : 0;

    sixel_set_kmedoids_algo_override(1, algo);
    sixel_set_kmedoids_seed_override(1, seed);
    if (iter_override > 0u) {
        sixel_set_kmedoids_iter_override(1, iter_override);
    } else {
        sixel_set_kmedoids_iter_override(0, 0u);
    }

    if (use_float32) {
        pixelformat = SIXEL_PIXELFORMAT_RGBFLOAT32;
        for (index = 0u; index < TEST_PIXEL_COUNT * 3u; ++index) {
            float_pixels[index] = (float)g_test_pixels_rgb[index] / 255.0f;
        }
        status = sixel_dither_initialize(dither,
                                         (unsigned char *)float_pixels,
                                         TEST_WIDTH,
                                         TEST_HEIGHT,
                                         pixelformat,
                                         SIXEL_LARGE_AUTO,
                                         SIXEL_REP_AUTO,
                                         SIXEL_QUALITY_AUTO);
    } else {
        status = sixel_dither_initialize(dither,
                                         (unsigned char *)g_test_pixels_rgb,
                                         TEST_WIDTH,
                                         TEST_HEIGHT,
                                         pixelformat,
                                         SIXEL_LARGE_AUTO,
                                         SIXEL_REP_AUTO,
                                         SIXEL_QUALITY_AUTO);
    }

    sixel_set_kmedoids_algo_override(0, SIXEL_PALETTE_KMEDOIDS_ALGO_PAM);
    sixel_set_kmedoids_seed_override(0, 1u);
    sixel_set_kmedoids_iter_override(0, 0u);

    if (SIXEL_FAILED(status)
            || sixel_dither_get_num_of_palette_colors(dither) <= 0) {
        sixel_dither_unref(dither);
        return 0;
    }

    status = sixel_dither_get_quantized_palette(dither, &palette_obj);
    if (SIXEL_FAILED(status) || palette_obj == NULL) {
        sixel_dither_unref(dither);
        return 0;
    }
    sixel_palette_unref(palette_obj);

    *dither_out = dither;
    return 1;
}

static double
test_palette_cost_for_pixels(unsigned char const *pixels,
                             unsigned int pixel_count,
                             unsigned char const *palette,
                             unsigned int palette_count)
{
    unsigned int pixel_index;
    unsigned int color_index;
    double diff0;
    double diff1;
    double diff2;
    double distance;
    double best_distance;
    double total;

    pixel_index = 0u;
    color_index = 0u;
    diff0 = 0.0;
    diff1 = 0.0;
    diff2 = 0.0;
    distance = 0.0;
    best_distance = 0.0;
    total = 0.0;
    if (pixels == NULL || palette == NULL || pixel_count == 0u
            || palette_count == 0u) {
        return 0.0;
    }

    for (pixel_index = 0u; pixel_index < pixel_count; ++pixel_index) {
        best_distance = -1.0;
        for (color_index = 0u; color_index < palette_count; ++color_index) {
            diff0 = (double)pixels[pixel_index * 3u + 0u]
                  - (double)palette[color_index * 3u + 0u];
            diff1 = (double)pixels[pixel_index * 3u + 1u]
                  - (double)palette[color_index * 3u + 1u];
            diff2 = (double)pixels[pixel_index * 3u + 2u]
                  - (double)palette[color_index * 3u + 2u];
            distance = diff0 * diff0 + diff1 * diff1 + diff2 * diff2;
            if (best_distance < 0.0 || distance < best_distance) {
                best_distance = distance;
            }
        }
        total += best_distance;
    }

    return total;
}

static int
test_verify_collected_sample(unsigned char const *pixels_8bit,
                             float const *pixels_float32,
                             unsigned int pixel_count,
                             int input_is_float32,
                             double const expected[3u])
{
    sixel_allocator_t *allocator;
    SIXELSTATUS status;
    unsigned int channels;
    unsigned int pixel_stride;
    unsigned int length;
    double *samples;
    double *sample_weights;
    unsigned int sample_count;
    unsigned int visible_count;
    double diff;

    allocator = NULL;
    status = SIXEL_FALSE;
    channels = 3u;
    pixel_stride = 0u;
    length = 0u;
    samples = NULL;
    sample_weights = NULL;
    sample_count = 0u;
    visible_count = 0u;
    diff = 0.0;
    if (expected == NULL) {
        return 0;
    }
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL))) {
        return 0;
    }

    if (input_is_float32) {
        pixel_stride = (unsigned int)sizeof(float) * channels;
        length = pixel_count * pixel_stride;
        status = sixel_kmedoids_test_collect_samples(
            (unsigned char const *)pixels_float32,
            length,
            channels,
            pixel_stride,
            1,
            3u,
            1u,
            0u,
            1.000,
            1u,
            &samples,
            &sample_weights,
            &sample_count,
            &visible_count,
            allocator);
    } else {
        pixel_stride = channels;
        length = pixel_count * pixel_stride;
        status = sixel_kmedoids_test_collect_samples(
            pixels_8bit,
            length,
            channels,
            pixel_stride,
            0,
            3u,
            1u,
            0u,
            1.000,
            1u,
            &samples,
            &sample_weights,
            &sample_count,
            &visible_count,
            allocator);
    }
    if (SIXEL_FAILED(status) || samples == NULL || sample_weights == NULL) {
        sixel_allocator_unref(allocator);
        return 0;
    }
    if (sample_count != 1u || visible_count != pixel_count) {
        sixel_allocator_free(allocator, sample_weights);
        sixel_allocator_free(allocator, samples);
        sixel_allocator_unref(allocator);
        return 0;
    }
    diff = samples[0u] - expected[0u];
    if (diff < 0.0) {
        diff = -diff;
    }
    if (diff > 1.0e-9) {
        sixel_allocator_free(allocator, sample_weights);
        sixel_allocator_free(allocator, samples);
        sixel_allocator_unref(allocator);
        return 0;
    }
    diff = samples[1u] - expected[1u];
    if (diff < 0.0) {
        diff = -diff;
    }
    if (diff > 1.0e-9) {
        sixel_allocator_free(allocator, sample_weights);
        sixel_allocator_free(allocator, samples);
        sixel_allocator_unref(allocator);
        return 0;
    }
    diff = samples[2u] - expected[2u];
    if (diff < 0.0) {
        diff = -diff;
    }
    if (diff > 1.0e-9) {
        sixel_allocator_free(allocator, sample_weights);
        sixel_allocator_free(allocator, samples);
        sixel_allocator_unref(allocator);
        return 0;
    }

    sixel_allocator_free(allocator, sample_weights);
    sixel_allocator_free(allocator, samples);
    sixel_allocator_unref(allocator);
    return 1;
}

/*
 * Copy palette entries through the non-deprecated palette object API.
 * The caller must release `*palette_out` with sixel_allocator_free().
 */
static int
test_copy_palette_from_dither(sixel_dither_t *dither,
                              sixel_allocator_t *allocator,
                              unsigned char **palette_out,
                              unsigned int *ncolors_out)
{
    SIXELSTATUS status;
    sixel_palette_t *palette_obj;
    unsigned char *palette;
    size_t ncolors;

    status = SIXEL_FALSE;
    palette_obj = NULL;
    palette = NULL;
    ncolors = 0u;
    if (dither == NULL
            || allocator == NULL
            || palette_out == NULL
            || ncolors_out == NULL) {
        return 0;
    }
    *palette_out = NULL;
    *ncolors_out = 0u;

    status = sixel_dither_get_quantized_palette(dither, &palette_obj);
    if (SIXEL_FAILED(status) || palette_obj == NULL) {
        return 0;
    }

    status = sixel_palette_copy_entries_8bit(
        palette_obj,
        &palette,
        &ncolors,
        SIXEL_PIXELFORMAT_RGB888,
        allocator);
    sixel_palette_unref(palette_obj);
    if (SIXEL_FAILED(status) || palette == NULL || ncolors == 0u) {
        return 0;
    }

    if (ncolors > (size_t)UINT_MAX) {
        sixel_allocator_free(allocator, palette);
        return 0;
    }

    *palette_out = palette;
    *ncolors_out = (unsigned int)ncolors;
    return 1;
}

static int
test_run_subset_case(sixel_kmedoids_algo_t algo,
                     int use_float32)
{
    sixel_allocator_t *allocator;
    sixel_dither_t *dither;
    unsigned char allowed[TEST_PIXEL_COUNT * 3u];
    unsigned int allowed_count;
    unsigned char *palette;
    unsigned int ncolors;

    allocator = NULL;
    dither = NULL;
    allowed_count = 0u;
    palette = NULL;
    ncolors = 0u;

    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL))) {
        return 0;
    }

    allowed_count = test_collect_unique_colors(g_test_pixels_rgb,
                                               TEST_PIXEL_COUNT,
                                               allowed,
                                               TEST_PIXEL_COUNT);
    if (allowed_count == 0u) {
        sixel_allocator_unref(allocator);
        return 0;
    }

    if (!test_build_dither(algo, 7u, 0u, use_float32, &dither, allocator)) {
        sixel_allocator_unref(allocator);
        return 0;
    }

    if (!test_copy_palette_from_dither(dither,
                                       allocator,
                                       &palette,
                                       &ncolors)) {
        sixel_dither_unref(dither);
        sixel_allocator_unref(allocator);
        return 0;
    }

    if (!test_verify_palette_subset(palette,
                                    ncolors,
                                    allowed,
                                    allowed_count)) {
        sixel_allocator_free(allocator, palette);
        sixel_dither_unref(dither);
        sixel_allocator_unref(allocator);
        return 0;
    }

    sixel_allocator_free(allocator, palette);
    sixel_dither_unref(dither);
    sixel_allocator_unref(allocator);
    return 1;
}

static int
test_run_seed_case(sixel_kmedoids_algo_t algo,
                   int use_float32)
{
    sixel_allocator_t *allocator;
    sixel_dither_t *dither_a;
    sixel_dither_t *dither_b;
    unsigned char *palette_a;
    unsigned char *palette_b;
    unsigned int colors_a;
    unsigned int colors_b;

    allocator = NULL;
    dither_a = NULL;
    dither_b = NULL;
    palette_a = NULL;
    palette_b = NULL;
    colors_a = 0u;
    colors_b = 0u;

    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL))) {
        return 0;
    }

    if (!test_build_dither(algo,
                           1234u,
                           0u,
                           use_float32,
                           &dither_a,
                           allocator)) {
        sixel_allocator_unref(allocator);
        return 0;
    }
    if (!test_build_dither(algo,
                           1234u,
                           0u,
                           use_float32,
                           &dither_b,
                           allocator)) {
        sixel_dither_unref(dither_a);
        sixel_allocator_unref(allocator);
        return 0;
    }

    if (!test_copy_palette_from_dither(dither_a,
                                       allocator,
                                       &palette_a,
                                       &colors_a)) {
        sixel_dither_unref(dither_a);
        sixel_dither_unref(dither_b);
        sixel_allocator_unref(allocator);
        return 0;
    }
    if (!test_copy_palette_from_dither(dither_b,
                                       allocator,
                                       &palette_b,
                                       &colors_b)) {
        sixel_allocator_free(allocator, palette_a);
        sixel_dither_unref(dither_a);
        sixel_dither_unref(dither_b);
        sixel_allocator_unref(allocator);
        return 0;
    }

    if (colors_a != colors_b) {
        sixel_allocator_free(allocator, palette_a);
        sixel_allocator_free(allocator, palette_b);
        sixel_dither_unref(dither_a);
        sixel_dither_unref(dither_b);
        sixel_allocator_unref(allocator);
        return 0;
    }
    if (memcmp(palette_a, palette_b, (size_t)colors_a * 3u) != 0) {
        sixel_allocator_free(allocator, palette_a);
        sixel_allocator_free(allocator, palette_b);
        sixel_dither_unref(dither_a);
        sixel_dither_unref(dither_b);
        sixel_allocator_unref(allocator);
        return 0;
    }

    sixel_allocator_free(allocator, palette_a);
    sixel_allocator_free(allocator, palette_b);
    sixel_dither_unref(dither_a);
    sixel_dither_unref(dither_b);
    sixel_allocator_unref(allocator);
    return 1;
}

static int
test_run_sample_representative_nearest_mean_case(void)
{
    unsigned char nearest_8bit[9u];
    unsigned char tie_8bit[6u];
    float nearest_float32[9u];
    float tie_float32[6u];
    double nearest_expected[3u];
    double tie_expected[3u];
    unsigned int index;

    nearest_8bit[0u] = 0u;
    nearest_8bit[1u] = 0u;
    nearest_8bit[2u] = 0u;
    nearest_8bit[3u] = 31u;
    nearest_8bit[4u] = 0u;
    nearest_8bit[5u] = 0u;
    nearest_8bit[6u] = 31u;
    nearest_8bit[7u] = 0u;
    nearest_8bit[8u] = 0u;

    tie_8bit[0u] = 10u;
    tie_8bit[1u] = 0u;
    tie_8bit[2u] = 0u;
    tie_8bit[3u] = 22u;
    tie_8bit[4u] = 0u;
    tie_8bit[5u] = 0u;

    for (index = 0u; index < 9u; ++index) {
        nearest_float32[index] = (float)nearest_8bit[index];
    }
    for (index = 0u; index < 6u; ++index) {
        tie_float32[index] = (float)tie_8bit[index];
    }

    nearest_expected[0u] = 31.0;
    nearest_expected[1u] = 0.0;
    nearest_expected[2u] = 0.0;
    tie_expected[0u] = 10.0;
    tie_expected[1u] = 0.0;
    tie_expected[2u] = 0.0;

    if (!test_verify_collected_sample(nearest_8bit,
                                      NULL,
                                      3u,
                                      0,
                                      nearest_expected)) {
        return 0;
    }
    if (!test_verify_collected_sample(tie_8bit,
                                      NULL,
                                      2u,
                                      0,
                                      tie_expected)) {
        return 0;
    }
    if (!test_verify_collected_sample(NULL,
                                      nearest_float32,
                                      3u,
                                      1,
                                      nearest_expected)) {
        return 0;
    }
    if (!test_verify_collected_sample(NULL,
                                      tie_float32,
                                      2u,
                                      1,
                                      tie_expected)) {
        return 0;
    }
    return 1;
}

static int
test_run_bandit_guided_seed_reproducibility_case(void)
{
    int ok;

    ok = 0;
    sixel_set_kmedoids_bandit_iter_override(1, 12u);
    sixel_set_kmedoids_bandit_candidates_override(1, 256u);
    sixel_set_kmedoids_bandit_batch_override(1, 96u);
    if (!test_run_seed_case(SIXEL_PALETTE_KMEDOIDS_ALGO_BANDITPAM, 0)) {
        goto end;
    }
    if (!test_run_seed_case(SIXEL_PALETTE_KMEDOIDS_ALGO_BANDITPAM, 1)) {
        goto end;
    }
    ok = 1;

end:
    sixel_set_kmedoids_bandit_batch_override(0, 0u);
    sixel_set_kmedoids_bandit_candidates_override(0, 0u);
    sixel_set_kmedoids_bandit_iter_override(0, 0u);
    return ok;
}

static int
test_run_polish_two_step_monotonic_case(void)
{
    sixel_allocator_t *allocator;
    double points[TEST_PIXEL_COUNT * 3u];
    double weights[TEST_PIXEL_COUNT];
    unsigned int medoids[TEST_REQCOLORS];
    unsigned int index;
    unsigned int first_iterations;
    unsigned int second_iterations;
    double before_cost;
    double after_first_cost;
    double after_second_cost;
    SIXELSTATUS status;

    allocator = NULL;
    index = 0u;
    first_iterations = 0u;
    second_iterations = 0u;
    before_cost = 0.0;
    after_first_cost = 0.0;
    after_second_cost = 0.0;
    status = SIXEL_FALSE;
    medoids[0u] = 0u;
    medoids[1u] = 2u;
    medoids[2u] = 5u;
    medoids[3u] = 9u;

    for (index = 0u; index < TEST_PIXEL_COUNT * 3u; ++index) {
        points[index] = (double)g_test_pixels_rgb[index];
    }
    for (index = 0u; index < TEST_PIXEL_COUNT; ++index) {
        weights[index] = (double)((index % 4u) + 1u);
    }
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL))) {
        return 0;
    }

    status = sixel_kmedoids_test_two_step_pam_polish_cost(
        points,
        weights,
        TEST_PIXEL_COUNT,
        medoids,
        TEST_REQCOLORS,
        &before_cost,
        &after_first_cost,
        &after_second_cost,
        &first_iterations,
        &second_iterations,
        allocator);
    sixel_allocator_unref(allocator);
    if (SIXEL_FAILED(status)) {
        return 0;
    }
    if (first_iterations > 1u || second_iterations > 1u) {
        return 0;
    }
    if (after_first_cost > before_cost + 1.0e-9) {
        return 0;
    }
    if (after_second_cost > after_first_cost + 1.0e-9) {
        return 0;
    }
    return 1;
}

static int
test_run_pam_monotonic_case(void)
{
    sixel_allocator_t *allocator;
    sixel_dither_t *dither_low;
    sixel_dither_t *dither_high;
    unsigned char *palette_low;
    unsigned char *palette_high;
    unsigned int colors_low;
    unsigned int colors_high;
    double cost_low;
    double cost_high;

    allocator = NULL;
    dither_low = NULL;
    dither_high = NULL;
    palette_low = NULL;
    palette_high = NULL;
    colors_low = 0u;
    colors_high = 0u;
    cost_low = 0.0;
    cost_high = 0.0;

    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL))) {
        return 0;
    }

    if (!test_build_dither(SIXEL_PALETTE_KMEDOIDS_ALGO_PAM,
                           42u,
                           1u,
                           0,
                           &dither_low,
                           allocator)) {
        sixel_allocator_unref(allocator);
        return 0;
    }
    if (!test_build_dither(SIXEL_PALETTE_KMEDOIDS_ALGO_PAM,
                           42u,
                           4u,
                           0,
                           &dither_high,
                           allocator)) {
        sixel_dither_unref(dither_low);
        sixel_allocator_unref(allocator);
        return 0;
    }

    if (!test_copy_palette_from_dither(dither_low,
                                       allocator,
                                       &palette_low,
                                       &colors_low)) {
        sixel_dither_unref(dither_low);
        sixel_dither_unref(dither_high);
        sixel_allocator_unref(allocator);
        return 0;
    }
    if (!test_copy_palette_from_dither(dither_high,
                                       allocator,
                                       &palette_high,
                                       &colors_high)) {
        sixel_allocator_free(allocator, palette_low);
        sixel_dither_unref(dither_low);
        sixel_dither_unref(dither_high);
        sixel_allocator_unref(allocator);
        return 0;
    }

    cost_low = test_palette_cost_for_pixels(g_test_pixels_rgb,
                                            TEST_PIXEL_COUNT,
                                            palette_low,
                                            colors_low);
    cost_high = test_palette_cost_for_pixels(g_test_pixels_rgb,
                                             TEST_PIXEL_COUNT,
                                             palette_high,
                                             colors_high);

    sixel_allocator_free(allocator, palette_low);
    sixel_allocator_free(allocator, palette_high);
    sixel_dither_unref(dither_low);
    sixel_dither_unref(dither_high);
    sixel_allocator_unref(allocator);

    return cost_high <= cost_low + 1.0e-9 ? 1 : 0;
}

static int
test_indices_are_unique(unsigned int const *indices,
                        unsigned int count,
                        unsigned int upper_bound)
{
    unsigned int index;
    unsigned int probe;

    index = 0u;
    probe = 0u;
    if (indices == NULL) {
        return 0;
    }

    for (index = 0u; index < count; ++index) {
        if (indices[index] >= upper_bound) {
            return 0;
        }
        for (probe = index + 1u; probe < count; ++probe) {
            if (indices[index] == indices[probe]) {
                return 0;
            }
        }
    }

    return 1;
}

static int
test_run_clara_sample_indices_case(void)
{
    sixel_allocator_t *allocator;
    unsigned int *indices_a;
    unsigned int *indices_b;
    unsigned int *indices_c;
    unsigned int index;

    allocator = NULL;
    indices_a = NULL;
    indices_b = NULL;
    indices_c = NULL;
    index = 0u;

    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL))) {
        return 0;
    }

    if (SIXEL_FAILED(sixel_kmedoids_test_pick_sample_indices(97u,
                                                             31u,
                                                             314159u,
                                                             &indices_a,
                                                             allocator))) {
        sixel_allocator_unref(allocator);
        return 0;
    }
    if (SIXEL_FAILED(sixel_kmedoids_test_pick_sample_indices(97u,
                                                             31u,
                                                             314159u,
                                                             &indices_b,
                                                             allocator))) {
        sixel_allocator_free(allocator, indices_a);
        sixel_allocator_unref(allocator);
        return 0;
    }
    if (!test_indices_are_unique(indices_a, 31u, 97u)
            || !test_indices_are_unique(indices_b, 31u, 97u)) {
        sixel_allocator_free(allocator, indices_a);
        sixel_allocator_free(allocator, indices_b);
        sixel_allocator_unref(allocator);
        return 0;
    }
    if (memcmp(indices_a, indices_b, sizeof(unsigned int) * 31u) != 0) {
        sixel_allocator_free(allocator, indices_a);
        sixel_allocator_free(allocator, indices_b);
        sixel_allocator_unref(allocator);
        return 0;
    }

    if (SIXEL_FAILED(sixel_kmedoids_test_pick_sample_indices(16u,
                                                             16u,
                                                             1u,
                                                             &indices_c,
                                                             allocator))) {
        sixel_allocator_free(allocator, indices_a);
        sixel_allocator_free(allocator, indices_b);
        sixel_allocator_unref(allocator);
        return 0;
    }
    for (index = 0u; index < 16u; ++index) {
        if (indices_c[index] != index) {
            sixel_allocator_free(allocator, indices_a);
            sixel_allocator_free(allocator, indices_b);
            sixel_allocator_free(allocator, indices_c);
            sixel_allocator_unref(allocator);
            return 0;
        }
    }

    sixel_allocator_free(allocator, indices_a);
    sixel_allocator_free(allocator, indices_b);
    sixel_allocator_free(allocator, indices_c);
    sixel_allocator_unref(allocator);
    return 1;
}

static int
test_run_bandit_delta_consistency_case(void)
{
    double points[TEST_PIXEL_COUNT * 3u];
    double weights[TEST_PIXEL_COUNT];
    unsigned int medoids[TEST_REQCOLORS];
    unsigned int nearest_delta[TEST_PIXEL_COUNT];
    double nearest_dist_delta[TEST_PIXEL_COUNT];
    double second_dist_delta[TEST_PIXEL_COUNT];
    unsigned int second_slot_delta[TEST_PIXEL_COUNT];
    unsigned int nearest_full[TEST_PIXEL_COUNT];
    double nearest_dist_full[TEST_PIXEL_COUNT];
    double second_dist_full[TEST_PIXEL_COUNT];
    unsigned int second_slot_full[TEST_PIXEL_COUNT];
    unsigned int index;
    double cost_delta;
    double cost_full;
    double diff;

    index = 0u;
    cost_delta = 0.0;
    cost_full = 0.0;
    diff = 0.0;

    medoids[0u] = 0u;
    medoids[1u] = 2u;
    medoids[2u] = 5u;
    medoids[3u] = 9u;

    for (index = 0u; index < TEST_PIXEL_COUNT * 3u; ++index) {
        points[index] = (double)g_test_pixels_rgb[index];
    }
    for (index = 0u; index < TEST_PIXEL_COUNT; ++index) {
        weights[index] = (double)((index % 3u) + 1u);
    }

    sixel_kmedoids_test_assign_points(points,
                                      weights,
                                      TEST_PIXEL_COUNT,
                                      medoids,
                                      TEST_REQCOLORS,
                                      nearest_delta,
                                      nearest_dist_delta,
                                      second_dist_delta,
                                      second_slot_delta,
                                      &cost_delta);

    medoids[1u] = 7u;
    sixel_kmedoids_test_update_assignments_after_swap(
        points,
        weights,
        TEST_PIXEL_COUNT,
        medoids,
        TEST_REQCOLORS,
        1u,
        7u,
        nearest_delta,
        nearest_dist_delta,
        second_dist_delta,
        second_slot_delta,
        &cost_delta);

    sixel_kmedoids_test_assign_points(points,
                                      weights,
                                      TEST_PIXEL_COUNT,
                                      medoids,
                                      TEST_REQCOLORS,
                                      nearest_full,
                                      nearest_dist_full,
                                      second_dist_full,
                                      second_slot_full,
                                      &cost_full);

    diff = cost_delta - cost_full;
    if (diff < 0.0) {
        diff = -diff;
    }
    if (diff > 1.0e-9) {
        return 0;
    }

    for (index = 0u; index < TEST_PIXEL_COUNT; ++index) {
        if (nearest_delta[index] != nearest_full[index]) {
            return 0;
        }
        if (second_slot_delta[index] != second_slot_full[index]) {
            return 0;
        }
        diff = nearest_dist_delta[index] - nearest_dist_full[index];
        if (diff < 0.0) {
            diff = -diff;
        }
        if (diff > 1.0e-9) {
            return 0;
        }
        diff = second_dist_delta[index] - second_dist_full[index];
        if (diff < 0.0) {
            diff = -diff;
        }
        if (diff > 1.0e-9) {
            return 0;
        }
    }

    return 1;
}

static double
test_swap_cost_full_ordered(double const *points,
                            double const *weights,
                            unsigned int point_count,
                            unsigned int const *nearest_slot,
                            double const *nearest_dist,
                            double const *second_dist,
                            unsigned int replace_slot,
                            unsigned int candidate_point,
                            unsigned int const *order)
{
    unsigned int ordered_index;
    unsigned int index;
    double distance;
    double chosen;
    double weight;
    double cost;

    ordered_index = 0u;
    index = 0u;
    distance = 0.0;
    chosen = 0.0;
    weight = 1.0;
    cost = 0.0;
    if (points == NULL || nearest_slot == NULL
            || nearest_dist == NULL || second_dist == NULL) {
        return 0.0;
    }

    for (ordered_index = 0u; ordered_index < point_count; ++ordered_index) {
        if (order != NULL) {
            index = order[ordered_index];
        } else {
            index = ordered_index;
        }
        distance = points[index * 3u + 0u] - points[candidate_point * 3u + 0u];
        chosen = distance * distance;
        distance = points[index * 3u + 1u] - points[candidate_point * 3u + 1u];
        chosen += distance * distance;
        distance = points[index * 3u + 2u] - points[candidate_point * 3u + 2u];
        chosen += distance * distance;

        if (nearest_slot[index] == replace_slot) {
            if (second_dist[index] < chosen) {
                chosen = second_dist[index];
            }
        } else if (nearest_dist[index] < chosen) {
            chosen = nearest_dist[index];
        }

        weight = 1.0;
        if (weights != NULL) {
            weight = weights[index];
        }
        cost += chosen * weight;
    }

    return cost;
}

static int
test_run_clarans_swap_cost_cutoff_case(void)
{
    double points[TEST_PIXEL_COUNT * 3u];
    double weights[TEST_PIXEL_COUNT];
    unsigned int order[TEST_PIXEL_COUNT];
    unsigned int medoids[TEST_REQCOLORS];
    unsigned int nearest_slot[TEST_PIXEL_COUNT];
    double nearest_dist[TEST_PIXEL_COUNT];
    double second_dist[TEST_PIXEL_COUNT];
    unsigned int second_slot[TEST_PIXEL_COUNT];
    unsigned char medoid_flags[TEST_PIXEL_COUNT];
    unsigned int index;
    unsigned int probe;
    unsigned int replace_slot;
    unsigned int candidate;
    double current_cost;
    double full_cost;
    double cutoff_cost;
    int early_stop;
    int full_accept;
    int cutoff_accept;
    unsigned int swap_temp;

    index = 0u;
    probe = 0u;
    replace_slot = 0u;
    candidate = 0u;
    current_cost = 0.0;
    full_cost = 0.0;
    cutoff_cost = 0.0;
    early_stop = 0;
    full_accept = 0;
    cutoff_accept = 0;
    swap_temp = 0u;

    medoids[0u] = 0u;
    medoids[1u] = 2u;
    medoids[2u] = 5u;
    medoids[3u] = 9u;

    for (index = 0u; index < TEST_PIXEL_COUNT * 3u; ++index) {
        points[index] = (double)g_test_pixels_rgb[index];
    }
    for (index = 0u; index < TEST_PIXEL_COUNT; ++index) {
        weights[index] = (double)((index % 4u) + 1u);
        order[index] = index;
        medoid_flags[index] = 0u;
    }
    for (index = 0u; index < TEST_REQCOLORS; ++index) {
        medoid_flags[medoids[index]] = 1u;
    }

    for (index = 0u; index + 1u < TEST_PIXEL_COUNT; ++index) {
        for (probe = index + 1u; probe < TEST_PIXEL_COUNT; ++probe) {
            if (weights[order[probe]] > weights[order[index]]
                    || (weights[order[probe]] == weights[order[index]]
                        && order[probe] < order[index])) {
                swap_temp = order[index];
                order[index] = order[probe];
                order[probe] = swap_temp;
            }
        }
    }

    sixel_kmedoids_test_assign_points(points,
                                      weights,
                                      TEST_PIXEL_COUNT,
                                      medoids,
                                      TEST_REQCOLORS,
                                      nearest_slot,
                                      nearest_dist,
                                      second_dist,
                                      second_slot,
                                      &current_cost);

    for (replace_slot = 0u; replace_slot < TEST_REQCOLORS; ++replace_slot) {
        for (candidate = 0u; candidate < TEST_PIXEL_COUNT; ++candidate) {
            if (medoid_flags[candidate] != 0u) {
                continue;
            }

            full_cost = test_swap_cost_full_ordered(points,
                                                    weights,
                                                    TEST_PIXEL_COUNT,
                                                    nearest_slot,
                                                    nearest_dist,
                                                    second_dist,
                                                    replace_slot,
                                                    candidate,
                                                    order);
            cutoff_cost = sixel_kmedoids_test_swap_cost_cutoff(
                points,
                weights,
                TEST_PIXEL_COUNT,
                nearest_slot,
                nearest_dist,
                second_dist,
                replace_slot,
                candidate,
                order,
                current_cost,
                &early_stop);

            full_accept = (full_cost + 1.0e-12 < current_cost) ? 1 : 0;
            cutoff_accept = (cutoff_cost + 1.0e-12 < current_cost) ? 1 : 0;
            if (full_accept != cutoff_accept) {
                return 0;
            }
            if (full_accept && early_stop) {
                return 0;
            }
        }
    }

    return 1;
}

static int
test_run_clarans_candidate_cache_equivalence_case(void)
{
    double points[TEST_PIXEL_COUNT * 3u];
    double weights[TEST_PIXEL_COUNT];
    unsigned int order[TEST_PIXEL_COUNT];
    unsigned int medoids[TEST_REQCOLORS];
    unsigned int nearest_slot[TEST_PIXEL_COUNT];
    double nearest_dist[TEST_PIXEL_COUNT];
    double second_dist[TEST_PIXEL_COUNT];
    unsigned int second_slot[TEST_PIXEL_COUNT];
    unsigned char medoid_flags[TEST_PIXEL_COUNT];
    double candidate_row[TEST_PIXEL_COUNT];
    unsigned int index;
    unsigned int replace_slot;
    unsigned int candidate;
    double uncached_cost;
    double cached_cost;
    double current_cost;
    double diff;
    double delta;
    int uncached_stop;
    int cached_stop;
    int uncached_accept;
    int cached_accept;
    SIXELSTATUS status;
    sixel_allocator_t *allocator;

    index = 0u;
    replace_slot = 0u;
    candidate = 0u;
    uncached_cost = 0.0;
    cached_cost = 0.0;
    current_cost = 0.0;
    diff = 0.0;
    delta = 0.0;
    uncached_stop = 0;
    cached_stop = 0;
    uncached_accept = 0;
    cached_accept = 0;
    status = SIXEL_FALSE;
    allocator = NULL;

    medoids[0u] = 0u;
    medoids[1u] = 2u;
    medoids[2u] = 5u;
    medoids[3u] = 9u;

    for (index = 0u; index < TEST_PIXEL_COUNT * 3u; ++index) {
        points[index] = (double)g_test_pixels_rgb[index];
    }
    for (index = 0u; index < TEST_PIXEL_COUNT; ++index) {
        weights[index] = (double)((index % 4u) + 1u);
        medoid_flags[index] = 0u;
        order[index] = index;
    }
    for (index = 0u; index < TEST_REQCOLORS; ++index) {
        medoid_flags[medoids[index]] = 1u;
    }

    sixel_kmedoids_test_assign_points(points,
                                      weights,
                                      TEST_PIXEL_COUNT,
                                      medoids,
                                      TEST_REQCOLORS,
                                      nearest_slot,
                                      nearest_dist,
                                      second_dist,
                                      second_slot,
                                      &current_cost);

    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL))) {
        return 0;
    }
    status = sixel_kmedoids_test_build_eval_order_residual(weights,
                                                            nearest_dist,
                                                            TEST_PIXEL_COUNT,
                                                            order,
                                                            allocator);
    if (SIXEL_FAILED(status)) {
        sixel_allocator_unref(allocator);
        return 0;
    }
    sixel_allocator_unref(allocator);

    for (replace_slot = 0u; replace_slot < TEST_REQCOLORS; ++replace_slot) {
        for (candidate = 0u; candidate < TEST_PIXEL_COUNT; ++candidate) {
            if (medoid_flags[candidate] != 0u) {
                continue;
            }
            for (index = 0u; index < TEST_PIXEL_COUNT; ++index) {
                delta = points[index * 3u + 0u]
                      - points[candidate * 3u + 0u];
                candidate_row[index] = delta * delta;
                delta = points[index * 3u + 1u]
                      - points[candidate * 3u + 1u];
                candidate_row[index] += delta * delta;
                delta = points[index * 3u + 2u]
                      - points[candidate * 3u + 2u];
                candidate_row[index] += delta * delta;
            }

            uncached_cost = sixel_kmedoids_test_swap_cost_cutoff(
                points,
                weights,
                TEST_PIXEL_COUNT,
                nearest_slot,
                nearest_dist,
                second_dist,
                replace_slot,
                candidate,
                order,
                current_cost,
                &uncached_stop);
            cached_cost = sixel_kmedoids_test_swap_cost_cutoff_with_row(
                points,
                weights,
                TEST_PIXEL_COUNT,
                nearest_slot,
                nearest_dist,
                second_dist,
                replace_slot,
                candidate,
                candidate_row,
                order,
                current_cost,
                &cached_stop);

            uncached_accept = (uncached_cost + 1.0e-12 < current_cost) ? 1 : 0;
            cached_accept = (cached_cost + 1.0e-12 < current_cost) ? 1 : 0;
            if (uncached_accept != cached_accept) {
                return 0;
            }
            if (uncached_stop != cached_stop) {
                return 0;
            }
            diff = uncached_cost - cached_cost;
            if (diff < 0.0) {
                diff = -diff;
            }
            if (diff > 1.0e-9) {
                return 0;
            }
        }
    }
    return 1;
}

static int
test_build_clarans_slot_orders(double const *weights,
                               unsigned int const *nearest_slot,
                               double const *nearest_dist,
                               double const *second_dist,
                               unsigned int point_count,
                               unsigned int k,
                               unsigned int *slot_orders,
                               sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    unsigned int slot;

    status = SIXEL_FALSE;
    slot = 0u;
    if (nearest_slot == NULL || nearest_dist == NULL || second_dist == NULL
            || slot_orders == NULL || allocator == NULL
            || point_count == 0u || k == 0u) {
        return 0;
    }
    for (slot = 0u; slot < k; ++slot) {
        status = sixel_kmedoids_test_build_clarans_slot_order(
            weights,
            nearest_slot,
            nearest_dist,
            second_dist,
            point_count,
            k,
            slot,
            slot_orders + slot * point_count,
            allocator);
        if (SIXEL_FAILED(status)) {
            return 0;
        }
    }
    return 1;
}

static int
test_run_clarans_candidate_batch_equivalence_case(void)
{
    sixel_allocator_t *allocator;
    double points[TEST_PIXEL_COUNT * 3u];
    double weights[TEST_PIXEL_COUNT];
    unsigned int medoids[TEST_REQCOLORS];
    unsigned int nearest_slot[TEST_PIXEL_COUNT];
    double nearest_dist[TEST_PIXEL_COUNT];
    double second_dist[TEST_PIXEL_COUNT];
    unsigned int second_slot[TEST_PIXEL_COUNT];
    unsigned int slot_orders[TEST_REQCOLORS * TEST_PIXEL_COUNT];
    unsigned int slots[8u];
    unsigned char seen_slots[TEST_REQCOLORS];
    unsigned int index;
    unsigned int slot;
    unsigned int candidate;
    unsigned int best_slot_expected;
    unsigned int best_slot_actual;
    unsigned int evaluated_expected;
    unsigned int evaluated_actual;
    double cost;
    double best_cost_expected;
    double best_cost_actual;
    double current_cost;
    double diff;
    SIXELSTATUS status;

    allocator = NULL;
    index = 0u;
    slot = 0u;
    candidate = 7u;
    best_slot_expected = UINT_MAX;
    best_slot_actual = UINT_MAX;
    evaluated_expected = 0u;
    evaluated_actual = 0u;
    cost = 0.0;
    best_cost_expected = 0.0;
    best_cost_actual = 0.0;
    current_cost = 0.0;
    diff = 0.0;
    status = SIXEL_FALSE;
    slots[0u] = 1u;
    slots[1u] = 3u;
    slots[2u] = 1u;
    slots[3u] = 0u;
    slots[4u] = 2u;
    slots[5u] = 3u;
    slots[6u] = 2u;
    slots[7u] = 0u;

    medoids[0u] = 0u;
    medoids[1u] = 2u;
    medoids[2u] = 5u;
    medoids[3u] = 9u;
    for (index = 0u; index < TEST_PIXEL_COUNT * 3u; ++index) {
        points[index] = (double)g_test_pixels_rgb[index];
    }
    for (index = 0u; index < TEST_PIXEL_COUNT; ++index) {
        weights[index] = (double)((index % 4u) + 1u);
    }
    for (index = 0u; index < TEST_REQCOLORS; ++index) {
        seen_slots[index] = 0u;
    }
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL))) {
        return 0;
    }

    sixel_kmedoids_test_assign_points(points,
                                      weights,
                                      TEST_PIXEL_COUNT,
                                      medoids,
                                      TEST_REQCOLORS,
                                      nearest_slot,
                                      nearest_dist,
                                      second_dist,
                                      second_slot,
                                      &current_cost);

    if (!test_build_clarans_slot_orders(weights,
                                        nearest_slot,
                                        nearest_dist,
                                        second_dist,
                                        TEST_PIXEL_COUNT,
                                        TEST_REQCOLORS,
                                        slot_orders,
                                        allocator)) {
        sixel_allocator_unref(allocator);
        return 0;
    }

    best_cost_expected = current_cost;
    for (index = 0u; index < 8u; ++index) {
        slot = slots[index];
        if (slot >= TEST_REQCOLORS || seen_slots[slot] != 0u) {
            continue;
        }
        seen_slots[slot] = 1u;
        ++evaluated_expected;
        cost = test_swap_cost_full_ordered(points,
                                           weights,
                                           TEST_PIXEL_COUNT,
                                           nearest_slot,
                                           nearest_dist,
                                           second_dist,
                                           slot,
                                           candidate,
                                           slot_orders
                                               + slot * TEST_PIXEL_COUNT);
        if (cost + 1.0e-12 < best_cost_expected) {
            best_cost_expected = cost;
            best_slot_expected = slot;
        }
    }

    status = sixel_kmedoids_test_clarans_candidate_batch_best(
        points,
        weights,
        TEST_PIXEL_COUNT,
        nearest_slot,
        nearest_dist,
        second_dist,
        TEST_REQCOLORS,
        candidate,
        slots,
        8u,
        slot_orders,
        current_cost,
        &best_slot_actual,
        &best_cost_actual,
        &evaluated_actual,
        allocator);
    sixel_allocator_unref(allocator);
    if (SIXEL_FAILED(status)) {
        return 0;
    }
    if (evaluated_actual != evaluated_expected) {
        return 0;
    }
    if ((best_slot_expected == UINT_MAX) != (best_slot_actual == UINT_MAX)) {
        return 0;
    }
    if (best_slot_expected != UINT_MAX
            && best_slot_actual != best_slot_expected) {
        return 0;
    }
    diff = best_cost_actual - best_cost_expected;
    if (diff < 0.0) {
        diff = -diff;
    }
    if (diff > 1.0e-9) {
        return 0;
    }
    return 1;
}

static int
test_run_clarans_slot_order_cutoff_consistency_case(void)
{
    sixel_allocator_t *allocator;
    double points[TEST_PIXEL_COUNT * 3u];
    double weights[TEST_PIXEL_COUNT];
    unsigned int medoids[TEST_REQCOLORS];
    unsigned int nearest_slot[TEST_PIXEL_COUNT];
    double nearest_dist[TEST_PIXEL_COUNT];
    double second_dist[TEST_PIXEL_COUNT];
    unsigned int second_slot[TEST_PIXEL_COUNT];
    unsigned int slot_orders[TEST_REQCOLORS * TEST_PIXEL_COUNT];
    unsigned char medoid_flags[TEST_PIXEL_COUNT];
    unsigned int index;
    unsigned int slot;
    unsigned int candidate;
    double full_cost;
    double cutoff_cost;
    double current_cost;
    int full_accept;
    int cutoff_accept;
    int early_stop;

    allocator = NULL;
    index = 0u;
    slot = 0u;
    candidate = 0u;
    full_cost = 0.0;
    cutoff_cost = 0.0;
    current_cost = 0.0;
    full_accept = 0;
    cutoff_accept = 0;
    early_stop = 0;
    medoids[0u] = 0u;
    medoids[1u] = 2u;
    medoids[2u] = 5u;
    medoids[3u] = 9u;

    for (index = 0u; index < TEST_PIXEL_COUNT * 3u; ++index) {
        points[index] = (double)g_test_pixels_rgb[index];
    }
    for (index = 0u; index < TEST_PIXEL_COUNT; ++index) {
        weights[index] = (double)((index % 4u) + 1u);
        medoid_flags[index] = 0u;
    }
    for (index = 0u; index < TEST_REQCOLORS; ++index) {
        medoid_flags[medoids[index]] = 1u;
    }
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL))) {
        return 0;
    }

    sixel_kmedoids_test_assign_points(points,
                                      weights,
                                      TEST_PIXEL_COUNT,
                                      medoids,
                                      TEST_REQCOLORS,
                                      nearest_slot,
                                      nearest_dist,
                                      second_dist,
                                      second_slot,
                                      &current_cost);

    if (!test_build_clarans_slot_orders(weights,
                                        nearest_slot,
                                        nearest_dist,
                                        second_dist,
                                        TEST_PIXEL_COUNT,
                                        TEST_REQCOLORS,
                                        slot_orders,
                                        allocator)) {
        sixel_allocator_unref(allocator);
        return 0;
    }

    for (slot = 0u; slot < TEST_REQCOLORS; ++slot) {
        for (candidate = 0u; candidate < TEST_PIXEL_COUNT; ++candidate) {
            if (medoid_flags[candidate] != 0u) {
                continue;
            }
            full_cost = test_swap_cost_full_ordered(
                points,
                weights,
                TEST_PIXEL_COUNT,
                nearest_slot,
                nearest_dist,
                second_dist,
                slot,
                candidate,
                slot_orders + slot * TEST_PIXEL_COUNT);
            cutoff_cost = sixel_kmedoids_test_swap_cost_cutoff(
                points,
                weights,
                TEST_PIXEL_COUNT,
                nearest_slot,
                nearest_dist,
                second_dist,
                slot,
                candidate,
                slot_orders + slot * TEST_PIXEL_COUNT,
                current_cost,
                &early_stop);

            full_accept = (full_cost + 1.0e-12 < current_cost) ? 1 : 0;
            cutoff_accept = (cutoff_cost + 1.0e-12 < current_cost) ? 1 : 0;
            if (full_accept != cutoff_accept) {
                sixel_allocator_unref(allocator);
                return 0;
            }
            if (full_accept && early_stop) {
                sixel_allocator_unref(allocator);
                return 0;
            }
        }
    }

    sixel_allocator_unref(allocator);
    return 1;
}

static int
test_run_clarans_slot_order_seed_reproducibility_case(void)
{
    int ok;

    ok = 0;
    sixel_set_kmedoids_clarans_local_override(1, 5u);
    sixel_set_kmedoids_clarans_neighbors_override(1, 160u);
    if (!test_run_seed_case(SIXEL_PALETTE_KMEDOIDS_ALGO_CLARANS, 0)) {
        goto end;
    }
    if (!test_run_seed_case(SIXEL_PALETTE_KMEDOIDS_ALGO_CLARANS, 1)) {
        goto end;
    }
    ok = 1;

end:
    sixel_set_kmedoids_clarans_neighbors_override(0, 0u);
    sixel_set_kmedoids_clarans_local_override(0, 0u);
    return ok;
}

static int
test_run_bandit_swap_cost_cutoff_case(void)
{
    double points[TEST_PIXEL_COUNT * 3u];
    double weights[TEST_PIXEL_COUNT];
    unsigned int order[TEST_PIXEL_COUNT];
    unsigned int medoids[TEST_REQCOLORS];
    unsigned int nearest_slot[TEST_PIXEL_COUNT];
    double nearest_dist[TEST_PIXEL_COUNT];
    double second_dist[TEST_PIXEL_COUNT];
    unsigned int second_slot[TEST_PIXEL_COUNT];
    unsigned char medoid_flags[TEST_PIXEL_COUNT];
    unsigned int index;
    unsigned int probe;
    unsigned int replace_slot;
    unsigned int candidate;
    unsigned int best_slot_full;
    unsigned int best_candidate_full;
    unsigned int best_slot_cutoff;
    unsigned int best_candidate_cutoff;
    unsigned int swap_temp;
    double current_cost;
    double full_cost;
    double cutoff_cost;
    double best_full;
    double best_cutoff;
    double diff;
    int early_stop;
    int full_accept;
    int cutoff_accept;

    index = 0u;
    probe = 0u;
    replace_slot = 0u;
    candidate = 0u;
    best_slot_full = UINT_MAX;
    best_candidate_full = UINT_MAX;
    best_slot_cutoff = UINT_MAX;
    best_candidate_cutoff = UINT_MAX;
    swap_temp = 0u;
    current_cost = 0.0;
    full_cost = 0.0;
    cutoff_cost = 0.0;
    best_full = 0.0;
    best_cutoff = 0.0;
    diff = 0.0;
    early_stop = 0;
    full_accept = 0;
    cutoff_accept = 0;

    medoids[0u] = 0u;
    medoids[1u] = 2u;
    medoids[2u] = 5u;
    medoids[3u] = 9u;

    for (index = 0u; index < TEST_PIXEL_COUNT * 3u; ++index) {
        points[index] = (double)g_test_pixels_rgb[index];
    }
    for (index = 0u; index < TEST_PIXEL_COUNT; ++index) {
        weights[index] = (double)((index % 4u) + 1u);
        order[index] = index;
        medoid_flags[index] = 0u;
    }
    for (index = 0u; index < TEST_REQCOLORS; ++index) {
        medoid_flags[medoids[index]] = 1u;
    }
    for (index = 0u; index + 1u < TEST_PIXEL_COUNT; ++index) {
        for (probe = index + 1u; probe < TEST_PIXEL_COUNT; ++probe) {
            if (weights[order[probe]] > weights[order[index]]
                    || (weights[order[probe]] == weights[order[index]]
                        && order[probe] < order[index])) {
                swap_temp = order[index];
                order[index] = order[probe];
                order[probe] = swap_temp;
            }
        }
    }

    sixel_kmedoids_test_assign_points(points,
                                      weights,
                                      TEST_PIXEL_COUNT,
                                      medoids,
                                      TEST_REQCOLORS,
                                      nearest_slot,
                                      nearest_dist,
                                      second_dist,
                                      second_slot,
                                      &current_cost);
    best_full = current_cost;
    best_cutoff = current_cost;

    for (replace_slot = 0u; replace_slot < TEST_REQCOLORS; ++replace_slot) {
        for (candidate = 0u; candidate < TEST_PIXEL_COUNT; ++candidate) {
            if (medoid_flags[candidate] != 0u) {
                continue;
            }
            full_cost = test_swap_cost_full_ordered(points,
                                                    weights,
                                                    TEST_PIXEL_COUNT,
                                                    nearest_slot,
                                                    nearest_dist,
                                                    second_dist,
                                                    replace_slot,
                                                    candidate,
                                                    order);
            if (full_cost + 1.0e-12 < best_full) {
                best_full = full_cost;
                best_slot_full = replace_slot;
                best_candidate_full = candidate;
            }

            cutoff_cost = sixel_kmedoids_test_swap_cost_cutoff(
                points,
                weights,
                TEST_PIXEL_COUNT,
                nearest_slot,
                nearest_dist,
                second_dist,
                replace_slot,
                candidate,
                order,
                best_cutoff,
                &early_stop);
            if (cutoff_cost + 1.0e-12 < best_cutoff) {
                if (early_stop) {
                    return 0;
                }
                best_cutoff = cutoff_cost;
                best_slot_cutoff = replace_slot;
                best_candidate_cutoff = candidate;
            }
        }
    }

    full_accept = (best_full + 1.0e-12 < current_cost) ? 1 : 0;
    cutoff_accept = (best_cutoff + 1.0e-12 < current_cost) ? 1 : 0;
    if (full_accept != cutoff_accept) {
        return 0;
    }
    if (!full_accept) {
        return 1;
    }
    if (best_slot_full != best_slot_cutoff
            || best_candidate_full != best_candidate_cutoff) {
        return 0;
    }
    diff = best_full - best_cutoff;
    if (diff < 0.0) {
        diff = -diff;
    }
    if (diff > 1.0e-9) {
        return 0;
    }
    return 1;
}

static int
test_run_clarans_guided_tiebreak_case(void)
{
    sixel_allocator_t *allocator;
    double weights[8u];
    unsigned int nearest_slot[8u];
    double nearest_dist[8u];
    unsigned char flags[8u];
    unsigned int hot_points_a[8u];
    unsigned int hot_points_b[8u];
    unsigned int hot_slots_a[3u];
    unsigned int hot_slots_b[3u];
    unsigned int hot_point_count_a;
    unsigned int hot_point_count_b;
    unsigned int hot_slot_count_a;
    unsigned int hot_slot_count_b;
    unsigned int index;
    SIXELSTATUS status;
    unsigned int expected_points[4u];
    unsigned int expected_slots[2u];

    allocator = NULL;
    hot_point_count_a = 0u;
    hot_point_count_b = 0u;
    hot_slot_count_a = 0u;
    hot_slot_count_b = 0u;
    index = 0u;
    status = SIXEL_FALSE;
    expected_points[0u] = 1u;
    expected_points[1u] = 2u;
    expected_points[2u] = 4u;
    expected_points[3u] = 5u;
    expected_slots[0u] = 0u;
    expected_slots[1u] = 1u;

    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL))) {
        return 0;
    }

    for (index = 0u; index < 8u; ++index) {
        weights[index] = 1.0;
        flags[index] = 0u;
    }
    flags[0u] = 1u;
    flags[3u] = 1u;
    flags[7u] = 1u;

    nearest_slot[0u] = 0u;
    nearest_slot[1u] = 0u;
    nearest_slot[2u] = 1u;
    nearest_slot[3u] = 1u;
    nearest_slot[4u] = 0u;
    nearest_slot[5u] = 1u;
    nearest_slot[6u] = 2u;
    nearest_slot[7u] = 2u;

    nearest_dist[0u] = 0.0;
    nearest_dist[1u] = 5.0;
    nearest_dist[2u] = 5.0;
    nearest_dist[3u] = 0.0;
    nearest_dist[4u] = 4.0;
    nearest_dist[5u] = 4.0;
    nearest_dist[6u] = 1.0;
    nearest_dist[7u] = 0.0;

    status = sixel_kmedoids_test_build_clarans_guided_sets(
        weights,
        8u,
        3u,
        nearest_slot,
        nearest_dist,
        flags,
        4u,
        2u,
        hot_points_a,
        &hot_point_count_a,
        hot_slots_a,
        &hot_slot_count_a,
        allocator);
    if (SIXEL_FAILED(status)) {
        sixel_allocator_unref(allocator);
        return 0;
    }

    status = sixel_kmedoids_test_build_clarans_guided_sets(
        weights,
        8u,
        3u,
        nearest_slot,
        nearest_dist,
        flags,
        4u,
        2u,
        hot_points_b,
        &hot_point_count_b,
        hot_slots_b,
        &hot_slot_count_b,
        allocator);
    if (SIXEL_FAILED(status)) {
        sixel_allocator_unref(allocator);
        return 0;
    }

    if (hot_point_count_a != 4u || hot_slot_count_a != 2u) {
        sixel_allocator_unref(allocator);
        return 0;
    }
    if (hot_point_count_b != hot_point_count_a
            || hot_slot_count_b != hot_slot_count_a) {
        sixel_allocator_unref(allocator);
        return 0;
    }
    for (index = 0u; index < hot_point_count_a; ++index) {
        if (hot_points_a[index] != expected_points[index]) {
            sixel_allocator_unref(allocator);
            return 0;
        }
        if (hot_points_a[index] != hot_points_b[index]) {
            sixel_allocator_unref(allocator);
            return 0;
        }
    }
    for (index = 0u; index < hot_slot_count_a; ++index) {
        if (hot_slots_a[index] != expected_slots[index]) {
            sixel_allocator_unref(allocator);
            return 0;
        }
        if (hot_slots_a[index] != hot_slots_b[index]) {
            sixel_allocator_unref(allocator);
            return 0;
        }
    }

    sixel_allocator_unref(allocator);
    return 1;
}

static int
test_run_clarans_guided_topk_equivalence_case(void)
{
    sixel_allocator_t *allocator;
    unsigned int const point_count = 17u;
    unsigned int const k = 6u;
    unsigned int const point_limit = 9u;
    unsigned int const slot_limit = 4u;
    double weights[17u];
    unsigned int nearest_slot[17u];
    double nearest_dist[17u];
    unsigned char flags[17u];
    double slot_scores[6u];
    test_guided_rank_t point_ranks[17u];
    test_guided_rank_t slot_ranks[6u];
    unsigned int hot_points[17u];
    unsigned int hot_slots[6u];
    unsigned int expected_hot_points[17u];
    unsigned int expected_hot_slots[6u];
    unsigned int hot_point_count;
    unsigned int hot_slot_count;
    unsigned int expected_hot_point_count;
    unsigned int expected_hot_slot_count;
    unsigned int rank_count;
    unsigned int slot_count;
    unsigned int index;
    unsigned int probe;
    unsigned int tmp_index;
    double tmp_score;
    double residual;
    SIXELSTATUS status;

    allocator = NULL;
    hot_point_count = 0u;
    hot_slot_count = 0u;
    expected_hot_point_count = 0u;
    expected_hot_slot_count = 0u;
    rank_count = 0u;
    slot_count = 0u;
    index = 0u;
    probe = 0u;
    tmp_index = 0u;
    tmp_score = 0.0;
    residual = 0.0;
    status = SIXEL_FALSE;

    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL))) {
        return 0;
    }

    for (index = 0u; index < point_count; ++index) {
        weights[index] = (double)((index % 5u) + 1u);
        nearest_slot[index] = (index * 3u + 1u) % k;
        nearest_dist[index] = (double)((index * index + index * 3u) % 19u);
        nearest_dist[index] += (double)(index % 3u) * 0.25;
        flags[index] = 0u;
    }
    flags[0u] = 1u;
    flags[3u] = 1u;
    flags[8u] = 1u;
    flags[11u] = 1u;
    flags[14u] = 1u;
    flags[16u] = 1u;

    for (index = 0u; index < k; ++index) {
        slot_scores[index] = 0.0;
    }
    for (index = 0u; index < point_count; ++index) {
        residual = weights[index] * nearest_dist[index];
        slot_scores[nearest_slot[index]] += residual;
        if (flags[index] != 0u) {
            continue;
        }
        point_ranks[rank_count].index = index;
        point_ranks[rank_count].score = residual;
        ++rank_count;
    }
    for (index = 0u; index + 1u < rank_count; ++index) {
        for (probe = index + 1u; probe < rank_count; ++probe) {
            if (test_guided_rank_is_better(point_ranks[probe].score,
                                           point_ranks[probe].index,
                                           point_ranks[index].score,
                                           point_ranks[index].index)) {
                tmp_index = point_ranks[index].index;
                tmp_score = point_ranks[index].score;
                point_ranks[index].index = point_ranks[probe].index;
                point_ranks[index].score = point_ranks[probe].score;
                point_ranks[probe].index = tmp_index;
                point_ranks[probe].score = tmp_score;
            }
        }
    }
    expected_hot_point_count = rank_count;
    if (expected_hot_point_count > point_limit) {
        expected_hot_point_count = point_limit;
    }
    for (index = 0u; index < expected_hot_point_count; ++index) {
        expected_hot_points[index] = point_ranks[index].index;
    }

    for (index = 0u; index < k; ++index) {
        slot_ranks[index].index = index;
        slot_ranks[index].score = slot_scores[index];
    }
    for (index = 0u; index + 1u < k; ++index) {
        for (probe = index + 1u; probe < k; ++probe) {
            if (test_guided_rank_is_better(slot_ranks[probe].score,
                                           slot_ranks[probe].index,
                                           slot_ranks[index].score,
                                           slot_ranks[index].index)) {
                tmp_index = slot_ranks[index].index;
                tmp_score = slot_ranks[index].score;
                slot_ranks[index].index = slot_ranks[probe].index;
                slot_ranks[index].score = slot_ranks[probe].score;
                slot_ranks[probe].index = tmp_index;
                slot_ranks[probe].score = tmp_score;
            }
        }
    }
    slot_count = k;
    if (slot_count > slot_limit) {
        slot_count = slot_limit;
    }
    expected_hot_slot_count = slot_count;
    for (index = 0u; index < expected_hot_slot_count; ++index) {
        expected_hot_slots[index] = slot_ranks[index].index;
    }

    status = sixel_kmedoids_test_build_clarans_guided_sets(
        weights,
        point_count,
        k,
        nearest_slot,
        nearest_dist,
        flags,
        point_limit,
        slot_limit,
        hot_points,
        &hot_point_count,
        hot_slots,
        &hot_slot_count,
        allocator);
    if (SIXEL_FAILED(status)) {
        sixel_allocator_unref(allocator);
        return 0;
    }
    if (hot_point_count != expected_hot_point_count
            || hot_slot_count != expected_hot_slot_count) {
        sixel_allocator_unref(allocator);
        return 0;
    }
    for (index = 0u; index < expected_hot_point_count; ++index) {
        if (hot_points[index] != expected_hot_points[index]) {
            sixel_allocator_unref(allocator);
            return 0;
        }
    }
    for (index = 0u; index < expected_hot_slot_count; ++index) {
        if (hot_slots[index] != expected_hot_slots[index]) {
            sixel_allocator_unref(allocator);
            return 0;
        }
    }

    sixel_allocator_unref(allocator);
    return 1;
}

static int
test_run_bandit_generation_seed_case(void)
{
    sixel_allocator_t *allocator;
    sixel_dither_t *dither_a;
    sixel_dither_t *dither_b;
    sixel_dither_t *dither_c;
    sixel_dither_t *dither_d;
    unsigned char *palette_a;
    unsigned char *palette_b;
    unsigned char *palette_c;
    unsigned char *palette_d;
    unsigned int colors_a;
    unsigned int colors_b;
    unsigned int colors_c;
    unsigned int colors_d;
    int ok;

    allocator = NULL;
    dither_a = NULL;
    dither_b = NULL;
    dither_c = NULL;
    dither_d = NULL;
    palette_a = NULL;
    palette_b = NULL;
    palette_c = NULL;
    palette_d = NULL;
    colors_a = 0u;
    colors_b = 0u;
    colors_c = 0u;
    colors_d = 0u;
    ok = 0;

    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL))) {
        return 0;
    }

    sixel_set_kmedoids_bandit_iter_override(1, 12u);
    sixel_set_kmedoids_bandit_candidates_override(1, 128u);
    sixel_set_kmedoids_bandit_batch_override(1, 64u);

    if (!test_build_dither(SIXEL_PALETTE_KMEDOIDS_ALGO_BANDITPAM,
                           777u,
                           0u,
                           0,
                           &dither_a,
                           allocator)) {
        goto end;
    }
    if (!test_build_dither(SIXEL_PALETTE_KMEDOIDS_ALGO_BANDITPAM,
                           777u,
                           0u,
                           0,
                           &dither_b,
                           allocator)) {
        goto end;
    }
    if (!test_build_dither(SIXEL_PALETTE_KMEDOIDS_ALGO_BANDITPAM,
                           777u,
                           0u,
                           1,
                           &dither_c,
                           allocator)) {
        goto end;
    }
    if (!test_build_dither(SIXEL_PALETTE_KMEDOIDS_ALGO_BANDITPAM,
                           777u,
                           0u,
                           1,
                           &dither_d,
                           allocator)) {
        goto end;
    }

    if (!test_copy_palette_from_dither(dither_a,
                                       allocator,
                                       &palette_a,
                                       &colors_a)) {
        goto end;
    }
    if (!test_copy_palette_from_dither(dither_b,
                                       allocator,
                                       &palette_b,
                                       &colors_b)) {
        goto end;
    }
    if (!test_copy_palette_from_dither(dither_c,
                                       allocator,
                                       &palette_c,
                                       &colors_c)) {
        goto end;
    }
    if (!test_copy_palette_from_dither(dither_d,
                                       allocator,
                                       &palette_d,
                                       &colors_d)) {
        goto end;
    }
    if (colors_a != colors_b || colors_c != colors_d) {
        goto end;
    }
    if (memcmp(palette_a, palette_b, (size_t)colors_a * 3u) != 0) {
        goto end;
    }
    if (memcmp(palette_c, palette_d, (size_t)colors_c * 3u) != 0) {
        goto end;
    }
    ok = 1;

end:
    sixel_set_kmedoids_bandit_iter_override(0, 0u);
    sixel_set_kmedoids_bandit_candidates_override(0, 0u);
    sixel_set_kmedoids_bandit_batch_override(0, 0u);
    if (palette_d != NULL) {
        sixel_allocator_free(allocator, palette_d);
    }
    if (palette_c != NULL) {
        sixel_allocator_free(allocator, palette_c);
    }
    if (palette_b != NULL) {
        sixel_allocator_free(allocator, palette_b);
    }
    if (palette_a != NULL) {
        sixel_allocator_free(allocator, palette_a);
    }
    if (dither_d != NULL) {
        sixel_dither_unref(dither_d);
    }
    if (dither_c != NULL) {
        sixel_dither_unref(dither_c);
    }
    if (dither_b != NULL) {
        sixel_dither_unref(dither_b);
    }
    if (dither_a != NULL) {
        sixel_dither_unref(dither_a);
    }
    if (allocator != NULL) {
        sixel_allocator_unref(allocator);
    }
    return ok;
}

static int
test_run_clarans_residual_eval_order_determinism_case(void)
{
    sixel_allocator_t *allocator;
    unsigned int const point_count = 13u;
    double weights[13u];
    double nearest_dist[13u];
    double scores[13u];
    unsigned int order_a[13u];
    unsigned int order_b[13u];
    unsigned int expected[13u];
    unsigned int index;
    unsigned int probe;
    unsigned int swap_tmp;
    SIXELSTATUS status;

    allocator = NULL;
    index = 0u;
    probe = 0u;
    swap_tmp = 0u;
    status = SIXEL_FALSE;
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL))) {
        return 0;
    }

    for (index = 0u; index < point_count; ++index) {
        weights[index] = (double)((index % 4u) + 1u);
        nearest_dist[index] = (double)((index * 5u + 3u) % 7u);
        nearest_dist[index] += (double)(index % 3u) * 0.25;
        expected[index] = index;
        scores[index] = weights[index] * nearest_dist[index];
    }
    /* Force deterministic tie-break checks. */
    scores[2u] = 4.0;
    scores[5u] = 4.0;
    scores[9u] = 4.0;
    nearest_dist[2u] = 2.0;
    nearest_dist[5u] = 1.0;
    nearest_dist[9u] = 4.0;
    weights[2u] = 2.0;
    weights[5u] = 4.0;
    weights[9u] = 1.0;

    for (index = 0u; index + 1u < point_count; ++index) {
        for (probe = index + 1u; probe < point_count; ++probe) {
            if (test_guided_rank_is_better(scores[expected[probe]],
                                           expected[probe],
                                           scores[expected[index]],
                                           expected[index])) {
                swap_tmp = expected[index];
                expected[index] = expected[probe];
                expected[probe] = swap_tmp;
            }
        }
    }

    status = sixel_kmedoids_test_build_eval_order_residual(weights,
                                                            nearest_dist,
                                                            point_count,
                                                            order_a,
                                                            allocator);
    if (SIXEL_FAILED(status)) {
        sixel_allocator_unref(allocator);
        return 0;
    }
    status = sixel_kmedoids_test_build_eval_order_residual(weights,
                                                            nearest_dist,
                                                            point_count,
                                                            order_b,
                                                            allocator);
    if (SIXEL_FAILED(status)) {
        sixel_allocator_unref(allocator);
        return 0;
    }
    for (index = 0u; index < point_count; ++index) {
        if (order_a[index] != order_b[index]) {
            sixel_allocator_unref(allocator);
            return 0;
        }
        if (order_a[index] != expected[index]) {
            sixel_allocator_unref(allocator);
            return 0;
        }
    }

    sixel_allocator_unref(allocator);
    return 1;
}

static int
test_run_pam_polish_monotonic_case(void)
{
    sixel_allocator_t *allocator;
    double points[TEST_PIXEL_COUNT * 3u];
    double weights[TEST_PIXEL_COUNT];
    unsigned int medoids[TEST_REQCOLORS];
    unsigned int iterations;
    unsigned int index;
    double before_cost;
    double after_cost;
    SIXELSTATUS status;

    allocator = NULL;
    iterations = 0u;
    index = 0u;
    before_cost = 0.0;
    after_cost = 0.0;
    status = SIXEL_FALSE;

    medoids[0u] = 0u;
    medoids[1u] = 2u;
    medoids[2u] = 5u;
    medoids[3u] = 9u;
    for (index = 0u; index < TEST_PIXEL_COUNT * 3u; ++index) {
        points[index] = (double)g_test_pixels_rgb[index];
    }
    for (index = 0u; index < TEST_PIXEL_COUNT; ++index) {
        weights[index] = (double)((index % 4u) + 1u);
    }
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL))) {
        return 0;
    }

    status = sixel_kmedoids_test_pam_polish_cost(points,
                                                 weights,
                                                 TEST_PIXEL_COUNT,
                                                 medoids,
                                                 TEST_REQCOLORS,
                                                 &before_cost,
                                                 &after_cost,
                                                 &iterations,
                                                 allocator);
    sixel_allocator_unref(allocator);
    if (SIXEL_FAILED(status)) {
        return 0;
    }
    if (iterations > 1u) {
        return 0;
    }
    if (after_cost > before_cost + 1.0e-9) {
        return 0;
    }
    return 1;
}

static int
test_run_clarans_delta_consistency_case(void)
{
    double points[TEST_PIXEL_COUNT * 3u];
    double weights[TEST_PIXEL_COUNT];
    unsigned int medoids[TEST_REQCOLORS];
    unsigned int swap_slots[3u];
    unsigned int swap_candidates[3u];
    unsigned int nearest_delta[TEST_PIXEL_COUNT];
    double nearest_dist_delta[TEST_PIXEL_COUNT];
    double second_dist_delta[TEST_PIXEL_COUNT];
    unsigned int second_slot_delta[TEST_PIXEL_COUNT];
    unsigned int nearest_full[TEST_PIXEL_COUNT];
    double nearest_dist_full[TEST_PIXEL_COUNT];
    double second_dist_full[TEST_PIXEL_COUNT];
    unsigned int second_slot_full[TEST_PIXEL_COUNT];
    unsigned int step;
    unsigned int index;
    double cost_delta;
    double cost_full;
    double diff;

    step = 0u;
    index = 0u;
    cost_delta = 0.0;
    cost_full = 0.0;
    diff = 0.0;

    medoids[0u] = 0u;
    medoids[1u] = 2u;
    medoids[2u] = 5u;
    medoids[3u] = 9u;
    swap_slots[0u] = 1u;
    swap_slots[1u] = 3u;
    swap_slots[2u] = 0u;
    swap_candidates[0u] = 7u;
    swap_candidates[1u] = 4u;
    swap_candidates[2u] = 10u;

    for (index = 0u; index < TEST_PIXEL_COUNT * 3u; ++index) {
        points[index] = (double)g_test_pixels_rgb[index];
    }
    for (index = 0u; index < TEST_PIXEL_COUNT; ++index) {
        weights[index] = (double)((index % 4u) + 1u);
    }

    sixel_kmedoids_test_assign_points(points,
                                      weights,
                                      TEST_PIXEL_COUNT,
                                      medoids,
                                      TEST_REQCOLORS,
                                      nearest_delta,
                                      nearest_dist_delta,
                                      second_dist_delta,
                                      second_slot_delta,
                                      &cost_delta);

    for (step = 0u; step < 3u; ++step) {
        medoids[swap_slots[step]] = swap_candidates[step];
        sixel_kmedoids_test_update_assignments_after_swap(
            points,
            weights,
            TEST_PIXEL_COUNT,
            medoids,
            TEST_REQCOLORS,
            swap_slots[step],
            swap_candidates[step],
            nearest_delta,
            nearest_dist_delta,
            second_dist_delta,
            second_slot_delta,
            &cost_delta);

        sixel_kmedoids_test_assign_points(points,
                                          weights,
                                          TEST_PIXEL_COUNT,
                                          medoids,
                                          TEST_REQCOLORS,
                                          nearest_full,
                                          nearest_dist_full,
                                          second_dist_full,
                                          second_slot_full,
                                          &cost_full);

        diff = cost_delta - cost_full;
        if (diff < 0.0) {
            diff = -diff;
        }
        if (diff > 1.0e-9) {
            return 0;
        }

        for (index = 0u; index < TEST_PIXEL_COUNT; ++index) {
            if (nearest_delta[index] != nearest_full[index]) {
                return 0;
            }
            if (second_slot_delta[index] != second_slot_full[index]) {
                return 0;
            }
            diff = nearest_dist_delta[index] - nearest_dist_full[index];
            if (diff < 0.0) {
                diff = -diff;
            }
            if (diff > 1.0e-9) {
                return 0;
            }
            diff = second_dist_delta[index] - second_dist_full[index];
            if (diff < 0.0) {
                diff = -diff;
            }
            if (diff > 1.0e-9) {
                return 0;
            }
        }
    }

    return 1;
}

static int
test_run_clarans_guided_delta_equivalence_case(void)
{
    sixel_allocator_t *allocator;
    unsigned int const point_count = 10u;
    unsigned int const k = 3u;
    unsigned int const hot_point_limit = 5u;
    unsigned int const hot_slot_limit = 2u;
    double weights[10u];
    unsigned char flags_before[10u];
    unsigned char flags_after[10u];
    unsigned int nearest_slot_before[10u];
    unsigned int nearest_slot_after[10u];
    double nearest_dist_before[10u];
    double nearest_dist_after[10u];
    unsigned int changed_points[10u];
    unsigned int hot_points_full[10u];
    unsigned int hot_points_delta[10u];
    unsigned int hot_slots_full[3u];
    unsigned int hot_slots_delta[3u];
    unsigned int hot_point_count_full;
    unsigned int hot_point_count_delta;
    unsigned int hot_slot_count_full;
    unsigned int hot_slot_count_delta;
    unsigned int changed_count;
    unsigned int index;
    SIXELSTATUS status;

    allocator = NULL;
    hot_point_count_full = 0u;
    hot_point_count_delta = 0u;
    hot_slot_count_full = 0u;
    hot_slot_count_delta = 0u;
    changed_count = 5u;
    index = 0u;
    status = SIXEL_FALSE;

    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL))) {
        return 0;
    }

    for (index = 0u; index < point_count; ++index) {
        weights[index] = (double)(index + 1u);
    }

    nearest_slot_before[0u] = 0u;
    nearest_slot_before[1u] = 0u;
    nearest_slot_before[2u] = 1u;
    nearest_slot_before[3u] = 1u;
    nearest_slot_before[4u] = 1u;
    nearest_slot_before[5u] = 2u;
    nearest_slot_before[6u] = 2u;
    nearest_slot_before[7u] = 2u;
    nearest_slot_before[8u] = 0u;
    nearest_slot_before[9u] = 2u;

    nearest_dist_before[0u] = 0.0;
    nearest_dist_before[1u] = 1.0;
    nearest_dist_before[2u] = 2.0;
    nearest_dist_before[3u] = 0.0;
    nearest_dist_before[4u] = 5.0;
    nearest_dist_before[5u] = 1.0;
    nearest_dist_before[6u] = 0.0;
    nearest_dist_before[7u] = 0.1;
    nearest_dist_before[8u] = 3.0;
    nearest_dist_before[9u] = 2.0;

    nearest_slot_after[0u] = 0u;
    nearest_slot_after[1u] = 0u;
    nearest_slot_after[2u] = 1u;
    nearest_slot_after[3u] = 0u;
    nearest_slot_after[4u] = 1u;
    nearest_slot_after[5u] = 2u;
    nearest_slot_after[6u] = 2u;
    nearest_slot_after[7u] = 1u;
    nearest_slot_after[8u] = 0u;
    nearest_slot_after[9u] = 1u;

    nearest_dist_after[0u] = 0.0;
    nearest_dist_after[1u] = 1.0;
    nearest_dist_after[2u] = 1.5;
    nearest_dist_after[3u] = 4.0;
    nearest_dist_after[4u] = 2.0;
    nearest_dist_after[5u] = 1.0;
    nearest_dist_after[6u] = 0.0;
    nearest_dist_after[7u] = 0.0;
    nearest_dist_after[8u] = 3.0;
    nearest_dist_after[9u] = 2.5;

    flags_before[0u] = 1u;
    flags_before[1u] = 0u;
    flags_before[2u] = 0u;
    flags_before[3u] = 1u;
    flags_before[4u] = 0u;
    flags_before[5u] = 0u;
    flags_before[6u] = 1u;
    flags_before[7u] = 0u;
    flags_before[8u] = 0u;
    flags_before[9u] = 0u;

    flags_after[0u] = 1u;
    flags_after[1u] = 0u;
    flags_after[2u] = 0u;
    flags_after[3u] = 0u;
    flags_after[4u] = 0u;
    flags_after[5u] = 0u;
    flags_after[6u] = 1u;
    flags_after[7u] = 1u;
    flags_after[8u] = 0u;
    flags_after[9u] = 0u;

    changed_points[0u] = 2u;
    changed_points[1u] = 3u;
    changed_points[2u] = 4u;
    changed_points[3u] = 7u;
    changed_points[4u] = 9u;

    status = sixel_kmedoids_test_build_clarans_guided_sets(
        weights,
        point_count,
        k,
        nearest_slot_after,
        nearest_dist_after,
        flags_after,
        hot_point_limit,
        hot_slot_limit,
        hot_points_full,
        &hot_point_count_full,
        hot_slots_full,
        &hot_slot_count_full,
        allocator);
    if (SIXEL_FAILED(status)) {
        sixel_allocator_unref(allocator);
        return 0;
    }

    status = sixel_kmedoids_test_apply_clarans_guided_delta(
        weights,
        point_count,
        k,
        nearest_slot_before,
        nearest_dist_before,
        flags_before,
        nearest_slot_after,
        nearest_dist_after,
        flags_after,
        hot_point_limit,
        hot_slot_limit,
        changed_points,
        changed_count,
        3u,
        7u,
        hot_points_delta,
        &hot_point_count_delta,
        hot_slots_delta,
        &hot_slot_count_delta,
        allocator);
    if (SIXEL_FAILED(status)) {
        sixel_allocator_unref(allocator);
        return 0;
    }

    if (hot_point_count_full != hot_point_count_delta
            || hot_slot_count_full != hot_slot_count_delta) {
        sixel_allocator_unref(allocator);
        return 0;
    }
    for (index = 0u; index < hot_point_count_full; ++index) {
        if (hot_points_full[index] != hot_points_delta[index]) {
            sixel_allocator_unref(allocator);
            return 0;
        }
    }
    for (index = 0u; index < hot_slot_count_full; ++index) {
        if (hot_slots_full[index] != hot_slots_delta[index]) {
            sixel_allocator_unref(allocator);
            return 0;
        }
    }

    sixel_allocator_unref(allocator);
    return 1;
}

static int
test_run_clarans_delta_refresh_fallback_case(void)
{
    sixel_allocator_t *allocator;
    unsigned int const point_count = 10u;
    double weights[10u];
    double nearest_before[10u];
    double nearest_after[10u];
    unsigned int order_delta[10u];
    unsigned int order_fallback[10u];
    unsigned int order_full[10u];
    unsigned int changed_points[5u];
    unsigned int index;
    int used_full_refresh;
    SIXELSTATUS status;

    allocator = NULL;
    index = 0u;
    used_full_refresh = 0;
    status = SIXEL_FALSE;
    changed_points[0u] = 2u;
    changed_points[1u] = 3u;
    changed_points[2u] = 4u;
    changed_points[3u] = 7u;
    changed_points[4u] = 9u;
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL))) {
        return 0;
    }

    for (index = 0u; index < point_count; ++index) {
        weights[index] = (double)((index % 5u) + 1u);
        nearest_before[index] = (double)((index * 3u + 1u) % 7u);
        nearest_before[index] += (double)(index % 2u) * 0.2;
        nearest_after[index] = nearest_before[index];
    }
    nearest_after[2u] = nearest_before[2u] * 0.60;
    nearest_after[3u] = nearest_before[3u] + 2.40;
    nearest_after[4u] = nearest_before[4u] * 0.45;
    nearest_after[7u] = nearest_before[7u] * 0.25;
    nearest_after[9u] = nearest_before[9u] + 1.80;

    status = sixel_kmedoids_test_build_eval_order_residual(weights,
                                                            nearest_before,
                                                            point_count,
                                                            order_delta,
                                                            allocator);
    if (SIXEL_FAILED(status)) {
        sixel_allocator_unref(allocator);
        return 0;
    }
    status = sixel_kmedoids_test_build_eval_order_residual(weights,
                                                            nearest_before,
                                                            point_count,
                                                            order_fallback,
                                                            allocator);
    if (SIXEL_FAILED(status)) {
        sixel_allocator_unref(allocator);
        return 0;
    }

    status = sixel_kmedoids_test_apply_eval_order_delta(weights,
                                                         nearest_before,
                                                         nearest_after,
                                                         point_count,
                                                         changed_points,
                                                         5u,
                                                         point_count,
                                                         order_delta,
                                                         &used_full_refresh,
                                                         allocator);
    if (SIXEL_FAILED(status) || used_full_refresh != 0) {
        sixel_allocator_unref(allocator);
        return 0;
    }
    status = sixel_kmedoids_test_apply_eval_order_delta(weights,
                                                         nearest_before,
                                                         nearest_after,
                                                         point_count,
                                                         changed_points,
                                                         5u,
                                                         2u,
                                                         order_fallback,
                                                         &used_full_refresh,
                                                         allocator);
    if (SIXEL_FAILED(status) || used_full_refresh == 0) {
        sixel_allocator_unref(allocator);
        return 0;
    }
    status = sixel_kmedoids_test_build_eval_order_residual(weights,
                                                            nearest_after,
                                                            point_count,
                                                            order_full,
                                                            allocator);
    if (SIXEL_FAILED(status)) {
        sixel_allocator_unref(allocator);
        return 0;
    }
    for (index = 0u; index < point_count; ++index) {
        if (order_delta[index] != order_full[index]) {
            sixel_allocator_unref(allocator);
            return 0;
        }
        if (order_fallback[index] != order_full[index]) {
            sixel_allocator_unref(allocator);
            return 0;
        }
    }
    if (!test_run_clarans_guided_delta_equivalence_case()) {
        sixel_allocator_unref(allocator);
        return 0;
    }

    sixel_allocator_unref(allocator);
    return 1;
}

static int
test_run_clarans_guided_reject_no_rebuild_case(void)
{
    sixel_allocator_t *allocator;
    sixel_dither_t *dither;
    unsigned char pixels[8u * 3u];
    unsigned int index;
    unsigned int local_count;
    unsigned int rebuild_count;
    SIXELSTATUS status;
    int ok;

    allocator = NULL;
    dither = NULL;
    index = 0u;
    local_count = 3u;
    rebuild_count = 0u;
    status = SIXEL_FALSE;
    ok = 0;

    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL))) {
        return 0;
    }

    for (index = 0u; index < 8u; ++index) {
        pixels[index * 3u + 0u] = 64u;
        pixels[index * 3u + 1u] = 96u;
        pixels[index * 3u + 2u] = 128u;
    }

    status = sixel_dither_new(&dither, 2, allocator);
    if (SIXEL_FAILED(status) || dither == NULL) {
        sixel_allocator_unref(allocator);
        return 0;
    }

    dither->quantize_model = SIXEL_QUANTIZE_MODEL_KMEDOIDS;
    dither->final_merge_mode = SIXEL_FINAL_MERGE_NONE;
    dither->prefer_float32 = 0;

    sixel_set_kmedoids_algo_override(1, SIXEL_PALETTE_KMEDOIDS_ALGO_CLARANS);
    sixel_set_kmedoids_seed_override(1, 123u);
    sixel_set_kmedoids_clarans_local_override(1, local_count);
    sixel_set_kmedoids_clarans_neighbors_override(1, 96u);
    sixel_kmedoids_test_reset_clarans_guided_full_build_count();

    status = sixel_dither_initialize(dither,
                                     pixels,
                                     8,
                                     1,
                                     SIXEL_PIXELFORMAT_RGB888,
                                     SIXEL_LARGE_AUTO,
                                     SIXEL_REP_AUTO,
                                     SIXEL_QUALITY_AUTO);
    rebuild_count = sixel_kmedoids_test_get_clarans_guided_full_build_count();
    if (SIXEL_SUCCEEDED(status) && rebuild_count == local_count) {
        ok = 1;
    }

    sixel_set_kmedoids_clarans_neighbors_override(0, 0u);
    sixel_set_kmedoids_clarans_local_override(0, 0u);
    sixel_set_kmedoids_seed_override(0, 1u);
    sixel_set_kmedoids_algo_override(0, SIXEL_PALETTE_KMEDOIDS_ALGO_AUTO);

    sixel_dither_unref(dither);
    sixel_allocator_unref(allocator);
    return ok;
}

static int
test_run_clarans_guided_seed_reproducibility_case(void)
{
    int ok;

    ok = 0;
    sixel_set_kmedoids_clarans_local_override(1, 4u);
    sixel_set_kmedoids_clarans_neighbors_override(1, 120u);

    if (!test_run_seed_case(SIXEL_PALETTE_KMEDOIDS_ALGO_CLARANS, 0)) {
        goto end;
    }
    if (!test_run_seed_case(SIXEL_PALETTE_KMEDOIDS_ALGO_CLARANS, 1)) {
        goto end;
    }
    ok = 1;

end:
    sixel_set_kmedoids_clarans_neighbors_override(0, 0u);
    sixel_set_kmedoids_clarans_local_override(0, 0u);
    return ok;
}

static int
test_run_clarans_exhausted_candidate_skip_determinism_case(void)
{
    if (!test_run_seed_case(SIXEL_PALETTE_KMEDOIDS_ALGO_CLARANS, 0)) {
        return 0;
    }
    if (!test_run_seed_case(SIXEL_PALETTE_KMEDOIDS_ALGO_CLARANS, 1)) {
        return 0;
    }
    return 1;
}

static int
test_run_clarans_adaptive_slot_probe_budget_consistency_case(void)
{
    int ok;

    ok = 0;
    sixel_set_kmedoids_clarans_local_override(1, 5u);
    sixel_set_kmedoids_clarans_neighbors_override(1, 160u);

    if (!test_run_seed_case(SIXEL_PALETTE_KMEDOIDS_ALGO_CLARANS, 0)) {
        goto end;
    }
    if (!test_run_seed_case(SIXEL_PALETTE_KMEDOIDS_ALGO_CLARANS, 1)) {
        goto end;
    }
    ok = 1;

end:
    sixel_set_kmedoids_clarans_neighbors_override(0, 0u);
    sixel_set_kmedoids_clarans_local_override(0, 0u);
    return ok;
}

int
test_palette_0002_kmedoids_constraints(int argc, char **argv)
{
    if (argc < 2) {
        return 1;
    }

    if (strcmp(argv[1], "subset-pam-8bit") == 0) {
        return test_run_subset_case(SIXEL_PALETTE_KMEDOIDS_ALGO_PAM, 0)
            ? 0 : 1;
    }
    if (strcmp(argv[1], "subset-clara-8bit") == 0) {
        return test_run_subset_case(SIXEL_PALETTE_KMEDOIDS_ALGO_CLARA, 0)
            ? 0 : 1;
    }
    if (strcmp(argv[1], "subset-clarans-8bit") == 0) {
        return test_run_subset_case(SIXEL_PALETTE_KMEDOIDS_ALGO_CLARANS, 0)
            ? 0 : 1;
    }
    if (strcmp(argv[1], "subset-banditpam-8bit") == 0) {
        return test_run_subset_case(SIXEL_PALETTE_KMEDOIDS_ALGO_BANDITPAM, 0)
            ? 0 : 1;
    }
    if (strcmp(argv[1], "subset-pam-float32") == 0) {
        return test_run_subset_case(SIXEL_PALETTE_KMEDOIDS_ALGO_PAM, 1)
            ? 0 : 1;
    }
    if (strcmp(argv[1], "subset-clara-float32") == 0) {
        return test_run_subset_case(SIXEL_PALETTE_KMEDOIDS_ALGO_CLARA, 1)
            ? 0 : 1;
    }
    if (strcmp(argv[1], "subset-clarans-float32") == 0) {
        return test_run_subset_case(SIXEL_PALETTE_KMEDOIDS_ALGO_CLARANS, 1)
            ? 0 : 1;
    }
    if (strcmp(argv[1], "subset-banditpam-float32") == 0) {
        return test_run_subset_case(SIXEL_PALETTE_KMEDOIDS_ALGO_BANDITPAM, 1)
            ? 0 : 1;
    }
    if (strcmp(argv[1], "seed-clara") == 0) {
        return test_run_seed_case(SIXEL_PALETTE_KMEDOIDS_ALGO_CLARA, 0)
            ? 0 : 1;
    }
    if (strcmp(argv[1], "seed-clarans") == 0) {
        return test_run_seed_case(SIXEL_PALETTE_KMEDOIDS_ALGO_CLARANS, 0)
            ? 0 : 1;
    }
    if (strcmp(argv[1], "seed-banditpam") == 0) {
        return test_run_seed_case(SIXEL_PALETTE_KMEDOIDS_ALGO_BANDITPAM, 0)
            ? 0 : 1;
    }
    if (strcmp(argv[1], "seed-auto") == 0) {
        return test_run_seed_case(SIXEL_PALETTE_KMEDOIDS_ALGO_AUTO, 0)
            ? 0 : 1;
    }
    if (strcmp(argv[1], "seed-clarans-float32") == 0) {
        return test_run_seed_case(SIXEL_PALETTE_KMEDOIDS_ALGO_CLARANS, 1)
            ? 0 : 1;
    }
    if (strcmp(argv[1], "pam-monotonic-8bit") == 0) {
        return test_run_pam_monotonic_case() ? 0 : 1;
    }
    if (strcmp(argv[1], "clara-sample-indices") == 0) {
        return test_run_clara_sample_indices_case() ? 0 : 1;
    }
    if (strcmp(argv[1], "bandit-delta-consistency") == 0) {
        return test_run_bandit_delta_consistency_case() ? 0 : 1;
    }
    if (strcmp(argv[1], "clarans-swap-cost-cutoff") == 0) {
        return test_run_clarans_swap_cost_cutoff_case() ? 0 : 1;
    }
    if (strcmp(argv[1], "clarans-candidate-cache-equivalence") == 0) {
        return test_run_clarans_candidate_cache_equivalence_case() ? 0 : 1;
    }
    if (strcmp(argv[1], "clarans-candidate-batch-equivalence") == 0) {
        return test_run_clarans_candidate_batch_equivalence_case() ? 0 : 1;
    }
    if (strcmp(argv[1], "clarans-slot-order-cutoff-consistency") == 0) {
        return test_run_clarans_slot_order_cutoff_consistency_case() ? 0 : 1;
    }
    if (strcmp(argv[1], "clarans-slot-order-seed-reproducibility") == 0) {
        return test_run_clarans_slot_order_seed_reproducibility_case()
            ? 0 : 1;
    }
    if (strcmp(argv[1], "bandit-swap-cost-cutoff") == 0) {
        return test_run_bandit_swap_cost_cutoff_case() ? 0 : 1;
    }
    if (strcmp(argv[1], "clarans-delta-consistency") == 0) {
        return test_run_clarans_delta_consistency_case() ? 0 : 1;
    }
    if (strcmp(argv[1], "clarans-guided-tiebreak") == 0) {
        return test_run_clarans_guided_tiebreak_case() ? 0 : 1;
    }
    if (strcmp(argv[1], "clarans-guided-topk-equivalence") == 0) {
        return test_run_clarans_guided_topk_equivalence_case() ? 0 : 1;
    }
    if (strcmp(argv[1], "clarans-residual-eval-order-determinism") == 0) {
        return test_run_clarans_residual_eval_order_determinism_case()
            ? 0 : 1;
    }
    if (strcmp(argv[1], "bandit-generation-seed") == 0) {
        return test_run_bandit_generation_seed_case() ? 0 : 1;
    }
    if (strcmp(argv[1], "pam-polish-monotonic") == 0) {
        return test_run_pam_polish_monotonic_case() ? 0 : 1;
    }
    if (strcmp(argv[1], "sample-representative-nearest-mean") == 0) {
        return test_run_sample_representative_nearest_mean_case() ? 0 : 1;
    }
    if (strcmp(argv[1], "bandit-guided-seed-reproducibility") == 0) {
        return test_run_bandit_guided_seed_reproducibility_case() ? 0 : 1;
    }
    if (strcmp(argv[1], "polish-two-step-monotonic") == 0) {
        return test_run_polish_two_step_monotonic_case() ? 0 : 1;
    }
    if (strcmp(argv[1], "clarans-guided-delta-equivalence") == 0) {
        return test_run_clarans_guided_delta_equivalence_case() ? 0 : 1;
    }
    if (strcmp(argv[1], "clarans-delta-refresh-fallback") == 0) {
        return test_run_clarans_delta_refresh_fallback_case() ? 0 : 1;
    }
    if (strcmp(argv[1], "clarans-guided-reject-no-rebuild") == 0) {
        return test_run_clarans_guided_reject_no_rebuild_case() ? 0 : 1;
    }
    if (strcmp(argv[1], "clarans-guided-seed-reproducibility") == 0) {
        return test_run_clarans_guided_seed_reproducibility_case() ? 0 : 1;
    }
    if (strcmp(argv[1], "clarans-exhausted-candidate-skip-determinism") == 0) {
        return test_run_clarans_exhausted_candidate_skip_determinism_case()
            ? 0 : 1;
    }
    if (strcmp(argv[1], "clarans-adaptive-slot-probe-budget-consistency")
            == 0) {
        return test_run_clarans_adaptive_slot_probe_budget_consistency_case()
            ? 0 : 1;
    }

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
