/*
 * SPDX-License-Identifier: MIT
 *
 * Palette filter failure-output test. This verifies that a partial dither
 * returned by a failing builder is released and removed from the output slot.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include <sixel.h>

#include "src/filter-factory.h"
#include "src/filter-palette.h"
#include "src/filter.h"
#include "tests/processing/filter/filter_test_common.h"

typedef struct failing_builder_context {
    sixel_allocator_t *allocator;
    int calls;
    int free_count_after_build;
} failing_builder_context_t;

static int palette_failure_free_count;

static void *
palette_failure_malloc(size_t size)
{
    return malloc(size);
}

static void *
palette_failure_calloc(size_t count, size_t size)
{
    return calloc(count, size);
}

static void *
palette_failure_realloc(void *ptr, size_t size)
{
    return realloc(ptr, size);
}

static void
palette_failure_free(void *ptr)
{
    if (ptr != NULL) {
        ++palette_failure_free_count;
    }
    free(ptr);
}

static SIXELSTATUS
failing_palette_builder(void *userdata,
                        sixel_sample_stream_t *samples,
                        sixel_dither_t **dither_out,
                        sixel_timeline_logger_t *logger)
{
    SIXELSTATUS status;
    failing_builder_context_t *context;

    status = SIXEL_FALSE;
    context = (failing_builder_context_t *)userdata;

    (void)logger;

    if (context == NULL || samples == NULL || samples->frame == NULL ||
            dither_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    ++context->calls;
    status = sixel_dither_new(dither_out, 8, context->allocator);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    context->free_count_after_build = palette_failure_free_count;

    return SIXEL_BAD_ALLOCATION;
}

static int
test_palette_failure_clears_partial_output(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_frame_t *frame;
    sixel_filter_t *filter;
    sixel_dither_t *dither;
    sixel_sample_stream_t samples;
    sixel_filter_palette_config_t config;
    failing_builder_context_t context;

    status = SIXEL_FALSE;
    allocator = NULL;
    frame = NULL;
    filter = NULL;
    dither = NULL;
    sixel_sample_stream_init(&samples);
    context.allocator = NULL;
    context.calls = 0;
    context.free_count_after_build = 0;
    palette_failure_free_count = 0;

    status = sixel_allocator_new(&allocator,
                                 palette_failure_malloc,
                                 palette_failure_calloc,
                                 palette_failure_realloc,
                                 palette_failure_free);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    context.allocator = allocator;

    status = make_rgb_frame(allocator, 1, 1, &frame);
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

    config.builder = failing_palette_builder;
    config.builder_userdata = &context;
    config.dither_out = &dither;
    status = sixel_filter_factory_create_by_kind(
        SIXEL_FILTER_KIND_PALETTE,
        &config,
        &filter);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    sixel_filter_bind_sample_input(filter, &samples);
    status = sixel_filter_run(filter, allocator, NULL);
    if (status != SIXEL_BAD_ALLOCATION || dither != NULL ||
            context.calls != 1 ||
            palette_failure_free_count <= context.free_count_after_build) {
        fprintf(stderr,
                "status=%04x output=%p calls=%d frees=%d build-frees=%d\n",
                status,
                (void *)dither,
                context.calls,
                palette_failure_free_count,
                context.free_count_after_build);
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

    status = SIXEL_OK;

cleanup:
    sixel_filter_free(filter);
    sixel_sample_stream_dispose(&samples);
    if (dither != NULL) {
        sixel_dither_unref(dither);
    }
    sixel_frame_unref(frame);
    sixel_allocator_unref(allocator);

    return SIXEL_SUCCEEDED(status);
}

int
test_filter_0030_filter_palette_failure_output(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (!test_palette_failure_clears_partial_output()) {
        fprintf(stderr, "palette failure left a partial dither output\n");
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
