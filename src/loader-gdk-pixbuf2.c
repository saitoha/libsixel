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
 * gdk-pixbuf2-backed loader extracted from loader.c.  Keeping the
 * implementation separate prevents unrelated backends from pulling gdk
 * headers and keeps the dispatcher lean.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#ifdef HAVE_GDK_PIXBUF2

#if !defined(_POSIX_C_SOURCE)
# define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>

#if HAVE_STDLIB_H
# include <stdlib.h>
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
#if HAVE_MATH_H
# include <math.h>
#endif
#if HAVE_STRING_H
# include <string.h>
#endif

/* Keep SIZE_MAX available even on strict C99 environments. */
#ifndef SIZE_MAX
# define SIZE_MAX ((size_t)-1)
#endif

#ifdef HAVE_GDK_PIXBUF2
# if !defined(GLIB_DISABLE_DEPRECATION_WARNINGS)
#  define GLIB_DISABLE_DEPRECATION_WARNINGS
#  define SIXEL_GLIB_DISABLE_DEPRECATION_WARNINGS
# endif
# if HAVE_DIAGNOSTIC_TYPEDEF_REDEFINITION && defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wtypedef-redefinition"
# endif
# include <gdk-pixbuf/gdk-pixbuf.h>
# include <gdk-pixbuf/gdk-pixbuf-simple-anim.h>
# if HAVE_DIAGNOSTIC_TYPEDEF_REDEFINITION && defined(__clang__)
#  pragma clang diagnostic pop
# endif
# if defined(SIXEL_GLIB_DISABLE_DEPRECATION_WARNINGS)
#  undef GLIB_DISABLE_DEPRECATION_WARNINGS
#  undef SIXEL_GLIB_DISABLE_DEPRECATION_WARNINGS
# endif
#endif

#include <sixel.h>

#include "allocator.h"
#include "compat_stub.h"
#include "frame.h"
#include "loader-common.h"
#include "loader-gdk-pixbuf2.h"
#include "probe.h"

typedef struct sixel_loader_gdkpixbuf_component {
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
    int srgb_decode_lut_prepared;
    double srgb_decode_lut[256];
} sixel_loader_gdkpixbuf_component_t;

typedef struct gdkpixbuf_png_decode_hint {
    SIXELSTATUS parse_status;
    int is_png;
    int bit_depth;
} gdkpixbuf_png_decode_hint_t;

static void
gdkpixbuf_set_error_message(char const *context, GError const *gerror)
{
    enum { message_capacity = 512 };
    char message[message_capacity];
    int written;

    written = 0;
    if (context == NULL) {
        return;
    }
    if (gerror == NULL ||
        gerror->message == NULL ||
        gerror->message[0] == '\0') {
        sixel_helper_set_additional_message(context);
        return;
    }

    written = sixel_compat_snprintf(message,
                                    sizeof(message),
                                    "%s (%s)",
                                    context,
                                    gerror->message);
    if (written < 0) {
        sixel_helper_set_additional_message(context);
        return;
    }
    message[message_capacity - 1] = '\0';
    sixel_helper_set_additional_message(message);
}

static void
gdkpixbuf_clear_error(GError **pgerror)
{
    if (pgerror == NULL || *pgerror == NULL) {
        return;
    }
    g_error_free(*pgerror);
    *pgerror = NULL;
}

static GdkPixbufAnimation *
gdkpixbuf_loader_get_animation_safe(GdkPixbufLoader *loader)
{
    GdkPixbufAnimation *animation;

    animation = NULL;
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    animation = gdk_pixbuf_loader_get_animation(loader);
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
# pragma GCC diagnostic pop
#endif
    return animation;
}

static gboolean
gdkpixbuf_animation_is_static_image_safe(GdkPixbufAnimation *animation)
{
    gboolean is_static;

    is_static = FALSE;
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    is_static = gdk_pixbuf_animation_is_static_image(animation);
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
# pragma GCC diagnostic pop
#endif
    return is_static;
}

static GdkPixbuf *
gdkpixbuf_loader_get_static_pixbuf(GdkPixbufLoader *loader,
                                   GdkPixbufAnimation *animation)
{
    GdkPixbuf *pixbuf;

    pixbuf = NULL;
#if HAVE_GDK_PIXBUF_244
    (void)animation;
    pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
#else
    (void)loader;
    pixbuf = gdk_pixbuf_animation_get_static_image(animation);
#endif
    return pixbuf;
}

