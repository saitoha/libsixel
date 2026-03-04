/*
 * SPDX-License-Identifier: MIT
 *
 * PNG helpers shared by builtin loader path.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#if HAVE_STDINT_H
#include <stdint.h>
#endif
#if HAVE_LIMITS_H
#include <limits.h>
#endif

#include "allocator.h"
#include "cms.h"
#include "compat_stub.h"
#include "frompng.h"

char const *stbi_failure_reason(void);
void stbi_image_free(void *retval_from_stbi_load);
int stbi_is_16_bit_from_memory(unsigned char const *buffer, int len);
unsigned short *stbi_load_16_from_memory(unsigned char const *buffer,
                                         int len,
                                         int *x,
                                         int *y,
                                         int *channels_in_file,
                                         int desired_channels);
unsigned char *stbi_load_from_memory(unsigned char const *buffer,
                                     int len,
                                     int *x,
                                     int *y,
                                     int *channels_in_file,
                                     int desired_channels);
char *stbi_zlib_decode_malloc_guesssize_headerflag(char const *buffer,
                                                   int len,
                                                   int initial_size,
                                                   int *outlen,
                                                   int parse_header);

#ifndef SIZE_MAX
#define SIZE_MAX ((size_t)-1)
#endif

static SIXELSTATUS
sixel_frompng_convert_rgb16_to_rgbfloat32(unsigned char **result,
                                           uint16_t const *src16,
                                           int width,
                                           int height,
                                           sixel_allocator_t *allocator)
{
    float *dst;
    size_t pixel_count;
    size_t total_bytes;
    size_t index;
    size_t src_offset;
    size_t dst_offset;

    if (result == NULL || src16 == NULL || allocator == NULL
            || width <= 0 || height <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    if ((size_t)width > SIZE_MAX / (size_t)height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    pixel_count = (size_t)width * (size_t)height;
    if (pixel_count > SIZE_MAX / (3u * sizeof(float))) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    total_bytes = pixel_count * 3u * sizeof(float);
    dst = (float *)sixel_allocator_malloc(allocator, total_bytes);
    if (dst == NULL) {
        sixel_helper_set_additional_message(
            "load_with_builtin: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    for (index = 0u; index < pixel_count; ++index) {
        src_offset = index * 3u;
        dst_offset = index * 3u;
        dst[dst_offset + 0u] = (float)src16[src_offset + 0u] / 65535.0f;
        dst[dst_offset + 1u] = (float)src16[src_offset + 1u] / 65535.0f;
        dst[dst_offset + 2u] = (float)src16[src_offset + 2u] / 65535.0f;
    }

    *result = (unsigned char *)dst;

    return SIXEL_OK;
}

#if HAVE_LCMS2
static void
sixel_frompng_convert_profile_to_srgb(unsigned char *pixels,
                                      int width,
                                      int height,
                                      sixel_cms_profile_t *src_profile)
{
    sixel_cms_profile_t *dst_profile;
    sixel_cms_transform_t *transform;
    sixel_cms_color_space_t src_colorspace;
    size_t pixel_count;
    unsigned char *gray_in;
    unsigned char *rgb_out;
    size_t i;

    dst_profile = NULL;
    transform = NULL;
    src_colorspace = SIXEL_CMS_COLORSPACE_RGB;
    pixel_count = 0u;
    gray_in = NULL;
    rgb_out = NULL;
    i = 0u;

    if (pixels == NULL || width <= 0 || height <= 0 || src_profile == NULL) {
        return;
    }

    src_colorspace = sixel_cms_get_color_space(src_profile);
    dst_profile = sixel_cms_create_srgb_profile();
    if (dst_profile == NULL) {
        goto cleanup;
    }

    pixel_count = (size_t)width * (size_t)height;
    if (src_colorspace == SIXEL_CMS_COLORSPACE_GRAY) {
        gray_in = (unsigned char *)malloc(pixel_count);
        rgb_out = (unsigned char *)malloc(pixel_count * 3u);
        if (gray_in == NULL || rgb_out == NULL) {
            goto cleanup;
        }
        for (i = 0u; i < pixel_count; ++i) {
            gray_in[i] = pixels[i * 3u];
        }
        transform = sixel_cms_create_transform(src_profile,
                                               SIXEL_CMS_PIXELFORMAT_GRAY_8,
                                               dst_profile,
                                               SIXEL_CMS_PIXELFORMAT_RGB_8,
                                               SIXEL_CMS_TRANSFORM_DEFAULT);
        if (transform == NULL) {
            goto cleanup;
        }
        sixel_cms_do_transform(transform, gray_in, rgb_out, pixel_count);
        memcpy(pixels, rgb_out, pixel_count * 3u);
    } else {
        transform = sixel_cms_create_transform(src_profile,
                                               SIXEL_CMS_PIXELFORMAT_RGB_8,
                                               dst_profile,
                                               SIXEL_CMS_PIXELFORMAT_RGB_8,
                                               SIXEL_CMS_TRANSFORM_DEFAULT);
        if (transform == NULL) {
            goto cleanup;
        }
        sixel_cms_do_transform(transform, pixels, pixels, pixel_count);
    }

cleanup:
    free(rgb_out);
    free(gray_in);
    if (transform != NULL) {
        sixel_cms_delete_transform(transform);
    }
    if (dst_profile != NULL) {
        sixel_cms_close_profile(dst_profile);
    }
}

void
sixel_frompng_convert_icc_to_srgb(unsigned char *pixels,
                                  int width,
                                  int height,
                                  unsigned char const *profile,
                                  size_t profile_length)
{
    sixel_cms_profile_t *src_profile;

    src_profile = NULL;
    if (pixels == NULL || width <= 0 || height <= 0
            || profile == NULL || profile_length == 0u) {
        return;
    }

    src_profile = sixel_cms_open_profile_from_mem(profile, profile_length);
    if (src_profile == NULL) {
        return;
    }
    sixel_frompng_convert_profile_to_srgb(pixels, width, height, src_profile);
    sixel_cms_close_profile(src_profile);
}

static uint32_t
sixel_frompng_read_be32(unsigned char const *p)
{
    return ((uint32_t)p[0] << 24)
        | ((uint32_t)p[1] << 16)
        | ((uint32_t)p[2] << 8)
        | (uint32_t)p[3];
}

static int
sixel_frompng_detect_chunk_flags(unsigned char const *buffer,
                                 size_t size,
                                 int *has_iccp,
                                 int *has_srgb,
                                 int *has_chrm,
                                 int *has_gama)
{
    static unsigned char const png_signature[8] = {
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au
    };
    size_t offset;
    uint32_t chunk_length;
    size_t chunk_total;
    unsigned char const *chunk_type;

    if (has_iccp == NULL || has_srgb == NULL || has_chrm == NULL
            || has_gama == NULL) {
        return 0;
    }

    *has_iccp = 0;
    *has_srgb = 0;
    *has_chrm = 0;
    *has_gama = 0;

    if (buffer == NULL || size < sizeof(png_signature) + 12u) {
        return 0;
    }
    if (memcmp(buffer, png_signature, sizeof(png_signature)) != 0) {
        return 0;
    }

    offset = sizeof(png_signature);
    while (offset + 12u <= size) {
        chunk_length = sixel_frompng_read_be32(buffer + offset);
        chunk_total = (size_t)chunk_length + 12u;
        if (chunk_total > size - offset) {
            return 0;
        }
        chunk_type = buffer + offset + 4u;

        if (memcmp(chunk_type, "iCCP", 4u) == 0) {
            *has_iccp = 1;
        } else if (memcmp(chunk_type, "sRGB", 4u) == 0) {
            *has_srgb = 1;
        } else if (memcmp(chunk_type, "cHRM", 4u) == 0) {
            *has_chrm = 1;
        } else if (memcmp(chunk_type, "gAMA", 4u) == 0) {
            *has_gama = 1;
        }
        offset += chunk_total;
    }

    return 1;
}

static int
sixel_frompng_build_profile_from_chunks(unsigned char const *buffer,
                                        size_t size,
                                        sixel_cms_profile_t **profile)
{
    static unsigned char const png_signature[8] = {
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au
    };
    sixel_cms_profile_t *built_profile;
    size_t offset;
    uint32_t chunk_length;
    size_t chunk_total;
    unsigned char const *chunk_type;
    unsigned char const *chunk_data;
    int has_chrm;
    int has_gama;
    double white_x;
    double white_y;
    double red_x;
    double red_y;
    double green_x;
    double green_y;
    double blue_x;
    double blue_y;
    double file_gamma;

    if (profile == NULL) {
        return 0;
    }
    *profile = NULL;
    if (buffer == NULL || size < sizeof(png_signature) + 12u) {
        return 0;
    }
    if (memcmp(buffer, png_signature, sizeof(png_signature)) != 0) {
        return 0;
    }

    built_profile = NULL;
    has_chrm = 0;
    has_gama = 0;
    white_x = 0.0;
    white_y = 0.0;
    red_x = 0.0;
    red_y = 0.0;
    green_x = 0.0;
    green_y = 0.0;
    blue_x = 0.0;
    blue_y = 0.0;
    file_gamma = 0.0;

    offset = sizeof(png_signature);
    while (offset + 12u <= size) {
        chunk_length = sixel_frompng_read_be32(buffer + offset);
        chunk_total = (size_t)chunk_length + 12u;
        if (chunk_total > size - offset) {
            return 0;
        }
        chunk_type = buffer + offset + 4u;
        chunk_data = buffer + offset + 8u;

        if (memcmp(chunk_type, "cHRM", 4u) == 0 && chunk_length >= 32u) {
            white_x = (double)sixel_frompng_read_be32(chunk_data + 0u)
                / 100000.0;
            white_y = (double)sixel_frompng_read_be32(chunk_data + 4u)
                / 100000.0;
            red_x = (double)sixel_frompng_read_be32(chunk_data + 8u)
                / 100000.0;
            red_y = (double)sixel_frompng_read_be32(chunk_data + 12u)
                / 100000.0;
            green_x = (double)sixel_frompng_read_be32(chunk_data + 16u)
                / 100000.0;
            green_y = (double)sixel_frompng_read_be32(chunk_data + 20u)
                / 100000.0;
            blue_x = (double)sixel_frompng_read_be32(chunk_data + 24u)
                / 100000.0;
            blue_y = (double)sixel_frompng_read_be32(chunk_data + 28u)
                / 100000.0;
            has_chrm = 1;
        } else if (memcmp(chunk_type, "gAMA", 4u) == 0
                   && chunk_length >= 4u) {
            file_gamma = (double)sixel_frompng_read_be32(chunk_data)
                / 100000.0;
            has_gama = 1;
        }
        offset += chunk_total;
    }

    if (!has_gama) {
        return 0;
    }
    if (!has_chrm) {
        white_x = 0.3127;
        white_y = 0.3290;
        red_x = 0.6400;
        red_y = 0.3300;
        green_x = 0.3000;
        green_y = 0.6000;
        blue_x = 0.1500;
        blue_y = 0.0600;
    }
    if (file_gamma <= 0.0) {
        return 0;
    }

    built_profile = sixel_cms_create_rgb_profile_from_gamma_chrm(file_gamma,
                                                                 white_x,
                                                                 white_y,
                                                                 red_x,
                                                                 red_y,
                                                                 green_x,
                                                                 green_y,
                                                                 blue_x,
                                                                 blue_y);
    if (built_profile == NULL) {
        return 0;
    }

    *profile = built_profile;
    return 1;
}

static int
sixel_frompng_extract_icc(unsigned char const *buffer,
                          size_t size,
                          unsigned char **profile,
                          size_t *profile_length)
{
    static unsigned char const png_signature[8] = {
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au
    };
    size_t offset;
    uint32_t chunk_length;
    unsigned char const *chunk_data;
    size_t chunk_total;
    size_t name_index;
    size_t compressed_offset;
    unsigned char compression_method;
    unsigned char *decoded;
    int decoded_length;

    if (profile == NULL || profile_length == NULL) {
        return 0;
    }
    *profile = NULL;
    *profile_length = 0u;

    if (buffer == NULL || size < sizeof(png_signature) + 12u) {
        return 0;
    }
    if (memcmp(buffer, png_signature, sizeof(png_signature)) != 0) {
        return 0;
    }

    offset = sizeof(png_signature);
    while (offset + 12u <= size) {
        chunk_length = sixel_frompng_read_be32(buffer + offset);
        chunk_total = (size_t)chunk_length + 12u;
        if (chunk_total > size - offset) {
            return 0;
        }
        if (memcmp(buffer + offset + 4u, "iCCP", 4u) != 0) {
            offset += chunk_total;
            continue;
        }

        chunk_data = buffer + offset + 8u;
        if (chunk_length < 3u) {
            return 0;
        }
        name_index = 0u;
        while (name_index < (size_t)chunk_length
                && chunk_data[name_index] != 0u) {
            ++name_index;
        }
        if (name_index == (size_t)chunk_length) {
            return 0;
        }
        compressed_offset = name_index + 1u;
        if (compressed_offset >= (size_t)chunk_length) {
            return 0;
        }
        compression_method = chunk_data[compressed_offset];
        if (compression_method != 0u) {
            return 0;
        }
        ++compressed_offset;
        if (compressed_offset >= (size_t)chunk_length) {
            return 0;
        }

        decoded = (unsigned char *)stbi_zlib_decode_malloc_guesssize_headerflag(
            (char const *)(chunk_data + compressed_offset),
            (int)((size_t)chunk_length - compressed_offset),
            16384,
            &decoded_length,
            1);
        if (decoded == NULL || decoded_length <= 0) {
            stbi_image_free(decoded);
            return 0;
        }
        *profile = decoded;
        *profile_length = (size_t)decoded_length;
        return 1;
    }

    return 0;
}

void
sixel_frompng_apply_colorspace_fallback(unsigned char *pixels,
                                        int width,
                                        int height,
                                        unsigned char const *buffer,
                                        size_t size,
                                        sixel_allocator_t *allocator)
{
    unsigned char *icc_profile;
    size_t icc_profile_length;
    sixel_cms_profile_t *chunk_profile;
    int has_iccp;
    int has_srgb;
    int has_chrm;
    int has_gama;

    icc_profile = NULL;
    icc_profile_length = 0u;
    chunk_profile = NULL;
    has_iccp = 0;
    has_srgb = 0;
    has_chrm = 0;
    has_gama = 0;

    if (pixels == NULL || width <= 0 || height <= 0
            || buffer == NULL || allocator == NULL) {
        return;
    }
    if (!sixel_frompng_detect_chunk_flags(buffer,
                                          size,
                                          &has_iccp,
                                          &has_srgb,
                                          &has_chrm,
                                          &has_gama)) {
        return;
    }
    if (has_iccp && has_srgb && has_chrm) {
        return;
    }
    if (has_iccp) {
        if (sixel_frompng_extract_icc(buffer,
                                      size,
                                      &icc_profile,
                                      &icc_profile_length)) {
            sixel_frompng_convert_icc_to_srgb(pixels,
                                              width,
                                              height,
                                              icc_profile,
                                              icc_profile_length);
        }
    } else if (has_srgb) {
        /* no-op */
    } else if (has_gama
               && sixel_frompng_build_profile_from_chunks(buffer,
                                                          size,
                                                          &chunk_profile)) {
        sixel_frompng_convert_profile_to_srgb(pixels,
                                              width,
                                              height,
                                              chunk_profile);
        sixel_cms_close_profile(chunk_profile);
    }
    if (icc_profile != NULL) {
        sixel_allocator_free(allocator, icc_profile);
    }
}
#else
void
sixel_frompng_convert_icc_to_srgb(unsigned char *pixels,
                                  int width,
                                  int height,
                                  unsigned char const *profile,
                                  size_t profile_length)
{
    (void)pixels;
    (void)width;
    (void)height;
    (void)profile;
    (void)profile_length;
}

