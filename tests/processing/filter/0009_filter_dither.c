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
static int
test_accumulation_buffer_respects_delta_threshold(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_dither_t *baseline_dither;
    sixel_dither_t *below_delta_dither;
    sixel_dither_t *within_delta_dither;
    sixel_index_t *baseline_indexes;
    sixel_index_t *below_delta_indexes;
    sixel_index_t *within_delta_indexes;
    unsigned char palette[6];
    unsigned char pixel[3];
    unsigned char accumulation[3];

    status = SIXEL_FALSE;
    allocator = NULL;
    baseline_dither = NULL;
    below_delta_dither = NULL;
    within_delta_dither = NULL;
    baseline_indexes = NULL;
    below_delta_indexes = NULL;
    within_delta_indexes = NULL;
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
    status = make_dither(allocator, 2, &below_delta_dither);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = make_dither(allocator, 2, &within_delta_dither);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    sixel_dither_set_palette(baseline_dither, palette);
    sixel_dither_set_pixelformat(baseline_dither, SIXEL_PIXELFORMAT_RGB888);
    sixel_dither_set_diffusion_type(baseline_dither, SIXEL_DIFFUSE_NONE);
    sixel_dither_set_optimize_palette(baseline_dither, 0);
    sixel_dither_set_transparent(baseline_dither, 0);

    sixel_dither_set_palette(below_delta_dither, palette);
    sixel_dither_set_pixelformat(below_delta_dither,
                                 SIXEL_PIXELFORMAT_RGB888);
    sixel_dither_set_diffusion_type(below_delta_dither, SIXEL_DIFFUSE_NONE);
    sixel_dither_set_optimize_palette(below_delta_dither, 0);
    sixel_dither_set_transparent(below_delta_dither, 0);
    sixel_dither_set_pipeline_accumulation_buffer_hint(
        below_delta_dither,
        accumulation,
        sizeof(accumulation),
        NULL,
        0U,
        1,
        1,
        0,
        0,
        1,
        1,
        0,
        1,
        4u,
        SIXEL_6DELTA_ERROR_DIFFUSE);

    sixel_dither_set_palette(within_delta_dither, palette);
    sixel_dither_set_pixelformat(within_delta_dither,
                                 SIXEL_PIXELFORMAT_RGB888);
    sixel_dither_set_diffusion_type(within_delta_dither,
                                    SIXEL_DIFFUSE_NONE);
    sixel_dither_set_optimize_palette(within_delta_dither, 0);
    sixel_dither_set_transparent(within_delta_dither, 0);
    sixel_dither_set_pipeline_accumulation_buffer_hint(
        within_delta_dither,
        accumulation,
        sizeof(accumulation),
        NULL,
        0U,
        1,
        1,
        0,
        0,
        1,
        1,
        0,
        1,
        5u,
        SIXEL_6DELTA_ERROR_DIFFUSE);

    baseline_indexes = sixel_dither_apply_palette(baseline_dither,
                                                  pixel,
                                                  1,
                                                  1);
    if (baseline_indexes == NULL || baseline_indexes[0] == 0) {
        goto cleanup;
    }
    below_delta_indexes = sixel_dither_apply_palette(below_delta_dither,
                                                     pixel,
                                                     1,
                                                     1);
    if (below_delta_indexes == NULL || below_delta_indexes[0] == 0) {
        goto cleanup;
    }
    within_delta_indexes = sixel_dither_apply_palette(within_delta_dither,
                                                      pixel,
                                                      1,
                                                      1);
    if (within_delta_indexes == NULL || within_delta_indexes[0] != 0) {
        goto cleanup;
    }
    status = SIXEL_OK;

cleanup:
    if (baseline_indexes != NULL && allocator != NULL) {
        sixel_allocator_free(allocator, baseline_indexes);
    }
    if (below_delta_indexes != NULL && allocator != NULL) {
        sixel_allocator_free(allocator, below_delta_indexes);
    }
    if (within_delta_indexes != NULL && allocator != NULL) {
        sixel_allocator_free(allocator, within_delta_indexes);
    }
    if (baseline_dither != NULL) {
        sixel_dither_unref(baseline_dither);
    }
    if (below_delta_dither != NULL) {
        sixel_dither_unref(below_delta_dither);
    }
    if (within_delta_dither != NULL) {
        sixel_dither_unref(within_delta_dither);
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
    if (!test_accumulation_buffer_respects_delta_threshold()) {
        fprintf(stderr, "6delta threshold dither path failed\n");
        success = 0;
    }
    if (!test_accumulation_result_rgb_tracks_palette_indexes()) {
        fprintf(stderr, "accumulation result rgb tracking failed\n");
        success = 0;
    }
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
