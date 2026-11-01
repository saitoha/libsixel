/*
 * SPDX-License-Identifier: MIT
 *
 * Unit test for frame creation through the component factory.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>
#include <6cells.h>

#include "src/factory.h"
#include "src/frame.h"

/*
 * IDL usage in this unit
 *
 * IComponents.getservice("services/factory", &factory)
 * IFactory.create("image/frame", allocator, &frame)
 * IFrame.get_pixels(view)
 * IFrame.unref()
 */

static unsigned char frame_borrowed_palette[] = {
    0x00u, 0x00u, 0x00u,
    0xffu, 0xffu, 0xffu
};
static unsigned char frame_borrowed_indexed_pixels[] = {
    0x00u, 0x01u
};
static unsigned char frame_borrowed_rgb_pixels[] = {
    0x10u, 0x20u, 0x30u,
    0x40u, 0x50u, 0x60u
};
static unsigned char *frame_owned_reinit_palette;
static unsigned char *frame_owned_reinit_pixels;
static int frame_borrowed_bad_free_count;
static int frame_owned_reinit_free_count;

static void *
frame_borrowed_malloc(size_t size)
{
    return malloc(size);
}

static void *
frame_borrowed_calloc(size_t count, size_t size)
{
    return calloc(count, size);
}

static void *
frame_borrowed_realloc(void *ptr, size_t size)
{
    return realloc(ptr, size);
}

static void
frame_borrowed_free(void *ptr)
{
    if (ptr == frame_borrowed_palette ||
        ptr == frame_borrowed_indexed_pixels ||
        ptr == frame_borrowed_rgb_pixels) {
        ++frame_borrowed_bad_free_count;
        return;
    }
    /*
     * Count owned-storage releases, then free the backing memory at the test
     * boundary.  That keeps an early release observable without risking a
     * double free during failure cleanup.
     */
    if (ptr != NULL && (ptr == frame_owned_reinit_palette ||
                        ptr == frame_owned_reinit_pixels)) {
        ++frame_owned_reinit_free_count;
        return;
    }

    free(ptr);
}

static void
frame_owned_reinit_release_storage(void)
{
    free(frame_owned_reinit_palette);
    free(frame_owned_reinit_pixels);
    frame_owned_reinit_palette = NULL;
    frame_owned_reinit_pixels = NULL;
}

