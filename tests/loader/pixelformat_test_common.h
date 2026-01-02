/*
 * Shared helpers for loader pixelformat TAP tests.
 */

#ifndef PIXELFORMAT_TEST_COMMON_H
#define PIXELFORMAT_TEST_COMMON_H

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include <sixel.h>

#include "allocator.h"
#include "chunk.h"
#include "status.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define RGBA_IMAGE_PATH \
    "/tests/data/inputs/formats/rgba.png"
#define JPEG_IMAGE_PATH \
    "/tests/data/inputs/formats/grayscale.jpg"

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

static int
build_image_path(char const *source_root,
                 char const *relative,
                 char *buffer,
                 size_t capacity)
{
    int written;

    written = snprintf(buffer, capacity, "%s%s", source_root, relative);
    if (written < 0 || (size_t)written >= capacity) {
        return 1;
    }

    return 0;
}

typedef SIXELSTATUS (*loader_entry_fn)(
    sixel_chunk_t const *,
    int,
    int,
    int,
    unsigned char *,
    int,
    sixel_load_image_function,
    void *);

static int
run_loader_case(char const *label,
                char const *relative_path,
                int expected_pixelformat,
                int expected_width,
                int expected_height,
                loader_entry_fn loader)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_chunk_t *chunk;
    loader_probe_context_t context;
    char const *source_root;
    char image_path[PATH_MAX];
    int cancel_flag;

    status = SIXEL_FALSE;
    allocator = NULL;
    chunk = NULL;
    cancel_flag = 0;
    source_root = getenv("MESON_SOURCE_ROOT");
    if (source_root == NULL) {
        source_root = ".";
    }

    if (build_image_path(source_root,
                         relative_path,
                         image_path,
                         sizeof(image_path)) != 0) {
        printf("not ok 1 - %s failed to build image path\n", label);
        return 1;
    }

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        printf("not ok 1 - %s allocator initialization failed\n", label);
        return 1;
    }

    status = sixel_chunk_new(&chunk,
                             image_path,
                             0,
                             &cancel_flag,
                             allocator);
    if (SIXEL_FAILED(status)) {
        printf("not ok 1 - %s failed to read sample\n", label);
        goto cleanup;
    }

    context.callback_count = 0;
    context.pixelformat = 0;
    context.width = 0;
    context.height = 0;

    status = loader(chunk,
                    1,
                    0,
                    256,
                    NULL,
                    SIXEL_LOOP_AUTO,
                    capture_frame,
                    &context);
    if (SIXEL_FAILED(status)) {
        printf("not ok 1 - %s loader reported failure (%d)\n",
               label,
               (int)status);
        goto cleanup;
    }

    if (context.callback_count != 1) {
        printf("not ok 1 - %s callback count mismatch\n", label);
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

    if (context.pixelformat != expected_pixelformat) {
        printf("not ok 1 - %s reported pixelformat %d\n",
               label,
               context.pixelformat);
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

    if (context.width != expected_width || context.height != expected_height) {
        printf("not ok 1 - %s unexpected geometry %dx%d\n",
               label,
               context.width,
               context.height);
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

    printf("ok 1 - %s preserves expected pixelformat\n", label);

cleanup:
    sixel_chunk_destroy(chunk);
    sixel_allocator_unref(allocator);

    return SIXEL_FAILED(status);
}

#endif /* PIXELFORMAT_TEST_COMMON_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
