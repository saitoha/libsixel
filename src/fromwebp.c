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

#include "cms.h"
#include "compat_stub.h"
#include "fromwebp.h"
#include "fromwebp-internal.h"
#include "loader-common.h"

#ifndef SIZE_MAX
# define SIZE_MAX ((size_t)-1)
#endif

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
    return status;
}

static void
sixel_webp_trace_unapplied_meta_codes(sixel_webp_decode_plan_t const *plan,
                                      int iccp_reported,
                                      int exif_reported)
{
    if (plan == NULL) {
        return;
    }
    if (plan->kind != SIXEL_WEBP_CONTAINER_KIND_VP8_STATIC &&
        plan->kind != SIXEL_WEBP_CONTAINER_KIND_VP8L_STATIC) {
        return;
    }
    if (plan->meta_has_iccp != 0 && iccp_reported == 0) {
        sixel_webp_trace_contract_add_code(SIXEL_WEBP_CODE_META_ICCP_IGNORED);
    }
    if (plan->meta_has_exif != 0 && exif_reported == 0) {
        sixel_webp_trace_contract_add_code(SIXEL_WEBP_CODE_META_EXIF_IGNORED);
    }
}

static int
sixel_webp_apply_iccp_to_srgb_rgba(unsigned char *pixels,
                                   int width,
                                   int height,
                                   unsigned char const *profile,
                                   size_t profile_size,
                                   sixel_allocator_t *allocator)
{
#if HAVE_LCMS2
    sixel_cms_profile_t *src_profile;
    sixel_cms_profile_t *dst_profile;
    sixel_cms_transform_t *transform;
    size_t pixel_count;
    int converted;

    src_profile = NULL;
    dst_profile = NULL;
    transform = NULL;
    pixel_count = 0u;
    converted = 0;
    (void)allocator;

    if (pixels == NULL || width <= 0 || height <= 0 ||
        profile == NULL || profile_size == 0u) {
        return 0;
    }
    if ((size_t)width > SIZE_MAX / (size_t)height) {
        return 0;
    }

    src_profile = sixel_cms_open_profile_from_mem(profile, profile_size);
    if (src_profile == NULL) {
        return 0;
    }
    dst_profile = sixel_cms_create_srgb_profile();
    if (dst_profile == NULL) {
        goto cleanup;
    }

    transform = sixel_cms_create_transform(
        src_profile,
        SIXEL_CMS_PIXELFORMAT_RGBA_8,
        dst_profile,
        SIXEL_CMS_PIXELFORMAT_RGBA_8,
        SIXEL_CMS_TRANSFORM_COPY_ALPHA);
    if (transform == NULL) {
        goto cleanup;
    }

    pixel_count = (size_t)width * (size_t)height;
    converted = sixel_cms_do_transform(transform,
                                       pixels,
                                       pixels,
                                       pixel_count);

cleanup:
    if (transform != NULL) {
        sixel_cms_delete_transform(transform);
    }
    if (dst_profile != NULL) {
        sixel_cms_close_profile(dst_profile);
    }
    if (src_profile != NULL) {
        sixel_cms_close_profile(src_profile);
    }
    return converted;
#else
    unsigned char *rgb_pixels;
    size_t pixel_count;
    size_t rgb_bytes;
    size_t i;
    size_t src_offset;
    size_t dst_offset;
    int converted;

    rgb_pixels = NULL;
    pixel_count = 0u;
    rgb_bytes = 0u;
    i = 0u;
    src_offset = 0u;
    dst_offset = 0u;
    converted = 0;

    if (pixels == NULL || width <= 0 || height <= 0 ||
        profile == NULL || profile_size == 0u ||
        allocator == NULL) {
        return 0;
    }
    if ((size_t)width > SIZE_MAX / (size_t)height) {
        return 0;
    }

    pixel_count = (size_t)width * (size_t)height;
    if (pixel_count > SIZE_MAX / 3u) {
        return 0;
    }
    rgb_bytes = pixel_count * 3u;
    rgb_pixels = (unsigned char *)sixel_allocator_malloc(allocator, rgb_bytes);
    if (rgb_pixels == NULL) {
        return 0;
    }

    for (i = 0u; i < pixel_count; ++i) {
        src_offset = i * 4u;
        dst_offset = i * 3u;
        rgb_pixels[dst_offset + 0u] = pixels[src_offset + 0u];
        rgb_pixels[dst_offset + 1u] = pixels[src_offset + 1u];
        rgb_pixels[dst_offset + 2u] = pixels[src_offset + 2u];
    }

    converted = sixel_cms_convert_to_srgb_with_profile_bytes(
        rgb_pixels,
        width,
        height,
        SIXEL_PIXELFORMAT_RGB888,
        profile,
        profile_size);
    if (converted != 0) {
        for (i = 0u; i < pixel_count; ++i) {
            src_offset = i * 3u;
            dst_offset = i * 4u;
            pixels[dst_offset + 0u] = rgb_pixels[src_offset + 0u];
            pixels[dst_offset + 1u] = rgb_pixels[src_offset + 1u];
            pixels[dst_offset + 2u] = rgb_pixels[src_offset + 2u];
        }
    }

    sixel_allocator_free(allocator, rgb_pixels);
    return converted;
#endif
}

