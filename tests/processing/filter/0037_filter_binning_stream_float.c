/*
 * SPDX-License-Identifier: MIT
 *
 * Verify that float sample streams reject non-finite source coordinates and
 * incomplete mapping parameters before histogram indexing.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdint.h>
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
test_filter_0037_filter_binning_stream_float(int argc, char **argv)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_frame_t *frame;
    sixel_sample_stream_t samples;
    sixel_palette_binning_state_t binning;
    sixel_weighted_point_set_t output;
    sixel_filter_binning_config_t config;
    sixel_filter_t *filter;
    union {
        uint32_t bits;
        float value;
    } not_a_number;
    float pixels[3];

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
    not_a_number.bits = UINT32_C(0x7fc00000);
    pixels[0] = not_a_number.value;
    pixels[1] = 0.25f;
    pixels[2] = 0.75f;

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
                                       1,
                                       1,
                                       SIXEL_PIXELFORMAT_RGBFLOAT32,
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
        1u,
        SIXEL_PALETTE_RESOLUTION_EXPLICIT);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    config.binning = &binning;
    config.input_is_float32 = 1;
    config.scale[0] = 255.0;
    config.scale[1] = 255.0;
    config.scale[2] = 255.0;
    config.coordinate_mode = SIXEL_FILTER_BINNING_COORDINATE_CLAMPED;
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
    if (status != SIXEL_BAD_INPUT ||
            binning.policy.phase != SIXEL_PALETTE_POLICY_RESOLVED ||
            output.ownership != SIXEL_WEIGHTED_POINT_EMPTY) {
        status = SIXEL_LOGIC_ERROR;
        goto cleanup;
    }
    sixel_filter_free(filter);
    filter = NULL;

    pixels[0] = 0.5f;
    config.scale[0] = 0.0;
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
    if (status != SIXEL_BAD_ARGUMENT ||
            binning.policy.phase != SIXEL_PALETTE_POLICY_RESOLVED ||
            output.ownership != SIXEL_WEIGHTED_POINT_EMPTY) {
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
        fprintf(stderr, "sample-stream float contract failed\n");
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
