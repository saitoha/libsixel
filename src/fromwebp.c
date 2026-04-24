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
sixel_webp_apply_decode_plan(sixel_webp_decode_plan_t const *plan)
{
    SIXELSTATUS status;

    status = SIXEL_OK;
    if (plan == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    switch (plan->kind) {
    case SIXEL_WEBP_CONTAINER_KIND_CORRUPT:
        sixel_webp_trace_contract_add_code(SIXEL_WEBP_CODE_ERR_MISSING_VP8L);
        sixel_helper_set_additional_message(
            "builtin webp: VP8L payload was not found.");
        status = SIXEL_BAD_INPUT;
        break;
    case SIXEL_WEBP_CONTAINER_KIND_VP8_STATIC:
    case SIXEL_WEBP_CONTAINER_KIND_VP8L_STATIC:
        status = SIXEL_OK;
        break;
    case SIXEL_WEBP_CONTAINER_KIND_UNSUPPORTED_VP8_ALPHA:
        sixel_webp_trace_contract_add_code(SIXEL_WEBP_CODE_UNSUP_VP8_ALPHA);
        sixel_helper_set_additional_message(
            "builtin webp: VP8+ALPHA WebP is not supported.");
        status = SIXEL_NOT_IMPLEMENTED;
        break;
    case SIXEL_WEBP_CONTAINER_KIND_UNSUPPORTED_ANIM:
        sixel_webp_trace_contract_add_code(SIXEL_WEBP_CODE_UNSUP_ANIM);
        sixel_helper_set_additional_message(
            "builtin webp: animated WebP is not supported.");
        status = SIXEL_NOT_IMPLEMENTED;
        break;
    default:
        return SIXEL_BAD_INPUT;
    }
    if (plan->kind == SIXEL_WEBP_CONTAINER_KIND_VP8_STATIC ||
        plan->kind == SIXEL_WEBP_CONTAINER_KIND_VP8L_STATIC) {
        if (plan->meta_iccp_ignored != 0) {
            sixel_webp_trace_contract_add_code(
                SIXEL_WEBP_CODE_META_ICCP_IGNORED);
        }
        if (plan->meta_exif_ignored != 0) {
            sixel_webp_trace_contract_add_code(
                SIXEL_WEBP_CODE_META_EXIF_IGNORED);
        }
    }

    return status;
}

SIXELSTATUS
sixel_fromwebp_load(sixel_chunk_t const *chunk,
                    sixel_frame_t *frame)
{
    SIXELSTATUS status;
    sixel_webp_container_info_t container;
    sixel_webp_decode_plan_t plan;
    unsigned char *rgba;
    int width;
    int height;

    status = SIXEL_OK;
    memset(&container, 0, sizeof(container));
    memset(&plan, 0, sizeof(plan));
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

    status = sixel_webp_build_decode_plan(&container, &plan);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    /* Keep feature handling policy in one place for future mode growth. */
    status = sixel_webp_apply_decode_plan(&plan);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    if (plan.kind == SIXEL_WEBP_CONTAINER_KIND_VP8_STATIC) {
        /* Decode layer accepts only VP8 payload bytes and allocator context. */
        status = sixel_webp_decode_vp8_payload(plan.vp8_payload,
                                               plan.vp8_payload_size,
                                               &rgba,
                                               &width,
                                               &height,
                                               chunk->allocator);
        if (SIXEL_FAILED(status)) {
            if (status == SIXEL_NOT_IMPLEMENTED) {
                sixel_webp_trace_contract_add_code(
                    SIXEL_WEBP_CODE_UNSUP_VP8_FEATURE);
            } else {
                sixel_webp_trace_contract_add_code(
                    SIXEL_WEBP_CODE_ERR_VP8_STREAM);
            }
            goto cleanup;
        }
        sixel_webp_trace_contract_add_code(SIXEL_WEBP_CODE_OK_VP8_STATIC);
    } else if (plan.kind == SIXEL_WEBP_CONTAINER_KIND_VP8L_STATIC) {
        /*
         * Decode layer accepts only VP8L payload bytes and allocator context.
         */
        status = sixel_webp_decode_vp8l_payload(plan.vp8l_payload,
                                                plan.vp8l_payload_size,
                                                &rgba,
                                                &width,
                                                &height,
                                                chunk->allocator);
        if (SIXEL_FAILED(status)) {
            sixel_webp_trace_contract_add_code(
                SIXEL_WEBP_CODE_ERR_VP8L_STREAM);
            goto cleanup;
        }
        sixel_webp_trace_contract_add_code(SIXEL_WEBP_CODE_OK_VP8L_STATIC);
    } else {
        status = SIXEL_BAD_INPUT;
        goto cleanup;
    }

    frame->width = width;
    frame->height = height;
    frame->loop_count = 1;
    frame->pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
    frame->colorspace = SIXEL_COLORSPACE_GAMMA;
    sixel_frame_set_pixels(frame, rgba);

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
