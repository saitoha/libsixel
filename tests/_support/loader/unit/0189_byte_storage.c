/* SPDX-License-Identifier: MIT */
/* Guard byte storage AND the absence of a transient RGB float image. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/frame-private.h"
#include "src/loader.h"
#include "src/loader-manager.h"

static size_t byte_probe_float_size;
static int byte_probe_float_allocations;
static int byte_probe_calls;
static int byte_probe_expect_float;

static void *
byte_probe_malloc(size_t size)
{
    if (size == byte_probe_float_size) {
        ++byte_probe_float_allocations;
    }
    return malloc(size);
}

static void *
byte_probe_calloc(size_t count, size_t size)
{
    if (size != 0u && count == byte_probe_float_size / size &&
        count * size == byte_probe_float_size) {
        ++byte_probe_float_allocations;
    }
    return calloc(count, size);
}

static void *
byte_probe_realloc(void *ptr, size_t size)
{
    if (size == byte_probe_float_size) {
        ++byte_probe_float_allocations;
    }
    return realloc(ptr, size);
}

static SIXELSTATUS
byte_probe_capture(sixel_frame_t *frame, void *context)
{
    (void)context;
    ++byte_probe_calls;
    if (byte_probe_expect_float) {
        return SIXEL_PIXELFORMAT_IS_FLOAT32(frame->pixelformat)
             ? SIXEL_OK : SIXEL_BAD_INPUT;
    }
    if ((frame->pixelformat != SIXEL_PIXELFORMAT_RGB888 &&
         frame->pixelformat != SIXEL_PIXELFORMAT_RGBA8888) ||
        frame->colorspace != SIXEL_COLORSPACE_GAMMA ||
        (size_t)frame->width * frame->height * 3u * sizeof(float) !=
            byte_probe_float_size) {
        fprintf(stderr, "byte frame contract: format=%d, size=%dx%d\n",
            frame->pixelformat, frame->width, frame->height);
        return SIXEL_BAD_INPUT;
    }
    return SIXEL_OK;
}

int
test_loader_0189_byte_storage(int argc, char **argv)
{
    sixel_allocator_t *allocator;
    sixel_loader_t *loader;
    SIXELSTATUS status;
    char order[160];
    int no;
    int length;

    if (argc != 7) { return 1; }
    if (!loader_manager_entry_available(argv[1])) { return 77; }
    allocator = NULL;
    loader = NULL;
    no = 0;
    byte_probe_float_size = (size_t)atoi(argv[4]) * 3u * sizeof(float);
    byte_probe_float_allocations = 0;
    byte_probe_calls = 0;
    byte_probe_expect_float = atoi(argv[5]);
    length = snprintf(order, sizeof(order), "%s:cms_engine=%s!",
        argv[1], argv[2]);
    if (length < 0 || (size_t)length >= sizeof(order)) { return 1; }
    status = sixel_allocator_new(&allocator, byte_probe_malloc,
        byte_probe_calloc, byte_probe_realloc, free);
    if (SIXEL_FAILED(status)) { goto end; }
    status = sixel_loader_new(&loader, allocator);
    if (SIXEL_FAILED(status)) { goto end; }
    status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_LOADER_ORDER,
        order);
    if (SIXEL_FAILED(status)) { goto end; }
    status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_USE_PALETTE, &no);
    if (SIXEL_FAILED(status)) { goto end; }
    sixel_loader_set_prefer_float32(loader, byte_probe_expect_float);
    status = sixel_loader_load_file(loader, argv[3], byte_probe_capture);
end:
    sixel_loader_unref(loader);
    sixel_allocator_unref(allocator);
    /* The final frame alone would miss a float-to-byte roundtrip. Source
     * fixtures have separate exact-value coverage; this is a storage guard.
     * Fixtures are large enough to distinguish image buffers from the
     * fixed-size allocator, loader and frame metadata allocations. */
    if (!byte_probe_expect_float && atoi(argv[6]) &&
        byte_probe_float_allocations != 0) {
        fprintf(stderr, "unexpected full RGB float allocations: %d\n",
            byte_probe_float_allocations);
        return 1;
    }
    return SIXEL_FAILED(status) || byte_probe_calls != 1;
}
