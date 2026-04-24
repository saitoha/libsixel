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
#include "fromwebp-internal.h"

static uint32_t
sixel_webp_read_u32le(unsigned char const *p)
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
    return sixel_webp_read_u32le(p);
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
    SIXEL_WEBP_PARSE_ERR_DUP_VP8X,
    SIXEL_WEBP_PARSE_ERR_VP8X_SIZE,
    SIXEL_WEBP_PARSE_ERR_DUP_VP8L
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
    { SIXEL_BAD_INPUT, SIXEL_WEBP_CODE_ERR_DUP_VP8X,
      "builtin webp: duplicate VP8X chunk is invalid." },
    { SIXEL_BAD_INPUT, SIXEL_WEBP_CODE_ERR_VP8X_SIZE,
      "builtin webp: VP8X chunk size is invalid." },
    { SIXEL_BAD_INPUT, SIXEL_WEBP_CODE_ERR_VP8L_STREAM,
      "builtin webp: duplicate VP8L chunk is invalid." }
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

    riff_size = sixel_webp_read_u32le(data + 4u);
    if (riff_size < 4u) {
        return sixel_webp_parse_fail(SIXEL_WEBP_PARSE_ERR_RIFF_SIZE_FIELD);
    }
#if SIZE_MAX <= UINT32_MAX
    /*
     * RIFF size is a 32-bit field. Overflow can only happen when size_t is
     * also 32-bit or narrower, so keep this guard off on wider targets.
     */
    if ((size_t)riff_size > SIZE_MAX - 8u) {
        return sixel_webp_parse_fail(SIXEL_WEBP_PARSE_ERR_RIFF_SIZE_OVERFLOW);
    }
#endif

    riff_total_size = (size_t)riff_size + 8u;
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
        chunk_size_u32 = sixel_webp_read_u32le(data + offset + 4u);
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
        } else if (fourcc == SIXEL_WEBP_CHUNK_VP8) {
            sixel_webp_record_chunk(&info->vp8,
                                    &info->vp8_count,
                                    offset,
                                    data + offset + 8u,
                                    chunk_size);
        } else if (fourcc == SIXEL_WEBP_CHUNK_VP8L) {
            if (info->vp8l_count != 0u) {
                return sixel_webp_parse_fail(SIXEL_WEBP_PARSE_ERR_DUP_VP8L);
            }
            sixel_webp_record_chunk(&info->vp8l,
                                    &info->vp8l_count,
                                    offset,
                                    data + offset + 8u,
                                    chunk_size);
        } else if (fourcc == SIXEL_WEBP_CHUNK_ALPHA) {
            sixel_webp_record_chunk(&info->alpha,
                                    &info->alpha_count,
                                    offset,
                                    data + offset + 8u,
                                    chunk_size);
        } else if (fourcc == SIXEL_WEBP_CHUNK_ANIM) {
            sixel_webp_record_chunk(&info->anim,
                                    &info->anim_count,
                                    offset,
                                    data + offset + 8u,
                                    chunk_size);
        } else if (fourcc == SIXEL_WEBP_CHUNK_ANMF) {
            sixel_webp_record_chunk(&info->anmf,
                                    &info->anmf_count,
                                    offset,
                                    data + offset + 8u,
                                    chunk_size);
        } else if (fourcc == SIXEL_WEBP_CHUNK_ICCP) {
            sixel_webp_record_chunk(&info->iccp,
                                    &info->iccp_count,
                                    offset,
                                    data + offset + 8u,
                                    chunk_size);
        } else if (fourcc == SIXEL_WEBP_CHUNK_EXIF) {
            sixel_webp_record_chunk(&info->exif,
                                    &info->exif_count,
                                    offset,
                                    data + offset + 8u,
                                    chunk_size);
        } else if (fourcc == SIXEL_WEBP_CHUNK_XMP) {
            sixel_webp_record_chunk(&info->xmp,
                                    &info->xmp_count,
                                    offset,
                                    data + offset + 8u,
                                    chunk_size);
        }

        offset += chunk_total_size;
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
        plan->kind = SIXEL_WEBP_CONTAINER_KIND_UNSUPPORTED_ANIM;
        return SIXEL_OK;
    }

    if (info->vp8_count != 0u && info->alpha_count != 0u) {
        plan->kind = SIXEL_WEBP_CONTAINER_KIND_UNSUPPORTED_VP8_ALPHA;
        plan->vp8_payload = info->vp8.payload;
        plan->vp8_payload_size = info->vp8.payload_size;
        return SIXEL_OK;
    }

    if (info->vp8_count != 0u) {
        plan->kind = SIXEL_WEBP_CONTAINER_KIND_VP8_STATIC;
        plan->vp8_payload = info->vp8.payload;
        plan->vp8_payload_size = info->vp8.payload_size;
        plan->meta_iccp_ignored = info->iccp_count != 0u ? 1 : 0;
        plan->meta_exif_ignored = info->exif_count != 0u ? 1 : 0;
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
    plan->meta_iccp_ignored = info->iccp_count != 0u ? 1 : 0;
    plan->meta_exif_ignored = info->exif_count != 0u ? 1 : 0;
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
