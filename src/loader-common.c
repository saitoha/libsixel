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
 * Shared loader helpers used across backend implementations.  This module
 * centralizes trace logging, thumbnail size hints, and small detection
 * helpers so backend files stay narrow and platform headers remain isolated.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#if HAVE_STRING_H
# include <string.h>
#endif
#if HAVE_STDARG_H
# include <stdarg.h>
#endif
#if HAVE_LIMITS_H
# include <limits.h>
#endif
#if HAVE_STDINT_H
# include <stdint.h>
#endif
#if HAVE_ERRNO_H
# include <errno.h>
#endif

/* Keep SIZE_MAX available even on strict C99 environments. */
#ifndef SIZE_MAX
# define SIZE_MAX ((size_t)-1)
#endif

#include <sixel.h>

#include "cms.h"
#include "compat_stub.h"
#include "frame.h"
#include "loader-common.h"
#include "logger.h"

#if defined(_MSC_VER)
# define SIXEL_LOADER_TLS __declspec(thread)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
# define SIXEL_LOADER_TLS _Thread_local
#elif defined(__GNUC__) || defined(__clang__)
# define SIXEL_LOADER_TLS __thread
#else
# define SIXEL_LOADER_TLS
#endif

static int loader_trace_enabled;
static int thumbnailer_default_size_hint = SIXEL_THUMBNAILER_DEFAULT_SIZE;
static int thumbnailer_size_hint = SIXEL_THUMBNAILER_DEFAULT_SIZE;
static int thumbnailer_size_hint_initialized;
static int wic_ico_minsize_default;
static int wic_ico_minsize;
static int wic_ico_minsize_initialized;
static int libpng_enable_cms_default = 0;
static int libpng_enable_cms = 0;
static int builtin_enable_cms_default = 0;
static int builtin_enable_cms = 0;
static int loader_background_colorspace_initialized;
static int loader_background_colorspace_value = SIXEL_COLORSPACE_GAMMA;
/*
 * Per-thread temporary override used by loader.c when OSC11 provides
 * a terminal UI background color.
 */
static SIXEL_LOADER_TLS int loader_background_colorspace_override = -1;
static int loader_cms_target_initialized;
static int loader_cms_prefer_8bit_flag;
static int loader_cms_target_colorspace_value = SIXEL_COLORSPACE_LINEAR;

#undef SIXEL_LOADER_TLS

#define SIXEL_ENV_WIC_ICO_MINSIZE "SIXEL_LOADER_WIC_ICO_MINSIZE"
#define SIXEL_ENV_WIC_ICO_MINSIZE_LEGACY "SIXEL_LODER_WIC_ICO_MINSIZE"

static void
loader_wic_initialize_ico_minsize(void)
{
    char const *env_value;
    char *endptr;
    long parsed;

    if (wic_ico_minsize_initialized) {
        return;
    }

    wic_ico_minsize_initialized = 1;
    wic_ico_minsize_default = 0;
    wic_ico_minsize = 0;

    env_value = sixel_compat_getenv(SIXEL_ENV_WIC_ICO_MINSIZE);
    if (env_value == NULL || env_value[0] == '\0') {
        env_value = sixel_compat_getenv(SIXEL_ENV_WIC_ICO_MINSIZE_LEGACY);
    }
    if (env_value == NULL || env_value[0] == '\0') {
        return;
    }

    errno = 0;
    parsed = strtol(env_value, &endptr, 10);
    if (errno != 0) {
        return;
    }
    if (endptr == env_value || *endptr != '\0') {
        return;
    }
    if (parsed <= 0) {
        return;
    }
    if (parsed > (long)INT_MAX) {
        parsed = (long)INT_MAX;
    }

    wic_ico_minsize_default = (int)parsed;
    wic_ico_minsize = wic_ico_minsize_default;
}

int
loader_wic_get_ico_minsize(void)
{
    loader_wic_initialize_ico_minsize();

    return wic_ico_minsize;
}

