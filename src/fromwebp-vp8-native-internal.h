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

#ifndef LIBSIXEL_FROMWEBP_VP8_NATIVE_INTERNAL_H
#define LIBSIXEL_FROMWEBP_VP8_NATIVE_INTERNAL_H

/* STDC_HEADERS */
#include <stddef.h>

#if HAVE_STDINT_H
# include <stdint.h>
#endif

#include "fromwebp-vp8-private.h"

#define SIXEL_WEBP_VP8_BOOL_BASE_PROB 128u
#define SIXEL_WEBP_VP8_CONTROL_OFFSET 10u
#define SIXEL_WEBP_VP8_MAX_TOKEN_PARTITIONS 8u
#define SIXEL_WEBP_VP8_COEFF_TYPES 4u
#define SIXEL_WEBP_VP8_COEFF_BANDS 8u
#define SIXEL_WEBP_VP8_PREV_COEFF_CONTEXTS 3u
#define SIXEL_WEBP_VP8_COEFF_NODES 11u
#define SIXEL_WEBP_VP8_SEGMENTS 4u
#define SIXEL_WEBP_VP8_LF_REFS 4u
#define SIXEL_WEBP_VP8_LF_MODES 4u
#define SIXEL_WEBP_VP8_BLOCK_COEFFS 16u
#define SIXEL_WEBP_VP8_BLOCKS_Y 16u
#define SIXEL_WEBP_VP8_BLOCKS_UV 4u
#define SIXEL_WEBP_VP8_BLOCKS_MB 25u
#define SIXEL_WEBP_VP8_SEGMENT_TREE_NODES 3u
#define SIXEL_WEBP_VP8_COEFF_TYPE_Y1 0u
#define SIXEL_WEBP_VP8_COEFF_TYPE_Y2 1u
#define SIXEL_WEBP_VP8_COEFF_TYPE_UV 2u
#define SIXEL_WEBP_VP8_COEFF_TYPE_Y1_B 3u
#define SIXEL_WEBP_VP8_YUV_FIX2 6u
#define SIXEL_WEBP_VP8_YUV_MASK2 ((256u << SIXEL_WEBP_VP8_YUV_FIX2) - 1u)

#define SIXEL_WEBP_VP8_IDCT_COSPI8SQRT2_MINUS1 20091
#define SIXEL_WEBP_VP8_IDCT_SINPI8SQRT2 35468

#define SIXEL_WEBP_VP8_BORDER_TOP 127u
#define SIXEL_WEBP_VP8_BORDER_LEFT 129u

#define SIXEL_WEBP_VP8_MODE_DC 0u
#define SIXEL_WEBP_VP8_MODE_TM 1u
#define SIXEL_WEBP_VP8_MODE_V  2u
#define SIXEL_WEBP_VP8_MODE_H  3u
#define SIXEL_WEBP_VP8_MODE_B  4u

#define SIXEL_WEBP_VP8_BMODE_DC 0u
#define SIXEL_WEBP_VP8_BMODE_TM 1u
#define SIXEL_WEBP_VP8_BMODE_VE 2u
#define SIXEL_WEBP_VP8_BMODE_HE 3u
#define SIXEL_WEBP_VP8_BMODE_RD 4u
#define SIXEL_WEBP_VP8_BMODE_VR 5u
#define SIXEL_WEBP_VP8_BMODE_LD 6u
#define SIXEL_WEBP_VP8_BMODE_VL 7u
#define SIXEL_WEBP_VP8_BMODE_HD 8u
#define SIXEL_WEBP_VP8_BMODE_HU 9u
#define SIXEL_WEBP_VP8_BMODES 10u

typedef struct sixel_webp_vp8_bool_decoder {
    unsigned char const *buffer;
    size_t size;
    size_t position;
    uint32_t value;
    unsigned int range;
    int bits;
    int eof;
} sixel_webp_vp8_bool_decoder_t;

