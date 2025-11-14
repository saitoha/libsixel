/*
 * SPDX-License-Identifier: MIT
 *
 * Internal palette helpers shared between the median-cut and k-means
 * implementations.  The table below explains the exported groups and the main
 * consumers so future refactors can trace the cross-module contract quickly.
 *
 *   Group              Purpose                          Consumers
 *   ----------------------------------------------------------------
 *   Tuple histogram    bucket sizing and histogram     palette-heckbert.c,
 *                                                      lut.c
 *   Reversible tones   snap values to SIXEL safe grid  palette.c,
 *                                                      palette-kmeans.c
 *   Final merge        share Ward / HK-means post      palette.c,
 *                      processing helpers              palette-kmeans.c,
 *                                                      palette-heckbert.c
 *   Environment        lazily parse tuning variables   palette.c,
 *                                                      palette-kmeans.c,
 *                                                      palette-heckbert.c
 */

#ifndef LIBSIXEL_PALETTE_INTERNAL_H
#define LIBSIXEL_PALETTE_INTERNAL_H

#include <sixel.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct histogram_control {
    unsigned int channel_shift;
    unsigned int channel_bits;
    unsigned int channel_mask;
    int reversible_rounding;
};

typedef unsigned long sample;
typedef sample *tuple;

struct tupleint {
    unsigned int value;
    sample tuple[1];
};

typedef struct tupleint **tupletable;

typedef struct {
    unsigned int size;
    tupletable table;
} tupletable2;

/* -- Tuple and histogram cooperation ------------------------------------- */

size_t
histogram_dense_size(unsigned int depth,
                     struct histogram_control const *control);

struct histogram_control
histogram_control_make_for_policy(unsigned int depth, int lut_policy);

unsigned int
histogram_pack_color(unsigned char const *data,
                     unsigned int depth,
                     struct histogram_control const *control);

SIXELSTATUS
sixel_lut_build_histogram(unsigned char const *data,
                          unsigned int length,
                          unsigned long depth,
                          int quality_mode,
                          int use_reversible,
                          int policy,
                          tupletable2 *colorfreqtable,
                          sixel_allocator_t *allocator);

/* -- Reversible palette helpers ----------------------------------------- */

void
sixel_palette_reversible_tuple(sample *tuple,
                               unsigned int depth);

void
sixel_palette_reversible_palette(unsigned char *palette,
                                 unsigned int colors,
                                 unsigned int depth);

/* -- Environment accessors ----------------------------------------------- */

void
sixel_final_merge_load_env(void);

unsigned int
sixel_final_merge_lloyd_iterations(int merge_mode);

unsigned int
sixel_final_merge_target(unsigned int reqcolors, int final_merge_mode);

unsigned int
sixel_palette_kmeans_iter_max(void);

double
sixel_palette_kmeans_threshold(void);

/* -- Final merge orchestration ------------------------------------------- */

int
sixel_resolve_final_merge_mode(int final_merge_mode);

void
sixel_final_merge_lloyd_histogram(tupletable2 const colorfreqtable,
                                  unsigned int depth,
                                  unsigned int cluster_count,
                                  unsigned long *cluster_weight,
                                  unsigned long *cluster_sums,
                                  unsigned int iterations);

int
sixel_palette_apply_merge(unsigned long *weights,
                          unsigned long *sums,
                          unsigned int depth,
                          int cluster_count,
                          int target,
                          int final_merge_mode,
                          int use_reversible,
                          sixel_allocator_t *allocator);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_PALETTE_INTERNAL_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
