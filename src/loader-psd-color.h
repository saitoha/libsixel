/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_LOADER_PSD_COLOR_H
#define LIBSIXEL_LOADER_PSD_COLOR_H

#include "frompsd-color.h"

/* The adapter owns color interpretation. The PSD parser only exposes native
 * planes and immutable metadata, so a later normalization stage can reuse it.
 * SIXEL_FALSE means no supported colorant definition; outputs stay untouched.
 */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_psd_render_color_data(sixel_chunk_t const *chunk,
                            sixel_builtin_psd_info_t const *info,
                            unsigned char const *bgcolor,
                            unsigned char **pixels,
                            unsigned char **mask, size_t *mask_size);

#endif
