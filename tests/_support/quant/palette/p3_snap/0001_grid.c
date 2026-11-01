/* SPDX-License-Identifier: MIT */
/* Destination grid: independent values, cache isolation and failure bounds. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/palette-snap-grid.h"
#include "src/palette-common-snap.h"
#include "src/palette-kmeans.h"
#include "src/palette.h"
#include "tests/data/color-numeric/p3-snap.h"

static SIXELSTATUS
p3_test_grid_new(sixel_snap_grid_t **grid, int policy, int colorspace,
                   sixel_allocator_t *allocator)
{
    sixel_color_def_t *destination;
    SIXELSTATUS status;

    destination = sixel_cdef_name("display-p3");
    status = sixel_snap_grid_new(grid, policy, colorspace, destination,
                                allocator);
    sixel_cdef_free(destination);
    return status;
}

static int const p3_grid_spaces[5] = {
    SIXEL_COLORSPACE_GAMMA, SIXEL_COLORSPACE_LINEAR, SIXEL_COLORSPACE_OKLAB,
    SIXEL_COLORSPACE_CIELAB, SIXEL_COLORSPACE_DIN99D
};

static int
p3_grid_close(double const actual[3], double const expected[3])
{
    int channel;

    /* 2e-5 typed units covers the independent matrix/TRC inversion and
     * opponent transforms. The oracle generator excludes near-tie corners;
     * adjacent candidates differ by substantially more than this budget. */
    for (channel = 0; channel < 3; ++channel) {
        if (!isfinite(actual[channel]) ||
                fabs(actual[channel] - expected[channel]) > 2e-5) {
            fprintf(stderr, "channel %d: %.17g expected %.17g\n",
                channel, actual[channel], expected[channel]);
            return 0;
        }
    }
    return 1;
}

static int
p3_grid_values(int strategy, int space)
{
    sixel_allocator_t *allocator;
    sixel_snap_grid_t *grid;
    double actual[3];
    int fixture;
    int policy;
    int repeat;
    int ok;

    grid = NULL;
    allocator = NULL;
    ok = 0;
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL)) ||
            SIXEL_FAILED(p3_test_grid_new(&grid, strategy,
                p3_grid_spaces[space], allocator))) {
        goto end;
    }
    /* Repetition observes hits without replacing independent correctness
     * with an equality comparison between two production cache strategies. */
    for (repeat = 0; repeat < 2; ++repeat) {
        for (fixture = 0; fixture < 3; ++fixture) {
            for (policy = 0; policy < 2; ++policy) {
                if (SIXEL_FAILED(sixel_snap_grid_target(grid,
                        p3_snap_codes[fixture][space][policy], actual)) ||
                        !p3_grid_close(actual,
                            p3_snap_values[fixture][space][policy + 1])) {
                    goto end;
                }
                if (SIXEL_FAILED(sixel_snap_grid_project(grid,
                        p3_snap_values[fixture][space][0], policy == 0,
                        .85, actual)) || !p3_grid_close(actual,
                            p3_snap_values[fixture][space][policy + 1])) {
                    goto end;
                }
            }
        }
    }
    ok = 1;
end:
    sixel_snap_grid_destroy(grid);
    sixel_allocator_unref(allocator);
    return ok;
}

static int
p3_grid_isolation(void)
{
    sixel_allocator_t *allocator;
    sixel_snap_grid_t *first;
    sixel_snap_grid_t *second;
    double actual[3];
    unsigned int code;
    int ok;

    first = second = NULL;
    allocator = NULL;
    ok = 0;
    code = p3_snap_codes[0][0][0];
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL)) ||
            SIXEL_FAILED(p3_test_grid_new(&first, SIXEL_SNAP_GRID_CACHE,
                SIXEL_COLORSPACE_GAMMA, allocator)) ||
            SIXEL_FAILED(p3_test_grid_new(&second, SIXEL_SNAP_GRID_CACHE,
                SIXEL_COLORSPACE_LINEAR, allocator)) ||
            SIXEL_FAILED(sixel_snap_grid_target(first, code, actual)) ||
            !p3_grid_close(actual, p3_snap_values[0][0][1]) ||
            SIXEL_FAILED(sixel_snap_grid_target(second,
                p3_snap_codes[0][1][0], actual)) ||
            !p3_grid_close(actual, p3_snap_values[0][1][1])) {
        goto end;
    }
    sixel_snap_grid_destroy(second);
    second = NULL;
    ok = SIXEL_SUCCEEDED(sixel_snap_grid_target(first, code, actual)) &&
        p3_grid_close(actual, p3_snap_values[0][0][1]);
end:
    sixel_snap_grid_destroy(second);
    sixel_snap_grid_destroy(first);
    sixel_allocator_unref(allocator);
    return ok;
}