static SIXELSTATUS
gdkpixbuf_decode_chunk_with_fallback(sixel_chunk_t const *pchunk,
                                     int is_sixel,
                                     GdkPixbufLoader **ploader)
{
    SIXELSTATUS status;
    GdkPixbufLoader *loader;
    GError *loader_error;
    int use_sixel_loader;

    status = SIXEL_FALSE;
    loader = NULL;
    loader_error = NULL;
    use_sixel_loader = 0;
    if (pchunk == NULL || ploader == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *ploader = NULL;

retry:
    if (use_sixel_loader != 0) {
        loader = gdk_pixbuf_loader_new_with_type("sixel", &loader_error);
        if (loader == NULL) {
            gdkpixbuf_set_error_message(
                "load_with_gdkpixbuf: sixel fallback loader creation failed.",
                loader_error);
            gdkpixbuf_clear_error(&loader_error);
            status = SIXEL_GDK_ERROR;
            goto end;
        }
    } else {
        loader = gdk_pixbuf_loader_new();
        if (loader == NULL) {
            gdkpixbuf_set_error_message(
                "load_with_gdkpixbuf: gdk_pixbuf_loader_new failed.",
                loader_error);
            gdkpixbuf_clear_error(&loader_error);
            status = SIXEL_GDK_ERROR;
            goto end;
        }
    }

    if (!gdk_pixbuf_loader_write(loader,
                                 pchunk->buffer,
                                 pchunk->size,
                                 &loader_error)) {
        if (use_sixel_loader != 0 || !is_sixel) {
            if (use_sixel_loader != 0) {
                gdkpixbuf_set_error_message(
                    "load_with_gdkpixbuf: sixel fallback loader write failed.",
                    loader_error);
            } else {
                gdkpixbuf_set_error_message(
                    "load_with_gdkpixbuf: generic loader write failed.",
                    loader_error);
            }
            gdkpixbuf_clear_error(&loader_error);
            status = SIXEL_GDK_ERROR;
            goto end;
        }
        gdkpixbuf_set_error_message(
            "load_with_gdkpixbuf: generic loader write failed "
            "before sixel fallback.",
            loader_error);
        gdkpixbuf_clear_error(&loader_error);
        g_object_unref(loader);
        loader = NULL;
        use_sixel_loader = 1;
        goto retry;
    }

    if (!gdk_pixbuf_loader_close(loader, &loader_error)) {
        if (use_sixel_loader != 0 || !is_sixel) {
            if (use_sixel_loader != 0) {
                gdkpixbuf_set_error_message(
                    "load_with_gdkpixbuf: sixel fallback loader close failed.",
                    loader_error);
            } else {
                gdkpixbuf_set_error_message(
                    "load_with_gdkpixbuf: generic loader close failed.",
                    loader_error);
            }
            gdkpixbuf_clear_error(&loader_error);
            status = SIXEL_GDK_ERROR;
            goto end;
        }
        gdkpixbuf_set_error_message(
            "load_with_gdkpixbuf: generic loader close failed "
            "before sixel fallback.",
            loader_error);
        gdkpixbuf_clear_error(&loader_error);
        g_object_unref(loader);
        loader = NULL;
        use_sixel_loader = 1;
        goto retry;
    }

    *ploader = loader;
    loader = NULL;
    status = SIXEL_OK;

end:
    if (loader != NULL) {
        gdk_pixbuf_loader_close(loader, NULL);
        g_object_unref(loader);
    }
    gdkpixbuf_clear_error(&loader_error);
    return status;
}

static SIXELSTATUS
gdkpixbuf_parse_animation_start_frame_no(int *start_frame_no)
{
    SIXELSTATUS status;
    char const *env_value;
    char *endptr;
    long parsed;

    status = SIXEL_OK;
    env_value = NULL;
    endptr = NULL;
    parsed = 0;

    *start_frame_no = INT_MIN;
    env_value = sixel_compat_getenv("SIXEL_LOADER_ANIMATION_START_FRAME_NO");
    if (env_value == NULL || env_value[0] == '\0') {
        goto end;
    }

    errno = 0;
    parsed = strtol(env_value, &endptr, 10);
    if (endptr == env_value || *endptr != '\0') {
        sixel_helper_set_additional_message(
            "SIXEL_LOADER_ANIMATION_START_FRAME_NO must be an integer.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (errno == ERANGE ||
        parsed < (long)INT_MIN ||
        parsed > (long)INT_MAX) {
        sixel_helper_set_additional_message(
            "SIXEL_LOADER_ANIMATION_START_FRAME_NO is out of range.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (errno != 0) {
        sixel_helper_set_additional_message(
            "SIXEL_LOADER_ANIMATION_START_FRAME_NO must be an integer.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    *start_frame_no = (int)parsed;

end:
    return status;
}

static SIXELSTATUS
gdkpixbuf_resolve_animation_start_frame_no(int start_frame_no,
                                           int frame_count,
                                           int *resolved)
{
    SIXELSTATUS status;
    int index;

    status = SIXEL_OK;
    index = 0;

    if (frame_count <= 0) {
        sixel_helper_set_additional_message(
            "Animation frame count must be positive.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    if (start_frame_no >= 0) {
        index = start_frame_no;
    } else {
        index = frame_count + start_frame_no;
    }

    if (index < 0 || index >= frame_count) {
        sixel_helper_set_additional_message(
            "SIXEL_LOADER_ANIMATION_START_FRAME_NO is outside"
            " the animation frame range.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    *resolved = index;

end:
    return status;
}

static GdkPixbufAnimationIter *
gdkpixbuf_animation_get_iter_with_usec(
    GdkPixbufAnimation *animation,
    gint64 current_time_usec)
{
    GdkPixbufAnimationIter *it;
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    GTimeVal time_val;

    time_val.tv_sec = current_time_usec / 1000000;
    time_val.tv_usec = current_time_usec % 1000000;
    it = gdk_pixbuf_animation_get_iter(animation, &time_val);
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
# pragma GCC diagnostic pop
#endif
    return it;
}

static gboolean
gdkpixbuf_animation_iter_advance_with_usec(
    GdkPixbufAnimationIter *it,
    gint64 current_time_usec)
{
    gboolean advanced;
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    GTimeVal time_val;

    time_val.tv_sec = current_time_usec / 1000000;
    time_val.tv_usec = current_time_usec % 1000000;
    advanced = gdk_pixbuf_animation_iter_advance(it, &time_val);
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
# pragma GCC diagnostic pop
#endif
    return advanced;
}

static gboolean
gdkpixbuf_animation_iter_on_loading_frame(
    GdkPixbufAnimationIter *it)
{
    gboolean on_loading_frame;

#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    on_loading_frame =
        gdk_pixbuf_animation_iter_on_currently_loading_frame(it);
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
# pragma GCC diagnostic pop
#endif
    return on_loading_frame;
}

static GdkPixbuf *
gdkpixbuf_animation_iter_get_current_pixbuf(
    GdkPixbufAnimationIter *it)
{
    GdkPixbuf *pixbuf;

#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    pixbuf = gdk_pixbuf_animation_iter_get_pixbuf(it);
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
# pragma GCC diagnostic pop
#endif
    return pixbuf;
}

static int
gdkpixbuf_animation_iter_get_current_delay(
    GdkPixbufAnimationIter *it)
{
    int delay_ms;

#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    delay_ms = gdk_pixbuf_animation_iter_get_delay_time(it);
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
# pragma GCC diagnostic pop
#endif
    return delay_ms;
}

static SIXELSTATUS
gdkpixbuf_count_animation_frames(GdkPixbufAnimation *animation,
                                 gint64 start_time_usec,
                                 int *frame_count)
{
    SIXELSTATUS status;
    GdkPixbufAnimationIter *it;
    GdkPixbuf *pixbuf;
    gboolean finished;
    gint64 current_time_usec;
    int count;
    int delay_ms;

    status = SIXEL_OK;
    it = NULL;
    pixbuf = NULL;
    finished = FALSE;
    current_time_usec = start_time_usec;
    count = 0;
    delay_ms = 0;

    it = gdkpixbuf_animation_get_iter_with_usec(animation, current_time_usec);
    if (it == NULL) {
        status = SIXEL_GDK_ERROR;
        goto end;
    }

    /*
     * gdk-pixbuf does not expose random frame seek for animations.
     * Count frames with iterator stepping so start-frame indexes can be
     * validated and negative offsets can be resolved before decoding.
     */
    while (!gdkpixbuf_animation_iter_on_loading_frame(it)) {
        pixbuf = gdkpixbuf_animation_iter_get_current_pixbuf(it);
        if (pixbuf == NULL) {
            finished = TRUE;
            break;
        }
        ++count;
        delay_ms = gdkpixbuf_animation_iter_get_current_delay(it);
        if (delay_ms < 0) {
            delay_ms = 0;
        }
        current_time_usec += (gint64)delay_ms * 1000;
        if (!gdkpixbuf_animation_iter_advance_with_usec(it,
                                                        current_time_usec)) {
            finished = TRUE;
        }
        if (finished) {
            break;
        }
    }

    if (count <= 0) {
        sixel_helper_set_additional_message(
            "gdk-pixbuf animation does not contain decodable frames.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    *frame_count = count;

end:
    if (it != NULL) {
        g_object_unref(it);
    }
    return status;
}

static double
gdkpixbuf_clamp_unit(double value)
{
    if (value < 0.0) {
        return 0.0;
    }
    if (value > 1.0) {
        return 1.0;
    }
    return value;
}

static double
gdkpixbuf_decode_srgb_unit(double value)
{
    value = gdkpixbuf_clamp_unit(value);
    if (value <= 0.04045) {
        return value / 12.92;
    }
#if HAVE_MATH_H
    return pow((value + 0.055) / 1.055, 2.4);
#else
    return value;
#endif
}

static void
gdkpixbuf_build_srgb_decode_u8_lut(double lut[256])
{
    int index;
    double unit;

    index = 0;
    unit = 0.0;
    if (lut == NULL) {
        return;
    }

    for (index = 0; index < 256; ++index) {
        unit = (double)index / 255.0;
        lut[index] = gdkpixbuf_decode_srgb_unit(unit);
    }
}

static void
gdkpixbuf_png_decode_hint_init(gdkpixbuf_png_decode_hint_t *hint)
{
    if (hint == NULL) {
        return;
    }

    hint->parse_status = SIXEL_FALSE;
    hint->is_png = 0;
    hint->bit_depth = 0;
}

static void
gdkpixbuf_png_decode_hint_reset(gdkpixbuf_png_decode_hint_t *hint,
                                sixel_allocator_t *allocator)
{
    if (hint == NULL) {
        return;
    }

    (void)allocator;
    gdkpixbuf_png_decode_hint_init(hint);
}

static unsigned int
gdkpixbuf_read_u32be(unsigned char const *bytes)
{
    unsigned int value;

    value = 0u;
    if (bytes == NULL) {
        return 0u;
    }

    value = (unsigned int)bytes[0] << 24;
    value |= (unsigned int)bytes[1] << 16;
    value |= (unsigned int)bytes[2] << 8;
    value |= (unsigned int)bytes[3];
    return value;
}

static SIXELSTATUS
gdkpixbuf_parse_png_decode_hint(sixel_chunk_t const *pchunk,
                                gdkpixbuf_png_decode_hint_t *hint)
{
    static unsigned char const png_signature[8] = {
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au
    };
    SIXELSTATUS status;
    size_t offset;
    size_t chunk_length;
    unsigned char const *type_ptr;
    unsigned char const *data_ptr;

    status = SIXEL_FALSE;
    offset = 0u;
    chunk_length = 0u;
    type_ptr = NULL;
    data_ptr = NULL;
    if (pchunk == NULL || hint == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    gdkpixbuf_png_decode_hint_init(hint);
    if (pchunk->buffer == NULL || pchunk->size < sizeof(png_signature)) {
        return SIXEL_FALSE;
    }
    if (memcmp(pchunk->buffer, png_signature, sizeof(png_signature)) != 0) {
        return SIXEL_FALSE;
    }

    hint->is_png = 1;
    offset = sizeof(png_signature);
    while (offset + 8u <= pchunk->size) {
        chunk_length = (size_t)gdkpixbuf_read_u32be(pchunk->buffer + offset);
        offset += 4u;
        if (offset + 4u + chunk_length + 4u > pchunk->size) {
            status = SIXEL_FALSE;
            hint->parse_status = status;
            return status;
        }

        type_ptr = pchunk->buffer + offset;
        offset += 4u;
        data_ptr = pchunk->buffer + offset;

        if (memcmp(type_ptr, "IHDR", 4u) == 0) {
            if (chunk_length < 13u) {
                status = SIXEL_FALSE;
                hint->parse_status = status;
                return status;
            }
            hint->bit_depth = (int)data_ptr[8];
            status = SIXEL_OK;
            hint->parse_status = status;
            return status;
        }

        offset += chunk_length + 4u;
    }

    status = SIXEL_FALSE;
    hint->parse_status = status;
    return status;
}

static void
gdkpixbuf_reset_frame_storage(sixel_frame_t *frame,
                              sixel_allocator_t *allocator)
{
    if (frame == NULL || allocator == NULL) {
        return;
    }

    sixel_allocator_free(allocator, frame->pixels.u8ptr);
    frame->pixels.u8ptr = NULL;
    sixel_allocator_free(allocator, frame->palette);
    frame->palette = NULL;
    sixel_allocator_free(allocator, frame->transparent_mask);
    frame->transparent_mask = NULL;
    frame->transparent_mask_size = 0u;
    frame->ncolors = -1;
    frame->transparent = -1;
    frame->alpha_zero_is_transparent = 0;
}

static SIXELSTATUS
gdkpixbuf_copy_pixbuf_to_frame(
    sixel_frame_t *frame,
    sixel_chunk_t const *pchunk,
    GdkPixbuf *pixbuf,
    unsigned char const *bgcolor,
    gdkpixbuf_png_decode_hint_t const *png_hint,
    int *lut_ready,
    double lut[256])
{
    SIXELSTATUS status;
    unsigned char *source_pixels;
    unsigned char *pixels_u8;
    float *pixels_f32;
    unsigned char *mask;
    size_t pixel_total;
    size_t buffer_size;
    size_t x;
    size_t y;
    size_t index;
    size_t source_offset;
    size_t dest_offset;
    int width;
    int height;
    int rowstride;
    int source_depth;
    int has_alpha;
    int has_effective_alpha;
    int alpha_all_opaque;
    int has_bg_for_alpha;
    int promote_float32;
    double const *decode_lut;
    unsigned int alpha_u8;
    double alpha_unit;
    double src_linear[3];
    double out_linear[3];
    double bg_linear[3];

    status = SIXEL_OK;
    source_pixels = NULL;
    pixels_u8 = NULL;
    pixels_f32 = NULL;
    mask = NULL;
    pixel_total = 0u;
    buffer_size = 0u;
    x = 0u;
    y = 0u;
    index = 0u;
    source_offset = 0u;
    dest_offset = 0u;
    width = 0;
    height = 0;
    rowstride = 0;
    source_depth = 0;
    has_alpha = 0;
    has_effective_alpha = 0;
    alpha_all_opaque = 0;
    has_bg_for_alpha = 0;
    promote_float32 = 0;
    decode_lut = NULL;
    alpha_u8 = 0u;
    alpha_unit = 0.0;
    src_linear[0] = 0.0;
    src_linear[1] = 0.0;
    src_linear[2] = 0.0;
    out_linear[0] = 0.0;
    out_linear[1] = 0.0;
    out_linear[2] = 0.0;
    bg_linear[0] = 0.0;
    bg_linear[1] = 0.0;
    bg_linear[2] = 0.0;
    if (frame == NULL || pchunk == NULL || pixbuf == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    width = gdk_pixbuf_get_width(pixbuf);
    height = gdk_pixbuf_get_height(pixbuf);
    if (width <= 0 || height <= 0) {
        sixel_helper_set_additional_message(
            "load_with_gdkpixbuf: invalid pixbuf dimensions.");
        return SIXEL_BAD_INPUT;
    }

    gdkpixbuf_reset_frame_storage(frame, pchunk->allocator);
    frame->width = width;
    frame->height = height;
    frame->transparent = -1;
    frame->alpha_zero_is_transparent = 0;
    frame->ncolors = -1;

    has_alpha = gdk_pixbuf_get_has_alpha(pixbuf) ? 1 : 0;
    source_depth = has_alpha ? 4 : 3;
    rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    source_pixels = gdk_pixbuf_get_pixels(pixbuf);
    if (source_pixels == NULL) {
        sixel_helper_set_additional_message(
            "load_with_gdkpixbuf: gdk_pixbuf_get_pixels() failed.");
        return SIXEL_GDK_ERROR;
    }

    if ((size_t)width > SIZE_MAX / (size_t)height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_total = (size_t)width * (size_t)height;

    has_effective_alpha = has_alpha;
    if (has_alpha != 0) {
        alpha_all_opaque = 1;
        for (y = 0u; y < (size_t)height && alpha_all_opaque != 0; ++y) {
            for (x = 0u; x < (size_t)width; ++x) {
                source_offset = (size_t)rowstride * y +
                    x * (size_t)source_depth;
                if (source_pixels[source_offset + 3u] != 255u) {
                    alpha_all_opaque = 0;
                    break;
                }
            }
        }
        if (alpha_all_opaque != 0) {
            has_effective_alpha = 0;
        }
    }

    has_bg_for_alpha = bgcolor != NULL && has_effective_alpha;
    promote_float32 = png_hint != NULL &&
        png_hint->parse_status == SIXEL_OK &&
        png_hint->is_png != 0 &&
        png_hint->bit_depth > 8;
    if (has_bg_for_alpha) {
        promote_float32 = 1;
    }
    if (promote_float32 != 0 &&
        lut_ready != NULL &&
        lut != NULL) {
        if (*lut_ready == 0) {
            gdkpixbuf_build_srgb_decode_u8_lut(lut);
            *lut_ready = 1;
        }
        decode_lut = lut;
    }

    /*
     * Use linear float output for high-depth PNGs and for background composited
     * alpha cases to avoid precision loss.
     */
    if (promote_float32 != 0) {
        if (pixel_total > SIZE_MAX / (3u * sizeof(float))) {
            status = SIXEL_BAD_INTEGER_OVERFLOW;
            goto cleanup;
        }
        pixels_f32 = (float *)sixel_allocator_malloc(
            pchunk->allocator,
            pixel_total * 3u * sizeof(float));
        if (pixels_f32 == NULL) {
            sixel_helper_set_additional_message(
                "load_with_gdkpixbuf: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto cleanup;
        }

        if (has_effective_alpha && bgcolor == NULL) {
            mask = (unsigned char *)sixel_allocator_malloc(pchunk->allocator,
                                                           pixel_total);
            if (mask == NULL) {
                sixel_helper_set_additional_message(
                    "load_with_gdkpixbuf: sixel_allocator_malloc() failed.");
                status = SIXEL_BAD_ALLOCATION;
                goto cleanup;
            }
        } else if (has_bg_for_alpha) {
            if (decode_lut != NULL) {
                bg_linear[0] = decode_lut[bgcolor[0]];
                bg_linear[1] = decode_lut[bgcolor[1]];
                bg_linear[2] = decode_lut[bgcolor[2]];
            } else {
                bg_linear[0] = gdkpixbuf_decode_srgb_unit(
                    (double)bgcolor[0] / 255.0);
                bg_linear[1] = gdkpixbuf_decode_srgb_unit(
                    (double)bgcolor[1] / 255.0);
                bg_linear[2] = gdkpixbuf_decode_srgb_unit(
                    (double)bgcolor[2] / 255.0);
            }
        }

        for (y = 0u; y < (size_t)height; ++y) {
            for (x = 0u; x < (size_t)width; ++x) {
                source_offset = (size_t)rowstride * y +
                    x * (size_t)source_depth;
                index = y * (size_t)width + x;
                alpha_u8 = has_effective_alpha
                    ? source_pixels[source_offset + 3u]
                    : 255u;
                alpha_unit = (double)alpha_u8 / 255.0;

                if (decode_lut != NULL) {
                    src_linear[0] = decode_lut[
                        source_pixels[source_offset + 0u]];
                    src_linear[1] = decode_lut[
                        source_pixels[source_offset + 1u]];
                    src_linear[2] = decode_lut[
                        source_pixels[source_offset + 2u]];
                } else {
                    src_linear[0] = gdkpixbuf_decode_srgb_unit(
                        (double)source_pixels[source_offset + 0u] / 255.0);
                    src_linear[1] = gdkpixbuf_decode_srgb_unit(
                        (double)source_pixels[source_offset + 1u] / 255.0);
                    src_linear[2] = gdkpixbuf_decode_srgb_unit(
                        (double)source_pixels[source_offset + 2u] / 255.0);
                }

                if (has_bg_for_alpha) {
                    out_linear[0] = src_linear[0] * alpha_unit +
                        bg_linear[0] * (1.0 - alpha_unit);
                    out_linear[1] = src_linear[1] * alpha_unit +
                        bg_linear[1] * (1.0 - alpha_unit);
                    out_linear[2] = src_linear[2] * alpha_unit +
                        bg_linear[2] * (1.0 - alpha_unit);
                } else if (has_effective_alpha) {
                    out_linear[0] = src_linear[0] * alpha_unit;
                    out_linear[1] = src_linear[1] * alpha_unit;
                    out_linear[2] = src_linear[2] * alpha_unit;
                    if (mask != NULL) {
                        mask[index] = alpha_u8 == 0u ? 1u : 0u;
                    }
                } else {
                    out_linear[0] = src_linear[0];
                    out_linear[1] = src_linear[1];
                    out_linear[2] = src_linear[2];
                }

                pixels_f32[index * 3u + 0u] = (float)out_linear[0];
                pixels_f32[index * 3u + 1u] = (float)out_linear[1];
                pixels_f32[index * 3u + 2u] = (float)out_linear[2];
            }
        }

        frame->pixels.f32ptr = pixels_f32;
        pixels_f32 = NULL;
        frame->pixelformat = SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
        frame->colorspace = SIXEL_COLORSPACE_LINEAR;
        frame->transparent = -1;
        frame->alpha_zero_is_transparent = 0;
        if (mask != NULL) {
            frame->transparent_mask = mask;
            frame->transparent_mask_size = pixel_total;
            frame->alpha_zero_is_transparent = 1;
            mask = NULL;
        }
        status = SIXEL_OK;
        goto cleanup;
    }

    /*
     * Default alpha handling emits RGB plus a binary transparent mask.
     * This mirrors other loaders and keeps the 8-bit fast path.
     */
    if (has_effective_alpha) {
        if (pixel_total > SIZE_MAX / 3u) {
            status = SIXEL_BAD_INTEGER_OVERFLOW;
            goto cleanup;
        }
        pixels_u8 = (unsigned char *)sixel_allocator_malloc(
            pchunk->allocator,
            pixel_total * 3u);
        if (pixels_u8 == NULL) {
            sixel_helper_set_additional_message(
                "load_with_gdkpixbuf: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto cleanup;
        }
        mask = (unsigned char *)sixel_allocator_malloc(pchunk->allocator,
                                                       pixel_total);
        if (mask == NULL) {
            sixel_helper_set_additional_message(
                "load_with_gdkpixbuf: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto cleanup;
        }
        for (y = 0u; y < (size_t)height; ++y) {
            for (x = 0u; x < (size_t)width; ++x) {
                source_offset = (size_t)rowstride * y +
                    x * (size_t)source_depth;
                dest_offset = (y * (size_t)width + x) * 3u;
                index = y * (size_t)width + x;
                alpha_u8 = source_pixels[source_offset + 3u];

                pixels_u8[dest_offset + 0u] = (unsigned char)(
                    ((unsigned int)source_pixels[source_offset + 0u] *
                     alpha_u8 + 127u) / 255u);
                pixels_u8[dest_offset + 1u] = (unsigned char)(
                    ((unsigned int)source_pixels[source_offset + 1u] *
                     alpha_u8 + 127u) / 255u);
                pixels_u8[dest_offset + 2u] = (unsigned char)(
                    ((unsigned int)source_pixels[source_offset + 2u] *
                     alpha_u8 + 127u) / 255u);

                if (alpha_u8 == 0u) {
                    mask[index] = 1u;
                } else {
                    mask[index] = 0u;
                }
            }
        }

        frame->pixels.u8ptr = pixels_u8;
        pixels_u8 = NULL;
        frame->pixelformat = SIXEL_PIXELFORMAT_RGB888;
        frame->colorspace = SIXEL_COLORSPACE_GAMMA;
        frame->transparent = -1;
        frame->alpha_zero_is_transparent = 0;
        if (mask != NULL) {
            frame->transparent_mask = mask;
            frame->transparent_mask_size = pixel_total;
            frame->alpha_zero_is_transparent = 1;
            mask = NULL;
        }
        status = SIXEL_OK;
        goto cleanup;
    }

    if (pixel_total > SIZE_MAX / 3u) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto cleanup;
    }
    buffer_size = pixel_total * 3u;
    pixels_u8 = (unsigned char *)sixel_allocator_malloc(pchunk->allocator,
                                                         buffer_size);
    if (pixels_u8 == NULL) {
        sixel_helper_set_additional_message(
            "load_with_gdkpixbuf: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto cleanup;
    }
    for (y = 0u; y < (size_t)height; ++y) {
        for (x = 0u; x < (size_t)width; ++x) {
            source_offset = (size_t)rowstride * y +
                x * (size_t)source_depth;
            dest_offset = (y * (size_t)width + x) * 3u;
            pixels_u8[dest_offset + 0u] = source_pixels[source_offset + 0u];
            pixels_u8[dest_offset + 1u] = source_pixels[source_offset + 1u];
            pixels_u8[dest_offset + 2u] = source_pixels[source_offset + 2u];
        }
    }

    frame->pixels.u8ptr = pixels_u8;
    pixels_u8 = NULL;
    frame->pixelformat = SIXEL_PIXELFORMAT_RGB888;
    frame->colorspace = SIXEL_COLORSPACE_GAMMA;
    frame->transparent = -1;
    frame->alpha_zero_is_transparent = 0;
    status = SIXEL_OK;

cleanup:
    sixel_allocator_free(pchunk->allocator, pixels_u8);
    sixel_allocator_free(pchunk->allocator, pixels_f32);
    sixel_allocator_free(pchunk->allocator, mask);
    return status;
}

static SIXELSTATUS
gdkpixbuf_emit_animation_frames(
    sixel_frame_t *frame,
    sixel_chunk_t const *pchunk,
    GdkPixbufAnimation *animation,
    gint64 start_time_usec,
    int fstatic,
    unsigned char const *bgcolor,
    gdkpixbuf_png_decode_hint_t const *png_hint,
    int resolved_start_frame_no,
    int validate_positive_start_frame,
    int loop_control,
    sixel_load_image_function fn_load,
    void *context,
    int *lut_ready,
    double lut[256])
{
    SIXELSTATUS status;
    GdkPixbufAnimationIter *it;
    GdkPixbuf *pixbuf;
    gboolean finished;
    gint64 current_time_usec;
    int delay_ms;
    int emit_callback;
    int source_frame_no;
    int start_frame_emitted;

    status = SIXEL_FALSE;
    it = NULL;
    pixbuf = NULL;
    finished = FALSE;
    current_time_usec = start_time_usec;
    delay_ms = 0;
    emit_callback = 1;
    source_frame_no = 0;
    start_frame_emitted = 0;
    if (frame == NULL ||
        pchunk == NULL ||
        animation == NULL ||
        fn_load == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    frame->frame_no = 0;
    frame->loop_count = 0;
    it = gdkpixbuf_animation_get_iter_with_usec(animation, current_time_usec);
    if (it == NULL) {
        gdkpixbuf_set_error_message(
            "load_with_gdkpixbuf: gdk_pixbuf_animation_get_iter failed.",
            NULL);
        status = SIXEL_GDK_ERROR;
        goto end;
    }

    for (;;) {
        finished = FALSE;
        while (!gdkpixbuf_animation_iter_on_loading_frame(it)) {
            pixbuf = gdkpixbuf_animation_iter_get_current_pixbuf(it);
            if (pixbuf == NULL) {
                finished = TRUE;
                break;
            }
            status = gdkpixbuf_copy_pixbuf_to_frame(frame,
                                                    pchunk,
                                                    pixbuf,
                                                    bgcolor,
                                                    png_hint,
                                                    lut_ready,
                                                    lut);
            if (SIXEL_FAILED(status)) {
                goto end;
            }

            delay_ms = gdkpixbuf_animation_iter_get_current_delay(it);
            if (delay_ms < 0) {
                delay_ms = 0;
            }
            current_time_usec += (gint64)delay_ms * 1000;
            frame->delay = delay_ms / 10;
            frame->multiframe = fstatic ? 0 : 1;
            if (!gdkpixbuf_animation_iter_advance_with_usec(
                    it,
                    current_time_usec)) {
                finished = TRUE;
            }

            emit_callback = 1;
            if (frame->loop_count == 0 &&
                resolved_start_frame_no != INT_MIN &&
                source_frame_no < resolved_start_frame_no) {
                emit_callback = 0;
            }
            if (emit_callback != 0) {
                if (frame->loop_count == 0 &&
                    resolved_start_frame_no != INT_MIN) {
                    start_frame_emitted = 1;
                }
                /*
                 * frame_no is consumed by the encoder to determine whether
                 * DECSC (first emitted frame) or DECRC (subsequent frames)
                 * should be written in tty scroll.
                 */
                if (frame->loop_count == 0 &&
                    resolved_start_frame_no != INT_MIN) {
                    frame->frame_no = source_frame_no -
                        resolved_start_frame_no;
                } else {
                    frame->frame_no = source_frame_no;
                }
                status = fn_load(frame, context);
                if (status != SIXEL_OK) {
                    goto end;
                }
                if (fstatic) {
                    finished = TRUE;
                }
            }

            gdkpixbuf_reset_frame_storage(frame, pchunk->allocator);
            source_frame_no++;

            if (finished) {
                break;
            }
        }

        if (source_frame_no == 0) {
            break;
        }
        if (validate_positive_start_frame != 0 &&
            frame->loop_count == 0 &&
            start_frame_emitted == 0) {
            sixel_helper_set_additional_message(
                "SIXEL_LOADER_ANIMATION_START_FRAME_NO is outside"
                " the animation frame range.");
            status = SIXEL_BAD_INPUT;
            goto end;
        }

        ++frame->loop_count;
        if (loop_control == SIXEL_LOOP_DISABLE || source_frame_no == 1) {
            break;
        }

        g_object_unref(it);
        it = NULL;
        current_time_usec = start_time_usec;
        it = gdkpixbuf_animation_get_iter_with_usec(animation,
                                                    current_time_usec);
        if (it == NULL) {
            gdkpixbuf_set_error_message(
                "load_with_gdkpixbuf: gdk_pixbuf_animation_get_iter failed.",
                NULL);
            status = SIXEL_GDK_ERROR;
            goto end;
        }
        frame->frame_no = 0;
        source_frame_no = 0;
    }

    status = SIXEL_OK;

end:
    if (it != NULL) {
        g_object_unref(it);
    }
    return status;
}

/*
 * Loader backed by gdk-pixbuf2. The entire animation is consumed via
 * GdkPixbufLoader, each frame is copied into a temporary buffer and forwarded
 * as a sixel_frame_t. Loop attributes provided by gdk-pixbuf are reconciled
 * with libsixel's loop control settings.
 */
static SIXELSTATUS
load_with_gdkpixbuf(
    sixel_chunk_t const *pchunk,      /* image data */
    int fstatic,                       /* static */
    int fuse_palette,                  /* whether to use palette if possible */
    int reqcolors,                     /* reqcolors */
    unsigned char *bgcolor,            /* background color */
    int loop_control,                  /* one of enum loop_control */
    int start_frame_no_set,
    int start_frame_no_override,
    sixel_load_image_function fn_load, /* callback */
    void *context,                     /* private data for callback */
    int *lut_ready,
    double lut[256]
)
{
    SIXELSTATUS status;
    GdkPixbuf *pixbuf;
    GdkPixbufLoader *loader;
    GdkPixbufAnimation *animation;
    sixel_frame_t *frame;
    gdkpixbuf_png_decode_hint_t png_hint;
    SIXELSTATUS png_hint_status;
    gboolean use_animation;
    gboolean is_sixel;
    SIXELSTATUS probe_status;
    int start_frame_no;
    int resolved_start_frame_no;
    int animation_frame_count;
    int validate_positive_start_frame;
    gint64 start_time_usec;

    status = SIXEL_FALSE;
    pixbuf = NULL;
    loader = NULL;
    animation = NULL;
    frame = NULL;
    gdkpixbuf_png_decode_hint_init(&png_hint);
    png_hint_status = SIXEL_FALSE;
    use_animation = FALSE;
    is_sixel = FALSE;
    probe_status = SIXEL_FALSE;
    start_frame_no = INT_MIN;
    resolved_start_frame_no = INT_MIN;
    animation_frame_count = 0;
    validate_positive_start_frame = 0;
    start_time_usec = 0;
    /*
     * gdk-pixbuf2 currently never emits PAL8. Keep legacy options accepted
     * for API compatibility, but they are intentional no-ops here.
     */
    (void)fuse_palette;
    (void)reqcolors;

    status = sixel_frame_new(&frame, pchunk->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    png_hint_status = gdkpixbuf_parse_png_decode_hint(pchunk, &png_hint);
    if (SIXEL_FAILED(png_hint_status) && png_hint_status != SIXEL_FALSE) {
        status = png_hint_status;
        goto end;
    }

#if (! GLIB_CHECK_VERSION(2, 36, 0))
    g_type_init();
#endif
    start_time_usec = g_get_real_time();
    /*
     * Try the generic loader first. Fall back to the SIXEL-specific loader
     * only when the data looks like SIXEL and the generic path fails.
     */
    probe_status = sixel_probe_is_probable(pchunk->buffer, pchunk->size);
    if (probe_status == SIXEL_OK) {
        is_sixel = TRUE;
    }
    status = gdkpixbuf_decode_chunk_with_fallback(pchunk, is_sixel, &loader);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    animation = gdkpixbuf_loader_get_animation_safe(loader);
    if (animation == NULL) {
        gdkpixbuf_set_error_message(
            "load_with_gdkpixbuf: gdk_pixbuf_loader_get_animation failed.",
            NULL);
        status = SIXEL_GDK_ERROR;
        goto end;
    }

    /*
     * animation_iter_advance() reports timeline transitions, not whether an
     * image is animated. Keep explicit static-image probing to avoid false
     * still-image classification for short-delay GIFs.
     */
    use_animation = gdkpixbuf_animation_is_static_image_safe(animation) ?
                    FALSE : TRUE;
    /*
     * Keep animated sources on the iterator path even for -S so start-frame
     * selection is applied before picking the one frame to emit.
     */
    if (use_animation != TRUE) {
        pixbuf = gdkpixbuf_loader_get_static_pixbuf(loader, animation);
        if (pixbuf == NULL) {
            gdkpixbuf_set_error_message(
                "load_with_gdkpixbuf: static image retrieval failed.",
                NULL);
            status = SIXEL_GDK_ERROR;
            goto end;
        }
        status = gdkpixbuf_copy_pixbuf_to_frame(frame,
                                                pchunk,
                                                pixbuf,
                                                bgcolor,
                                                &png_hint,
                                                lut_ready,
                                                lut);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        frame->delay = 0;
        frame->multiframe = 0;
        frame->loop_count = 0;
        status = fn_load(frame, context);
        if (status != SIXEL_OK) {
            goto end;
        }
        status = SIXEL_OK;
        goto end;
    }

    /*
     * Keep start-frame controls animation-only. Static images should decode
     * successfully even when env start-frame values are malformed.
     */
    if (start_frame_no_set) {
        start_frame_no = start_frame_no_override;
    } else {
        status = gdkpixbuf_parse_animation_start_frame_no(&start_frame_no);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    if (start_frame_no != INT_MIN) {
        /*
         * Negative offsets need one pre-count pass to resolve from the tail.
         * Positive offsets are validated during frame iteration.
         */
        if (start_frame_no < 0) {
            status = gdkpixbuf_count_animation_frames(animation,
                                                      start_time_usec,
                                                      &animation_frame_count);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
            status = gdkpixbuf_resolve_animation_start_frame_no(
                start_frame_no,
                animation_frame_count,
                &resolved_start_frame_no);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
        } else {
            resolved_start_frame_no = start_frame_no;
            validate_positive_start_frame = 1;
        }
    }

    status = gdkpixbuf_emit_animation_frames(
        frame,
        pchunk,
        animation,
        start_time_usec,
        fstatic,
        bgcolor,
        &png_hint,
        resolved_start_frame_no,
        validate_positive_start_frame,
        loop_control,
        fn_load,
        context,
        lut_ready,
        lut);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = SIXEL_OK;

end:
    if (frame) {
        /* drop the reference we obtained from sixel_frame_new() */
        sixel_frame_unref(frame);
    }
    if (loader) {
        g_object_unref(loader);
    }
    gdkpixbuf_png_decode_hint_reset(&png_hint, pchunk->allocator);

    return status;

}

static void
sixel_loader_gdkpixbuf2_ref(sixel_loader_component_t *component)
{
    sixel_loader_gdkpixbuf_component_t *self;

    self = NULL;
    if (component == NULL) {
        return;
    }

    self = (sixel_loader_gdkpixbuf_component_t *)component;
    ++self->ref;
}

static void
sixel_loader_gdkpixbuf2_unref(sixel_loader_component_t *component)
{
    sixel_loader_gdkpixbuf_component_t *self;
    sixel_allocator_t *allocator;

    self = NULL;
    allocator = NULL;
    if (component == NULL) {
        return;
    }

    self = (sixel_loader_gdkpixbuf_component_t *)component;
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
sixel_loader_gdkpixbuf2_setopt(sixel_loader_component_t *component,
                               int option,
                               void const *value)
{
    sixel_loader_gdkpixbuf_component_t *self;
    int const *flag;
    unsigned char const *color;

    self = NULL;
    flag = NULL;
    color = NULL;
    if (component == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    self = (sixel_loader_gdkpixbuf_component_t *)component;
    switch (option) {
    case SIXEL_LOADER_OPTION_REQUIRE_STATIC:
        flag = (int const *)value;
        self->fstatic = flag != NULL ? *flag : 0;
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_USE_PALETTE:
        /*
         * Accepted for API compatibility. gdk-pixbuf2 output policy does not
         * emit PAL8, so this flag is intentionally a no-op.
         */
        flag = (int const *)value;
        self->fuse_palette = flag != NULL ? *flag : 0;
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_REQCOLORS:
        /*
         * Retained for option compatibility only. gdk-pixbuf2 does not use
         * reqcolors to choose output pixel format.
         */
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
sixel_loader_gdkpixbuf2_load(sixel_loader_component_t *component,
                             sixel_chunk_t const *chunk,
                             sixel_load_image_function fn_load,
                             void *context)
{
    sixel_loader_gdkpixbuf_component_t *self;
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

    self = (sixel_loader_gdkpixbuf_component_t *)component;
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

    status = load_with_gdkpixbuf(chunk,
                                 self->fstatic,
                                 self->fuse_palette,
                                 self->reqcolors,
                                 bgcolor,
                                 self->loop_control,
                                 self->has_start_frame_no,
                                 self->start_frame_no,
                                 loader_timeline_emit_frame_callback,
                                 &timeline_state,
                                 &self->srgb_decode_lut_prepared,
                                 self->srgb_decode_lut);

    loader_timeline_callback_close_header(&timeline_state, status);
    loader_timeline_callback_close_decode(&timeline_state, status);
    loader_timeline_optional_skip_if_unmarked("post/colorspace");
    loader_timeline_optional_skip_if_unmarked("post/background");
    loader_timeline_optional_skip_if_unmarked("post/icc");

    return status;
}

static char const *
sixel_loader_gdkpixbuf2_name(sixel_loader_component_t const *component)
{
    (void)component;
    return "gdk-pixbuf2";
}

static sixel_loader_component_vtbl_t const g_sixel_loader_gdkpixbuf2_vtbl = {
    sixel_loader_gdkpixbuf2_ref,
    sixel_loader_gdkpixbuf2_unref,
    sixel_loader_gdkpixbuf2_setopt,
    sixel_loader_gdkpixbuf2_load,
    sixel_loader_gdkpixbuf2_name
};

SIXELSTATUS
sixel_loader_gdkpixbuf2_new(sixel_allocator_t *allocator,
                            sixel_loader_component_t **ppcomponent)
{
    sixel_loader_gdkpixbuf_component_t *self;

    self = NULL;
    if (allocator == NULL || ppcomponent == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *ppcomponent = NULL;
    self = (sixel_loader_gdkpixbuf_component_t *)
        sixel_allocator_malloc(allocator, sizeof(*self));
    if (self == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    memset(self, 0, sizeof(*self));
    self->base.vtbl = &g_sixel_loader_gdkpixbuf2_vtbl;
    self->allocator = allocator;
    self->ref = 1u;
    self->reqcolors = SIXEL_PALETTE_MAX;
    self->srgb_decode_lut_prepared = 0;
    self->loop_control = SIXEL_LOOP_AUTO;
    self->start_frame_no = INT_MIN;
    sixel_allocator_ref(allocator);
    *ppcomponent = &self->base;
    return SIXEL_OK;
}

#endif  /* HAVE_GDK_PIXBUF2 */

#if !HAVE_GDK_PIXBUF2
/*
 * Keep this compilation unit non-empty even when gdk-pixbuf support is
 * disabled so pedantic compilers remain quiet.
 */
typedef int loader_gdkpixbuf2_disabled;
#endif


/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
