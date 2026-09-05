/*
 * SPDX-License-Identifier: MIT
 *
 * Verify that binning consumes the typed sample-stream edge directly and can
 * publish bins in packed-key order for deterministic quantizer tie-breaking.
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
test_filter_0034_filter_binning_sample_stream(int argc, char **argv)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_frame_t *frame;
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
    sixel_sample_stream_init(&samples);
    sixel_palette_binning_state_init(
        &binning,
        SIXEL_PALETTE_BINNING_HARD,
        SIXEL_PALETTE_POLICY_ORIGIN_EXPLICIT);
    sixel_weighted_point_set_init(&output);
    memset(&config, 0, sizeof(config));
    filter = NULL;
    pixels[0] = 240u;
    pixels[1] = 0u;
    pixels[2] = 0u;
    pixels[3] = 0u;
    pixels[4] = 240u;
    pixels[5] = 0u;
    pixels[6] = 0u;
    pixels[7] = 0u;
    pixels[8] = 240u;
    pixels[9] = 0u;
    pixels[10] = 240u;
    pixels[11] = 0u;

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
                                       4,
                                       1,
                                       SIXEL_PIXELFORMAT_RGB888,
                                       NULL,
                                       (-1));
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
        4u,
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
            binning.policy.phase != SIXEL_PALETTE_POLICY_EXECUTED ||
            output.ownership != SIXEL_WEIGHTED_POINT_OWNED ||
            output.source_point_count != 4u ||
            output.point_count != 3u ||
            output.total_weight != 4.0 ||
            output.weights[0] != 1.0 ||
            output.coordinates[0] != 0.0 ||
            output.coordinates[1] != 0.0 ||
            output.coordinates[2] != 240.0 ||
            output.weights[1] != 2.0 ||
            output.coordinates[3] != 0.0 ||
            output.coordinates[4] != 240.0 ||
            output.coordinates[5] != 0.0 ||
            output.weights[2] != 1.0 ||
            output.coordinates[6] != 240.0 ||
            output.coordinates[7] != 0.0 ||
            output.coordinates[8] != 0.0) {
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
        fprintf(stderr, "sample-stream binning contract failed\n");
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
