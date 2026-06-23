/*
 * SPDX-License-Identifier: MIT
 *
 * Dither filter tests. These checks verify that the filter configures the
 * dither object for the incoming frame and reports progress through the
 * shared callback interface.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include <sixel.h>

#include "src/filter-dither.h"
#include "src/filter-factory.h"
#include "src/filter.h"
#include "tests/processing/filter/filter_test_common.h"

#if !defined(_WIN32)
static SIXELSTATUS
configure_test_dither(sixel_dither_t *dither,
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

static int
test_transparent_mask_fence_core(int parallel_active)
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
    status = configure_test_dither(baseline_dither,
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
    status = configure_test_dither(masked_dither,
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
            if (x == 0) {
                masked_pixels[masked_offset + 0u] = 127u;
                masked_pixels[masked_offset + 1u] = 127u;
                masked_pixels[masked_offset + 2u] = 127u;
            } else {
                masked_pixels[masked_offset + 0u] = 100u;
                masked_pixels[masked_offset + 1u] = 100u;
                masked_pixels[masked_offset + 2u] = 100u;
            }
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
        if (masked_indexes[masked_offset] != 0) {
            goto cleanup;
        }
        if (masked_indexes[masked_offset + 1u] != baseline_indexes[y]) {
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

static int
test_transparent_mask_fence_serial(void)
{
    return test_transparent_mask_fence_core(0);
}

static int
test_accumulation_buffer_beats_palette_candidate(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_dither_t *baseline_dither;
    sixel_dither_t *accumulation_dither;
    sixel_index_t *baseline_indexes;
    sixel_index_t *accumulation_indexes;
    unsigned char palette[6];
    unsigned char pixel[3];
    unsigned char accumulation[3];

    status = SIXEL_FALSE;
    allocator = NULL;
    baseline_dither = NULL;
    accumulation_dither = NULL;
    baseline_indexes = NULL;
    accumulation_indexes = NULL;
    palette[0] = 0u;
    palette[1] = 0u;
    palette[2] = 0u;
    palette[3] = 160u;
    palette[4] = 160u;
    palette[5] = 160u;
    pixel[0] = 100u;
    pixel[1] = 100u;
    pixel[2] = 100u;
    accumulation[0] = 105u;
    accumulation[1] = 105u;
    accumulation[2] = 105u;

    status = make_allocator(&allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = make_dither(allocator, 2, &baseline_dither);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = make_dither(allocator, 2, &accumulation_dither);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    sixel_dither_set_palette(baseline_dither, palette);
    sixel_dither_set_pixelformat(baseline_dither, SIXEL_PIXELFORMAT_RGB888);
    sixel_dither_set_diffusion_type(baseline_dither, SIXEL_DIFFUSE_NONE);
    sixel_dither_set_optimize_palette(baseline_dither, 0);
    sixel_dither_set_transparent(baseline_dither, 0);

    sixel_dither_set_palette(accumulation_dither, palette);
    sixel_dither_set_pixelformat(accumulation_dither,
                                 SIXEL_PIXELFORMAT_RGB888);
    sixel_dither_set_diffusion_type(accumulation_dither, SIXEL_DIFFUSE_NONE);
    sixel_dither_set_optimize_palette(accumulation_dither, 0);
    sixel_dither_set_transparent(accumulation_dither, 0);
    sixel_dither_set_pipeline_accumulation_buffer_hint(
        accumulation_dither,
        accumulation,
        sizeof(accumulation),
        1,
        1,
        0);

    baseline_indexes = sixel_dither_apply_palette(baseline_dither,
                                                  pixel,
                                                  1,
                                                  1);
    if (baseline_indexes == NULL || baseline_indexes[0] == 0) {
        goto cleanup;
    }
    accumulation_indexes = sixel_dither_apply_palette(accumulation_dither,
                                                      pixel,
                                                      1,
                                                      1);
    if (accumulation_indexes == NULL || accumulation_indexes[0] != 0) {
        goto cleanup;
    }
    status = SIXEL_OK;

cleanup:
    if (baseline_indexes != NULL && allocator != NULL) {
        sixel_allocator_free(allocator, baseline_indexes);
    }
    if (accumulation_indexes != NULL && allocator != NULL) {
        sixel_allocator_free(allocator, accumulation_indexes);
    }
    if (baseline_dither != NULL) {
        sixel_dither_unref(baseline_dither);
    }
    if (accumulation_dither != NULL) {
        sixel_dither_unref(accumulation_dither);
    }
    if (allocator != NULL) {
        sixel_allocator_unref(allocator);
    }

    return SIXEL_SUCCEEDED(status);
}

static int
test_accumulation_result_rgb_tracks_palette_indexes(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_dither_t *dither;
    sixel_index_t indexes[3];
    unsigned char palette[9];
    unsigned char const *rgb;
    size_t rgb_size;

    status = SIXEL_FALSE;
    allocator = NULL;
    dither = NULL;
    rgb = NULL;
    rgb_size = 0U;
    indexes[0] = 0;
    indexes[1] = 2;
    indexes[2] = 1;
    palette[0] = 1u;
    palette[1] = 2u;
    palette[2] = 3u;
    palette[3] = 4u;
    palette[4] = 5u;
    palette[5] = 6u;
    palette[6] = 7u;
    palette[7] = 8u;
    palette[8] = 9u;

    status = make_allocator(&allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = make_dither(allocator, 3, &dither);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_dither_set_pipeline_accumulation_result_rgb(
        dither,
        indexes,
        3U,
        palette,
        3U);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    rgb = sixel_dither_get_pipeline_accumulation_result_rgb(dither,
                                                            &rgb_size);
    if (rgb == NULL || rgb_size != 9U) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }
    if (rgb[0] != 1u || rgb[1] != 2u || rgb[2] != 3u ||
        rgb[3] != 7u || rgb[4] != 8u || rgb[5] != 9u ||
        rgb[6] != 4u || rgb[7] != 5u || rgb[8] != 6u) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }
    sixel_dither_clear_pipeline_accumulation_result_rgb(dither);
    rgb = sixel_dither_get_pipeline_accumulation_result_rgb(dither,
                                                            &rgb_size);
    if (rgb != NULL || rgb_size != 0U) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }
    status = SIXEL_OK;

cleanup:
    if (dither != NULL) {
        sixel_dither_unref(dither);
    }
    if (allocator != NULL) {
        sixel_allocator_unref(allocator);
    }

    return SIXEL_SUCCEEDED(status);
}

#if SIXEL_ENABLE_THREADS
static int
test_transparent_mask_fence_parallel(void)
{
    return test_transparent_mask_fence_core(1);
}
#endif  /* SIXEL_ENABLE_THREADS */
#endif  /* !defined(_WIN32) */

