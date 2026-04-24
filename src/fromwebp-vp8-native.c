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

#if defined(HAVE_CONFIG_H)
# include "config.h"
#endif

/* STDC_HEADERS */
#include <stddef.h>
#include <limits.h>

#if HAVE_STDINT_H
# include <stdint.h>
#endif
#if HAVE_STRING_H
# include <string.h>
#endif

#ifndef SIZE_MAX
# define SIZE_MAX ((size_t)-1)
#endif

#include "compat_stub.h"
#include "fromwebp-vp8-private.h"
#include "loader-common.h"

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

#include "fromwebp-vp8-tables.h"

static unsigned int const sixel_webp_vp8_kf_ymode_prob[4] =
    {145u, 156u, 163u, 128u};
static unsigned int const sixel_webp_vp8_kf_uvmode_prob[3] =
    {142u, 114u, 183u};
static unsigned int const
sixel_webp_vp8_kf_bmode_prob[SIXEL_WEBP_VP8_BMODES]
                            [SIXEL_WEBP_VP8_BMODES][9] = {
    {
     {231u, 120u, 48u, 89u, 115u, 113u, 120u, 152u, 112u},
     {152u, 179u, 64u, 126u, 170u, 118u, 46u, 70u, 95u},
     {175u, 69u, 143u, 80u, 85u, 82u, 72u, 155u, 103u},
     {56u, 58u, 10u, 171u, 218u, 189u, 17u, 13u, 152u},
     {114u, 26u, 17u, 163u, 44u, 195u, 21u, 10u, 173u},
     {121u, 24u, 80u, 195u, 26u, 62u, 44u, 64u, 85u},
     {144u, 71u, 10u, 38u, 171u, 213u, 144u, 34u, 26u},
     {170u, 46u, 55u, 19u, 136u, 160u, 33u, 206u, 71u},
     {63u, 20u, 8u, 114u, 114u, 208u, 12u, 9u, 226u},
     {81u, 40u, 11u, 96u, 182u, 84u, 29u, 16u, 36u}
    },
    {
     {134u, 183u, 89u, 137u, 98u, 101u, 106u, 165u, 148u},
     {72u, 187u, 100u, 130u, 157u, 111u, 32u, 75u, 80u},
     {66u, 102u, 167u, 99u, 74u, 62u, 40u, 234u, 128u},
     {41u, 53u, 9u, 178u, 241u, 141u, 26u, 8u, 107u},
     {74u, 43u, 26u, 146u, 73u, 166u, 49u, 23u, 157u},
     {65u, 38u, 105u, 160u, 51u, 52u, 31u, 115u, 128u},
     {104u, 79u, 12u, 27u, 217u, 255u, 87u, 17u, 7u},
     {87u, 68u, 71u, 44u, 114u, 51u, 15u, 186u, 23u},
     {47u, 41u, 14u, 110u, 182u, 183u, 21u, 17u, 194u},
     {66u, 45u, 25u, 102u, 197u, 189u, 23u, 18u, 22u}
    },
    {
     {88u, 88u, 147u, 150u, 42u, 46u, 45u, 196u, 205u},
     {43u, 97u, 183u, 117u, 85u, 38u, 35u, 179u, 61u},
     {39u, 53u, 200u, 87u, 26u, 21u, 43u, 232u, 171u},
     {56u, 34u, 51u, 104u, 114u, 102u, 29u, 93u, 77u},
     {39u, 28u, 85u, 171u, 58u, 165u, 90u, 98u, 64u},
     {34u, 22u, 116u, 206u, 23u, 34u, 43u, 166u, 73u},
     {107u, 54u, 32u, 26u, 51u, 1u, 81u, 43u, 31u},
     {68u, 25u, 106u, 22u, 64u, 171u, 36u, 225u, 114u},
     {34u, 19u, 21u, 102u, 132u, 188u, 16u, 76u, 124u},
     {62u, 18u, 78u, 95u, 85u, 57u, 50u, 48u, 51u}
    },
    {
     {193u, 101u, 35u, 159u, 215u, 111u, 89u, 46u, 111u},
     {60u, 148u, 31u, 172u, 219u, 228u, 21u, 18u, 111u},
     {112u, 113u, 77u, 85u, 179u, 255u, 38u, 120u, 114u},
     {40u, 42u, 1u, 196u, 245u, 209u, 10u, 25u, 109u},
     {88u, 43u, 29u, 140u, 166u, 213u, 37u, 43u, 154u},
     {61u, 63u, 30u, 155u, 67u, 45u, 68u, 1u, 209u},
     {100u, 80u, 8u, 43u, 154u, 1u, 51u, 26u, 71u},
     {142u, 78u, 78u, 16u, 255u, 128u, 34u, 197u, 171u},
     {41u, 40u, 5u, 102u, 211u, 183u, 4u, 1u, 221u},
     {51u, 50u, 17u, 168u, 209u, 192u, 23u, 25u, 82u}
    },
    {
     {138u, 31u, 36u, 171u, 27u, 166u, 38u, 44u, 229u},
     {67u, 87u, 58u, 169u, 82u, 115u, 26u, 59u, 179u},
     {63u, 59u, 90u, 180u, 59u, 166u, 93u, 73u, 154u},
     {40u, 40u, 21u, 116u, 143u, 209u, 34u, 39u, 175u},
     {47u, 15u, 16u, 183u, 34u, 223u, 49u, 45u, 183u},
     {46u, 17u, 33u, 183u, 6u, 98u, 15u, 32u, 183u},
     {57u, 46u, 22u, 24u, 128u, 1u, 54u, 17u, 37u},
     {65u, 32u, 73u, 115u, 28u, 128u, 23u, 128u, 205u},
     {40u, 3u, 9u, 115u, 51u, 192u, 18u, 6u, 223u},
     {87u, 37u, 9u, 115u, 59u, 77u, 64u, 21u, 47u}
    },
    {
     {104u, 55u, 44u, 218u, 9u, 54u, 53u, 130u, 226u},
     {64u, 90u, 70u, 205u, 40u, 41u, 23u, 26u, 57u},
     {54u, 57u, 112u, 184u, 5u, 41u, 38u, 166u, 213u},
     {30u, 34u, 26u, 133u, 152u, 116u, 10u, 32u, 134u},
     {39u, 19u, 53u, 221u, 26u, 114u, 32u, 73u, 255u},
     {31u, 9u, 65u, 234u, 2u, 15u, 1u, 118u, 73u},
     {75u, 32u, 12u, 51u, 192u, 255u, 160u, 43u, 51u},
     {88u, 31u, 35u, 67u, 102u, 85u, 55u, 186u, 85u},
     {56u, 21u, 23u, 111u, 59u, 205u, 45u, 37u, 192u},
     {55u, 38u, 70u, 124u, 73u, 102u, 1u, 34u, 98u}
    },
    {
     {125u, 98u, 42u, 88u, 104u, 85u, 117u, 175u, 82u},
     {95u, 84u, 53u, 89u, 128u, 100u, 113u, 101u, 45u},
     {75u, 79u, 123u, 47u, 51u, 128u, 81u, 171u, 1u},
     {57u, 17u, 5u, 71u, 102u, 57u, 53u, 41u, 49u},
     {38u, 33u, 13u, 121u, 57u, 73u, 26u, 1u, 85u},
     {41u, 10u, 67u, 138u, 77u, 110u, 90u, 47u, 114u},
     {115u, 21u, 2u, 10u, 102u, 255u, 166u, 23u, 6u},
     {101u, 29u, 16u, 10u, 85u, 128u, 101u, 196u, 26u},
     {57u, 18u, 10u, 102u, 102u, 213u, 34u, 20u, 43u},
     {117u, 20u, 15u, 36u, 163u, 128u, 68u, 1u, 26u}
    },
    {
     {102u, 61u, 71u, 37u, 34u, 53u, 31u, 243u, 192u},
     {69u, 60u, 71u, 38u, 73u, 119u, 28u, 222u, 37u},
     {68u, 45u, 128u, 34u, 1u, 47u, 11u, 245u, 171u},
     {62u, 17u, 19u, 70u, 146u, 85u, 55u, 62u, 70u},
     {37u, 43u, 37u, 154u, 100u, 163u, 85u, 160u, 1u},
     {63u, 9u, 92u, 136u, 28u, 64u, 32u, 201u, 85u},
     {75u, 15u, 9u, 9u, 64u, 255u, 184u, 119u, 16u},
     {86u, 6u, 28u, 5u, 64u, 255u, 25u, 248u, 1u},
     {56u, 8u, 17u, 132u, 137u, 255u, 55u, 116u, 128u},
     {58u, 15u, 20u, 82u, 135u, 57u, 26u, 121u, 40u}
    },
    {
     {164u, 50u, 31u, 137u, 154u, 133u, 25u, 35u, 218u},
     {51u, 103u, 44u, 131u, 131u, 123u, 31u, 6u, 158u},
     {86u, 40u, 64u, 135u, 148u, 224u, 45u, 183u, 128u},
     {22u, 26u, 17u, 131u, 240u, 154u, 14u, 1u, 209u},
     {45u, 16u, 21u, 91u, 64u, 222u, 7u, 1u, 197u},
     {56u, 21u, 39u, 155u, 60u, 138u, 23u, 102u, 213u},
     {83u, 12u, 13u, 54u, 192u, 255u, 68u, 47u, 28u},
     {85u, 26u, 85u, 85u, 128u, 128u, 32u, 146u, 171u},
     {18u, 11u, 7u, 63u, 144u, 171u, 4u, 4u, 246u},
     {35u, 27u, 10u, 146u, 174u, 171u, 12u, 26u, 128u}
    },
    {
     {190u, 80u, 35u, 99u, 180u, 80u, 126u, 54u, 45u},
     {85u, 126u, 47u, 87u, 176u, 51u, 41u, 20u, 32u},
     {101u, 75u, 128u, 139u, 118u, 146u, 116u, 128u, 85u},
     {56u, 41u, 15u, 176u, 236u, 85u, 37u, 9u, 62u},
     {71u, 30u, 17u, 119u, 118u, 255u, 17u, 18u, 138u},
     {101u, 38u, 60u, 138u, 55u, 70u, 43u, 26u, 142u},
     {146u, 36u, 19u, 30u, 171u, 255u, 97u, 27u, 20u},
     {138u, 45u, 61u, 62u, 219u, 1u, 81u, 188u, 64u},
     {32u, 41u, 20u, 117u, 151u, 142u, 20u, 21u, 163u},
     {112u, 19u, 12u, 61u, 195u, 128u, 48u, 4u, 24u}
    }
};

static unsigned int const sixel_webp_vp8_zigzag[16] =
    {0u, 1u, 4u, 8u, 5u, 2u, 3u, 6u,
     9u, 12u, 13u, 10u, 7u, 11u, 14u, 15u};
static unsigned int const sixel_webp_vp8_coeff_band[17] =
    {0u, 1u, 2u, 3u, 6u, 4u, 5u, 6u, 6u,
     6u, 6u, 6u, 6u, 6u, 6u, 7u, 0u};

static unsigned int const sixel_webp_vp8_cat3_prob[3] =
    {173u, 148u, 140u};
static unsigned int const sixel_webp_vp8_cat4_prob[4] =
    {176u, 155u, 140u, 135u};
static unsigned int const sixel_webp_vp8_cat5_prob[5] =
    {180u, 157u, 141u, 134u, 130u};
static unsigned int const sixel_webp_vp8_cat6_prob[11] =
    {254u, 254u, 243u, 230u, 196u, 177u, 153u, 140u, 133u, 130u, 129u};

static int const sixel_webp_vp8_dc_qlookup[128] = {
    4, 5, 6, 7, 8, 9, 10, 10, 11, 12, 13, 14, 15, 16, 17, 17,
    18, 19, 20, 20, 21, 21, 22, 22, 23, 23, 24, 25, 25, 26, 27, 28,
    29, 30, 31, 32, 33, 34, 35, 36, 37, 37, 38, 39, 40, 41, 42, 43,
    44, 45, 46, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58,
    59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74,
    75, 76, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89,
    91, 93, 95, 96, 98, 100, 101, 102, 104, 106, 108, 110, 112, 114,
    116, 118, 122, 124, 126, 128, 130, 132, 134, 136, 138, 140, 143,
    145, 148, 151, 154, 157
};

static int const sixel_webp_vp8_ac_qlookup[128] = {
    4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
    20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,
    36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,
    52, 53, 54, 55, 56, 57, 58, 60, 62, 64, 66, 68, 70, 72, 74, 76,
    78, 80, 82, 84, 86, 88, 90, 92, 94, 96, 98, 100, 102, 104, 106,
    108, 110, 112, 114, 116, 119, 122, 125, 128, 131, 134, 137, 140,
    143, 146, 149, 152, 155, 158, 161, 164, 167, 170, 173, 177, 181,
    185, 189, 193, 197, 201, 205, 209, 213, 217, 221, 225, 229, 234,
    239, 245, 249, 254, 259, 264, 269, 274, 279, 284
};

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

static unsigned int
sixel_webp_vp8_read_u24le(unsigned char const *p)
{
    if (p == NULL) {
        return 0u;
    }
    return (unsigned int)p[0]
        | ((unsigned int)p[1] << 8)
        | ((unsigned int)p[2] << 16);
}

static void
sixel_webp_vp8_bool_load_final_byte(sixel_webp_vp8_bool_decoder_t *decoder)
{
    if (decoder == NULL || decoder->buffer == NULL) {
        return;
    }

    if (decoder->position < decoder->size) {
        decoder->bits += 8;
        decoder->value =
            (uint32_t)decoder->buffer[decoder->position]
            | (decoder->value << 8);
        decoder->position++;
    } else if (decoder->eof == 0) {
        decoder->value <<= 8;
        decoder->bits += 8;
        decoder->eof = 1;
    } else {
        decoder->bits = 0;
    }
}