static int
test_frame_factory_creates_default_frame(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_factory_t *factory;
    sixel_frame_interface_t *frame;
    sixel_frame_pixels_view_t view;
    void *object;
    void *service;

    status = SIXEL_FALSE;
    allocator = NULL;
    factory = NULL;
    frame = NULL;
    object = NULL;
    service = NULL;
    memset(&view, 0, sizeof(view));

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_components_getservice("services/factory", &service);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    factory = (sixel_factory_t *)service;
    if (factory == NULL || factory->vtbl == NULL ||
        factory->vtbl->create == NULL ||
        factory->vtbl->unref == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    status = factory->vtbl->create(factory, "image/frame",
                                   allocator, &object);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    frame = (sixel_frame_interface_t *)object;
    object = NULL;
    if (frame == NULL || frame->vtbl == NULL ||
        frame->vtbl->get_pixels == NULL ||
        frame->vtbl->unref == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    status = frame->vtbl->get_pixels(frame, &view);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (view.width != 0 || view.height != 0
            || view.pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    status = SIXEL_OK;

end:
    if (factory != NULL && factory->vtbl != NULL &&
        factory->vtbl->unref != NULL) {
        factory->vtbl->unref(factory);
    }
    if (frame != NULL && frame->vtbl != NULL && frame->vtbl->unref != NULL) {
        frame->vtbl->unref(frame);
    }
    if (allocator != NULL) {
        sixel_allocator_unref(allocator);
    }
    return SIXEL_SUCCEEDED(status);
}

static int
test_frame_borrowed_storage_contract(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_frame_t *frame;

    status = SIXEL_FALSE;
    allocator = NULL;
    frame = NULL;

    frame_borrowed_bad_free_count = 0;
    status = sixel_allocator_new(&allocator,
                                 frame_borrowed_malloc,
                                 frame_borrowed_calloc,
                                 frame_borrowed_realloc,
                                 frame_borrowed_free);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_frame_new(&frame, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_frame_init_borrowed(frame,
                                       frame_borrowed_indexed_pixels,
                                       2,
                                       1,
                                       SIXEL_PIXELFORMAT_PAL8,
                                       frame_borrowed_palette,
                                       2);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    sixel_frame_unref(frame);
    frame = NULL;
    if (frame_borrowed_bad_free_count != 0) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    status = sixel_frame_new(&frame, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_frame_init_borrowed(frame,
                                       frame_borrowed_rgb_pixels,
                                       2,
                                       1,
                                       SIXEL_PIXELFORMAT_RGB888,
                                       NULL,
                                       (-1));
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_frame_set_pixelformat(frame, SIXEL_PIXELFORMAT_RGBFLOAT32);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (sixel_frame_get_pixelformat(frame) != SIXEL_PIXELFORMAT_RGBFLOAT32) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    sixel_frame_unref(frame);
    frame = NULL;
    if (frame_borrowed_bad_free_count != 0) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    status = SIXEL_OK;

end:
    if (frame != NULL) {
        sixel_frame_unref(frame);
    }
    if (allocator != NULL) {
        sixel_allocator_unref(allocator);
    }
    return SIXEL_SUCCEEDED(status);
}

static int
test_frame_reinit_keeps_same_owned_storage(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_frame_t *frame;

    status = SIXEL_FALSE;
    allocator = NULL;
    frame = NULL;

    frame_owned_reinit_release_storage();
    frame_owned_reinit_free_count = 0;
    status = sixel_allocator_new(&allocator,
                                 frame_borrowed_malloc,
                                 frame_borrowed_calloc,
                                 frame_borrowed_realloc,
                                 frame_borrowed_free);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    frame_owned_reinit_pixels = (unsigned char *)
        sixel_allocator_malloc(allocator, 2u);
    if (frame_owned_reinit_pixels == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    frame_owned_reinit_palette = (unsigned char *)
        sixel_allocator_malloc(allocator, 6u);
    if (frame_owned_reinit_palette == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    frame_owned_reinit_pixels[0] = 0x00u;
    frame_owned_reinit_pixels[1] = 0x01u;
    frame_owned_reinit_palette[0] = 0x00u;
    frame_owned_reinit_palette[1] = 0x00u;
    frame_owned_reinit_palette[2] = 0x00u;
    frame_owned_reinit_palette[3] = 0xffu;
    frame_owned_reinit_palette[4] = 0xffu;
    frame_owned_reinit_palette[5] = 0xffu;

    status = sixel_frame_new(&frame, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_frame_init(frame,
                              frame_owned_reinit_pixels,
                              2,
                              1,
                              SIXEL_PIXELFORMAT_PAL8,
                              frame_owned_reinit_palette,
                              2);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_frame_init(frame,
                              frame_owned_reinit_pixels,
                              2,
                              1,
                              SIXEL_PIXELFORMAT_PAL8,
                              frame_owned_reinit_palette,
                              2);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (frame_owned_reinit_free_count != 0) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    sixel_frame_unref(frame);
    frame = NULL;
    if (frame_owned_reinit_free_count != 2) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    status = SIXEL_OK;

end:
    if (frame != NULL) {
        sixel_frame_unref(frame);
    }
    if (allocator != NULL) {
        sixel_allocator_unref(allocator);
    }
    frame_owned_reinit_release_storage();
    return SIXEL_SUCCEEDED(status);
}

int
test_frame_0001_frame_factory(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (!test_frame_factory_creates_default_frame()) {
        fprintf(stderr, "frame factory create failed\n");
        return EXIT_FAILURE;
    }
    if (!test_frame_borrowed_storage_contract()) {
        fprintf(stderr, "frame borrowed storage contract failed\n");
        return EXIT_FAILURE;
    }
    if (!test_frame_reinit_keeps_same_owned_storage()) {
        fprintf(stderr, "frame owned storage reinit failed\n");
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
