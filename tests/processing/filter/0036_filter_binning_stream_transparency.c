/*
 * SPDX-License-Identifier: MIT
 *
 * Verify that direct sample-stream binning honors both frame transparency
 * sources: an explicit mask and the opt-in alpha-zero interpretation.
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

int
test_filter_0036_filter_binning_stream_transparency(int argc, char **argv)
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
    unsigned char *mask;
    unsigned char pixels[12];

    (void)argc;
    (void)argv;
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
    mask = NULL;
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
    mask = (unsigned char *)sixel_allocator_malloc(allocator, 3u);
    if (mask == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto cleanup;
    }
    mask[0] = 0u;
    mask[1] = 1u;
    mask[2] = 0u;
    transparency.transparent = (-1);
    transparency.alpha_zero_is_transparent = 0;
    transparency.transparent_mask = mask;
    transparency.transparent_mask_size = 3u;
    status = frame_if->vtbl->set_transparency(frame_if, &transparency);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    mask = NULL;
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
    if (SIXEL_FAILED(status) || output.point_count != 2u ||
            output.total_weight != 2.0 ||
            output.coordinates[2] != 240.0 ||
            output.coordinates[3] != 240.0) {
        status = SIXEL_LOGIC_ERROR;
        goto cleanup;
    }
    sixel_filter_free(filter);
    filter = NULL;
    sixel_weighted_point_set_dispose(&output);

    transparency.alpha_zero_is_transparent = 1;
    status = frame_if->vtbl->set_transparency(frame_if, &transparency);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    sixel_palette_binning_state_init(
        &binning,
        SIXEL_PALETTE_BINNING_HARD,
        SIXEL_PALETTE_POLICY_ORIGIN_EXPLICIT);
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
    if (SIXEL_FAILED(status) || output.point_count != 1u ||
            output.total_weight != 1.0 ||
            output.coordinates[0] != 0.0 ||
            output.coordinates[1] != 0.0 ||
            output.coordinates[2] != 240.0) {
        status = SIXEL_LOGIC_ERROR;
        goto cleanup;
    }
    status = SIXEL_OK;

cleanup:
    sixel_allocator_free(allocator, mask);
    sixel_filter_free(filter);
    sixel_weighted_point_set_dispose(&output);
    sixel_sample_stream_dispose(&samples);
    sixel_frame_unref(frame);
    sixel_allocator_unref(allocator);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "sample-stream transparency contract failed\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
