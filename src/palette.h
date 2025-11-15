/*
 * Copyright (c) 2024 libsixel developers
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

#include <sixel.h>

#ifdef __cplusplus
extern "C" {
#endif

struct sixel_lut;

/*
 * Internal statistics accumulator used while clustering provisional
 * palette entries.  The tuple stores RGB centroids along with the
 * aggregate weight gathered during histogram analysis.
 */
typedef struct sixel_final_merge_cluster {
    double r;
    double g;
    double b;
    double count;
} sixel_final_merge_cluster_t;

/*
 * Control how the final merge phase consolidates provisional clusters
 * into the target palette.  The hkmeans mode performs weighted
 * refinement after the hierarchical ward pass.
 */
typedef enum sixel_final_merge_dispatch {
    SIXEL_FINAL_MERGE_DISPATCH_NONE = 0,
    SIXEL_FINAL_MERGE_DISPATCH_WARD,
    SIXEL_FINAL_MERGE_DISPATCH_HKMEANS,
} sixel_final_merge_dispatch_t;

/*
 * sixel_palette_t centralizes palette state shared between quantization and
 * dithering phases.  The structure now stores both configuration knobs and
 * generated results so callers can keep a single object throughout the image
 * conversion pipeline.
 */
struct sixel_palette {
    unsigned int ref;               /* reference counter */
    sixel_allocator_t *allocator;   /* allocator associated with palette */
    unsigned char *entries;         /* palette entries, RGB triplets */
    size_t entries_size;            /* allocated length for entries */
    unsigned int entry_count;       /* number of active palette entries */
    unsigned int requested_colors;  /* requested palette size */
    unsigned int original_colors;   /* original color count before quant */
    int depth;                      /* sample depth for generated palette */
    int method_for_largest;         /* histogram split heuristic */
    int method_for_rep;             /* representative color selector */
    int quality_mode;               /* histogram sampling quality */
    int force_palette;              /* keep palette size constant */
    int use_reversible;             /* reversible tone enforcement */
    int quantize_model;             /* palette solver selector */
    int final_merge_mode;           /* palette merge strategy */
    int complexion;                 /* complexion correction score */
    int lut_policy;                 /* histogram LUT selection */
    int sixel_reversible;           /* reversible tone flag proxy */
    int final_merge;                /* final merge flag proxy */
    struct sixel_lut *lut;          /* reusable lookup table */
};

void
sixel_palette_set_lut_policy(int lut_policy);

void
sixel_palette_set_method_for_largest(int method);

SIXELSTATUS
sixel_palette_make_palette(unsigned char **result,
                           unsigned char const *data,
                           unsigned int length,
                           int pixelformat,
                           unsigned int reqcolors,
                           unsigned int *ncolors,
                           unsigned int *origcolors,
                           int methodForLargest,
                           int methodForRep,
                           int qualityMode,
                           int force_palette,
                           int use_reversible,
                           int quantize_model,
                           int final_merge_mode,
                           sixel_allocator_t *allocator);

void
sixel_palette_free_palette(unsigned char *data,
                           sixel_allocator_t *allocator);

#if HAVE_TESTS
SIXELAPI int
sixel_palette_tests_main(void);
#endif

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
