/* SPDX-License-Identifier: MIT
 * The independent oracle is the input's output-distinguishable gray tones:
 * the dark ramp and bright UI fit in the smallest model's 32-address axis.
 */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/dither.h"
#include "src/palette.h"
#include "src/filter-binning.h"
#include "src/filter-palette-colors.h"
#include "src/filter-factory.h"
#include "src/sample-stream.h"
#include "src/weighted-point-set.h"
#include "src/palette-bin-histogram.h"
#include "src/colorspace.h"
#include "src/palette-common-snap.h"
#include "adaptive_range_test_common.h"

int
adaptive_range_test_palette(int model)
{
    sixel_dither_t *dither;
    SIXELSTATUS status;
    unsigned char pixels[64 * 64 * 3];
    unsigned char seen[256];
    unsigned char const *palette;
    sixel_palette_entries_view_t view;
    unsigned int x;
    unsigned int y;
    unsigned int value;
    unsigned int i;
    unsigned int count;
    int result;

    dither = NULL;
    result = EXIT_FAILURE;
    memset(seen, 0, sizeof(seen));
    for (y = 0u; y < 64u; ++y) {
        for (x = 0u; x < 64u; ++x) {
            value = x >= 61u ? (x == 61u ? 244u :
                x == 62u ? 250u : 255u) : (x + y) % 24u;
            i = (y * 64u + x) * 3u;
            pixels[i] = pixels[i + 1u] = pixels[i + 2u] =
                (unsigned char)value;
        }
    }
    status = sixel_dither_new(&dither, 256, NULL);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    dither->quantize_model = model;
    status = sixel_dither_initialize(dither, pixels, 64, 64,
        SIXEL_PIXELFORMAT_RGB888, SIXEL_LARGE_AUTO, SIXEL_REP_AUTO,
        SIXEL_QUALITY_FULL);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    memset(&view, 0, sizeof(view));
    status = dither->palette->vtbl->get_entries(dither->palette, &view);
    if (SIXEL_FAILED(status) || view.entries == NULL || view.depth != 3) {
        goto cleanup;
    }
    palette = view.entries;
    count = view.entry_count;
    for (i = 0u; i < count; ++i) {
        value = palette[i * 3u];
        if (palette[i * 3u + 1u] == value &&
                palette[i * 3u + 2u] == value) {
            seen[sixel_safe_tones[value]] = 1u;
        }
    }
    for (value = 0u; value < 24u; ++value) {
        if (!seen[sixel_safe_tones[value]]) {
            fprintf(stderr, "model %d lost safe dark tone %u (%u colors)\n",
                model, value, count);
            goto cleanup;
        }
    }
    if (!seen[sixel_safe_tones[244]] || !seen[sixel_safe_tones[250]] ||
            !seen[sixel_safe_tones[255]]) {
        fprintf(stderr, "model %d lost a bright UI color\n", model);
        goto cleanup;
    }
    result = EXIT_SUCCESS;
cleanup:
    sixel_dither_unref(dither);
    return result;
}

/* The stream filter must retain native coordinates and exclude the same
 * transparent pixels from range statistics and histogram mass. */
int
adaptive_range_test_stream(int hide_highlight)
{
    sixel_allocator_t *allocator;
    sixel_sample_stream_t transformed;
    sixel_filter_palette_colors_config_t colors;
    sixel_sample_stream_t samples;
    sixel_palette_binning_state_t binning;
    sixel_weighted_point_set_t points;
    sixel_filter_binning_config_t config;
    sixel_frame_transparency_t transparency;
    sixel_filter_t *filter;
    SIXELSTATUS status;
    float pixels[12];
    unsigned char mask[4];
    unsigned int i;
    unsigned int channel;
    unsigned int count;
    unsigned int found;
    int result;

    allocator = NULL;
    filter = NULL;
    result = EXIT_FAILURE;
    count = hide_highlight ? 3u : 4u;
    memset(&config, 0, sizeof(config));
    memset(&transparency, 0, sizeof(transparency));
    memset(mask, 0, sizeof(mask));
    mask[3] = (unsigned char)hide_highlight;
    transparency.transparent_mask = mask;
    transparency.transparent_mask_size = sizeof(mask);
    sixel_sample_stream_init(&transformed);
    memset(&colors, 0, sizeof(colors));
    colors.collect_statistics = 1;
    sixel_sample_stream_init(&samples);
    sixel_weighted_point_set_init(&points);
    sixel_palette_binning_state_init(&binning, SIXEL_PALETTE_BINNING_HARD,
        SIXEL_PALETTE_POLICY_ORIGIN_EXPLICIT);
    for (i = 0u; i < 4u; ++i) {
        for (channel = 0u; channel < 3u; ++channel) {
            pixels[i * 3u + channel] = i == 3u ? 1.0f :
                (float)(i + 1u) * 0.005f;
        }
    }
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_sample_stream_bind_borrowed_buffer(&samples, pixels,
        sizeof(pixels), SIXEL_PIXELFORMAT_RGBFLOAT32, SIXEL_COLORSPACE_GAMMA,
        &transparency, SIXEL_PALETTE_SAMPLING_FULL_FRAME,
        SIXEL_PALETTE_SAMPLING_SOURCE_PREPROCESSED_FRAME);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_filter_factory_create_by_name("palette-colorspace",
        &colors, &filter);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    sixel_filter_bind_sample_input(filter, &samples);
    sixel_filter_bind_sample_output(filter, &transformed,
        samples.pixelformat, samples.colorspace);
    status = sixel_filter_run(filter, allocator, NULL);
    sixel_filter_free(filter);
    filter = NULL;
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_palette_binning_resolve(&binning,
        SIXEL_PALETTE_BINNING_HARD, 6u, SIXEL_PALETTE_BINNING_GRID_UNIFORM,
        SIXEL_PALETTE_BINNING_KERNEL_NONE,
        SIXEL_PALETTE_BINNING_BACKEND_COMPACT_SPARSE, 4u,
        SIXEL_PALETTE_RESOLUTION_EXPLICIT);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    config.binning = &binning;
    config.input_is_float32 = 1;
    config.adaptive_range = 1;
    config.requested_colors = 256u;
    for (channel = 0u; channel < 3u; ++channel) {
        config.scale[channel] = 255.0;
    }
    status = sixel_filter_factory_create_by_kind(SIXEL_FILTER_KIND_BINNING,
        &config, &filter);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    sixel_filter_bind_sample_input(filter, &transformed);
    sixel_filter_bind_weighted_output(filter, &points, SIXEL_COLORSPACE_GAMMA);
    status = sixel_filter_run(filter, allocator, NULL);
    if (SIXEL_FAILED(status) || points.point_count != count ||
            points.total_weight != count || points.weights == NULL) {
        goto cleanup;
    }
    found = 0u;
    for (i = 0u; i < count; ++i) {
        for (channel = 0u; channel < count; ++channel) {
            if (points.coordinates[i * 3u] == pixels[channel * 3u] &&
                    points.coordinates[i * 3u + 1u] ==
                        pixels[channel * 3u + 1u] &&
                    points.coordinates[i * 3u + 2u] ==
                        pixels[channel * 3u + 2u] &&
                    points.weights[i] == 1.0) {
                found |= 1u << channel;
            }
        }
    }
    if (found == (1u << count) - 1u) {
        result = EXIT_SUCCESS;
    }
cleanup:
    sixel_filter_free(filter);
    sixel_weighted_point_set_dispose(&points);
    sixel_sample_stream_dispose(&transformed);
    sixel_sample_stream_dispose(&samples);
    sixel_allocator_unref(allocator);
    return result;
}