typedef struct sixel_webp_vp8_partition_layout {
    size_t control_partition_offset;
    size_t control_partition_size;
    size_t token_partition_table_offset;
    size_t token_partition_data_offset;
    unsigned int token_partition_count;
    size_t token_partition_size[SIXEL_WEBP_VP8_MAX_TOKEN_PARTITIONS];
} sixel_webp_vp8_partition_layout_t;

typedef struct sixel_webp_vp8_segment_header {
    int enabled;
    int update_map;
    int update_data;
    int absolute_delta;
    int quant_delta[SIXEL_WEBP_VP8_SEGMENTS];
    int filter_delta[SIXEL_WEBP_VP8_SEGMENTS];
    int map_prob[3];
} sixel_webp_vp8_segment_header_t;

typedef struct sixel_webp_vp8_filter_header {
    int simple;
    unsigned int level;
    unsigned int sharpness;
    int update_delta;
    int ref_delta[SIXEL_WEBP_VP8_LF_REFS];
    int mode_delta[SIXEL_WEBP_VP8_LF_MODES];
} sixel_webp_vp8_filter_header_t;

typedef struct sixel_webp_vp8_quant_header {
    int y_ac_qi;
    int y_dc_delta;
    int y2_dc_delta;
    int y2_ac_delta;
    int uv_dc_delta;
    int uv_ac_delta;
} sixel_webp_vp8_quant_header_t;

typedef struct sixel_webp_vp8_entropy_header {
    int refresh_entropy_probs;
    int mb_no_coeff_skip;
    int prob_skip_false;
    int coef_prob_update_count;
    unsigned char coef_probs[SIXEL_WEBP_VP8_COEFF_TYPES]
                            [SIXEL_WEBP_VP8_COEFF_BANDS]
                            [SIXEL_WEBP_VP8_PREV_COEFF_CONTEXTS]
                            [SIXEL_WEBP_VP8_COEFF_NODES];
} sixel_webp_vp8_entropy_header_t;

typedef struct sixel_webp_vp8_frame_context {
    unsigned int mb_cols;
    unsigned int mb_rows;
    unsigned int token_partition_count;
    sixel_webp_vp8_bool_decoder_t mode_decoder;
    sixel_webp_vp8_segment_header_t segment;
    sixel_webp_vp8_filter_header_t filter;
    sixel_webp_vp8_quant_header_t quant;
    sixel_webp_vp8_entropy_header_t entropy;
} sixel_webp_vp8_frame_context_t;

typedef struct sixel_webp_vp8_planes {
    unsigned char *y;
    unsigned char *u;
    unsigned char *v;
    unsigned int y_stride;
    unsigned int uv_stride;
    unsigned int uv_width;
    unsigned int uv_height;
} sixel_webp_vp8_planes_t;

typedef struct sixel_webp_vp8_quant_values {
    int y1_dc;
    int y1_ac;
    int y2_dc;
    int y2_ac;
    int uv_dc;
    int uv_ac;
} sixel_webp_vp8_quant_values_t;

SIXEL_INTERNAL_API SIXELSTATUS
sixel_webp_vp8_bool_init(sixel_webp_vp8_bool_decoder_t *decoder,
                         unsigned char const *buffer,
                         size_t size);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_webp_vp8_bool_read(sixel_webp_vp8_bool_decoder_t *decoder,
                         unsigned int probability,
                         int *pbit);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_webp_vp8_bool_read_literal(sixel_webp_vp8_bool_decoder_t *decoder,
                                 unsigned int nbits,
                                 unsigned int *pvalue);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_webp_vp8_decode_coeff_block(
    sixel_webp_vp8_bool_decoder_t *decoder,
    unsigned char const probs[SIXEL_WEBP_VP8_COEFF_BANDS]
                             [SIXEL_WEBP_VP8_PREV_COEFF_CONTEXTS]
                             [SIXEL_WEBP_VP8_COEFF_NODES],
    unsigned int start_coeff,
    unsigned int coeff_context,
    int16_t coeffs[SIXEL_WEBP_VP8_BLOCK_COEFFS],
    unsigned int *peob);

#endif  /* LIBSIXEL_FROMWEBP_VP8_NATIVE_INTERNAL_H */


/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
