/*
 * Shared helpers for loader pixelformat checks.
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

#include "src/allocator.h"
#include "src/chunk.h"
#include "src/status.h"
#include "src/loader-component.h"

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

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define RGBA_IMAGE_PATH \
    "/tests/data/inputs/formats/rgba.png"
#define JPEG_IMAGE_PATH \
    "/tests/data/inputs/formats/grayscale.jpg"
#define WEBP_IMAGE_PATH \
    "/tests/data/inputs/snake_64.webp"
#define WEBP_ANIMATED_IMAGE_PATH \
    "/tests/data/inputs/formats/animated-lossless-2frame.webp"

typedef struct loader_probe_context {
    int callback_count;
    int pixelformat;
    int width;
    int height;
} loader_probe_context_t;

/*
 * Loader backends expect `context` to follow loader callback state layout.
 *
 * tests/io/loader invokes backend entry points directly, so this test-local
 * adapter mirrors the minimal state needed by cancellation checks while still
 * forwarding the frame callback to loader_probe_context_t.
 */
typedef struct loader_probe_callback_state {
    void *loader;
    sixel_load_image_function fn;
    void *context;
} loader_probe_callback_state_t;

#define GEOMETRY_ANY (-1)
#define SIXEL_TEST_SKIP 77

static SIXEL_TEST_UNUSED SIXELSTATUS
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

static SIXEL_TEST_UNUSED SIXELSTATUS
capture_frame_trampoline(sixel_frame_t *frame, void *data)
{
    loader_probe_callback_state_t *state;

    state = (loader_probe_callback_state_t *)data;
    if (state == NULL || state->fn == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    return state->fn(frame, state->context);
}

static SIXEL_TEST_UNUSED int
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
    int,
    int,
    sixel_load_image_function,
    void *);

static SIXEL_TEST_UNUSED int
run_loader_case_with_options(char const *label,
                             char const *relative_path,
                             int expected_pixelformat,
                             int expected_width,
                             int expected_height,
                             int fstatic,
                             int fuse_palette,
                             int reqcolors,
                             loader_entry_fn loader)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_chunk_t *chunk;
    loader_probe_context_t context;
    char const *source_root;
#if defined(_MSC_VER)
    char *source_root_dupe;
    size_t source_root_len;
#endif
    char image_path[PATH_MAX];
    int cancel_flag;
    int result;
    loader_probe_callback_state_t callback_state;

    status = SIXEL_FALSE;
    allocator = NULL;
    chunk = NULL;
    cancel_flag = 0;
    result = 1;
#if defined(_MSC_VER)
    source_root = NULL;
    source_root_dupe = NULL;
    source_root_len = 0;
    _dupenv_s(&source_root_dupe, &source_root_len, "MESON_SOURCE_ROOT");
    if (source_root_dupe == NULL) {
        _dupenv_s(&source_root_dupe, &source_root_len, "abs_top_srcdir");
    }
    if (source_root_dupe == NULL) {
        _dupenv_s(&source_root_dupe, &source_root_len, "TOP_SRCDIR");
    }
    if (source_root_dupe != NULL) {
        source_root = source_root_dupe;
    }
#else
    source_root = getenv("MESON_SOURCE_ROOT");
    if (source_root == NULL) {
        source_root = getenv("abs_top_srcdir");
    }
    if (source_root == NULL) {
        source_root = getenv("TOP_SRCDIR");
    }
