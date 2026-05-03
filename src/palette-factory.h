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

#ifndef LIBSIXEL_PALETTE_FACTORY_H
#define LIBSIXEL_PALETTE_FACTORY_H

#include "factory.h"
#include "palette.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__has_attribute)
# if __has_attribute(unused)
#  define SIXEL_PALETTE_FACTORY_UNUSED __attribute__((unused))
# endif
#endif

#ifndef SIXEL_PALETTE_FACTORY_UNUSED
# if defined(__GNUC__) && !defined(__PCC__)
#  define SIXEL_PALETTE_FACTORY_UNUSED __attribute__((unused))
# else
#  define SIXEL_PALETTE_FACTORY_UNUSED
# endif
#endif

/*
 * Internal palette construction goes through the class factory so callers
 * depend on the quant/palette contract rather than concrete storage.
 */
static inline SIXELSTATUS SIXEL_PALETTE_FACTORY_UNUSED
sixel_palette_create_from_factory(sixel_palette_t **palette_out,
                                  sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    sixel_factory_t *factory;
    void *object;
    void *service;

    status = SIXEL_FALSE;
    factory = NULL;
    object = NULL;
    service = NULL;

    if (palette_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *palette_out = NULL;

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
                                   "quant/palette",
                                   allocator,
                                   &object);
    factory->vtbl->unref(factory);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (object == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    *palette_out = (sixel_palette_t *)object;
    return SIXEL_OK;
}

#undef SIXEL_PALETTE_FACTORY_UNUSED

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_PALETTE_FACTORY_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
