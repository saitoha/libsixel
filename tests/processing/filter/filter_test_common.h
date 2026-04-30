/*
 * SPDX-License-Identifier: MIT
 *
 * Common helpers for filter unit tests. These routines build small frames,
 * track progress callbacks, and provide a shared allocator constructor so
 * individual test files stay focused on filter behavior.
 */

#ifndef LIBSIXEL_TESTS_FILTER_TEST_COMMON_H
#define LIBSIXEL_TESTS_FILTER_TEST_COMMON_H

/* STDC_HEADERS */
#include <math.h>
#include <stdlib.h>

#include <sixel.h>

#include "src/allocator.h"
#include "src/dither.h"
#include "src/frame-private.h"

#if defined(__clang__)
# if __has_attribute(unused)
#  define SIXEL_TEST_UNUSED __attribute__((unused))
# else
#  define SIXEL_TEST_UNUSED
# endif
#elif defined(__GNUC__)
# define SIXEL_TEST_UNUSED __attribute__((unused))
#else
# define SIXEL_TEST_UNUSED
#endif

typedef struct test_progress {
    int began;
    int progressed;
    int completed;
    int aborted;
} test_progress_t;

typedef struct test_output_counter {
    int calls;
    int bytes;
} test_output_counter_t;

static SIXEL_TEST_UNUSED void
progress_cb(sixel_filter_t *filter,
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

static SIXEL_TEST_UNUSED SIXELSTATUS
make_allocator(sixel_allocator_t **allocator_out)
{
    SIXELSTATUS status;

    status = sixel_allocator_new(allocator_out,
                                 malloc,
                                 calloc,
                                 realloc,
                                 free);

    return status;
}

static SIXEL_TEST_UNUSED SIXELSTATUS
make_rgb_frame(sixel_allocator_t *allocator,
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

    sixel_frame_set_colorspace(frame, SIXEL_COLORSPACE_GAMMA);

end:
    if (SIXEL_FAILED(status)) {
        sixel_frame_unref(frame);
    } else {
        *frame_out = frame;
    }

    return status;
}

static SIXEL_TEST_UNUSED SIXELSTATUS
make_float_frame(sixel_allocator_t *allocator,
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

    status = sixel_frame_as_interface(frame)->vtbl->init_pixels(
        sixel_frame_as_interface(frame),
        &(sixel_frame_pixels_request_t){
            pixels,
            NULL,
            width,
            height,
            SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
            SIXEL_COLORSPACE_LINEAR,
            (-1),
            SIXEL_FRAME_PIXELS_FLOAT32
        });
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    pixels = NULL;

end:
    if (SIXEL_FAILED(status)) {
        sixel_allocator_free(allocator, pixels);
        sixel_frame_unref(frame);
    } else {
        *frame_out = frame;
    }

    return status;
}

static SIXEL_TEST_UNUSED SIXELSTATUS
make_dither(sixel_allocator_t *allocator,
            int ncolors,
            sixel_dither_t **dither_out)
{
    SIXELSTATUS status;
    sixel_dither_t *dither;

    status = SIXEL_FALSE;
    dither = NULL;

    if (allocator == NULL || dither_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_dither_new(&dither, ncolors, allocator);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    *dither_out = dither;

    return SIXEL_OK;
}

static int
output_counter_write(char *data, int size, void *priv)
{
    test_output_counter_t *counter;

    counter = (test_output_counter_t *)priv;
    if (counter == NULL) {
        return 0;
    }

    counter->calls += 1;
    counter->bytes += size;

    (void)data;

    return size;
}

static SIXEL_TEST_UNUSED SIXELSTATUS
make_counter_output(sixel_allocator_t *allocator,
                    test_output_counter_t *counter,
                    sixel_output_t **output_out)
{
    SIXELSTATUS status;

    status = SIXEL_FALSE;

    if (allocator == NULL || counter == NULL || output_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    counter->calls = 0;
    counter->bytes = 0;

    status = sixel_output_new(output_out,
                              output_counter_write,
                              counter,
                              allocator);

    return status;
}

#endif /* LIBSIXEL_TESTS_FILTER_TEST_COMMON_H */
