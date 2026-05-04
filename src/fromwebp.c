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
#if HAVE_STDLIB_H
# include <stdlib.h>
#endif
#if HAVE_LIMITS_H
# include <limits.h>
#endif

#include <sixel.h>

#include "cms.h"
#include "chunk-view.h"
#include "compat_stub.h"
#include "fromwebp.h"
#include "frame-private.h"
#include "frame-factory.h"
#include "fromwebp-container.h"
#include "fromwebp-internal.h"
#include "loader-common.h"
#include "loader.h"

#ifndef SIZE_MAX
# define SIZE_MAX ((size_t)-1)
#endif

typedef struct sixel_webp_anim_frame {
    sixel_webp_container_kind_t kind;
    unsigned char const *vp8_payload;
    size_t vp8_payload_size;
    unsigned char const *alpha_payload;
    size_t alpha_payload_size;
    unsigned char const *vp8l_payload;
    size_t vp8l_payload_size;
    int x_offset;
    int y_offset;
    int width;
    int height;
    int duration_ms;
    int dispose_to_background;
    int blend_over;
} sixel_webp_anim_frame_t;

typedef struct sixel_webp_anim_stream {
    sixel_webp_anim_frame_t *frames;
    int frame_count;
    int canvas_width;
    int canvas_height;
    int loop_count;
} sixel_webp_anim_stream_t;

typedef enum sixel_webp_xmp_cms_profile_kind {
    SIXEL_WEBP_XMP_CMS_PROFILE_UNKNOWN = 0,
    SIXEL_WEBP_XMP_CMS_PROFILE_SRGB,
    SIXEL_WEBP_XMP_CMS_PROFILE_DISPLAY_P3,
    SIXEL_WEBP_XMP_CMS_PROFILE_ADOBE_RGB_1998
} sixel_webp_xmp_cms_profile_kind_t;

typedef struct sixel_webp_xmp_profile_alias {
    char const *name;
    sixel_webp_xmp_cms_profile_kind_t kind;
} sixel_webp_xmp_profile_alias_t;

typedef struct sixel_webp_anim_meta_cache {
    sixel_cms_profile_t *iccp_profile;
    sixel_cms_profile_t *xmp_cms_profile;
    int exif_orientation;
    int xmp_orientation;
    int xmp_orientation_size_limited;
    int xmp_cms_size_limited;
} sixel_webp_anim_meta_cache_t;

static unsigned int
sixel_webp_anim_read_u24le(unsigned char const *p)
{
    if (p == NULL) {
        return 0u;
    }
    return (unsigned int)p[0]
        | ((unsigned int)p[1] << 8)
        | ((unsigned int)p[2] << 16);
}

static unsigned int
sixel_webp_anim_read_u32le(unsigned char const *p)
{
    if (p == NULL) {
        return 0u;
    }
    return (unsigned int)p[0]
        | ((unsigned int)p[1] << 8)
        | ((unsigned int)p[2] << 16)
        | ((unsigned int)p[3] << 24);
}

