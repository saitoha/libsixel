/* SPDX-License-Identifier: MIT */
#include "tests/_support/lso2-gpu/fixture.h"
#include "tests/_support/lso2-gpu/expected_taps.h"

/* GPU diffusion through the NONE lookup program for every fixed-tap method,
 * on Metal and Vulkan alike. A deadzone above every possible correction
 * discards all carry, so the result is the exact nearest entry of each
 * source pixel in either scan. Live carry reproduces the frozen indexes, and
 * the deferred solve and resolve against history agree with the monolithic
 * run and the frozen indexes. */

static int
direct_carry_frozen(unsigned char const *indexes, char const *expected)
{
    int i;

    for (i = 0; i < LSO2_GPU_PIXELS; ++i) {
        if (indexes[i] != (unsigned char)(expected[i] - '0')) {
            return 0;
        }
    }
    return 1;
}

static int
direct_carry_solve(int method, int scan, double deadzone, int history,
    int deferred, unsigned char *indexes)
{
    lso2_gpu_fixture_t f;
    int outcome;

    outcome = lso2_gpu_fixture_open_direct(&f, method);
    if (outcome != 0) goto end;
    outcome = 1;
    f.configuration.options.diffusion_deadzone = deadzone;
    f.configuration.method_for_scan = scan;
    f.prepare.scan = scan;
    if (history) {
        LSO2_GPU_CHECK(lso2_gpu_fixture_baseline(&f, 1) == SIXEL_OK);
    }
    LSO2_GPU_CHECK(lso2_gpu_fixture_prepare(&f) == SIXEL_OK);
    if (deferred) {
        LSO2_GPU_CHECK(lso2_gpu_fixture_deferred(&f) == SIXEL_OK);
    } else {
        LSO2_GPU_CHECK(lso2_gpu_fixture_run(&f) == SIXEL_OK);
    }
    memcpy(indexes, f.image->indexes, LSO2_GPU_PIXELS);
    outcome = 0;
end:
    lso2_gpu_fixture_close(&f);
    return outcome;
}

/* Nearest of the fixture's four gray lookup entries for its source
 * pattern, lowest index on ties; the fifth entry is the reserved key. */
static void
direct_carry_nearest(unsigned char *indexes)
{
    static int const gray[4] = { 0, 13, 26, 51 };
    int x, y, j, c, best, distance, minimum, gap, rgb[3];

    for (y = 0; y < LSO2_GPU_HEIGHT; ++y) {
        for (x = 0; x < LSO2_GPU_WIDTH; ++x) {
            rgb[0] = (2 * x + y) % 52;
            rgb[1] = (x + 2 * y) % 52;
            rgb[2] = (x + y) % 52;
            best = 0;
            minimum = -1;
            for (j = 0; j < 4; ++j) {
                distance = 0;
                for (c = 0; c < 3; ++c) {
                    gap = rgb[c] - gray[j];
                    distance += gap * gap;
                }
                if (minimum < 0 || distance < minimum) {
                    minimum = distance;
                    best = j;
                }
            }
            indexes[y * LSO2_GPU_WIDTH + x] = (unsigned char)best;
        }
    }
}

/* Each entry selects one result, with an independent nearest/frozen oracle.
 * History is setup for that result, not an additional reported scenario. */
int
test_direct_carry_case(int method_index, int observation)
{
    unsigned char actual[LSO2_GPU_PIXELS], reference[LSO2_GPU_PIXELS];
    int method, scan, outcome;

    method = lso2_gpu_expected_taps[method_index].method;
    scan = observation == 1 || observation >= 3
        ? SIXEL_SCAN_SERPENTINE : SIXEL_SCAN_RASTER;
    outcome = direct_carry_solve(method, scan,
        observation < 2 ? 1.0e6 : 0.0, observation >= 3,
        observation == 4, actual);
    if (outcome != 0) return outcome;
    if (observation < 2) {
        direct_carry_nearest(reference);
        return memcmp(actual, reference, LSO2_GPU_PIXELS) != 0;
    }
    return !direct_carry_frozen(actual, observation == 2
        ? lso2_gpu_expected_taps[method_index].raster
        : lso2_gpu_expected_taps[method_index].history);
}