void
sixel_helper_set_wic_ico_minsize(int size)
{
    loader_wic_initialize_ico_minsize();

    if (size > 0) {
        wic_ico_minsize = size;
    } else {
        wic_ico_minsize = wic_ico_minsize_default;
    }
}

int
loader_libpng_get_enable_cms(void)
{
    return libpng_enable_cms;
}

void
sixel_helper_set_libpng_enable_cms(int enable)
{
    if (enable >= 0) {
        libpng_enable_cms = enable != 0 ? 1 : 0;
    } else {
        libpng_enable_cms = libpng_enable_cms_default;
    }
}

int
loader_builtin_get_enable_cms(void)
{
    return builtin_enable_cms;
}

void
sixel_helper_set_builtin_enable_cms(int enable)
{
    if (enable >= 0) {
        builtin_enable_cms = enable != 0 ? 1 : 0;
    } else {
        builtin_enable_cms = builtin_enable_cms_default;
    }
}

SIXEL_INTERNAL_API void
sixel_helper_set_loader_background_colorspace(int colorspace)
{
    if (colorspace == SIXEL_COLORSPACE_GAMMA ||
            colorspace == SIXEL_COLORSPACE_LINEAR) {
        loader_background_colorspace_override = colorspace;
    } else {
        loader_background_colorspace_override = -1;
    }
}

static void
loader_background_initialize_colorspace(void)
{
    char const *env_value;

    if (loader_background_colorspace_initialized) {
        return;
    }

    loader_background_colorspace_initialized = 1;
    loader_background_colorspace_value = SIXEL_COLORSPACE_GAMMA;

    env_value = sixel_compat_getenv("SIXEL_LOADER_BACKGROUND_COLORSPACE");
    if (env_value == NULL || env_value[0] == '\0') {
        return;
    }

    if (strcmp(env_value, "linear") == 0) {
        loader_background_colorspace_value = SIXEL_COLORSPACE_LINEAR;
    } else if (strcmp(env_value, "gamma") == 0) {
        loader_background_colorspace_value = SIXEL_COLORSPACE_GAMMA;
    }
}

SIXEL_INTERNAL_API int
loader_background_colorspace(void)
{
    int override_value;

    override_value = loader_background_colorspace_override;
    if (override_value == SIXEL_COLORSPACE_GAMMA ||
            override_value == SIXEL_COLORSPACE_LINEAR) {
        return override_value;
    }

    loader_background_initialize_colorspace();

    return loader_background_colorspace_value;
}

static void
loader_cms_initialize_target(void)
{
    char const *prefer8_env;
    char const *target_env;

    if (loader_cms_target_initialized) {
        return;
    }
    loader_cms_target_initialized = 1;
    loader_cms_prefer_8bit_flag = 0;
    loader_cms_target_colorspace_value = SIXEL_COLORSPACE_LINEAR;

    prefer8_env = sixel_compat_getenv("SIXEL_LOADER_PREFER_8BIT");
    if (prefer8_env != NULL && strcmp(prefer8_env, "1") == 0) {
        loader_cms_prefer_8bit_flag = 1;
    }

    target_env = sixel_compat_getenv("SIXEL_LOADER_CMS_TARGET_COLORSPACE");
    if (target_env == NULL || target_env[0] == '\0') {
        return;
    }
    if (strcmp(target_env, "gamma") == 0) {
        loader_cms_target_colorspace_value = SIXEL_COLORSPACE_GAMMA;
    } else if (strcmp(target_env, "linear") == 0) {
        loader_cms_target_colorspace_value = SIXEL_COLORSPACE_LINEAR;
    } else if (strcmp(target_env, "cielab") == 0) {
        loader_cms_target_colorspace_value = SIXEL_COLORSPACE_CIELAB;
    } else if (strcmp(target_env, "oklab") == 0) {
        loader_cms_target_colorspace_value = SIXEL_COLORSPACE_OKLAB;
    } else if (strcmp(target_env, "din99d") == 0) {
        loader_cms_target_colorspace_value = SIXEL_COLORSPACE_DIN99D;
    }
}

