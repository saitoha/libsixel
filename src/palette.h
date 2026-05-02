/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers
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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef LIBSIXEL_PALETTE_H
#define LIBSIXEL_PALETTE_H

#include <6cells.h>

#ifdef __cplusplus
extern "C" {
#endif

struct sixel_final_merge_cluster;
typedef struct sixel_final_merge_cluster sixel_final_merge_cluster_t;

/*
 * Control how the final merge phase consolidates provisional clusters
 * into the target palette.  The Ward path merges clusters using
 * linkage costs while the remaining options keep the provisional
 * palette untouched.
 */
typedef enum sixel_final_merge_dispatch {
    SIXEL_FINAL_MERGE_DISPATCH_NONE = 0,
    SIXEL_FINAL_MERGE_DISPATCH_WARD,
} sixel_final_merge_dispatch_t;

/*
 * Telemetry describing how long each palette stage spent inside the
 * quantizer.  The orchestrator uses the aggregate to emit a single-line
 * timeline summary so operators can understand where palette/build time
 * was consumed without expanding multiple rows in the visualiser.
 */
typedef struct sixel_palette_telemetry {
    double init_ms;
    double iterate_ms;
    double merge_ms;
    double export_ms;
    unsigned int iterate_count;
    unsigned int merge_iterate_count;
    int merge_mode;
} sixel_palette_telemetry_t;

/* @classid quant/palette */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_factory_new(sixel_allocator_t *allocator, void **object);


#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_PALETTE_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
