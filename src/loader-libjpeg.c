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
#if HAVE_STDINT_H
# include <stdint.h>
#endif
#include <jpeglib.h>

#include <sixel.h>

#include "allocator.h"
#include "cms.h"
#include "chunk.h"
#include "loader-common.h"
#include "loader-component.h"
#include "frame.h"
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
    int fstatic;
    int fuse_palette;
    int reqcolors;
    unsigned char bgcolor[3];
    int has_bgcolor;
    int loop_control;
    int has_start_frame_no;
    int start_frame_no;
} sixel_loader_libjpeg_component_t;

#if HAVE_LCMS2
/*
 * JPEG ICC profile blocks are stored in APP2 markers as "ICC_PROFILE\0"
 * payloads with sequence numbering. This parser reassembles the profile in
 * marker order and returns a contiguous ICC blob for lcms2.
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
#endif

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
#endif

static SIXELSTATUS
jpeg_promote_rgb888_to_linear_float32(unsigned char **result,
                                      int width,
                                      int height,
                                      sixel_allocator_t *allocator
#if HAVE_LCMS2
                                      ,
                                      unsigned char const *icc_profile,
                                      unsigned int icc_profile_length
#endif
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
    if (icc_profile != NULL && icc_profile_length > 0u) {
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
          sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    JDIMENSION row_stride;
    size_t pixel_count;
    size_t size;
    JSAMPARRAY buffer;
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr pub;
    int data_precision;
#if HAVE_LCMS2
    unsigned char *icc_profile;
    unsigned int icc_profile_length;
#endif

    status = SIXEL_JPEG_ERROR;
    row_stride = 0u;
    pixel_count = 0u;
    size = 0u;
    buffer = NULL;
    data_precision = 8;
    *result = NULL;
#if HAVE_LCMS2
    icc_profile = NULL;
    icc_profile_length = 0u;
#endif
    cinfo.err = jpeg_std_error(&pub);

    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, data, datasize);
#if HAVE_LCMS2
    jpeg_save_markers(&cinfo, JPEG_APP0 + 2, 0xFFFF);
#endif
    jpeg_read_header(&cinfo, TRUE);
#if HAVE_LCMS2
    (void)jpeg_collect_icc_profile(&cinfo,
                                   &icc_profile,
                                   &icc_profile_length,
                                   allocator);
#endif

    /* disable colormap (indexed color), grayscale -> rgb */
    cinfo.quantize_colors = FALSE;
    cinfo.out_color_space = JCS_RGB;
    jpeg_start_decompress(&cinfo);
    data_precision = cinfo.data_precision;

    if (cinfo.output_components != 3) {
        sixel_helper_set_additional_message(
            "load_jpeg: unknown pixel format.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    *ppixelformat = SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;

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
    if (pixel_count > SIZE_MAX / (size_t)cinfo.output_components) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto end;
    }
    if (data_precision <= 8) {
        size = pixel_count * (size_t)cinfo.output_components;
        *result = (unsigned char *)sixel_allocator_malloc(allocator, size);
        if (*result == NULL) {
            sixel_helper_set_additional_message(
                "load_jpeg: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        row_stride = cinfo.output_width * (unsigned int)cinfo.output_components;
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

        status = jpeg_promote_rgb888_to_linear_float32(result,
                                                       *pwidth,
                                                       *pheight,
                                                       allocator
#if HAVE_LCMS2
                                                       ,
                                                       icc_profile,
                                                       icc_profile_length
#endif
                                                       );
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    } else if (data_precision <= 16) {
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
        (void)jpeg_convert_icc_rgbf32_to_srgb((float *)*result,
                                              pixel_count,
                                              icc_profile,
                                              icc_profile_length);
#endif
        status = jpeg_convert_rgbf32_gamma_to_linear((float *)*result,
                                                     pixel_count);
        if (SIXEL_FAILED(status)) {
            goto end;
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
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
#if HAVE_LCMS2
    sixel_allocator_free(allocator, icc_profile);
#endif

    return status;
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

    status = SIXEL_FALSE;
    frame = NULL;
    pixels = NULL;
    pixelformat = SIXEL_PIXELFORMAT_RGB888;

    (void)fstatic;
    (void)fuse_palette;
    (void)reqcolors;
    (void)loop_control;
    (void)start_frame_no_set;
    (void)start_frame_no;

    status = sixel_frame_new(&frame, pchunk->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = load_jpeg(&pixels,
                       pchunk->buffer,
                       pchunk->size,
                       &frame->width,
                       &frame->height,
                       &pixelformat,
                       pchunk->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_frame_set_pixelformat(frame, pixelformat);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat)) {
        sixel_frame_set_pixels_float32(frame, (float *)pixels);
    } else {
        sixel_frame_set_pixels(frame, pixels);
    }

    status = sixel_frame_strip_alpha(frame, bgcolor);
    if (SIXEL_FAILED(status)) {
        goto end;
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

    self = NULL;
    bgcolor = NULL;
    if (component == NULL || chunk == NULL || fn_load == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    self = (sixel_loader_libjpeg_component_t *)component;
    if (self->has_bgcolor) {
        bgcolor = self->bgcolor;
    }

    return load_with_libjpeg(chunk,
                             self->fstatic,
                             self->fuse_palette,
                             self->reqcolors,
                             bgcolor,
                             self->loop_control,
                             self->has_start_frame_no,
                             self->start_frame_no,
                             fn_load,
                             context);
}

static char const *
sixel_loader_libjpeg_name(sixel_loader_component_t const *component)
{
    (void)component;
    return "libjpeg";
}

static sixel_loader_component_vtbl_t const g_sixel_loader_libjpeg_vtbl = {
    sixel_loader_libjpeg_ref,
    sixel_loader_libjpeg_unref,
    sixel_loader_libjpeg_setopt,
    sixel_loader_libjpeg_load,
    sixel_loader_libjpeg_name
};

SIXELSTATUS
sixel_loader_libjpeg_new(sixel_allocator_t *allocator,
                         sixel_loader_component_t **ppcomponent)
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
    self->reqcolors = 256;
    self->start_frame_no = INT_MIN;
    sixel_allocator_ref(allocator);
    *ppcomponent = &self->base;
    return SIXEL_OK;
}

int
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
