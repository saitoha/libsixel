/* SPDX-License-Identifier: MIT
 * Saturating the distinct-color counter must not truncate the aligned
 * projections needed by later visible samples. Gray codes 4/5/6 all have
 * safe code 5; float storage contributes less than 1e-7 normalized RGB.
 */
#include <math.h>
#include <stdlib.h>
#include "src/colorspace.h"
#include "src/sample-statistics.h"

int
test_0024_range_projection_after_saturation(int argc, char **argv)
{
    sixel_allocator_t *allocator;
    sixel_sample_statistics_t *statistics;
    SIXELSTATUS status;
    double coordinates[3];
    unsigned int index;
    unsigned int axis;
    int result;

    (void)argc;
    (void)argv;
    allocator = NULL;
    statistics = NULL;
    result = EXIT_FAILURE;
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_sample_statistics_new(&statistics,
        SIXEL_COLORSPACE_GAMMA, 0, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    for (index = 0u; index < 1503u; ++index) {
        coordinates[0] = ((index % 16u) * 16u) / 255.0;
        coordinates[1] = (((index / 16u) % 16u) * 16u) / 255.0;
        coordinates[2] = ((index / 256u) * 32u) / 255.0;
        if (index >= 1500u) {
            for (axis = 0u; axis < 3u; ++axis) {
                coordinates[axis] = (index - 1496u) / 255.0;
            }
        }
        status = sixel_sample_statistics_observe(statistics, coordinates);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }
    if (statistics->point_count != 1503u ||
            statistics->safe_color_count != SIXEL_SAMPLE_SAFE_LIMIT) {
        goto end;
    }
    for (index = 1500u; index < 1503u; ++index) {
        for (axis = 0u; axis < 3u; ++axis) {
            if (!isfinite(statistics->safe_coordinates[index * 3u + axis]) ||
                    fabs(statistics->safe_coordinates[index * 3u + axis] -
                        5.0 / 255.0) > 1e-7) {
                goto end;
            }
        }
    }
    result = EXIT_SUCCESS;
end:
    sixel_sample_statistics_unref(statistics);
    sixel_allocator_unref(allocator);
    return result;
}
