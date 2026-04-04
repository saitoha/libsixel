/*
 * Helper exposing k-medoids constraints for TAP shell wrappers.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "src/allocator.h"
#include "src/palette.h"
#include "src/palette-kmedoids.h"

#define TEST_PIXEL_COUNT 12u
#define TEST_REQCOLORS 4u

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
test_build_palette(sixel_kmedoids_algo_t algo,
                   uint32_t seed,
                   int use_float32,
                   unsigned char **palette_out,
                   unsigned int *ncolors_out,
                   sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    unsigned char *palette;
    unsigned int ncolors;
    unsigned int origcolors;
    float float_pixels[TEST_PIXEL_COUNT * 3u];
    unsigned int index;

    status = SIXEL_FALSE;
    palette = NULL;
    ncolors = 0u;
    origcolors = 0u;
    index = 0u;
    if (palette_out == NULL || ncolors_out == NULL || allocator == NULL) {
        return 0;
    }
    *palette_out = NULL;
    *ncolors_out = 0u;

    sixel_set_kmedoids_algo_override(1, algo);
    sixel_set_kmedoids_seed_override(1, seed);

    if (use_float32) {
        for (index = 0u; index < TEST_PIXEL_COUNT * 3u; ++index) {
            float_pixels[index] = (float)g_test_pixels_rgb[index] / 255.0f;
        }
        status = sixel_palette_make_palette(&palette,
                                            float_pixels,
                                            sizeof(float_pixels),
                                            SIXEL_PIXELFORMAT_RGBFLOAT32,
                                            TEST_REQCOLORS,
                                            &ncolors,
                                            &origcolors,
                                            SIXEL_LARGE_AUTO,
                                            SIXEL_REP_AUTO,
                                            SIXEL_QUALITY_AUTO,
                                            0,
                                            0,
                                            SIXEL_QUANTIZE_MODEL_KMEDOIDS,
                                            SIXEL_FINAL_MERGE_NONE,
                                            1,
                                            allocator);
    } else {
        status = sixel_palette_make_palette(&palette,
                                            g_test_pixels_rgb,
                                            sizeof(g_test_pixels_rgb),
                                            SIXEL_PIXELFORMAT_RGB888,
                                            TEST_REQCOLORS,
                                            &ncolors,
                                            &origcolors,
                                            SIXEL_LARGE_AUTO,
                                            SIXEL_REP_AUTO,
                                            SIXEL_QUALITY_AUTO,
                                            0,
                                            0,
                                            SIXEL_QUANTIZE_MODEL_KMEDOIDS,
                                            SIXEL_FINAL_MERGE_NONE,
                                            0,
                                            allocator);
    }

    sixel_set_kmedoids_algo_override(0, SIXEL_PALETTE_KMEDOIDS_ALGO_PAM);
    sixel_set_kmedoids_seed_override(0, 1u);

    if (SIXEL_FAILED(status) || palette == NULL || ncolors == 0u) {
        if (palette != NULL) {
            sixel_palette_free_palette(palette, allocator);
        }
        return 0;
    }

    *palette_out = palette;
    *ncolors_out = ncolors;
    return 1;
}

static int
test_check_medoid_subset(void)
{
    sixel_allocator_t *allocator;
    unsigned char allowed[TEST_PIXEL_COUNT * 3u];
    unsigned int allowed_count;
    unsigned char *palette;
    unsigned int ncolors;
    unsigned int algo_index;
    int float_mode;
    sixel_kmedoids_algo_t algo_values[4];

    allocator = NULL;
    allowed_count = 0u;
    palette = NULL;
    ncolors = 0u;
    algo_index = 0u;
    float_mode = 0;
    algo_values[0] = SIXEL_PALETTE_KMEDOIDS_ALGO_PAM;
    algo_values[1] = SIXEL_PALETTE_KMEDOIDS_ALGO_CLARA;
    algo_values[2] = SIXEL_PALETTE_KMEDOIDS_ALGO_CLARANS;
    algo_values[3] = SIXEL_PALETTE_KMEDOIDS_ALGO_BANDITPAM;

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

    for (float_mode = 0; float_mode <= 1; ++float_mode) {
        for (algo_index = 0u; algo_index < 4u; ++algo_index) {
            if (!test_build_palette(algo_values[algo_index],
                                    7u,
                                    float_mode,
                                    &palette,
                                    &ncolors,
                                    allocator)) {
                sixel_allocator_unref(allocator);
                return 0;
            }
            if (!test_verify_palette_subset(palette,
                                            ncolors,
                                            allowed,
                                            allowed_count)) {
                sixel_palette_free_palette(palette, allocator);
                sixel_allocator_unref(allocator);
                return 0;
            }
            sixel_palette_free_palette(palette, allocator);
            palette = NULL;
            ncolors = 0u;
        }
    }

    sixel_allocator_unref(allocator);
    return 1;
}

static int
test_check_seed_reproducibility(void)
{
    sixel_allocator_t *allocator;
    unsigned char *palette_a;
    unsigned char *palette_b;
    unsigned int colors_a;
    unsigned int colors_b;
    unsigned int algo_index;
    sixel_kmedoids_algo_t algo_values[3];

    allocator = NULL;
    palette_a = NULL;
    palette_b = NULL;
    colors_a = 0u;
    colors_b = 0u;
    algo_index = 0u;
    algo_values[0] = SIXEL_PALETTE_KMEDOIDS_ALGO_CLARA;
    algo_values[1] = SIXEL_PALETTE_KMEDOIDS_ALGO_CLARANS;
    algo_values[2] = SIXEL_PALETTE_KMEDOIDS_ALGO_BANDITPAM;

    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL))) {
        return 0;
    }

    for (algo_index = 0u; algo_index < 3u; ++algo_index) {
        if (!test_build_palette(algo_values[algo_index],
                                1234u,
                                0,
                                &palette_a,
                                &colors_a,
                                allocator)) {
            sixel_allocator_unref(allocator);
            return 0;
        }
        if (!test_build_palette(algo_values[algo_index],
                                1234u,
                                0,
                                &palette_b,
                                &colors_b,
                                allocator)) {
            sixel_palette_free_palette(palette_a, allocator);
            sixel_allocator_unref(allocator);
            return 0;
        }

        if (colors_a != colors_b) {
            sixel_palette_free_palette(palette_a, allocator);
            sixel_palette_free_palette(palette_b, allocator);
            sixel_allocator_unref(allocator);
            return 0;
        }
        if (memcmp(palette_a,
                   palette_b,
                   (size_t)colors_a * 3u) != 0) {
            sixel_palette_free_palette(palette_a, allocator);
            sixel_palette_free_palette(palette_b, allocator);
            sixel_allocator_unref(allocator);
            return 0;
        }

        sixel_palette_free_palette(palette_a, allocator);
        sixel_palette_free_palette(palette_b, allocator);
        palette_a = NULL;
        palette_b = NULL;
        colors_a = 0u;
        colors_b = 0u;
    }

    sixel_allocator_unref(allocator);
    return 1;
}

int
test_palette_0002_kmedoids_constraints(int argc, char **argv)
{
    int run_seed;

    run_seed = 0;
    if (argc > 1 && strcmp(argv[1], "--seed") == 0) {
        run_seed = 1;
    }

    if (run_seed) {
        return test_check_seed_reproducibility() ? 0 : 1;
    }

    return test_check_medoid_subset() ? 0 : 1;
}
