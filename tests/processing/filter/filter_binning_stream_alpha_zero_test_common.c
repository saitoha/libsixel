/*
 * SPDX-License-Identifier: MIT
 *
 * Shared direct sample-stream alpha-zero binning test implementation.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "src/filter-binning.h"
#include "src/filter-factory.h"
#include "src/filter.h"
#include "src/palette-plan.h"
#include "src/sample-stream.h"
#include "src/weighted-point-set.h"
#include "filter_binning_stream_alpha_zero_test_common.h"

int
filter_binning_stream_alpha_zero_run(int alpha_zero_is_transparent,
                                     size_t expected_point_count,
                                     double expected_total_weight,
                                     int expected_red_points)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_frame_t *frame;
    sixel_frame_interface_t *frame_if;
    sixel_frame_transparency_t transparency;
    sixel_sample_stream_t samples;
    sixel_palette_binning_state_t binning;
    sixel_weighted_point_set_t output;
    sixel_filter_binning_config_t config;
    sixel_filter_t *filter;
    unsigned char pixels[12];
    size_t index;
    int red_points;

    status = SIXEL_FALSE;
    allocator = NULL;
    frame = NULL;
    frame_if = NULL;
    memset(&transparency, 0, sizeof(transparency));
    sixel_sample_stream_init(&samples);
    sixel_palette_binning_state_init(
        &binning,
        SIXEL_PALETTE_BINNING_HARD,
        SIXEL_PALETTE_POLICY_ORIGIN_EXPLICIT);
    sixel_weighted_point_set_init(&output);
    memset(&config, 0, sizeof(config));
    filter = NULL;
    index = 0u;
    red_points = 0;
    pixels[0] = 240u;
    pixels[1] = 0u;
    pixels[2] = 0u;
    pixels[3] = 0u;
    pixels[4] = 0u;
    pixels[5] = 240u;
    pixels[6] = 0u;
    pixels[7] = 255u;
    pixels[8] = 0u;
    pixels[9] = 0u;
    pixels[10] = 240u;
    pixels[11] = 255u;

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_frame_new(&frame, allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_frame_init_borrowed(frame,
                                       pixels,
                                       3,
                                       1,
                                       SIXEL_PIXELFORMAT_RGBA8888,
                                       NULL,
                                       (-1));
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    frame_if = sixel_frame_as_interface(frame);
    if (frame_if == NULL || frame_if->vtbl == NULL ||
        frame_if->vtbl->set_transparency == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }
    transparency.transparent = (-1);
    transparency.alpha_zero_is_transparent = alpha_zero_is_transparent;
    status = frame_if->vtbl->set_transparency(frame_if, &transparency);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_sample_stream_bind_borrowed(
        &samples,
        frame,
        SIXEL_PALETTE_SAMPLING_FULL_FRAME,
        SIXEL_PALETTE_SAMPLING_SOURCE_PREPROCESSED_FRAME);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_palette_binning_resolve(
        &binning,
        SIXEL_PALETTE_BINNING_HARD,
        4u,
        SIXEL_PALETTE_BINNING_GRID_UNIFORM,
        SIXEL_PALETTE_BINNING_KERNEL_NONE,
        SIXEL_PALETTE_BINNING_BACKEND_COMPACT_SPARSE,
        3u,
        SIXEL_PALETTE_RESOLUTION_EXPLICIT);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    config.binning = &binning;
    config.output_order =
        SIXEL_FILTER_BINNING_OUTPUT_BIN_KEY_ASCENDING;
    status = sixel_filter_factory_create_by_kind(
        SIXEL_FILTER_KIND_BINNING,
        &config,
        &filter);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    sixel_filter_bind_sample_input(filter, &samples);
    sixel_filter_bind_weighted_output(filter,
                                      &output,
                                      SIXEL_COLORSPACE_GAMMA);
    status = sixel_filter_run(filter, allocator, NULL);
    if (SIXEL_FAILED(status) ||
        output.point_count != expected_point_count ||
        output.total_weight != expected_total_weight) {
        status = SIXEL_LOGIC_ERROR;
        goto cleanup;
    }
    for (index = 0u; index < output.point_count; ++index) {
        if (output.coordinates[index * 3u] != 0.0) {
            red_points += 1;
        }
    }
    if (red_points != expected_red_points) {
        status = SIXEL_LOGIC_ERROR;
        goto cleanup;
    }
    status = SIXEL_OK;

cleanup:
    sixel_filter_free(filter);
    sixel_weighted_point_set_dispose(&output);
    sixel_sample_stream_dispose(&samples);
    sixel_frame_unref(frame);
    sixel_allocator_unref(allocator);
    return SIXEL_SUCCEEDED(status);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
