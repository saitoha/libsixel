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
#include "fromwebp-container.h"

static uint32_t
sixel_webp_container_read_u32le(unsigned char const *p)
{
    if (p == NULL) {
        return 0u;
    }
    return (uint32_t)p[0]
        | ((uint32_t)p[1] << 8)
        | ((uint32_t)p[2] << 16)
        | ((uint32_t)p[3] << 24);
}

static uint32_t
sixel_webp_read_fourcc(unsigned char const *p)
{
    return sixel_webp_container_read_u32le(p);
}

static uint32_t
sixel_webp_container_read_u24le(unsigned char const *p)
{
    if (p == NULL) {
        return 0u;
    }
    return (uint32_t)p[0]
        | ((uint32_t)p[1] << 8)
        | ((uint32_t)p[2] << 16);
}

typedef enum sixel_webp_parse_error_id {
    SIXEL_WEBP_PARSE_ERR_RIFF_HEADER_TRUNC = 0,
    SIXEL_WEBP_PARSE_ERR_RIFF_SIGNATURE,
    SIXEL_WEBP_PARSE_ERR_RIFF_SIZE_FIELD,
    SIXEL_WEBP_PARSE_ERR_RIFF_SIZE_OVERFLOW,
    SIXEL_WEBP_PARSE_ERR_RIFF_SIZE_EXCEEDS,
    SIXEL_WEBP_PARSE_ERR_CHUNK_HEADER_TRUNC,
    SIXEL_WEBP_PARSE_ERR_CHUNK_PAYLOAD_OVERFLOW,
    SIXEL_WEBP_PARSE_ERR_CHUNK_PAYLOAD_EXCEEDS,
    SIXEL_WEBP_PARSE_ERR_ODD_PADDING_NONZERO,
    SIXEL_WEBP_PARSE_ERR_FIRST_CHUNK,
    SIXEL_WEBP_PARSE_ERR_DUP_VP8,
    SIXEL_WEBP_PARSE_ERR_DUP_VP8X,
    SIXEL_WEBP_PARSE_ERR_VP8X_SIZE,
    SIXEL_WEBP_PARSE_ERR_DUP_ALPHA,
    SIXEL_WEBP_PARSE_ERR_DUP_ANIM,
    SIXEL_WEBP_PARSE_ERR_DUP_ICCP,
    SIXEL_WEBP_PARSE_ERR_DUP_EXIF,
    SIXEL_WEBP_PARSE_ERR_DUP_XMP,
    SIXEL_WEBP_PARSE_ERR_DUP_VP8L,
    SIXEL_WEBP_PARSE_ERR_CONFLICT_VP8_VP8L,
    SIXEL_WEBP_PARSE_ERR_CONFLICT_VP8L_ALPHA,
    SIXEL_WEBP_PARSE_ERR_VP8X_FLAG_ICCP_MISMATCH,
    SIXEL_WEBP_PARSE_ERR_VP8X_FLAG_EXIF_MISMATCH,
    SIXEL_WEBP_PARSE_ERR_VP8X_FLAG_XMP_MISMATCH,
    SIXEL_WEBP_PARSE_ERR_VP8X_FLAG_ANIM_MISMATCH,
    SIXEL_WEBP_PARSE_ERR_VP8X_FLAG_ALPHA_MISMATCH
} sixel_webp_parse_error_id_t;

typedef struct sixel_webp_parse_error_entry {
    SIXELSTATUS status;
    char const *trace_code;
    char const *message;
} sixel_webp_parse_error_entry_t;

