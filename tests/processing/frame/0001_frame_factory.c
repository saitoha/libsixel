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
    if (factory != NULL) {
        factory->vtbl->unref(factory);
    }
    if (frame != NULL) {
        frame->vtbl->unref(frame);
    }
    if (allocator != NULL) {
        sixel_allocator_unref(allocator);
    }
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
