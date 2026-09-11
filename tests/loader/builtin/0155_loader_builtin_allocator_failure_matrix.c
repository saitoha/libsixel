/* Verify allocation-failure propagation and cleanup across builtin formats. */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <6cells.h>

#include "loader_builtin_memory_test_common.h"

typedef struct allocator_fixture {
    char const *label;
    char const *path;
} allocator_fixture_t;

typedef struct allocator_capture {
    unsigned int callback_count;
    uint64_t digest;
    size_t size;
    int width;
    int height;
    int pixelformat;
    int colorspace;
    int alpha_zero_is_transparent;
    size_t mask_size;
    uint64_t mask_digest;
} allocator_capture_t;

static unsigned int loader0155_allocation_count;
static unsigned int loader0155_allocation_target;
static unsigned int loader0155_live_count;
static size_t loader0155_failure_size;

static void *
loader0155_malloc(size_t size)
{
    void *ptr;

    ptr = NULL;
    ++loader0155_allocation_count;
    if (loader0155_allocation_count == loader0155_allocation_target) {
        loader0155_failure_size = size;
        return NULL;
    }
    ptr = malloc(size);
    if (ptr != NULL) {
        ++loader0155_live_count;
    }
    return ptr;
}

static void *
loader0155_calloc(size_t count, size_t size)
{
    void *ptr;

    ptr = NULL;
    ++loader0155_allocation_count;
    if (loader0155_allocation_count == loader0155_allocation_target) {
        loader0155_failure_size = count * size;
        return NULL;
    }
    ptr = calloc(count, size);
    if (ptr != NULL) {
        ++loader0155_live_count;
    }
    return ptr;
}

static void *
loader0155_realloc(void *ptr, size_t size)
{
    void *resized;
    int had_allocation;

    resized = NULL;
    had_allocation = ptr != NULL;
    ++loader0155_allocation_count;
    if (loader0155_allocation_count == loader0155_allocation_target) {
        loader0155_failure_size = size;
        return NULL;
    }
    resized = realloc(ptr, size);
    if (!had_allocation && resized != NULL) {
        ++loader0155_live_count;
    }
    return resized;
}

static void
loader0155_free(void *ptr)
{
    if (ptr != NULL && loader0155_live_count != 0u) {
        --loader0155_live_count;
    }
    free(ptr);
}

