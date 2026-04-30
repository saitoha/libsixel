/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef LIBSIXEL_FRAME_FACTORY_H
#define LIBSIXEL_FRAME_FACTORY_H

#include "components.h"
#include "factory.h"
#include "frame.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Internal frame construction goes through the class factory so src/ callers
 * depend on the image/frame contract rather than the concrete constructor.
 * Public boundary code should keep using sixel_frame_new().
 */
static inline SIXELSTATUS
sixel_frame_create_interface_from_factory(
    sixel_allocator_t *allocator,
    sixel_frame_interface_t **frame_out)
{
    SIXELSTATUS status;
    sixel_factory_t *factory;
    void *object;
    void *service;

    status = SIXEL_FALSE;
    factory = NULL;
    object = NULL;
    service = NULL;

    if (frame_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *frame_out = NULL;
    status = sixel_components_getservice("services/factory", &service);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    factory = (sixel_factory_t *)service;
    if (factory == NULL || factory->vtbl == NULL ||
        factory->vtbl->create == NULL || factory->vtbl->unref == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = factory->vtbl->create(factory,
                                   "image/frame",
                                   allocator,
                                   &object);
    factory->vtbl->unref(factory);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (object == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    *frame_out = (sixel_frame_interface_t *)object;
    return SIXEL_OK;
}

static inline SIXELSTATUS
sixel_frame_create_from_factory(sixel_frame_t **frame_out,
                                sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    sixel_frame_interface_t *frame_if;

    status = SIXEL_FALSE;
    frame_if = NULL;

    if (frame_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *frame_out = NULL;

    status = sixel_frame_create_interface_from_factory(allocator, &frame_if);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    *frame_out = (sixel_frame_t *)frame_if;
    return SIXEL_OK;
}

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_FRAME_FACTORY_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