static int
p3_grid_invalid(void)
{
    sixel_allocator_t *allocator;
    sixel_snap_grid_t *grid;
    double source[3] = {0.0, 0.0, 0.0};
    double actual[3] = {9.0, 8.0, 7.0};
    double saved[3];
    int ok;

    grid = NULL;
    allocator = NULL;
    ok = 0;
    memcpy(saved, actual, sizeof(saved));
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL)) ||
            SIXEL_SUCCEEDED(p3_test_grid_new(&grid, -1,
                SIXEL_COLORSPACE_LINEAR, allocator)) || grid != NULL ||
            SIXEL_SUCCEEDED(p3_test_grid_new(&grid, SIXEL_SNAP_GRID_TABLE,
                -1, allocator)) || grid != NULL ||
            SIXEL_FAILED(p3_test_grid_new(&grid, SIXEL_SNAP_GRID_TABLE,
                SIXEL_COLORSPACE_LINEAR, allocator))) {
        goto end;
    }
    source[1] = (double)NAN;
    ok = SIXEL_FAILED(sixel_snap_grid_target(grid, 1030301U, actual)) &&
        SIXEL_FAILED(sixel_snap_grid_project(grid, source, 1, .85, actual)) &&
        memcmp(saved, actual, sizeof(saved)) == 0;
    source[1] = 0.0;
    ok = ok && SIXEL_SUCCEEDED(sixel_snap_grid_project(grid, source,
        1, .85, actual)) && actual[0] == 0.0 && actual[1] == 0.0 &&
        actual[2] == 0.0;
end:
    sixel_snap_grid_destroy(grid);
    sixel_allocator_unref(allocator);
    return ok;
}

static int
p3_grid_seed(int pca)
{
    sixel_allocator_t *allocator;
    sixel_palette_snap_options_t options;
    double samples[6];
    double center[3];
    double expected[3];
    double distances[2];
    double weights[2] = {1.0, 1.0};
    uint32_t rng;
    int channel;
    int ok;

    allocator = NULL;
    ok = 0;
    rng = 1U;
    memset(&options, 0, sizeof(options));
    options.policy_override = options.timing_override = 1;
    options.policy = SIXEL_PALETTE_SNAP_POLICY_NEAREST;
    options.timing = SIXEL_PALETTE_SNAP_TIMING_SEED;
    options.approach_rate_override = options.channel_factor_l_override = 1;
    options.approach_rate = .25;
    options.channel_factor_l = .85;
    sixel_set_palette_snap_override(&options);
    sixel_palette_snap_set_output(1, 1, SIXEL_SNAP_GRID_TABLE);
    for (channel = 0; channel < 3; ++channel) {
        samples[channel] = samples[channel + 3] =
            p3_snap_values[2][2][0][channel];
        expected[channel] = .75 * samples[channel] +
            .25 * p3_snap_values[2][2][1][channel];
    }
    /* Distinct symmetric PCA samples have the same independent mean. A
     * fallback to the legacy seed would select an endpoint and fail. */
    if (pca) {
        samples[0] -= .01;
        samples[3] += .01;
    }
    if (SIXEL_SUCCEEDED(sixel_allocator_new(&allocator, NULL, NULL, NULL,
            NULL)) && SIXEL_SUCCEEDED(sixel_palette_snap_begin(
                SIXEL_PIXELFORMAT_OKLABFLOAT32,
                SIXEL_QUANTIZE_MODEL_KMEANS, allocator)) &&
            SIXEL_SUCCEEDED(sixel_kmeans_choose_initial_centroids(center, 1,
                samples, weights, 2, 1, SIXEL_PIXELFORMAT_OKLABFLOAT32,
                distances, allocator, pca ? SIXEL_PALETTE_KMEANS_INIT_PCA
                : SIXEL_PALETTE_KMEANS_INIT_NONE, &rng))) {
        ok = p3_grid_close(center, expected);
    }
    ok = SIXEL_SUCCEEDED(sixel_palette_snap_end()) && ok;
    sixel_set_palette_snap_override(NULL);
    sixel_allocator_unref(allocator);
    return ok;
}

/* Solver merge coordinates have a nominal byte scale. CIELAB's L range
 * is 1 and its signed a/b range is 3. Heckbert omits the -1.5 origin shift;
 * the other solvers include it. These are representation contracts, not
 * values obtained by calling the production packing helper. */