#endif
    if (source_root == NULL) {
        source_root = ".";
    }

    if (build_image_path(source_root,
                         relative_path,
                         image_path,
                         sizeof(image_path)) != 0) {
        fprintf(stderr, "%s: failed to build image path\n", label);
        return 1;
    }

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: allocator initialization failed\n", label);
        return 1;
    }

    status = sixel_chunk_new(&chunk,
                             image_path,
                             0,
                             &cancel_flag,
                             allocator);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: failed to read sample\n", label);
        goto cleanup;
    }

    context.callback_count = 0;
    context.pixelformat = 0;
    context.width = 0;
    context.height = 0;
    callback_state.loader = NULL;
    callback_state.fn = capture_frame;
    callback_state.context = &context;

    status = loader(chunk,
                    fstatic,
                    fuse_palette,
                    reqcolors,
                    NULL,
                    SIXEL_LOOP_AUTO,
                    0,
                    INT_MIN,
                    capture_frame_trampoline,
                    &callback_state);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "%s: loader reported failure (%d)\n",
                label,
                (int)status);
        goto cleanup;
    }

    if (context.callback_count != 1) {
        fprintf(stderr, "%s: callback count mismatch\n", label);
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

    if (context.pixelformat != expected_pixelformat) {
        fprintf(stderr,
                "%s: reported pixelformat %d\n",
                label,
                context.pixelformat);
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

    if (context.width <= 0 || context.height <= 0) {
        fprintf(stderr,
                "%s: invalid geometry %dx%d\n",
                label,
                context.width,
                context.height);
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

    if (expected_width != GEOMETRY_ANY && expected_height != GEOMETRY_ANY) {
        if (context.width != expected_width ||
            context.height != expected_height) {
            fprintf(stderr,
                    "%s: unexpected geometry %dx%d\n",
                    label,
                    context.width,
                    context.height);
            status = SIXEL_BAD_ARGUMENT;
            goto cleanup;
        }
    }
    result = 0;

cleanup:
    sixel_chunk_destroy(chunk);
    sixel_allocator_unref(allocator);
#if defined(_MSC_VER)
    free(source_root_dupe);
#endif

    return result;
}



typedef SIXELSTATUS (*loader_component_new_fn)(
    sixel_allocator_t *,
    sixel_loader_component_t **);

static SIXEL_TEST_UNUSED int
run_loader_component_case(char const *label,
                          char const *relative_path,
                          int expected_pixelformat,
                          int expected_width,
                          int expected_height,
                          loader_component_new_fn new_component)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_chunk_t *chunk;
    sixel_loader_component_t *component;
    loader_probe_context_t context;
    char const *source_root;
#if defined(_MSC_VER)
    char *source_root_dupe;
    size_t source_root_len;
#endif
    char image_path[PATH_MAX];
    int cancel_flag;
    int result;
    int require_static;
    int use_palette;
    int reqcolors;
    loader_probe_callback_state_t callback_state;

    status = SIXEL_FALSE;
    allocator = NULL;
    chunk = NULL;
    component = NULL;
    cancel_flag = 0;
    result = 1;
    require_static = 1;
    use_palette = 0;
    reqcolors = 256;
#if defined(_MSC_VER)
    source_root = NULL;
    source_root_dupe = NULL;
    source_root_len = 0;
    _dupenv_s(&source_root_dupe, &source_root_len, "MESON_SOURCE_ROOT");
    if (source_root_dupe == NULL) {
        _dupenv_s(&source_root_dupe, &source_root_len, "abs_top_srcdir");
    }
    if (source_root_dupe == NULL) {
        _dupenv_s(&source_root_dupe, &source_root_len, "TOP_SRCDIR");
    }
    if (source_root_dupe != NULL) {
        source_root = source_root_dupe;
    }
#else
    source_root = getenv("MESON_SOURCE_ROOT");
    if (source_root == NULL) {
        source_root = getenv("abs_top_srcdir");
    }
    if (source_root == NULL) {
        source_root = getenv("TOP_SRCDIR");
    }
#endif
    if (source_root == NULL) {
        source_root = ".";
    }

    if (build_image_path(source_root,
                         relative_path,
                         image_path,
                         sizeof(image_path)) != 0) {
        fprintf(stderr, "%s: failed to build image path\n", label);
        return 1;
    }

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: allocator initialization failed\n", label);
        return 1;
    }

    status = sixel_chunk_new(&chunk,
                             image_path,
                             0,
                             &cancel_flag,
                             allocator);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: failed to read sample\n", label);
        goto cleanup;
    }

    status = new_component(allocator, &component);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: component init failed (%d)\n", label, (int)status);
        goto cleanup;
    }

    context.callback_count = 0;
    context.pixelformat = 0;
    context.width = 0;
    context.height = 0;
    callback_state.loader = NULL;
    callback_state.fn = capture_frame;
    callback_state.context = &context;

    status = sixel_loader_component_setopt(component,
                                           SIXEL_LOADER_OPTION_REQUIRE_STATIC,
                                           &require_static);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_loader_component_setopt(component,
                                           SIXEL_LOADER_OPTION_USE_PALETTE,
                                           &use_palette);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_loader_component_setopt(component,
                                           SIXEL_LOADER_OPTION_REQCOLORS,
                                           &reqcolors);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = sixel_loader_component_load(component,
                                         chunk,
                                         capture_frame_trampoline,
                                         &callback_state);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "%s: loader reported failure (%d)\n",
                label,
                (int)status);
        goto cleanup;
    }

    if (context.callback_count != 1) {
        fprintf(stderr, "%s: callback count mismatch\n", label);
        goto cleanup;
    }

    if (context.pixelformat != expected_pixelformat) {
        fprintf(stderr,
                "%s: reported pixelformat %d\n",
                label,
                context.pixelformat);
        goto cleanup;
    }

    if (context.width <= 0 || context.height <= 0) {
        fprintf(stderr,
                "%s: invalid geometry %dx%d\n",
                label,
                context.width,
                context.height);
        goto cleanup;
    }

    if (expected_width != GEOMETRY_ANY && expected_height != GEOMETRY_ANY) {
        if (context.width != expected_width ||
            context.height != expected_height) {
            fprintf(stderr,
                    "%s: unexpected geometry %dx%d\n",
                    label,
                    context.width,
                    context.height);
            goto cleanup;
        }
    }

    result = 0;

cleanup:
    sixel_loader_component_unref(component);
    sixel_chunk_destroy(chunk);
    sixel_allocator_unref(allocator);
#if defined(_MSC_VER)
    free(source_root_dupe);
#endif

    return result;
}

static SIXEL_TEST_UNUSED int
run_loader_case(char const *label,
                char const *relative_path,
                int expected_pixelformat,
                int expected_width,
                int expected_height,
                loader_entry_fn loader)
{
    return run_loader_case_with_options(label,
                                        relative_path,
                                        expected_pixelformat,
                                        expected_width,
                                        expected_height,
                                        1,
                                        0,
                                        256,
                                        loader);
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