/* Independent small populations distinguish raw coordinate variation from
 * recoverable output colors, and exercise the strict palette-size boundary. */
int
adaptive_range_test_gate(int scenario)
{
    sixel_allocator_t *allocator;
    sixel_sample_statistics_t *statistics;
    sixel_weighted_point_set_t points;
    double coordinates[12];
    double gamma[3];
    double scale[3];
    double offset[3];
    unsigned int values[4];
    unsigned int index;
    unsigned int axis;
    unsigned int count;
    unsigned int requested;
    unsigned int expected_safe;
    int expected_gate;
    int eligible;
    int space;
    int result;
    SIXELSTATUS status;

    allocator = NULL;
    statistics = NULL;
    result = EXIT_FAILURE;
    values[0] = 0u;
    values[1] = 3u;
    values[2] = 5u;
    values[3] = 255u;
    count = 3u;
    requested = 256u;
    expected_safe = 3u;
    expected_gate = 1;
    space = SIXEL_COLORSPACE_GAMMA;
    if (scenario == 16) {
        values[1] = 8u;
        values[2] = 16u;
        requested = 3u;
        expected_gate = 0;
    } else if (scenario == 17) {
        values[0] = 4u;
        values[1] = 5u;
        values[2] = 6u;
        expected_safe = 1u;
        expected_gate = 0;
    } else if (scenario == 19) {
        /* These dark codes share one coarse L bin after conversion. */
        space = SIXEL_COLORSPACE_CIELAB;
    } else if (scenario == 20) {
        requested = 1u;
        expected_gate = 0;
    } else if (scenario == 21) {
        count = 4u;
        expected_safe = 4u;
    }
    sixel_weighted_point_set_init(&points);
    points.coordinates = coordinates;
    points.point_count = count;
    for (axis = 0u; axis < 3u; ++axis) {
        scale[axis] = 255.0;
        offset[axis] = 0.0;
    }
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_sample_statistics_new(&statistics, space,
        scenario != 19, allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    for (index = 0u; index < count; ++index) {
        for (axis = 0u; axis < 3u; ++axis) {
            gamma[axis] = (double)values[index] / 255.0;
            coordinates[index * 3u + axis] = values[index];
        }
        if (scenario == 19 && !sixel_color_convert(
                coordinates + index * 3u, gamma, SIXEL_COLORSPACE_GAMMA,
                space)) {
            goto cleanup;
        }
        status = sixel_sample_statistics_observe(statistics,
            coordinates + index * 3u);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
    }
    eligible = -1;
    status = sixel_palette_bin_range_gate(statistics, NULL, &points,
        32u, requested, scale, offset, SIXEL_PALETTE_BINNING_GRID_UNIFORM,
        0, NULL, &eligible);
    if (SIXEL_SUCCEEDED(status) && eligible == expected_gate &&
            statistics->safe_color_count == expected_safe) {
        result = EXIT_SUCCESS;
    } else {
        fprintf(stderr, "gate %d: status=%d eligible=%d safe=%u\n",
            scenario, status, eligible, statistics->safe_color_count);
    }
cleanup:
    sixel_sample_statistics_unref(statistics);
    sixel_allocator_unref(allocator);
    return result;
}
