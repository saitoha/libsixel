/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2021-2025 libsixel developers. See `AUTHORS`.
 * Copyright (c) 2014-2019 Hayaki Saito
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF, OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * libjpeg-backed loader helpers extracted from loader.c to prepare for
 * backend-specific translation units. The functions here remain thin wrappers
 * around the shared callbacks and frame utilities so the registration table
 * can reference them without pulling libjpeg headers into unrelated code.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if HAVE_JPEG

#include <stdio.h>
#if HAVE_STRING_H
# include <string.h>
#endif
#if HAVE_ERRNO_H
# include <errno.h>
#endif
#if HAVE_LIMITS_H
# include <limits.h>
#endif
#include <setjmp.h>
#if HAVE_STDINT_H
# include <stdint.h>
#endif
#include <jpeglib.h>

#include <sixel.h>

#include "allocator.h"
#include "cms.h"
#include "chunk.h"
#include "icc-apply.h"
#include "icc-parse.h"
#include "loader-common.h"
#include "loader.h"
#include "frame-private.h"
#include "frame-factory.h"
#include "loader-libjpeg.h"
#include "logger.h"

#if defined(HAVE_JPEG12_API) && HAVE_JPEG12_API
# define SIXEL_LIBJPEG_HAS_JPEG12 1
#elif defined(J12SAMPLE) && defined(MAXJ12SAMPLE)
# define SIXEL_LIBJPEG_HAS_JPEG12 1
#else
# define SIXEL_LIBJPEG_HAS_JPEG12 0
#endif

#if defined(HAVE_JPEG16_API) && HAVE_JPEG16_API
# define SIXEL_LIBJPEG_HAS_JPEG16 1
#elif defined(J16SAMPLE) && defined(MAXJ16SAMPLE)
# define SIXEL_LIBJPEG_HAS_JPEG16 1
#else
# define SIXEL_LIBJPEG_HAS_JPEG16 0
#endif

typedef struct sixel_loader_libjpeg_component {
    sixel_loader_component_t base;
    sixel_allocator_t *allocator;
    unsigned int ref;
    int enable_cms;
    int fstatic;
    int fuse_palette;
    int reqcolors;
    unsigned char bgcolor[3];
    int has_bgcolor;
    int loop_control;
    int has_start_frame_no;
    int start_frame_no;
    int enable_orientation;
} sixel_loader_libjpeg_component_t;

static int
loader_can_try_libjpeg(sixel_chunk_t const *chunk);

typedef struct sixel_loader_libjpeg_error_context {
    struct jpeg_error_mgr pub;
    jmp_buf jmpbuf;
    char message[JMSG_LENGTH_MAX];
} sixel_loader_libjpeg_error_context_t;

static void
sixel_loader_libjpeg_error_exit(j_common_ptr cinfo)
{
    sixel_loader_libjpeg_error_context_t *ctx;

    ctx = NULL;
    if (cinfo == NULL || cinfo->err == NULL) {
        return;
    }

    ctx = (sixel_loader_libjpeg_error_context_t *)cinfo->err;
    if (ctx != NULL) {
        ctx->message[0] = '\0';
        (*cinfo->err->format_message)(cinfo, ctx->message);
        if (ctx->message[0] != '\0') {
            sixel_helper_set_additional_message(ctx->message);
        }
        longjmp(ctx->jmpbuf, 1);
    }
}

/*
 * JPEG ICC profile blocks are stored in APP2 markers as "ICC_PROFILE\0"
 * payloads with sequence numbering. This parser reassembles the profile in
 * marker order and returns a contiguous ICC blob.
 */
static int
jpeg_collect_icc_profile(struct jpeg_decompress_struct *cinfo,
                         unsigned char **profile,
                         unsigned int *profile_length,
                         sixel_allocator_t *allocator)
{
    jpeg_saved_marker_ptr marker;
    unsigned char **chunks;
    unsigned int *chunk_sizes;
    unsigned int chunk_count;
    unsigned int max_index;
    unsigned int total_size;
    unsigned int index;
    unsigned int sequence_no;
    unsigned int sequence_count;
    unsigned int payload_size;
    unsigned char *assembled;
    unsigned int offset;

    marker = NULL;
    chunks = NULL;
    chunk_sizes = NULL;
    chunk_count = 0u;
    max_index = 0u;
    total_size = 0u;
    index = 0u;
    sequence_no = 0u;
    sequence_count = 0u;
    payload_size = 0u;
    assembled = NULL;
    offset = 0u;

    *profile = NULL;
    *profile_length = 0u;
    for (marker = cinfo->marker_list; marker != NULL; marker = marker->next) {
        if (marker->marker != (JPEG_APP0 + 2) || marker->data_length < 14u) {
            continue;
        }
        if (memcmp(marker->data, "ICC_PROFILE\0", 12) != 0) {
            continue;
        }

        sequence_no = (unsigned int)marker->data[12];
        sequence_count = (unsigned int)marker->data[13];
        if (sequence_no == 0u || sequence_count == 0u ||
            sequence_no > sequence_count) {
            return 0;
        }
        if (chunk_count == 0u) {
            chunk_count = sequence_count;
            chunks = (unsigned char **)sixel_allocator_calloc(
                allocator,
                chunk_count,
                sizeof(*chunks));
            chunk_sizes = (unsigned int *)sixel_allocator_calloc(
                allocator,
                chunk_count,
                sizeof(*chunk_sizes));
            if (chunks == NULL || chunk_sizes == NULL) {
                goto cleanup;
            }
        }
        if (sequence_count != chunk_count) {
            goto cleanup;
        }

        index = sequence_no - 1u;
        if (chunks[index] != NULL) {
            goto cleanup;
        }
        payload_size = (unsigned int)marker->data_length - 14u;
        chunks[index] = (unsigned char *)sixel_allocator_malloc(
            allocator,
            payload_size);
        if (chunks[index] == NULL) {
            goto cleanup;
        }
        memcpy(chunks[index], marker->data + 14, payload_size);
        chunk_sizes[index] = payload_size;
        if (index > max_index) {
            max_index = index;
        }
    }

    if (chunk_count == 0u) {
        return 0;
    }
    if (max_index + 1u != chunk_count) {
        goto cleanup;
    }
    for (index = 0u; index < chunk_count; ++index) {
        if (chunks[index] == NULL ||
            total_size > UINT_MAX - chunk_sizes[index]) {
            goto cleanup;
        }
        total_size += chunk_sizes[index];
    }

    assembled = (unsigned char *)sixel_allocator_malloc(allocator, total_size);
    if (assembled == NULL) {
        goto cleanup;
    }
    for (index = 0u; index < chunk_count; ++index) {
        memcpy(assembled + offset, chunks[index], chunk_sizes[index]);
        offset += chunk_sizes[index];
    }

    *profile = assembled;
    *profile_length = total_size;
    assembled = NULL;

cleanup:
    if (chunks != NULL) {
        for (index = 0u; index < chunk_count; ++index) {
            sixel_allocator_free(allocator, chunks[index]);
        }
    }
    sixel_allocator_free(allocator, chunks);
    sixel_allocator_free(allocator, chunk_sizes);
    sixel_allocator_free(allocator, assembled);
    return *profile != NULL;
}
static void
jpeg_unpack_rgb8_to_rgbf32(float *dst,
                           unsigned char const *src,
                           size_t pixel_count)
{
    size_t index;
    size_t base;

    if (dst == NULL || src == NULL || pixel_count == 0u) {
        return;
    }

    for (index = 0u; index < pixel_count; ++index) {
        base = index * 3u;
        dst[base + 0u] = (float)src[base + 0u] / 255.0f;
        dst[base + 1u] = (float)src[base + 1u] / 255.0f;
        dst[base + 2u] = (float)src[base + 2u] / 255.0f;
    }
}