static SIXELSTATUS
sixel_webp_parse_animation_start_frame_no(int *start_frame_no_set,
                                          int *start_frame_no)
{
    SIXELSTATUS status;
    char const *env_value;
    char *endptr;
    long parsed;

    status = SIXEL_OK;
    env_value = NULL;
    endptr = NULL;
    parsed = 0;
    if (start_frame_no_set == NULL || start_frame_no == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *start_frame_no_set = 0;
    *start_frame_no = 0;
    env_value = sixel_compat_getenv("SIXEL_LOADER_ANIMATION_START_FRAME_NO");
    if (env_value == NULL || env_value[0] == '\0') {
        return SIXEL_OK;
    }

    parsed = strtol(env_value, &endptr, 10);
    if (endptr == env_value || *endptr != '\0') {
        sixel_helper_set_additional_message(
            "SIXEL_LOADER_ANIMATION_START_FRAME_NO must be an integer.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (parsed < (long)INT_MIN || parsed > (long)INT_MAX) {
        sixel_helper_set_additional_message(
            "SIXEL_LOADER_ANIMATION_START_FRAME_NO is out of range.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    *start_frame_no_set = 1;
    *start_frame_no = (int)parsed;

end:
    return status;
}

static SIXELSTATUS
sixel_webp_resolve_animation_start_frame_no(int start_frame_no,
                                            int frame_count,
                                            int *resolved)
{
    SIXELSTATUS status;
    long long index;

    status = SIXEL_OK;
    index = 0;
    if (resolved == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (frame_count <= 0) {
        sixel_helper_set_additional_message(
            "Animation frame count must be positive.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    if (start_frame_no >= 0) {
        index = (long long)start_frame_no;
    } else {
        index = (long long)frame_count + (long long)start_frame_no;
    }
    if (index < 0 || index >= (long long)frame_count) {
        sixel_helper_set_additional_message(
            "SIXEL_LOADER_ANIMATION_START_FRAME_NO is outside"
            " the animation frame range.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    *resolved = (int)index;

end:
    return status;
}

static int
sixel_webp_should_stop_after_loop(int loop_control,
                                  int frames_in_loop,
                                  int loop_no,
                                  int stream_loop_count)
{
    if (loop_control == SIXEL_LOOP_DISABLE) {
        return 1;
    }
    if (frames_in_loop <= 1) {
        return 1;
    }
    if (loop_control == SIXEL_LOOP_AUTO &&
        stream_loop_count > 0 &&
        loop_no >= stream_loop_count) {
        return 1;
    }
    return 0;
}

static void
sixel_webp_trace_anim_timing_contract(sixel_webp_anim_stream_t const *stream)
{
    int frame_no;
    int saw_zero_duration;
    int saw_nonzero_duration;

    frame_no = 0;
    saw_zero_duration = 0;
    saw_nonzero_duration = 0;
    if (stream == NULL || stream->frames == NULL || stream->frame_count <= 0) {
        return;
    }

    if (stream->loop_count > 0) {
        sixel_webp_trace_contract_add_code(
            SIXEL_WEBP_CODE_ANIM_LOOPCOUNT_FINITE_SEEN);
    } else {
        sixel_webp_trace_contract_add_code(
            SIXEL_WEBP_CODE_ANIM_LOOPCOUNT_INFINITE_SEEN);
    }

    for (frame_no = 0; frame_no < stream->frame_count; ++frame_no) {
        if (stream->frames[frame_no].duration_ms == 0) {
            saw_zero_duration = 1;
        } else {
            saw_nonzero_duration = 1;
        }
    }

    if (saw_zero_duration != 0) {
        sixel_webp_trace_contract_add_code(
            SIXEL_WEBP_CODE_ANIM_DURATION_ZERO_SEEN);
    }
    if (saw_nonzero_duration != 0) {
        sixel_webp_trace_contract_add_code(
            SIXEL_WEBP_CODE_ANIM_DURATION_NONZERO_SEEN);
    }
}

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
    case SIXEL_WEBP_CONTAINER_KIND_VP8_ALPHA_STATIC:
    case SIXEL_WEBP_CONTAINER_KIND_VP8L_STATIC:
        status = SIXEL_OK;
        break;
    case SIXEL_WEBP_CONTAINER_KIND_ANIM_MVP:
        sixel_webp_trace_contract_add_code(SIXEL_WEBP_CODE_UNSUP_ANIM);
        sixel_helper_set_additional_message(
            "builtin webp: animated WebP is not supported.");
        status = SIXEL_NOT_IMPLEMENTED;
        break;
    case SIXEL_WEBP_CONTAINER_KIND_UNSUPPORTED_ANIM:
        sixel_webp_trace_contract_add_code(SIXEL_WEBP_CODE_UNSUP_ANIM);
        switch (plan->anim_unsupported_reason) {
        case SIXEL_WEBP_ANIM_UNSUPPORTED_REASON_FRAME_LIMIT:
            sixel_webp_trace_contract_add_code(
                SIXEL_WEBP_CODE_UNSUP_ANIM_FRAME_LIMIT);
            break;
        case SIXEL_WEBP_ANIM_UNSUPPORTED_REASON_DIMENSION_LIMIT:
            sixel_webp_trace_contract_add_code(
                SIXEL_WEBP_CODE_UNSUP_ANIM_DIMENSION_LIMIT);
            break;
        case SIXEL_WEBP_ANIM_UNSUPPORTED_REASON_PIXEL_LIMIT:
            sixel_webp_trace_contract_add_code(
                SIXEL_WEBP_CODE_UNSUP_ANIM_PIXEL_LIMIT);
            break;
        default:
            break;
        }
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
                                      int exif_reported,
                                      int xmp_reported,
                                      int xmp_cms_reported)
{
    if (plan == NULL) {
        return;
    }
    if (plan->kind != SIXEL_WEBP_CONTAINER_KIND_VP8_STATIC &&
        plan->kind != SIXEL_WEBP_CONTAINER_KIND_VP8_ALPHA_STATIC &&
        plan->kind != SIXEL_WEBP_CONTAINER_KIND_VP8L_STATIC &&
        plan->kind != SIXEL_WEBP_CONTAINER_KIND_ANIM_MVP) {
        return;
    }
    if (plan->meta_has_iccp != 0 && iccp_reported == 0) {
        sixel_webp_trace_contract_add_code(SIXEL_WEBP_CODE_META_ICCP_IGNORED);
    }
    if (plan->meta_has_exif != 0 && exif_reported == 0) {
        sixel_webp_trace_contract_add_code(SIXEL_WEBP_CODE_META_EXIF_IGNORED);
    }
    if (plan->meta_has_xmp != 0 && xmp_reported == 0) {
        sixel_webp_trace_contract_add_code(SIXEL_WEBP_CODE_META_XMP_IGNORED);
    }
    if (plan->meta_has_xmp != 0 && xmp_cms_reported == 0) {
        sixel_webp_trace_contract_add_code(
            SIXEL_WEBP_CODE_META_XMP_CMS_IGNORED);
    }
}

static int
sixel_webp_apply_profile_to_srgb_rgba(unsigned char *pixels,
                                      int width,
                                      int height,
                                      sixel_cms_profile_t *src_profile,
                                      sixel_allocator_t *allocator)
{
#if HAVE_LCMS2
    sixel_cms_profile_t *dst_profile;
    sixel_cms_transform_t *transform;
    size_t pixel_count;
    int converted;

    dst_profile = NULL;
    transform = NULL;
    pixel_count = 0u;
    converted = 0;
    (void)allocator;

    if (pixels == NULL || width <= 0 || height <= 0 || src_profile == NULL) {
        return 0;
    }
    if ((size_t)width > SIZE_MAX / (size_t)height) {
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
        src_profile == NULL || allocator == NULL) {
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

    converted = sixel_cms_convert_profile_to_srgb(
        rgb_pixels,
        width,
        height,
        SIXEL_PIXELFORMAT_RGB888,
        src_profile);
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

static int
sixel_webp_apply_iccp_to_srgb_rgba(unsigned char *pixels,
                                   int width,
                                   int height,
                                   unsigned char const *profile,
                                   size_t profile_size,
                                   sixel_allocator_t *allocator)
{
    sixel_cms_profile_t *src_profile;
    int converted;

    src_profile = NULL;
    converted = 0;
    if (pixels == NULL || width <= 0 || height <= 0 ||
        profile == NULL || profile_size == 0u) {
        return 0;
    }

    src_profile = sixel_cms_open_profile_from_mem(profile, profile_size);
    if (src_profile == NULL) {
        return 0;
    }
    converted = sixel_webp_apply_profile_to_srgb_rgba(pixels,
                                                      width,
                                                      height,
                                                      src_profile,
                                                      allocator);
    sixel_cms_close_profile(src_profile);
    return converted;
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

static int
sixel_webp_xmp_payload_exceeds_parse_limit(size_t payload_size)
{
    return payload_size > SIXEL_WEBP_MAX_XMP_PARSE_BYTES ? 1 : 0;
}

static int
sixel_webp_xmp_is_space(unsigned char ch)
{
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

static int
sixel_webp_xmp_is_name_char(unsigned char ch)
{
    if ((ch >= 'a' && ch <= 'z') ||
        (ch >= 'A' && ch <= 'Z') ||
        (ch >= '0' && ch <= '9') ||
        ch == '_' || ch == '-' || ch == '.' || ch == ':') {
        return 1;
    }
    return 0;
}

static int
sixel_webp_xmp_find_prefixed_local_name(unsigned char const *payload,
                                        size_t payload_size,
                                        size_t local_begin,
                                        size_t local_size,
                                        size_t *name_begin)
{
    size_t begin;

    begin = 0u;
    if (payload == NULL || local_size == 0u || local_begin >= payload_size ||
        local_begin + local_size > payload_size) {
        return 0;
    }

    begin = local_begin;
    while (begin > 0u &&
           sixel_webp_xmp_is_name_char(payload[begin - 1u]) != 0) {
        --begin;
    }
    if (local_begin == 0u || payload[local_begin - 1u] != ':') {
        return 0;
    }
    if (begin == local_begin - 1u) {
        return 0;
    }
    if (local_begin + local_size < payload_size &&
        sixel_webp_xmp_is_name_char(payload[local_begin + local_size]) != 0) {
        return 0;
    }
    if (name_begin != NULL) {
        *name_begin = begin;
    }
    return 1;
}

static int
sixel_webp_xmp_find_token(unsigned char const *payload,
                          size_t payload_size,
                          char const *token,
                          size_t token_size,
                          size_t start,
                          size_t *found_offset)
{
    size_t offset;

    offset = 0u;
    if (payload == NULL || token == NULL || found_offset == NULL ||
        token_size == 0u || payload_size < token_size || start > payload_size) {
        return 0;
    }
    for (offset = start; offset + token_size <= payload_size; ++offset) {
        if (memcmp(payload + offset, token, token_size) == 0) {
            *found_offset = offset;
            return 1;
        }
    }
    return 0;
}

static int
sixel_webp_xmp_find_tag_end(unsigned char const *payload,
                            size_t payload_size,
                            size_t start,
                            size_t *tag_end)
{
    size_t cursor;
    unsigned char quote;
    unsigned char ch;

    cursor = 0u;
    quote = '\0';
    ch = '\0';
    if (payload == NULL || tag_end == NULL || start >= payload_size) {
        return 0;
    }

    cursor = start;
    while (cursor < payload_size) {
        ch = payload[cursor];
        if (quote == '\0') {
            if (ch == '"' || ch == '\'') {
                quote = ch;
            } else if (ch == '>') {
                *tag_end = cursor;
                return 1;
            }
        } else if (ch == quote) {
            quote = '\0';
        }
        ++cursor;
    }
    return 0;
}

static int
sixel_webp_xmp_parse_orientation(unsigned char const *payload,
                                 size_t payload_size,
                                 int *orientation)
{
    static char const attr_token[] = "tiff:Orientation";
    static char const elem_begin[] = "<tiff:Orientation>";
    static char const elem_end[] = "</tiff:Orientation>";
    size_t offset;
    size_t cursor;
    size_t end_offset;
    unsigned char quote;
    unsigned char digit;
    int parsed;

    offset = 0u;
    cursor = 0u;
    end_offset = 0u;
    quote = '\0';
    digit = 0u;
    parsed = 0;
    if (payload == NULL || orientation == NULL) {
        return 0;
    }
    *orientation = 1;

    while (sixel_webp_xmp_find_token(payload,
                                     payload_size,
                                     attr_token,
                                     sizeof(attr_token) - 1u,
                                     offset,
                                     &cursor) != 0) {
        cursor += sizeof(attr_token) - 1u;
        while (cursor < payload_size &&
               sixel_webp_xmp_is_space(payload[cursor]) != 0) {
            ++cursor;
        }
        if (cursor >= payload_size || payload[cursor] != '=') {
            offset = cursor;
            continue;
        }
        ++cursor;
        while (cursor < payload_size &&
               sixel_webp_xmp_is_space(payload[cursor]) != 0) {
            ++cursor;
        }
        if (cursor >= payload_size) {
            break;
        }
        quote = payload[cursor];
        if (quote != '"' && quote != '\'') {
            offset = cursor;
            continue;
        }
        ++cursor;
        if (cursor >= payload_size || payload[cursor] < '1' ||
            payload[cursor] > '8') {
            offset = cursor;
            continue;
        }
        digit = payload[cursor];
        ++cursor;
        while (cursor < payload_size &&
               sixel_webp_xmp_is_space(payload[cursor]) != 0) {
            ++cursor;
        }
        if (cursor >= payload_size || payload[cursor] != quote) {
            offset = cursor;
            continue;
        }
        parsed = (int)(digit - (unsigned char)'0');
        *orientation = parsed;
        return 1;
    }

    offset = 0u;
    while (sixel_webp_xmp_find_token(payload,
                                     payload_size,
                                     elem_begin,
                                     sizeof(elem_begin) - 1u,
                                     offset,
                                     &cursor) != 0) {
        cursor += sizeof(elem_begin) - 1u;
        while (cursor < payload_size &&
               sixel_webp_xmp_is_space(payload[cursor]) != 0) {
            ++cursor;
        }
        if (cursor >= payload_size || payload[cursor] < '1' ||
            payload[cursor] > '8') {
            offset = cursor;
            continue;
        }
        digit = payload[cursor];
        ++cursor;
        while (cursor < payload_size &&
               sixel_webp_xmp_is_space(payload[cursor]) != 0) {
            ++cursor;
        }
        if (sixel_webp_xmp_find_token(payload,
                                      payload_size,
                                      elem_end,
                                      sizeof(elem_end) - 1u,
                                      cursor,
                                      &end_offset) == 0 ||
            end_offset != cursor) {
            offset = cursor;
            continue;
        }
        parsed = (int)(digit - (unsigned char)'0');
        *orientation = parsed;
        return 1;
    }
    return 0;
}

static unsigned char
sixel_webp_xmp_ascii_tolower(unsigned char ch)
{
    if (ch >= 'A' && ch <= 'Z') {
        return (unsigned char)(ch + ('a' - 'A'));
    }
    return ch;
}

static int
sixel_webp_xmp_span_case_equal(unsigned char const *payload,
                               size_t begin,
                               size_t end,
                               char const *text)
{
    size_t payload_cursor;
    size_t text_cursor;
    size_t text_size;
    unsigned char left;
    unsigned char right;
    int payload_space;
    int text_space;

    payload_cursor = 0u;
    text_cursor = 0u;
    text_size = 0u;
    left = '\0';
    right = '\0';
    payload_space = 0;
    text_space = 0;
    if (payload == NULL || text == NULL || begin > end) {
        return 0;
    }

    /*
     * Compare one trimmed XMP span against an alias using ASCII fold.
     * Treat contiguous ASCII whitespace as one separator so attributes
     * such as "Display\tP3" and "Display P3" map to the same profile.
     */
    text_size = strlen(text);
    payload_cursor = begin;
    text_cursor = 0u;

    while (payload_cursor < end && text_cursor < text_size) {
        payload_space =
            sixel_webp_xmp_is_space(payload[payload_cursor]);
        text_space = sixel_webp_xmp_is_space(
            (unsigned char)text[text_cursor]);
        if (payload_space != 0 || text_space != 0) {
            if (payload_space == 0 || text_space == 0) {
                return 0;
            }
            while (payload_cursor < end &&
                   sixel_webp_xmp_is_space(payload[payload_cursor]) != 0) {
                ++payload_cursor;
            }
            while (text_cursor < text_size &&
                   sixel_webp_xmp_is_space(
                       (unsigned char)text[text_cursor]) != 0) {
                ++text_cursor;
            }
            continue;
        }

        left = sixel_webp_xmp_ascii_tolower(payload[payload_cursor]);
        right = sixel_webp_xmp_ascii_tolower(
            (unsigned char)text[text_cursor]);
        if (left != right) {
            return 0;
        }
        ++payload_cursor;
        ++text_cursor;
    }

    while (payload_cursor < end &&
           sixel_webp_xmp_is_space(payload[payload_cursor]) != 0) {
        ++payload_cursor;
    }
    while (text_cursor < text_size &&
           sixel_webp_xmp_is_space((unsigned char)text[text_cursor]) != 0) {
        ++text_cursor;
    }

    return (payload_cursor == end && text_cursor == text_size) ? 1 : 0;
}

static int
sixel_webp_xmp_match_profile_name(unsigned char const *payload,
                                  size_t value_begin,
                                  size_t value_end,
                                  sixel_webp_xmp_cms_profile_kind_t *kind)
{
    /*
     * Keep profile aliases explicit so accepted spellings stay stable
     * across platforms and parser refactors.
     */
    static sixel_webp_xmp_profile_alias_t const profile_aliases[] = {
        { "sRGB IEC61966-2.1", SIXEL_WEBP_XMP_CMS_PROFILE_SRGB },
        { "sRGB", SIXEL_WEBP_XMP_CMS_PROFILE_SRGB },
        { "IEC 61966-2-1", SIXEL_WEBP_XMP_CMS_PROFILE_SRGB },
        { "IEC61966-2.1", SIXEL_WEBP_XMP_CMS_PROFILE_SRGB },
        { "Display P3", SIXEL_WEBP_XMP_CMS_PROFILE_DISPLAY_P3 },
        { "DisplayP3", SIXEL_WEBP_XMP_CMS_PROFILE_DISPLAY_P3 },
        { "Adobe RGB (1998)", SIXEL_WEBP_XMP_CMS_PROFILE_ADOBE_RGB_1998 },
        { "Adobe RGB 1998", SIXEL_WEBP_XMP_CMS_PROFILE_ADOBE_RGB_1998 },
        { "AdobeRGB1998", SIXEL_WEBP_XMP_CMS_PROFILE_ADOBE_RGB_1998 },
        { "AdobeRGB", SIXEL_WEBP_XMP_CMS_PROFILE_ADOBE_RGB_1998 }
    };
    size_t alias_count;
    size_t alias_index;

    alias_count = sizeof(profile_aliases) / sizeof(profile_aliases[0]);
    alias_index = 0u;
    if (payload == NULL || kind == NULL || value_begin > value_end) {
        return 0;
    }

    while (value_begin < value_end &&
           sixel_webp_xmp_is_space(payload[value_begin]) != 0) {
        ++value_begin;
    }
    while (value_end > value_begin &&
           sixel_webp_xmp_is_space(payload[value_end - 1u]) != 0) {
        --value_end;
    }
    if (value_begin >= value_end) {
        return 0;
    }

    for (alias_index = 0u; alias_index < alias_count; ++alias_index) {
        if (sixel_webp_xmp_span_case_equal(payload,
                                           value_begin,
                                           value_end,
                                           profile_aliases[alias_index].name) !=
            0) {
            *kind = profile_aliases[alias_index].kind;
            return 1;
        }
    }
    *kind = SIXEL_WEBP_XMP_CMS_PROFILE_UNKNOWN;
    return 0;
}

static int
sixel_webp_xmp_parse_icc_profile_kind(unsigned char const *payload,
                                      size_t payload_size,
                                      sixel_webp_xmp_cms_profile_kind_t *kind)
{
    static char const local_name[] = "ICCProfile";
    size_t offset;
    size_t cursor;
    size_t name_begin;
    size_t close_name_begin;
    size_t tag_end;
    size_t close_cursor;
    size_t value_begin;
    size_t value_end;
    size_t local_size;
    unsigned char quote;
    int close_found;

    offset = 0u;
    cursor = 0u;
    name_begin = 0u;
    close_name_begin = 0u;
    tag_end = 0u;
    close_cursor = 0u;
    value_begin = 0u;
    value_end = 0u;
    local_size = sizeof(local_name) - 1u;
    quote = '\0';
    close_found = 0;
    if (payload == NULL || kind == NULL) {
        return 0;
    }
    *kind = SIXEL_WEBP_XMP_CMS_PROFILE_UNKNOWN;

    while (sixel_webp_xmp_find_token(payload,
                                     payload_size,
                                     local_name,
                                     local_size,
                                     offset,
                                     &cursor) != 0) {
        if (sixel_webp_xmp_find_prefixed_local_name(payload,
                                                    payload_size,
                                                    cursor,
                                                    local_size,
                                                    NULL) == 0) {
            offset = cursor + 1u;
            continue;
        }
        cursor += local_size;
        while (cursor < payload_size &&
               sixel_webp_xmp_is_space(payload[cursor]) != 0) {
            ++cursor;
        }
        if (cursor >= payload_size || payload[cursor] != '=') {
            offset = cursor;
            continue;
        }
        ++cursor;
        while (cursor < payload_size &&
               sixel_webp_xmp_is_space(payload[cursor]) != 0) {
            ++cursor;
        }
        if (cursor >= payload_size) {
            break;
        }
        quote = payload[cursor];
        if (quote != '"' && quote != '\'') {
            offset = cursor;
            continue;
        }
        ++cursor;
        value_begin = cursor;
        while (cursor < payload_size && payload[cursor] != quote) {
            ++cursor;
        }
        if (cursor >= payload_size) {
            break;
        }
        value_end = cursor;
        if (sixel_webp_xmp_match_profile_name(payload,
                                              value_begin,
                                              value_end,
                                              kind) != 0) {
            return 1;
        }
        offset = cursor + 1u;
    }

    offset = 0u;
    while (sixel_webp_xmp_find_token(payload, payload_size, local_name,
                                     local_size, offset, &cursor) != 0) {
        if (sixel_webp_xmp_find_prefixed_local_name(payload,
                                                    payload_size,
                                                    cursor,
                                                    local_size,
                                                    &name_begin) == 0) {
            offset = cursor + 1u;
            continue;
        }
        if (name_begin < 1u || payload[name_begin - 1u] != '<') {
            offset = cursor + 1u;
            continue;
        }
        cursor += local_size;
        if (sixel_webp_xmp_find_tag_end(payload,
                                        payload_size,
                                        cursor,
                                        &tag_end) == 0) {
            offset = cursor;
            continue;
        }
        value_begin = tag_end + 1u;
        close_cursor = value_begin;
        close_found = 0;
        while (sixel_webp_xmp_find_token(payload, payload_size, local_name,
                                         local_size, close_cursor,
                                         &close_cursor) != 0) {
            if (sixel_webp_xmp_find_prefixed_local_name(payload,
                                                        payload_size,
                                                        close_cursor,
                                                        local_size,
                                                        &close_name_begin) == 0) {
                close_cursor += 1u;
                continue;
            }
            if (close_name_begin < 2u ||
                payload[close_name_begin - 2u] != '<' ||
                payload[close_name_begin - 1u] != '/') {
                close_cursor += 1u;
                continue;
            }
            tag_end = close_cursor + local_size;
            while (tag_end < payload_size &&
                   sixel_webp_xmp_is_space(payload[tag_end]) != 0) {
                ++tag_end;
            }
            if (tag_end >= payload_size || payload[tag_end] != '>') {
                close_cursor += 1u;
                continue;
            }
            value_end = close_name_begin - 2u;
            close_found = 1;
            break;
        }
        if (close_found == 0 || value_end < value_begin) {
            break;
        }
        if (sixel_webp_xmp_match_profile_name(payload,
                                              value_begin,
                                              value_end,
                                              kind) != 0) {
            return 1;
        }
        offset = tag_end + 1u;
        value_end = 0u;
    }
    return 0;
}

static sixel_cms_profile_t *
sixel_webp_create_xmp_profile(sixel_webp_xmp_cms_profile_kind_t kind)
{
    switch (kind) {
    case SIXEL_WEBP_XMP_CMS_PROFILE_SRGB:
        return sixel_cms_create_srgb_profile();
    case SIXEL_WEBP_XMP_CMS_PROFILE_DISPLAY_P3:
        return sixel_cms_create_rgb_profile_from_gamma_chrm(
            2.2,
            0.3127,
            0.3290,
            0.6800,
            0.3200,
            0.2650,
            0.6900,
            0.1500,
            0.0600);
    case SIXEL_WEBP_XMP_CMS_PROFILE_ADOBE_RGB_1998:
        return sixel_cms_create_rgb_profile_from_gamma_chrm(
            2.2,
            0.3127,
            0.3290,
            0.6400,
            0.3300,
            0.2100,
            0.7100,
            0.1500,
            0.0600);
    default:
        break;
    }
    return NULL;
}

static SIXELSTATUS
sixel_webp_try_apply_xmp_cms(sixel_webp_decode_plan_t const *plan,
                             int enable_cms,
                             sixel_frame_t *frame,
                             sixel_allocator_t *allocator,
                             int *applied,
                             int *size_limited)
{
    sixel_webp_xmp_cms_profile_kind_t kind;
    sixel_cms_profile_t *src_profile;
    int found_profile;
    int converted;

    kind = SIXEL_WEBP_XMP_CMS_PROFILE_UNKNOWN;
    src_profile = NULL;
    found_profile = 0;
    converted = 0;
    if (plan == NULL || frame == NULL || applied == NULL ||
        size_limited == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *applied = 0;
    *size_limited = 0;
    if (plan->meta_has_xmp == 0 ||
        enable_cms == 0 ||
        plan->meta_has_iccp != 0 ||
        plan->xmp_payload == NULL ||
        plan->xmp_payload_size == 0u) {
        return SIXEL_OK;
    }
    if (sixel_webp_xmp_payload_exceeds_parse_limit(plan->xmp_payload_size) !=
        0) {
        *size_limited = 1;
        return SIXEL_OK;
    }

    found_profile = sixel_webp_xmp_parse_icc_profile_kind(
        plan->xmp_payload,
        plan->xmp_payload_size,
        &kind);
    if (found_profile == 0 || kind == SIXEL_WEBP_XMP_CMS_PROFILE_UNKNOWN) {
        return SIXEL_OK;
    }

    src_profile = sixel_webp_create_xmp_profile(kind);
    if (src_profile == NULL) {
        return SIXEL_OK;
    }
    converted = sixel_webp_apply_profile_to_srgb_rgba(frame->pixels.u8ptr,
                                                      frame->width,
                                                      frame->height,
                                                      src_profile,
                                                      allocator);
    sixel_cms_close_profile(src_profile);
    if (converted != 0) {
        *applied = 1;
    }
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_webp_try_apply_xmp_orientation(sixel_webp_decode_plan_t const *plan,
                                     int enable_orientation,
                                     sixel_frame_t *frame,
                                     int *applied,
                                     int *size_limited)
{
    SIXELSTATUS status;
    int parsed_orientation;
    int found_orientation;

    status = SIXEL_OK;
    parsed_orientation = 1;
    found_orientation = 0;

    if (plan == NULL || frame == NULL || applied == NULL ||
        size_limited == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *applied = 0;
    *size_limited = 0;
    if (plan->meta_has_xmp == 0 ||
        enable_orientation == 0 ||
        plan->xmp_payload == NULL ||
        plan->xmp_payload_size == 0u ||
        plan->meta_has_exif != 0) {
        return SIXEL_OK;
    }
    if (sixel_webp_xmp_payload_exceeds_parse_limit(plan->xmp_payload_size) !=
        0) {
        *size_limited = 1;
        return SIXEL_OK;
    }

    found_orientation = sixel_webp_xmp_parse_orientation(plan->xmp_payload,
                                                         plan->xmp_payload_size,
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

static void
sixel_webp_anim_meta_cache_reset(sixel_webp_anim_meta_cache_t *cache)
{
    if (cache == NULL) {
        return;
    }
    if (cache->iccp_profile != NULL) {
        sixel_cms_close_profile(cache->iccp_profile);
    }
    if (cache->xmp_cms_profile != NULL) {
        sixel_cms_close_profile(cache->xmp_cms_profile);
    }
    memset(cache, 0, sizeof(*cache));
}

static SIXELSTATUS
sixel_webp_build_anim_meta_cache(sixel_webp_decode_plan_t const *plan,
                                 int enable_cms,
                                 int enable_orientation,
                                 sixel_webp_anim_meta_cache_t *cache)
{
    sixel_webp_xmp_cms_profile_kind_t kind;
    int found_profile;
    int parsed_orientation;
    int found_orientation;

    kind = SIXEL_WEBP_XMP_CMS_PROFILE_UNKNOWN;
    found_profile = 0;
    parsed_orientation = 0;
    found_orientation = 0;
    if (plan == NULL || cache == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    memset(cache, 0, sizeof(*cache));

    if (plan->meta_has_iccp != 0 &&
        enable_cms != 0 &&
        plan->iccp_payload != NULL &&
        plan->iccp_payload_size != 0u) {
        cache->iccp_profile = sixel_cms_open_profile_from_mem(
            plan->iccp_payload,
            plan->iccp_payload_size);
    }

    if (plan->meta_has_xmp != 0 &&
        enable_cms != 0 &&
        plan->meta_has_iccp == 0 &&
        plan->xmp_payload != NULL &&
        plan->xmp_payload_size != 0u) {
        if (sixel_webp_xmp_payload_exceeds_parse_limit(
                plan->xmp_payload_size) != 0) {
            cache->xmp_cms_size_limited = 1;
        } else {
            found_profile = sixel_webp_xmp_parse_icc_profile_kind(
                plan->xmp_payload,
                plan->xmp_payload_size,
                &kind);
            if (found_profile != 0 &&
                kind != SIXEL_WEBP_XMP_CMS_PROFILE_UNKNOWN) {
                cache->xmp_cms_profile = sixel_webp_create_xmp_profile(kind);
            }
        }
    }

    if (plan->meta_has_exif != 0 &&
        enable_orientation != 0 &&
        plan->exif_payload != NULL &&
        plan->exif_payload_size != 0u) {
        found_orientation = loader_exif_parse_orientation(
            plan->exif_payload,
            plan->exif_payload_size,
            &parsed_orientation);
        if (found_orientation != 0 &&
            parsed_orientation >= 2 &&
            parsed_orientation <= 8) {
            cache->exif_orientation = parsed_orientation;
        }
    }

    if (plan->meta_has_xmp != 0 &&
        enable_orientation != 0 &&
        plan->meta_has_exif == 0 &&
        plan->xmp_payload != NULL &&
        plan->xmp_payload_size != 0u) {
        if (sixel_webp_xmp_payload_exceeds_parse_limit(
                plan->xmp_payload_size) != 0) {
            cache->xmp_orientation_size_limited = 1;
        } else {
            found_orientation = sixel_webp_xmp_parse_orientation(
                plan->xmp_payload,
                plan->xmp_payload_size,
                &parsed_orientation);
            if (found_orientation != 0 &&
                parsed_orientation >= 2 &&
                parsed_orientation <= 8) {
                cache->xmp_orientation = parsed_orientation;
            }
        }
    }

    return SIXEL_OK;
}

static int
sixel_webp_apply_anim_cached_iccp(sixel_webp_anim_meta_cache_t const *cache,
                                  sixel_frame_t *frame,
                                  sixel_allocator_t *allocator)
{
    int converted;

    converted = 0;
    if (cache == NULL || frame == NULL || allocator == NULL ||
        cache->iccp_profile == NULL) {
        return 0;
    }
    converted = sixel_webp_apply_profile_to_srgb_rgba(frame->pixels.u8ptr,
                                                      frame->width,
                                                      frame->height,
                                                      cache->iccp_profile,
                                                      allocator);
    return converted != 0 ? 1 : 0;
}

static int
sixel_webp_apply_anim_cached_xmp_cms(
    sixel_webp_anim_meta_cache_t const *cache,
    sixel_frame_t *frame,
    sixel_allocator_t *allocator)
{
    int converted;

    converted = 0;
    if (cache == NULL || frame == NULL || allocator == NULL ||
        cache->xmp_cms_profile == NULL) {
        return 0;
    }
    converted = sixel_webp_apply_profile_to_srgb_rgba(frame->pixels.u8ptr,
                                                      frame->width,
                                                      frame->height,
                                                      cache->xmp_cms_profile,
                                                      allocator);
    return converted != 0 ? 1 : 0;
}

static SIXELSTATUS
sixel_webp_apply_anim_cached_orientation(sixel_frame_t *frame,
                                         int orientation,
                                         int *applied)
{
    SIXELSTATUS status;

    status = SIXEL_OK;
    if (frame == NULL || applied == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *applied = 0;
    if (orientation < 2 || orientation > 8) {
        return SIXEL_OK;
    }
    status = loader_frame_apply_orientation(frame, orientation);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    *applied = 1;
    return SIXEL_OK;
}

static void
sixel_webp_anim_stream_reset(sixel_webp_anim_stream_t *stream,
                             sixel_allocator_t *allocator)
{
    if (stream == NULL) {
        return;
    }
    if (stream->frames != NULL && allocator != NULL) {
        sixel_allocator_free(allocator, stream->frames);
    }
    memset(stream, 0, sizeof(*stream));
}

static SIXELSTATUS
sixel_webp_parse_anmf_frame(unsigned char const *payload,
                            size_t payload_size,
                            int canvas_width,
                            int canvas_height,
                            sixel_webp_anim_frame_t *frame)
{
    unsigned int x;
    unsigned int y;
    unsigned int width_minus_one;
    unsigned int height_minus_one;
    unsigned int frame_width_u;
    unsigned int frame_height_u;
    unsigned int duration_ms;
    unsigned int flags;
    unsigned int dispose_flag;
    unsigned int blend_flag;
    unsigned long long right;
    unsigned long long bottom;
    unsigned int fourcc;
    unsigned int chunk_size_u32;
    size_t offset;
    size_t chunk_size;
    size_t chunk_total_size;

    x = 0u;
    y = 0u;
    width_minus_one = 0u;
    height_minus_one = 0u;
    frame_width_u = 0u;
    frame_height_u = 0u;
    duration_ms = 0u;
    flags = 0u;
    dispose_flag = 0u;
    blend_flag = 0u;
    right = 0u;
    bottom = 0u;
    fourcc = 0u;
    chunk_size_u32 = 0u;
    offset = 0u;
    chunk_size = 0u;
    chunk_total_size = 0u;
    if (payload == NULL || payload_size < 24u ||
        frame == NULL || canvas_width <= 0 || canvas_height <= 0) {
        return SIXEL_BAD_INPUT;
    }

    memset(frame, 0, sizeof(*frame));
    x = sixel_webp_anim_read_u24le(payload + 0u) * 2u;
    y = sixel_webp_anim_read_u24le(payload + 3u) * 2u;
    width_minus_one = sixel_webp_anim_read_u24le(payload + 6u);
    height_minus_one = sixel_webp_anim_read_u24le(payload + 9u);
    frame_width_u = width_minus_one + 1u;
    frame_height_u = height_minus_one + 1u;
    duration_ms = sixel_webp_anim_read_u24le(payload + 12u);
    flags = (unsigned int)payload[15u];
    if ((flags & 0xfcu) != 0u) {
        sixel_helper_set_additional_message(
            "builtin webp: ANMF frame flags contain reserved bits.");
        return SIXEL_BAD_INPUT;
    }
    if (x > (unsigned int)INT_MAX ||
        y > (unsigned int)INT_MAX ||
        frame_width_u > (unsigned int)INT_MAX ||
        frame_height_u > (unsigned int)INT_MAX) {
        sixel_helper_set_additional_message(
            "builtin webp: ANMF frame rectangle is out of range.");
        return SIXEL_BAD_INPUT;
    }
    right = (unsigned long long)x + (unsigned long long)frame_width_u;
    bottom = (unsigned long long)y + (unsigned long long)frame_height_u;
    if (right > (unsigned long long)canvas_width ||
        bottom > (unsigned long long)canvas_height) {
        sixel_helper_set_additional_message(
            "builtin webp: ANMF frame rectangle exceeds canvas bounds.");
        return SIXEL_BAD_INPUT;
    }

    /*
     * ANMF bit 0 is dispose-to-background and bit 1 toggles blending:
     * 0 => blend-over previous canvas, 1 => replace destination.
     */
    dispose_flag = flags & 0x01u;
    blend_flag = flags & 0x02u;
    frame->x_offset = (int)x;
    frame->y_offset = (int)y;
    frame->width = (int)frame_width_u;
    frame->height = (int)frame_height_u;
    frame->duration_ms = (int)duration_ms;
    frame->dispose_to_background = dispose_flag != 0u ? 1 : 0;
    frame->blend_over = blend_flag == 0u ? 1 : 0;

    offset = 16u;
    while (offset < payload_size) {
        if (payload_size - offset < 8u) {
            return SIXEL_BAD_INPUT;
        }
        fourcc = sixel_webp_anim_read_u32le(payload + offset);
        chunk_size_u32 = sixel_webp_anim_read_u32le(payload + offset + 4u);
        chunk_size = (size_t)chunk_size_u32;
        chunk_total_size = 8u + chunk_size + (chunk_size & 1u);
        if (chunk_total_size > payload_size - offset) {
            return SIXEL_BAD_INPUT;
        }
        if ((chunk_size_u32 & 1u) != 0u &&
            payload[offset + 8u + chunk_size] != 0u) {
            return SIXEL_BAD_INPUT;
        }

        if (fourcc == SIXEL_WEBP_CHUNK_ALPHA) {
            if (frame->alpha_payload != NULL) {
                return SIXEL_BAD_INPUT;
            }
            frame->alpha_payload = payload + offset + 8u;
            frame->alpha_payload_size = chunk_size;
        } else if (fourcc == SIXEL_WEBP_CHUNK_VP8) {
            if (frame->vp8_payload != NULL || frame->vp8l_payload != NULL) {
                return SIXEL_BAD_INPUT;
            }
            frame->vp8_payload = payload + offset + 8u;
            frame->vp8_payload_size = chunk_size;
        } else if (fourcc == SIXEL_WEBP_CHUNK_VP8L) {
            if (frame->vp8l_payload != NULL || frame->vp8_payload != NULL ||
                frame->alpha_payload != NULL) {
                return SIXEL_BAD_INPUT;
            }
            frame->vp8l_payload = payload + offset + 8u;
            frame->vp8l_payload_size = chunk_size;
        }
        offset += chunk_total_size;
    }

    if (frame->vp8_payload != NULL && frame->alpha_payload != NULL) {
        frame->kind = SIXEL_WEBP_CONTAINER_KIND_VP8_ALPHA_STATIC;
        return SIXEL_OK;
    }
    if (frame->vp8_payload != NULL) {
        frame->kind = SIXEL_WEBP_CONTAINER_KIND_VP8_STATIC;
        return SIXEL_OK;
    }
    if (frame->vp8l_payload != NULL) {
        frame->kind = SIXEL_WEBP_CONTAINER_KIND_VP8L_STATIC;
        return SIXEL_OK;
    }

    sixel_helper_set_additional_message(
        "builtin webp: ANMF payload has no decodable frame chunk.");
    return SIXEL_BAD_INPUT;
}

static SIXELSTATUS
sixel_webp_parse_anim_stream(sixel_chunk_t const *chunk,
                             sixel_allocator_t *allocator,
                             sixel_webp_decode_plan_t const *plan,
                             sixel_webp_anim_stream_t *stream)
{
    SIXELSTATUS status;
    sixel_chunk_bytes_view_t view;
    unsigned char const *data;
    size_t size;
    unsigned int riff_size_u32;
    size_t riff_total_size;
    unsigned int fourcc;
    unsigned int chunk_size_u32;
    size_t chunk_size;
    size_t chunk_total_size;
    size_t offset;
    int frame_index;

    status = SIXEL_OK;
    view.bytes = NULL;
    view.size = 0u;
    data = NULL;
    size = 0u;
    riff_size_u32 = 0u;
    riff_total_size = 0u;
    fourcc = 0u;
    chunk_size_u32 = 0u;
    chunk_size = 0u;
    chunk_total_size = 0u;
    offset = 0u;
    frame_index = 0;
    if (chunk == NULL || plan == NULL || stream == NULL ||
        plan->anim_frame_count <= 0 ||
        plan->canvas_width <= 0 ||
        plan->canvas_height <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (allocator == NULL ||
        sixel_chunk_get_bytes(chunk, &view) != SIXEL_OK ||
        view.bytes == NULL || view.size < 12u) {
        return SIXEL_BAD_ARGUMENT;
    }

    memset(stream, 0, sizeof(*stream));
    stream->canvas_width = plan->canvas_width;
    stream->canvas_height = plan->canvas_height;
    stream->loop_count = plan->anim_loop_count;
    stream->frame_count = plan->anim_frame_count;
    stream->frames = (sixel_webp_anim_frame_t *)sixel_allocator_calloc(
        allocator,
        (size_t)stream->frame_count,
        sizeof(*stream->frames));
    if (stream->frames == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    data = view.bytes;
    size = view.size;
    if (memcmp(data, "RIFF", 4u) != 0 || memcmp(data + 8u, "WEBP", 4u) != 0) {
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    riff_size_u32 = sixel_webp_anim_read_u32le(data + 4u);
    riff_total_size = (size_t)riff_size_u32 + 8u;
    if (riff_total_size < (size_t)riff_size_u32) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto end;
    }
    if (riff_total_size > size) {
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    offset = 12u;
    while (offset < riff_total_size) {
        if (riff_total_size - offset < 8u) {
            status = SIXEL_BAD_INPUT;
            goto end;
        }
        fourcc = sixel_webp_anim_read_u32le(data + offset);
        chunk_size_u32 = sixel_webp_anim_read_u32le(data + offset + 4u);
        chunk_size = (size_t)chunk_size_u32;
        chunk_total_size = 8u + chunk_size + (chunk_size & 1u);
        if (chunk_total_size > riff_total_size - offset) {
            status = SIXEL_BAD_INPUT;
            goto end;
        }

        if (fourcc == SIXEL_WEBP_CHUNK_ANMF) {
            if (frame_index >= stream->frame_count) {
                status = SIXEL_BAD_INPUT;
                goto end;
            }
            status = sixel_webp_parse_anmf_frame(
                data + offset + 8u,
                chunk_size,
                stream->canvas_width,
                stream->canvas_height,
                &stream->frames[frame_index]);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
            ++frame_index;
        }
        offset += chunk_total_size;
    }
    if (frame_index != stream->frame_count) {
        status = SIXEL_BAD_INPUT;
        goto end;
    }

end:
    if (SIXEL_FAILED(status)) {
        sixel_webp_anim_stream_reset(stream, allocator);
    }
    return status;
}

static int
sixel_webp_vp8l_payload_header_has_alpha(unsigned char const *payload,
                                         size_t payload_size,
                                         int *has_alpha)
{
    if (payload == NULL || has_alpha == NULL || payload_size < 5u) {
        return 0;
    }
    if (payload[0] != 0x2fu) {
        return 0;
    }
    *has_alpha = (payload[4] & 0x10u) != 0u ? 1 : 0;
    return 1;
}

static SIXELSTATUS
sixel_webp_validate_anim_alpha_flag(sixel_webp_anim_stream_t const *stream,
                                    unsigned int vp8x_flags)
{
    unsigned int alpha_flag_set;
    int has_anim_alpha;
    int vp8l_has_alpha;
    int frame_index;

    alpha_flag_set = 0u;
    has_anim_alpha = 0;
    vp8l_has_alpha = 0;
    frame_index = 0;

    if (stream == NULL || stream->frames == NULL || stream->frame_count <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }

    alpha_flag_set = (vp8x_flags & SIXEL_WEBP_VP8X_ALPHA_FLAG) != 0u;
    for (frame_index = 0; frame_index < stream->frame_count; ++frame_index) {
        if (stream->frames[frame_index].kind ==
            SIXEL_WEBP_CONTAINER_KIND_VP8_ALPHA_STATIC) {
            has_anim_alpha = 1;
            break;
        }
        if (stream->frames[frame_index].kind !=
            SIXEL_WEBP_CONTAINER_KIND_VP8L_STATIC) {
            continue;
        }
        vp8l_has_alpha = 0;
        if (sixel_webp_vp8l_payload_header_has_alpha(
                stream->frames[frame_index].vp8l_payload,
                stream->frames[frame_index].vp8l_payload_size,
                &vp8l_has_alpha) == 0) {
            continue;
        }
        if (vp8l_has_alpha != 0) {
            has_anim_alpha = 1;
            break;
        }
    }

    if (has_anim_alpha != 0 && alpha_flag_set == 0u) {
        sixel_helper_set_additional_message(
            "builtin webp: VP8X alpha flag does not match ANMF alpha "
            "frames.");
        return SIXEL_BAD_INPUT;
    }
    if (has_anim_alpha != 0) {
        return SIXEL_OK;
    }
    /*
     * Keep decoder compatibility with libwebp: ANIM streams with VP8X alpha
     * set but no actual frame alpha stay decodable.
     */
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_webp_decode_anim_frame_rgba(sixel_webp_anim_frame_t const *anim_frame,
                                  sixel_allocator_t *allocator,
                                  unsigned char **prgba,
                                  int *pwidth,
                                  int *pheight)
{
    SIXELSTATUS status;

    status = SIXEL_OK;
    if (anim_frame == NULL || allocator == NULL || prgba == NULL ||
        pwidth == NULL || pheight == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *prgba = NULL;
    *pwidth = 0;
    *pheight = 0;

    if (anim_frame->kind == SIXEL_WEBP_CONTAINER_KIND_VP8_STATIC ||
        anim_frame->kind == SIXEL_WEBP_CONTAINER_KIND_VP8_ALPHA_STATIC) {
        status = sixel_webp_decode_vp8_payload(anim_frame->vp8_payload,
                                               anim_frame->vp8_payload_size,
                                               prgba,
                                               pwidth,
                                               pheight,
                                               allocator);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        if (anim_frame->kind == SIXEL_WEBP_CONTAINER_KIND_VP8_ALPHA_STATIC) {
            status = sixel_webp_apply_vp8_alpha_payload(
                *prgba,
                *pwidth,
                *pheight,
                anim_frame->alpha_payload,
                anim_frame->alpha_payload_size,
                allocator);
            if (SIXEL_FAILED(status)) {
                sixel_allocator_free(allocator, *prgba);
                *prgba = NULL;
                return status;
            }
        }
        if (*pwidth != anim_frame->width || *pheight != anim_frame->height) {
            sixel_allocator_free(allocator, *prgba);
            *prgba = NULL;
            *pwidth = 0;
            *pheight = 0;
            return SIXEL_BAD_INPUT;
        }
        return SIXEL_OK;
    }

    if (anim_frame->kind == SIXEL_WEBP_CONTAINER_KIND_VP8L_STATIC) {
        status = sixel_webp_decode_vp8l_payload(anim_frame->vp8l_payload,
                                                anim_frame->vp8l_payload_size,
                                                prgba,
                                                pwidth,
                                                pheight,
                                                allocator);
        if (SIXEL_SUCCEEDED(status) &&
            (*pwidth != anim_frame->width || *pheight != anim_frame->height)) {
            sixel_allocator_free(allocator, *prgba);
            *prgba = NULL;
            *pwidth = 0;
            *pheight = 0;
            return SIXEL_BAD_INPUT;
        }
        return status;
    }
    return SIXEL_BAD_INPUT;
}

static void
sixel_webp_trace_anim_frame_error(sixel_webp_anim_frame_t const *anim_frame,
                                  SIXELSTATUS status)
{
    (void)status;

    if (anim_frame == NULL) {
        return;
    }
    if (anim_frame->kind == SIXEL_WEBP_CONTAINER_KIND_VP8L_STATIC) {
        sixel_webp_trace_contract_add_code(SIXEL_WEBP_CODE_ERR_VP8L_STREAM);
        return;
    }

    /*
     * VP8 and VP8+ALPHA frame decode failures are both normalized to stream
     * corruption in the current builtin WebP contract.
     */
    sixel_webp_trace_contract_add_code(SIXEL_WEBP_CODE_ERR_VP8_STREAM);
}

static void
sixel_webp_trace_anim_frame_success(sixel_webp_anim_frame_t const *anim_frame)
{
    if (anim_frame == NULL) {
        return;
    }
    if (anim_frame->kind == SIXEL_WEBP_CONTAINER_KIND_VP8_ALPHA_STATIC) {
        sixel_webp_trace_contract_add_code(
            SIXEL_WEBP_CODE_OK_VP8_ALPHA_STATIC);
    } else if (anim_frame->kind == SIXEL_WEBP_CONTAINER_KIND_VP8_STATIC) {
        sixel_webp_trace_contract_add_code(SIXEL_WEBP_CODE_OK_VP8_STATIC);
    } else if (anim_frame->kind == SIXEL_WEBP_CONTAINER_KIND_VP8L_STATIC) {
        sixel_webp_trace_contract_add_code(SIXEL_WEBP_CODE_OK_VP8L_STATIC);
    }
}

static void
sixel_webp_anim_background_to_rgba(unsigned int anim_background,
                                   unsigned char *rgba)
{
    if (rgba == NULL) {
        return;
    }
    rgba[0] = (unsigned char)((anim_background >> 16u) & 0xffu);
    rgba[1] = (unsigned char)((anim_background >> 8u) & 0xffu);
    rgba[2] = (unsigned char)(anim_background & 0xffu);
    rgba[3] = (unsigned char)((anim_background >> 24u) & 0xffu);
}

static void
sixel_webp_anim_fill_canvas(unsigned char *canvas_pixels,
                            int canvas_width,
                            int canvas_height,
                            unsigned char const *rgba)
{
    int x;
    int y;
    size_t dst_offset;

    x = 0;
    y = 0;
    dst_offset = 0u;
    if (canvas_pixels == NULL || rgba == NULL ||
        canvas_width <= 0 || canvas_height <= 0) {
        return;
    }
    for (y = 0; y < canvas_height; ++y) {
        for (x = 0; x < canvas_width; ++x) {
            dst_offset = ((size_t)y * (size_t)canvas_width + (size_t)x) * 4u;
            canvas_pixels[dst_offset + 0u] = rgba[0u];
            canvas_pixels[dst_offset + 1u] = rgba[1u];
            canvas_pixels[dst_offset + 2u] = rgba[2u];
            canvas_pixels[dst_offset + 3u] = rgba[3u];
        }
    }
}

static void
sixel_webp_anim_clear_rect(unsigned char *canvas_pixels,
                           int canvas_width,
                           sixel_webp_anim_frame_t const *anim_frame,
                           unsigned char const *rgba)
{
    int x;
    int y;
    int px;
    int py;
    size_t dst_offset;

    x = 0;
    y = 0;
    px = 0;
    py = 0;
    dst_offset = 0u;
    if (canvas_pixels == NULL || canvas_width <= 0 || anim_frame == NULL ||
        rgba == NULL) {
        return;
    }
    for (y = 0; y < anim_frame->height; ++y) {
        py = anim_frame->y_offset + y;
        for (x = 0; x < anim_frame->width; ++x) {
            px = anim_frame->x_offset + x;
            dst_offset = ((size_t)py * (size_t)canvas_width
                          + (size_t)px) * 4u;
            canvas_pixels[dst_offset + 0u] = rgba[0u];
            canvas_pixels[dst_offset + 1u] = rgba[1u];
            canvas_pixels[dst_offset + 2u] = rgba[2u];
            canvas_pixels[dst_offset + 3u] = rgba[3u];
        }
    }
}

static void
sixel_webp_anim_composite_rect(unsigned char *canvas_pixels,
                               int canvas_width,
                               sixel_webp_anim_frame_t const *anim_frame,
                               unsigned char const *subframe_pixels)
{
    int x;
    int y;
    int px;
    int py;
    size_t src_offset;
    size_t dst_offset;
    unsigned int sa;
    unsigned int da;
    unsigned int oa;
    unsigned char const *sp;
    unsigned char *dp;

    x = 0;
    y = 0;
    px = 0;
    py = 0;
    src_offset = 0u;
    dst_offset = 0u;
    sa = 0u;
    da = 0u;
    oa = 0u;
    sp = NULL;
    dp = NULL;
    if (canvas_pixels == NULL || canvas_width <= 0 || anim_frame == NULL ||
        subframe_pixels == NULL) {
        return;
    }
    for (y = 0; y < anim_frame->height; ++y) {
        py = anim_frame->y_offset + y;
        for (x = 0; x < anim_frame->width; ++x) {
            px = anim_frame->x_offset + x;
            src_offset = ((size_t)y * (size_t)anim_frame->width
                          + (size_t)x) * 4u;
            dst_offset = ((size_t)py * (size_t)canvas_width
                          + (size_t)px) * 4u;
            sp = subframe_pixels + src_offset;
            dp = canvas_pixels + dst_offset;
            if (anim_frame->blend_over == 0) {
                dp[0u] = sp[0u];
                dp[1u] = sp[1u];
                dp[2u] = sp[2u];
                dp[3u] = sp[3u];
                continue;
            }
            sa = (unsigned int)sp[3u];
            da = (unsigned int)dp[3u];
            oa = sa + ((da * (255u - sa)) / 255u);
            if (oa == 0u) {
                dp[0u] = 0u;
                dp[1u] = 0u;
                dp[2u] = 0u;
                dp[3u] = 0u;
                continue;
            }
            dp[0u] = (unsigned char)((sp[0u] * sa + dp[0u] * da
                                      * (255u - sa) / 255u) / oa);
            dp[1u] = (unsigned char)((sp[1u] * sa + dp[1u] * da
                                      * (255u - sa) / 255u) / oa);
            dp[2u] = (unsigned char)((sp[2u] * sa + dp[2u] * da
                                      * (255u - sa) / 255u) / oa);
            dp[3u] = (unsigned char)oa;
        }
    }
}

static SIXELSTATUS
sixel_webp_anim_copy_canvas(unsigned char const *canvas_pixels,
                            size_t canvas_bytes,
                            sixel_allocator_t *allocator,
                            unsigned char **output)
{
    unsigned char *copied;

    copied = NULL;
    if (output == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *output = NULL;
    if (canvas_pixels == NULL || allocator == NULL || canvas_bytes == 0u) {
        return SIXEL_BAD_ARGUMENT;
    }
    copied = (unsigned char *)sixel_allocator_malloc(allocator, canvas_bytes);
    if (copied == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    memcpy(copied, canvas_pixels, canvas_bytes);
    *output = copied;
    return SIXEL_OK;
}

SIXELSTATUS
sixel_fromwebp_load_animation(sixel_chunk_t const *chunk,
                              sixel_allocator_t *allocator,
                              int fstatic,
                              int loop_control,
                              int start_frame_no_set,
                              int start_frame_no_override,
                              int enable_cms,
                              int enable_orientation,
                              sixel_load_image_function fn_load,
                              void *context,
                              void *cancel_context,
                              int *handled)
{
    SIXELSTATUS status;
    sixel_webp_container_info_t container;
    sixel_webp_decode_plan_t plan;
    sixel_webp_anim_stream_t stream;
    sixel_frame_t *frame;
    unsigned char *rgba;
    unsigned char *canvas_pixels;
    unsigned char *emitted_pixels;
    unsigned char background_rgba[4];
    int subframe_width;
    int subframe_height;
    int emit_callback;
    int loop_no;
    int source_frame_no;
    int emit_frame_no;
    int frames_in_loop;
    int emitted_frames;
    int start_frame_env_set;
    int start_frame_env;
    int start_frame_resolved;
    int effective_start_frame_set;
    int iccp_code_reported;
    int exif_code_reported;
    int xmp_code_reported;
    int xmp_cms_code_reported;
    int iccp_applied;
    int xmp_cms_applied;
    int exif_applied;
    int xmp_applied;
    sixel_webp_anim_meta_cache_t meta_cache;

    status = SIXEL_OK;
    memset(&container, 0, sizeof(container));
    memset(&plan, 0, sizeof(plan));
    memset(&stream, 0, sizeof(stream));
    memset(&meta_cache, 0, sizeof(meta_cache));
    frame = NULL;
    rgba = NULL;
    canvas_pixels = NULL;
    emitted_pixels = NULL;
    memset(background_rgba, 0, sizeof(background_rgba));
    subframe_width = 0;
    subframe_height = 0;
    emit_callback = 0;
    loop_no = 0;
    source_frame_no = 0;
    emit_frame_no = 0;
    frames_in_loop = 0;
    emitted_frames = 0;
    start_frame_env_set = 0;
    start_frame_env = 0;
    start_frame_resolved = 0;
    effective_start_frame_set = 0;
    iccp_code_reported = 0;
    exif_code_reported = 0;
    xmp_code_reported = 0;
    xmp_cms_code_reported = 0;
    iccp_applied = 0;
    xmp_cms_applied = 0;
    exif_applied = 0;
    xmp_applied = 0;
    if (handled == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *handled = 0;
    if (chunk == NULL || allocator == NULL || fn_load == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_webp_parse_container(chunk, &container);
    if (SIXEL_FAILED(status)) {
        return SIXEL_OK;
    }
    status = sixel_webp_build_decode_plan(&container, &plan);
    if (SIXEL_FAILED(status)) {
        return SIXEL_OK;
    }
    if (plan.kind != SIXEL_WEBP_CONTAINER_KIND_ANIM_MVP) {
        return SIXEL_OK;
    }

    *handled = 1;
    status = sixel_webp_parse_anim_stream(chunk, allocator, &plan, &stream);
    if (SIXEL_FAILED(status)) {
        sixel_webp_trace_contract_add_code(SIXEL_WEBP_CODE_ERR_VP8_STREAM);
        goto end;
    }
    status = sixel_webp_validate_anim_alpha_flag(&stream,
                                                 container.vp8x_flags);
    if (SIXEL_FAILED(status)) {
        sixel_webp_trace_contract_add_code(
            SIXEL_WEBP_CODE_ERR_VP8X_FLAG_ALPHA_MISMATCH);
        goto end;
    }
    if ((size_t)stream.canvas_width > SIZE_MAX / (size_t)stream.canvas_height ||
        (size_t)stream.canvas_width * (size_t)stream.canvas_height >
            SIZE_MAX / 4u) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto end;
    }
    canvas_pixels = (unsigned char *)sixel_allocator_malloc(
        allocator,
        (size_t)stream.canvas_width * (size_t)stream.canvas_height * 4u);
    if (canvas_pixels == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    sixel_webp_anim_background_to_rgba((unsigned int)container.anim_background,
                                       background_rgba);

    if (start_frame_no_set != 0) {
        effective_start_frame_set = 1;
        start_frame_resolved = start_frame_no_override;
    } else {
        status = sixel_webp_parse_animation_start_frame_no(&start_frame_env_set,
                                                           &start_frame_env);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        effective_start_frame_set = start_frame_env_set;
        start_frame_resolved = start_frame_env;
    }
    if (effective_start_frame_set != 0) {
        status = sixel_webp_resolve_animation_start_frame_no(
            start_frame_resolved,
            stream.frame_count,
            &start_frame_resolved);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }
    status = sixel_webp_build_anim_meta_cache(&plan,
                                              enable_cms,
                                              enable_orientation,
                                              &meta_cache);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    sixel_webp_trace_anim_timing_contract(&stream);

    for (loop_no = 0;; ++loop_no) {
        if (sixel_loader_callback_is_canceled(cancel_context)) {
            status = SIXEL_INTERRUPTED;
            goto end;
        }
        sixel_webp_anim_fill_canvas(canvas_pixels,
                                    stream.canvas_width,
                                    stream.canvas_height,
                                    background_rgba);
        frames_in_loop = 0;
        for (source_frame_no = 0; source_frame_no < stream.frame_count;
             ++source_frame_no) {
            if (sixel_loader_callback_is_canceled(cancel_context)) {
                status = SIXEL_INTERRUPTED;
                goto end;
            }

            emit_callback = 1;
            if (effective_start_frame_set != 0 &&
                loop_no == 0 &&
                source_frame_no < start_frame_resolved) {
                emit_callback = 0;
            }
            emit_frame_no = source_frame_no;
            if (effective_start_frame_set != 0 && loop_no == 0) {
                emit_frame_no = source_frame_no - start_frame_resolved;
            }

            status = sixel_webp_decode_anim_frame_rgba(
                &stream.frames[source_frame_no],
                allocator,
                &rgba,
                &subframe_width,
                &subframe_height);
            if (SIXEL_FAILED(status)) {
                sixel_webp_trace_anim_frame_error(
                    &stream.frames[source_frame_no],
                    status);
                goto end;
            }
            if (subframe_width != stream.frames[source_frame_no].width ||
                subframe_height != stream.frames[source_frame_no].height) {
                sixel_webp_trace_contract_add_code(
                    SIXEL_WEBP_CODE_ERR_VP8_STREAM);
                status = SIXEL_BAD_INPUT;
                goto end;
            }
            sixel_webp_trace_anim_frame_success(
                &stream.frames[source_frame_no]);

            sixel_webp_anim_composite_rect(canvas_pixels,
                                           stream.canvas_width,
                                           &stream.frames[source_frame_no],
                                           rgba);
            sixel_allocator_free(allocator, rgba);
            rgba = NULL;

            if (emit_callback == 0) {
                if (stream.frames[source_frame_no].dispose_to_background != 0) {
                    sixel_webp_anim_clear_rect(canvas_pixels,
                                               stream.canvas_width,
                                               &stream.frames[source_frame_no],
                                               background_rgba);
                }
                ++frames_in_loop;
                continue;
            }

            status = sixel_frame_create_from_factory(&frame, allocator);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
            status = sixel_webp_anim_copy_canvas(
                canvas_pixels,
                (size_t)stream.canvas_width * (size_t)stream.canvas_height * 4u,
                allocator,
                &emitted_pixels);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
            sixel_frame_set_delay(frame,
                                  stream.frames[source_frame_no].duration_ms
                                      / 10);
            sixel_frame_set_frame_no(frame, emit_frame_no);
            sixel_frame_set_loop_count(frame, loop_no);
            sixel_frame_set_multiframe(frame,
                                       (!fstatic && stream.frame_count > 1)
                                           ? 1
                                           : 0);
            status = sixel_frame_as_interface(frame)->vtbl->init_pixels(
                sixel_frame_as_interface(frame),
                &(sixel_frame_pixels_request_t){
                    emitted_pixels,
                    NULL,
                    stream.canvas_width,
                    stream.canvas_height,
                    SIXEL_PIXELFORMAT_RGBA8888,
                    SIXEL_COLORSPACE_GAMMA,
                    -1,
                    SIXEL_FRAME_PIXELS_U8
                });
            if (SIXEL_FAILED(status)) {
                goto end;
            }
            emitted_pixels = NULL;

            if (plan.meta_has_iccp != 0) {
                iccp_applied = sixel_webp_apply_anim_cached_iccp(
                    &meta_cache,
                    frame,
                    allocator);
                if (iccp_applied != 0) {
                    sixel_webp_trace_contract_add_code(
                        SIXEL_WEBP_CODE_META_ICCP_APPLIED);
                } else {
                    sixel_webp_trace_contract_add_code(
                        SIXEL_WEBP_CODE_META_ICCP_IGNORED);
                }
                iccp_code_reported = 1;
            }

            if (plan.meta_has_xmp != 0) {
                xmp_cms_applied = sixel_webp_apply_anim_cached_xmp_cms(
                    &meta_cache,
                    frame,
                    allocator);
                if (xmp_cms_applied != 0) {
                    sixel_webp_trace_contract_add_code(
                        SIXEL_WEBP_CODE_META_XMP_CMS_APPLIED);
                } else {
                    sixel_webp_trace_contract_add_code(
                        SIXEL_WEBP_CODE_META_XMP_CMS_IGNORED);
                    if (meta_cache.xmp_cms_size_limited != 0) {
                        sixel_webp_trace_contract_add_code(
                            SIXEL_WEBP_CODE_META_XMP_CMS_SIZE_LIMIT_IGNORED);
                    }
                }
                xmp_cms_code_reported = 1;
            }

            if (plan.meta_has_exif != 0) {
                status = sixel_webp_apply_anim_cached_orientation(
                    frame,
                    meta_cache.exif_orientation,
                    &exif_applied);
                if (SIXEL_FAILED(status)) {
                    sixel_webp_trace_contract_add_code(
                        SIXEL_WEBP_CODE_META_EXIF_IGNORED);
                    exif_code_reported = 1;
                    goto end;
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

            if (plan.meta_has_xmp != 0) {
                status = sixel_webp_apply_anim_cached_orientation(
                    frame,
                    meta_cache.xmp_orientation,
                    &xmp_applied);
                if (SIXEL_FAILED(status)) {
                    sixel_webp_trace_contract_add_code(
                        SIXEL_WEBP_CODE_META_XMP_IGNORED);
                    xmp_code_reported = 1;
                    goto end;
                }
                if (xmp_applied != 0) {
                    sixel_webp_trace_contract_add_code(
                        SIXEL_WEBP_CODE_META_XMP_APPLIED);
                } else {
                    sixel_webp_trace_contract_add_code(
                        SIXEL_WEBP_CODE_META_XMP_IGNORED);
                    if (meta_cache.xmp_orientation_size_limited != 0) {
                        sixel_webp_trace_contract_add_code(
                            SIXEL_WEBP_CODE_META_XMP_SIZE_LIMIT_IGNORED);
                    }
                }
                xmp_code_reported = 1;
            }

            status = fn_load(frame, context);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
            sixel_frame_unref(frame);
            frame = NULL;
            ++emitted_frames;

            if (stream.frames[source_frame_no].dispose_to_background != 0) {
                sixel_webp_anim_clear_rect(canvas_pixels,
                                           stream.canvas_width,
                                           &stream.frames[source_frame_no],
                                           background_rgba);
            }
            ++frames_in_loop;
            if (fstatic) {
                sixel_webp_trace_contract_add_code(SIXEL_WEBP_CODE_OK_ANIM);
                status = SIXEL_OK;
                goto end;
            }
        }

        if (emitted_frames <= 0) {
            sixel_helper_set_additional_message(
                "builtin webp: no frame was emitted from animation stream.");
            status = SIXEL_BAD_INPUT;
            goto end;
        }
        if (sixel_webp_should_stop_after_loop(loop_control,
                                              frames_in_loop,
                                              loop_no + 1,
                                              stream.loop_count)) {
            sixel_webp_trace_contract_add_code(SIXEL_WEBP_CODE_OK_ANIM);
            status = SIXEL_OK;
            goto end;
        }
    }

end:
    sixel_frame_unref(frame);
    if (rgba != NULL && chunk != NULL && allocator != NULL) {
        sixel_allocator_free(allocator, rgba);
    }
    if (emitted_pixels != NULL && chunk != NULL && allocator != NULL) {
        sixel_allocator_free(allocator, emitted_pixels);
    }
    if (canvas_pixels != NULL && chunk != NULL && allocator != NULL) {
        sixel_allocator_free(allocator, canvas_pixels);
    }
    sixel_webp_anim_meta_cache_reset(&meta_cache);
    sixel_webp_anim_stream_reset(&stream, chunk != NULL ? allocator
                                                        : NULL);
    sixel_webp_trace_unapplied_meta_codes(&plan,
                                          iccp_code_reported,
                                          exif_code_reported,
                                          xmp_code_reported,
                                          xmp_cms_code_reported);
    sixel_webp_trace_contract_flush(SIXEL_SUCCEEDED(status) ? 0 : 1);
    return status;
}

SIXELSTATUS
sixel_fromwebp_load(sixel_chunk_t const *chunk,
                    sixel_allocator_t *allocator,
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
    int xmp_code_reported;
    int xmp_cms_code_reported;
    int iccp_applied;
    int xmp_cms_applied;
    int exif_applied;
    int xmp_applied;
    int xmp_orientation_size_limited;
    int xmp_cms_size_limited;

    status = SIXEL_OK;
    memset(&container, 0, sizeof(container));
    memset(&plan, 0, sizeof(plan));
    rgba = NULL;
    width = 0;
    height = 0;
    iccp_code_reported = 0;
    exif_code_reported = 0;
    xmp_code_reported = 0;
    xmp_cms_code_reported = 0;
    iccp_applied = 0;
    xmp_cms_applied = 0;
    exif_applied = 0;
    xmp_applied = 0;
    xmp_orientation_size_limited = 0;
    xmp_cms_size_limited = 0;

    if (chunk == NULL || allocator == NULL || frame == NULL) {
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

    if (plan.kind == SIXEL_WEBP_CONTAINER_KIND_VP8_STATIC ||
        plan.kind == SIXEL_WEBP_CONTAINER_KIND_VP8_ALPHA_STATIC) {
        /* Decode layer accepts only VP8 payload bytes and allocator context. */
        status = sixel_webp_decode_vp8_payload(plan.vp8_payload,
                                               plan.vp8_payload_size,
                                               &rgba,
                                               &width,
                                               &height,
                                               allocator);
        if (SIXEL_FAILED(status)) {
            sixel_webp_trace_contract_add_code(
                SIXEL_WEBP_CODE_ERR_VP8_STREAM);
            goto cleanup;
        }
        if (plan.kind == SIXEL_WEBP_CONTAINER_KIND_VP8_ALPHA_STATIC) {
            status = sixel_webp_apply_vp8_alpha_payload(
                rgba,
                width,
                height,
                plan.alpha_payload,
                plan.alpha_payload_size,
                allocator);
            if (SIXEL_FAILED(status)) {
                sixel_webp_trace_contract_add_code(
                    SIXEL_WEBP_CODE_ERR_VP8_STREAM);
                goto cleanup;
            }
            sixel_webp_trace_contract_add_code(
                SIXEL_WEBP_CODE_OK_VP8_ALPHA_STATIC);
        } else {
            sixel_webp_trace_contract_add_code(SIXEL_WEBP_CODE_OK_VP8_STATIC);
        }
    } else if (plan.kind == SIXEL_WEBP_CONTAINER_KIND_VP8L_STATIC) {
        /*
         * Decode layer accepts only VP8L payload bytes and allocator context.
         */
        status = sixel_webp_decode_vp8l_payload(plan.vp8l_payload,
                                                plan.vp8l_payload_size,
                                                &rgba,
                                                &width,
                                                &height,
                                                allocator);
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

    frame->loop_count = 1;
    status = sixel_frame_as_interface(frame)->vtbl->init_pixels(
        sixel_frame_as_interface(frame),
        &(sixel_frame_pixels_request_t){
            rgba,
            NULL,
            width,
            height,
            SIXEL_PIXELFORMAT_RGBA8888,
            SIXEL_COLORSPACE_GAMMA,
            -1,
            SIXEL_FRAME_PIXELS_U8
        });
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
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
                allocator);
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

    if (plan.meta_has_xmp != 0) {
        status = sixel_webp_try_apply_xmp_cms(&plan,
                                              enable_cms,
                                              frame,
                                              allocator,
                                              &xmp_cms_applied,
                                              &xmp_cms_size_limited);
        if (SIXEL_FAILED(status)) {
            sixel_webp_trace_contract_add_code(
                SIXEL_WEBP_CODE_META_XMP_CMS_IGNORED);
            xmp_cms_code_reported = 1;
            goto cleanup;
        }
        if (xmp_cms_applied != 0) {
            sixel_webp_trace_contract_add_code(
                SIXEL_WEBP_CODE_META_XMP_CMS_APPLIED);
        } else {
            sixel_webp_trace_contract_add_code(
                SIXEL_WEBP_CODE_META_XMP_CMS_IGNORED);
            if (xmp_cms_size_limited != 0) {
                sixel_webp_trace_contract_add_code(
                    SIXEL_WEBP_CODE_META_XMP_CMS_SIZE_LIMIT_IGNORED);
            }
        }
        xmp_cms_code_reported = 1;
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

    if (plan.meta_has_xmp != 0) {
        status = sixel_webp_try_apply_xmp_orientation(
            &plan,
            enable_orientation,
            frame,
            &xmp_applied,
            &xmp_orientation_size_limited);
        if (SIXEL_FAILED(status)) {
            sixel_webp_trace_contract_add_code(
                SIXEL_WEBP_CODE_META_XMP_IGNORED);
            xmp_code_reported = 1;
            goto cleanup;
        }
        if (xmp_applied != 0) {
            sixel_webp_trace_contract_add_code(
                SIXEL_WEBP_CODE_META_XMP_APPLIED);
        } else {
            sixel_webp_trace_contract_add_code(
                SIXEL_WEBP_CODE_META_XMP_IGNORED);
            if (xmp_orientation_size_limited != 0) {
                sixel_webp_trace_contract_add_code(
                    SIXEL_WEBP_CODE_META_XMP_SIZE_LIMIT_IGNORED);
            }
        }
        xmp_code_reported = 1;
    }

cleanup:
    if (rgba != NULL && chunk != NULL && allocator != NULL) {
        sixel_allocator_free(allocator, rgba);
    }
    sixel_webp_trace_unapplied_meta_codes(&plan,
                                          iccp_code_reported,
                                          exif_code_reported,
                                          xmp_code_reported,
                                          xmp_cms_code_reported);
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
