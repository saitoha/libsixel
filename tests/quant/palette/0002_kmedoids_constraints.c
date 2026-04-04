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

#include <sixel.h>

#include "src/dither.h"
#include "src/palette-kmedoids.h"

#define TEST_PIXEL_COUNT 12u
#define TEST_WIDTH 12
#define TEST_HEIGHT 1
#define TEST_REQCOLORS 4

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
                  int use_float32,
                  sixel_dither_t **dither_out,
                  sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    sixel_dither_t *dither;
    float float_pixels[TEST_PIXEL_COUNT * 3u];
    unsigned int index;
    int pixelformat;

    status = SIXEL_FALSE;
    dither = NULL;
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

    if (SIXEL_FAILED(status)
            || sixel_dither_get_palette(dither) == NULL
            || sixel_dither_get_num_of_palette_colors(dither) <= 0) {
        sixel_dither_unref(dither);
        return 0;
    }

    *dither_out = dither;
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
    int ncolors;

    allocator = NULL;
    dither = NULL;
    allowed_count = 0u;
    palette = NULL;
    ncolors = 0;

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

    if (!test_build_dither(algo, 7u, use_float32, &dither, allocator)) {
        sixel_allocator_unref(allocator);
        return 0;
    }

    palette = sixel_dither_get_palette(dither);
    ncolors = sixel_dither_get_num_of_palette_colors(dither);
    if (!test_verify_palette_subset(palette,
                                    (unsigned int)ncolors,
                                    allowed,
                                    allowed_count)) {
        sixel_dither_unref(dither);
        sixel_allocator_unref(allocator);
        return 0;
    }

    sixel_dither_unref(dither);
    sixel_allocator_unref(allocator);
    return 1;
}

static int
test_run_seed_case(sixel_kmedoids_algo_t algo)
{
    sixel_allocator_t *allocator;
    sixel_dither_t *dither_a;
    sixel_dither_t *dither_b;
    unsigned char *palette_a;
    unsigned char *palette_b;
    int colors_a;
    int colors_b;

    allocator = NULL;
    dither_a = NULL;
    dither_b = NULL;
    palette_a = NULL;
    palette_b = NULL;
    colors_a = 0;
    colors_b = 0;

    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL))) {
        return 0;
    }

    if (!test_build_dither(algo, 1234u, 0, &dither_a, allocator)) {
        sixel_allocator_unref(allocator);
        return 0;
    }
    if (!test_build_dither(algo, 1234u, 0, &dither_b, allocator)) {
        sixel_dither_unref(dither_a);
        sixel_allocator_unref(allocator);
        return 0;
    }

    palette_a = sixel_dither_get_palette(dither_a);
    palette_b = sixel_dither_get_palette(dither_b);
    colors_a = sixel_dither_get_num_of_palette_colors(dither_a);
    colors_b = sixel_dither_get_num_of_palette_colors(dither_b);
    if (colors_a != colors_b) {
        sixel_dither_unref(dither_a);
        sixel_dither_unref(dither_b);
        sixel_allocator_unref(allocator);
        return 0;
    }
    if (memcmp(palette_a, palette_b, (size_t)colors_a * 3u) != 0) {
        sixel_dither_unref(dither_a);
        sixel_dither_unref(dither_b);
        sixel_allocator_unref(allocator);
        return 0;
    }

    sixel_dither_unref(dither_a);
    sixel_dither_unref(dither_b);
    sixel_allocator_unref(allocator);
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
        return test_run_seed_case(SIXEL_PALETTE_KMEDOIDS_ALGO_CLARA)
            ? 0 : 1;
    }
    if (strcmp(argv[1], "seed-clarans") == 0) {
        return test_run_seed_case(SIXEL_PALETTE_KMEDOIDS_ALGO_CLARANS)
            ? 0 : 1;
    }
    if (strcmp(argv[1], "seed-banditpam") == 0) {
        return test_run_seed_case(SIXEL_PALETTE_KMEDOIDS_ALGO_BANDITPAM)
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
