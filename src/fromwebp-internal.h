/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See AUTHORS.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
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

#ifndef LIBSIXEL_FROMWEBP_INTERNAL_H
#define LIBSIXEL_FROMWEBP_INTERNAL_H

#include <stddef.h>

#include <sixel.h>

#include "allocator.h"
#include "chunk.h"

#define SIXEL_WEBP_MAX_DIMENSION 32767
#define SIXEL_WEBP_MAX_IMAGE_PIXELS ((size_t)268435456u)
#define SIXEL_WEBP_MAX_ANIMATION_FRAMES 1024u

#define SIXEL_WEBP_CHUNK_VP8  0x20385056u /* "VP8 " little-endian */
#define SIXEL_WEBP_CHUNK_VP8L 0x4c385056u /* "VP8L" little-endian */
#define SIXEL_WEBP_CHUNK_VP8X 0x58385056u /* "VP8X" little-endian */
#define SIXEL_WEBP_CHUNK_ALPHA 0x48504c41u /* alpha chunk little-endian */
#define SIXEL_WEBP_CHUNK_ANIM 0x4d494e41u /* "ANIM" little-endian */
#define SIXEL_WEBP_CHUNK_ANMF 0x464d4e41u /* "ANMF" little-endian */
#define SIXEL_WEBP_CHUNK_ICCP 0x50434349u /* "ICCP" little-endian */
#define SIXEL_WEBP_CHUNK_EXIF 0x46495845u /* "EXIF" little-endian */
#define SIXEL_WEBP_CHUNK_XMP  0x20504d58u /* "XMP " little-endian */

#define SIXEL_WEBP_VP8X_ANIMATION_FLAG 0x02u

#define SIXEL_WEBP_CODE_OK_VP8_STATIC "W_OK_VP8_STATIC"
#define SIXEL_WEBP_CODE_OK_VP8_ALPHA_STATIC "W_OK_VP8_ALPHA_STATIC"
#define SIXEL_WEBP_CODE_OK_VP8L_STATIC "W_OK_VP8L_STATIC"
#define SIXEL_WEBP_CODE_OK_ANIM "W_OK_ANIM"
#define SIXEL_WEBP_CODE_META_ICCP_APPLIED "W_META_ICCP_APPLIED"
#define SIXEL_WEBP_CODE_META_EXIF_APPLIED "W_META_EXIF_APPLIED"
#define SIXEL_WEBP_CODE_META_ICCP_IGNORED "W_META_ICCP_IGNORED"
#define SIXEL_WEBP_CODE_META_EXIF_IGNORED "W_META_EXIF_IGNORED"
#define SIXEL_WEBP_CODE_UNSUP_VP8_ALPHA "W_UNSUP_VP8_ALPHA"
#define SIXEL_WEBP_CODE_UNSUP_VP8_FEATURE "W_UNSUP_VP8_FEATURE"
#define SIXEL_WEBP_CODE_UNSUP_ANIM "W_UNSUP_ANIM"
#define SIXEL_WEBP_CODE_ERR_RIFF_SIGNATURE "W_ERR_RIFF_SIGNATURE"
#define SIXEL_WEBP_CODE_ERR_RIFF_HEADER_TRUNC "W_ERR_RIFF_HEADER_TRUNC"
#define SIXEL_WEBP_CODE_ERR_RIFF_SIZE_FIELD "W_ERR_RIFF_SIZE_FIELD"
#define SIXEL_WEBP_CODE_ERR_RIFF_SIZE_EXCEEDS "W_ERR_RIFF_SIZE_EXCEEDS"
#define SIXEL_WEBP_CODE_ERR_FIRST_CHUNK "W_ERR_FIRST_CHUNK"
#define SIXEL_WEBP_CODE_ERR_VP8X_SIZE "W_ERR_VP8X_SIZE"
#define SIXEL_WEBP_CODE_ERR_DUP_VP8X "W_ERR_DUP_VP8X"
#define SIXEL_WEBP_CODE_ERR_CHUNK_HDR_TRUNC "W_ERR_CHUNK_HDR_TRUNC"
#define SIXEL_WEBP_CODE_ERR_CHUNK_PAYLOAD_EXCEEDS "W_ERR_CHUNK_PAYLOAD_EXCEEDS"
#define SIXEL_WEBP_CODE_ERR_ODD_PADDING_NONZERO "W_ERR_ODD_PADDING_NONZERO"
#define SIXEL_WEBP_CODE_ERR_MISSING_VP8L "W_ERR_MISSING_VP8L"
#define SIXEL_WEBP_CODE_ERR_VP8_STREAM "W_ERR_VP8_STREAM"
#define SIXEL_WEBP_CODE_ERR_VP8L_STREAM "W_ERR_VP8L_STREAM"

