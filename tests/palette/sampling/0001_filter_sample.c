/*
 * SPDX-License-Identifier: MIT
 *
 * Unit tests for the sample filter. These tests exercise typed sample-stream
 * output, stride selection, clipping support, and progress reporting.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "src/cms.h"
#include "src/filter-factory.h"
#include "src/filter-sample.h"
#include "src/filter.h"
#include "tests/_support/processing/filter/filter_test_common.h"

#include "tests/_support/planner/pipeline/playback_test_common.h"

/* Check display colors independently of how the encoder orders registers. */
static int
test_indexed_capacity(char const *capacity, char const *sampling)
{
    static unsigned char const expected[8][3] = {
        {0, 255, 0}, {0, 255, 0}, {255, 0, 0}, {255, 0, 0},
        {255, 0, 0}, {255, 0, 0}, {0, 255, 0}, {0, 255, 0}
    };
    unsigned char source[] = {1, 1, 0, 0, 0, 0, 1, 1};
    unsigned char palette[] = {255, 0, 0, 0, 255, 0};
    unsigned char *indices = NULL, *decoded = NULL;
    playback_test_buffer_t stream = {0};
    sixel_frame_t *frame = NULL;
    sixel_output_t *output = NULL;
    sixel_encoder_t *encoder = NULL;
    int i, width, height, ncolors;
    int result = EXIT_FAILURE;

    if (SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
        SIXEL_FAILED(sixel_frame_init_borrowed(frame, source, 4, 2,
            SIXEL_PIXELFORMAT_PAL8, palette, 2)) ||
        SIXEL_FAILED(sixel_output_new(&output, playback_test_write,
            &stream, NULL)) ||
        SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
        SIXEL_FAILED(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_GPU_POLICY,
            "off")) ||
        SIXEL_FAILED(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_DIFFUSION,
            "none")) ||
        SIXEL_FAILED(sixel_encoder_setopt(encoder,
            SIXEL_OPTFLAG_SAMPLING_POLICY, sampling)) ||
        (capacity != NULL && SIXEL_FAILED(sixel_encoder_setopt(encoder,
            SIXEL_OPTFLAG_COLORS, capacity))) ||
        SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame, output)) ||
        SIXEL_FAILED(sixel_decode_raw(stream.bytes, (int)stream.size, &indices,
            &width, &height, &decoded, &ncolors, NULL)) ||
        width != 4 || height != 2 || indices == NULL || decoded == NULL) {
        goto end;
    }
    for (i = 0; i < 8; ++i) {
        if (indices[i] >= ncolors ||
            memcmp(decoded + 3 * indices[i], expected[i], 3)) {
            goto end;
        }
    }
    result = EXIT_SUCCESS;
end:
    free(indices);
    free(decoded);
    free(stream.bytes);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    sixel_frame_unref(frame);
    return result;
}

