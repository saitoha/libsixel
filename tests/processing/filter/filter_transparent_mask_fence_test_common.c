/*
 * SPDX-License-Identifier: MIT
 *
 * Shared transparent-mask dither-fence test implementation. The serial and
 * parallel test entries select the execution path independently.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>

#include <sixel.h>

#include "src/filter-dither.h"
#include "tests/processing/filter/filter_test_common.h"
#include "tests/processing/filter/filter_transparent_mask_fence_test_common.h"

#if !defined(_WIN32)
static SIXELSTATUS
configure_mask_fence_dither(sixel_dither_t *dither,
                            int width,
                            int height,
                            int parallel_active)
{
    unsigned char palette[6];

    palette[0] = 0u;
    palette[1] = 0u;
    palette[2] = 0u;
    palette[3] = 255u;
    palette[4] = 255u;
    palette[5] = 255u;
    if (dither == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    sixel_dither_set_palette(dither, palette);
    sixel_dither_set_pixelformat(dither, SIXEL_PIXELFORMAT_RGB888);
    sixel_dither_set_diffusion_type(dither, SIXEL_DIFFUSE_FS);
    sixel_dither_set_diffusion_scan(dither, SIXEL_SCAN_RASTER);
    sixel_dither_set_optimize_palette(dither, 0);
    sixel_dither_set_transparent(dither, 0);
    if (parallel_active) {
        dither->pipeline_parallel_active = 1;
        dither->pipeline_band_height = 6;
        dither->pipeline_band_overlap = 0;
        dither->pipeline_dither_threads = 2;
        dither->pipeline_image_width = width;
        dither->pipeline_image_height = height;
    }
    return SIXEL_OK;
}

int
filter_dither_transparent_mask_fence_run(int parallel_active)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_dither_t *baseline_dither;
    sixel_dither_t *masked_dither;
    sixel_index_t *baseline_indexes;
    sixel_index_t *masked_indexes;
    enum {
        baseline_width = 1,
        masked_width = 2,
        image_height = 12
    };
    unsigned char baseline_pixels[baseline_width * image_height * 3];
    unsigned char masked_pixels[masked_width * image_height * 3];
    unsigned char transparent_mask[masked_width * image_height];
    int y;
    int x;
    size_t baseline_offset;
    size_t masked_offset;
    size_t mask_offset;

    status = SIXEL_FALSE;
    allocator = NULL;
    baseline_dither = NULL;
    masked_dither = NULL;
    baseline_indexes = NULL;
    masked_indexes = NULL;
    y = 0;
    x = 0;
    baseline_offset = 0u;
    masked_offset = 0u;
    mask_offset = 0u;
    status = make_allocator(&allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = make_dither(allocator, 2, &baseline_dither);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = configure_mask_fence_dither(baseline_dither,
                                         baseline_width,
                                         image_height,
                                         parallel_active);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = make_dither(allocator, 2, &masked_dither);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = configure_mask_fence_dither(masked_dither,
                                         masked_width,
                                         image_height,
                                         parallel_active);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    for (y = 0; y < image_height; ++y) {
        baseline_offset = (size_t)y * (size_t)baseline_width * 3u;
        baseline_pixels[baseline_offset + 0u] = 100u;
        baseline_pixels[baseline_offset + 1u] = 100u;
        baseline_pixels[baseline_offset + 2u] = 100u;
        for (x = 0; x < masked_width; ++x) {
            masked_offset = ((size_t)y * (size_t)masked_width
                             + (size_t)x) * 3u;
            masked_pixels[masked_offset + 0u] = x == 0 ? 127u : 100u;
            masked_pixels[masked_offset + 1u] = x == 0 ? 127u : 100u;
            masked_pixels[masked_offset + 2u] = x == 0 ? 127u : 100u;
            mask_offset = (size_t)y * (size_t)masked_width + (size_t)x;
            transparent_mask[mask_offset] = x == 0 ? 1u : 0u;
        }
    }
    baseline_indexes = sixel_dither_apply_palette(baseline_dither,
                                                  baseline_pixels,
                                                  baseline_width,
                                                  image_height);
    if (baseline_indexes == NULL) {
        goto cleanup;
    }
    sixel_dither_set_pipeline_transparent_mask_hint(
        masked_dither,
        transparent_mask,
        sizeof(transparent_mask),
        0);
    masked_indexes = sixel_dither_apply_palette(masked_dither,
                                                masked_pixels,
                                                masked_width,
                                                image_height);
    if (masked_indexes == NULL) {
        goto cleanup;
    }
    for (y = 0; y < image_height; ++y) {
        masked_offset = (size_t)y * (size_t)masked_width;
        if (masked_indexes[masked_offset] != 0 ||
            masked_indexes[masked_offset + 1u] != baseline_indexes[y]) {
            goto cleanup;
        }
    }
    status = SIXEL_OK;

cleanup:
    if (baseline_indexes != NULL && allocator != NULL) {
        sixel_allocator_free(allocator, baseline_indexes);
    }
    if (masked_indexes != NULL && allocator != NULL) {
        sixel_allocator_free(allocator, masked_indexes);
    }
    if (baseline_dither != NULL) {
        sixel_dither_unref(baseline_dither);
    }
    if (masked_dither != NULL) {
        sixel_dither_unref(masked_dither);
    }
    if (allocator != NULL) {
        sixel_allocator_unref(allocator);
    }
    return SIXEL_SUCCEEDED(status);
}
#else
int
filter_dither_transparent_mask_fence_run(int parallel_active)
{
    (void)parallel_active;
    return 0;
}
#endif

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
