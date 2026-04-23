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
        sixel_webp_trace_contract_add_code(
            SIXEL_WEBP_CODE_ERR_RIFF_HEADER_TRUNC);
        sixel_helper_set_additional_message(
            "builtin webp: RIFF header is truncated.");
        return SIXEL_BAD_INPUT;
    }
    if (memcmp(data, "RIFF", 4u) != 0 ||
        memcmp(data + 8u, "WEBP", 4u) != 0) {
        sixel_webp_trace_contract_add_code(
            SIXEL_WEBP_CODE_ERR_RIFF_SIGNATURE);
        sixel_helper_set_additional_message(
            "builtin webp: RIFF/WEBP signature is invalid.");
        return SIXEL_BAD_INPUT;
    }

    riff_size = sixel_webp_read_u32le(data + 4u);
    if (riff_size < 4u) {
        sixel_webp_trace_contract_add_code(
            SIXEL_WEBP_CODE_ERR_RIFF_SIZE_FIELD);
        sixel_helper_set_additional_message(
            "builtin webp: RIFF size field is invalid.");
        return SIXEL_BAD_INPUT;
    }
    if ((size_t)riff_size > SIZE_MAX - 8u) {
        sixel_webp_trace_contract_add_code(
            SIXEL_WEBP_CODE_ERR_RIFF_SIZE_EXCEEDS);
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    riff_total_size = (size_t)riff_size + 8u;
    if (riff_total_size > size) {
        sixel_webp_trace_contract_add_code(
            SIXEL_WEBP_CODE_ERR_RIFF_SIZE_EXCEEDS);
        sixel_helper_set_additional_message(
            "builtin webp: RIFF size exceeds input buffer.");
        return SIXEL_BAD_INPUT;
    }

    /*
     * This stage only validates RIFF/chunk structure and records container
     * capabilities. Actual decode decisions are made by the orchestrator.
     */
    offset = 12u;
    while (offset < riff_total_size) {
        if (riff_total_size - offset < 8u) {
            sixel_webp_trace_contract_add_code(
                SIXEL_WEBP_CODE_ERR_CHUNK_HDR_TRUNC);
            sixel_helper_set_additional_message(
                "builtin webp: chunk header is truncated.");
            return SIXEL_BAD_INPUT;
        }

        fourcc = sixel_webp_read_fourcc(data + offset);
        chunk_size_u32 = sixel_webp_read_u32le(data + offset + 4u);
        chunk_size = (size_t)chunk_size_u32;
        chunk_total_size = 8u + chunk_size + (chunk_size & 1u);

        if (chunk_size > SIZE_MAX - 8u - offset) {
            sixel_webp_trace_contract_add_code(
                SIXEL_WEBP_CODE_ERR_CHUNK_PAYLOAD_EXCEEDS);
            return SIXEL_BAD_INTEGER_OVERFLOW;
        }
        if (chunk_total_size > riff_total_size - offset) {
            sixel_webp_trace_contract_add_code(
                SIXEL_WEBP_CODE_ERR_CHUNK_PAYLOAD_EXCEEDS);
            sixel_helper_set_additional_message(
                "builtin webp: chunk payload exceeds RIFF size.");
            return SIXEL_BAD_INPUT;
        }
        if ((chunk_size_u32 & 1u) != 0u &&
            data[offset + 8u + chunk_size] != 0u) {
            sixel_webp_trace_contract_add_code(
                SIXEL_WEBP_CODE_ERR_ODD_PADDING_NONZERO);
            sixel_helper_set_additional_message(
                "builtin webp: odd-sized chunk has non-zero padding.");
            return SIXEL_BAD_INPUT;
        }

        if (offset == 12u &&
            fourcc != SIXEL_WEBP_CHUNK_VP8 &&
            fourcc != SIXEL_WEBP_CHUNK_VP8L &&
            fourcc != SIXEL_WEBP_CHUNK_VP8X) {
            sixel_webp_trace_contract_add_code(
                SIXEL_WEBP_CODE_ERR_FIRST_CHUNK);
            sixel_helper_set_additional_message(
                "builtin webp: first chunk must be VP8/VP8L/VP8X.");
            return SIXEL_BAD_INPUT;
        }

        if (fourcc == SIXEL_WEBP_CHUNK_VP8X) {
            if (info->saw_vp8x != 0) {
                sixel_webp_trace_contract_add_code(
                    SIXEL_WEBP_CODE_ERR_DUP_VP8X);
                sixel_helper_set_additional_message(
                    "builtin webp: duplicate VP8X chunk is invalid.");
                return SIXEL_BAD_INPUT;
            }
            if (chunk_size != 10u) {
                sixel_webp_trace_contract_add_code(
                    SIXEL_WEBP_CODE_ERR_VP8X_SIZE);
                sixel_helper_set_additional_message(
                    "builtin webp: VP8X chunk size is invalid.");
                return SIXEL_BAD_INPUT;
            }
            info->saw_vp8x = 1;
            info->vp8x_flags = data[offset + 8u];
        } else if (fourcc == SIXEL_WEBP_CHUNK_VP8) {
            info->saw_vp8 = 1;
        } else if (fourcc == SIXEL_WEBP_CHUNK_VP8L) {
            if (info->saw_vp8l != 0) {
                sixel_webp_trace_contract_add_code(
                    SIXEL_WEBP_CODE_ERR_VP8L_STREAM);
                sixel_helper_set_additional_message(
                    "builtin webp: duplicate VP8L chunk is invalid.");
                return SIXEL_BAD_INPUT;
            }
            info->saw_vp8l = 1;
            info->vp8l_payload = data + offset + 8u;
            info->vp8l_payload_size = chunk_size;
        } else if (fourcc == SIXEL_WEBP_CHUNK_ALPH) {
            info->saw_alph = 1;
        } else if (fourcc == SIXEL_WEBP_CHUNK_ANIM) {
            info->saw_anim = 1;
        } else if (fourcc == SIXEL_WEBP_CHUNK_ANMF) {
            info->saw_anmf = 1;
        } else if (fourcc == SIXEL_WEBP_CHUNK_ICCP) {
            info->saw_iccp = 1;
        } else if (fourcc == SIXEL_WEBP_CHUNK_EXIF) {
            info->saw_exif = 1;
        } else if (fourcc == SIXEL_WEBP_CHUNK_XMP) {
            info->saw_xmp = 1;
        }

        offset += chunk_total_size;
    }

    return status;
}

sixel_webp_container_kind_t
sixel_webp_classify_container(sixel_webp_container_info_t const *info)
{
    if (info == NULL) {
        return SIXEL_WEBP_CONTAINER_KIND_CORRUPT;
    }

    if (info->saw_anim != 0 || info->saw_anmf != 0 ||
        ((info->vp8x_flags & SIXEL_WEBP_VP8X_ANIMATION_FLAG) != 0u)) {
        return SIXEL_WEBP_CONTAINER_KIND_UNSUPPORTED_ANIM;
    }

    if (info->saw_vp8 != 0 || info->saw_alph != 0) {
        return SIXEL_WEBP_CONTAINER_KIND_UNSUPPORTED_VP8;
    }

    if (info->saw_vp8l == 0 ||
        info->vp8l_payload == NULL ||
        info->vp8l_payload_size == 0u) {
        return SIXEL_WEBP_CONTAINER_KIND_CORRUPT;
    }

    return SIXEL_WEBP_CONTAINER_KIND_VP8L_STATIC;
}


/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