static sixel_webp_parse_error_entry_t const
sixel_webp_parse_error_table[] = {
    { SIXEL_BAD_INPUT, SIXEL_WEBP_CODE_ERR_RIFF_HEADER_TRUNC,
      "builtin webp: RIFF header is truncated." },
    { SIXEL_BAD_INPUT, SIXEL_WEBP_CODE_ERR_RIFF_SIGNATURE,
      "builtin webp: RIFF/WEBP signature is invalid." },
    { SIXEL_BAD_INPUT, SIXEL_WEBP_CODE_ERR_RIFF_SIZE_FIELD,
      "builtin webp: RIFF size field is invalid." },
    { SIXEL_BAD_INTEGER_OVERFLOW, SIXEL_WEBP_CODE_ERR_RIFF_SIZE_EXCEEDS,
      NULL },
    { SIXEL_BAD_INPUT, SIXEL_WEBP_CODE_ERR_RIFF_SIZE_EXCEEDS,
      "builtin webp: RIFF size exceeds input buffer." },
    { SIXEL_BAD_INPUT, SIXEL_WEBP_CODE_ERR_CHUNK_HDR_TRUNC,
      "builtin webp: chunk header is truncated." },
    { SIXEL_BAD_INTEGER_OVERFLOW, SIXEL_WEBP_CODE_ERR_CHUNK_PAYLOAD_EXCEEDS,
      NULL },
    { SIXEL_BAD_INPUT, SIXEL_WEBP_CODE_ERR_CHUNK_PAYLOAD_EXCEEDS,
      "builtin webp: chunk payload exceeds RIFF size." },
    { SIXEL_BAD_INPUT, SIXEL_WEBP_CODE_ERR_ODD_PADDING_NONZERO,
      "builtin webp: odd-sized chunk has non-zero padding." },
    { SIXEL_BAD_INPUT, SIXEL_WEBP_CODE_ERR_FIRST_CHUNK,
      "builtin webp: first chunk must be VP8/VP8L/VP8X." },
    { SIXEL_BAD_INPUT, SIXEL_WEBP_CODE_ERR_DUP_VP8,
      "builtin webp: duplicate VP8 chunk is invalid." },
    { SIXEL_BAD_INPUT, SIXEL_WEBP_CODE_ERR_DUP_VP8X,
      "builtin webp: duplicate VP8X chunk is invalid." },
    { SIXEL_BAD_INPUT, SIXEL_WEBP_CODE_ERR_VP8X_SIZE,
      "builtin webp: VP8X chunk size is invalid." },
    { SIXEL_BAD_INPUT, SIXEL_WEBP_CODE_ERR_DUP_ALPHA,
      "builtin webp: duplicate alpha chunk is invalid." },
    { SIXEL_BAD_INPUT, SIXEL_WEBP_CODE_ERR_DUP_ANIM,
      "builtin webp: duplicate ANIM chunk is invalid." },
    { SIXEL_BAD_INPUT, SIXEL_WEBP_CODE_ERR_DUP_ICCP,
      "builtin webp: duplicate ICCP chunk is invalid." },
    { SIXEL_BAD_INPUT, SIXEL_WEBP_CODE_ERR_DUP_EXIF,
      "builtin webp: duplicate EXIF chunk is invalid." },
    { SIXEL_BAD_INPUT, SIXEL_WEBP_CODE_ERR_DUP_XMP,
      "builtin webp: duplicate XMP chunk is invalid." },
    { SIXEL_BAD_INPUT, SIXEL_WEBP_CODE_ERR_VP8L_STREAM,
      "builtin webp: duplicate VP8L chunk is invalid." },
    { SIXEL_BAD_INPUT, SIXEL_WEBP_CODE_ERR_CONFLICT_VP8_VP8L,
      "builtin webp: VP8 and VP8L chunks cannot coexist." },
    { SIXEL_BAD_INPUT, SIXEL_WEBP_CODE_ERR_CONFLICT_VP8L_ALPHA,
      "builtin webp: VP8L and alpha chunks cannot coexist." },
    { SIXEL_BAD_INPUT, SIXEL_WEBP_CODE_ERR_VP8X_FLAG_ICCP_MISMATCH,
      "builtin webp: VP8X ICCP flag does not match ICCP chunk presence." },
    { SIXEL_BAD_INPUT, SIXEL_WEBP_CODE_ERR_VP8X_FLAG_EXIF_MISMATCH,
      "builtin webp: VP8X EXIF flag does not match EXIF chunk presence." },
    { SIXEL_BAD_INPUT, SIXEL_WEBP_CODE_ERR_VP8X_FLAG_XMP_MISMATCH,
      "builtin webp: VP8X XMP flag does not match XMP chunk presence." },
    { SIXEL_BAD_INPUT, SIXEL_WEBP_CODE_ERR_VP8X_FLAG_ANIM_MISMATCH,
      "builtin webp: VP8X animation flag does not match ANIM/ANMF chunks." },
    { SIXEL_BAD_INPUT, SIXEL_WEBP_CODE_ERR_VP8X_FLAG_ALPHA_MISMATCH,
      "builtin webp: alpha chunk requires VP8 payload and VP8X alpha flag." }
};