static int
test_dither_updates_pixelformat_and_progress(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_filter_t *filter;
    sixel_filter_dither_config_t config;
    sixel_frame_t *frame;
    sixel_dither_t *dither;
    test_progress_t progress;

    status = SIXEL_FALSE;
    allocator = NULL;
    filter = NULL;
    frame = NULL;
    dither = NULL;
    progress.began = 0;
    progress.progressed = 0;
    progress.completed = 0;
    progress.aborted = 0;

    status = make_allocator(&allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = make_rgb_frame(allocator, 3, 2, &frame);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = make_dither(allocator, 16, &dither);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    config.dither = dither;

    status = sixel_filter_factory_create_by_kind(SIXEL_FILTER_KIND_DITHER,
                                                 &config,
                                                 &filter);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    sixel_filter_bind_input(filter,
                            &frame,
                            frame->pixelformat,
                            frame->colorspace);
    sixel_filter_set_progress(filter, progress_cb, &progress, 1);

    status = sixel_filter_run(filter, allocator, NULL);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    if (dither->pixelformat != frame->pixelformat) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

    if (progress.began != 1 || progress.completed != 1 || progress.aborted) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

cleanup:
    sixel_filter_teardown(filter);
    sixel_filter_free(filter);
    if (dither != NULL) {
        sixel_dither_unref(dither);
    }
    sixel_frame_unref(frame);
    sixel_allocator_unref(allocator);

    return SIXEL_SUCCEEDED(status);
}

int
test_filter_0009_filter_dither(int argc, char **argv)
{
    int success;

    (void) argc;
    (void) argv;

    success = 1;

    if (!test_dither_updates_pixelformat_and_progress()) {
        fprintf(stderr, "dither filter sets format and progress failed\n");
        success = 0;
    }
#if !defined(_WIN32)
    if (!test_transparent_mask_fence_serial()) {
        fprintf(stderr, "transparent mask fence serial path failed\n");
        success = 0;
    }
    if (!test_accumulation_buffer_beats_palette_candidate()) {
        fprintf(stderr, "accumulation candidate dither path failed\n");
        success = 0;
    }
    if (!test_accumulation_result_rgb_tracks_palette_indexes()) {
        fprintf(stderr, "accumulation result rgb tracking failed\n");
        success = 0;
    }
#if SIXEL_ENABLE_THREADS
    if (!test_transparent_mask_fence_parallel()) {
        fprintf(stderr, "transparent mask fence parallel path failed\n");
        success = 0;
    }
#endif  /* SIXEL_ENABLE_THREADS */
#endif  /* !defined(_WIN32) */

    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
