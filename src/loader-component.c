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
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "loader-component.h"

void
sixel_loader_component_ref(sixel_loader_component_t *component)
{
    if (component == NULL || component->vtbl == NULL ||
        component->vtbl->ref == NULL) {
        return;
    }

    component->vtbl->ref(component);
}

void
sixel_loader_component_unref(sixel_loader_component_t *component)
{
    if (component == NULL || component->vtbl == NULL ||
        component->vtbl->unref == NULL) {
        return;
    }

    component->vtbl->unref(component);
}

SIXELSTATUS
sixel_loader_component_setopt(sixel_loader_component_t *component,
                              int option,
                              void const *value)
{
    if (component == NULL || component->vtbl == NULL ||
        component->vtbl->setopt == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    return component->vtbl->setopt(component, option, value);
}

SIXELSTATUS
sixel_loader_component_load(sixel_loader_component_t *component,
                            sixel_chunk_t const *chunk,
                            sixel_load_image_function fn_load,
                            void *context)
{
    if (component == NULL || component->vtbl == NULL ||
        component->vtbl->load == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    return component->vtbl->load(component, chunk, fn_load, context);
}

char const *
sixel_loader_component_get_name(sixel_loader_component_t const *component)
{
    if (component == NULL || component->vtbl == NULL ||
        component->vtbl->name == NULL) {
        return NULL;
    }

    return component->vtbl->name(component);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
