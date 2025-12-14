/*
 * SPDX-License-Identifier: MIT
 *
 * Simple unit tests for filter components. Each test emits TAP output so the
 * surrounding harness can be a thin shell wrapper. The focus is on clip
 * behavior because it recently gained float32 support and conditional
 * activation.
 */

#include "config.h"

/* STDC_HEADERS */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <sixel.h>

#include "allocator.h"
#include "filter-clip.h"
#include "filter-factory.h"
#include "filter.h"
#include "frame.h"

typedef struct test_progress {
    int began;
    int progressed;
    int completed;
    int aborted;
} test_progress_t;

static void progress_cb(sixel_filter_t *filter,
                        sixel_filter_event_t event,
                        int completed_units,
                        int total_units,
                        void *userdata)
{
    test_progress_t *progress;

    progress = (test_progress_t *)userdata;
    if (progress == NULL) {
        return;
    }

    (void)filter;
    (void)completed_units;
    (void)total_units;

    switch (event) {
    case SIXEL_FILTER_EVENT_BEGIN:
        ++progress->began;
        break;
    case SIXEL_FILTER_EVENT_PROGRESS:
        ++progress->progressed;
        break;
    case SIXEL_FILTER_EVENT_COMPLETE:
        ++progress->completed;
        break;
    case SIXEL_FILTER_EVENT_ABORT:
        ++progress->aborted;
        break;
    }
}

static SIXELSTATUS make_allocator(sixel_allocator_t **allocator_out)
{
    SIXELSTATUS status;

    status = sixel_allocator_new(allocator_out,
                                 malloc,
                                 calloc,
                                 realloc,
                                 free);

    return status;
}

static SIXELSTATUS make_rgb_frame(sixel_allocator_t *allocator,
                                  int width,
                                  int height,
                                  sixel_frame_t **frame_out)
{
    SIXELSTATUS status;
    sixel_frame_t *frame;
    unsigned char *pixels;
    size_t bytes;
    int i;

    status = SIXEL_FALSE;
    frame = NULL;
    pixels = NULL;
    bytes = 0U;
    i = 0;

    status = sixel_frame_new(&frame, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    bytes = (size_t)(width * height * 3);
    pixels = (unsigned char *)sixel_allocator_malloc(allocator, bytes);
    if (pixels == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    for (i = 0; i < (int)bytes; ++i) {
        pixels[i] = (unsigned char)(i + 1);
    }

    status = sixel_frame_init(frame,
                              pixels,
                              width,
                              height,
                              SIXEL_PIXELFORMAT_RGB888,
                              NULL,
                              (-1));
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    frame->colorspace = SIXEL_COLORSPACE_GAMMA;

end:
    if (SIXEL_FAILED(status)) {
        sixel_frame_unref(frame);
    } else {
        *frame_out = frame;
    }

    return status;
}

static SIXELSTATUS make_float_frame(sixel_allocator_t *allocator,
                                    int width,
                                    int height,
                                    sixel_frame_t **frame_out)
{
    SIXELSTATUS status;
    sixel_frame_t *frame;
    float *pixels;
    size_t elements;
    int i;

    status = SIXEL_FALSE;
    frame = NULL;
    pixels = NULL;
    elements = 0U;
    i = 0;

    status = sixel_frame_new(&frame, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    elements = (size_t)(width * height * 3);
    pixels = (float *)sixel_allocator_malloc(allocator,
                                             elements * sizeof(float));
    if (pixels == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    for (i = 0; i < (int)elements; ++i) {
        pixels[i] = (float)(i + 1);
    }

    status = sixel_frame_init_float32(frame,
                                      pixels,
                                      width,
                                      height,
                                      SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
                                      NULL,
                                      (-1));
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    frame->colorspace = SIXEL_COLORSPACE_LINEAR;

end:
    if (SIXEL_FAILED(status)) {
        sixel_frame_unref(frame);
    } else {
        *frame_out = frame;
    }

    return status;
}

static int test_clip_noop(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_filter_t *filter;
    sixel_filter_clip_config_t config;
    sixel_frame_t *input_frame;
    sixel_frame_t *output_frame;
    test_progress_t progress;

    status = SIXEL_FALSE;
    allocator = NULL;
    filter = NULL;
    input_frame = NULL;
    output_frame = NULL;
    progress.began = 0;
    progress.progressed = 0;
    progress.completed = 0;
    progress.aborted = 0;

    status = make_allocator(&allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = make_rgb_frame(allocator, 2, 2, &input_frame);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    config.clip_x = 0;
    config.clip_y = 0;
    config.clip_width = 0;
    config.clip_height = 0;

    status = sixel_filter_factory_create_by_kind(SIXEL_FILTER_KIND_CLIP,
                                                 &config,
                                                 &filter);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    sixel_filter_bind_input(filter,
                            &input_frame,
                            input_frame->pixelformat,
                            input_frame->colorspace);
    sixel_filter_bind_output(filter,
                             &output_frame,
                             input_frame->pixelformat,
                             input_frame->colorspace);
    sixel_filter_set_progress(filter, progress_cb, &progress, 1);

    status = sixel_filter_run(filter, allocator, NULL);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    if (input_frame != output_frame) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

    if (input_frame->width != 2 || input_frame->height != 2) {
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
    sixel_allocator_unref(allocator);

    return SIXEL_SUCCEEDED(status);
}

static int test_clip_float_accepts_format(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_filter_t *filter;
    sixel_filter_clip_config_t config;
    sixel_frame_t *input_frame;
    sixel_frame_t *output_frame;
    test_progress_t progress;
    float expected;

    status = SIXEL_FALSE;
    allocator = NULL;
    filter = NULL;
    input_frame = NULL;
    output_frame = NULL;
    progress.began = 0;
    progress.progressed = 0;
    progress.completed = 0;
    progress.aborted = 0;
    expected = 0.0f;

    status = make_allocator(&allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = make_float_frame(allocator, 2, 1, &input_frame);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    expected = input_frame->pixels.f32ptr[3];

    config.clip_x = 1;
    config.clip_y = 0;
    config.clip_width = 1;
    config.clip_height = 1;

    status = sixel_filter_factory_create_by_name("clip",
                                                 &config,
                                                 &filter);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    sixel_filter_bind_input(filter,
                            &input_frame,
                            input_frame->pixelformat,
                            input_frame->colorspace);
    sixel_filter_bind_output(filter,
                             &output_frame,
                             input_frame->pixelformat,
                             input_frame->colorspace);
    sixel_filter_set_progress(filter, progress_cb, &progress, 1);

    status = sixel_filter_run(filter, allocator, NULL);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    if (output_frame == NULL || output_frame->pixelformat !=
            SIXEL_PIXELFORMAT_LINEARRGBFLOAT32) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

    if (output_frame->width != 1 || output_frame->height != 1) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

    if (fabsf(output_frame->pixels.f32ptr[0] - expected) > 0.0001f) {
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
    sixel_allocator_unref(allocator);

    return SIXEL_SUCCEEDED(status);
}

int main(void)
{
    int success;

    success = 1;
    printf("1..2\n");

    if (test_clip_noop()) {
        printf("ok 1 - clip filter skips empty region\n");
    } else {
        printf("not ok 1 - clip filter skips empty region\n");
        success = 0;
    }

    if (test_clip_float_accepts_format()) {
        printf("ok 2 - clip filter trims float32 frames\n");
    } else {
        printf("not ok 2 - clip filter trims float32 frames\n");
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