void
sixel_frompng_apply_colorspace_fallback(unsigned char *pixels,
                                        int width,
                                        int height,
                                        unsigned char const *buffer,
                                        size_t size,
                                        sixel_allocator_t *allocator)
{
    (void)pixels;
    (void)width;
    (void)height;
    (void)buffer;
    (void)size;
    (void)allocator;
}
#endif  /* HAVE_LCMS2 */

SIXELSTATUS
sixel_frompng_load_nonindexed(sixel_chunk_t const *pchunk,
                              sixel_frame_t *frame,
                              int enable_cms)
{
    SIXELSTATUS status;
    int png_is_16bit;
    int depth;
    uint16_t *pixels16;
    unsigned char *pixels8;
    unsigned char *pixels_float32;

    status = SIXEL_FALSE;
    png_is_16bit = 0;
    depth = 0;
    pixels16 = NULL;
    pixels8 = NULL;
    pixels_float32 = NULL;

    if (pchunk == NULL || frame == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    png_is_16bit = stbi_is_16_bit_from_memory(pchunk->buffer, (int)pchunk->size);
    if (png_is_16bit != 0) {
        pixels16 = stbi_load_16_from_memory(pchunk->buffer,
                                            (int)pchunk->size,
                                            &frame->width,
                                            &frame->height,
                                            &depth,
                                            3);
        if (pixels16 == NULL) {
            sixel_helper_set_additional_message(stbi_failure_reason());
            return SIXEL_STBI_ERROR;
        }
        status = sixel_frompng_convert_rgb16_to_rgbfloat32(&pixels_float32,
                                                            pixels16,
                                                            frame->width,
                                                            frame->height,
                                                            pchunk->allocator);
        stbi_image_free(pixels16);
        pixels16 = NULL;
        if (SIXEL_FAILED(status)) {
            return status;
        }
        sixel_frame_set_pixels(frame, pixels_float32);
        frame->loop_count = 1;
        frame->pixelformat = SIXEL_PIXELFORMAT_RGBFLOAT32;
        frame->colorspace = SIXEL_COLORSPACE_GAMMA;
        return SIXEL_OK;
    }

    pixels8 = stbi_load_from_memory(pchunk->buffer,
                                    (int)pchunk->size,
                                    &frame->width,
                                    &frame->height,
                                    &depth,
                                    3);
    if (pixels8 == NULL) {
        sixel_helper_set_additional_message(stbi_failure_reason());
        return SIXEL_STBI_ERROR;
    }
    sixel_frame_set_pixels(frame, pixels8);
    frame->loop_count = 1;
    frame->pixelformat = SIXEL_PIXELFORMAT_RGB888;
    frame->colorspace = SIXEL_COLORSPACE_GAMMA;

    if (enable_cms) {
        sixel_frompng_apply_colorspace_fallback(pixels8,
                                                frame->width,
                                                frame->height,
                                                pchunk->buffer,
                                                pchunk->size,
                                                pchunk->allocator);
    }

    return SIXEL_OK;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 : */
/* EOF */
