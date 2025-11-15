/*
 * SPDX-License-Identifier: MIT
 *
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

#endif /* LIBSIXEL_PALETTE_COMMON_MERGE_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