static void
sixel_webp_vp8_bool_fill(sixel_webp_vp8_bool_decoder_t *decoder)
{
    uint32_t bits;

    bits = 0u;
    if (decoder == NULL || decoder->buffer == NULL) {
        return;
    }

    if (decoder->position + 3u <= decoder->size) {
        bits = (uint32_t)decoder->buffer[decoder->position] << 16;
        bits |= (uint32_t)decoder->buffer[decoder->position + 1u] << 8;
        bits |= (uint32_t)decoder->buffer[decoder->position + 2u];
        decoder->position += 3u;
        decoder->value = bits | (decoder->value << 24);
        decoder->bits += 24;
    } else {
        sixel_webp_vp8_bool_load_final_byte(decoder);
    }
}

static SIXELSTATUS
sixel_webp_vp8_bool_init(sixel_webp_vp8_bool_decoder_t *decoder,
                         unsigned char const *buffer,
                         size_t size)
{
    if (decoder == NULL || buffer == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    decoder->buffer = buffer;
    decoder->size = size;
    decoder->position = 0u;
    decoder->value = 0u;
    decoder->range = 254u;
    decoder->bits = -8;
    decoder->eof = 0;
    sixel_webp_vp8_bool_fill(decoder);

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_webp_vp8_bool_read(sixel_webp_vp8_bool_decoder_t *decoder,
                         unsigned int probability,
                         int *pbit)
{
    SIXELSTATUS status;
    unsigned int split;
    unsigned int value;
    unsigned int range;
    unsigned int tmp;
    unsigned int shift;
    int bit;

    status = SIXEL_OK;
    split = 0u;
    value = 0u;
    range = 0u;
    shift = 0u;
    bit = 0;
    if (decoder == NULL || pbit == NULL || probability > 255u) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (decoder->bits < 0) {
        sixel_webp_vp8_bool_fill(decoder);
        if (decoder->bits < 0) {
            sixel_helper_set_additional_message(
                "builtin webp: VP8 bitstream partition is truncated.");
            status = SIXEL_BAD_INPUT;
            goto end;
        }
    }

    range = decoder->range;
    split = (range * probability) >> 8;
    value = (unsigned int)(decoder->value >> (unsigned int)decoder->bits);
    if (value > split) {
        range -= split;
        decoder->value -=
            (uint32_t)(split + 1u) << (unsigned int)decoder->bits;
        bit = 1;
    } else {
        range = split + 1u;
        bit = 0;
    }

    tmp = range;
    shift = 0u;
    while (tmp > 1u) {
        tmp >>= 1u;
        shift++;
    }
    shift = 7u - shift;
    range <<= shift;
    decoder->bits -= (int)shift;
    decoder->range = range - 1u;
    *pbit = bit;

end:
    return status;
}

static SIXELSTATUS
sixel_webp_vp8_bool_read_literal(sixel_webp_vp8_bool_decoder_t *decoder,
                                 unsigned int nbits,
                                 unsigned int *pvalue)
{
    SIXELSTATUS status;
    unsigned int value;
    unsigned int i;
    int bit;

    status = SIXEL_OK;
    value = 0u;
    i = 0u;
    bit = 0;
    if (decoder == NULL || pvalue == NULL || nbits > 24u) {
        return SIXEL_BAD_ARGUMENT;
    }

    for (i = 0u; i < nbits; ++i) {
        status = sixel_webp_vp8_bool_read(decoder,
                                          SIXEL_WEBP_VP8_BOOL_BASE_PROB,
                                          &bit);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        value = (value << 1) | (unsigned int)bit;
    }

    *pvalue = value;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_webp_vp8_bool_read_signed_value(sixel_webp_vp8_bool_decoder_t *decoder,
                                      unsigned int value,
                                      int *psigned)
{
    unsigned int split;
    unsigned int dec_value;
    int32_t mask;
    int signed_value;

    split = 0u;
    dec_value = 0u;
    mask = 0;
    signed_value = 0;
    if (decoder == NULL || psigned == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (decoder->bits < 0) {
        sixel_webp_vp8_bool_fill(decoder);
        if (decoder->bits < 0) {
            sixel_helper_set_additional_message(
                "builtin webp: VP8 bitstream partition is truncated.");
            return SIXEL_BAD_INPUT;
        }
    }

    split = decoder->range >> 1u;
    dec_value = (unsigned int)(decoder->value >> (unsigned int)decoder->bits);
    mask = (int32_t)(split - dec_value) >> 31;
    decoder->bits--;
    decoder->range = decoder->range + (unsigned int)mask;
    decoder->range |= 1u;
    decoder->value -= (uint32_t)((split + 1u) & (unsigned int)mask)
        << (unsigned int)(decoder->bits + 1);

    if (mask != 0) {
        signed_value = -(int)value;
    } else {
        signed_value = (int)value;
    }

    *psigned = signed_value;
    return SIXEL_OK;
}

static unsigned char
sixel_webp_vp8_clamp_u8(int value)
{
    if (value < 0) {
        return 0u;
    }
    if (value > 255) {
        return 255u;
    }
    return (unsigned char)value;
}

static unsigned char
sixel_webp_vp8_clip_yuv2(int value)
{
    if ((value & ~(int)SIXEL_WEBP_VP8_YUV_MASK2) == 0) {
        return (unsigned char)(value >> SIXEL_WEBP_VP8_YUV_FIX2);
    }
    if (value < 0) {
        return 0u;
    }
    return 255u;
}

static unsigned char
sixel_webp_vp8_avg2(int a,
                    int b)
{
    return (unsigned char)((a + b + 1) >> 1);
}

static unsigned char
sixel_webp_vp8_avg3(int a,
                    int b,
                    int c)
{
    return (unsigned char)((a + (b << 1) + c + 2) >> 2);
}

static int
sixel_webp_vp8_alloc_planes(
    sixel_webp_vp8_planes_t *planes,
    sixel_webp_vp8_frame_header_t const *header,
    sixel_allocator_t *allocator)
{
    size_t y_size;
    size_t uv_size;
    size_t y_pixel_count;
    size_t uv_pixel_count;
    unsigned int width;
    unsigned int height;
    unsigned int uv_width;
    unsigned int uv_height;

    y_size = 0u;
    uv_size = 0u;
    y_pixel_count = 0u;
    uv_pixel_count = 0u;
    width = 0u;
    height = 0u;
    uv_width = 0u;
    uv_height = 0u;
    if (planes == NULL || header == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    memset(planes, 0, sizeof(*planes));
    width = (unsigned int)header->width;
    height = (unsigned int)header->height;
    uv_width = (unsigned int)((header->width + 1) / 2);
    uv_height = (unsigned int)((header->height + 1) / 2);
    if (width == 0u || height == 0u || uv_width == 0u || uv_height == 0u) {
        return SIXEL_BAD_INPUT;
    }

    if ((size_t)width > SIZE_MAX / (size_t)height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    y_pixel_count = (size_t)width * (size_t)height;
    if (y_pixel_count > SIZE_MAX) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    y_size = y_pixel_count;

    if ((size_t)uv_width > SIZE_MAX / (size_t)uv_height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    uv_pixel_count = (size_t)uv_width * (size_t)uv_height;
    if (uv_pixel_count > SIZE_MAX) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    uv_size = uv_pixel_count;

    planes->y = (unsigned char *)sixel_allocator_malloc(allocator, y_size);
    planes->u = (unsigned char *)sixel_allocator_malloc(allocator, uv_size);
    planes->v = (unsigned char *)sixel_allocator_malloc(allocator, uv_size);
    if (planes->y == NULL || planes->u == NULL || planes->v == NULL) {
        sixel_helper_set_additional_message(
            "builtin webp: sixel_allocator_malloc() failed.");
        sixel_allocator_free(allocator, planes->y);
        sixel_allocator_free(allocator, planes->u);
        sixel_allocator_free(allocator, planes->v);
        memset(planes, 0, sizeof(*planes));
        return SIXEL_BAD_ALLOCATION;
    }

    planes->y_stride = width;
    planes->uv_stride = uv_width;
    planes->uv_width = uv_width;
    planes->uv_height = uv_height;
    memset(planes->y, 128, y_size);
    memset(planes->u, 128, uv_size);
    memset(planes->v, 128, uv_size);
    return SIXEL_OK;
}

static void
sixel_webp_vp8_free_planes(sixel_webp_vp8_planes_t *planes,
                           sixel_allocator_t *allocator)
{
    if (planes == NULL || allocator == NULL) {
        return;
    }
    sixel_allocator_free(allocator, planes->y);
    sixel_allocator_free(allocator, planes->u);
    sixel_allocator_free(allocator, planes->v);
    memset(planes, 0, sizeof(*planes));
}

static int
sixel_webp_vp8_clamp_qindex(int qindex)
{
    if (qindex < 0) {
        return 0;
    }
    if (qindex > 127) {
        return 127;
    }
    return qindex;
}

static int
sixel_webp_vp8_get_dc_quant(int qindex,
                            int delta)
{
    int index;

    index = sixel_webp_vp8_clamp_qindex(qindex + delta);
    return sixel_webp_vp8_dc_qlookup[index];
}

static int
sixel_webp_vp8_get_ac_quant(int qindex,
                            int delta)
{
    int index;

    index = sixel_webp_vp8_clamp_qindex(qindex + delta);
    return sixel_webp_vp8_ac_qlookup[index];
}

static int
sixel_webp_vp8_get_ac2_quant(int qindex,
                             int delta)
{
    int ac;

    ac = sixel_webp_vp8_get_ac_quant(qindex, delta);
    ac = (ac * 101581) >> 16;
    if (ac < 8) {
        ac = 8;
    }
    return ac;
}

static SIXELSTATUS
sixel_webp_vp8_read_segment_id(
    sixel_webp_vp8_bool_decoder_t *decoder,
    sixel_webp_vp8_segment_header_t const *segment,
    unsigned int *psegment_id)
{
    SIXELSTATUS status;
    int bit;
    unsigned int segment_id;

    status = SIXEL_OK;
    bit = 0;
    segment_id = 0u;
    if (decoder == NULL || segment == NULL || psegment_id == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (segment->enabled == 0 || segment->update_map == 0) {
        *psegment_id = 0u;
        return SIXEL_OK;
    }

    status = sixel_webp_vp8_bool_read(
        decoder, (unsigned int)segment->map_prob[0], &bit);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (bit != 0) {
        status = sixel_webp_vp8_bool_read(
            decoder, (unsigned int)segment->map_prob[2], &bit);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        segment_id = 2u + (unsigned int)bit;
    } else {
        status = sixel_webp_vp8_bool_read(
            decoder, (unsigned int)segment->map_prob[1], &bit);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        segment_id = (unsigned int)bit;
    }

    *psegment_id = segment_id;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_webp_vp8_read_kf_ymode(sixel_webp_vp8_bool_decoder_t *decoder,
                             unsigned int *pymode)
{
    SIXELSTATUS status;
    int bit;

    status = SIXEL_OK;
    bit = 0;
    if (decoder == NULL || pymode == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_webp_vp8_bool_read(
        decoder, sixel_webp_vp8_kf_ymode_prob[0], &bit);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (bit == 0) {
        *pymode = SIXEL_WEBP_VP8_MODE_B;
        return SIXEL_OK;
    }

    status = sixel_webp_vp8_bool_read(
        decoder, sixel_webp_vp8_kf_ymode_prob[1], &bit);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (bit == 0) {
        status = sixel_webp_vp8_bool_read(
            decoder, sixel_webp_vp8_kf_ymode_prob[2], &bit);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        if (bit == 0) {
            *pymode = SIXEL_WEBP_VP8_MODE_DC;
        } else {
            *pymode = SIXEL_WEBP_VP8_MODE_V;
        }
        return SIXEL_OK;
    }

    status = sixel_webp_vp8_bool_read(
        decoder, sixel_webp_vp8_kf_ymode_prob[3], &bit);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (bit == 0) {
        *pymode = SIXEL_WEBP_VP8_MODE_H;
    } else {
        *pymode = SIXEL_WEBP_VP8_MODE_TM;
    }
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_webp_vp8_read_uv_mode(sixel_webp_vp8_bool_decoder_t *decoder,
                            unsigned int *puv_mode)
{
    SIXELSTATUS status;
    int bit;

    status = SIXEL_OK;
    bit = 0;
    if (decoder == NULL || puv_mode == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_webp_vp8_bool_read(
        decoder, sixel_webp_vp8_kf_uvmode_prob[0], &bit);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (bit == 0) {
        *puv_mode = SIXEL_WEBP_VP8_MODE_DC;
        return SIXEL_OK;
    }

    status = sixel_webp_vp8_bool_read(
        decoder, sixel_webp_vp8_kf_uvmode_prob[1], &bit);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (bit == 0) {
        *puv_mode = SIXEL_WEBP_VP8_MODE_V;
        return SIXEL_OK;
    }

    status = sixel_webp_vp8_bool_read(
        decoder, sixel_webp_vp8_kf_uvmode_prob[2], &bit);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (bit == 0) {
        *puv_mode = SIXEL_WEBP_VP8_MODE_H;
    } else {
        *puv_mode = SIXEL_WEBP_VP8_MODE_TM;
    }
    return SIXEL_OK;
}

static unsigned int
sixel_webp_vp8_map_ymode_to_bmode(unsigned int ymode)
{
    switch (ymode) {
    case SIXEL_WEBP_VP8_MODE_V:
        return SIXEL_WEBP_VP8_BMODE_VE;
    case SIXEL_WEBP_VP8_MODE_H:
        return SIXEL_WEBP_VP8_BMODE_HE;
    case SIXEL_WEBP_VP8_MODE_TM:
        return SIXEL_WEBP_VP8_BMODE_TM;
    default:
        return SIXEL_WEBP_VP8_BMODE_DC;
    }
}

static SIXELSTATUS
sixel_webp_vp8_read_bmode(
    sixel_webp_vp8_bool_decoder_t *decoder,
    unsigned int const probs[9],
    unsigned int *pbmode)
{
    SIXELSTATUS status;
    int bit;

    status = SIXEL_OK;
    bit = 0;
    if (decoder == NULL || probs == NULL || pbmode == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_webp_vp8_bool_read(decoder, probs[0], &bit);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (bit == 0) {
        *pbmode = SIXEL_WEBP_VP8_BMODE_DC;
        return SIXEL_OK;
    }
    status = sixel_webp_vp8_bool_read(decoder, probs[1], &bit);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (bit == 0) {
        *pbmode = SIXEL_WEBP_VP8_BMODE_TM;
        return SIXEL_OK;
    }
    status = sixel_webp_vp8_bool_read(decoder, probs[2], &bit);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (bit == 0) {
        *pbmode = SIXEL_WEBP_VP8_BMODE_VE;
        return SIXEL_OK;
    }

    status = sixel_webp_vp8_bool_read(decoder, probs[3], &bit);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (bit == 0) {
        status = sixel_webp_vp8_bool_read(decoder, probs[4], &bit);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        if (bit == 0) {
            *pbmode = SIXEL_WEBP_VP8_BMODE_HE;
            return SIXEL_OK;
        }
        status = sixel_webp_vp8_bool_read(decoder, probs[5], &bit);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        if (bit == 0) {
            *pbmode = SIXEL_WEBP_VP8_BMODE_RD;
        } else {
            *pbmode = SIXEL_WEBP_VP8_BMODE_VR;
        }
        return SIXEL_OK;
    }

    status = sixel_webp_vp8_bool_read(decoder, probs[6], &bit);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (bit == 0) {
        *pbmode = SIXEL_WEBP_VP8_BMODE_LD;
        return SIXEL_OK;
    }
    status = sixel_webp_vp8_bool_read(decoder, probs[7], &bit);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (bit == 0) {
        *pbmode = SIXEL_WEBP_VP8_BMODE_VL;
        return SIXEL_OK;
    }
    status = sixel_webp_vp8_bool_read(decoder, probs[8], &bit);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (bit == 0) {
        *pbmode = SIXEL_WEBP_VP8_BMODE_HD;
    } else {
        *pbmode = SIXEL_WEBP_VP8_BMODE_HU;
    }
    return SIXEL_OK;
}

static void
sixel_webp_vp8_predict_bmode(unsigned int bmode,
                             unsigned char const *plane,
                             unsigned int stride,
                             unsigned int width,
                             unsigned int height,
                             unsigned int x,
                             unsigned int y,
                             unsigned char pred[16])
{
    unsigned int i;
    unsigned int ref_x;
    unsigned int ref_y;
    unsigned int mb_right;
    unsigned int mb_top_y;
    unsigned char above[8];
    unsigned char left[4];
    unsigned char top_left;
    int a;
    int b;
    int c;
    int d;
    int e;
    int f;
    int g;
    int h;
    int l0;
    int l1;
    int l2;
    int l3;
    int t;

    i = 0u;
    ref_x = 0u;
    ref_y = 0u;
    mb_right = (x & ~15u) + 15u;
    mb_top_y = y & ~15u;
    if (mb_right >= width) {
        mb_right = width - 1u;
    }
    top_left = SIXEL_WEBP_VP8_BORDER_TOP;
    a = 0;
    b = 0;
    c = 0;
    d = 0;
    e = 0;
    f = 0;
    g = 0;
    h = 0;
    l0 = 0;
    l1 = 0;
    l2 = 0;
    l3 = 0;
    t = 0;
    memset(above, SIXEL_WEBP_VP8_BORDER_TOP, sizeof(above));
    memset(left, SIXEL_WEBP_VP8_BORDER_LEFT, sizeof(left));
    memset(pred, 0, 16u);

    for (i = 0u; i < 8u; ++i) {
        ref_x = x + i;
        ref_y = y - 1u;
        if (y != 0u) {
            if ((y & 15u) != 0u && ref_x > mb_right) {
                if (mb_top_y == 0u) {
                    above[i] = SIXEL_WEBP_VP8_BORDER_TOP;
                    continue;
                }
                ref_x = (x & ~15u) + 16u + (ref_x - mb_right - 1u);
                ref_y = mb_top_y - 1u;
            }
            if (ref_x >= width) {
                ref_x = width - 1u;
            }
            above[i] = plane[ref_y * stride + ref_x];
        }
    }
    for (i = 0u; i < 4u; ++i) {
        ref_y = y + i;
        if (ref_y >= height) {
            ref_y = height - 1u;
        }
        if (x != 0u) {
            left[i] = plane[ref_y * stride + (x - 1u)];
        }
    }
    if (x != 0u && y != 0u) {
        top_left = plane[(y - 1u) * stride + (x - 1u)];
    } else if (x == 0u && y != 0u) {
        top_left = SIXEL_WEBP_VP8_BORDER_LEFT;
    } else {
        top_left = SIXEL_WEBP_VP8_BORDER_TOP;
    }

    a = above[0];
    b = above[1];
    c = above[2];
    d = above[3];
    e = above[4];
    f = above[5];
    g = above[6];
    h = above[7];
    l0 = left[0];
    l1 = left[1];
    l2 = left[2];
    l3 = left[3];
    t = top_left;

    switch (bmode) {
    case SIXEL_WEBP_VP8_BMODE_DC:
        i = (unsigned int)(a + b + c + d + l0 + l1 + l2 + l3 + 4) >> 3;
        memset(pred, (int)i, 16u);
        break;
    case SIXEL_WEBP_VP8_BMODE_TM:
        pred[0] = sixel_webp_vp8_clamp_u8(l0 + a - t);
        pred[1] = sixel_webp_vp8_clamp_u8(l0 + b - t);
        pred[2] = sixel_webp_vp8_clamp_u8(l0 + c - t);
        pred[3] = sixel_webp_vp8_clamp_u8(l0 + d - t);
        pred[4] = sixel_webp_vp8_clamp_u8(l1 + a - t);
        pred[5] = sixel_webp_vp8_clamp_u8(l1 + b - t);
        pred[6] = sixel_webp_vp8_clamp_u8(l1 + c - t);
        pred[7] = sixel_webp_vp8_clamp_u8(l1 + d - t);
        pred[8] = sixel_webp_vp8_clamp_u8(l2 + a - t);
        pred[9] = sixel_webp_vp8_clamp_u8(l2 + b - t);
        pred[10] = sixel_webp_vp8_clamp_u8(l2 + c - t);
        pred[11] = sixel_webp_vp8_clamp_u8(l2 + d - t);
        pred[12] = sixel_webp_vp8_clamp_u8(l3 + a - t);
        pred[13] = sixel_webp_vp8_clamp_u8(l3 + b - t);
        pred[14] = sixel_webp_vp8_clamp_u8(l3 + c - t);
        pred[15] = sixel_webp_vp8_clamp_u8(l3 + d - t);
        break;
    case SIXEL_WEBP_VP8_BMODE_VE:
        pred[0] = sixel_webp_vp8_avg3(t, a, b);
        pred[1] = sixel_webp_vp8_avg3(a, b, c);
        pred[2] = sixel_webp_vp8_avg3(b, c, d);
        pred[3] = sixel_webp_vp8_avg3(c, d, e);
        memcpy(pred + 4u, pred, 4u);
        memcpy(pred + 8u, pred, 4u);
        memcpy(pred + 12u, pred, 4u);
        break;
    case SIXEL_WEBP_VP8_BMODE_HE:
        memset(pred + 0u, (int)sixel_webp_vp8_avg3(t, l0, l1), 4u);
        memset(pred + 4u, (int)sixel_webp_vp8_avg3(l0, l1, l2), 4u);
        memset(pred + 8u, (int)sixel_webp_vp8_avg3(l1, l2, l3), 4u);
        memset(pred + 12u, (int)sixel_webp_vp8_avg3(l2, l3, l3), 4u);
        break;
    case SIXEL_WEBP_VP8_BMODE_LD:
        pred[0] = sixel_webp_vp8_avg3(a, b, c);
        pred[1] = pred[4] = sixel_webp_vp8_avg3(b, c, d);
        pred[2] = pred[5] = pred[8] = sixel_webp_vp8_avg3(c, d, e);
        pred[3] = pred[6] = pred[9] = pred[12] =
            sixel_webp_vp8_avg3(d, e, f);
        pred[7] = pred[10] = pred[13] = sixel_webp_vp8_avg3(e, f, g);
        pred[11] = pred[14] = sixel_webp_vp8_avg3(f, g, h);
        pred[15] = sixel_webp_vp8_avg3(g, h, h);
        break;
    case SIXEL_WEBP_VP8_BMODE_RD:
        pred[12] = sixel_webp_vp8_avg3(l1, l2, l3);
        pred[13] = pred[8] = sixel_webp_vp8_avg3(l0, l1, l2);
        pred[14] = pred[9] = pred[4] = sixel_webp_vp8_avg3(t, l0, l1);
        pred[15] = pred[10] = pred[5] = pred[0] =
            sixel_webp_vp8_avg3(a, t, l0);
        pred[11] = pred[6] = pred[1] = sixel_webp_vp8_avg3(b, a, t);
        pred[7] = pred[2] = sixel_webp_vp8_avg3(c, b, a);
        pred[3] = sixel_webp_vp8_avg3(d, c, b);
        break;
    case SIXEL_WEBP_VP8_BMODE_VR:
        pred[0] = pred[9] = sixel_webp_vp8_avg2(t, a);
        pred[1] = pred[10] = sixel_webp_vp8_avg2(a, b);
        pred[2] = pred[11] = sixel_webp_vp8_avg2(b, c);
        pred[3] = sixel_webp_vp8_avg2(c, d);
        pred[12] = sixel_webp_vp8_avg3(l2, l1, l0);
        pred[8] = sixel_webp_vp8_avg3(l1, l0, t);
        pred[4] = pred[13] = sixel_webp_vp8_avg3(l0, t, a);
        pred[5] = pred[14] = sixel_webp_vp8_avg3(t, a, b);
        pred[6] = pred[15] = sixel_webp_vp8_avg3(a, b, c);
        pred[7] = sixel_webp_vp8_avg3(b, c, d);
        break;
    case SIXEL_WEBP_VP8_BMODE_VL:
        pred[0] = sixel_webp_vp8_avg2(a, b);
        pred[1] = pred[8] = sixel_webp_vp8_avg2(b, c);
        pred[2] = pred[9] = sixel_webp_vp8_avg2(c, d);
        pred[3] = pred[10] = sixel_webp_vp8_avg2(d, e);
        pred[11] = sixel_webp_vp8_avg3(e, f, g);
        pred[4] = sixel_webp_vp8_avg3(a, b, c);
        pred[5] = pred[12] = sixel_webp_vp8_avg3(b, c, d);
        pred[6] = pred[13] = sixel_webp_vp8_avg3(c, d, e);
        pred[7] = pred[14] = sixel_webp_vp8_avg3(d, e, f);
        pred[15] = sixel_webp_vp8_avg3(f, g, h);
        break;
    case SIXEL_WEBP_VP8_BMODE_HD:
        pred[0] = pred[6] = sixel_webp_vp8_avg2(l0, t);
        pred[4] = pred[10] = sixel_webp_vp8_avg2(l1, l0);
        pred[8] = pred[14] = sixel_webp_vp8_avg2(l2, l1);
        pred[12] = sixel_webp_vp8_avg2(l3, l2);
        pred[3] = sixel_webp_vp8_avg3(a, b, c);
        pred[2] = sixel_webp_vp8_avg3(t, a, b);
        pred[1] = pred[7] = sixel_webp_vp8_avg3(l0, t, a);
        pred[5] = pred[11] = sixel_webp_vp8_avg3(l1, l0, t);
        pred[9] = pred[15] = sixel_webp_vp8_avg3(l2, l1, l0);
        pred[13] = sixel_webp_vp8_avg3(l3, l2, l1);
        break;
    default:
        pred[0] = sixel_webp_vp8_avg2(l0, l1);
        pred[2] = pred[4] = sixel_webp_vp8_avg2(l1, l2);
        pred[6] = pred[8] = sixel_webp_vp8_avg2(l2, l3);
        pred[1] = sixel_webp_vp8_avg3(l0, l1, l2);
        pred[3] = pred[5] = sixel_webp_vp8_avg3(l1, l2, l3);
        pred[7] = pred[9] = sixel_webp_vp8_avg3(l2, l3, l3);
        pred[15] = pred[14] = pred[13] = pred[12] =
            pred[11] = pred[10] = l3;
        break;
    }
}

static SIXELSTATUS
sixel_webp_vp8_init_token_decoders(
    sixel_webp_vp8_bool_decoder_t *token_decoders,
    unsigned int token_partition_count,
    unsigned char const *payload,
    size_t payload_size,
    sixel_webp_vp8_partition_layout_t const *layout)
{
    SIXELSTATUS status;
    unsigned int i;
    size_t offset;
    size_t partition_size;

    status = SIXEL_OK;
    i = 0u;
    offset = 0u;
    partition_size = 0u;
    if (token_decoders == NULL || token_partition_count == 0u ||
        token_partition_count > SIXEL_WEBP_VP8_MAX_TOKEN_PARTITIONS ||
        payload == NULL || layout == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    offset = layout->token_partition_data_offset;
    for (i = 0u; i < token_partition_count; ++i) {
        partition_size = layout->token_partition_size[i];
        if (partition_size == 0u || offset >= payload_size ||
            partition_size > payload_size - offset) {
            sixel_helper_set_additional_message(
                "builtin webp: VP8 token partition is truncated.");
            return SIXEL_BAD_INPUT;
        }
        status = sixel_webp_vp8_bool_init(token_decoders + i,
                                          payload + offset,
                                          partition_size);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        offset += partition_size;
    }

    return SIXEL_OK;
}

static void
sixel_webp_vp8_copy_block(unsigned char const *pred,
                          unsigned int pred_stride,
                          unsigned char *dst,
                          unsigned int dst_stride,
                          unsigned int width,
                          unsigned int height)
{
    unsigned int x;
    unsigned int y;

    x = 0u;
    y = 0u;
    for (y = 0u; y < height; ++y) {
        for (x = 0u; x < width; ++x) {
            dst[y * dst_stride + x] = pred[y * pred_stride + x];
        }
    }
}

static void
sixel_webp_vp8_idct4x4(int const *input,
                       int *output)
{
    unsigned int i;
    int a1;
    int b1;
    int c1;
    int d1;
    int temp1;
    int temp2;
    int tmp[16];

    i = 0u;
    a1 = 0;
    b1 = 0;
    c1 = 0;
    d1 = 0;
    temp1 = 0;
    temp2 = 0;
    memset(tmp, 0, sizeof(tmp));

    for (i = 0u; i < 4u; ++i) {
        a1 = input[i + 0u] + input[i + 8u];
        b1 = input[i + 0u] - input[i + 8u];
        temp1 = (input[i + 4u] * SIXEL_WEBP_VP8_IDCT_SINPI8SQRT2) >> 16;
        temp2 = input[i + 12u] +
            ((input[i + 12u] * SIXEL_WEBP_VP8_IDCT_COSPI8SQRT2_MINUS1)
             >> 16);
        c1 = temp1 - temp2;
        temp1 = input[i + 4u] +
            ((input[i + 4u] * SIXEL_WEBP_VP8_IDCT_COSPI8SQRT2_MINUS1)
             >> 16);
        temp2 = (input[i + 12u] * SIXEL_WEBP_VP8_IDCT_SINPI8SQRT2) >> 16;
        d1 = temp1 + temp2;
        tmp[i * 4u + 0u] = a1 + d1;
        tmp[i * 4u + 1u] = b1 + c1;
        tmp[i * 4u + 2u] = b1 - c1;
        tmp[i * 4u + 3u] = a1 - d1;
    }

    for (i = 0u; i < 4u; ++i) {
        a1 = tmp[i + 0u] + tmp[i + 8u];
        b1 = tmp[i + 0u] - tmp[i + 8u];
        temp1 = (tmp[i + 4u] * SIXEL_WEBP_VP8_IDCT_SINPI8SQRT2) >> 16;
        temp2 = tmp[i + 12u] +
            ((tmp[i + 12u] * SIXEL_WEBP_VP8_IDCT_COSPI8SQRT2_MINUS1)
             >> 16);
        c1 = temp1 - temp2;
        temp1 = tmp[i + 4u] +
            ((tmp[i + 4u] * SIXEL_WEBP_VP8_IDCT_COSPI8SQRT2_MINUS1)
             >> 16);
        temp2 = (tmp[i + 12u] * SIXEL_WEBP_VP8_IDCT_SINPI8SQRT2) >> 16;
        d1 = temp1 + temp2;
        output[i * 4u + 0u] = (a1 + d1 + 4) >> 3;
        output[i * 4u + 1u] = (b1 + c1 + 4) >> 3;
        output[i * 4u + 2u] = (b1 - c1 + 4) >> 3;
        output[i * 4u + 3u] = (a1 - d1 + 4) >> 3;
    }
}

static void
sixel_webp_vp8_iwht4x4(int const *input,
                       int *output)
{
    unsigned int i;
    int a1;
    int b1;
    int c1;
    int d1;
    int a2;
    int b2;
    int c2;
    int d2;
    int tmp[16];

    i = 0u;
    a1 = 0;
    b1 = 0;
    c1 = 0;
    d1 = 0;
    a2 = 0;
    b2 = 0;
    c2 = 0;
    d2 = 0;
    memset(tmp, 0, sizeof(tmp));
    for (i = 0u; i < 4u; ++i) {
        a1 = input[i + 0u] + input[i + 12u];
        b1 = input[i + 4u] + input[i + 8u];
        c1 = input[i + 4u] - input[i + 8u];
        d1 = input[i + 0u] - input[i + 12u];
        tmp[i + 0u] = a1 + b1;
        tmp[i + 4u] = c1 + d1;
        tmp[i + 8u] = a1 - b1;
        tmp[i + 12u] = d1 - c1;
    }

    for (i = 0u; i < 4u; ++i) {
        a1 = tmp[i * 4u + 0u] + tmp[i * 4u + 3u];
        b1 = tmp[i * 4u + 1u] + tmp[i * 4u + 2u];
        c1 = tmp[i * 4u + 1u] - tmp[i * 4u + 2u];
        d1 = tmp[i * 4u + 0u] - tmp[i * 4u + 3u];
        a2 = a1 + b1;
        b2 = c1 + d1;
        c2 = a1 - b1;
        d2 = d1 - c1;
        output[i * 4u + 0u] = (a2 + 3) >> 3;
        output[i * 4u + 1u] = (b2 + 3) >> 3;
        output[i * 4u + 2u] = (c2 + 3) >> 3;
        output[i * 4u + 3u] = (d2 + 3) >> 3;
    }
}

static void
sixel_webp_vp8_add_residual(unsigned char const *pred,
                            unsigned int pred_stride,
                            unsigned char *dst,
                            unsigned int dst_stride,
                            unsigned int width,
                            unsigned int height,
                            int const *coeff)
{
    unsigned int x;
    unsigned int y;
    int output[16];
    int value;

    x = 0u;
    y = 0u;
    value = 0;
    memset(output, 0, sizeof(output));
    sixel_webp_vp8_idct4x4(coeff, output);
    for (y = 0u; y < height; ++y) {
        for (x = 0u; x < width; ++x) {
            value = output[y * 4u + x] + (int)pred[y * pred_stride + x];
            dst[y * dst_stride + x] = sixel_webp_vp8_clamp_u8(value);
        }
    }
}

static void
sixel_webp_vp8_dequant_block(int16_t const *source,
                             int dc_quant,
                             int ac_quant,
                             int *out)
{
    unsigned int i;

    i = 0u;
    for (i = 0u; i < SIXEL_WEBP_VP8_BLOCK_COEFFS; ++i) {
        if (i == 0u) {
            out[i] = (int)source[i] * dc_quant;
        } else {
            out[i] = (int)source[i] * ac_quant;
        }
    }
}

static SIXELSTATUS
sixel_webp_vp8_decode_coeff_block(
    sixel_webp_vp8_bool_decoder_t *decoder,
    unsigned char const probs[SIXEL_WEBP_VP8_COEFF_BANDS]
                             [SIXEL_WEBP_VP8_PREV_COEFF_CONTEXTS]
                             [SIXEL_WEBP_VP8_COEFF_NODES],
    unsigned int start_coeff,
    unsigned int coeff_context,
    int16_t coeffs[SIXEL_WEBP_VP8_BLOCK_COEFFS],
    unsigned int *peob)
{
    SIXELSTATUS status;
    unsigned int n;
    unsigned int value;
    unsigned int j;
    unsigned int cat;
    unsigned int bit0;
    unsigned int bit1;
    unsigned int k;
    unsigned int cat_len;
    int bit;
    int signed_value;
    unsigned char const *p;
    unsigned int const *cat_prob;

    status = SIXEL_OK;
    n = 0u;
    value = 0u;
    j = 0u;
    cat = 0u;
    bit0 = 0u;
    bit1 = 0u;
    k = 0u;
    cat_len = 0u;
    bit = 0;
    signed_value = 0;
    p = NULL;
    cat_prob = NULL;
    if (decoder == NULL || probs == NULL || coeffs == NULL || peob == NULL ||
        start_coeff > SIXEL_WEBP_VP8_BLOCK_COEFFS ||
        coeff_context > 2u) {
        return SIXEL_BAD_ARGUMENT;
    }

    memset(coeffs, 0, sizeof(int16_t) * SIXEL_WEBP_VP8_BLOCK_COEFFS);
    *peob = start_coeff;
    if (start_coeff >= SIXEL_WEBP_VP8_BLOCK_COEFFS) {
        return SIXEL_OK;
    }

    n = start_coeff;
    p = probs[n][coeff_context];
    while (n < SIXEL_WEBP_VP8_BLOCK_COEFFS) {
        status = sixel_webp_vp8_bool_read(decoder, p[0], &bit);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        if (bit == 0) {
            *peob = n;
            return SIXEL_OK;
        }

        status = sixel_webp_vp8_bool_read(decoder, p[1], &bit);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        while (bit == 0) {
            ++n;
            if (n == SIXEL_WEBP_VP8_BLOCK_COEFFS) {
                *peob = n;
                return SIXEL_OK;
            }
            p = probs[sixel_webp_vp8_coeff_band[n]][0];
            status = sixel_webp_vp8_bool_read(decoder, p[1], &bit);
            if (SIXEL_FAILED(status)) {
                return status;
            }
        }

        status = sixel_webp_vp8_bool_read(decoder, p[2], &bit);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        if (bit == 0) {
            value = 1u;
            p = probs[sixel_webp_vp8_coeff_band[n + 1u]][1];
        } else {
            status = sixel_webp_vp8_bool_read(decoder, p[3], &bit);
            if (SIXEL_FAILED(status)) {
                return status;
            }
            if (bit == 0) {
                status = sixel_webp_vp8_bool_read(decoder, p[4], &bit);
                if (SIXEL_FAILED(status)) {
                    return status;
                }
                if (bit == 0) {
                    value = 2u;
                } else {
                    status = sixel_webp_vp8_bool_read(decoder, p[5], &bit);
                    if (SIXEL_FAILED(status)) {
                        return status;
                    }
                    value = 3u + (unsigned int)bit;
                }
            } else {
                status = sixel_webp_vp8_bool_read(decoder, p[6], &bit);
                if (SIXEL_FAILED(status)) {
                    return status;
                }
                if (bit == 0) {
                    status = sixel_webp_vp8_bool_read(decoder, p[7], &bit);
                    if (SIXEL_FAILED(status)) {
                        return status;
                    }
                    if (bit == 0) {
                        status = sixel_webp_vp8_bool_read(
                            decoder, 159u, &bit);
                        if (SIXEL_FAILED(status)) {
                            return status;
                        }
                        value = 5u + (unsigned int)bit;
                    } else {
                        status = sixel_webp_vp8_bool_read(
                            decoder, 165u, &bit);
                        if (SIXEL_FAILED(status)) {
                            return status;
                        }
                        value = 7u + ((unsigned int)bit << 1);
                        status = sixel_webp_vp8_bool_read(
                            decoder, 145u, &bit);
                        if (SIXEL_FAILED(status)) {
                            return status;
                        }
                        value += (unsigned int)bit;
                    }
                } else {
                    status = sixel_webp_vp8_bool_read(decoder, p[8], &bit);
                    if (SIXEL_FAILED(status)) {
                        return status;
                    }
                    bit1 = (unsigned int)bit;
                    status = sixel_webp_vp8_bool_read(
                        decoder, p[9u + bit1], &bit);
                    if (SIXEL_FAILED(status)) {
                        return status;
                    }
                    bit0 = (unsigned int)bit;
                    cat = (bit1 << 1) + bit0;
                    value = 0u;
                    switch (cat) {
                    case 0u:
                        cat_prob = sixel_webp_vp8_cat3_prob;
                        cat_len = 3u;
                        break;
                    case 1u:
                        cat_prob = sixel_webp_vp8_cat4_prob;
                        cat_len = 4u;
                        break;
                    case 2u:
                        cat_prob = sixel_webp_vp8_cat5_prob;
                        cat_len = 5u;
                        break;
                    default:
                        cat_prob = sixel_webp_vp8_cat6_prob;
                        cat_len = 11u;
                        break;
                    }
                    for (k = 0u; k < cat_len; ++k) {
                        status = sixel_webp_vp8_bool_read(
                            decoder, cat_prob[k], &bit);
                        if (SIXEL_FAILED(status)) {
                            return status;
                        }
                        value = (value << 1) + (unsigned int)bit;
                    }
                    value += 3u + (8u << cat);
                }
            }
            p = probs[sixel_webp_vp8_coeff_band[n + 1u]][2];
        }

        j = sixel_webp_vp8_zigzag[n];
        if (j >= SIXEL_WEBP_VP8_BLOCK_COEFFS) {
            return SIXEL_BAD_INPUT;
        }
        status = sixel_webp_vp8_bool_read_signed_value(decoder,
                                                       value,
                                                       &signed_value);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        coeffs[j] = (int16_t)signed_value;

        ++n;
    }

    *peob = SIXEL_WEBP_VP8_BLOCK_COEFFS;
    return SIXEL_OK;
}

static void
sixel_webp_vp8_build_luma_pred(unsigned int ymode,
                               unsigned int mb_x,
                               unsigned int mb_y,
                               sixel_webp_vp8_planes_t const *planes,
                               sixel_webp_vp8_frame_header_t const *header,
                               unsigned char pred[16 * 16])
{
    unsigned int i;
    unsigned int x;
    unsigned int y;
    unsigned int x0;
    unsigned int y0;
    unsigned int width;
    unsigned int height;
    unsigned char top[16];
    unsigned char left[16];
    unsigned char top_left;
    unsigned int sum_top;
    unsigned int sum_left;
    unsigned int dc;
    int value;

    i = 0u;
    x = 0u;
    y = 0u;
    x0 = mb_x * 16u;
    y0 = mb_y * 16u;
    width = (unsigned int)header->width;
    height = (unsigned int)header->height;
    top_left = SIXEL_WEBP_VP8_BORDER_TOP;
    sum_top = 0u;
    sum_left = 0u;
    dc = 128u;
    value = 0;
    memset(top, SIXEL_WEBP_VP8_BORDER_TOP, sizeof(top));
    memset(left, SIXEL_WEBP_VP8_BORDER_LEFT, sizeof(left));

    for (i = 0u; i < 16u; ++i) {
        x = x0 + i;
        if (x >= width) {
            x = width - 1u;
        }
        y = y0 + i;
        if (y >= height) {
            y = height - 1u;
        }
        if (mb_y != 0u) {
            top[i] = planes->y[(y0 - 1u) * planes->y_stride + x];
        }
        if (mb_x != 0u) {
            left[i] = planes->y[y * planes->y_stride + (x0 - 1u)];
        }
        sum_top += top[i];
        sum_left += left[i];
    }

    if (mb_x != 0u && mb_y != 0u) {
        top_left = planes->y[(y0 - 1u) * planes->y_stride + (x0 - 1u)];
    } else if (mb_x == 0u && mb_y != 0u) {
        top_left = SIXEL_WEBP_VP8_BORDER_LEFT;
    } else {
        top_left = SIXEL_WEBP_VP8_BORDER_TOP;
    }

    switch (ymode) {
    case SIXEL_WEBP_VP8_MODE_DC:
        if (mb_x == 0u && mb_y == 0u) {
            dc = 128u;
        } else if (mb_y == 0u) {
            dc = (sum_left + 8u) >> 4;
        } else if (mb_x == 0u) {
            dc = (sum_top + 8u) >> 4;
        } else {
            dc = (sum_top + sum_left + 16u) >> 5;
        }
        memset(pred, (int)dc, 16u * 16u);
        break;
    case SIXEL_WEBP_VP8_MODE_V:
        for (y = 0u; y < 16u; ++y) {
            for (x = 0u; x < 16u; ++x) {
                pred[y * 16u + x] = top[x];
            }
        }
        break;
    case SIXEL_WEBP_VP8_MODE_H:
        for (y = 0u; y < 16u; ++y) {
            for (x = 0u; x < 16u; ++x) {
                pred[y * 16u + x] = left[y];
            }
        }
        break;
    default:
        for (y = 0u; y < 16u; ++y) {
            for (x = 0u; x < 16u; ++x) {
                value = (int)top[x] + (int)left[y] - (int)top_left;
                pred[y * 16u + x] = sixel_webp_vp8_clamp_u8(value);
            }
        }
        break;
    }
}

static void
sixel_webp_vp8_build_chroma_pred(unsigned int uv_mode,
                                 unsigned int mb_x,
                                 unsigned int mb_y,
                                 unsigned char const *plane,
                                 unsigned int stride,
                                 unsigned int width,
                                 unsigned int height,
                                 unsigned char pred[8 * 8])
{
    unsigned int i;
    unsigned int x;
    unsigned int y;
    unsigned int x0;
    unsigned int y0;
    unsigned char top[8];
    unsigned char left[8];
    unsigned char top_left;
    unsigned int sum_top;
    unsigned int sum_left;
    unsigned int dc;
    int value;

    i = 0u;
    x = 0u;
    y = 0u;
    x0 = mb_x * 8u;
    y0 = mb_y * 8u;
    top_left = SIXEL_WEBP_VP8_BORDER_TOP;
    sum_top = 0u;
    sum_left = 0u;
    dc = 128u;
    value = 0;
    memset(top, SIXEL_WEBP_VP8_BORDER_TOP, sizeof(top));
    memset(left, SIXEL_WEBP_VP8_BORDER_LEFT, sizeof(left));

    for (i = 0u; i < 8u; ++i) {
        x = x0 + i;
        if (x >= width) {
            x = width - 1u;
        }
        y = y0 + i;
        if (y >= height) {
            y = height - 1u;
        }
        if (mb_y != 0u) {
            top[i] = plane[(y0 - 1u) * stride + x];
        }
        if (mb_x != 0u) {
            left[i] = plane[y * stride + (x0 - 1u)];
        }
        sum_top += top[i];
        sum_left += left[i];
    }

    if (mb_x != 0u && mb_y != 0u) {
        top_left = plane[(y0 - 1u) * stride + (x0 - 1u)];
    } else if (mb_x == 0u && mb_y != 0u) {
        top_left = SIXEL_WEBP_VP8_BORDER_LEFT;
    } else {
        top_left = SIXEL_WEBP_VP8_BORDER_TOP;
    }

    switch (uv_mode) {
    case SIXEL_WEBP_VP8_MODE_DC:
        if (mb_x == 0u && mb_y == 0u) {
            dc = 128u;
        } else if (mb_y == 0u) {
            dc = (sum_left + 4u) >> 3;
        } else if (mb_x == 0u) {
            dc = (sum_top + 4u) >> 3;
        } else {
            dc = (sum_top + sum_left + 8u) >> 4;
        }
        memset(pred, (int)dc, 8u * 8u);
        break;
    case SIXEL_WEBP_VP8_MODE_V:
        for (y = 0u; y < 8u; ++y) {
            for (x = 0u; x < 8u; ++x) {
                pred[y * 8u + x] = top[x];
            }
        }
        break;
    case SIXEL_WEBP_VP8_MODE_H:
        for (y = 0u; y < 8u; ++y) {
            for (x = 0u; x < 8u; ++x) {
                pred[y * 8u + x] = left[y];
            }
        }
        break;
    default:
        for (y = 0u; y < 8u; ++y) {
            for (x = 0u; x < 8u; ++x) {
                value = (int)top[x] + (int)left[y] - (int)top_left;
                pred[y * 8u + x] = sixel_webp_vp8_clamp_u8(value);
            }
        }
        break;
    }
}

static void
sixel_webp_vp8_prepare_quant(
    sixel_webp_vp8_frame_context_t const *context,
    unsigned int segment_id,
    sixel_webp_vp8_quant_values_t *quant_values)
{
    int qindex;
    int segment_delta;

    qindex = 0;
    segment_delta = 0;
    if (context == NULL || quant_values == NULL) {
        return;
    }

    qindex = context->quant.y_ac_qi;
    if (context->segment.enabled != 0 && context->segment.update_data != 0 &&
        segment_id < SIXEL_WEBP_VP8_SEGMENTS) {
        segment_delta = context->segment.quant_delta[segment_id];
        if (context->segment.absolute_delta != 0) {
            qindex = segment_delta;
        } else {
            qindex += segment_delta;
        }
    }
    qindex = sixel_webp_vp8_clamp_qindex(qindex);
    quant_values->y1_dc =
        sixel_webp_vp8_get_dc_quant(qindex, context->quant.y_dc_delta);
    quant_values->y1_ac = sixel_webp_vp8_get_ac_quant(qindex, 0);
    quant_values->y2_dc =
        sixel_webp_vp8_get_dc_quant(qindex, context->quant.y2_dc_delta) * 2;
    quant_values->y2_ac =
        sixel_webp_vp8_get_ac2_quant(qindex, context->quant.y2_ac_delta);
    quant_values->uv_dc =
        sixel_webp_vp8_get_dc_quant(qindex, context->quant.uv_dc_delta);
    if (quant_values->uv_dc > 132) {
        quant_values->uv_dc = 132;
    }
    quant_values->uv_ac =
        sixel_webp_vp8_get_ac_quant(qindex, context->quant.uv_ac_delta);
}

static SIXELSTATUS
sixel_webp_vp8_decode_native_intra(
    sixel_webp_vp8_bool_decoder_t *token_decoders,
    sixel_webp_vp8_frame_header_t const *header,
    sixel_webp_vp8_frame_context_t *context,
    sixel_webp_vp8_planes_t *planes,
    sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    unsigned char *above_y;
    unsigned char *above_u;
    unsigned char *above_v;
    unsigned char *above_y2;
    unsigned char *above_mb_mode;
    unsigned char *above_bottom_bmode;
    unsigned char left_y[4];
    unsigned char left_u[2];
    unsigned char left_v[2];
    unsigned char left_y2;
    unsigned char left_mb_mode;
    unsigned char left_right_bmode[4];
    unsigned int mb_x;
    unsigned int mb_y;
    unsigned int block;
    unsigned int row;
    unsigned int col;
    unsigned int segment_id;
    unsigned int ymode;
    unsigned int uv_mode;
    unsigned int above_mode;
    unsigned int left_mode;
    unsigned int y_start_coeff;
    unsigned int b_modes[16];
    unsigned int coeff_context;
    unsigned int eob;
    unsigned int x0;
    unsigned int y0;
    unsigned int width;
    unsigned int height;
    unsigned int uv_width;
    unsigned int uv_height;
    unsigned int dst_x;
    unsigned int dst_y;
    unsigned int copy_w;
    unsigned int copy_h;
    unsigned int nz_y;
    unsigned int nz_u;
    unsigned int nz_v;
    unsigned int nz_y2;
    int bit;
    int skip_coeff;
    int y2_output[16];
    int dequant[16];
    int16_t coeffs[SIXEL_WEBP_VP8_BLOCKS_MB][SIXEL_WEBP_VP8_BLOCK_COEFFS];
    sixel_webp_vp8_quant_values_t quant_values;
    sixel_webp_vp8_bool_decoder_t *mode_decoder;
    sixel_webp_vp8_bool_decoder_t *token_decoder;
    unsigned char y_pred[16 * 16];
    unsigned char u_pred[8 * 8];
    unsigned char v_pred[8 * 8];
    unsigned char block_pred[16];
    size_t above_y_size;
    size_t above_u_size;
    size_t above_v_size;
    size_t above_y2_size;
    size_t above_mb_mode_size;
    size_t above_bottom_bmode_size;
    unsigned int total_mb;
    unsigned int total_nz_y;
    unsigned int total_nz_u;
    unsigned int total_nz_v;
    unsigned int total_nz_y2;
    unsigned int ymode_hist[5];
    unsigned int uv_hist[4];
    unsigned int bmode_hist[10];

    status = SIXEL_OK;
    above_y = NULL;
    above_u = NULL;
    above_v = NULL;
    above_y2 = NULL;
    above_mb_mode = NULL;
    above_bottom_bmode = NULL;
    memset(left_y, 0, sizeof(left_y));
    memset(left_u, 0, sizeof(left_u));
    memset(left_v, 0, sizeof(left_v));
    left_y2 = 0u;
    left_mb_mode = SIXEL_WEBP_VP8_MODE_DC;
    memset(left_right_bmode, SIXEL_WEBP_VP8_BMODE_DC,
           sizeof(left_right_bmode));
    mb_x = 0u;
    mb_y = 0u;
    block = 0u;
    row = 0u;
    col = 0u;
    segment_id = 0u;
    ymode = 0u;
    uv_mode = 0u;
    above_mode = 0u;
    left_mode = 0u;
    y_start_coeff = 0u;
    memset(b_modes, 0, sizeof(b_modes));
    coeff_context = 0u;
    eob = 0u;
    x0 = 0u;
    y0 = 0u;
    width = 0u;
    height = 0u;
    uv_width = 0u;
    uv_height = 0u;
    dst_x = 0u;
    dst_y = 0u;
    copy_w = 0u;
    copy_h = 0u;
    nz_y = 0u;
    nz_u = 0u;
    nz_v = 0u;
    nz_y2 = 0u;
    bit = 0;
    skip_coeff = 0;
    memset(y2_output, 0, sizeof(y2_output));
    memset(dequant, 0, sizeof(dequant));
    memset(coeffs, 0, sizeof(coeffs));
    memset(&quant_values, 0, sizeof(quant_values));
    mode_decoder = NULL;
    token_decoder = NULL;
    memset(y_pred, 0, sizeof(y_pred));
    memset(u_pred, 0, sizeof(u_pred));
    memset(v_pred, 0, sizeof(v_pred));
    memset(block_pred, 0, sizeof(block_pred));
    above_y_size = 0u;
    above_u_size = 0u;
    above_v_size = 0u;
    above_y2_size = 0u;
    above_mb_mode_size = 0u;
    above_bottom_bmode_size = 0u;
    total_mb = 0u;
    total_nz_y = 0u;
    total_nz_u = 0u;
    total_nz_v = 0u;
    total_nz_y2 = 0u;
    memset(ymode_hist, 0, sizeof(ymode_hist));
    memset(uv_hist, 0, sizeof(uv_hist));
    memset(bmode_hist, 0, sizeof(bmode_hist));
    if (token_decoders == NULL || header == NULL || context == NULL ||
        planes == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    mode_decoder = &context->mode_decoder;
    token_decoder = &token_decoders[0];
    width = (unsigned int)header->width;
    height = (unsigned int)header->height;
    uv_width = planes->uv_width;
    uv_height = planes->uv_height;
    above_y_size = (size_t)context->mb_cols * 4u;
    above_u_size = (size_t)context->mb_cols * 2u;
    above_v_size = (size_t)context->mb_cols * 2u;
    above_y2_size = (size_t)context->mb_cols;
    above_mb_mode_size = (size_t)context->mb_cols;
    above_bottom_bmode_size = (size_t)context->mb_cols * 4u;

    above_y = (unsigned char *)sixel_allocator_malloc(allocator, above_y_size);
    above_u = (unsigned char *)sixel_allocator_malloc(allocator, above_u_size);
    above_v = (unsigned char *)sixel_allocator_malloc(allocator, above_v_size);
    above_y2 =
        (unsigned char *)sixel_allocator_malloc(allocator, above_y2_size);
    above_mb_mode = (unsigned char *)sixel_allocator_malloc(
        allocator, above_mb_mode_size);
    above_bottom_bmode = (unsigned char *)sixel_allocator_malloc(
        allocator, above_bottom_bmode_size);
    if (above_y == NULL || above_u == NULL || above_v == NULL ||
        above_y2 == NULL || above_mb_mode == NULL ||
        above_bottom_bmode == NULL) {
        sixel_helper_set_additional_message(
            "builtin webp: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto cleanup;
    }
    memset(above_y, 0, above_y_size);
    memset(above_u, 0, above_u_size);
    memset(above_v, 0, above_v_size);
    memset(above_y2, 0, above_y2_size);
    memset(above_mb_mode, SIXEL_WEBP_VP8_MODE_DC, above_mb_mode_size);
    memset(above_bottom_bmode,
           SIXEL_WEBP_VP8_BMODE_DC,
           above_bottom_bmode_size);

    for (mb_y = 0u; mb_y < context->mb_rows; ++mb_y) {
        memset(left_y, 0, sizeof(left_y));
        memset(left_u, 0, sizeof(left_u));
        memset(left_v, 0, sizeof(left_v));
        left_y2 = 0u;
        left_mb_mode = SIXEL_WEBP_VP8_MODE_DC;
        memset(left_right_bmode,
               SIXEL_WEBP_VP8_BMODE_DC,
               sizeof(left_right_bmode));
        for (mb_x = 0u; mb_x < context->mb_cols; ++mb_x) {
            segment_id = 0u;
            ymode = SIXEL_WEBP_VP8_MODE_DC;
            uv_mode = SIXEL_WEBP_VP8_MODE_DC;
            skip_coeff = 0;
            nz_y = 0u;
            nz_u = 0u;
            nz_v = 0u;
            nz_y2 = 0u;
            memset(coeffs, 0, sizeof(coeffs));
            memset(y2_output, 0, sizeof(y2_output));
            memset(b_modes, 0, sizeof(b_modes));

            status = sixel_webp_vp8_read_segment_id(
                mode_decoder, &context->segment, &segment_id);
            if (SIXEL_FAILED(status)) {
                goto cleanup;
            }

            if (context->entropy.mb_no_coeff_skip != 0) {
                status = sixel_webp_vp8_bool_read(
                    mode_decoder,
                    (unsigned int)context->entropy.prob_skip_false,
                    &bit);
                if (SIXEL_FAILED(status)) {
                    goto cleanup;
                }
                skip_coeff = bit;
            }

            status = sixel_webp_vp8_read_kf_ymode(mode_decoder, &ymode);
            if (SIXEL_FAILED(status)) {
                goto cleanup;
            }
            if (ymode == SIXEL_WEBP_VP8_MODE_B) {
                for (block = 0u; block < SIXEL_WEBP_VP8_BLOCKS_Y; ++block) {
                    row = block >> 2u;
                    col = block & 3u;
                    if (row == 0u) {
                        if (mb_y == 0u) {
                            above_mode = SIXEL_WEBP_VP8_BMODE_DC;
                        } else if (above_mb_mode[mb_x] ==
                                   SIXEL_WEBP_VP8_MODE_B) {
                            above_mode = (unsigned int)
                                above_bottom_bmode[mb_x * 4u + col];
                        } else {
                            above_mode = sixel_webp_vp8_map_ymode_to_bmode(
                                (unsigned int)above_mb_mode[mb_x]);
                        }
                    } else {
                        above_mode = b_modes[block - 4u];
                    }
                    if (col == 0u) {
                        if (mb_x == 0u) {
                            left_mode = SIXEL_WEBP_VP8_BMODE_DC;
                        } else if (left_mb_mode == SIXEL_WEBP_VP8_MODE_B) {
                            left_mode = (unsigned int)left_right_bmode[row];
                        } else {
                            left_mode = sixel_webp_vp8_map_ymode_to_bmode(
                                (unsigned int)left_mb_mode);
                        }
                    } else {
                        left_mode = b_modes[block - 1u];
                    }
                    status = sixel_webp_vp8_read_bmode(
                        mode_decoder,
                        sixel_webp_vp8_kf_bmode_prob[above_mode][left_mode],
                        &b_modes[block]);
                    if (SIXEL_FAILED(status)) {
                        goto cleanup;
                    }
                }
            }

            status = sixel_webp_vp8_read_uv_mode(mode_decoder, &uv_mode);
            if (SIXEL_FAILED(status)) {
                goto cleanup;
            }
            if (ymode < 5u) {
                ymode_hist[ymode]++;
            }
            if (uv_mode < 4u) {
                uv_hist[uv_mode]++;
            }
            if (ymode == SIXEL_WEBP_VP8_MODE_B) {
                for (block = 0u; block < SIXEL_WEBP_VP8_BLOCKS_Y; ++block) {
                    if (b_modes[block] < 10u) {
                        bmode_hist[b_modes[block]]++;
                    }
                }
            }

            sixel_webp_vp8_prepare_quant(context, segment_id, &quant_values);

            if (skip_coeff == 0) {
                if (ymode != SIXEL_WEBP_VP8_MODE_B) {
                    coeff_context = (unsigned int)above_y2[mb_x] +
                                    (unsigned int)left_y2;
                    if (coeff_context > 2u) {
                        coeff_context = 2u;
                    }
                    status = sixel_webp_vp8_decode_coeff_block(
                        token_decoder,
                        context->entropy
                            .coef_probs[SIXEL_WEBP_VP8_COEFF_TYPE_Y2],
                        0u,
                        coeff_context,
                        coeffs[24],
                        &eob);
                    if (SIXEL_FAILED(status)) {
                        goto cleanup;
                    }
                    above_y2[mb_x] = (eob != 0u) ? 1u : 0u;
                    if (eob != 0u) {
                        nz_y2 = 1u;
                    }
                    left_y2 = above_y2[mb_x];
                }

                y_start_coeff = (ymode == SIXEL_WEBP_VP8_MODE_B) ? 0u : 1u;
                for (block = 0u; block < SIXEL_WEBP_VP8_BLOCKS_Y; ++block) {
                    coeff_context = (unsigned int)above_y[mb_x * 4u +
                                                          (block & 3u)] +
                                    (unsigned int)left_y[block >> 2u];
                    if (coeff_context > 2u) {
                        coeff_context = 2u;
                    }
                    status = sixel_webp_vp8_decode_coeff_block(
                        token_decoder,
                        context->entropy.coef_probs[(ymode ==
                                                     SIXEL_WEBP_VP8_MODE_B) ?
                                SIXEL_WEBP_VP8_COEFF_TYPE_Y1_B :
                                SIXEL_WEBP_VP8_COEFF_TYPE_Y1],
                        y_start_coeff,
                        coeff_context,
                        coeffs[block],
                        &eob);
                    if (SIXEL_FAILED(status)) {
                        goto cleanup;
                    }
                    above_y[mb_x * 4u + (block & 3u)] =
                        (eob != y_start_coeff) ? 1u : 0u;
                    left_y[block >> 2u] = (eob != y_start_coeff) ? 1u : 0u;
                    if (eob != y_start_coeff) {
                        ++nz_y;
                    }
                }

                for (block = 0u; block < SIXEL_WEBP_VP8_BLOCKS_UV; ++block) {
                    coeff_context = (unsigned int)above_u[mb_x * 2u +
                                                          (block & 1u)] +
                                    (unsigned int)left_u[block >> 1u];
                    if (coeff_context > 2u) {
                        coeff_context = 2u;
                    }
                    status = sixel_webp_vp8_decode_coeff_block(
                        token_decoder,
                        context->entropy.coef_probs
                            [SIXEL_WEBP_VP8_COEFF_TYPE_UV],
                        0u,
                        coeff_context,
                        coeffs[16u + block],
                        &eob);
                    if (SIXEL_FAILED(status)) {
                        goto cleanup;
                    }
                    above_u[mb_x * 2u + (block & 1u)] = (eob != 0u) ? 1u : 0u;
                    left_u[block >> 1u] = (eob != 0u) ? 1u : 0u;
                    if (eob != 0u) {
                        ++nz_u;
                    }
                }

                for (block = 0u; block < SIXEL_WEBP_VP8_BLOCKS_UV; ++block) {
                    coeff_context = (unsigned int)above_v[mb_x * 2u +
                                                          (block & 1u)] +
                                    (unsigned int)left_v[block >> 1u];
                    if (coeff_context > 2u) {
                        coeff_context = 2u;
                    }
                    status = sixel_webp_vp8_decode_coeff_block(
                        token_decoder,
                        context->entropy.coef_probs
                            [SIXEL_WEBP_VP8_COEFF_TYPE_UV],
                        0u,
                        coeff_context,
                        coeffs[20u + block],
                        &eob);
                    if (SIXEL_FAILED(status)) {
                        goto cleanup;
                    }
                    above_v[mb_x * 2u + (block & 1u)] = (eob != 0u) ? 1u : 0u;
                    left_v[block >> 1u] = (eob != 0u) ? 1u : 0u;
                    if (eob != 0u) {
                        ++nz_v;
                    }
                }
            } else {
                if (ymode != SIXEL_WEBP_VP8_MODE_B) {
                    above_y2[mb_x] = 0u;
                    left_y2 = 0u;
                }
                for (block = 0u; block < 4u; ++block) {
                    above_y[mb_x * 4u + block] = 0u;
                    left_y[block] = 0u;
                }
                for (block = 0u; block < 2u; ++block) {
                    above_u[mb_x * 2u + block] = 0u;
                    above_v[mb_x * 2u + block] = 0u;
                    left_u[block] = 0u;
                    left_v[block] = 0u;
                }
            }
            if (mb_x < 2u && mb_y < 2u &&
                sixel_trace_topic_is_enabled("webp_decode") != 0) {
                sixel_trace_topic_message(
                    "webp_decode",
                    "LSXWEBPDBG|diag=VP8MB|x=%u|y=%u|seg=%u|ym=%u|uv=%u|"
                    "skip=%d|nzy2=%u|nzy=%u|nzu=%u|nzv=%u",
                    mb_x,
                    mb_y,
                    segment_id,
                    ymode,
                    uv_mode,
                    skip_coeff,
                    nz_y2,
                    nz_y,
                    nz_u,
                    nz_v);
            }
            ++total_mb;
            total_nz_y += nz_y;
            total_nz_u += nz_u;
            total_nz_v += nz_v;
            total_nz_y2 += nz_y2;

            x0 = mb_x * 16u;
            y0 = mb_y * 16u;
            if (ymode != SIXEL_WEBP_VP8_MODE_B) {
                sixel_webp_vp8_build_luma_pred(
                    ymode, mb_x, mb_y, planes, header, y_pred);
                if (skip_coeff != 0) {
                    for (block = 0u; block < SIXEL_WEBP_VP8_BLOCKS_Y;
                         ++block) {
                        dst_x = x0 + (block & 3u) * 4u;
                        dst_y = y0 + (block >> 2u) * 4u;
                        if (dst_x >= width || dst_y >= height) {
                            continue;
                        }
                        copy_w = 4u;
                        copy_h = 4u;
                        if (copy_w > width - dst_x) {
                            copy_w = width - dst_x;
                        }
                        if (copy_h > height - dst_y) {
                            copy_h = height - dst_y;
                        }
                        sixel_webp_vp8_copy_block(
                            y_pred + ((block >> 2u) * 4u) * 16u +
                                (block & 3u) * 4u,
                            16u,
                            planes->y + dst_y * planes->y_stride + dst_x,
                            planes->y_stride,
                            copy_w,
                            copy_h);
                    }
                } else {
                    sixel_webp_vp8_dequant_block(coeffs[24],
                                                 quant_values.y2_dc,
                                                 quant_values.y2_ac,
                                                 dequant);
                    sixel_webp_vp8_iwht4x4(dequant, y2_output);
                    for (block = 0u; block < SIXEL_WEBP_VP8_BLOCKS_Y;
                         ++block) {
                        dst_x = x0 + (block & 3u) * 4u;
                        dst_y = y0 + (block >> 2u) * 4u;
                        if (dst_x >= width || dst_y >= height) {
                            continue;
                        }
                        copy_w = 4u;
                        copy_h = 4u;
                        if (copy_w > width - dst_x) {
                            copy_w = width - dst_x;
                        }
                        if (copy_h > height - dst_y) {
                            copy_h = height - dst_y;
                        }
                        sixel_webp_vp8_dequant_block(coeffs[block],
                                                     quant_values.y1_dc,
                                                     quant_values.y1_ac,
                                                     dequant);
                        dequant[0] += y2_output[block];
                        sixel_webp_vp8_add_residual(
                            y_pred + ((block >> 2u) * 4u) * 16u +
                                (block & 3u) * 4u,
                            16u,
                            planes->y + dst_y * planes->y_stride + dst_x,
                            planes->y_stride,
                            copy_w,
                            copy_h,
                            dequant);
                    }
                }
            } else {
                for (block = 0u; block < SIXEL_WEBP_VP8_BLOCKS_Y; ++block) {
                    dst_x = x0 + (block & 3u) * 4u;
                    dst_y = y0 + (block >> 2u) * 4u;
                    if (dst_x >= width || dst_y >= height) {
                        continue;
                    }
                    copy_w = 4u;
                    copy_h = 4u;
                    if (copy_w > width - dst_x) {
                        copy_w = width - dst_x;
                    }
                    if (copy_h > height - dst_y) {
                        copy_h = height - dst_y;
                    }
                    sixel_webp_vp8_predict_bmode(b_modes[block],
                                                 planes->y,
                                                 planes->y_stride,
                                                 width,
                                                 height,
                                                 dst_x,
                                                 dst_y,
                                                 block_pred);
                    if (skip_coeff != 0) {
                        sixel_webp_vp8_copy_block(block_pred,
                                                  4u,
                                                  planes->y +
                                                      dst_y *
                                                      planes->y_stride + dst_x,
                                                  planes->y_stride,
                                                  copy_w,
                                                  copy_h);
                    } else {
                        sixel_webp_vp8_dequant_block(coeffs[block],
                                                     quant_values.y1_dc,
                                                     quant_values.y1_ac,
                                                     dequant);
                        sixel_webp_vp8_add_residual(
                            block_pred,
                            4u,
                            planes->y +
                                dst_y * planes->y_stride + dst_x,
                            planes->y_stride,
                            copy_w,
                            copy_h,
                            dequant);
                    }
                }
            }

            sixel_webp_vp8_build_chroma_pred(uv_mode,
                                             mb_x,
                                             mb_y,
                                             planes->u,
                                             planes->uv_stride,
                                             uv_width,
                                             uv_height,
                                             u_pred);
            sixel_webp_vp8_build_chroma_pred(uv_mode,
                                             mb_x,
                                             mb_y,
                                             planes->v,
                                             planes->uv_stride,
                                             uv_width,
                                             uv_height,
                                             v_pred);
            x0 = mb_x * 8u;
            y0 = mb_y * 8u;
            for (block = 0u; block < SIXEL_WEBP_VP8_BLOCKS_UV; ++block) {
                dst_x = x0 + (block & 1u) * 4u;
                dst_y = y0 + (block >> 1u) * 4u;
                if (dst_x >= uv_width || dst_y >= uv_height) {
                    continue;
                }
                copy_w = 4u;
                copy_h = 4u;
                if (copy_w > uv_width - dst_x) {
                    copy_w = uv_width - dst_x;
                }
                if (copy_h > uv_height - dst_y) {
                    copy_h = uv_height - dst_y;
                }
                if (skip_coeff != 0) {
                    sixel_webp_vp8_copy_block(
                        u_pred + ((block >> 1u) * 4u) * 8u +
                            (block & 1u) * 4u,
                        8u,
                        planes->u + dst_y * planes->uv_stride + dst_x,
                        planes->uv_stride,
                        copy_w,
                        copy_h);
                    sixel_webp_vp8_copy_block(
                        v_pred + ((block >> 1u) * 4u) * 8u +
                            (block & 1u) * 4u,
                        8u,
                        planes->v + dst_y * planes->uv_stride + dst_x,
                        planes->uv_stride,
                        copy_w,
                        copy_h);
                } else {
                    sixel_webp_vp8_dequant_block(coeffs[16u + block],
                                                 quant_values.uv_dc,
                                                 quant_values.uv_ac,
                                                 dequant);
                    sixel_webp_vp8_add_residual(
                        u_pred + ((block >> 1u) * 4u) * 8u +
                            (block & 1u) * 4u,
                        8u,
                        planes->u + dst_y * planes->uv_stride + dst_x,
                        planes->uv_stride,
                        copy_w,
                        copy_h,
                        dequant);
                    sixel_webp_vp8_dequant_block(coeffs[20u + block],
                                                 quant_values.uv_dc,
                                                 quant_values.uv_ac,
                                                 dequant);
                    sixel_webp_vp8_add_residual(
                        v_pred + ((block >> 1u) * 4u) * 8u +
                            (block & 1u) * 4u,
                        8u,
                        planes->v + dst_y * planes->uv_stride + dst_x,
                        planes->uv_stride,
                        copy_w,
                        copy_h,
                        dequant);
                }
            }

            above_mb_mode[mb_x] = (unsigned char)ymode;
            if (ymode == SIXEL_WEBP_VP8_MODE_B) {
                above_bottom_bmode[mb_x * 4u + 0u] =
                    (unsigned char)b_modes[12u];
                above_bottom_bmode[mb_x * 4u + 1u] =
                    (unsigned char)b_modes[13u];
                above_bottom_bmode[mb_x * 4u + 2u] =
                    (unsigned char)b_modes[14u];
                above_bottom_bmode[mb_x * 4u + 3u] =
                    (unsigned char)b_modes[15u];
                left_right_bmode[0] = (unsigned char)b_modes[3u];
                left_right_bmode[1] = (unsigned char)b_modes[7u];
                left_right_bmode[2] = (unsigned char)b_modes[11u];
                left_right_bmode[3] = (unsigned char)b_modes[15u];
            } else {
                left_right_bmode[0] = (unsigned char)
                    sixel_webp_vp8_map_ymode_to_bmode(ymode);
                left_right_bmode[1] = left_right_bmode[0];
                left_right_bmode[2] = left_right_bmode[0];
                left_right_bmode[3] = left_right_bmode[0];
            }
            left_mb_mode = (unsigned char)ymode;
        }
    }
    if (sixel_trace_topic_is_enabled("webp_decode") != 0) {
        sixel_trace_topic_message(
            "webp_decode",
            "LSXWEBPDBG|diag=VP8SUM|mb=%u|nzy=%u|nzu=%u|nzv=%u|nzy2=%u",
            total_mb,
            total_nz_y,
            total_nz_u,
            total_nz_v,
            total_nz_y2);
        sixel_trace_topic_message(
            "webp_decode",
            "LSXWEBPDBG|diag=VP8MODE|y=%u,%u,%u,%u,%u|uv=%u,%u,%u,%u|"
            "b=%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",
            ymode_hist[0],
            ymode_hist[1],
            ymode_hist[2],
            ymode_hist[3],
            ymode_hist[4],
            uv_hist[0],
            uv_hist[1],
            uv_hist[2],
            uv_hist[3],
            bmode_hist[0],
            bmode_hist[1],
            bmode_hist[2],
            bmode_hist[3],
            bmode_hist[4],
            bmode_hist[5],
            bmode_hist[6],
            bmode_hist[7],
            bmode_hist[8],
            bmode_hist[9]);
    }

cleanup:
    sixel_allocator_free(allocator, above_y);
    sixel_allocator_free(allocator, above_u);
    sixel_allocator_free(allocator, above_v);
    sixel_allocator_free(allocator, above_y2);
    sixel_allocator_free(allocator, above_mb_mode);
    sixel_allocator_free(allocator, above_bottom_bmode);
    return status;
}

static SIXELSTATUS
sixel_webp_vp8_convert_yuv420_to_rgba(
    sixel_webp_vp8_planes_t const *planes,
    sixel_webp_vp8_frame_header_t const *header,
    unsigned char **prgba,
    sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    unsigned char *rgba;
    unsigned int x;
    unsigned int y;
    unsigned int uv_x;
    unsigned int uv_y;
    unsigned int width;
    unsigned int height;
    size_t pixel_count;
    size_t rgba_size;
    int yv;
    int u;
    int v;
    int r;
    int g;
    int b;

    status = SIXEL_OK;
    rgba = NULL;
    x = 0u;
    y = 0u;
    uv_x = 0u;
    uv_y = 0u;
    width = 0u;
    height = 0u;
    pixel_count = 0u;
    rgba_size = 0u;
    yv = 0;
    u = 0;
    v = 0;
    r = 0;
    g = 0;
    b = 0;
    if (planes == NULL || header == NULL || prgba == NULL ||
        allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    width = (unsigned int)header->width;
    height = (unsigned int)header->height;
    if ((size_t)width > SIZE_MAX / (size_t)height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_count = (size_t)width * (size_t)height;
    if (pixel_count > SIZE_MAX / 4u) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    rgba_size = pixel_count * 4u;
    rgba = (unsigned char *)sixel_allocator_malloc(allocator, rgba_size);
    if (rgba == NULL) {
        sixel_helper_set_additional_message(
            "builtin webp: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    for (y = 0u; y < height; ++y) {
        uv_y = y >> 1;
        for (x = 0u; x < width; ++x) {
            uv_x = x >> 1;
            yv = (int)planes->y[y * planes->y_stride + x];
            u = (int)planes->u[uv_y * planes->uv_stride + uv_x];
            v = (int)planes->v[uv_y * planes->uv_stride + uv_x];
            r = ((yv * 19077) >> 8) + ((v * 26149) >> 8) - 14234;
            g = ((yv * 19077) >> 8) - ((u * 6419) >> 8) -
                ((v * 13320) >> 8) + 8708;
            b = ((yv * 19077) >> 8) + ((u * 33050) >> 8) - 17685;
            rgba[((size_t)y * width + x) * 4u + 0u] =
                sixel_webp_vp8_clip_yuv2(r);
            rgba[((size_t)y * width + x) * 4u + 1u] =
                sixel_webp_vp8_clip_yuv2(g);
            rgba[((size_t)y * width + x) * 4u + 2u] =
                sixel_webp_vp8_clip_yuv2(b);
            rgba[((size_t)y * width + x) * 4u + 3u] = 255u;
        }
    }

    *prgba = rgba;
    rgba = NULL;

    sixel_allocator_free(allocator, rgba);
    return status;
}

static SIXELSTATUS
sixel_webp_vp8_bool_read_signed_literal(
    sixel_webp_vp8_bool_decoder_t *decoder,
    unsigned int nbits,
    int *pvalue)
{
    SIXELSTATUS status;
    unsigned int value;
    int sign;

    status = SIXEL_OK;
    value = 0u;
    sign = 0;
    if (decoder == NULL || pvalue == NULL || nbits > 31u) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_webp_vp8_bool_read_literal(decoder, nbits, &value);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (value == 0u) {
        *pvalue = 0;
        return SIXEL_OK;
    }

    status = sixel_webp_vp8_bool_read(decoder,
                                      SIXEL_WEBP_VP8_BOOL_BASE_PROB,
                                      &sign);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (sign != 0) {
        *pvalue = -(int)value;
    } else {
        *pvalue = (int)value;
    }

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_webp_vp8_parse_optional_signed(
    sixel_webp_vp8_bool_decoder_t *decoder,
    unsigned int nbits,
    int *pvalue)
{
    SIXELSTATUS status;
    int bit;

    status = SIXEL_OK;
    bit = 0;
    if (decoder == NULL || pvalue == NULL || nbits > 31u) {
        return SIXEL_BAD_ARGUMENT;
    }

    *pvalue = 0;
    status = sixel_webp_vp8_bool_read(decoder,
                                      SIXEL_WEBP_VP8_BOOL_BASE_PROB,
                                      &bit);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (bit == 0) {
        return SIXEL_OK;
    }

    return sixel_webp_vp8_bool_read_signed_literal(decoder, nbits, pvalue);
}

static SIXELSTATUS
sixel_webp_vp8_parse_segment_header(
    sixel_webp_vp8_bool_decoder_t *decoder,
    sixel_webp_vp8_segment_header_t *segment)
{
    SIXELSTATUS status;
    unsigned int i;
    unsigned int value;
    int bit;

    status = SIXEL_OK;
    i = 0u;
    value = 255u;
    bit = 0;
    if (decoder == NULL || segment == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    memset(segment, 0, sizeof(*segment));
    for (i = 0u; i < 3u; ++i) {
        segment->map_prob[i] = 255;
    }

    status = sixel_webp_vp8_bool_read(decoder,
                                      SIXEL_WEBP_VP8_BOOL_BASE_PROB,
                                      &bit);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    segment->enabled = bit;
    if (segment->enabled == 0) {
        return SIXEL_OK;
    }

    status = sixel_webp_vp8_bool_read(decoder,
                                      SIXEL_WEBP_VP8_BOOL_BASE_PROB,
                                      &bit);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    segment->update_map = bit;

    status = sixel_webp_vp8_bool_read(decoder,
                                      SIXEL_WEBP_VP8_BOOL_BASE_PROB,
                                      &bit);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    segment->update_data = bit;

    if (segment->update_data != 0) {
        status = sixel_webp_vp8_bool_read(decoder,
                                          SIXEL_WEBP_VP8_BOOL_BASE_PROB,
                                          &bit);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        segment->absolute_delta = bit;

        for (i = 0u; i < SIXEL_WEBP_VP8_SEGMENTS; ++i) {
            status = sixel_webp_vp8_parse_optional_signed(
                decoder, 7u, &segment->quant_delta[i]);
            if (SIXEL_FAILED(status)) {
                return status;
            }
        }
        for (i = 0u; i < SIXEL_WEBP_VP8_SEGMENTS; ++i) {
            status = sixel_webp_vp8_parse_optional_signed(
                decoder, 6u, &segment->filter_delta[i]);
            if (SIXEL_FAILED(status)) {
                return status;
            }
        }
    }

    if (segment->update_map != 0) {
        for (i = 0u; i < 3u; ++i) {
            status = sixel_webp_vp8_bool_read(
                decoder, SIXEL_WEBP_VP8_BOOL_BASE_PROB, &bit);
            if (SIXEL_FAILED(status)) {
                return status;
            }
            if (bit != 0) {
                status = sixel_webp_vp8_bool_read_literal(decoder,
                                                          8u,
                                                          &value);
                if (SIXEL_FAILED(status)) {
                    return status;
                }
                segment->map_prob[i] = (int)value;
            }
        }
    }

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_webp_vp8_parse_filter_header(
    sixel_webp_vp8_bool_decoder_t *decoder,
    sixel_webp_vp8_filter_header_t *filter)
{
    SIXELSTATUS status;
    unsigned int i;
    unsigned int value;
    int bit;

    status = SIXEL_OK;
    i = 0u;
    value = 0u;
    bit = 0;
    if (decoder == NULL || filter == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    memset(filter, 0, sizeof(*filter));
    status = sixel_webp_vp8_bool_read_literal(decoder, 1u, &value);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    filter->simple = (int)value;

    status = sixel_webp_vp8_bool_read_literal(decoder, 6u, &value);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    filter->level = value;

    status = sixel_webp_vp8_bool_read_literal(decoder, 3u, &value);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    filter->sharpness = value;

    status = sixel_webp_vp8_bool_read(decoder,
                                      SIXEL_WEBP_VP8_BOOL_BASE_PROB,
                                      &bit);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (bit == 0) {
        return SIXEL_OK;
    }

    status = sixel_webp_vp8_bool_read(decoder,
                                      SIXEL_WEBP_VP8_BOOL_BASE_PROB,
                                      &bit);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    filter->update_delta = bit;
    if (filter->update_delta == 0) {
        return SIXEL_OK;
    }

    for (i = 0u; i < SIXEL_WEBP_VP8_LF_REFS; ++i) {
        status = sixel_webp_vp8_parse_optional_signed(
            decoder, 6u, &filter->ref_delta[i]);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }
    for (i = 0u; i < SIXEL_WEBP_VP8_LF_MODES; ++i) {
        status = sixel_webp_vp8_parse_optional_signed(
            decoder, 6u, &filter->mode_delta[i]);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_webp_vp8_parse_quant_header(
    sixel_webp_vp8_bool_decoder_t *decoder,
    sixel_webp_vp8_quant_header_t *quant)
{
    SIXELSTATUS status;
    unsigned int value;

    status = SIXEL_OK;
    value = 0u;
    if (decoder == NULL || quant == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    memset(quant, 0, sizeof(*quant));
    status = sixel_webp_vp8_bool_read_literal(decoder, 7u, &value);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    quant->y_ac_qi = (int)value;

    status = sixel_webp_vp8_parse_optional_signed(decoder,
                                                  4u,
                                                  &quant->y_dc_delta);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    status = sixel_webp_vp8_parse_optional_signed(decoder,
                                                  4u,
                                                  &quant->y2_dc_delta);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    status = sixel_webp_vp8_parse_optional_signed(decoder,
                                                  4u,
                                                  &quant->y2_ac_delta);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    status = sixel_webp_vp8_parse_optional_signed(decoder,
                                                  4u,
                                                  &quant->uv_dc_delta);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    status = sixel_webp_vp8_parse_optional_signed(decoder,
                                                  4u,
                                                  &quant->uv_ac_delta);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_webp_vp8_parse_entropy_header(
    sixel_webp_vp8_bool_decoder_t *decoder,
    sixel_webp_vp8_entropy_header_t *entropy)
{
    SIXELSTATUS status;
    unsigned int type;
    unsigned int band;
    unsigned int context;
    unsigned int node;
    unsigned int literal;
    int bit;
    unsigned int update_prob;

    status = SIXEL_OK;
    type = 0u;
    band = 0u;
    context = 0u;
    node = 0u;
    literal = 0u;
    bit = 0;
    update_prob = 0u;
    if (decoder == NULL || entropy == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    memset(entropy, 0, sizeof(*entropy));
    memcpy(entropy->coef_probs,
           sixel_webp_vp8_default_coef_probs,
           sizeof(entropy->coef_probs));
    entropy->prob_skip_false = -1;

    status = sixel_webp_vp8_bool_read(decoder,
                                      SIXEL_WEBP_VP8_BOOL_BASE_PROB,
                                      &bit);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    entropy->refresh_entropy_probs = bit;

    for (type = 0u; type < SIXEL_WEBP_VP8_COEFF_TYPES; ++type) {
        for (band = 0u; band < SIXEL_WEBP_VP8_COEFF_BANDS; ++band) {
            for (context = 0u;
                 context < SIXEL_WEBP_VP8_PREV_COEFF_CONTEXTS;
                 ++context) {
                for (node = 0u; node < SIXEL_WEBP_VP8_COEFF_NODES; ++node) {
                    update_prob = (unsigned int)
                        sixel_webp_vp8_coef_update_probs[type]
                                                       [band]
                                                       [context]
                                                       [node];
                    status = sixel_webp_vp8_bool_read(
                        decoder, update_prob, &bit);
                    if (SIXEL_FAILED(status)) {
                        return status;
                    }
                    if (bit != 0) {
                        status = sixel_webp_vp8_bool_read_literal(
                            decoder, 8u, &literal);
                        if (SIXEL_FAILED(status)) {
                            return status;
                        }
                        entropy->coef_probs[type][band][context][node] =
                            (unsigned char)literal;
                        ++entropy->coef_prob_update_count;
                    }
                }
            }
        }
    }

    status = sixel_webp_vp8_bool_read(decoder,
                                      SIXEL_WEBP_VP8_BOOL_BASE_PROB,
                                      &bit);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    entropy->mb_no_coeff_skip = bit;
    if (bit != 0) {
        status = sixel_webp_vp8_bool_read_literal(decoder, 8u, &literal);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        entropy->prob_skip_false = (int)literal;
    }

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_webp_vp8_parse_control_header(
    sixel_webp_vp8_bool_decoder_t *decoder,
    sixel_webp_vp8_frame_context_t *context)
{
    SIXELSTATUS status;
    unsigned int value;

    status = SIXEL_OK;
    value = 0u;
    if (decoder == NULL || context == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_webp_vp8_bool_read_literal(decoder, 1u, &value);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (value != 0u) {
        sixel_helper_set_additional_message(
            "builtin webp: unsupported VP8 colorspace flag.");
        return SIXEL_NOT_IMPLEMENTED;
    }

    status = sixel_webp_vp8_bool_read_literal(decoder, 1u, &value);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    status = sixel_webp_vp8_parse_segment_header(decoder, &context->segment);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    status = sixel_webp_vp8_parse_filter_header(decoder, &context->filter);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    status = sixel_webp_vp8_bool_read_literal(decoder, 2u, &value);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    context->token_partition_count = 1u << value;

    status = sixel_webp_vp8_parse_quant_header(decoder, &context->quant);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    status = sixel_webp_vp8_parse_entropy_header(decoder, &context->entropy);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    return SIXEL_OK;
}

static void
sixel_webp_vp8_trace_frame_context(
    sixel_webp_vp8_frame_header_t const *header,
    sixel_webp_vp8_partition_layout_t const *layout,
    sixel_webp_vp8_frame_context_t const *context)
{
    if (header == NULL || layout == NULL || context == NULL) {
        return;
    }
    if (sixel_trace_topic_is_enabled("webp_decode") == 0) {
        return;
    }

    sixel_trace_topic_message(
        "webp_decode",
        "LSXWEBPDBG|diag=VP8CTRL|part0=%zu|tok=%u|seg=%d,%d,%d,%d|"
        "q=%d,%d,%d,%d,%d,%d|lf=%d,%u,%u,%d|skip=%d,%d|cupd=%d|"
        "segq=%d,%d,%d,%d|segf=%d,%d,%d,%d|sprob=%d,%d,%d",
        layout->control_partition_size,
        layout->token_partition_count,
        context->segment.enabled,
        context->segment.update_map,
        context->segment.update_data,
        context->segment.absolute_delta,
        context->quant.y_ac_qi,
        context->quant.y_dc_delta,
        context->quant.y2_dc_delta,
        context->quant.y2_ac_delta,
        context->quant.uv_dc_delta,
        context->quant.uv_ac_delta,
        context->filter.simple,
        context->filter.level,
        context->filter.sharpness,
        context->filter.update_delta,
        context->entropy.mb_no_coeff_skip,
        context->entropy.prob_skip_false,
        context->entropy.coef_prob_update_count,
        context->segment.quant_delta[0],
        context->segment.quant_delta[1],
        context->segment.quant_delta[2],
        context->segment.quant_delta[3],
        context->segment.filter_delta[0],
        context->segment.filter_delta[1],
        context->segment.filter_delta[2],
        context->segment.filter_delta[3],
        context->segment.map_prob[0],
        context->segment.map_prob[1],
        context->segment.map_prob[2]);
}

static SIXELSTATUS
sixel_webp_vp8_parse_partition_layout(
    unsigned char const *payload,
    size_t payload_size,
    sixel_webp_vp8_frame_header_t const *header,
    sixel_webp_vp8_partition_layout_t *layout,
    sixel_webp_vp8_frame_context_t *context)
{
    SIXELSTATUS status;
    sixel_webp_vp8_bool_decoder_t decoder;
    size_t control_end;
    size_t table_size;
    size_t remaining;
    unsigned int i;
    unsigned int value;

    status = SIXEL_OK;
    memset(&decoder, 0, sizeof(decoder));
    control_end = 0u;
    table_size = 0u;
    remaining = 0u;
    i = 0u;
    value = 0u;
    if (payload == NULL || payload_size == 0u || header == NULL ||
        layout == NULL || context == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    memset(layout, 0, sizeof(*layout));
    memset(context, 0, sizeof(*context));
    layout->control_partition_offset = SIXEL_WEBP_VP8_CONTROL_OFFSET;
    layout->control_partition_size = header->first_partition_size;

    if (layout->control_partition_offset > payload_size ||
        layout->control_partition_size >
            payload_size - layout->control_partition_offset) {
        sixel_helper_set_additional_message(
            "builtin webp: VP8 first partition is truncated.");
        return SIXEL_BAD_INPUT;
    }

    status = sixel_webp_vp8_bool_init(
        &decoder,
        payload + layout->control_partition_offset,
        layout->control_partition_size);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    status = sixel_webp_vp8_parse_control_header(&decoder, context);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    context->mode_decoder = decoder;

    layout->token_partition_count = context->token_partition_count;
    if (layout->token_partition_count == 0u ||
        layout->token_partition_count > SIXEL_WEBP_VP8_MAX_TOKEN_PARTITIONS) {
        sixel_helper_set_additional_message(
            "builtin webp: VP8 token partition count is invalid.");
        return SIXEL_BAD_INPUT;
    }

    control_end = layout->control_partition_offset +
                  layout->control_partition_size;
    layout->token_partition_table_offset = control_end;

    if (layout->token_partition_count > 1u) {
        if ((size_t)(layout->token_partition_count - 1u) >
            SIZE_MAX / 3u) {
            return SIXEL_BAD_INTEGER_OVERFLOW;
        }
        table_size = (size_t)(layout->token_partition_count - 1u) * 3u;
    } else {
        table_size = 0u;
    }

    if (control_end > payload_size || table_size > payload_size - control_end) {
        sixel_helper_set_additional_message(
            "builtin webp: VP8 token partition table is truncated.");
        return SIXEL_BAD_INPUT;
    }

    layout->token_partition_data_offset = control_end + table_size;
    remaining = payload_size - layout->token_partition_data_offset;
    for (i = 0u; i + 1u < layout->token_partition_count; ++i) {
        value = sixel_webp_vp8_read_u24le(
            payload + control_end + (size_t)i * 3u);
        if ((size_t)value > remaining) {
            sixel_helper_set_additional_message(
                "builtin webp: VP8 token partition exceeds payload.");
            return SIXEL_BAD_INPUT;
        }
        layout->token_partition_size[i] = (size_t)value;
        remaining -= (size_t)value;
    }
    layout->token_partition_size[layout->token_partition_count - 1u] =
        remaining;

    context->mb_cols = (unsigned int)((header->width + 15) / 16);
    context->mb_rows = (unsigned int)((header->height + 15) / 16);
    sixel_webp_vp8_trace_frame_context(header, layout, context);

    return SIXEL_OK;
}

SIXELSTATUS
sixel_webp_vp8_decode_native_payload(unsigned char const *payload,
                                     size_t payload_size,
                                     sixel_webp_vp8_frame_header_t const
                                         *header,
                                     unsigned char **prgba,
                                     int *pwidth,
                                     int *pheight,
                                     sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    sixel_webp_vp8_partition_layout_t layout;
    sixel_webp_vp8_frame_context_t context;
    sixel_webp_vp8_bool_decoder_t token_decoders[
        SIXEL_WEBP_VP8_MAX_TOKEN_PARTITIONS];
    sixel_webp_vp8_planes_t planes;

    status = SIXEL_OK;
    memset(&layout, 0, sizeof(layout));
    memset(&context, 0, sizeof(context));
    memset(token_decoders, 0, sizeof(token_decoders));
    memset(&planes, 0, sizeof(planes));

    /*
     * This entrypoint keeps the stage boundaries explicit:
     * 1) partition/control parsing
     * 2) macroblock-level intra reconstruction into YUV planes
     * 3) YUV420 to RGBA8888 conversion
     */
    if (payload == NULL || payload_size == 0u || header == NULL ||
        prgba == NULL || pwidth == NULL || pheight == NULL ||
        allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *prgba = NULL;
    *pwidth = 0;
    *pheight = 0;
    (void)allocator;

    status = sixel_webp_vp8_parse_partition_layout(payload,
                                                   payload_size,
                                                   header,
                                                   &layout,
                                                   &context);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    status = sixel_webp_vp8_validate_dimensions(header->width,
                                                header->height);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (layout.token_partition_count != 1u) {
        sixel_helper_set_additional_message(
            "builtin webp: VP8 token partition count > 1 is unsupported.");
        return SIXEL_NOT_IMPLEMENTED;
    }

    status = sixel_webp_vp8_alloc_planes(&planes, header, allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = sixel_webp_vp8_init_token_decoders(token_decoders,
                                                layout.token_partition_count,
                                                payload,
                                                payload_size,
                                                &layout);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = sixel_webp_vp8_decode_native_intra(token_decoders,
                                                header,
                                                &context,
                                                &planes,
                                                allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = sixel_webp_vp8_convert_yuv420_to_rgba(&planes,
                                                   header,
                                                   prgba,
                                                   allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    *pwidth = header->width;
    *pheight = header->height;

cleanup:
    sixel_webp_vp8_free_planes(&planes, allocator);
    return status;
}


/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
