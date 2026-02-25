/*
 * Shared helpers for librsvg loader regression tests.
 */

#ifndef LIBRSVG_TEST_COMMON_H
#define LIBRSVG_TEST_COMMON_H

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "src/allocator.h"
#include "src/chunk.h"
#include "src/loader-component.h"
#include "src/loader-factory.h"

#ifndef SIXEL_TEST_SKIP
#define SIXEL_TEST_SKIP 77
#endif

#ifndef SIXEL_TEST_UNUSED
# if defined(__clang__)
#  if __has_attribute(unused)
#   define SIXEL_TEST_UNUSED __attribute__((unused))
#  else
#   define SIXEL_TEST_UNUSED
#  endif
# elif defined(__GNUC__)
#  define SIXEL_TEST_UNUSED __attribute__((unused))
# else
#  define SIXEL_TEST_UNUSED
# endif
#endif

typedef struct librsvg_probe_context {
    int callback_count;
    int pixelformat;
    int width;
    int height;
    unsigned char first_pixel[3];
} librsvg_probe_context_t;

typedef struct librsvg_callback_state {
    sixel_load_image_function fn;
    void *context;
} librsvg_callback_state_t;

static SIXEL_TEST_UNUSED SIXELSTATUS
librsvg_capture_frame(sixel_frame_t *frame, void *data)
{
    librsvg_probe_context_t *context;
    unsigned char const *pixels;

    context = (librsvg_probe_context_t *)data;
    context->callback_count += 1;
    context->pixelformat = sixel_frame_get_pixelformat(frame);
    context->width = sixel_frame_get_width(frame);
    context->height = sixel_frame_get_height(frame);

    pixels = sixel_frame_get_pixels(frame);
    if (pixels != NULL) {
        context->first_pixel[0] = pixels[0];
        context->first_pixel[1] = pixels[1];
        context->first_pixel[2] = pixels[2];
    }

    return SIXEL_OK;
}

static SIXEL_TEST_UNUSED SIXELSTATUS
librsvg_capture_frame_trampoline(sixel_frame_t *frame, void *data)
{
    librsvg_callback_state_t *state;

    state = (librsvg_callback_state_t *)data;
    if (state == NULL || state->fn == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    return state->fn(frame, state->context);
}

static SIXEL_TEST_UNUSED int
run_librsvg_component_chunk_case(unsigned char const *svg,
                                 size_t svg_size,
                                 unsigned char const *bgcolor,
                                 librsvg_probe_context_t *probe)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_loader_factory_t *factory;
    sixel_loader_component_t *component;
    sixel_chunk_t chunk;
    int require_static;
    int use_palette;
    int reqcolors;
    librsvg_callback_state_t callback_state;

    status = SIXEL_FALSE;
    allocator = NULL;
    factory = NULL;
    component = NULL;
    memset(&chunk, 0, sizeof(chunk));
    require_static = 1;
    use_palette = 0;
    reqcolors = 256;
    callback_state.fn = librsvg_capture_frame;
    callback_state.context = probe;

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        return 1;
    }

    status = loader_factory_get_default(&factory);
    if (SIXEL_FAILED(status)) {
        sixel_allocator_unref(allocator);
        return 1;
    }

    status = loader_factory_create_component(factory,
                                             "librsvg",
                                             allocator,
                                             &component);
    if (SIXEL_FAILED(status)) {
        loader_factory_unref(factory);
        sixel_allocator_unref(allocator);
        return 1;
    }
    loader_factory_unref(factory);

    chunk.buffer = (unsigned char *)svg;
    chunk.size = svg_size;
    chunk.max_size = svg_size;
    chunk.source_path = NULL;
    chunk.allocator = allocator;

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

    status = sixel_loader_component_setopt(component,
                                           SIXEL_LOADER_OPTION_BGCOLOR,
                                           bgcolor);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = sixel_loader_component_load(component,
                                         &chunk,
                                         librsvg_capture_frame_trampoline,
                                         &callback_state);

cleanup:
    sixel_loader_component_unref(component);
    sixel_allocator_unref(allocator);

    return SIXEL_SUCCEEDED(status) ? 0 : 1;
}

#endif /* LIBRSVG_TEST_COMMON_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
