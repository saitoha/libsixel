/*
 * Verify CoreGraphics loads RGBA sources as four-component frames.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include <sixel.h>

#include "allocator.h"
#include "chunk.h"
#include "loader-coregraphics.h"
#include "status.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define RGBA_IMAGE_PATH \
    "/tests/data/inputs/formats/rgba.png"

typedef struct loader_probe_context {
    int callback_count;
    int pixelformat;
    int width;
    int height;
} loader_probe_context_t;

static SIXELSTATUS
capture_frame(sixel_frame_t *frame, void *data)
{
    loader_probe_context_t *context;

    context = (loader_probe_context_t *)data;
    context->callback_count += 1;
    context->pixelformat = sixel_frame_get_pixelformat(frame);
    context->width = sixel_frame_get_width(frame);
    context->height = sixel_frame_get_height(frame);

    return SIXEL_OK;
}

#if HAVE_COREGRAPHICS
static int
run_coregraphics_loader_test(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_chunk_t *chunk;
    loader_probe_context_t context;
    char const *source_root;
    char image_path[PATH_MAX];
    int cancel_flag;
    int written;

    status = SIXEL_FALSE;
    allocator = NULL;
    chunk = NULL;
    cancel_flag = 0;
    source_root = getenv("MESON_SOURCE_ROOT");
    if (source_root == NULL) {
        source_root = ".";
    }

    written = snprintf(image_path,
                       sizeof(image_path),
                       "%s" RGBA_IMAGE_PATH,
                       source_root);
    if (written < 0 || (size_t)written >= sizeof(image_path)) {
        printf("not ok 1 - failed to build image path\n");
        return 1;
    }

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        printf("not ok 1 - allocator initialization failed\n");
        return 1;
    }

    status = sixel_chunk_new(&chunk, image_path, 0, &cancel_flag, allocator);
    if (SIXEL_FAILED(status)) {
        printf("not ok 1 - failed to read RGBA sample\n");
        goto cleanup;
    }

    context.callback_count = 0;
    context.pixelformat = 0;
    context.width = 0;
    context.height = 0;

    status = load_with_coregraphics(chunk,
                                    1,
                                    0,
                                    256,
                                    NULL,
                                    SIXEL_LOOP_AUTO,
                                    capture_frame,
                                    &context);
    if (SIXEL_FAILED(status)) {
        printf("not ok 1 - CoreGraphics loader reported failure\n");
        goto cleanup;
    }

    if (context.callback_count != 1) {
        printf("not ok 1 - loader callback count mismatch\n");
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

    if (context.pixelformat != SIXEL_PIXELFORMAT_RGBA8888) {
        printf("not ok 1 - loader reported pixelformat %d\n",
               context.pixelformat);
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

    if (context.width != 2 || context.height != 1) {
        printf("not ok 1 - unexpected frame geometry %dx%d\n",
               context.width,
               context.height);
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

    printf("ok 1 - CoreGraphics loader preserves RGBA format\n");

cleanup:
    sixel_chunk_destroy(chunk);
    sixel_allocator_unref(allocator);

    return SIXEL_FAILED(status);
}
#endif

int
main(void)
{
#if HAVE_COREGRAPHICS
    printf("1..1\n");
    return run_coregraphics_loader_test();
#else
    printf("1..0 # SKIP CoreGraphics loader unavailable\n");
    return 0;
#endif
}
