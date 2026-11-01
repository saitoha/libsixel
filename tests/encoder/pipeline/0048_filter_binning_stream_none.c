/*
 * SPDX-License-Identifier: MIT
 *
 * Verify that the none policy preserves visible stream samples in scan order.
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
test_filter_0039_filter_binning_stream_none(int argc, char **argv)
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
        SIXEL_PALETTE_BINNING_NONE,
        SIXEL_PALETTE_POLICY_ORIGIN_EXPLICIT);
    sixel_weighted_point_set_init(&output);
    memset(&config, 0, sizeof(config));
    filter = NULL;
    pixels[0] = 10u;
    pixels[1] = 20u;
    pixels[2] = 30u;
    pixels[3] = 0u;
    pixels[4] = 40u;
    pixels[5] = 50u;
    pixels[6] = 60u;
    pixels[7] = 255u;
    pixels[8] = 70u;
    pixels[9] = 80u;
    pixels[10] = 90u;
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
    transparency.alpha_zero_is_transparent = 1;
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
        SIXEL_PALETTE_BINNING_NONE,
        0u,
        SIXEL_PALETTE_BINNING_GRID_NONE,
        SIXEL_PALETTE_BINNING_KERNEL_NONE,
        SIXEL_PALETTE_BINNING_BACKEND_DIRECT,
        3u,
        SIXEL_PALETTE_RESOLUTION_EXPLICIT);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    config.binning = &binning;
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
            binning.policy.phase != SIXEL_PALETTE_POLICY_EXECUTED ||
            output.policy != SIXEL_PALETTE_BINNING_NONE ||
            output.source_point_count != 3u ||
            output.point_count != 2u || output.weights != NULL ||
            output.total_weight != 2.0 ||
            output.coordinates[0] != 40.0 ||
            output.coordinates[1] != 50.0 ||
            output.coordinates[2] != 60.0 ||
            output.coordinates[3] != 70.0 ||
            output.coordinates[4] != 80.0 ||
            output.coordinates[5] != 90.0) {
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
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "none sample-stream binning contract failed\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
