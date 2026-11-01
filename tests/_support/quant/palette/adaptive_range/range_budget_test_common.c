/* SPDX-License-Identifier: MIT
 * Direct address checks use a complete small sampled population. The bounds
 * are derived from requested colors and independently enumerated RGB codes,
 * not from the implementation's diagnostic counters.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "src/colorspace.h"
#include "src/palette-bin-histogram.h"
#include "range_budget_test_common.h"

int
range_budget_test(int scenario)
{
    sixel_allocator_t *allocator;
    sixel_sample_statistics_t *statistics;
    sixel_weighted_point_set_t points;
    sixel_palette_bin_range_t range;
    SIXELSTATUS status;
    double coordinates[256 * 3];
    double scale[3];
    double offset[3];
    unsigned char seen[32 * 32 * 32];
    unsigned int keys[256];
    unsigned int index;
    unsigned int axis;
    unsigned int count;
    unsigned int key;
    unsigned int bin;
    unsigned int used;
    unsigned int strength;
    unsigned int limit;
    int space;
    int bytes;
    int result;

    allocator = NULL;
    statistics = NULL;
    result = EXIT_FAILURE;
    count = scenario == 22 || scenario == 25 || scenario == 27 ? 256u : 24u;
    strength = scenario == 25 ? 50u : 100u;
    space = scenario == 23 ? SIXEL_COLORSPACE_CIELAB : SIXEL_COLORSPACE_GAMMA;
    bytes = scenario != 23;
    memset(seen, 0, sizeof(seen));
    sixel_weighted_point_set_init(&points);
    sixel_palette_bin_range_init(&range, 32u);
    for (axis = 0u; axis < 3u; ++axis) {
        scale[axis] = 255.0;
        offset[axis] = 0.0;
    }
    if (scenario == 23) {
        scale[0] = 2.55;
        scale[1] = scale[2] = 1.0;
        offset[1] = offset[2] = 128.0;
    }
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_sample_statistics_new(&statistics, space, bytes, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    for (index = 0u; index < count; ++index) {
        if (count == 256u) {
            coordinates[index * 3u] = (index % 8u) * 3u;
            coordinates[index * 3u + 1u] = ((index / 8u) % 8u) * 3u;
            coordinates[index * 3u + 2u] = (index / 64u) * 3u;
        } else {
            for (axis = 0u; axis < 3u; ++axis) {
                coordinates[index * 3u + axis] = index;
            }
            if (scenario == 23) {
                /* Neutral CIELAB from linear sRGB's dark segment. For
                 * codes 4, 5 and 6 the RGB rounding boundary is 0.5 code
                 * away; all three have the literal safe output code 5. */
                coordinates[index * 3u] = (24389.0 / 27.0) *
                    ((double)index / (255.0 * 12.92));
                coordinates[index * 3u + 1u] = 0.0;
                coordinates[index * 3u + 2u] = 0.0;
            }
        }
        status = sixel_sample_statistics_observe(statistics,
            coordinates + index * 3u);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }
    points.coordinates = coordinates;
    points.point_count = count;
    status = sixel_palette_bin_range_prepare(&range, statistics, NULL,
        &points, count == 256u ? 32u : 256u, scale, offset,
        SIXEL_PALETTE_BINNING_GRID_NONE, 1, NULL, strength,
        scenario == 27 ? SIXEL_QUANTIZE_MODEL_MEDIANCUT :
            SIXEL_QUANTIZE_MODEL_KMEDOIDS);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    used = 0u;
    for (index = 0u; index < count; ++index) {
        key = 0u;
        for (axis = 0u; axis < 3u; ++axis) {
            bin = (unsigned int)(coordinates[index * 3u + axis] *
                (bytes ? 1.0 : scale[axis]) +
                (bytes ? 0.0 : offset[axis])) / 8u;
            key = key * 32u + sixel_palette_bin_range_sample_index(&range,
                axis, bin, coordinates[index * 3u + axis], index);
        }
        keys[index] = key;
        if (!seen[key]) {
            seen[key] = 1u;
            ++used;
        }
    }
    if (scenario == 27) {
        /* Median-cut has a P budget instead of the medoid fitter's 2P.
         * A discrete refinement may not fit: retaining all 18 nominal
         * cells is valid, exceeding the 32-candidate cap is not. */
        result = used >= 18u && used <= 32u ? EXIT_SUCCESS : EXIT_FAILURE;
    } else if (scenario == 22 || scenario == 25) {
        /* 3 x 3 x 2 nominal bins; the 2P budget is 64. */
        limit = scenario == 25 ? 18u + (64u - 18u) / 2u : 64u;
        result = used >= 18u && used <= limit &&
            (scenario == 25 || used > 18u) ? EXIT_SUCCESS : EXIT_FAILURE;
    } else if (scenario == 23) {
        result = used > 1u && keys[4] == keys[5] && keys[5] == keys[6]
            ? EXIT_SUCCESS : EXIT_FAILURE;
    } else {
        /* Codes 7 and 8 share safe tone 8, but belong to distinct nominal
         * bins. Refinement must not merge those pre-existing intervals. */
        result = keys[7] != keys[8] ? EXIT_SUCCESS : EXIT_FAILURE;
    }
end:
    sixel_sample_statistics_unref(statistics);
    sixel_allocator_unref(allocator);
    return result;
}
