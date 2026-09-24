/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_FROMPSD_COLOR_H
#define LIBSIXEL_FROMPSD_COLOR_H

#include <stdint.h>
#include "frompsd.h"

#define SIXEL_PSD_COLOR_CHANNELS 56u

/* Borrowed file metadata only: no CMS profiles or renderer state. */
typedef struct sixel_psd_ink {
    unsigned int space;
    unsigned int component[4];
    unsigned int opacity;
    unsigned int kind;
} sixel_psd_ink_t;

typedef struct sixel_psd_color_data {
    unsigned char const *duotone_lab;
    unsigned int ink_count;
    sixel_psd_ink_t ink[SIXEL_PSD_COLOR_CHANNELS];
    unsigned int identifier_present;
    unsigned int identifier_count;
    uint32_t identifier[SIXEL_PSD_COLOR_CHANNELS];
    unsigned int alternate_count;
    uint32_t alternate_id[SIXEL_PSD_COLOR_CHANNELS];
    sixel_psd_ink_t alternate[SIXEL_PSD_COLOR_CHANNELS];
} sixel_psd_color_data_t;

SIXEL_INTERNAL_API SIXELSTATUS
sixel_psd_read_color_data(sixel_chunk_t const *chunk,
                          sixel_builtin_psd_info_t const *info,
                          sixel_psd_color_data_t *data);

/* Native big-endian planar samples; the caller owns the decoded buffer. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_psd_read_color_planes(sixel_chunk_t const *chunk,
                            sixel_builtin_psd_info_t const *info,
                            unsigned char **planes, size_t *plane_bytes);

/* Read explicit merged-transparency flags without interpreting pixels. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_psd_merged_alpha(sixel_chunk_t const *chunk,
                       sixel_builtin_psd_info_t const *info, int *present);

/* Resolve extra-plane roles and align identifiers with display records.
 * UINT_MAX denotes no merged transparency. No pixels or CMS are involved. */
SIXEL_INTERNAL_API int
sixel_psd_process_layout(sixel_psd_color_data_t *data,
                         unsigned int channels, unsigned int process,
                         int merged, unsigned int *alpha_plane,
                         unsigned int *display_plane);

#endif
