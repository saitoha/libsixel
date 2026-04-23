/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef LIBSIXEL_FACTORY_H
#define LIBSIXEL_FACTORY_H

#include <sixel.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * IDL (internal contract)
 *
 * interface IFactory {
 *   ref();
 *   unref();
 *   create(class_name, out object);
 * }
 *
 * Ownership/lifetime:
 * - The factory is a process singleton service.
 * - ref/unref are part of the interface for consistency and are no-op.
 *
 * Creation path:
 * - services/factory -> IFactory
 * - IFactory.create("lookup/...", &policy)
 */

typedef struct sixel_factory_interface sixel_factory_t;

typedef struct sixel_factory_vtbl {
    void (*ref)(sixel_factory_t *factory);
    void (*unref)(sixel_factory_t *factory);
    SIXELSTATUS (*create)(sixel_factory_t *factory,
                          char const *class_name,
                          void **object);
} sixel_factory_vtbl_t;

struct sixel_factory_interface {
    sixel_factory_vtbl_t const *vtbl;
};

SIXEL_INTERNAL_API SIXELSTATUS
sixel_factory_get_default(sixel_factory_t **factory);

SIXEL_INTERNAL_API void
sixel_factory_ref(sixel_factory_t *factory);

SIXEL_INTERNAL_API void
sixel_factory_unref(sixel_factory_t *factory);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_factory_create(sixel_factory_t *factory,
                     char const *class_name,
                     void **object);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_FACTORY_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
