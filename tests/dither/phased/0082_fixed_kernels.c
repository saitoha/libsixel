/* SPDX-License-Identifier: MIT */
#include "tests/_support/phased/fixture.h"

/* Solve the fixture with one phased kernel. A deadzone above every possible
 * correction discards all carry, so any kernel then reduces to the plain
 * lookup of the source; without it each kernel must leave its own mark. */
static SIXELSTATUS
fixed_kernels_solve(int kernel, int scan, double deadzone,
    unsigned char *indexes)
{
    phased_fixture_t f;
    SIXELSTATUS status;
    int outcome;

    outcome = phased_fixture_open(&f, 1);
    if (outcome != 0) {
        phased_fixture_close(&f);
        return outcome == 77 ? SIXEL_FEATURE_ERROR : SIXEL_RUNTIME_ERROR;
    }
    f.configuration.options.phased_kernel = kernel;
    f.configuration.options.clamp_policy = SIXEL_DITHER_CLAMP_SOFT;
    f.configuration.options.clamp_threshold = 1.01;
    f.configuration.options.diffusion_deadzone = deadzone;
    f.configuration.method_for_scan = scan;
    f.prepare.scan = scan;
    status = phased_fixture_prepare(&f);
    if (status == SIXEL_OK) status = phased_fixture_run(&f);
    if (status == SIXEL_OK)
        memcpy(indexes, f.image->indexes, PHASED_PIXELS);
    phased_fixture_close(&f);
    return status;
}

static SIXELSTATUS
fixed_kernels_configure(int kernel, int scan, int policy, double threshold,
    double deadzone, float perturb)
{
    phased_fixture_t f;
    SIXELSTATUS status;

    if (phased_fixture_open(&f, 0) != 0) {
        phased_fixture_close(&f);
        return SIXEL_RUNTIME_ERROR;
    }
    f.configuration.options.phased_kernel = kernel;
    f.configuration.options.clamp_policy = policy;
    f.configuration.options.clamp_threshold = threshold;
    f.configuration.options.diffusion_deadzone = deadzone;
    f.configuration.options.diffusion_perturb = perturb;
    f.configuration.method_for_scan = scan;
    status = f.dither->vtbl->configure(f.dither, &f.configuration);
    phased_fixture_close(&f);
    return status;
}

int
test_phased_fixed_kernels(int argc, char **argv)
{
    static int const kernels[] = {
        SIXEL_DIFFUSE_FS, SIXEL_DIFFUSE_ATKINSON, SIXEL_DIFFUSE_JAJUNI,
        SIXEL_DIFFUSE_STUCKI, SIXEL_DIFFUSE_BURKES, SIXEL_DIFFUSE_SIERRA1,
        SIXEL_DIFFUSE_SIERRA2, SIXEL_DIFFUSE_SIERRA3
    };
    unsigned char plain[PHASED_PIXELS], quiet[PHASED_PIXELS];
    unsigned char fs[PHASED_PIXELS], other[PHASED_PIXELS];
    SIXELSTATUS status;
    size_t i;

    (void)argc;
    (void)argv;
    status = fixed_kernels_solve(SIXEL_DIFFUSE_FS, SIXEL_SCAN_RASTER, 1.0e6,
        plain);
    if (status == SIXEL_FEATURE_ERROR) return 77;
    if (status != SIXEL_OK) goto fail;
    for (i = 0; i < sizeof(kernels)/sizeof(kernels[0]); ++i) {
        if (fixed_kernels_solve(kernels[i], SIXEL_SCAN_RASTER, 1.0e6, quiet)
                != SIXEL_OK || memcmp(quiet, plain, PHASED_PIXELS) != 0)
            goto fail;
    }
    if (fixed_kernels_solve(SIXEL_DIFFUSE_FS, SIXEL_SCAN_SERPENTINE, 1.0e6,
            quiet) != SIXEL_OK || memcmp(quiet, plain, PHASED_PIXELS) != 0)
        goto fail;
    /* Live carries select each kernel's taps and weights. */
    if (fixed_kernels_solve(SIXEL_DIFFUSE_FS, SIXEL_SCAN_RASTER, 0.0, fs)
            != SIXEL_OK || memcmp(fs, plain, PHASED_PIXELS) == 0)
        goto fail;
    if (fixed_kernels_solve(SIXEL_DIFFUSE_ATKINSON, SIXEL_SCAN_RASTER, 0.0,
            other) != SIXEL_OK || memcmp(other, fs, PHASED_PIXELS) == 0)
        goto fail;
    if (fixed_kernels_solve(SIXEL_DIFFUSE_LSO2, SIXEL_SCAN_SERPENTINE, 0.0,
            other) != SIXEL_OK || memcmp(other, fs, PHASED_PIXELS) == 0)
        goto fail;
    /* Raster order belongs to fixed kernels; LSO2 stays serpentine. */
    if (fixed_kernels_configure(SIXEL_DIFFUSE_LSO2, SIXEL_SCAN_RASTER,
            SIXEL_DITHER_CLAMP_SOFT, 1.01, 5.0, 0.0f) != SIXEL_FEATURE_ERROR)
        goto fail;
    if (fixed_kernels_configure(SIXEL_DIFFUSE_FS, SIXEL_SCAN_RASTER,
            SIXEL_DITHER_CLAMP_NONE, 1.01, 5.0, 0.0f) != SIXEL_FEATURE_ERROR)
        goto fail;
    if (fixed_kernels_configure(SIXEL_DIFFUSE_FS, SIXEL_SCAN_RASTER,
            SIXEL_DITHER_CLAMP_SOFT, 1.01, 5.0, 0.5f) != SIXEL_FEATURE_ERROR)
        goto fail;
    if (fixed_kernels_configure(SIXEL_DIFFUSE_FS, SIXEL_SCAN_RASTER,
            SIXEL_DITHER_CLAMP_SOFT, 0.5, 5.0, 0.0f) != SIXEL_BAD_ARGUMENT)
        goto fail;
    if (fixed_kernels_configure(SIXEL_DIFFUSE_FS, SIXEL_SCAN_RASTER,
            SIXEL_DITHER_CLAMP_SOFT, 1.01, -1.0, 0.0f) != SIXEL_BAD_ARGUMENT)
        goto fail;
    if (fixed_kernels_configure(SIXEL_DIFFUSE_NONE, SIXEL_SCAN_SERPENTINE,
            SIXEL_DITHER_CLAMP_SOFT, 1.01, 5.0, 0.0f) != SIXEL_BAD_ARGUMENT)
        goto fail;
    /* A cleared kernel field keeps meaning LSO2. */
    if (fixed_kernels_configure(0, SIXEL_SCAN_SERPENTINE,
            SIXEL_DITHER_CLAMP_AUTO, 0.0, 0.0, 0.0f) != SIXEL_OK)
        goto fail;
    return 0;
fail:
    fprintf(stderr, "phased fixed kernels: %s\n",
        sixel_helper_get_additional_message());
    return 1;
}