int
loader_cms_prefer_8bit(void)
{
    loader_cms_initialize_target();

    return loader_cms_prefer_8bit_flag;
}

int
loader_cms_target_colorspace(void)
{
    loader_cms_initialize_target();

    return loader_cms_target_colorspace_value;
}

SIXELAPI int
loader_cms_target_pixelformat(void)
{
    loader_cms_initialize_target();

    if (loader_cms_prefer_8bit_flag) {
        return SIXEL_PIXELFORMAT_RGB888;
    }
    switch (loader_cms_target_colorspace_value) {
    case SIXEL_COLORSPACE_GAMMA:
        return SIXEL_PIXELFORMAT_RGBFLOAT32;
    case SIXEL_COLORSPACE_CIELAB:
        return SIXEL_PIXELFORMAT_CIELABFLOAT32;
    case SIXEL_COLORSPACE_OKLAB:
        return SIXEL_PIXELFORMAT_OKLABFLOAT32;
    case SIXEL_COLORSPACE_DIN99D:
        return SIXEL_PIXELFORMAT_DIN99DFLOAT32;
    case SIXEL_COLORSPACE_LINEAR:
    default:
        return SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
    }
}

void
sixel_helper_set_loader_cms_engine(int engine)
{
    if (engine < 0) {
        sixel_cms_set_engine(SIXEL_CMS_ENGINE_AUTO);
        return;
    }

    sixel_cms_set_engine((sixel_cms_engine_t)engine);
}

static unsigned short
loader_exif_read_u16(unsigned char const *data, int little_endian)
{
    if (data == NULL) {
        return 0u;
    }
    if (little_endian) {
        return (unsigned short)((unsigned short)data[0] |
                                (unsigned short)data[1] << 8u);
    }

    return (unsigned short)((unsigned short)data[0] << 8u |
                            (unsigned short)data[1]);
}

static unsigned int
loader_exif_read_u32(unsigned char const *data, int little_endian)
{
    if (data == NULL) {
        return 0u;
    }
    if (little_endian) {
        return (unsigned int)data[0] |
               ((unsigned int)data[1] << 8u) |
               ((unsigned int)data[2] << 16u) |
               ((unsigned int)data[3] << 24u);
    }

    return ((unsigned int)data[0] << 24u) |
           ((unsigned int)data[1] << 16u) |
           ((unsigned int)data[2] << 8u) |
           (unsigned int)data[3];
}

