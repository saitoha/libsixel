/* SPDX-License-Identifier: MIT */
#include "tests/_support/lso2-gpu/fixture.h"
#include "src/dither-policy-atkinson-gpu.h"

/* Solve the shared fixture image with the atkinson GPU component. A deadzone
 * above every possible correction discards all carry, so the result is the
 * plain lookup of the source in either scan; live carry must change it. */
static int
atkinson_gpu_carry_solve(int scan, double deadzone, unsigned char *indexes)
{
    lso2_gpu_fixture_t f;
    SIXELSTATUS status;
    int outcome;

    outcome = lso2_gpu_fixture_open(&f, 1);
    if (outcome == 0) {
        f.dither->vtbl->unref(f.dither);
        f.dither = NULL;
        if (sixel_dither_policy_atkinson_gpu_new(f.allocator,
                (void **)&f.dither) != SIXEL_OK) outcome = 1;
    }
    if (outcome != 0) {
        lso2_gpu_fixture_close(&f);
        return outcome;
    }
    f.prepare.gpu_dither = f.dither;
    f.configuration.options.method_for_diffuse = SIXEL_DIFFUSE_ATKINSON;
    f.configuration.options.clamp_policy = SIXEL_DITHER_CLAMP_SOFT;
    f.configuration.options.clamp_threshold = 1.01;
    f.configuration.options.diffusion_deadzone = deadzone;
    f.configuration.method_for_scan = scan;
    f.prepare.scan = scan;
    status = lso2_gpu_fixture_prepare(&f);
    if (status == SIXEL_OK) status = lso2_gpu_fixture_run(&f);
    if (status == SIXEL_OK)
        memcpy(indexes, f.image->indexes, LSO2_GPU_PIXELS);
    lso2_gpu_fixture_close(&f);
    return status == SIXEL_OK ? 0 : 1;
}

SIXELSTATUS
test_atkinson_gpu_carry_configure(int method, int scan, int policy,
    double threshold, double deadzone, float perturb)
{
    sixel_gpu_dither_preparer_interface_t *dither;
    sixel_dither_policy_prepare_request_t request;
    sixel_allocator_t *allocator;
    SIXELSTATUS status;

    allocator = NULL;
    dither = NULL;
    if (sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL) != SIXEL_OK)
        return SIXEL_RUNTIME_ERROR;
    status = sixel_dither_policy_atkinson_gpu_new(allocator, (void **)&dither);
    if (status == SIXEL_OK) {
        memset(&request, 0, sizeof(request));
        request.options.method_for_diffuse = method;
        request.options.ncolors = 5;
        request.options.keycolor_reserved = 1;
        request.options.clamp_policy = policy;
        request.options.clamp_threshold = threshold;
        request.options.diffusion_deadzone = deadzone;
        request.options.diffusion_perturb = perturb;
        request.pixelformat = SIXEL_PIXELFORMAT_RGB888;
        request.method_for_scan = scan;
        status = dither->vtbl->configure(dither, &request);
        dither->vtbl->unref(dither);
    } else {
        /* Constructor failure must not satisfy a configure rejection. */
        status = SIXEL_RUNTIME_ERROR;
    }
    sixel_allocator_unref(allocator);
    return status;
}

/* Scan independence with carry suppressed and a live-carry effect are
 * separate observations. Each comparison starts from the same fixture. */
int
test_atkinson_gpu_carry_observation(int live)
{
    unsigned char baseline[LSO2_GPU_PIXELS], actual[LSO2_GPU_PIXELS];
    int outcome;

    outcome = atkinson_gpu_carry_solve(SIXEL_SCAN_RASTER, 1.0e6,
        baseline);
    if (outcome != 0) return outcome;
    outcome = atkinson_gpu_carry_solve(
        live ? SIXEL_SCAN_RASTER : SIXEL_SCAN_SERPENTINE,
        live ? 0.0 : 1.0e6, actual);
    if (outcome != 0) return outcome;
    return live ? memcmp(actual, baseline, LSO2_GPU_PIXELS) == 0
                : memcmp(actual, baseline, LSO2_GPU_PIXELS) != 0;
}
