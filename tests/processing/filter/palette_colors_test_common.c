/* SPDX-License-Identifier: MIT
 * The oracle uses an explicit four-point population. Conversion is checked
 * against the sRGB transfer formula, independently of the frame converter.
 */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "src/filter-factory.h"
#include "src/filter-palette-colors.h"
#include "src/filter-binning.h"
#include "filter_test_common.h"
#include "palette_colors_test_common.h"

int
palette_colors_test(int scenario)
{
    sixel_allocator_t *allocator;
    sixel_frame_t *frame;
    sixel_sample_stream_t input;
    sixel_sample_stream_t output;
    sixel_filter_palette_colors_config_t config;
    sixel_filter_binning_config_t bin_config;
    sixel_palette_binning_state_t binning;
    sixel_weighted_point_set_t points;
    sixel_filter_t *filter;
    sixel_frame_transparency_t transparency;
    sixel_sample_statistics_t *statistics;
    unsigned char bytes[12] = {0, 0, 0, 128, 128, 128,
                              128, 128, 128, 255, 255, 255};
    float floats[12] = {0.0f, 0.0f, 0.0f, 0.1f, 0.2f, 0.3f,
                       0.1f, 0.2f, 0.3f, 0.0f, 0.0f, 0.0f};
    unsigned char mask[4] = {0, 0, 0, 1};
    double expected;
    double observed;
    size_t index;
    size_t weight;
    unsigned int axis;
    SIXELSTATUS status;
    int result;

    allocator = NULL;
    frame = NULL;
    filter = NULL;
    result = EXIT_FAILURE;
    memset(&config, 0, sizeof(config));
    memset(&bin_config, 0, sizeof(bin_config));
    memset(&transparency, 0, sizeof(transparency));
    sixel_sample_stream_init(&input);
    sixel_sample_stream_init(&output);
    sixel_weighted_point_set_init(&points);
    status = make_allocator(&allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (scenario == 50) {
        status = make_rgb_frame(allocator, 4, 1, &frame);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        memcpy(sixel_frame_get_pixels(frame), bytes, sizeof(bytes));
        status = sixel_sample_stream_bind_borrowed(&input, frame,
            SIXEL_PALETTE_SAMPLING_ADAPTIVE_GRID,
            SIXEL_PALETTE_SAMPLING_SOURCE_LOADED_FRAME);
        config.convert = 1;
        config.target_pixelformat = SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
        config.target_colorspace = SIXEL_COLORSPACE_LINEAR;
    } else if (scenario == 52 || scenario == 53) {
        floats[9] = NAN;
        if (scenario == 52) {
            transparency.transparent_mask = mask;
            transparency.transparent_mask_size = sizeof(mask);
        }
        status = sixel_sample_stream_bind_borrowed_buffer(&input, floats,
            sizeof(floats), SIXEL_PIXELFORMAT_OKLABFLOAT32,
            SIXEL_COLORSPACE_OKLAB, &transparency,
            SIXEL_PALETTE_SAMPLING_FULL_FRAME,
            SIXEL_PALETTE_SAMPLING_SOURCE_PREPROCESSED_FRAME);
    } else {
        status = sixel_sample_stream_bind_borrowed_buffer(&input, bytes,
            sizeof(bytes), SIXEL_PIXELFORMAT_RGB888,
            SIXEL_COLORSPACE_GAMMA, NULL,
            SIXEL_PALETTE_SAMPLING_FULL_FRAME,
            SIXEL_PALETTE_SAMPLING_SOURCE_PREPROCESSED_FRAME);
    }
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    config.collect_statistics = scenario != 51;
    status = sixel_filter_factory_create_by_name("palette-colorspace",
        &config, &filter);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    sixel_filter_bind_sample_input(filter, &input);
    sixel_filter_bind_sample_output(filter, &output,
        config.convert ? config.target_pixelformat : input.pixelformat,
        config.convert ? config.target_colorspace : input.colorspace);
    status = sixel_filter_run(filter, allocator, NULL);
    if (scenario == 53) {
        if (status == SIXEL_BAD_INPUT &&
                output.storage == SIXEL_SAMPLE_STREAM_EMPTY &&
                output.statistics == NULL) {
            result = EXIT_SUCCESS;
        }
        goto end;
    }
    if (SIXEL_FAILED(status) || output.policy != input.policy ||
            output.source != input.source) {
        goto end;
    }
    /* Metadata and observations survive destruction of the producing filter. */
    sixel_filter_free(filter);
    filter = NULL;
    statistics = output.statistics;
    if (scenario == 51) {
        if (statistics == NULL && output.buffer == input.buffer) {
            result = EXIT_SUCCESS;
        }
        goto end;
    }
    if (statistics == NULL || statistics->colorspace != output.colorspace ||
            statistics->point_count != (scenario == 52 ? 3u : 4u)) {
        goto end;
    }
    if (scenario == 54) {
        sixel_palette_binning_state_init(&binning,
            SIXEL_PALETTE_BINNING_AUTO, SIXEL_PALETTE_POLICY_ORIGIN_DEFAULT);
        status = sixel_palette_binning_resolve(&binning,
            SIXEL_PALETTE_BINNING_HARD, 5u,
            SIXEL_PALETTE_BINNING_GRID_UNIFORM,
            SIXEL_PALETTE_BINNING_KERNEL_NONE,
            SIXEL_PALETTE_BINNING_BACKEND_COMPACT_SPARSE,
            4u, SIXEL_PALETTE_RESOLUTION_EXPLICIT);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        statistics->colorspace = SIXEL_COLORSPACE_LINEAR;
        bin_config.binning = &binning;
        bin_config.adaptive_range = 1;
        status = sixel_filter_factory_create_by_name("binning", &bin_config,
            &filter);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        sixel_filter_bind_sample_input(filter, &output);
        sixel_filter_bind_weighted_output(filter, &points, output.colorspace);
        status = sixel_filter_run(filter, allocator, NULL);
        if (status == SIXEL_LOGIC_ERROR &&
                points.ownership == SIXEL_WEIGHTED_POINT_EMPTY) {
            result = EXIT_SUCCESS;
        }
        goto end;
    }
    for (axis = 0u; axis < 3u; ++axis) {
        if (scenario == 49) {
            if (statistics->byte_counts[axis][0] != 1u ||
                    statistics->byte_counts[axis][128] != 2u ||
                    statistics->byte_counts[axis][255] != 1u ||
                    output.buffer != input.buffer) {
                goto end;
            }
        } else {
            weight = 0u;
            observed = 0.0;
            for (index = 0u; index < statistics->capacities[axis]; ++index) {
                weight += statistics->axes[axis][index].count;
                observed += statistics->axes[axis][index].value *
                    (double)statistics->axes[axis][index].count;
            }
            expected = scenario == 50
                ? 1.0 + 2.0 * pow((128.0 / 255.0 + 0.055) / 1.055, 2.4)
                : 2.0 * (double)floats[3u + axis];
            if (weight != statistics->point_count ||
                    fabs(observed - expected) > 3e-7) {
                goto end;
            }
        }
    }
    if (scenario == 50 && (output.frame == input.frame ||
            memcmp(sixel_frame_get_pixels(frame), bytes, sizeof(bytes)) != 0 ||
            output.colorspace != SIXEL_COLORSPACE_LINEAR)) {
        goto end;
    }
    result = EXIT_SUCCESS;
end:
    if (result != EXIT_SUCCESS) {
        fprintf(stderr, "palette colors scenario %d failed: %x\n",
            scenario, status);
    }
    sixel_filter_free(filter);
    sixel_weighted_point_set_dispose(&points);
    sixel_sample_stream_dispose(&output);
    sixel_sample_stream_dispose(&input);
    sixel_frame_unref(frame);
    sixel_allocator_unref(allocator);
    return result;
}
