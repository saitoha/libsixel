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
    typedef struct sixel_webp_kind_dispatch {
        SIXELSTATUS status;
        char const *trace_code;
        char const *message;
    } sixel_webp_kind_dispatch_t;
    static sixel_webp_kind_dispatch_t const dispatch[] = {
        { SIXEL_BAD_INPUT, SIXEL_WEBP_CODE_ERR_MISSING_VP8L,
          "builtin webp: VP8L payload was not found." },
        { SIXEL_OK, NULL, NULL },
        { SIXEL_NOT_IMPLEMENTED, SIXEL_WEBP_CODE_UNSUP_VP8_STATIC,
          "builtin webp: VP8 static WebP is not supported." },
        { SIXEL_NOT_IMPLEMENTED, SIXEL_WEBP_CODE_UNSUP_VP8_ALPHA,
          "builtin webp: VP8+ALPHA WebP is not supported." },
        { SIXEL_NOT_IMPLEMENTED, SIXEL_WEBP_CODE_UNSUP_ANIM,
          "builtin webp: animated WebP is not supported." }
    };
    size_t kind_index;
    sixel_webp_kind_dispatch_t const *selected;

    selected = NULL;
    if (plan == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    kind_index = (size_t)plan->kind;
    if (kind_index >= sizeof(dispatch) / sizeof(dispatch[0])) {
        return SIXEL_BAD_INPUT;
    }
    selected = &dispatch[kind_index];
    if (selected->trace_code != NULL) {
        sixel_webp_trace_contract_add_code(selected->trace_code);
    }
    if (selected->message != NULL) {
        sixel_helper_set_additional_message(selected->message);
    }
    if (plan->kind == SIXEL_WEBP_CONTAINER_KIND_VP8L_STATIC) {
        if (plan->meta_iccp_ignored != 0) {
            sixel_webp_trace_contract_add_code(
                SIXEL_WEBP_CODE_META_ICCP_IGNORED);
        }
        if (plan->meta_exif_ignored != 0) {
            sixel_webp_trace_contract_add_code(
                SIXEL_WEBP_CODE_META_EXIF_IGNORED);
        }
    }

    return selected->status;
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

    /* Decode layer accepts only VP8L payload bytes and allocator context. */
    status = sixel_webp_decode_vp8l_payload(plan.vp8l_payload,
                                            plan.vp8l_payload_size,
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
