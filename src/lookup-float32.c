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

/*
 * IDL (internal contract)
 *
 * interface ILookupFloat32BackendLifecycle {
 *   init(allocator);
 *   clear();
 *   finalize();
 * }
 *
 * Ownership/lifetime:
 * - init() allocates optional helper handles used by policy objects.
 * - clear() releases cached lookup structures while keeping the handle alive.
 * - finalize() releases both caches and the owned helper handle itself.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "lookup-float32.h"

#define SIXEL_LOOKUP_EYTZINGER_WINDOW 6

static void
sixel_lookup_float32_1d_eytzinger_release(sixel_lookup_float32_t *lut)
{
    if (lut == NULL) {
        return;
    }

    free(lut->eytz.keys);
    free(lut->eytz.palette_index);
    free(lut->eytz.rank);
    free(lut->eytz.sorted_palette_index);
    free(lut->eytz.sorted_keys);
    lut->eytz.keys = NULL;
    lut->eytz.palette_index = NULL;
    lut->eytz.rank = NULL;
    lut->eytz.sorted_palette_index = NULL;
    lut->eytz.sorted_keys = NULL;
    lut->eytz.ready = 0;
    lut->eytz.count = 0;
}

static void
sixel_lookup_float32_rbc_clear(sixel_lookup_float32_t *lut)
{
    if (lut == NULL) {
        return;
    }

    free(lut->rbc.pivots);
    free(lut->rbc.radius);
    free(lut->rbc.member_offset);
    free(lut->rbc.member_index);
    free(lut->rbc.mean);
    free(lut->rbc.inv_cov);
    lut->rbc.pivots = NULL;
    lut->rbc.radius = NULL;
    lut->rbc.member_offset = NULL;
    lut->rbc.member_index = NULL;
    lut->rbc.mean = NULL;
    lut->rbc.inv_cov = NULL;
    lut->rbc.pivot_count = 0;
    lut->rbc.ready = 0;
}

static void
sixel_lookup_float32_release_palette(sixel_lookup_float32_t *lut)
{
    if (lut == NULL) {
        return;
    }

    if (lut->palette != NULL) {
        sixel_allocator_free(lut->allocator, lut->palette);
        lut->palette = NULL;
    }
}

static void
sixel_lookup_float32_release_kdtree(sixel_lookup_float32_t *lut)
{
    if (lut == NULL) {
        return;
    }

    free(lut->kdnodes);
    lut->kdnodes = NULL;
    lut->kdnodes_count = 0;
    lut->kdtree_root = -1;
}

void
sixel_lookup_float32_init(sixel_lookup_float32_t *lut,
                          sixel_allocator_t *allocator)
{
    if (lut == NULL) {
        return;
    }

    memset(lut, 0, sizeof(sixel_lookup_float32_t));
    lut->policy = SIXEL_LUT_POLICY_6BIT;
    lut->depth = 0;
    lut->ncolors = 0;
    lut->complexion = 1;
    lut->weights[0] = 1.0f;
    lut->weights[1] = 1.0f;
    lut->weights[2] = 1.0f;
    lut->palette = NULL;
    lut->kdnodes = NULL;
    lut->kdtree_root = -1;
    lut->kdnodes_count = 0;
    lut->allocator = allocator;
    lut->fhedt = NULL;
    lut->fhedt_ready = 0;
    lut->vptree = NULL;
    lut->vptree_ready = 0;
    lut->eytz.count = 0;
    lut->eytz.ready = 0;
    lut->eytz.keys = NULL;
    lut->eytz.palette_index = NULL;
    lut->eytz.rank = NULL;
    lut->eytz.sorted_palette_index = NULL;
    lut->eytz.sorted_keys = NULL;
    lut->eytz.window = SIXEL_LOOKUP_EYTZINGER_WINDOW;
    lut->eytz.weights[0] = 1.0f;
    lut->eytz.weights[1] = 1.0f;
    lut->eytz.weights[2] = 1.0f;
    lut->rbc.pivot_count = 0;
    lut->rbc.pivots = NULL;
    lut->rbc.radius = NULL;
    lut->rbc.member_offset = NULL;
    lut->rbc.member_index = NULL;
    lut->rbc.mean = NULL;
    lut->rbc.inv_cov = NULL;
    lut->rbc.ready = 0;
    (void)sixel_lookup_fhedt_float32_create(allocator, &lut->fhedt);
    (void)sixel_lookup_vptree_float32_create(allocator, &lut->vptree);
}

void
sixel_lookup_float32_clear(sixel_lookup_float32_t *lut)
{
    if (lut == NULL) {
        return;
    }

    sixel_lookup_float32_release_palette(lut);
    sixel_lookup_float32_release_kdtree(lut);
    sixel_lookup_float32_rbc_clear(lut);
    sixel_lookup_float32_1d_eytzinger_release(lut);
    if (lut->fhedt != NULL) {
        sixel_lookup_fhedt_float32_unref(lut->fhedt);
        lut->fhedt = NULL;
    }
    lut->fhedt_ready = 0;
    if (lut->vptree != NULL) {
        sixel_lookup_vptree_float32_unref(lut->vptree);
        lut->vptree = NULL;
    }
    lut->vptree_ready = 0;
    lut->ncolors = 0;
    lut->depth = 0;
}

void
sixel_lookup_float32_finalize(sixel_lookup_float32_t *lut)
{
    if (lut == NULL) {
        return;
    }

    sixel_lookup_float32_clear(lut);
    lut->allocator = NULL;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