static int
p3_grid_scaled(int zero_origin)
{
    sixel_allocator_t *allocator;
    sixel_palette_snap_options_t options;
    double actual[3];
    double expected[3];
    double minimum;
    double range;
    int channel;
    int ok;

    allocator = NULL;
    ok = 0;
    memset(&options, 0, sizeof(options));
    options.policy_override = options.timing_override = 1;
    options.policy = SIXEL_PALETTE_SNAP_POLICY_NEAREST;
    options.timing = SIXEL_PALETTE_SNAP_TIMING_PREMERGE;
    options.approach_rate_override = options.channel_factor_l_override = 1;
    options.approach_rate = .25;
    options.channel_factor_l = .85;
    sixel_set_palette_snap_override(&options);
    sixel_palette_snap_set_output(1, 1, SIXEL_SNAP_GRID_TABLE);
    if (SIXEL_SUCCEEDED(sixel_allocator_new(&allocator, NULL, NULL, NULL,
            NULL)) && SIXEL_SUCCEEDED(sixel_palette_snap_begin(
                SIXEL_PIXELFORMAT_CIELABFLOAT32, zero_origin
                    ? SIXEL_QUANTIZE_MODEL_MEDIANCUT
                    : SIXEL_QUANTIZE_MODEL_KMEANS, allocator))) {
        for (channel = 0; channel < 3; ++channel) {
            minimum = channel == 0 || zero_origin ? 0.0 : -1.5;
            range = channel == 0 ? 1.0 : 3.0;
            actual[channel] = (p3_snap_values[2][3][0][channel] - minimum)
                * 255.0 / range;
            expected[channel] = .75 * p3_snap_values[2][3][0][channel] +
                .25 * p3_snap_values[2][3][1][channel];
        }
        sixel_palette_snap_scaled_triple(actual,
            SIXEL_PIXELFORMAT_CIELABFLOAT32,
            SIXEL_PALETTE_SNAP_STAGE_FINAL_MERGE_PRE);
        for (channel = 0; channel < 3; ++channel) {
            minimum = channel == 0 || zero_origin ? 0.0 : -1.5;
            range = channel == 0 ? 1.0 : 3.0;
            actual[channel] = actual[channel] * range / 255.0 + minimum;
        }
        ok = p3_grid_close(actual, expected);
    }
    ok = SIXEL_SUCCEEDED(sixel_palette_snap_end()) && ok;
    sixel_set_palette_snap_override(NULL);
    sixel_allocator_unref(allocator);
    return ok;
}

static int p3_grid_fail_after;
static int p3_grid_live;

static void *
p3_grid_malloc(size_t size)
{
    void *result;

    if (p3_grid_fail_after-- == 0) {
        return NULL;
    }
    result = malloc(size);
    p3_grid_live += result != NULL;
    return result;
}

static void *
p3_grid_calloc(size_t count, size_t size)
{
    void *result;

    result = p3_grid_malloc(count * size);
    if (result != NULL) {
        memset(result, 0, count * size);
    }
    return result;
}

static void
p3_grid_free(void *memory)
{
    p3_grid_live -= memory != NULL;
    free(memory);
}

static int
p3_grid_failure(int full)
{
    sixel_allocator_t *allocator;
    sixel_snap_grid_t *grid;
    int baseline;
    int allocation;
    int ok;

    allocator = NULL;
    grid = NULL;
    p3_grid_fail_after = -1;
    p3_grid_live = 0;
    ok = 0;
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, p3_grid_malloc,
            p3_grid_calloc, realloc, p3_grid_free))) {
        return 0;
    }
    baseline = p3_grid_live;
    /* Fail the object or its optional storage. Neither may select a cheaper
     * method silently or leak its already-created profile/allocator owner. */
    for (allocation = 0; allocation < 2; ++allocation) {
        p3_grid_fail_after = allocation;
        if (p3_test_grid_new(&grid, full ? SIXEL_SNAP_GRID_FULL
                : SIXEL_SNAP_GRID_CACHE, SIXEL_COLORSPACE_LINEAR,
                allocator) != SIXEL_BAD_ALLOCATION || grid != NULL ||
                p3_grid_live != baseline) {
            goto end;
        }
    }
    p3_grid_fail_after = -1;
    ok = SIXEL_SUCCEEDED(p3_test_grid_new(&grid, SIXEL_SNAP_GRID_TABLE,
        SIXEL_COLORSPACE_LINEAR, allocator));
end:
    sixel_snap_grid_destroy(grid);
    sixel_allocator_unref(allocator);
    return ok && p3_grid_live == 0;
}

int
test_p3_snap_grid(int argc, char **argv)
{
    int strategy;
    int space;

    if (argc == 3 && strcmp(argv[1], "seed") == 0) {
        return !p3_grid_seed(atoi(argv[2]));
    }
    if (argc == 3 && strcmp(argv[1], "scaled") == 0) {
        return !p3_grid_scaled(atoi(argv[2]) != 0);
    }
    if (argc == 3 && strcmp(argv[1], "failure") == 0) {
        return !p3_grid_failure(atoi(argv[2]));
    }
    if (argc == 2 && strcmp(argv[1], "isolation") == 0) {
        return !p3_grid_isolation();
    }
    if (argc == 2 && strcmp(argv[1], "invalid") == 0) {
        return !p3_grid_invalid();
    }
    if (argc != 3) {
        return 1;
    }
    strategy = atoi(argv[1]);
    space = atoi(argv[2]);
    if (space < 0 || space > 4) {
        return 1;
    }
    return !p3_grid_values(strategy, space);
}
