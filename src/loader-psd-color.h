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

/* Shared interpretation helpers; saved masks have no display-ink value. */
SIXEL_INTERNAL_API int
sixel_psd_prepare_spots(sixel_psd_color_data_t *data, double palette[][3]);

SIXEL_INTERNAL_API int
sixel_psd_color_sample(unsigned char const *p, unsigned int depth,
                        int bounded, double *sample);

/* Convert normalized L/100, a/128, b/128 using PSD's D50 white point.
 * This is intrinsic Lab interpretation, independent of embedded ICC policy. */
SIXEL_INTERNAL_API int
sixel_psd_lab_to_linear(float const *source, float *output, size_t count);

#endif