static void
jpeg_unpack_cmyk8_to_rgbf32(float *dst,
                            unsigned char const *src,
                            size_t pixel_count)
{
    size_t index;
    size_t src_base;
    size_t dst_base;
    float scale;
    float c;
    float m;
    float y;
    float k;

    index = 0u;
    src_base = 0u;
    dst_base = 0u;
    scale = 1.0f / 255.0f;
    c = 0.0f;
    m = 0.0f;
    y = 0.0f;
    k = 0.0f;
    if (dst == NULL || src == NULL || pixel_count == 0u) {
        return;
    }

    for (index = 0u; index < pixel_count; ++index) {
        src_base = index * 4u;
        dst_base = index * 3u;
        c = (float)src[src_base + 0u] * scale;
        m = (float)src[src_base + 1u] * scale;
        y = (float)src[src_base + 2u] * scale;
        k = (float)src[src_base + 3u] * scale;
        dst[dst_base + 0u] = c * k;
        dst[dst_base + 1u] = m * k;
        dst[dst_base + 2u] = y * k;
    }
}

static void
jpeg_unpack_cmyk16_to_rgbf32(float *dst,
                             uint16_t const *src,
                             size_t pixel_count)
{
    size_t index;
    size_t src_base;
    size_t dst_base;
    float scale;
    float c;
    float m;
    float y;
    float k;

    index = 0u;
    src_base = 0u;
    dst_base = 0u;
#if defined(MAXJ16SAMPLE)
    scale = 1.0f / (float)MAXJ16SAMPLE;
#else
    /*
     * jpeg_decode_cmyk16_from_precision() normalizes 12-bit samples into the
     * full uint16_t range. Use 16-bit full-scale when libjpeg16 constants are
     * unavailable at compile time.
     */
    scale = 1.0f / 65535.0f;
#endif
    c = 0.0f;
    m = 0.0f;
    y = 0.0f;
    k = 0.0f;
    if (dst == NULL || src == NULL || pixel_count == 0u) {
        return;
    }

    for (index = 0u; index < pixel_count; ++index) {
        src_base = index * 4u;
        dst_base = index * 3u;
        c = (float)src[src_base + 0u] * scale;
        m = (float)src[src_base + 1u] * scale;
        y = (float)src[src_base + 2u] * scale;
        k = (float)src[src_base + 3u] * scale;
        dst[dst_base + 0u] = c * k;
        dst[dst_base + 1u] = m * k;
        dst[dst_base + 2u] = y * k;
    }
}

#if SIXEL_LIBJPEG_HAS_JPEG12
static void
jpeg_unpack_rgb12_to_rgbf32(float *dst,
                            J12SAMPLE const *src,
                            size_t pixel_count)
{
    size_t index;
    size_t base;
    float scale;

    index = 0u;
    base = 0u;
    scale = 1.0f / (float)MAXJ12SAMPLE;
    if (dst == NULL || src == NULL || pixel_count == 0u) {
        return;
    }

    for (index = 0u; index < pixel_count; ++index) {
        base = index * 3u;
        dst[base + 0u] = (float)src[base + 0u] * scale;
        dst[base + 1u] = (float)src[base + 1u] * scale;
        dst[base + 2u] = (float)src[base + 2u] * scale;
    }
}
#endif

#if SIXEL_LIBJPEG_HAS_JPEG16
static void
jpeg_unpack_rgb16_to_rgbf32(float *dst,
                            J16SAMPLE const *src,
                            size_t pixel_count)
{
    size_t index;
    size_t base;
    float scale;

    index = 0u;
    base = 0u;
    scale = 1.0f / (float)MAXJ16SAMPLE;
    if (dst == NULL || src == NULL || pixel_count == 0u) {
        return;
    }

    for (index = 0u; index < pixel_count; ++index) {
        base = index * 3u;
        dst[base + 0u] = (float)src[base + 0u] * scale;
        dst[base + 1u] = (float)src[base + 1u] * scale;
        dst[base + 2u] = (float)src[base + 2u] * scale;
    }
}
#endif

static SIXELSTATUS
jpeg_convert_rgbf32_gamma_to_linear(float *pixels,
                                    size_t pixel_count)
{
    size_t float_bytes;

    float_bytes = 0u;
    if (pixels == NULL || pixel_count == 0u) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (pixel_count > SIZE_MAX / (3u * sizeof(float))) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    float_bytes = pixel_count * 3u * sizeof(float);

    return sixel_helper_convert_colorspace((unsigned char *)pixels,
                                           float_bytes,
                                           SIXEL_PIXELFORMAT_RGBFLOAT32,
                                           SIXEL_COLORSPACE_GAMMA,
                                           SIXEL_COLORSPACE_LINEAR);
}

#if HAVE_LCMS2
static int
jpeg_convert_icc_rgbf32_to_srgb(float *pixels,
                                size_t pixel_count,
                                unsigned char const *profile,
                                unsigned int profile_length)
{
    sixel_cms_profile_t *src_profile;
    sixel_cms_profile_t *dst_profile;
    sixel_cms_transform_t *transform;
    int converted;

    src_profile = NULL;
    dst_profile = NULL;
    transform = NULL;
    converted = 0;
    if (pixels == NULL || pixel_count == 0u ||
        profile == NULL || profile_length == 0u) {
        return 0;
    }

    src_profile = sixel_cms_open_profile_from_mem(profile, profile_length);
    if (src_profile == NULL) {
        goto cleanup;
    }
    dst_profile = sixel_cms_create_srgb_profile();
    if (dst_profile == NULL) {
        goto cleanup;
    }
    transform = sixel_cms_create_transform(src_profile,
                                           SIXEL_CMS_PIXELFORMAT_RGB_F32,
                                           dst_profile,
                                           SIXEL_CMS_PIXELFORMAT_RGB_F32,
                                           SIXEL_CMS_TRANSFORM_DEFAULT);
    if (transform == NULL) {
        goto cleanup;
    }
    converted = sixel_cms_do_transform(transform,
                                       pixels,
                                       pixels,
                                       pixel_count);

cleanup:
    sixel_cms_delete_transform(transform);
    sixel_cms_close_profile(dst_profile);
    sixel_cms_close_profile(src_profile);
    return converted;
}

