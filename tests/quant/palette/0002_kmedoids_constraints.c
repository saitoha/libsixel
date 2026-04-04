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
    if (strcmp(argv[1], "clarans-delta-consistency") == 0) {
        return test_run_clarans_delta_consistency_case() ? 0 : 1;
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
