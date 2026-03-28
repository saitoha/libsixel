/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
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
 * FROM, OUT OF, OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef LIBSIXEL_FROMPSD_H
#define LIBSIXEL_FROMPSD_H

#include <stddef.h>

#include <sixel.h>

#include "chunk.h"

typedef enum sixel_builtin_icc_extract_status {
    SIXEL_BUILTIN_ICC_EXTRACT_ABSENT = 0,
    SIXEL_BUILTIN_ICC_EXTRACT_FOUND = 1,
    SIXEL_BUILTIN_ICC_EXTRACT_MALFORMED = -1
} sixel_builtin_icc_extract_status_t;

typedef struct sixel_builtin_psd_info {
    unsigned int version;
    unsigned int channels;
    unsigned int width;
    unsigned int height;
    unsigned int depth;
    unsigned int color_mode;
    unsigned int compression;
    size_t color_mode_data_offset;
    size_t color_mode_data_length;
    size_t image_resources_offset;
    size_t image_resources_length;
    size_t layer_mask_offset;
    size_t layer_mask_length;
    size_t image_data_offset;
} sixel_builtin_psd_info_t;

typedef enum sixel_builtin_psd_decode_mode {
    SIXEL_BUILTIN_PSD_DECODE_MODE_NONE = 0,
    SIXEL_BUILTIN_PSD_DECODE_MODE_GRAY_INDEXED_8BIT = 1,
    SIXEL_BUILTIN_PSD_DECODE_MODE_CMYK_8BIT = 2,
    SIXEL_BUILTIN_PSD_DECODE_MODE_LAB_8BIT = 3,
    SIXEL_BUILTIN_PSD_DECODE_MODE_RGB_8BIT = 4,
    SIXEL_BUILTIN_PSD_DECODE_MODE_BITMAP_1BIT = 5,
    SIXEL_BUILTIN_PSD_DECODE_MODE_GRAY_DUOTONE_16BIT = 6,
    SIXEL_BUILTIN_PSD_DECODE_MODE_RGB_16BIT = 7,
    SIXEL_BUILTIN_PSD_DECODE_MODE_RGB_32BIT = 8,
    SIXEL_BUILTIN_PSD_DECODE_MODE_GRAY_DUOTONE_32BIT = 9,
    SIXEL_BUILTIN_PSD_DECODE_MODE_CMYK_32BIT = 10,
    SIXEL_BUILTIN_PSD_DECODE_MODE_LAB_32BIT = 11,
    SIXEL_BUILTIN_PSD_DECODE_MODE_CMYK_16BIT = 12,
    SIXEL_BUILTIN_PSD_DECODE_MODE_LAB_16BIT = 13
} sixel_builtin_psd_decode_mode_t;

typedef enum sixel_builtin_psd_validation_status {
    SIXEL_BUILTIN_PSD_VALIDATE_MALFORMED = -1,
    SIXEL_BUILTIN_PSD_VALIDATE_OK = 0,
    SIXEL_BUILTIN_PSD_VALIDATE_UNSUPPORTED = 1
} sixel_builtin_psd_validation_status_t;

int
sixel_builtin_extract_psd_icc(unsigned char const *buffer,
                              size_t size,
                              unsigned char const **profile,
                              size_t *profile_length);

int
sixel_builtin_parse_psd_info(sixel_chunk_t const *chunk,
                             sixel_builtin_psd_info_t *info);

int
sixel_builtin_validate_psd_info(
    sixel_chunk_t const *chunk,
    sixel_builtin_psd_info_t const *info,
    int *pdecode_mode,
    int *pskip_icc_conversion,
    int *pcolorspace,
    char *message,
    size_t message_size);

SIXELSTATUS
sixel_builtin_decode_psd_bitmap_1bit(
    sixel_chunk_t const *chunk,
    sixel_builtin_psd_info_t const *info,
    unsigned char *bgcolor,
    unsigned char **ppixels,
    unsigned char **ptransparent_mask,
    size_t *ptransparent_mask_size,
    int *pwidth,
    int *pheight,
    int *ppixelformat);

SIXELSTATUS
sixel_builtin_decode_psd_gray_or_indexed_8bit(
    sixel_chunk_t const *chunk,
    sixel_builtin_psd_info_t const *info,
    unsigned char *bgcolor,
    unsigned char **ppixels,
    unsigned char **ptransparent_mask,
    size_t *ptransparent_mask_size,
    int *pwidth,
    int *pheight,
    int *ppixelformat);

