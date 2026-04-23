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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if HAVE_STRING_H
#include <string.h>
#endif

#include "components.h"
#include "factory.h"

/*
 * IDL (internal contract)
 *
 * interface IComponents {
 *   getservice(name, out service);
 * }
 *
 * Ownership/lifetime:
 * - Returned services are owned by the caller according to each service
 *   contract (factory currently behaves as a singleton no-op refcount).
 */

SIXELSTATUS
sixel_components_getservice(char const *name,
                            void **service)
{
    sixel_factory_t *factory;

    factory = NULL;
    if (service != NULL) {
        *service = NULL;
    }

    if (name == NULL || service == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (strcmp(name, "services/factory") == 0) {
        if (SIXEL_FAILED(sixel_factory_get_default(&factory))) {
            return SIXEL_FALSE;
        }
        *service = factory;
        return SIXEL_OK;
    }

    sixel_helper_set_additional_message(
        "sixel_components_getservice: unknown service name.");
    return SIXEL_BAD_ARGUMENT;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