static SIXELSTATUS
sixel_webp_parse_fail(sixel_webp_parse_error_id_t error_id)
{
    sixel_webp_parse_error_entry_t const *entry;

    entry = NULL;
    if ((size_t)error_id >=
        sizeof(sixel_webp_parse_error_table)
            / sizeof(sixel_webp_parse_error_table[0])) {
        return SIXEL_BAD_ARGUMENT;
    }
    entry = &sixel_webp_parse_error_table[(size_t)error_id];
    if (entry->trace_code != NULL) {
        sixel_webp_trace_contract_add_code(entry->trace_code);
    }
    if (entry->message != NULL) {
        sixel_helper_set_additional_message(entry->message);
    }
    return entry->status;
}

static void
sixel_webp_record_chunk(sixel_webp_chunk_ref_t *ref,
                        unsigned int *count,
                        size_t chunk_offset,
                        unsigned char const *payload,
                        size_t payload_size)
{
    if (count != NULL) {
        ++(*count);
    }
    if (ref == NULL || ref->present != 0) {
        return;
    }
    ref->present = 1;
    ref->chunk_offset = chunk_offset;
    ref->payload = payload;
    ref->payload_size = payload_size;
}

static SIXELSTATUS
sixel_webp_validate_vp8x_flags(sixel_webp_container_info_t const *info)
{
    unsigned int iccp_flag_set;
    unsigned int alpha_flag_set;
    unsigned int exif_flag_set;
    unsigned int xmp_flag_set;
    unsigned int anim_flag_set;
    unsigned int has_alpha_chunk;
    unsigned int has_iccp_chunk;
    unsigned int has_exif_chunk;
    unsigned int has_xmp_chunk;
    unsigned int has_anim_chunk;
    unsigned int has_anmf_chunk;
    unsigned int is_anim_container;

    iccp_flag_set = 0u;
    alpha_flag_set = 0u;
    exif_flag_set = 0u;
    xmp_flag_set = 0u;
    anim_flag_set = 0u;
    has_alpha_chunk = 0u;
    has_iccp_chunk = 0u;
    has_exif_chunk = 0u;
    has_xmp_chunk = 0u;
    has_anim_chunk = 0u;
    has_anmf_chunk = 0u;
    is_anim_container = 0u;
    if (info == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    has_alpha_chunk = info->alpha_count != 0u;
    if (info->vp8x_count == 0u) {
        if (has_alpha_chunk != 0u) {
            return sixel_webp_parse_fail(
                SIXEL_WEBP_PARSE_ERR_VP8X_FLAG_ALPHA_MISMATCH);
        }
        return SIXEL_OK;
    }
    iccp_flag_set = (info->vp8x_flags & SIXEL_WEBP_VP8X_ICCP_FLAG) != 0u;
    alpha_flag_set = (info->vp8x_flags & SIXEL_WEBP_VP8X_ALPHA_FLAG) != 0u;
    exif_flag_set = (info->vp8x_flags & SIXEL_WEBP_VP8X_EXIF_FLAG) != 0u;
    xmp_flag_set = (info->vp8x_flags & SIXEL_WEBP_VP8X_XMP_FLAG) != 0u;
    anim_flag_set =
        (info->vp8x_flags & SIXEL_WEBP_VP8X_ANIMATION_FLAG) != 0u;
    has_alpha_chunk = info->alpha_count != 0u;
    has_iccp_chunk = info->iccp_count != 0u;
    has_exif_chunk = info->exif_count != 0u;
    has_xmp_chunk = info->xmp_count != 0u;
    has_anim_chunk = info->anim_count != 0u;
    has_anmf_chunk = info->anmf_count != 0u;
    is_anim_container = (anim_flag_set != 0u ||
                         has_anim_chunk != 0u ||
                         has_anmf_chunk != 0u);

    if (iccp_flag_set != has_iccp_chunk) {
        return sixel_webp_parse_fail(
            SIXEL_WEBP_PARSE_ERR_VP8X_FLAG_ICCP_MISMATCH);
    }
    if (exif_flag_set != has_exif_chunk) {
        return sixel_webp_parse_fail(
            SIXEL_WEBP_PARSE_ERR_VP8X_FLAG_EXIF_MISMATCH);
    }
    if (xmp_flag_set != has_xmp_chunk) {
        return sixel_webp_parse_fail(
            SIXEL_WEBP_PARSE_ERR_VP8X_FLAG_XMP_MISMATCH);
    }
    if (anim_flag_set == 0u && (has_anim_chunk != 0u || has_anmf_chunk != 0u)) {
        return sixel_webp_parse_fail(
            SIXEL_WEBP_PARSE_ERR_VP8X_FLAG_ANIM_MISMATCH);
    }
    if (anim_flag_set != 0u && (has_anim_chunk == 0u || has_anmf_chunk == 0u)) {
        return sixel_webp_parse_fail(
            SIXEL_WEBP_PARSE_ERR_VP8X_FLAG_ANIM_MISMATCH);
    }
    if (is_anim_container == 0u) {
        if (alpha_flag_set != has_alpha_chunk) {
            return sixel_webp_parse_fail(
                SIXEL_WEBP_PARSE_ERR_VP8X_FLAG_ALPHA_MISMATCH);
        }
        if (has_alpha_chunk != 0u && info->vp8_count == 0u) {
            return sixel_webp_parse_fail(
                SIXEL_WEBP_PARSE_ERR_VP8X_FLAG_ALPHA_MISMATCH);
        }
    }
    return SIXEL_OK;
}

SIXELSTATUS
sixel_webp_parse_container(sixel_chunk_t const *chunk,
                           sixel_webp_container_info_t *info)
{
    SIXELSTATUS status;
    unsigned char const *data;
    size_t size;
    uint32_t riff_size;
    size_t riff_total_size;
    size_t offset;
    uint32_t fourcc;
    uint32_t chunk_size_u32;
    size_t chunk_size;
    size_t chunk_total_size;

    status = SIXEL_OK;
    data = NULL;
    size = 0u;
    riff_size = 0u;
    riff_total_size = 0u;
    offset = 0u;
    fourcc = 0u;
    chunk_size_u32 = 0u;
    chunk_size = 0u;
    chunk_total_size = 0u;

    if (chunk == NULL || info == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    memset(info, 0, sizeof(*info));

    data = chunk->buffer;
    size = chunk->size;
    if (data == NULL || size < 12u) {
        return sixel_webp_parse_fail(SIXEL_WEBP_PARSE_ERR_RIFF_HEADER_TRUNC);
    }
    if (memcmp(data, "RIFF", 4u) != 0 ||
        memcmp(data + 8u, "WEBP", 4u) != 0) {
        return sixel_webp_parse_fail(SIXEL_WEBP_PARSE_ERR_RIFF_SIGNATURE);
    }

    riff_size = sixel_webp_container_read_u32le(data + 4u);
    if (riff_size < 4u) {
        return sixel_webp_parse_fail(SIXEL_WEBP_PARSE_ERR_RIFF_SIZE_FIELD);
    }
    riff_total_size = (size_t)riff_size + 8u;
    if (riff_total_size < (size_t)riff_size) {
        return sixel_webp_parse_fail(SIXEL_WEBP_PARSE_ERR_RIFF_SIZE_OVERFLOW);
    }
    if (riff_total_size > size) {
        return sixel_webp_parse_fail(SIXEL_WEBP_PARSE_ERR_RIFF_SIZE_EXCEEDS);
    }

    /*
     * This stage only validates RIFF/chunk structure and records container
     * capabilities. Actual decode decisions are made by the orchestrator.
     */
    offset = 12u;
    while (offset < riff_total_size) {
        if (riff_total_size - offset < 8u) {
            return sixel_webp_parse_fail(
                SIXEL_WEBP_PARSE_ERR_CHUNK_HEADER_TRUNC);
        }

        fourcc = sixel_webp_read_fourcc(data + offset);
        chunk_size_u32 = sixel_webp_container_read_u32le(data + offset + 4u);
        chunk_size = (size_t)chunk_size_u32;
        chunk_total_size = 8u + chunk_size + (chunk_size & 1u);

        if (chunk_size > SIZE_MAX - 8u - offset) {
            return sixel_webp_parse_fail(
                SIXEL_WEBP_PARSE_ERR_CHUNK_PAYLOAD_OVERFLOW);
        }
        if (chunk_total_size > riff_total_size - offset) {
            return sixel_webp_parse_fail(
                SIXEL_WEBP_PARSE_ERR_CHUNK_PAYLOAD_EXCEEDS);
        }
        if ((chunk_size_u32 & 1u) != 0u &&
            data[offset + 8u + chunk_size] != 0u) {
            return sixel_webp_parse_fail(
                SIXEL_WEBP_PARSE_ERR_ODD_PADDING_NONZERO);
        }

        if (offset == 12u &&
            fourcc != SIXEL_WEBP_CHUNK_VP8 &&
            fourcc != SIXEL_WEBP_CHUNK_VP8L &&
            fourcc != SIXEL_WEBP_CHUNK_VP8X) {
            return sixel_webp_parse_fail(SIXEL_WEBP_PARSE_ERR_FIRST_CHUNK);
        }

        if (fourcc == SIXEL_WEBP_CHUNK_VP8X) {
            if (info->vp8x_count != 0u) {
                return sixel_webp_parse_fail(SIXEL_WEBP_PARSE_ERR_DUP_VP8X);
            }
            if (chunk_size != 10u) {
                return sixel_webp_parse_fail(SIXEL_WEBP_PARSE_ERR_VP8X_SIZE);
            }
            sixel_webp_record_chunk(&info->vp8x,
                                    &info->vp8x_count,
                                    offset,
                                    data + offset + 8u,
                                    chunk_size);
            info->vp8x_flags = data[offset + 8u];
            info->canvas_width = (unsigned int)
                sixel_webp_container_read_u24le(data + offset + 12u) + 1u;
            info->canvas_height = (unsigned int)
                sixel_webp_container_read_u24le(data + offset + 15u) + 1u;
        } else if (fourcc == SIXEL_WEBP_CHUNK_VP8) {
            if (info->vp8_count != 0u) {
                return sixel_webp_parse_fail(SIXEL_WEBP_PARSE_ERR_DUP_VP8);
            }
            if (info->vp8l_count != 0u) {
                return sixel_webp_parse_fail(
                    SIXEL_WEBP_PARSE_ERR_CONFLICT_VP8_VP8L);
            }
            sixel_webp_record_chunk(&info->vp8,
                                    &info->vp8_count,
                                    offset,
                                    data + offset + 8u,
                                    chunk_size);
        } else if (fourcc == SIXEL_WEBP_CHUNK_VP8L) {
            if (info->vp8l_count != 0u) {
                return sixel_webp_parse_fail(SIXEL_WEBP_PARSE_ERR_DUP_VP8L);
            }
            if (info->vp8_count != 0u) {
                return sixel_webp_parse_fail(
                    SIXEL_WEBP_PARSE_ERR_CONFLICT_VP8_VP8L);
            }
            if (info->alpha_count != 0u) {
                return sixel_webp_parse_fail(
                    SIXEL_WEBP_PARSE_ERR_CONFLICT_VP8L_ALPHA);
            }
            sixel_webp_record_chunk(&info->vp8l,
                                    &info->vp8l_count,
                                    offset,
                                    data + offset + 8u,
                                    chunk_size);
        } else if (fourcc == SIXEL_WEBP_CHUNK_ALPHA) {
            if (info->alpha_count != 0u) {
                return sixel_webp_parse_fail(SIXEL_WEBP_PARSE_ERR_DUP_ALPHA);
            }
            if (info->vp8l_count != 0u) {
                return sixel_webp_parse_fail(
                    SIXEL_WEBP_PARSE_ERR_CONFLICT_VP8L_ALPHA);
            }
            sixel_webp_record_chunk(&info->alpha,
                                    &info->alpha_count,
                                    offset,
                                    data + offset + 8u,
                                    chunk_size);
        } else if (fourcc == SIXEL_WEBP_CHUNK_ANIM) {
            if (info->anim_count != 0u) {
                return sixel_webp_parse_fail(SIXEL_WEBP_PARSE_ERR_DUP_ANIM);
            }
            sixel_webp_record_chunk(&info->anim,
                                    &info->anim_count,
                                    offset,
                                    data + offset + 8u,
                                    chunk_size);
            if (chunk_size == 6u) {
                info->anim_background =
                    sixel_webp_container_read_u32le(data + offset + 8u);
                info->anim_loop_count = (unsigned int)data[offset + 12u]
                    | ((unsigned int)data[offset + 13u] << 8);
            }
        } else if (fourcc == SIXEL_WEBP_CHUNK_ANMF) {
            sixel_webp_record_chunk(&info->anmf,
                                    &info->anmf_count,
                                    offset,
                                    data + offset + 8u,
                                    chunk_size);
        } else if (fourcc == SIXEL_WEBP_CHUNK_ICCP) {
            if (info->iccp_count != 0u) {
                return sixel_webp_parse_fail(SIXEL_WEBP_PARSE_ERR_DUP_ICCP);
            }
            sixel_webp_record_chunk(&info->iccp,
                                    &info->iccp_count,
                                    offset,
                                    data + offset + 8u,
                                    chunk_size);
        } else if (fourcc == SIXEL_WEBP_CHUNK_EXIF) {
            if (info->exif_count != 0u) {
                return sixel_webp_parse_fail(SIXEL_WEBP_PARSE_ERR_DUP_EXIF);
            }
            sixel_webp_record_chunk(&info->exif,
                                    &info->exif_count,
                                    offset,
                                    data + offset + 8u,
                                    chunk_size);
        } else if (fourcc == SIXEL_WEBP_CHUNK_XMP) {
            if (info->xmp_count != 0u) {
                return sixel_webp_parse_fail(SIXEL_WEBP_PARSE_ERR_DUP_XMP);
            }
            sixel_webp_record_chunk(&info->xmp,
                                    &info->xmp_count,
                                    offset,
                                    data + offset + 8u,
                                    chunk_size);
        }

        offset += chunk_total_size;
    }

    /*
     * Strictly reject VP8X metadata flag mismatches as malformed input.
     * Alpha-flag alignment is intentionally excluded in this phase.
     */
    status = sixel_webp_validate_vp8x_flags(info);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    return status;
}

sixel_webp_container_kind_t
sixel_webp_classify_container(sixel_webp_container_info_t const *info)
{
    sixel_webp_decode_plan_t plan;

    memset(&plan, 0, sizeof(plan));
    if (info == NULL) {
        return SIXEL_WEBP_CONTAINER_KIND_CORRUPT;
    }
    if (SIXEL_FAILED(sixel_webp_build_decode_plan(info, &plan))) {
        return SIXEL_WEBP_CONTAINER_KIND_CORRUPT;
    }
    return plan.kind;
}

static int
sixel_webp_anim_canvas_is_supported(sixel_webp_container_info_t const *info)
{
    uint64_t pixel_count;

    pixel_count = 0u;
    if (info == NULL) {
        return 0;
    }
    if (info->canvas_width == 0u || info->canvas_height == 0u) {
        return 0;
    }
    if (info->canvas_width > (uint32_t)SIXEL_WEBP_MAX_DIMENSION ||
        info->canvas_height > (uint32_t)SIXEL_WEBP_MAX_DIMENSION) {
        return 0;
    }
    pixel_count = (uint64_t)info->canvas_width * (uint64_t)info->canvas_height;
    if (pixel_count > (uint64_t)SIXEL_WEBP_MAX_IMAGE_PIXELS) {
        return 0;
    }
    return 1;
}

SIXELSTATUS
sixel_webp_build_decode_plan(sixel_webp_container_info_t const *info,
                             sixel_webp_decode_plan_t *plan)
{
    if (info == NULL || plan == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    memset(plan, 0, sizeof(*plan));
    plan->kind = SIXEL_WEBP_CONTAINER_KIND_CORRUPT;
    if (info->anim_count != 0u || info->anmf_count != 0u ||
        ((info->vp8x_flags & SIXEL_WEBP_VP8X_ANIMATION_FLAG) != 0u)) {
        if (info->vp8x_count == 1u &&
            (info->vp8x_flags & SIXEL_WEBP_VP8X_ANIMATION_FLAG) != 0u &&
            info->anim_count != 0u &&
            info->anmf_count != 0u &&
            info->anim.payload_size == 6u &&
            info->anmf.payload_size >= 24u &&
            info->anmf_count <= SIXEL_WEBP_MAX_ANIMATION_FRAMES &&
            sixel_webp_anim_canvas_is_supported(info) != 0) {
            plan->kind = SIXEL_WEBP_CONTAINER_KIND_ANIM_MVP;
            plan->meta_has_iccp = info->iccp_count != 0u ? 1 : 0;
            plan->meta_has_exif = info->exif_count != 0u ? 1 : 0;
            plan->iccp_payload = info->iccp.payload;
            plan->iccp_payload_size = info->iccp.payload_size;
            plan->exif_payload = info->exif.payload;
            plan->exif_payload_size = info->exif.payload_size;
            plan->canvas_width = (int)info->canvas_width;
            plan->canvas_height = (int)info->canvas_height;
            plan->anim_loop_count = (int)info->anim_loop_count;
            plan->anim_frame_count = (int)info->anmf_count;
            return SIXEL_OK;
        }
        plan->kind = SIXEL_WEBP_CONTAINER_KIND_UNSUPPORTED_ANIM;
        return SIXEL_OK;
    }

    if (info->vp8_count != 0u && info->alpha_count != 0u) {
        plan->kind = SIXEL_WEBP_CONTAINER_KIND_VP8_ALPHA_STATIC;
        plan->vp8_payload = info->vp8.payload;
        plan->vp8_payload_size = info->vp8.payload_size;
        plan->alpha_payload = info->alpha.payload;
        plan->alpha_payload_size = info->alpha.payload_size;
        plan->meta_has_iccp = info->iccp_count != 0u ? 1 : 0;
        plan->meta_has_exif = info->exif_count != 0u ? 1 : 0;
        plan->iccp_payload = info->iccp.payload;
        plan->iccp_payload_size = info->iccp.payload_size;
        plan->exif_payload = info->exif.payload;
        plan->exif_payload_size = info->exif.payload_size;
        return SIXEL_OK;
    }

    if (info->vp8_count != 0u) {
        plan->kind = SIXEL_WEBP_CONTAINER_KIND_VP8_STATIC;
        plan->vp8_payload = info->vp8.payload;
        plan->vp8_payload_size = info->vp8.payload_size;
        plan->meta_has_iccp = info->iccp_count != 0u ? 1 : 0;
        plan->meta_has_exif = info->exif_count != 0u ? 1 : 0;
        plan->iccp_payload = info->iccp.payload;
        plan->iccp_payload_size = info->iccp.payload_size;
        plan->exif_payload = info->exif.payload;
        plan->exif_payload_size = info->exif.payload_size;
        return SIXEL_OK;
    }

    if (info->vp8l_count == 0u ||
        info->vp8l.present == 0 ||
        info->vp8l.payload == NULL ||
        info->vp8l.payload_size == 0u) {
        return SIXEL_OK;
    }

    plan->kind = SIXEL_WEBP_CONTAINER_KIND_VP8L_STATIC;
    plan->vp8l_payload = info->vp8l.payload;
    plan->vp8l_payload_size = info->vp8l.payload_size;
    plan->meta_has_iccp = info->iccp_count != 0u ? 1 : 0;
    plan->meta_has_exif = info->exif_count != 0u ? 1 : 0;
    plan->iccp_payload = info->iccp.payload;
    plan->iccp_payload_size = info->iccp.payload_size;
    plan->exif_payload = info->exif.payload;
    plan->exif_payload_size = info->exif.payload_size;
    return SIXEL_OK;
}


/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