static int
jpeg_convert_icc_cmyk8_to_srgb_f32(float *dst_pixels,
                                   unsigned char const *src_pixels,
                                   size_t pixel_count,
                                   unsigned char const *profile,
                                   unsigned int profile_length)
{
    sixel_cms_profile_t *src_profile;
    sixel_cms_profile_t *dst_profile;
    sixel_cms_transform_t *transform;
    int converted;

    src_profile = NULL;
    dst_profile = NULL;
    transform = NULL;
    converted = 0;
    if (dst_pixels == NULL || src_pixels == NULL || pixel_count == 0u ||
        profile == NULL || profile_length == 0u) {
        return 0;
    }

    src_profile = sixel_cms_open_profile_from_mem(profile, profile_length);
    if (src_profile == NULL) {
        goto cleanup;
    }
    dst_profile = sixel_cms_create_srgb_profile();
    if (dst_profile == NULL) {
        goto cleanup;
    }
    transform = sixel_cms_create_transform(src_profile,
                                           SIXEL_CMS_PIXELFORMAT_CMYK_8,
                                           dst_profile,
                                           SIXEL_CMS_PIXELFORMAT_RGB_F32,
                                           SIXEL_CMS_TRANSFORM_DEFAULT);
    if (transform == NULL) {
        goto cleanup;
    }
    converted = sixel_cms_do_transform(transform,
                                       src_pixels,
                                       dst_pixels,
                                       pixel_count);

cleanup:
    sixel_cms_delete_transform(transform);
    sixel_cms_close_profile(dst_profile);
    sixel_cms_close_profile(src_profile);
    return converted;
}

static int
jpeg_convert_icc_cmyk16_to_srgb_f32(float *dst_pixels,
                                    uint16_t const *src_pixels,
                                    size_t pixel_count,
                                    unsigned char const *profile,
                                    unsigned int profile_length)
{
    sixel_cms_profile_t *src_profile;
    sixel_cms_profile_t *dst_profile;
    sixel_cms_transform_t *transform;
    int converted;

    src_profile = NULL;
    dst_profile = NULL;
    transform = NULL;
    converted = 0;
    if (dst_pixels == NULL || src_pixels == NULL || pixel_count == 0u ||
        profile == NULL || profile_length == 0u) {
        return 0;
    }

    src_profile = sixel_cms_open_profile_from_mem(profile, profile_length);
    if (src_profile == NULL) {
        goto cleanup;
    }
    dst_profile = sixel_cms_create_srgb_profile();
    if (dst_profile == NULL) {
        goto cleanup;
    }
    transform = sixel_cms_create_transform(src_profile,
                                           SIXEL_CMS_PIXELFORMAT_CMYK_16,
                                           dst_profile,
                                           SIXEL_CMS_PIXELFORMAT_RGB_F32,
                                           SIXEL_CMS_TRANSFORM_DEFAULT);
    if (transform == NULL) {
        goto cleanup;
    }
    converted = sixel_cms_do_transform(transform,
                                       src_pixels,
                                       dst_pixels,
                                       pixel_count);

cleanup:
    sixel_cms_delete_transform(transform);
    sixel_cms_close_profile(dst_profile);
    sixel_cms_close_profile(src_profile);
    return converted;
}
#endif

#if !HAVE_LCMS2
static int
jpeg_convert_icc_cmyk8_to_srgb_f32_nolcms(float *dst_pixels,
                                          unsigned char const *src_pixels,
                                          size_t pixel_count,
                                          unsigned char const *profile_data,
                                          size_t profile_length)
{
    sixel_icc_profile_t profile;
    int parsed;
    int converted;

    memset(&profile, 0, sizeof(profile));
    parsed = 0;
    converted = 0;
    if (dst_pixels == NULL || src_pixels == NULL ||
        pixel_count == 0u || profile_data == NULL || profile_length == 0u) {
        return 0;
    }

    if (!sixel_icc_parse_profile(profile_data, profile_length, &profile)) {
        goto cleanup;
    }
    parsed = 1;
    if (profile.kind == SIXEL_ICC_PROFILE_KIND_CMYK) {
        converted = sixel_icc_apply_cmyk_u8_to_rgb_float32(dst_pixels,
                                                            src_pixels,
                                                            pixel_count,
                                                            &profile);
    }

cleanup:
    if (parsed) {
        sixel_icc_profile_destroy(&profile);
    }
    return converted;
}

static int
jpeg_convert_icc_cmyk16_to_srgb_f32_nolcms(float *dst_pixels,
                                           uint16_t const *src_pixels,
                                           size_t pixel_count,
                                           unsigned char const *profile_data,
                                           size_t profile_length)
{
    sixel_icc_profile_t profile;
    int parsed;
    int converted;

    memset(&profile, 0, sizeof(profile));
    parsed = 0;
    converted = 0;
    if (dst_pixels == NULL || src_pixels == NULL ||
        pixel_count == 0u || profile_data == NULL || profile_length == 0u) {
        return 0;
    }

    if (!sixel_icc_parse_profile(profile_data, profile_length, &profile)) {
        goto cleanup;
    }
    parsed = 1;
    if (profile.kind == SIXEL_ICC_PROFILE_KIND_CMYK) {
        converted = sixel_icc_apply_cmyk_u16_to_rgb_float32(dst_pixels,
                                                             src_pixels,
                                                             pixel_count,
                                                             &profile);
    }

cleanup:
    if (parsed) {
        sixel_icc_profile_destroy(&profile);
    }
    return converted;
}
#endif