int
loader_exif_parse_orientation(unsigned char const *data,
                              size_t size,
                              int *orientation)
{
    int little_endian;
    unsigned short magic;
    unsigned int ifd_offset;
    unsigned short entry_count;
    size_t entries_offset;
    size_t index;
    unsigned short tag;
    unsigned short type;
    unsigned int count;
    unsigned int value_field;
    int parsed_orientation;

    little_endian = 0;
    magic = 0u;
    ifd_offset = 0u;
    entry_count = 0u;
    entries_offset = 0u;
    index = 0u;
    tag = 0u;
    type = 0u;
    count = 0u;
    value_field = 0u;
    parsed_orientation = 0;

    if (data == NULL || orientation == NULL) {
        return 0;
    }

    if (size >= 6u && memcmp(data, "Exif\0\0", 6u) == 0) {
        data += 6u;
        size -= 6u;
    }

    if (size < 8u) {
        return 0;
    }
    if (data[0] == (unsigned char)'I' && data[1] == (unsigned char)'I') {
        little_endian = 1;
    } else if (data[0] == (unsigned char)'M' &&
               data[1] == (unsigned char)'M') {
        little_endian = 0;
    } else {
        return 0;
    }

    magic = loader_exif_read_u16(data + 2u, little_endian);
    if (magic != 42u) {
        return 0;
    }

    ifd_offset = loader_exif_read_u32(data + 4u, little_endian);
    if ((size_t)ifd_offset > size - 2u) {
        return 0;
    }
    entry_count = loader_exif_read_u16(data + ifd_offset, little_endian);
    entries_offset = (size_t)ifd_offset + 2u;
    if (entry_count > 0u &&
        entries_offset > size - (size_t)entry_count * 12u) {
        return 0;
    }

    for (index = 0u; index < (size_t)entry_count; ++index) {
        unsigned char const *entry;

        entry = data + entries_offset + index * 12u;
        tag = loader_exif_read_u16(entry, little_endian);
        if (tag != 0x0112u) {
            continue;
        }

        type = loader_exif_read_u16(entry + 2u, little_endian);
        count = loader_exif_read_u32(entry + 4u, little_endian);
        value_field = loader_exif_read_u32(entry + 8u, little_endian);
        if (type != 3u || count == 0u) {
            return 0;
        }

        if (count == 1u) {
            if (little_endian) {
                parsed_orientation = (int)(value_field & 0xffffu);
            } else {
                parsed_orientation = (int)((value_field >> 16u) & 0xffffu);
            }
        } else {
            if ((size_t)value_field > size - 2u) {
                return 0;
            }
            parsed_orientation = (int)loader_exif_read_u16(
                data + value_field,
                little_endian);
        }

        if (parsed_orientation < 1 || parsed_orientation > 8) {
            return 0;
        }

        *orientation = parsed_orientation;
        return 1;
    }

    return 0;
}

static void
loader_exif_map_coordinates(int orientation,
                            int src_width,
                            int src_height,
                            int dst_x,
                            int dst_y,
                            int *src_x,
                            int *src_y)
{
    int mapped_x;
    int mapped_y;

    mapped_x = dst_x;
    mapped_y = dst_y;
    switch (orientation) {
    case 2:
        mapped_x = src_width - 1 - dst_x;
        mapped_y = dst_y;
        break;
    case 3:
        mapped_x = src_width - 1 - dst_x;
        mapped_y = src_height - 1 - dst_y;
        break;
    case 4:
        mapped_x = dst_x;
        mapped_y = src_height - 1 - dst_y;
        break;
    case 5:
        mapped_x = dst_y;
        mapped_y = dst_x;
        break;
    case 6:
        mapped_x = dst_y;
        mapped_y = src_height - 1 - dst_x;
        break;
    case 7:
        mapped_x = src_width - 1 - dst_y;
        mapped_y = src_height - 1 - dst_x;
        break;
    case 8:
        mapped_x = src_width - 1 - dst_y;
        mapped_y = dst_x;
        break;
    case 1:
    default:
        mapped_x = dst_x;
        mapped_y = dst_y;
        break;
    }

    if (src_x != NULL) {
        *src_x = mapped_x;
    }
    if (src_y != NULL) {
        *src_y = mapped_y;
    }
}