static SIXELSTATUS
test_frame_get_pixels_view(sixel_frame_t const *frame,
                           sixel_frame_pixels_view_t *view)
{
    sixel_frame_interface_t *frame_if;

    frame_if = NULL;
    if (frame == NULL || view == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    frame_if = (sixel_frame_interface_t *)frame;
    if (frame_if->vtbl == NULL || frame_if->vtbl->get_pixels == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    return frame_if->vtbl->get_pixels(frame_if, view);
}

static int
test_sample_stride_override(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_filter_t *filter;
    sixel_filter_sample_config_t config = {0};
    sixel_frame_t *input_frame;
    sixel_sample_stream_t samples;
    sixel_frame_pixels_view_t input_view;
    sixel_frame_pixels_view_t sample_view;
    test_progress_t progress;
    size_t stride;
    size_t total;
    int expected_width;
    int expected_height;
    unsigned char expected_first;
    unsigned char expected_last;

    status = SIXEL_FALSE;
    allocator = NULL;
    filter = NULL;
    input_frame = NULL;
    sixel_sample_stream_init(&samples);
    memset(&input_view, 0, sizeof(input_view));
    memset(&sample_view, 0, sizeof(sample_view));
    progress.began = 0;
    progress.progressed = 0;
    progress.completed = 0;
    progress.aborted = 0;
    stride = 1u;
    total = 0u;
    expected_width = 0;
    expected_height = 0;
    expected_first = 0;
    expected_last = 0;

    status = make_allocator(&allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = make_rgb_frame(allocator, 4, 4, &input_frame);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = test_frame_get_pixels_view(input_frame, &input_view);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    config.policy = SIXEL_PALETTE_SAMPLING_ADAPTIVE_GRID;
    config.source = SIXEL_PALETTE_SAMPLING_SOURCE_LOADED_FRAME;
    config.clip_x = 0;
    config.clip_y = 0;
    config.clip_width = 0;
    config.clip_height = 0;
    config.reqcolors = 256;
    config.quality_mode = SIXEL_QUALITY_AUTO;
    config.palette_sample_override = 1;
    config.palette_sample_target = 1;

    status = sixel_filter_factory_create_by_name("sample",
                                                 &config,
                                                 &filter);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    sixel_filter_bind_input(filter,
                            &input_frame,
                            input_view.pixelformat,
                            input_view.colorspace);
    sixel_filter_bind_sample_output(filter,
                                    &samples,
                                    input_view.pixelformat,
                                    input_view.colorspace);
    sixel_filter_set_progress(filter, progress_cb, &progress, 1);

    expected_first = input_view.pixels[0];
    expected_last = input_view.pixels[((size_t)(input_view.width
                    * (input_view.height - 1)
                    + (input_view.width - 1))) * 3u];

    total = (size_t)input_view.width * (size_t)input_view.height;
    while (stride < total
           && total / (stride * stride)
               > config.palette_sample_target) {
        ++stride;
    }

    expected_width = (input_view.width + (int)stride - 1) / (int)stride;
    expected_height = (input_view.height + (int)stride - 1)
            / (int)stride;

    status = sixel_filter_run(filter, allocator, NULL);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    if (samples.frame == NULL || samples.frame == input_frame ||
            samples.storage != SIXEL_SAMPLE_STREAM_OWNED_FRAME ||
            samples.policy != SIXEL_PALETTE_SAMPLING_ADAPTIVE_GRID ||
            samples.source != SIXEL_PALETTE_SAMPLING_SOURCE_LOADED_FRAME) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }
    status = test_frame_get_pixels_view(samples.frame, &sample_view);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    if (sample_view.width != expected_width
            || sample_view.height != expected_height) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

    if (sample_view.pixels[0] != expected_first
            || sample_view.pixels[(expected_width * expected_height - 1) * 3]
                != expected_last) {
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
    sixel_sample_stream_dispose(&samples);
    sixel_frame_unref(input_frame);
    sixel_allocator_unref(allocator);

    return SIXEL_SUCCEEDED(status);
}

static int
test_sample_respects_clip_region(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_filter_t *filter;
    sixel_filter_sample_config_t config = {0};
    sixel_frame_t *input_frame;
    sixel_frame_t *sample_frame;
    sixel_frame_pixels_view_t input_view;
    sixel_frame_pixels_view_t sample_view;
    test_progress_t progress;
    unsigned char expected;

    status = SIXEL_FALSE;
    allocator = NULL;
    filter = NULL;
    input_frame = NULL;
    sample_frame = NULL;
    memset(&input_view, 0, sizeof(input_view));
    memset(&sample_view, 0, sizeof(sample_view));
    progress.began = 0;
    progress.progressed = 0;
    progress.completed = 0;
    progress.aborted = 0;
    expected = 0;

    status = make_allocator(&allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = make_rgb_frame(allocator, 3, 2, &input_frame);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = test_frame_get_pixels_view(input_frame, &input_view);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    expected = input_view.pixels[3];

    config.policy = SIXEL_PALETTE_SAMPLING_AUTO;
    config.source = SIXEL_PALETTE_SAMPLING_SOURCE_NONE;
    config.clip_x = 1;
    config.clip_y = 0;
    config.clip_width = 2;
    config.clip_height = 1;
    config.reqcolors = 256;
    config.quality_mode = SIXEL_QUALITY_AUTO;
    config.palette_sample_override = 0;
    config.palette_sample_target = 0;

    status = sixel_filter_factory_create_by_kind(SIXEL_FILTER_KIND_SAMPLE,
                                                 &config,
                                                 &filter);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    sixel_filter_bind_input(filter,
                            &input_frame,
                            input_view.pixelformat,
                            input_view.colorspace);
    sixel_filter_bind_output(filter,
                             &sample_frame,
                             input_view.pixelformat,
                             input_view.colorspace);
    sixel_filter_set_progress(filter, progress_cb, &progress, 1);

    status = sixel_filter_run(filter, allocator, NULL);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    if (sample_frame == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }
    status = test_frame_get_pixels_view(sample_frame, &sample_view);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    if (sample_view.width != 2 || sample_view.height != 1) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

    if (sample_view.pixels[0] != expected) {
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
    sixel_frame_unref(input_frame);
    sixel_frame_unref(sample_frame);
    sixel_allocator_unref(allocator);

    return SIXEL_SUCCEEDED(status);
}

static int
test_sample_full_frame_is_borrowed(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_filter_t *filter;
    sixel_filter_sample_config_t config = {0};
    sixel_frame_t *input_frame;
    sixel_sample_stream_t samples;
    test_progress_t progress;

    status = SIXEL_FALSE;
    allocator = NULL;
    filter = NULL;
    input_frame = NULL;
    sixel_sample_stream_init(&samples);
    progress.began = 0;
    progress.progressed = 0;
    progress.completed = 0;
    progress.aborted = 0;

    status = make_allocator(&allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = make_rgb_frame(allocator, 3, 2, &input_frame);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    config.policy = SIXEL_PALETTE_SAMPLING_FULL_FRAME;
    config.source = SIXEL_PALETTE_SAMPLING_SOURCE_PREPROCESSED_FRAME;
    status = sixel_filter_factory_create_by_kind(
        SIXEL_FILTER_KIND_SAMPLE,
        &config,
        &filter);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    sixel_filter_bind_input(filter,
                            &input_frame,
                            sixel_frame_get_pixelformat(input_frame),
                            sixel_frame_get_colorspace(input_frame));
    sixel_filter_bind_sample_output(
        filter,
        &samples,
        sixel_frame_get_pixelformat(input_frame),
        sixel_frame_get_colorspace(input_frame));
    sixel_filter_set_progress(filter, progress_cb, &progress, 1);

    status = sixel_filter_run(filter, allocator, NULL);
    if (SIXEL_FAILED(status) || samples.frame != input_frame ||
            samples.storage != SIXEL_SAMPLE_STREAM_BORROWED_FRAME ||
            samples.policy != SIXEL_PALETTE_SAMPLING_FULL_FRAME ||
            samples.source !=
                SIXEL_PALETTE_SAMPLING_SOURCE_PREPROCESSED_FRAME ||
            samples.point_count != 6u ||
            progress.began != 1 || progress.completed != 1 ||
            progress.aborted != 0) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }
    sixel_sample_stream_dispose(&samples);
    if (sixel_frame_get_width(input_frame) != 3) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }
    status = SIXEL_OK;

cleanup:
    sixel_filter_teardown(filter);
    sixel_filter_free(filter);
    sixel_sample_stream_dispose(&samples);
    sixel_frame_unref(input_frame);
    sixel_allocator_unref(allocator);

    return SIXEL_SUCCEEDED(status);
}

/* Crop to red and release the source. The grid sample must still carry
 * the Display P3 red primary, including its negative linear sRGB channels. */
static int
test_profiled_grid(void)
{
    float pixels[] = {0, 1, 0, 1, 0, 0};
    double const red[] = {1.2249401763, -.0420569547, -.0196375546};
    sixel_frame_t *frame = NULL;
    sixel_allocator_t *allocator = NULL;
    sixel_frame_interface_t *iface;
    sixel_frame_pixels_view_t view;
    sixel_sample_stream_t samples;
    sixel_filter_sample_config_t config = {0};
    sixel_filter_t *filter = NULL;
    sixel_color_def_t *profile;
    int c;
    int result = EXIT_FAILURE;

    profile = sixel_cdef_name("display-p3");
    sixel_sample_stream_init(&samples);
    config.policy = SIXEL_PALETTE_SAMPLING_ADAPTIVE_GRID;
    config.source = SIXEL_PALETTE_SAMPLING_SOURCE_LOADED_FRAME;
    config.clip_x = 1;
    config.clip_width = config.clip_height = 1;
    if (profile == NULL || SIXEL_FAILED(make_allocator(&allocator)) ||
        SIXEL_FAILED(sixel_frame_new(&frame, allocator)) ||
        SIXEL_FAILED(sixel_frame_init_borrowed(frame, pixels, 2, 1,
            SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, -1)) ||
        SIXEL_FAILED(sixel_filter_factory_create_by_name("sample", &config,
                                                         &filter))) {
        goto end;
    }
    iface = sixel_frame_as_interface(frame);
    if (SIXEL_FAILED(iface->vtbl->set_rgb_definition(iface, profile)) ||
        SIXEL_FAILED(iface->vtbl->get_pixels(iface, &view))) {
        goto end;
    }
    sixel_filter_bind_input(filter, &frame, view.pixelformat, view.colorspace);
    sixel_filter_bind_sample_output(filter, &samples, view.pixelformat,
                                    view.colorspace);
    if (SIXEL_FAILED(sixel_filter_run(filter, allocator, NULL)) ||
        samples.frame == NULL || samples.frame == frame) {
        goto end;
    }
    sixel_frame_unref(frame);
    frame = NULL;
    iface = sixel_frame_as_interface(samples.frame);
    if (SIXEL_FAILED(iface->vtbl->get_pixels(iface, &view)) ||
        view.width != 1 || view.height != 1 || view.rgb_definition == NULL ||
        SIXEL_FAILED(iface->vtbl->set_pixelformat(iface,
            SIXEL_PIXELFORMAT_LINEARRGBFLOAT32)) ||
        SIXEL_FAILED(iface->vtbl->get_pixels(iface, &view)) ||
        view.pixels_float32 == NULL) {
        goto end;
    }
    for (c = 0; c < 3; ++c) {
        if (!(fabs(view.pixels_float32[c] - red[c]) < 1e-5)) {
            goto end;
        }
    }
    result = EXIT_SUCCESS;
end:
    sixel_filter_free(filter);
    sixel_sample_stream_dispose(&samples);
    sixel_frame_unref(frame);
    sixel_allocator_unref(allocator);
    sixel_cdef_free(profile);
    return result;
}

int
test_filter_0002_filter_sample(int argc, char **argv)
{
    int success;

    if (argc == 2 && strcmp(argv[1], "indexed-capacity") == 0) {
        return test_indexed_capacity(NULL, "adaptive-grid") ||
               test_indexed_capacity("256", "adaptive-grid") ||
               test_indexed_capacity(NULL, "full-frame");
    }
    if (argc == 2 && strcmp(argv[1], "profiled-grid") == 0) {
        return test_profiled_grid();
    }
    if (argc != 1) {
        return EXIT_FAILURE;
    }

    success = 1;

    if (!test_sample_stride_override()) {
        fprintf(stderr, "sample filter honors override target failed\n");
        success = 0;
    }

    if (!test_sample_respects_clip_region()) {
        fprintf(stderr, "sample filter crops before sampling failed\n");
        success = 0;
    }

    if (!test_sample_full_frame_is_borrowed()) {
        fprintf(stderr, "sample filter full-frame borrowing failed\n");
        success = 0;
    }

    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "gnu"
 * indent-tabs-mode: nil
 * End:
 */
