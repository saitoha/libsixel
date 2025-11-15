/*
 * SPDX-License-Identifier: MIT
 *
 * Declarations for palette safe-tone snapping utilities.  Keeping the helpers
 * in a dedicated header allows quantizers to request reversible palette support
 * without pulling in the broader merge infrastructure.
 */

#ifndef LIBSIXEL_PALETTE_COMMON_SNAP_H
#define LIBSIXEL_PALETTE_COMMON_SNAP_H

#ifdef __cplusplus
extern "C" {
#endif

void
sixel_palette_reversible_palette(unsigned char *palette,
                                 unsigned int colors,
                                 unsigned int depth);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_PALETTE_COMMON_SNAP_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