static SIXELSTATUS
jpeg_promote_rgb888_to_linear_float32(unsigned char **result,
                                      int width,
                                      int height,
                                      sixel_allocator_t *allocator,
                                      int enable_cms,
                                      unsigned char const *icc_profile,
                                      unsigned int icc_profile_length
                                      )
{
    SIXELSTATUS status;
    size_t pixel_count;
    size_t float_bytes;
    unsigned char *rgb_pixels;
    float *float_pixels;
#if HAVE_LCMS2
    sixel_cms_profile_t *src_profile;
    sixel_cms_profile_t *dst_profile;
    sixel_cms_transform_t *transform;
    int cms_converted;
#endif

    status = SIXEL_OK;
    pixel_count = 0u;
    float_bytes = 0u;
    rgb_pixels = NULL;
    float_pixels = NULL;
#if HAVE_LCMS2
    src_profile = NULL;
    dst_profile = NULL;
    transform = NULL;
    cms_converted = 0;
#endif

    if (result == NULL || *result == NULL || allocator == NULL ||
        width <= 0 || height <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    if ((size_t)width > SIZE_MAX / (size_t)height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    pixel_count = (size_t)width * (size_t)height;
    if (pixel_count > SIZE_MAX / (3u * sizeof(float))) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    float_bytes = pixel_count * 3u * sizeof(float);

    rgb_pixels = *result;
    float_pixels = (float *)sixel_allocator_malloc(allocator, float_bytes);
    if (float_pixels == NULL) {
        sixel_helper_set_additional_message(
            "jpeg_promote_rgb888_to_linear_float32: "
            "sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

#if HAVE_LCMS2
    if (enable_cms && icc_profile != NULL && icc_profile_length > 0u) {
        src_profile = sixel_cms_open_profile_from_mem(icc_profile,
                                                      icc_profile_length);
        if (src_profile != NULL) {
            dst_profile = sixel_cms_create_srgb_profile();
        }
        if (src_profile != NULL && dst_profile != NULL) {
            transform = sixel_cms_create_transform(src_profile,
                                                   SIXEL_CMS_PIXELFORMAT_RGB_8,
                                                   dst_profile,
                                                   SIXEL_CMS_PIXELFORMAT_RGB_F32,
                                                   SIXEL_CMS_TRANSFORM_DEFAULT);
        }
        if (transform != NULL &&
            sixel_cms_do_transform(transform,
                                   rgb_pixels,
                                   float_pixels,
                                   pixel_count)) {
            cms_converted = 1;
        }
    }
#endif

#if HAVE_LCMS2
    if (!cms_converted) {
        jpeg_unpack_rgb8_to_rgbf32(float_pixels, rgb_pixels, pixel_count);
    }
#else
    jpeg_unpack_rgb8_to_rgbf32(float_pixels, rgb_pixels, pixel_count);
#endif

#if !HAVE_LCMS2
    if (enable_cms && icc_profile != NULL && icc_profile_length > 0u) {
        sixel_cms_convert_to_srgb_with_profile_bytes(
            (unsigned char *)float_pixels,
            width,
            height,
            SIXEL_PIXELFORMAT_RGBFLOAT32,
            icc_profile,
            (size_t)icc_profile_length);
    }
#endif

    if (enable_cms) {
        status = jpeg_convert_rgbf32_gamma_to_linear(float_pixels, pixel_count);
        if (SIXEL_FAILED(status)) {
            sixel_allocator_free(allocator, float_pixels);
#if HAVE_LCMS2
            sixel_cms_delete_transform(transform);
            sixel_cms_close_profile(dst_profile);
            sixel_cms_close_profile(src_profile);
#endif
            return status;
        }
    }

    sixel_allocator_free(allocator, *result);
    *result = (unsigned char *)float_pixels;

#if HAVE_LCMS2
    sixel_cms_delete_transform(transform);
    sixel_cms_close_profile(dst_profile);
    sixel_cms_close_profile(src_profile);
#endif
    return SIXEL_OK;
}

static SIXELSTATUS
jpeg_promote_cmyk888_to_linear_float32(unsigned char **result,
                                       int width,
                                       int height,
                                       sixel_allocator_t *allocator,
                                       int enable_cms,
                                       unsigned char const *icc_profile,
                                       unsigned int icc_profile_length
                                       )
{
    SIXELSTATUS status;
    size_t pixel_count;
    size_t float_bytes;
    unsigned char *cmyk_pixels;
    float *float_pixels;
    int cms_converted;

    status = SIXEL_OK;
    pixel_count = 0u;
    float_bytes = 0u;
    cmyk_pixels = NULL;
    float_pixels = NULL;
    cms_converted = 0;

    if (result == NULL || *result == NULL || allocator == NULL ||
        width <= 0 || height <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    if ((size_t)width > SIZE_MAX / (size_t)height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    pixel_count = (size_t)width * (size_t)height;
    if (pixel_count > SIZE_MAX / (3u * sizeof(float))) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    float_bytes = pixel_count * 3u * sizeof(float);

    cmyk_pixels = *result;
    float_pixels = (float *)sixel_allocator_malloc(allocator, float_bytes);
    if (float_pixels == NULL) {
        sixel_helper_set_additional_message(
            "jpeg_promote_cmyk888_to_linear_float32: "
            "sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

#if HAVE_LCMS2
    if (enable_cms && icc_profile != NULL && icc_profile_length > 0u) {
        if (jpeg_convert_icc_cmyk8_to_srgb_f32(float_pixels,
                                               cmyk_pixels,
                                               pixel_count,
                                               icc_profile,
                                               icc_profile_length)) {
            cms_converted = 1;
        }
    }
#endif

#if !HAVE_LCMS2
    if (enable_cms && icc_profile != NULL && icc_profile_length > 0u) {
        if (jpeg_convert_icc_cmyk8_to_srgb_f32_nolcms(float_pixels,
                                                      cmyk_pixels,
                                                      pixel_count,
                                                      icc_profile,
                                                      (size_t)icc_profile_length)) {
            cms_converted = 1;
        }
    }
#endif

    if (!cms_converted) {
        jpeg_unpack_cmyk8_to_rgbf32(float_pixels, cmyk_pixels, pixel_count);
    }

#if !HAVE_LCMS2
    if (!cms_converted &&
        enable_cms && icc_profile != NULL && icc_profile_length > 0u) {
        sixel_cms_convert_to_srgb_with_profile_bytes(
            (unsigned char *)float_pixels,
            width,
            height,
            SIXEL_PIXELFORMAT_RGBFLOAT32,
            icc_profile,
            (size_t)icc_profile_length);
    }
#endif

    if (enable_cms) {
        status = jpeg_convert_rgbf32_gamma_to_linear(float_pixels, pixel_count);
        if (SIXEL_FAILED(status)) {
            sixel_allocator_free(allocator, float_pixels);
            return status;
        }
    }

    sixel_allocator_free(allocator, *result);
    *result = (unsigned char *)float_pixels;
    return SIXEL_OK;
}

static SIXELSTATUS
jpeg_promote_cmyk16_to_linear_float32(unsigned char **result,
                                      int width,
                                      int height,
                                      sixel_allocator_t *allocator,
                                      int enable_cms,
                                      unsigned char const *icc_profile,
                                      unsigned int icc_profile_length
                                      )
{
    SIXELSTATUS status;
    size_t pixel_count;
    size_t float_bytes;
    uint16_t *cmyk_pixels;
    float *float_pixels;
    int cms_converted;

    status = SIXEL_OK;
    pixel_count = 0u;
    float_bytes = 0u;
    cmyk_pixels = NULL;
    float_pixels = NULL;
    cms_converted = 0;

    if (result == NULL || *result == NULL || allocator == NULL ||
        width <= 0 || height <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    if ((size_t)width > SIZE_MAX / (size_t)height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    pixel_count = (size_t)width * (size_t)height;
    if (pixel_count > SIZE_MAX / (3u * sizeof(float))) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    float_bytes = pixel_count * 3u * sizeof(float);

    cmyk_pixels = (uint16_t *)*result;
    float_pixels = (float *)sixel_allocator_malloc(allocator, float_bytes);
    if (float_pixels == NULL) {
        sixel_helper_set_additional_message(
            "jpeg_promote_cmyk16_to_linear_float32: "
            "sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

#if HAVE_LCMS2
    if (enable_cms && icc_profile != NULL && icc_profile_length > 0u) {
        if (jpeg_convert_icc_cmyk16_to_srgb_f32(float_pixels,
                                                cmyk_pixels,
                                                pixel_count,
                                                icc_profile,
                                                icc_profile_length)) {
            cms_converted = 1;
        }
    }
#endif

#if !HAVE_LCMS2
    if (enable_cms && icc_profile != NULL && icc_profile_length > 0u) {
        if (jpeg_convert_icc_cmyk16_to_srgb_f32_nolcms(float_pixels,
                                                       cmyk_pixels,
                                                       pixel_count,
                                                       icc_profile,
                                                       (size_t)icc_profile_length)) {
            cms_converted = 1;
        }
    }
#endif

    if (!cms_converted) {
        jpeg_unpack_cmyk16_to_rgbf32(float_pixels, cmyk_pixels, pixel_count);
    }

#if !HAVE_LCMS2
    if (!cms_converted &&
        enable_cms && icc_profile != NULL && icc_profile_length > 0u) {
        sixel_cms_convert_to_srgb_with_profile_bytes(
            (unsigned char *)float_pixels,
            width,
            height,
            SIXEL_PIXELFORMAT_RGBFLOAT32,
            icc_profile,
            (size_t)icc_profile_length);
    }
#endif

    if (enable_cms) {
        status = jpeg_convert_rgbf32_gamma_to_linear(float_pixels, pixel_count);
        if (SIXEL_FAILED(status)) {
            sixel_allocator_free(allocator, float_pixels);
            return status;
        }
    }

    sixel_allocator_free(allocator, *result);
    *result = (unsigned char *)float_pixels;
    return SIXEL_OK;
}

static SIXELSTATUS
jpeg_decode_to_float32_from_precision(struct jpeg_decompress_struct *cinfo,
                                      float *dst,
                                      int data_precision)
{
#if SIXEL_LIBJPEG_HAS_JPEG12 || SIXEL_LIBJPEG_HAS_JPEG16
    JDIMENSION row_stride;
    size_t row_index;
    size_t pixel_count;
#endif
#if SIXEL_LIBJPEG_HAS_JPEG12
    J12SAMPARRAY buffer12;
#endif
#if SIXEL_LIBJPEG_HAS_JPEG16
    J16SAMPARRAY buffer16;
#endif

#if SIXEL_LIBJPEG_HAS_JPEG12 || SIXEL_LIBJPEG_HAS_JPEG16
    row_stride = 0u;
    row_index = 0u;
    pixel_count = 0u;
#endif
#if SIXEL_LIBJPEG_HAS_JPEG12
    buffer12 = NULL;
#endif
#if SIXEL_LIBJPEG_HAS_JPEG16
    buffer16 = NULL;
#endif

    if (cinfo == NULL || dst == NULL || cinfo->output_components != 3) {
        return SIXEL_BAD_ARGUMENT;
    }
    if ((size_t)cinfo->output_width > SIZE_MAX / (size_t)cinfo->output_height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
#if SIXEL_LIBJPEG_HAS_JPEG12 || SIXEL_LIBJPEG_HAS_JPEG16
    row_stride = cinfo->output_width * (unsigned int)cinfo->output_components;
    pixel_count = (size_t)cinfo->output_width;
#endif

#if SIXEL_LIBJPEG_HAS_JPEG12
    if (data_precision > 8 && data_precision <= 12) {
        buffer12 = (J12SAMPARRAY)(*cinfo->mem->alloc_sarray)((j_common_ptr)cinfo,
                                                              JPOOL_IMAGE,
                                                              row_stride,
                                                              1);
        while (cinfo->output_scanline < cinfo->output_height) {
            if (jpeg12_read_scanlines(cinfo, buffer12, 1) != 1u) {
                return SIXEL_JPEG_ERROR;
            }
            if (cinfo->err->num_warnings > 0) {
                sixel_helper_set_additional_message(
                    "jpeg12_read_scanlines: error/warning occuered.");
                return SIXEL_BAD_INPUT;
            }
            row_index = (size_t)cinfo->output_scanline - 1u;
            jpeg_unpack_rgb12_to_rgbf32(dst + row_index * pixel_count * 3u,
                                        buffer12[0],
                                        pixel_count);
        }
        return SIXEL_OK;
    }
#endif

#if SIXEL_LIBJPEG_HAS_JPEG16
    if (data_precision > 12 && data_precision <= 16) {
        buffer16 = (J16SAMPARRAY)(*cinfo->mem->alloc_sarray)((j_common_ptr)cinfo,
                                                              JPOOL_IMAGE,
                                                              row_stride,
                                                              1);
        while (cinfo->output_scanline < cinfo->output_height) {
            if (jpeg16_read_scanlines(cinfo, buffer16, 1) != 1u) {
                return SIXEL_JPEG_ERROR;
            }
            if (cinfo->err->num_warnings > 0) {
                sixel_helper_set_additional_message(
                    "jpeg16_read_scanlines: error/warning occuered.");
                return SIXEL_BAD_INPUT;
            }
            row_index = (size_t)cinfo->output_scanline - 1u;
            jpeg_unpack_rgb16_to_rgbf32(dst + row_index * pixel_count * 3u,
                                        buffer16[0],
                                        pixel_count);
        }
        return SIXEL_OK;
    }
#endif

#if !SIXEL_LIBJPEG_HAS_JPEG12 && !SIXEL_LIBJPEG_HAS_JPEG16
    (void)data_precision;
#endif
    sixel_helper_set_additional_message(
        "load_jpeg: unsupported JPEG precision for this build.");
    return SIXEL_BAD_INPUT;
}

static SIXELSTATUS
jpeg_decode_cmyk16_from_precision(struct jpeg_decompress_struct *cinfo,
                                  uint16_t *dst,
                                  int data_precision)
{
#if SIXEL_LIBJPEG_HAS_JPEG12 || SIXEL_LIBJPEG_HAS_JPEG16
    JDIMENSION row_stride;
    size_t row_index;
    size_t row_sample_count;
    size_t sample_index;
    size_t row_base;
    unsigned int sample;
#endif
#if SIXEL_LIBJPEG_HAS_JPEG12
    J12SAMPARRAY buffer12;
#endif
#if SIXEL_LIBJPEG_HAS_JPEG16
    J16SAMPARRAY buffer16;
#endif

#if SIXEL_LIBJPEG_HAS_JPEG12 || SIXEL_LIBJPEG_HAS_JPEG16
    row_stride = 0u;
    row_index = 0u;
    row_sample_count = 0u;
    sample_index = 0u;
    row_base = 0u;
    sample = 0u;
#endif
#if SIXEL_LIBJPEG_HAS_JPEG12
    buffer12 = NULL;
#endif
#if SIXEL_LIBJPEG_HAS_JPEG16
    buffer16 = NULL;
#endif

    if (cinfo == NULL || dst == NULL || cinfo->output_components != 4) {
        return SIXEL_BAD_ARGUMENT;
    }
    if ((size_t)cinfo->output_width > SIZE_MAX / (size_t)cinfo->output_height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
#if SIXEL_LIBJPEG_HAS_JPEG12 || SIXEL_LIBJPEG_HAS_JPEG16
    row_stride = cinfo->output_width * (unsigned int)cinfo->output_components;
    row_sample_count = (size_t)cinfo->output_width * 4u;
#endif

#if SIXEL_LIBJPEG_HAS_JPEG12
    if (data_precision > 8 && data_precision <= 12) {
        buffer12 = (J12SAMPARRAY)(*cinfo->mem->alloc_sarray)((j_common_ptr)cinfo,
                                                              JPOOL_IMAGE,
                                                              row_stride,
                                                              1);
        while (cinfo->output_scanline < cinfo->output_height) {
            if (jpeg12_read_scanlines(cinfo, buffer12, 1) != 1u) {
                return SIXEL_JPEG_ERROR;
            }
            if (cinfo->err->num_warnings > 0) {
                sixel_helper_set_additional_message(
                    "jpeg12_read_scanlines: error/warning occuered.");
                return SIXEL_BAD_INPUT;
            }
            row_index = (size_t)cinfo->output_scanline - 1u;
            row_base = row_index * row_sample_count;
            for (sample_index = 0u; sample_index < row_sample_count; ++sample_index) {
                sample = (unsigned int)buffer12[0][sample_index];
                dst[row_base + sample_index] = (uint16_t)(
                    ((uint32_t)sample * 65535u + (uint32_t)MAXJ12SAMPLE / 2u)
                    / (uint32_t)MAXJ12SAMPLE);
            }
        }
        return SIXEL_OK;
    }
#endif

#if SIXEL_LIBJPEG_HAS_JPEG16
    if (data_precision > 12 && data_precision <= 16) {
        buffer16 = (J16SAMPARRAY)(*cinfo->mem->alloc_sarray)((j_common_ptr)cinfo,
                                                              JPOOL_IMAGE,
                                                              row_stride,
                                                              1);
        while (cinfo->output_scanline < cinfo->output_height) {
            if (jpeg16_read_scanlines(cinfo, buffer16, 1) != 1u) {
                return SIXEL_JPEG_ERROR;
            }
            if (cinfo->err->num_warnings > 0) {
                sixel_helper_set_additional_message(
                    "jpeg16_read_scanlines: error/warning occuered.");
                return SIXEL_BAD_INPUT;
            }
            row_index = (size_t)cinfo->output_scanline - 1u;
            row_base = row_index * row_sample_count;
            for (sample_index = 0u; sample_index < row_sample_count; ++sample_index) {
                dst[row_base + sample_index] = (uint16_t)buffer16[0][sample_index];
            }
        }
        return SIXEL_OK;
    }
#endif

#if !SIXEL_LIBJPEG_HAS_JPEG12 && !SIXEL_LIBJPEG_HAS_JPEG16
    (void)data_precision;
#endif
    sixel_helper_set_additional_message(
        "load_jpeg: unsupported JPEG precision for this build.");
    return SIXEL_BAD_INPUT;
}

/*
 * import from @uobikiemukot's sdump loader.h
 *
 * The helper keeps libjpeg-specific state localized so only this file needs
 * to include jpeglib.h. Callers receive LINEARRGBFLOAT32 buffers and metadata
 * filled through the OUT parameters.
 */
static SIXELSTATUS
load_jpeg(unsigned char **result,
          unsigned char *data,
          size_t datasize,
          int *pwidth,
          int *pheight,
          int *ppixelformat,
          sixel_allocator_t *allocator,
          int enable_cms)
{
    SIXELSTATUS status;
    JDIMENSION row_stride;
    size_t pixel_count;
    size_t size;
    JSAMPARRAY buffer;
    struct jpeg_decompress_struct cinfo;
    sixel_loader_libjpeg_error_context_t jerr;
    int data_precision;
    volatile int decode_cmyk;
    unsigned int output_components;
    volatile int jpeg_failed;
    unsigned char * volatile icc_profile;
    volatile unsigned int icc_profile_length;
    unsigned char *icc_profile_tmp;
    unsigned int icc_profile_length_tmp;

    status = SIXEL_JPEG_ERROR;
    row_stride = 0u;
    pixel_count = 0u;
    size = 0u;
    buffer = NULL;
    data_precision = 8;
    decode_cmyk = 0;
    output_components = 0u;
    jpeg_failed = 0;
    icc_profile_tmp = NULL;
    icc_profile_length_tmp = 0u;
    *result = NULL;
    memset(&cinfo, 0, sizeof(cinfo));
    memset(&jerr, 0, sizeof(jerr));
    icc_profile = NULL;
    icc_profile_length = 0u;
    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = sixel_loader_libjpeg_error_exit;

    if (setjmp(jerr.jmpbuf) != 0) {
        jpeg_failed = 1;
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, data, datasize);
    if (enable_cms) {
        jpeg_save_markers(&cinfo, JPEG_APP0 + 2, 0xFFFF);
    }
    jpeg_read_header(&cinfo, TRUE);
    if (enable_cms) {
        icc_profile_tmp = NULL;
        icc_profile_length_tmp = 0u;
        (void)jpeg_collect_icc_profile(&cinfo,
                                       &icc_profile_tmp,
                                       &icc_profile_length_tmp,
                                       allocator);
        icc_profile = icc_profile_tmp;
        icc_profile_length = icc_profile_length_tmp;
    }

    /* disable colormap (indexed color), grayscale -> rgb */
    cinfo.quantize_colors = FALSE;
    if (cinfo.num_components == 4) {
        cinfo.out_color_space = JCS_CMYK;
        decode_cmyk = 1;
    } else {
        cinfo.out_color_space = JCS_RGB;
    }
    jpeg_start_decompress(&cinfo);
    data_precision = cinfo.data_precision;
    output_components = cinfo.output_components;

    if ((!decode_cmyk && output_components != 3u) ||
        (decode_cmyk && output_components != 4u)) {
        sixel_helper_set_additional_message(
            "load_jpeg: unknown pixel format.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    *ppixelformat = enable_cms ? SIXEL_PIXELFORMAT_LINEARRGBFLOAT32
                               : SIXEL_PIXELFORMAT_RGBFLOAT32;

    if (cinfo.output_width > INT_MAX || cinfo.output_height > INT_MAX) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto end;
    }
    *pwidth = (int)cinfo.output_width;
    *pheight = (int)cinfo.output_height;

    if ((size_t)cinfo.output_width > SIZE_MAX / (size_t)cinfo.output_height) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto end;
    }
    pixel_count = (size_t)cinfo.output_width * (size_t)cinfo.output_height;
    if (pixel_count > SIZE_MAX / (size_t)output_components) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto end;
    }
    if (data_precision <= 8) {
        size = pixel_count * (size_t)output_components;
        *result = (unsigned char *)sixel_allocator_malloc(allocator, size);
        if (*result == NULL) {
            sixel_helper_set_additional_message(
                "load_jpeg: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        row_stride = cinfo.output_width * output_components;
        buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo,
                                            JPOOL_IMAGE,
                                            row_stride,
                                            1);

        while (cinfo.output_scanline < cinfo.output_height) {
            if (jpeg_read_scanlines(&cinfo, buffer, 1) != 1u) {
                status = SIXEL_JPEG_ERROR;
                goto end;
            }
            if (cinfo.err->num_warnings > 0) {
                sixel_helper_set_additional_message(
                    "jpeg_read_scanlines: error/warning occuered.");
                status = SIXEL_BAD_INPUT;
                goto end;
            }
            memcpy(*result + ((size_t)cinfo.output_scanline - 1u)
                             * (size_t)row_stride,
                   buffer[0],
                   row_stride);
        }

        if (decode_cmyk) {
            status = jpeg_promote_cmyk888_to_linear_float32(result,
                                                            *pwidth,
                                                            *pheight,
                                                            allocator,
                                                            enable_cms,
                                                            icc_profile,
                                                            icc_profile_length
                                                            );
        } else {
            status = jpeg_promote_rgb888_to_linear_float32(result,
                                                           *pwidth,
                                                           *pheight,
                                                           allocator,
                                                           enable_cms,
                                                           icc_profile,
                                                           icc_profile_length
                                                           );
        }
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    } else if (data_precision <= 16) {
        if (decode_cmyk) {
            if (pixel_count > SIZE_MAX / (4u * sizeof(uint16_t))) {
                status = SIXEL_BAD_INTEGER_OVERFLOW;
                goto end;
            }
            size = pixel_count * 4u * sizeof(uint16_t);
            *result = (unsigned char *)sixel_allocator_malloc(allocator, size);
            if (*result == NULL) {
                sixel_helper_set_additional_message(
                    "load_jpeg: sixel_allocator_malloc() failed.");
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }

            status = jpeg_decode_cmyk16_from_precision(&cinfo,
                                                       (uint16_t *)*result,
                                                       data_precision);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
            status = jpeg_promote_cmyk16_to_linear_float32(result,
                                                           *pwidth,
                                                           *pheight,
                                                           allocator,
                                                           enable_cms,
                                                           icc_profile,
                                                           icc_profile_length
                                                           );
            if (SIXEL_FAILED(status)) {
                goto end;
            }
        } else {
            if (pixel_count > SIZE_MAX / (3u * sizeof(float))) {
                status = SIXEL_BAD_INTEGER_OVERFLOW;
                goto end;
            }
            size = pixel_count * 3u * sizeof(float);
            *result = (unsigned char *)sixel_allocator_malloc(allocator, size);
            if (*result == NULL) {
                sixel_helper_set_additional_message(
                    "load_jpeg: sixel_allocator_malloc() failed.");
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }

            status = jpeg_decode_to_float32_from_precision(&cinfo,
                                                           (float *)*result,
                                                           data_precision);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
#if HAVE_LCMS2
            if (enable_cms) {
                (void)jpeg_convert_icc_rgbf32_to_srgb((float *)*result,
                                                      pixel_count,
                                                      icc_profile,
                                                      icc_profile_length);
            }
#else
            if (enable_cms && icc_profile != NULL && icc_profile_length > 0u) {
                sixel_cms_convert_to_srgb_with_profile_bytes(
                    *result,
                    *pwidth,
                    *pheight,
                    SIXEL_PIXELFORMAT_RGBFLOAT32,
                    icc_profile,
                    (size_t)icc_profile_length);
            }
#endif
            if (enable_cms) {
                status = jpeg_convert_rgbf32_gamma_to_linear((float *)*result,
                                                             pixel_count);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
            }
        }
    } else {
        sixel_helper_set_additional_message(
            "load_jpeg: unsupported JPEG precision.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    status = SIXEL_OK;

end:
    if (SIXEL_FAILED(status)) {
        sixel_allocator_free(allocator, *result);
        *result = NULL;
    }
    if (cinfo.mem != NULL) {
        if (!jpeg_failed) {
            (void)jpeg_finish_decompress(&cinfo);
        }
        jpeg_destroy_decompress(&cinfo);
    }
    sixel_allocator_free(allocator, icc_profile);

    return status;
}

static unsigned short
jpeg_read_u16be(unsigned char const *p)
{
    if (p == NULL) {
        return 0u;
    }

    return (unsigned short)((unsigned short)p[0] << 8u |
                            (unsigned short)p[1]);
}

/*
 * Scan JPEG APP1 markers and parse Exif orientation when present.
 *
 * The parser ignores unknown APP1 payloads (for example XMP) and keeps the
 * default orientation if the metadata is missing or malformed.
 */
static int
jpeg_parse_exif_orientation(unsigned char const *data,
                            size_t size,
                            int *orientation)
{
    size_t offset;
    unsigned char marker;
    unsigned short segment_length;
    size_t payload_size;

    offset = 0u;
    marker = 0u;
    segment_length = 0u;
    payload_size = 0u;
    if (data == NULL || orientation == NULL || size < 4u) {
        return 0;
    }
    if (data[0] != (unsigned char)0xff || data[1] != (unsigned char)0xd8) {
        return 0;
    }

    offset = 2u;
    while (offset + 1u < size) {
        if (data[offset] != (unsigned char)0xff) {
            ++offset;
            continue;
        }
        while (offset < size && data[offset] == (unsigned char)0xff) {
            ++offset;
        }
        if (offset >= size) {
            break;
        }

        marker = data[offset];
        ++offset;
        if (marker == (unsigned char)0xd9 || marker == (unsigned char)0xda) {
            break;
        }
        if (marker == (unsigned char)0x01 ||
            (marker >= (unsigned char)0xd0 &&
             marker <= (unsigned char)0xd7)) {
            continue;
        }
        if (offset + 2u > size) {
            break;
        }

        segment_length = jpeg_read_u16be(data + offset);
        offset += 2u;
        if (segment_length < 2u) {
            break;
        }
        payload_size = (size_t)segment_length - 2u;
        if (offset > size || payload_size > size - offset) {
            break;
        }

        if (marker == (unsigned char)0xe1 && payload_size >= 6u &&
            memcmp(data + offset, "Exif\0\0", 6u) == 0 &&
            loader_exif_parse_orientation(data + offset,
                                          payload_size,
                                          orientation)) {
            return 1;
        }

        offset += payload_size;
    }

    return 0;
}

/*
 * Dedicated libjpeg loader wiring minimal pipeline.
 *
 *    +------------+     +-------------------+     +--------------------+
 *    | JPEG chunk | --> | libjpeg decode    | --> | sixel frame emit   |
 *    +------------+     +-------------------+     +--------------------+
 */
static SIXELSTATUS
load_with_libjpeg(
    sixel_chunk_t const       /* in */     *pchunk,
    int                       /* in */     enable_cms,
    int                       /* in */     enable_orientation,
    int                       /* in */     fstatic,
    int                       /* in */     fuse_palette,
    int                       /* in */     reqcolors,
    unsigned char             /* in */     *bgcolor,
    int                       /* in */     loop_control,
    int                       /* in */     start_frame_no_set,
    int                       /* in */     start_frame_no,
    sixel_load_image_function /* in */     fn_load,
    void                      /* in/out */ *context)
{
    SIXELSTATUS status;
    sixel_frame_t *frame;
    unsigned char *pixels;
    int pixelformat;
    int exif_orientation;

    status = SIXEL_FALSE;
    frame = NULL;
    pixels = NULL;
    pixelformat = SIXEL_PIXELFORMAT_RGB888;
    exif_orientation = 1;

    (void)fstatic;
    (void)fuse_palette;
    (void)reqcolors;
    (void)loop_control;
    (void)start_frame_no_set;
    (void)start_frame_no;

    if (enable_orientation) {
        (void)jpeg_parse_exif_orientation(pchunk->buffer,
                                          pchunk->size,
                                          &exif_orientation);
    }

    status = sixel_frame_create_from_factory(&frame, pchunk->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = load_jpeg(&pixels,
                       pchunk->buffer,
                       pchunk->size,
                       &frame->width,
                       &frame->height,
                       &pixelformat,
                       pchunk->allocator,
                       enable_cms);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_frame_as_interface(frame)->vtbl->init_pixels(
        sixel_frame_as_interface(frame),
        &(sixel_frame_pixels_request_t){
            pixels,
            NULL,
            frame->width,
            frame->height,
            pixelformat,
            -1,
            -1,
            SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat)
            ? SIXEL_FRAME_PIXELS_FLOAT32
            : SIXEL_FRAME_PIXELS_U8
        });
    if (SIXEL_FAILED(status)) {
        sixel_allocator_free(pchunk->allocator, pixels);
        goto end;
    }
    pixels = NULL;

    status = sixel_frame_strip_alpha(frame, bgcolor);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    if (enable_orientation && exif_orientation >= 2 && exif_orientation <= 8) {
        status = loader_frame_apply_orientation(frame, exif_orientation);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    status = fn_load(frame, context);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = SIXEL_OK;

end:
    sixel_frame_unref(frame);

    return status;
}

static void
sixel_loader_libjpeg_ref(sixel_loader_component_t *component)
{
    sixel_loader_libjpeg_component_t *self;

    self = NULL;
    if (component == NULL) {
        return;
    }

    self = (sixel_loader_libjpeg_component_t *)component;
    ++self->ref;
}

static void
sixel_loader_libjpeg_unref(sixel_loader_component_t *component)
{
    sixel_loader_libjpeg_component_t *self;
    sixel_allocator_t *allocator;

    self = NULL;
    allocator = NULL;
    if (component == NULL) {
        return;
    }

    self = (sixel_loader_libjpeg_component_t *)component;
    if (self->ref == 0u) {
        return;
    }

    --self->ref;
    if (self->ref > 0u) {
        return;
    }

    allocator = self->allocator;
    sixel_allocator_free(allocator, self);
    sixel_allocator_unref(allocator);
}

static SIXELSTATUS
sixel_loader_libjpeg_setopt(sixel_loader_component_t *component,
                            int option,
                            void const *value)
{
    sixel_loader_libjpeg_component_t *self;
    int const *flag;
    unsigned char const *color;

    self = NULL;
    flag = NULL;
    color = NULL;
    if (component == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    self = (sixel_loader_libjpeg_component_t *)component;
    switch (option) {
    case SIXEL_LOADER_COMPONENT_OPTION_LIBJPEG_ENABLE_CMS:
        flag = (int const *)value;
        self->enable_cms = (flag != NULL && *flag != 0) ? 1 : 0;
        return SIXEL_OK;
    case SIXEL_LOADER_COMPONENT_OPTION_LIBJPEG_ENABLE_ORIENTATION:
        flag = (int const *)value;
        self->enable_orientation = (flag == NULL || *flag != 0) ? 1 : 0;
        return SIXEL_OK;
    case SIXEL_LOADER_COMPONENT_OPTION_CMS_ENGINE:
        flag = (int const *)value;
        if (flag != NULL && *flag >= 0) {
            self->enable_cms = (*flag == SIXEL_CMS_ENGINE_NONE) ? 0 : 1;
        }
        sixel_helper_set_loader_cms_engine(flag != NULL ? *flag : -1);
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_REQUIRE_STATIC:
        flag = (int const *)value;
        self->fstatic = flag != NULL ? *flag : 0;
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_USE_PALETTE:
        flag = (int const *)value;
        self->fuse_palette = flag != NULL ? *flag : 0;
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_REQCOLORS:
        flag = (int const *)value;
        if (flag != NULL) {
            self->reqcolors = *flag;
        }
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_BGCOLOR:
        if (value == NULL) {
            self->has_bgcolor = 0;
            return SIXEL_OK;
        }
        color = (unsigned char const *)value;
        self->bgcolor[0] = color[0];
        self->bgcolor[1] = color[1];
        self->bgcolor[2] = color[2];
        self->has_bgcolor = 1;
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_LOOP_CONTROL:
        flag = (int const *)value;
        if (flag != NULL) {
            self->loop_control = *flag;
        }
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_START_FRAME_NO:
        if (value == NULL) {
            self->has_start_frame_no = 0;
            self->start_frame_no = INT_MIN;
            return SIXEL_OK;
        }
        flag = (int const *)value;
        self->start_frame_no = *flag;
        self->has_start_frame_no = 1;
        return SIXEL_OK;
    default:
        return SIXEL_OK;
    }
}

static SIXELSTATUS
sixel_loader_libjpeg_load(sixel_loader_component_t *component,
                          sixel_chunk_t const *chunk,
                          sixel_load_image_function fn_load,
                          void *context)
{
    sixel_loader_libjpeg_component_t *self;
    unsigned char *bgcolor;
    SIXELSTATUS status;
    int header_job_id;
    int decode_job_id;
    sixel_loader_timeline_callback_state_t timeline_state;

    self = NULL;
    bgcolor = NULL;
    status = SIXEL_FALSE;
    header_job_id = -1;
    decode_job_id = -1;
    if (component == NULL || chunk == NULL || fn_load == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    self = (sixel_loader_libjpeg_component_t *)component;
    if (self->has_bgcolor) {
        bgcolor = self->bgcolor;
    }

    header_job_id = loader_timeline_phase_start("header/read");
    decode_job_id = loader_timeline_phase_start("decode/pixels");
    loader_timeline_callback_state_init(&timeline_state,
                                        fn_load,
                                        context,
                                        header_job_id,
                                        decode_job_id);

    status = load_with_libjpeg(chunk,
                               self->enable_cms,
                               self->enable_orientation,
                               self->fstatic,
                               self->fuse_palette,
                               self->reqcolors,
                               bgcolor,
                               self->loop_control,
                               self->has_start_frame_no,
                               self->start_frame_no,
                               loader_timeline_emit_frame_callback,
                               &timeline_state);

    loader_timeline_callback_close_header(&timeline_state, status);
    loader_timeline_callback_close_decode(&timeline_state, status);
    loader_timeline_optional_skip_if_unmarked("post/colorspace");
    loader_timeline_optional_skip_if_unmarked("post/background");
    loader_timeline_optional_skip_if_unmarked("post/icc");

    return status;
}

static char const *
sixel_loader_libjpeg_name(sixel_loader_component_t const *component)
{
    (void)component;
    return "libjpeg";
}

static int
sixel_loader_libjpeg_predicate(sixel_loader_component_t *component,
                               sixel_chunk_t const *chunk)
{
    (void)component;
    return loader_can_try_libjpeg(chunk);
}

static sixel_loader_component_vtbl_t const g_sixel_loader_libjpeg_vtbl = {
    sixel_loader_libjpeg_ref,
    sixel_loader_libjpeg_unref,
    sixel_loader_libjpeg_setopt,
    sixel_loader_libjpeg_load,
    sixel_loader_libjpeg_name,
    sixel_loader_libjpeg_predicate
};

SIXELSTATUS
sixel_loader_libjpeg_new(sixel_allocator_t *allocator,
                         void **ppcomponent)
{
    sixel_loader_libjpeg_component_t *self;

    self = NULL;
    if (allocator == NULL || ppcomponent == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *ppcomponent = NULL;
    self = (sixel_loader_libjpeg_component_t *)
        sixel_allocator_malloc(allocator, sizeof(*self));
    if (self == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    memset(self, 0, sizeof(*self));
    self->base.vtbl = &g_sixel_loader_libjpeg_vtbl;
    self->allocator = allocator;
    self->ref = 1u;
    self->enable_cms = 0;
    self->enable_orientation = 1;
    self->reqcolors = 256;
    self->start_frame_no = INT_MIN;
    sixel_allocator_ref(allocator);
    *ppcomponent = &self->base;
    return SIXEL_OK;
}

static int
loader_can_try_libjpeg(sixel_chunk_t const *chunk)
{
    if (chunk == NULL) {
        return 0;
    }

    return chunk_is_jpeg(chunk);
}

#else  /* !HAVE_JPEG */

/*
 * Keep a harmless placeholder around so pedantic builds skip the empty unit
 * warning when libjpeg is not part of the build.
 */
enum { sixel_loader_libjpeg_placeholder = 0 };

#if defined(__GNUC__) || defined(__clang__)
# define SIXEL_LIBJPEG_PLACEHOLDER_UNUSED __attribute__((unused))
#else
# define SIXEL_LIBJPEG_PLACEHOLDER_UNUSED
#endif

static void
sixel_loader_libjpeg_placeholder_function(void)
    SIXEL_LIBJPEG_PLACEHOLDER_UNUSED;

static void
sixel_loader_libjpeg_placeholder_function(void)
{
    /*
     * The placeholder ties the enum to a symbol so MSVC does not warn about
     * an empty translation unit when libjpeg support is disabled.
     */
    (void)sixel_loader_libjpeg_placeholder;
}

#undef SIXEL_LIBJPEG_PLACEHOLDER_UNUSED

#endif  /* HAVE_JPEG */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
