/*
 * SPDX-License-Identifier: MIT
 *
 * Safe-tone snapping helpers for palettes.  The implementation mirrors the
 * reversible tone logic historically embedded inside palette.c but now lives in
 * a dedicated module so other translation units can reuse it without dragging
 * in unrelated merge infrastructure.
 */

#include "config.h"

#include <stddef.h>

#include "lut.h"
#include "palette-common-snap.h"

void
sixel_palette_reversible_palette(unsigned char *palette,
                                 unsigned int colors,
                                 unsigned int depth)
{
    unsigned int index;
    unsigned int plane;
    unsigned int count;
    unsigned int value;
    size_t offset;

    index = 0U;
    plane = 0U;
    count = colors;
    value = 0U;
    offset = 0U;
    if (palette == NULL) {
        return;
    }
    for (index = 0U; index < count; ++index) {
        for (plane = 0U; plane < depth; ++plane) {
            offset = (size_t)index * (size_t)depth + (size_t)plane;
            value = palette[offset];
            palette[offset] = sixel_palette_reversible_value(value);
        }
    }
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
