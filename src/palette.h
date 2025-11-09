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
    unsigned short *cachetable;     /* quantization cache table */
    size_t cachetable_size;         /* quantization cache length */
};

SIXELAPI SIXELSTATUS
sixel_palette_new(sixel_palette_t **palette,
                  sixel_allocator_t *allocator);

SIXELAPI sixel_palette_t *
sixel_palette_ref(sixel_palette_t *palette);

SIXELAPI void
sixel_palette_unref(sixel_palette_t *palette);

SIXELAPI SIXELSTATUS
sixel_palette_generate(sixel_palette_t *palette,
                       unsigned char const *data,
                       unsigned int length,
                       int pixelformat,
                       sixel_allocator_t *allocator);

SIXELAPI SIXELSTATUS
sixel_palette_resize(sixel_palette_t *palette,
                     unsigned int colors,
                     int depth,
                     sixel_allocator_t *allocator);

SIXELAPI SIXELSTATUS
sixel_palette_set_entries(sixel_palette_t *palette,
                          unsigned char const *entries,
                          unsigned int colors,
                          int depth,
                          sixel_allocator_t *allocator);

SIXELAPI unsigned char *
sixel_palette_get_entries(sixel_palette_t *palette);

SIXELAPI unsigned int
sixel_palette_get_entry_count(sixel_palette_t const *palette);

SIXELAPI int
sixel_palette_get_depth(sixel_palette_t const *palette);

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
