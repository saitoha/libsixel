/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * Shared helpers for the palette final-merge pipeline.  The orchestrator and
 * both quantizer implementations rely on this module to decode runtime
 * configuration, execute Ward/HK-means refinement, and expose the oversplit
 * heuristics.  Keeping the declarations here prevents palette.h from growing
 * k-means/Heckbert specific details while still making the services available
 * across the translation units.
 */

#ifndef LIBSIXEL_PALETTE_COMMON_MERGE_H
#define LIBSIXEL_PALETTE_COMMON_MERGE_H

#include "palette.h"

#ifdef __cplusplus
extern "C" {
#endif

void
sixel_final_merge_load_env(void);

unsigned int
sixel_final_merge_lloyd_iterations(int merge_mode);

unsigned int
sixel_final_merge_target(unsigned int reqcolors, int final_merge_mode);

int
sixel_resolve_final_merge_mode(int final_merge_mode);

int
sixel_palette_apply_merge(unsigned long *weights,
                          double *sums,
                          unsigned int depth,
                          int cluster_count,
                          int target,
                          int final_merge_mode,
                          int use_reversible,
                          sixel_allocator_t *allocator);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_PALETTE_COMMON_MERGE_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