typedef enum sixel_webp_container_kind {
    SIXEL_WEBP_CONTAINER_KIND_CORRUPT = 0,
    SIXEL_WEBP_CONTAINER_KIND_VP8_STATIC,
    SIXEL_WEBP_CONTAINER_KIND_VP8_ALPHA_STATIC,
    SIXEL_WEBP_CONTAINER_KIND_VP8L_STATIC,
    SIXEL_WEBP_CONTAINER_KIND_ANIM_MVP,
    SIXEL_WEBP_CONTAINER_KIND_UNSUPPORTED_ANIM
} sixel_webp_container_kind_t;

typedef struct sixel_webp_chunk_ref {
    int present;
    size_t chunk_offset;
    unsigned char const *payload;
    size_t payload_size;
} sixel_webp_chunk_ref_t;

typedef struct sixel_webp_container_info {
    /*
     * The parser stores first chunk references and per-type occurrence counts
     * so future feature layers can consume container metadata without walking
     * RIFF again.
     */
    sixel_webp_chunk_ref_t vp8;
    sixel_webp_chunk_ref_t vp8l;
    sixel_webp_chunk_ref_t vp8x;
    sixel_webp_chunk_ref_t alpha;
    sixel_webp_chunk_ref_t anim;
    sixel_webp_chunk_ref_t anmf;
    sixel_webp_chunk_ref_t iccp;
    sixel_webp_chunk_ref_t exif;
    sixel_webp_chunk_ref_t xmp;
    unsigned int vp8_count;
    unsigned int vp8l_count;
    unsigned int vp8x_count;
    unsigned int alpha_count;
    unsigned int anim_count;
    unsigned int anmf_count;
    unsigned int iccp_count;
    unsigned int exif_count;
    unsigned int xmp_count;
    unsigned char vp8x_flags;
    unsigned int canvas_width;
    unsigned int canvas_height;
    unsigned int anim_loop_count;
    unsigned int anim_background;
} sixel_webp_container_info_t;

typedef struct sixel_webp_decode_plan {
    sixel_webp_container_kind_t kind;
    unsigned char const *vp8_payload;
    size_t vp8_payload_size;
    unsigned char const *alpha_payload;
    size_t alpha_payload_size;
    unsigned char const *vp8l_payload;
    size_t vp8l_payload_size;
    unsigned char const *iccp_payload;
    size_t iccp_payload_size;
    unsigned char const *exif_payload;
    size_t exif_payload_size;
    int meta_has_iccp;
    int meta_has_exif;
    int canvas_width;
    int canvas_height;
    int anim_loop_count;
    int anim_frame_count;
} sixel_webp_decode_plan_t;

SIXEL_INTERNAL_API SIXELSTATUS
sixel_webp_parse_container(sixel_chunk_t const *chunk,
                           sixel_webp_container_info_t *info);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_webp_build_decode_plan(sixel_webp_container_info_t const *info,
                             sixel_webp_decode_plan_t *plan);

SIXEL_INTERNAL_API sixel_webp_container_kind_t
sixel_webp_classify_container(sixel_webp_container_info_t const *info);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_webp_decode_vp8l_payload(unsigned char const *payload,
                               size_t payload_size,
                               unsigned char **prgba,
                               int *pwidth,
                               int *pheight,
                               sixel_allocator_t *allocator);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_webp_decode_vp8_payload(unsigned char const *payload,
                              size_t payload_size,
                              unsigned char **prgba,
                              int *pwidth,
                              int *pheight,
                              sixel_allocator_t *allocator);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_webp_apply_vp8_alpha_payload(unsigned char *rgba,
                                   int width,
                                   int height,
                                   unsigned char const *payload,
                                   size_t payload_size,
                                   sixel_allocator_t *allocator);

SIXEL_INTERNAL_API void
sixel_webp_trace_contract_add_code(char const *code);

SIXEL_INTERNAL_API void
sixel_webp_trace_contract_flush(int rc);

#endif  /* LIBSIXEL_FROMWEBP_INTERNAL_H */


/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
