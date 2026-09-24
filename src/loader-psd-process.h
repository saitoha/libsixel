/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_LOADER_PSD_PROCESS_H
#define LIBSIXEL_LOADER_PSD_PROCESS_H

#include "frompsd.h"

/* Interpret an opaque process composite and its spot planes after native
 * decoding. Profile bytes describe the process components only. Ordinary
 * documents without spots return SIXEL_FALSE and keep their fast path. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_psd_render_process_data(sixel_chunk_t const *chunk,
                              sixel_builtin_psd_info_t const *info,
                              int icc_status,
                              unsigned char const *icc, size_t icc_size,
                              unsigned char **pixels);

#endif
