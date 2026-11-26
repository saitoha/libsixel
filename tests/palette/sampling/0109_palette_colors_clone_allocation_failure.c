/* SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include <string.h>
#include "src/filter-factory-palette-colors.h"
#include "src/filter-palette-colors.h"
#include "../../_support/processing/filter/filter_test_common.h"

static int palette_clone_live_allocations;
static int palette_clone_pixel_failures;

static void *
palette_clone_malloc(size_t size)
{
    void *ptr;

    /* The 1x1 RGB clone has exactly three pixel bytes. Fail this request
     * after the clone's frame object has been allocated. */
    if (size == 3u) {
        ++palette_clone_pixel_failures;
        return NULL;
    }
    ptr = malloc(size);
    palette_clone_live_allocations += ptr != NULL;
    return ptr;
}

static void *
palette_clone_calloc(size_t count, size_t size)
{
    void *ptr;

    ptr = calloc(count, size);
    palette_clone_live_allocations += ptr != NULL;
    return ptr;
}

static void
palette_clone_free(void *ptr)
{
    palette_clone_live_allocations -= ptr != NULL;
    free(ptr);
}

int
test_palette_colors_clone_allocation_failure(int argc, char **argv)
{
    unsigned char const original[] = {1, 2, 3};
    sixel_allocator_t *allocator;
    sixel_allocator_t *clone_allocator;
    sixel_frame_t *frame;
    sixel_filter_t *filter;
    sixel_sample_stream_t input;
    sixel_sample_stream_t output;
    sixel_filter_palette_colors_config_t config;
    SIXELSTATUS status;
    int result;

    (void)argc;
    (void)argv;
    allocator = NULL;
    clone_allocator = NULL;
    frame = NULL;
    filter = NULL;
    result = EXIT_FAILURE;
    palette_clone_live_allocations = 0;
    palette_clone_pixel_failures = 0;
    sixel_sample_stream_init(&input);
    sixel_sample_stream_init(&output);
    memset(&config, 0, sizeof(config));
    status = make_allocator(&allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = make_rgb_frame(allocator, 1, 1, &frame);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_sample_stream_bind_borrowed(&input, frame,
        SIXEL_PALETTE_SAMPLING_FULL_FRAME,
        SIXEL_PALETTE_SAMPLING_SOURCE_PREPROCESSED_FRAME);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    config.copy_payload = 1;
    config.target_layout = SIXEL_PIXELFORMAT_RGB888;
    config.target_colorspace = SIXEL_COLORSPACE_SRGB;
    status = sixel_filter_factory_create_palette_colors(&config, &filter);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_allocator_new(&clone_allocator, palette_clone_malloc,
        palette_clone_calloc, NULL, palette_clone_free);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    sixel_filter_bind_sample_input(filter, &input);
    sixel_filter_bind_sample_output(filter, &output,
        SIXEL_PIXELFORMAT_RGB888, SIXEL_COLORSPACE_SRGB);
    status = sixel_filter_run(filter, clone_allocator, NULL);
    if (status == SIXEL_BAD_ALLOCATION &&
        palette_clone_pixel_failures == 1 &&
        palette_clone_live_allocations == 1 &&
        output.storage == SIXEL_SAMPLE_STREAM_EMPTY &&
        memcmp(sixel_frame_get_pixels(frame), original,
               sizeof(original)) == 0) {
        result = EXIT_SUCCESS;
    }

end:
    sixel_filter_free(filter);
    sixel_sample_stream_dispose(&output);
    sixel_sample_stream_dispose(&input);
    sixel_frame_unref(frame);
    sixel_allocator_unref(clone_allocator);
    sixel_allocator_unref(allocator);
    return result;
}