static SIXELSTATUS
sixel_webp_try_apply_exif_orientation(sixel_webp_decode_plan_t const *plan,
                                      int enable_orientation,
                                      sixel_frame_t *frame,
                                      int *applied)
{
    SIXELSTATUS status;
    int parsed_orientation;
    int found_orientation;

    status = SIXEL_OK;
    parsed_orientation = 1;
    found_orientation = 0;

    if (plan == NULL || frame == NULL || applied == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *applied = 0;
    if (plan->meta_has_exif == 0 ||
        enable_orientation == 0 ||
        plan->exif_payload == NULL ||
        plan->exif_payload_size == 0u) {
        return SIXEL_OK;
    }

    found_orientation = loader_exif_parse_orientation(plan->exif_payload,
                                                      plan->exif_payload_size,
                                                      &parsed_orientation);
    if (found_orientation == 0 ||
        parsed_orientation < 2 ||
        parsed_orientation > 8) {
        return SIXEL_OK;
    }

    status = loader_frame_apply_orientation(frame, parsed_orientation);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    *applied = 1;
    return SIXEL_OK;
}

SIXELSTATUS
sixel_fromwebp_load(sixel_chunk_t const *chunk,
                    int enable_cms,
                    int enable_orientation,
                    sixel_frame_t *frame)
{
    SIXELSTATUS status;
    sixel_webp_container_info_t container;
    sixel_webp_decode_plan_t plan;
    unsigned char *rgba;
    int width;
    int height;
    int iccp_code_reported;
    int exif_code_reported;
    int iccp_applied;
    int exif_applied;

    status = SIXEL_OK;
    memset(&container, 0, sizeof(container));
    memset(&plan, 0, sizeof(plan));
    rgba = NULL;
    width = 0;
    height = 0;
    iccp_code_reported = 0;
    exif_code_reported = 0;
    iccp_applied = 0;
    exif_applied = 0;

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
    rgba = NULL;

    /*
     * WebP metadata application is intentionally best-effort except for
     * orientation transform allocation failures, which are fatal.
     */
    if (plan.meta_has_iccp != 0) {
        iccp_applied = 0;
        if (enable_cms != 0 &&
            plan.iccp_payload != NULL &&
            plan.iccp_payload_size != 0u) {
            iccp_applied = sixel_webp_apply_iccp_to_srgb_rgba(
                frame->pixels.u8ptr,
                frame->width,
                frame->height,
                plan.iccp_payload,
                plan.iccp_payload_size,
                chunk->allocator);
        }
        if (iccp_applied != 0) {
            sixel_webp_trace_contract_add_code(
                SIXEL_WEBP_CODE_META_ICCP_APPLIED);
        } else {
            sixel_webp_trace_contract_add_code(
                SIXEL_WEBP_CODE_META_ICCP_IGNORED);
        }
        iccp_code_reported = 1;
    }

    if (plan.meta_has_exif != 0) {
        status = sixel_webp_try_apply_exif_orientation(&plan,
                                                       enable_orientation,
                                                       frame,
                                                       &exif_applied);
        if (SIXEL_FAILED(status)) {
            sixel_webp_trace_contract_add_code(
                SIXEL_WEBP_CODE_META_EXIF_IGNORED);
            exif_code_reported = 1;
            goto cleanup;
        }
        if (exif_applied != 0) {
            sixel_webp_trace_contract_add_code(
                SIXEL_WEBP_CODE_META_EXIF_APPLIED);
        } else {
            sixel_webp_trace_contract_add_code(
                SIXEL_WEBP_CODE_META_EXIF_IGNORED);
        }
        exif_code_reported = 1;
    }

cleanup:
    if (rgba != NULL && chunk != NULL && chunk->allocator != NULL) {
        sixel_allocator_free(chunk->allocator, rgba);
    }
    sixel_webp_trace_unapplied_meta_codes(&plan,
                                          iccp_code_reported,
                                          exif_code_reported);
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
