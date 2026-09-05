/*
 * SPDX-License-Identifier: MIT
 *
 * Verify the half-open [0, 256) hard grid used by legacy K-center.
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
test_filter_0041_filter_binning_uniform_256(int argc, char **argv)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_sample_stream_t samples;
    sixel_palette_binning_state_t binning;
    sixel_weighted_point_set_t output;
    sixel_filter_binning_config_t config;
    sixel_filter_t *filter;
    float pixels[6];

    (void)argc;
    (void)argv;
    status = SIXEL_FALSE;
    allocator = NULL;
    sixel_sample_stream_init(&samples);
    sixel_palette_binning_state_init(
        &binning,
        SIXEL_PALETTE_BINNING_HARD,
        SIXEL_PALETTE_POLICY_ORIGIN_EXPLICIT);
    sixel_weighted_point_set_init(&output);
    memset(&config, 0, sizeof(config));
    filter = NULL;
    pixels[0] = 0.49f;
    pixels[1] = 0.0f;
    pixels[2] = 0.0f;
    pixels[3] = 0.50f;
    pixels[4] = 0.0f;
    pixels[5] = 0.0f;

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_sample_stream_bind_borrowed_buffer(
        &samples,
        pixels,
        sizeof(pixels),
        SIXEL_PIXELFORMAT_OKLABFLOAT32,
        SIXEL_COLORSPACE_OKLAB,
        NULL,
        SIXEL_PALETTE_SAMPLING_FULL_FRAME,
        SIXEL_PALETTE_SAMPLING_SOURCE_PREPROCESSED_FRAME);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_palette_binning_resolve(
        &binning,
        SIXEL_PALETTE_BINNING_HARD,
        5u,
        SIXEL_PALETTE_BINNING_GRID_UNIFORM_256,
        SIXEL_PALETTE_BINNING_KERNEL_NONE,
        SIXEL_PALETTE_BINNING_BACKEND_COMPACT_SPARSE,
        2u,
        SIXEL_PALETTE_RESOLUTION_EXPLICIT);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    config.binning = &binning;
    config.scale[0] = 255.0;
    config.scale[1] = 255.0;
    config.scale[2] = 255.0;
    config.offset[1] = 127.5;
    config.offset[2] = 127.5;
    config.coordinate_mode =
        SIXEL_FILTER_BINNING_COORDINATE_CLAMPED;
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
                                      SIXEL_COLORSPACE_OKLAB);
    status = sixel_filter_run(filter, allocator, NULL);
    if (SIXEL_FAILED(status) || output.point_count != 1u ||
            output.weights == NULL || output.weights[0] != 2.0 ||
            output.coordinates[0] <= 0.494 ||
            output.coordinates[0] >= 0.496) {
        status = SIXEL_LOGIC_ERROR;
        goto cleanup;
    }
    status = SIXEL_OK;

cleanup:
    sixel_filter_free(filter);
    sixel_weighted_point_set_dispose(&output);
    sixel_sample_stream_dispose(&samples);
    sixel_allocator_unref(allocator);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "uniform-256 hard-grid contract failed\n");
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
