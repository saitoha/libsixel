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

#ifndef LIBSIXEL_FILTER_VPTREE_H
#define LIBSIXEL_FILTER_VPTREE_H

#include <sixel.h>

#include "filter-lookup.h"
#include "filter.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * IDL (internal contract)
 *
 * interface IVptreeFilter {
 *   init(filter, config);
 * }
 *
 * Ownership/lifetime:
 * - apply() may publish result_out->policy with owned=1.
 * - Caller unrefs the policy only when owned is non-zero.
 *
 * Creation path:
 * - The filter delegates policy construction to ILookupFilter.build().
 */

/*
 * Configuration for the VP-tree filter. The filter builds a VP-tree lookup
 * policy object from the merged palette and propagates the result to the
 * caller. The lookup configuration must request `SIXEL_LUT_POLICY_VPTREE`.
 */
typedef struct sixel_filter_vptree_config {
    sixel_filter_lookup_config_t lookup_config;
    sixel_filter_lookup_result_t *result_out;
} sixel_filter_vptree_config_t;

SIXELSTATUS
sixel_filter_vptree_init(sixel_filter_t *filter,
                         const sixel_filter_vptree_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_FILTER_VPTREE_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