SIXELSTATUS
sixel_builtin_decode_psd_gray_or_duotone_16bit(
    sixel_chunk_t const *chunk,
    sixel_builtin_psd_info_t const *info,
    unsigned char *bgcolor,
    unsigned char **ppixels,
    unsigned char **ptransparent_mask,
    size_t *ptransparent_mask_size,
    int *pwidth,
    int *pheight,
    int *ppixelformat);

SIXELSTATUS
sixel_builtin_decode_psd_cmyk_8bit(
    sixel_chunk_t const *chunk,
    sixel_builtin_psd_info_t const *info,
    unsigned char *bgcolor,
    unsigned char const *icc_profile,
    size_t icc_profile_length,
    int *pcms_applied,
    unsigned char **ppixels,
    unsigned char **ptransparent_mask,
    size_t *ptransparent_mask_size,
    int *pwidth,
    int *pheight,
    int *ppixelformat);

SIXELSTATUS
sixel_builtin_decode_psd_cmyk_16bit(
    sixel_chunk_t const *chunk,
    sixel_builtin_psd_info_t const *info,
    unsigned char *bgcolor,
    unsigned char const *icc_profile,
    size_t icc_profile_length,
    int *pcms_applied,
    unsigned char **ppixels,
    unsigned char **ptransparent_mask,
    size_t *ptransparent_mask_size,
    int *pwidth,
    int *pheight,
    int *ppixelformat);

SIXELSTATUS
sixel_builtin_decode_psd_rgb_8bit(
    sixel_chunk_t const *chunk,
    sixel_builtin_psd_info_t const *info,
    unsigned char *bgcolor,
    unsigned char **ppixels,
    unsigned char **ptransparent_mask,
    size_t *ptransparent_mask_size,
    int *pwidth,
    int *pheight,
    int *ppixelformat);

SIXELSTATUS
sixel_builtin_decode_psd_rgb_16bit(
    sixel_chunk_t const *chunk,
    sixel_builtin_psd_info_t const *info,
    unsigned char *bgcolor,
    unsigned char **ppixels,
    unsigned char **ptransparent_mask,
    size_t *ptransparent_mask_size,
    int *pwidth,
    int *pheight,
    int *ppixelformat);

SIXELSTATUS
sixel_builtin_decode_psd_rgb_32bit(
    sixel_chunk_t const *chunk,
    sixel_builtin_psd_info_t const *info,
    unsigned char *bgcolor,
    unsigned char **ppixels,
    unsigned char **ptransparent_mask,
    size_t *ptransparent_mask_size,
    int *pwidth,
    int *pheight,
    int *ppixelformat);

SIXELSTATUS
sixel_builtin_decode_psd_gray_or_duotone_32bit(
    sixel_chunk_t const *chunk,
    sixel_builtin_psd_info_t const *info,
    unsigned char *bgcolor,
    unsigned char **ppixels,
    unsigned char **ptransparent_mask,
    size_t *ptransparent_mask_size,
    int *pwidth,
    int *pheight,
    int *ppixelformat);

SIXELSTATUS
sixel_builtin_decode_psd_lab_8bit(
    sixel_chunk_t const *chunk,
    sixel_builtin_psd_info_t const *info,
    unsigned char *bgcolor,
    unsigned char **ppixels,
    unsigned char **ptransparent_mask,
    size_t *ptransparent_mask_size,
    int *pwidth,
    int *pheight,
    int *ppixelformat);

SIXELSTATUS
sixel_builtin_decode_psd_lab_16bit(
    sixel_chunk_t const *chunk,
    sixel_builtin_psd_info_t const *info,
    unsigned char *bgcolor,
    unsigned char **ppixels,
    unsigned char **ptransparent_mask,
    size_t *ptransparent_mask_size,
    int *pwidth,
    int *pheight,
    int *ppixelformat);

SIXELSTATUS
sixel_builtin_decode_psd_cmyk_32bit(
    sixel_chunk_t const *chunk,
    sixel_builtin_psd_info_t const *info,
    unsigned char *bgcolor,
    unsigned char const *icc_profile,
    size_t icc_profile_length,
    int *pcms_applied,
    unsigned char **ppixels,
    unsigned char **ptransparent_mask,
    size_t *ptransparent_mask_size,
    int *pwidth,
    int *pheight,
    int *ppixelformat);

SIXELSTATUS
sixel_builtin_decode_psd_lab_32bit(
    sixel_chunk_t const *chunk,
    sixel_builtin_psd_info_t const *info,
    unsigned char *bgcolor,
    unsigned char **ppixels,
    unsigned char **ptransparent_mask,
    size_t *ptransparent_mask_size,
    int *pwidth,
    int *pheight,
    int *ppixelformat);

#endif /* LIBSIXEL_FROMPSD_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 : */
/* EOF */