static SIXELSTATUS
loader0155_capture(sixel_frame_t *frame, void *data)
{
    allocator_capture_t *capture;
    sixel_frame_interface_t *frame_if;
    sixel_frame_transparency_t transparency;
    SIXELSTATUS status;
    unsigned char const *pixels;
    int depth;

    capture = (allocator_capture_t *)data;
    frame_if = NULL;
    memset(&transparency, 0, sizeof(transparency));
    status = SIXEL_FALSE;
    pixels = NULL;
    depth = 0;
    if (frame == NULL || capture == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    capture->width = sixel_frame_get_width(frame);
    capture->height = sixel_frame_get_height(frame);
    capture->pixelformat = sixel_frame_get_pixelformat(frame);
    capture->colorspace = sixel_frame_get_colorspace(frame);
    depth = sixel_helper_compute_depth(capture->pixelformat);
    pixels = sixel_frame_get_pixels(frame);
    if (capture->width <= 0 || capture->height <= 0 || depth <= 0 ||
        pixels == NULL) {
        return SIXEL_BAD_INPUT;
    }
    capture->size = (size_t)capture->width * (size_t)capture->height *
                    (size_t)depth;
    capture->digest = edge_digest_bytes(pixels, capture->size);
    frame_if = sixel_frame_as_interface(frame);
    if (frame_if == NULL || frame_if->vtbl == NULL ||
        frame_if->vtbl->get_transparency == NULL) {
        return SIXEL_BAD_INPUT;
    }
    status = frame_if->vtbl->get_transparency(frame_if, &transparency);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    capture->alpha_zero_is_transparent =
        transparency.alpha_zero_is_transparent;
    capture->mask_size = transparency.transparent_mask_size;
    capture->mask_digest = 0u;
    if (transparency.transparent_mask != NULL &&
        transparency.transparent_mask_size != 0u) {
        capture->mask_digest = edge_digest_bytes(
            transparency.transparent_mask,
            transparency.transparent_mask_size);
    }
    ++capture->callback_count;
    return SIXEL_OK;
}

int
test_loader_0155_loader_builtin_allocator_failure_matrix(int argc,
                                                          char **argv)
{
    static allocator_fixture_t const fixtures[] = {
        { "PNG", "/tests/data/inputs/formats/libpng-minimal-1x1-rgba.png" },
        { "JPEG", "/tests/data/inputs/formats/"
                  "snake-jpeg-8bit-rgb-seq444.jpg" },
        { "GIF", "/tests/data/inputs/snake_64.gif" },
        { "WebP", "/tests/data/inputs/snake_16.webp" },
        { "HDR", "/tests/data/inputs/formats/stbi_minimal.hdr" },
        { "PSD", "/tests/data/inputs/formats/stbi_minimal.psd" },
        { "BMP", "/tests/data/inputs/formats/"
                 "bmp-info40-topdown-24bpp-2x2.bmp" },
        { "TGA", "/tests/data/inputs/formats/tga-rgba-2x2-top-left.tga" },
        { "PIC", "/tests/data/inputs/formats/stbi_minimal.pic" },
        { "PNM", "/tests/data/inputs/snake_64.ppm" },
        { "SIXEL", "/tests/data/inputs/snake_64.six" }
    };
    edge_loader_options_t options;
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    size_t fixture_index;
    unsigned int allocation_total;
    unsigned int failure_target;
    unsigned int live_baseline;
    allocator_capture_t baseline;
    allocator_capture_t actual;
    int result;

    (void)argc;
    (void)argv;
    edge_loader_options_init(&options);
    options.require_static = 1;
    status = SIXEL_FALSE;
    allocator = NULL;
    fixture_index = 0u;
    allocation_total = 0u;
    failure_target = 0u;
    live_baseline = 0u;
    baseline.callback_count = 0u;
    baseline.digest = 0u;
    baseline.size = 0u;
    baseline.width = 0;
    baseline.height = 0;
    baseline.pixelformat = 0;
    baseline.colorspace = 0;
    baseline.alpha_zero_is_transparent = 0;
    baseline.mask_size = 0u;
    baseline.mask_digest = 0u;
    actual = baseline;
    result = 1;
    loader0155_allocation_count = 0u;
    loader0155_allocation_target = 0u;
    loader0155_live_count = 0u;
    loader0155_failure_size = 0u;
    status = sixel_allocator_new(&allocator,
                                 loader0155_malloc,
                                 loader0155_calloc,
                                 loader0155_realloc,
                                 loader0155_free);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    live_baseline = loader0155_live_count;
    for (fixture_index = 0u;
         fixture_index < sizeof(fixtures) / sizeof(fixtures[0]);
         ++fixture_index) {
        loader0155_allocation_count = 0u;
        loader0155_allocation_target = 0u;
        actual = baseline;
        actual.callback_count = 0u;
        status = SIXEL_FALSE;
        result = edge_load_fixture_custom(fixtures[fixture_index].label,
                                          fixtures[fixture_index].path,
                                          &options,
                                          allocator,
                                          loader0155_capture,
                                          &actual,
                                          &status);
        allocation_total = loader0155_allocation_count;
        if (result != 0 || status != SIXEL_OK ||
            actual.callback_count != 1u ||
            allocation_total == 0u ||
            loader0155_live_count != live_baseline) {
            fprintf(stderr, "%s baseline allocation load failed\n",
                    fixtures[fixture_index].label);
            goto cleanup;
        }
        baseline = actual;
        for (failure_target = 1u;
             failure_target <= allocation_total;
             ++failure_target) {
            loader0155_allocation_count = 0u;
            loader0155_allocation_target = failure_target;
            loader0155_failure_size = 0u;
            actual = baseline;
            actual.callback_count = 0u;
            status = SIXEL_FALSE;
            result = edge_load_fixture_custom(fixtures[fixture_index].label,
                                              fixtures[fixture_index].path,
                                              &options,
                                              allocator,
                                              loader0155_capture,
                                              &actual,
                                              &status);
            loader0155_allocation_target = 0u;
            if (result != 0 ||
                (status != SIXEL_OK && !SIXEL_FAILED(status)) ||
                (SIXEL_FAILED(status) && actual.callback_count != 0u) ||
                (status == SIXEL_OK &&
                 (actual.callback_count != 1u ||
                  actual.digest != baseline.digest ||
                  actual.size != baseline.size ||
                  actual.width != baseline.width ||
                  actual.height != baseline.height ||
                  actual.pixelformat != baseline.pixelformat ||
                  actual.colorspace != baseline.colorspace ||
                  actual.alpha_zero_is_transparent !=
                      baseline.alpha_zero_is_transparent ||
                  actual.mask_size != baseline.mask_size ||
                  actual.mask_digest != baseline.mask_digest)) ||
                loader0155_live_count != live_baseline) {
                result = 1;
                fprintf(stderr,
                        "%s allocation %u/%u size=%lu: result=%d status=%d "
                        "callbacks=%u live=%u/%u\n",
                        fixtures[fixture_index].label,
                        failure_target,
                        allocation_total,
                        (unsigned long)loader0155_failure_size,
                        result,
                        (int)status,
                        actual.callback_count,
                        loader0155_live_count,
                        live_baseline);
                goto cleanup;
            }
        }
    }
    result = 0;

cleanup:
    loader0155_allocation_target = 0u;
    sixel_allocator_unref(allocator);
    if (loader0155_live_count != 0u) {
        fprintf(stderr, "builtin allocation test leaked %u allocations\n",
                loader0155_live_count);
        return 1;
    }
    return result;
}
