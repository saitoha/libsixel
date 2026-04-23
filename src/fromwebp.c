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

#if HAVE_STRING_H
# include <string.h>
#endif

#include <sixel.h>

#include "compat_stub.h"
#include "fromwebp.h"
#include "fromwebp-internal.h"

static SIXELSTATUS
sixel_webp_apply_container_kind(sixel_webp_container_kind_t kind,
                                sixel_webp_container_info_t const *info)
{
    SIXELSTATUS status;

    status = SIXEL_OK;
    if (info == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    switch (kind) {
    case SIXEL_WEBP_CONTAINER_KIND_UNSUPPORTED_ANIM:
        sixel_webp_trace_contract_add_code(SIXEL_WEBP_CODE_UNSUP_ANIM);
        sixel_helper_set_additional_message(
            "builtin webp: animated WebP is not supported.");
        status = SIXEL_NOT_IMPLEMENTED;
        break;

    case SIXEL_WEBP_CONTAINER_KIND_UNSUPPORTED_VP8:
        sixel_webp_trace_contract_add_code(SIXEL_WEBP_CODE_UNSUP_VP8_LOSSY);
        sixel_helper_set_additional_message(
            "builtin webp: VP8 lossy WebP is not supported.");
        status = SIXEL_NOT_IMPLEMENTED;
        break;

    case SIXEL_WEBP_CONTAINER_KIND_CORRUPT:
        sixel_webp_trace_contract_add_code(SIXEL_WEBP_CODE_ERR_MISSING_VP8L);
        sixel_helper_set_additional_message(
            "builtin webp: VP8L payload was not found.");
        status = SIXEL_BAD_INPUT;
        break;

    case SIXEL_WEBP_CONTAINER_KIND_VP8L_STATIC:
    default:
        if (info->saw_iccp != 0) {
            sixel_webp_trace_contract_add_code(
                SIXEL_WEBP_CODE_META_ICCP_IGNORED);
        }
        if (info->saw_exif != 0) {
            sixel_webp_trace_contract_add_code(
                SIXEL_WEBP_CODE_META_EXIF_IGNORED);
        }
        status = SIXEL_OK;
        break;
    }

    return status;
}

SIXELSTATUS
sixel_fromwebp_load(sixel_chunk_t const *chunk,
                    sixel_frame_t *frame)
{
    SIXELSTATUS status;
    sixel_webp_container_info_t container;
    sixel_webp_container_kind_t kind;
    unsigned char *rgba;
    int width;
    int height;

    status = SIXEL_OK;
    memset(&container, 0, sizeof(container));
    kind = SIXEL_WEBP_CONTAINER_KIND_CORRUPT;
    rgba = NULL;
    width = 0;
    height = 0;

    if (chunk == NULL || frame == NULL || chunk->allocator == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

    status = sixel_webp_parse_container(chunk, &container);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    /* Keep feature classification separate from RIFF validation details. */
    kind = sixel_webp_classify_container(&container);
    status = sixel_webp_apply_container_kind(kind, &container);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    /* Decode layer accepts only VP8L payload bytes and allocator context. */
    status = sixel_webp_decode_vp8l_payload(container.vp8l_payload,
                                            container.vp8l_payload_size,
                                            &rgba,
                                            &width,
                                            &height,
                                            chunk->allocator);
    if (SIXEL_FAILED(status)) {
        sixel_webp_trace_contract_add_code(SIXEL_WEBP_CODE_ERR_VP8L_STREAM);
        goto cleanup;
    }

    frame->width = width;
    frame->height = height;
    frame->loop_count = 1;
    frame->pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
    frame->colorspace = SIXEL_COLORSPACE_GAMMA;
    sixel_frame_set_pixels(frame, rgba);
    sixel_webp_trace_contract_add_code(SIXEL_WEBP_CODE_OK_VP8L_STATIC);

cleanup:
    sixel_webp_trace_contract_flush(SIXEL_SUCCEEDED(status) ? 0 : 1);
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
