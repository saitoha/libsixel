/* SPDX-License-Identifier: MIT */
#include "tests/_support/lso2-gpu/fixture.h"
#include "tests/_support/lso2-gpu/expected_taps.h"

/* Device-less GPU diffusion (Vulkan) for every fixed-tap method. A deadzone
 * above every possible correction discards all carry, so the result is
 * the exact nearest entry of each source pixel in either scan. Live carry
 * reproduces the frozen indexes, and the deferred solve and resolve
 * against history agree with the monolithic run and the frozen indexes. */

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

int
test_gpu_direct_0014_diffuse_carry(int argc, char **argv)
{
    unsigned char plain[LSO2_GPU_PIXELS], raster[LSO2_GPU_PIXELS];
    unsigned char serpentine[LSO2_GPU_PIXELS], live[LSO2_GPU_PIXELS];
    unsigned char run[LSO2_GPU_PIXELS], deferred[LSO2_GPU_PIXELS];
    size_t m;
    int method, outcome;

    (void)argc;
    (void)argv;
    direct_carry_nearest(plain);
    for (m = 0; m < sizeof(lso2_gpu_expected_taps) /
            sizeof(*lso2_gpu_expected_taps); ++m) {
        method = lso2_gpu_expected_taps[m].method;
        outcome = direct_carry_solve(method, SIXEL_SCAN_RASTER, 1.0e6, 0, 0,
            raster);
        if (outcome != 0) return outcome;
        outcome = direct_carry_solve(method, SIXEL_SCAN_SERPENTINE, 1.0e6, 0,
            0, serpentine);
        if (outcome != 0) return outcome;
        outcome = direct_carry_solve(method, SIXEL_SCAN_RASTER, 0.0, 0, 0,
            live);
        if (outcome != 0) return outcome;
        if (memcmp(raster, plain, LSO2_GPU_PIXELS) != 0 ||
                memcmp(serpentine, plain, LSO2_GPU_PIXELS) != 0 ||
                !direct_carry_frozen(live, lso2_gpu_expected_taps[m].raster)) {
            fprintf(stderr, "direct carry: method %d carry mismatch\n",
                method);
            return 1;
        }
        outcome = direct_carry_solve(method, SIXEL_SCAN_SERPENTINE, 0.0, 1,
            0, run);
        if (outcome != 0) return outcome;
        outcome = direct_carry_solve(method, SIXEL_SCAN_SERPENTINE, 0.0, 1,
            1, deferred);
        if (outcome != 0) return outcome;
        if (memcmp(run, deferred, LSO2_GPU_PIXELS) != 0 ||
                !direct_carry_frozen(run, lso2_gpu_expected_taps[m].history)) {
            fprintf(stderr, "direct carry: method %d history mismatch\n",
                method);
            return 1;
        }
    }
    return 0;
}
