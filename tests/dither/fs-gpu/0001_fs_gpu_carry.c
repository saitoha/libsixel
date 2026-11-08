/* SPDX-License-Identifier: MIT */
#include "tests/_support/lso2-gpu/fixture.h"
#include "src/dither-policy-fs-gpu.h"

/* Solve the shared fixture image with the fs GPU component. A deadzone
 * above every possible correction discards all carry, so the result is the
 * plain lookup of the source in either scan; live carry must change it. */
static SIXELSTATUS
fs_gpu_carry_solve(int scan, double deadzone, unsigned char *indexes)
{
    lso2_gpu_fixture_t f;
    SIXELSTATUS status;
    int outcome;

    outcome = lso2_gpu_fixture_open(&f, 1);
    if (outcome == 0) {
        f.dither->vtbl->unref(f.dither);
        f.dither = NULL;
        if (sixel_dither_policy_fs_gpu_new(f.allocator,
                (void **)&f.dither) != SIXEL_OK) outcome = 1;
    }
    if (outcome != 0) {
        lso2_gpu_fixture_close(&f);
        return outcome == 77 ? SIXEL_FEATURE_ERROR : SIXEL_RUNTIME_ERROR;
    }
    f.prepare.gpu_dither = f.dither;
    f.configuration.options.method_for_diffuse = SIXEL_DIFFUSE_FS;
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
    return status;
}

static SIXELSTATUS
fs_gpu_carry_configure(int method, int scan, int policy, double threshold,
    double deadzone, float perturb)
{
    sixel_gpu_dither_preparer_interface_t *dither;
    sixel_dither_policy_prepare_request_t request;
    sixel_allocator_t *allocator;
    SIXELSTATUS status;

    allocator = NULL;
    dither = NULL;
    if (sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL) != SIXEL_OK)
        return SIXEL_RUNTIME_ERROR;
    status = sixel_dither_policy_fs_gpu_new(allocator, (void **)&dither);
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
    }
    sixel_allocator_unref(allocator);
    return status;
}

int
test_fs_gpu_carry(int argc, char **argv)
{
    unsigned char raster[LSO2_GPU_PIXELS], serpentine[LSO2_GPU_PIXELS];
    unsigned char live[LSO2_GPU_PIXELS];
    SIXELSTATUS status;

    (void)argc;
    (void)argv;
    /* The component accepts only its own method and the CPU carry limits
     * its GPU form reproduces. */
    if (fs_gpu_carry_configure(SIXEL_DIFFUSE_FS, SIXEL_SCAN_RASTER,
            SIXEL_DITHER_CLAMP_SOFT, 1.01, 5.0, 0.0f) != SIXEL_OK)
        goto fail;
    if (fs_gpu_carry_configure(SIXEL_DIFFUSE_FS, SIXEL_SCAN_SERPENTINE,
            SIXEL_DITHER_CLAMP_HARD, 1.0, 0.0, 0.0f) != SIXEL_OK)
        goto fail;
    if (fs_gpu_carry_configure(SIXEL_DIFFUSE_ATKINSON, SIXEL_SCAN_RASTER,
            SIXEL_DITHER_CLAMP_SOFT, 1.01, 5.0, 0.0f) != SIXEL_BAD_ARGUMENT)
        goto fail;
    if (fs_gpu_carry_configure(SIXEL_DIFFUSE_LSO2, SIXEL_SCAN_SERPENTINE,
            SIXEL_DITHER_CLAMP_SOFT, 1.01, 5.0, 0.0f) != SIXEL_BAD_ARGUMENT)
        goto fail;
    if (fs_gpu_carry_configure(SIXEL_DIFFUSE_FS, SIXEL_SCAN_RASTER,
            SIXEL_DITHER_CLAMP_NONE, 1.01, 5.0, 0.0f) != SIXEL_FEATURE_ERROR)
        goto fail;
    if (fs_gpu_carry_configure(SIXEL_DIFFUSE_FS, SIXEL_SCAN_RASTER,
            SIXEL_DITHER_CLAMP_SOFT, 1.01, 5.0, 0.5f) != SIXEL_FEATURE_ERROR)
        goto fail;
    if (fs_gpu_carry_configure(SIXEL_DIFFUSE_FS, SIXEL_SCAN_RASTER,
            SIXEL_DITHER_CLAMP_SOFT, 0.5, 5.0, 0.0f) != SIXEL_BAD_ARGUMENT)
        goto fail;
    if (fs_gpu_carry_configure(SIXEL_DIFFUSE_FS, SIXEL_SCAN_RASTER,
            SIXEL_DITHER_CLAMP_SOFT, 1.01, -1.0, 0.0f) != SIXEL_BAD_ARGUMENT)
        goto fail;
    status = fs_gpu_carry_solve(SIXEL_SCAN_RASTER, 1.0e6, raster);
    if (status == SIXEL_FEATURE_ERROR) return 77;
    if (status != SIXEL_OK) goto fail;
    if (fs_gpu_carry_solve(SIXEL_SCAN_SERPENTINE, 1.0e6, serpentine)
            != SIXEL_OK || memcmp(raster, serpentine, LSO2_GPU_PIXELS) != 0)
        goto fail;
    if (fs_gpu_carry_solve(SIXEL_SCAN_RASTER, 0.0, live) != SIXEL_OK ||
            memcmp(live, raster, LSO2_GPU_PIXELS) == 0)
        goto fail;
    return 0;
fail:
    fprintf(stderr, "fs gpu carry: %s\n",
        sixel_helper_get_additional_message());
    return 1;
}