SIXELSTATUS
loader_frame_apply_orientation(sixel_frame_t *frame,
                               int orientation)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    int src_width;
    int src_height;
    int dst_width;
    int dst_height;
    size_t src_pixel_count;
    size_t dst_pixel_count;
    int depth;
    int channels;
    int x;
    int y;
    int src_x;
    int src_y;
    size_t src_index;
    size_t dst_index;
    unsigned char *src_bytes;
    unsigned char *dst_bytes;
    float *src_floats;
    float *dst_floats;
    unsigned char *src_mask;
    unsigned char *dst_mask;
    size_t pixel_stride;

    status = SIXEL_OK;
    allocator = NULL;
    src_width = 0;
    src_height = 0;
    dst_width = 0;
    dst_height = 0;
    src_pixel_count = 0u;
    dst_pixel_count = 0u;
    depth = 0;
    channels = 0;
    x = 0;
    y = 0;
    src_x = 0;
    src_y = 0;
    src_index = 0u;
    dst_index = 0u;
    src_bytes = NULL;
    dst_bytes = NULL;
    src_floats = NULL;
    dst_floats = NULL;
    src_mask = NULL;
    dst_mask = NULL;
    pixel_stride = 0u;

    if (frame == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (orientation <= 1 || orientation > 8) {
        return SIXEL_OK;
    }
    if (frame->width <= 0 || frame->height <= 0 || frame->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    src_width = frame->width;
    src_height = frame->height;
    if ((size_t)src_width > SIZE_MAX / (size_t)src_height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    src_pixel_count = (size_t)src_width * (size_t)src_height;
    if (orientation >= 5 && orientation <= 8) {
        dst_width = src_height;
        dst_height = src_width;
    } else {
        dst_width = src_width;
        dst_height = src_height;
    }
    if ((size_t)dst_width > SIZE_MAX / (size_t)dst_height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    dst_pixel_count = (size_t)dst_width * (size_t)dst_height;
    allocator = frame->allocator;
    depth = sixel_helper_compute_depth(frame->pixelformat);
    if (depth <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (SIXEL_PIXELFORMAT_IS_FLOAT32(frame->pixelformat)) {
        if (depth <= 0 || depth % (int)sizeof(float) != 0) {
            return SIXEL_BAD_ARGUMENT;
        }
        channels = depth / (int)sizeof(float);
        if (channels <= 0) {
            return SIXEL_BAD_ARGUMENT;
        }
        if (frame->pixels.f32ptr == NULL) {
            return SIXEL_BAD_ARGUMENT;
        }
        if ((size_t)channels > SIZE_MAX / sizeof(float)) {
            return SIXEL_BAD_INTEGER_OVERFLOW;
        }
        pixel_stride = (size_t)depth;
        if (dst_pixel_count > SIZE_MAX / pixel_stride) {
            return SIXEL_BAD_INTEGER_OVERFLOW;
        }

        dst_floats = (float *)sixel_allocator_malloc(allocator,
                                                     dst_pixel_count *
                                                     pixel_stride);
        if (dst_floats == NULL) {
            return SIXEL_BAD_ALLOCATION;
        }
        src_floats = frame->pixels.f32ptr;
        for (y = 0; y < dst_height; ++y) {
            for (x = 0; x < dst_width; ++x) {
                loader_exif_map_coordinates(orientation,
                                            src_width,
                                            src_height,
                                            x,
                                            y,
                                            &src_x,
                                            &src_y);
                src_index = (size_t)src_y * (size_t)src_width + (size_t)src_x;
                dst_index = (size_t)y * (size_t)dst_width + (size_t)x;
                memcpy((unsigned char *)(dst_floats + dst_index * channels),
                       (unsigned char const *)(src_floats +
                                               src_index * channels),
                       pixel_stride);
            }
        }
        frame->pixels.f32ptr = dst_floats;
        sixel_allocator_free(allocator, src_floats);
    } else {
        if (frame->pixelformat == SIXEL_PIXELFORMAT_PAL1 ||
            frame->pixelformat == SIXEL_PIXELFORMAT_PAL2 ||
            frame->pixelformat == SIXEL_PIXELFORMAT_PAL4 ||
            frame->pixelformat == SIXEL_PIXELFORMAT_G1 ||
            frame->pixelformat == SIXEL_PIXELFORMAT_G2 ||
            frame->pixelformat == SIXEL_PIXELFORMAT_G4) {
            return SIXEL_BAD_ARGUMENT;
        }
        if (depth <= 0) {
            return SIXEL_BAD_ARGUMENT;
        }
        if (frame->pixels.u8ptr == NULL) {
            return SIXEL_BAD_ARGUMENT;
        }
        pixel_stride = (size_t)depth;
        if (dst_pixel_count > SIZE_MAX / pixel_stride) {
            return SIXEL_BAD_INTEGER_OVERFLOW;
        }

        dst_bytes = (unsigned char *)sixel_allocator_malloc(allocator,
                                                             dst_pixel_count *
                                                             pixel_stride);
        if (dst_bytes == NULL) {
            return SIXEL_BAD_ALLOCATION;
        }
        src_bytes = frame->pixels.u8ptr;
        for (y = 0; y < dst_height; ++y) {
            for (x = 0; x < dst_width; ++x) {
                loader_exif_map_coordinates(orientation,
                                            src_width,
                                            src_height,
                                            x,
                                            y,
                                            &src_x,
                                            &src_y);
                src_index = (size_t)src_y * (size_t)src_width + (size_t)src_x;
                dst_index = (size_t)y * (size_t)dst_width + (size_t)x;
                memcpy(dst_bytes + dst_index * pixel_stride,
                       src_bytes + src_index * pixel_stride,
                       pixel_stride);
            }
        }
        frame->pixels.u8ptr = dst_bytes;
        sixel_allocator_free(allocator, src_bytes);
    }

    src_mask = frame->transparent_mask;
    dst_mask = NULL;
    if (src_mask != NULL) {
        if (frame->transparent_mask_size != src_pixel_count) {
            sixel_allocator_free(allocator, src_mask);
            frame->transparent_mask = NULL;
            frame->transparent_mask_size = 0u;
        } else {
            dst_mask = (unsigned char *)sixel_allocator_malloc(allocator,
                                                               dst_pixel_count);
            if (dst_mask == NULL) {
                return SIXEL_BAD_ALLOCATION;
            }
            for (y = 0; y < dst_height; ++y) {
                for (x = 0; x < dst_width; ++x) {
                    loader_exif_map_coordinates(orientation,
                                                src_width,
                                                src_height,
                                                x,
                                                y,
                                                &src_x,
                                                &src_y);
                    src_index = (size_t)src_y * (size_t)src_width +
                                (size_t)src_x;
                    dst_index = (size_t)y * (size_t)dst_width + (size_t)x;
                    dst_mask[dst_index] = src_mask[src_index];
                }
            }
            frame->transparent_mask = dst_mask;
            frame->transparent_mask_size = dst_pixel_count;
            sixel_allocator_free(allocator, src_mask);
        }
    }

    frame->width = dst_width;
    frame->height = dst_height;

    return status;
}

void
loader_thumbnailer_initialize_size_hint(void)
{
    char const *env_value;
    char *endptr;
    long parsed;

    if (thumbnailer_size_hint_initialized) {
        return;
    }

    thumbnailer_size_hint_initialized = 1;
    thumbnailer_default_size_hint = SIXEL_THUMBNAILER_DEFAULT_SIZE;
    thumbnailer_size_hint = thumbnailer_default_size_hint;

    env_value = sixel_compat_getenv("SIXEL_THUMBNAILER_HINT_SIZE");
    if (env_value == NULL || env_value[0] == '\0') {
        return;
    }

    errno = 0;
    parsed = strtol(env_value, &endptr, 10);
    if (errno != 0) {
        return;
    }
    if (endptr == env_value || *endptr != '\0') {
        return;
    }
    if (parsed <= 0) {
        return;
    }
    if (parsed > (long)INT_MAX) {
        parsed = (long)INT_MAX;
    }

    thumbnailer_default_size_hint = (int)parsed;
    thumbnailer_size_hint = thumbnailer_default_size_hint;
}

int
loader_thumbnailer_get_size_hint(void)
{
    loader_thumbnailer_initialize_size_hint();

    return thumbnailer_size_hint;
}

int
loader_thumbnailer_get_default_size_hint(void)
{
    loader_thumbnailer_initialize_size_hint();

    return thumbnailer_default_size_hint;
}

void
sixel_helper_set_loader_trace(int enable)
{
    loader_trace_enabled = enable ? 1 : 0;
}

void
sixel_helper_set_thumbnail_size_hint(int size)
{
    loader_thumbnailer_initialize_size_hint();

    if (size > 0) {
        thumbnailer_size_hint = size;
    } else {
        thumbnailer_size_hint = thumbnailer_default_size_hint;
    }
}

void
loader_trace_message(char const *format, ...)
{
    va_list args;

    if (!loader_trace_enabled) {
        return;
    }

    fprintf(stderr, "libsixel: ");

    va_start(args, format);
    sixel_compat_vfprintf(stderr, format, args);
    va_end(args);

    fprintf(stderr, "\n");
}


/*
 * Return non-zero when SIXEL_TRACE_TOPIC contains the given token.
 * Supported separators are comma, colon, semicolon, and whitespace.
 */
int
sixel_trace_topic_is_enabled(char const *topic)
{
    char const *topics;
    char const *cursor;
    char const *token_end;
    size_t topic_length;
    size_t token_length;

    topics = NULL;
    cursor = NULL;
    token_end = NULL;
    topic_length = 0u;
    token_length = 0u;

    if (topic == NULL || topic[0] == '\0') {
        return 0;
    }

    topic_length = strlen(topic);
    if (topic_length == 0u) {
        return 0;
    }

    topics = sixel_compat_getenv("SIXEL_TRACE_TOPIC");
    if (topics == NULL || topics[0] == '\0') {
        return 0;
    }

    cursor = topics;
    while (*cursor != '\0') {
        while (*cursor != '\0' &&
               (*cursor == ' ' || *cursor == '\t' || *cursor == ',' ||
                *cursor == ':' || *cursor == ';')) {
            ++cursor;
        }
        if (*cursor == '\0') {
            break;
        }

        token_end = cursor;
        while (*token_end != '\0' &&
               *token_end != ' ' && *token_end != '\t' &&
               *token_end != ',' && *token_end != ':' &&
               *token_end != ';') {
            ++token_end;
        }

        token_length = (size_t)(token_end - cursor);
        if (token_length == topic_length &&
                strncmp(cursor, topic, token_length) == 0) {
            return 1;
        }

        cursor = token_end;
    }

    return 0;
}

/* Emit topic-scoped diagnostics selected through SIXEL_TRACE_TOPIC. */
void
sixel_trace_topic_message(
    char const *topic,
    char const *format,
    ...)
{
    va_list args;

    if (!sixel_trace_topic_is_enabled(topic)) {
        return;
    }

    fprintf(stderr,
            "libsixel[%s]: ",
            topic != NULL && topic[0] != '\0' ? topic : "trace");

    va_start(args, format);
    sixel_compat_vfprintf(stderr, format, args);
    va_end(args);

    fprintf(stderr, "\n");
}

void
loader_trace_try(char const *name)
{
    if (loader_trace_enabled) {
        fprintf(stderr, "libsixel: trying %s loader\n", name);
    }
}

void
loader_trace_result(char const *name, SIXELSTATUS status)
{
    if (!loader_trace_enabled) {
        return;
    }
    if (SIXEL_SUCCEEDED(status)) {
        fprintf(stderr, "libsixel: loader %s succeeded\n", name);
    } else {
        fprintf(stderr, "libsixel: loader %s failed (%s)\n",
                name, sixel_helper_format_error(status));
    }
}

int
loader_trace_is_enabled(void)
{
    return loader_trace_enabled;
}

int
chunk_is_png(sixel_chunk_t const *chunk)
{
    if (chunk == NULL || chunk->size < 8) {
        return 0;
    }

    /*
     * PNG streams begin with an 8-byte signature.  Checking the fixed magic
     * sequence keeps the detection fast and avoids depending on libpng
     * helpers when only the signature is needed.
     */
    if (chunk->buffer[0] == (unsigned char)0x89 &&
        chunk->buffer[1] == 'P' &&
        chunk->buffer[2] == 'N' &&
        chunk->buffer[3] == 'G' &&
        chunk->buffer[4] == (unsigned char)0x0d &&
        chunk->buffer[5] == (unsigned char)0x0a &&
        chunk->buffer[6] == (unsigned char)0x1a &&
        chunk->buffer[7] == (unsigned char)0x0a) {
        return 1;
    }

    return 0;
}

int
chunk_is_jpeg(sixel_chunk_t const *chunk)
{
    if (chunk == NULL || chunk->size < 2) {
        return 0;
    }

    /*
     * JPEG files start with SOI (Start of Image) marker 0xFF 0xD8.  The GD
     * loader uses this to decide whether libgd should attempt JPEG decoding.
     */
    if (chunk->buffer[0] == (unsigned char)0xff &&
        chunk->buffer[1] == (unsigned char)0xd8) {
        return 1;
    }

    return 0;
}

int
chunk_is_webp(sixel_chunk_t const *chunk)
{
    if (chunk == NULL || chunk->size < 12) {
        return 0;
    }

    /*
     * WebP files use a RIFF container.  The stream starts with \"RIFF\",
     * followed by a 32-bit size field, and then the literal \"WEBP\" tag.
     */
    if (chunk->buffer[0] == 'R' &&
        chunk->buffer[1] == 'I' &&
        chunk->buffer[2] == 'F' &&
        chunk->buffer[3] == 'F' &&
        chunk->buffer[8] == 'W' &&
        chunk->buffer[9] == 'E' &&
        chunk->buffer[10] == 'B' &&
        chunk->buffer[11] == 'P') {
        return 1;
    }

    return 0;
}

int
chunk_is_bmp(sixel_chunk_t const *chunk)
{
    if (chunk == NULL || chunk->size < 2) {
        return 0;
    }

    /* BMP headers begin with the literal characters 'B' 'M'. */
    if (chunk->buffer[0] == 'B' && chunk->buffer[1] == 'M') {
        return 1;
    }

    return 0;
}

int
chunk_is_tiff(sixel_chunk_t const *chunk)
{
    if (chunk == NULL || chunk->size < 4) {
        return 0;
    }

    /*
     * TIFF headers begin with either "II*\0", "MM\0*", or the BigTIFF
     * variants "II+\0"/"MM\0+". Checking the first four bytes is enough to
     * decide whether the stream can be probed by libtiff.
     */
    if ((chunk->buffer[0] == 'I' && chunk->buffer[1] == 'I' &&
         chunk->buffer[2] == (unsigned char)0x2a &&
         chunk->buffer[3] == (unsigned char)0x00) ||
        (chunk->buffer[0] == 'M' && chunk->buffer[1] == 'M' &&
         chunk->buffer[2] == (unsigned char)0x00 &&
         chunk->buffer[3] == (unsigned char)0x2a) ||
        (chunk->buffer[0] == 'I' && chunk->buffer[1] == 'I' &&
         chunk->buffer[2] == (unsigned char)0x2b &&
         chunk->buffer[3] == (unsigned char)0x00) ||
        (chunk->buffer[0] == 'M' && chunk->buffer[1] == 'M' &&
         chunk->buffer[2] == (unsigned char)0x00 &&
         chunk->buffer[3] == (unsigned char)0x2b)) {
        return 1;
    }

    return 0;
}

int
chunk_is_gif(sixel_chunk_t const *chunk)
{
    if (chunk->size < 6) {
        return 0;
    }
    if (chunk->buffer[0] == 'G' &&
        chunk->buffer[1] == 'I' &&
        chunk->buffer[2] == 'F' &&
        chunk->buffer[3] == '8' &&
        (chunk->buffer[4] == '7' || chunk->buffer[4] == '9') &&
        chunk->buffer[5] == 'a') {
        return 1;
    }
    return 0;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
