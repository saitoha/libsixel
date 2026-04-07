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
} sixel_loader_gdkpixbuf_component_t;

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
    if (gerror == NULL || gerror->message == NULL || gerror->message[0] == '\0') {
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
    if (errno != 0 || endptr == env_value || *endptr != '\0') {
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

static unsigned char
gdkpixbuf_blend_channel(unsigned int source,
                        unsigned int alpha,
                        unsigned int background)
{
    return (unsigned char)(
        (source * alpha + background * (255u - alpha) + 127u) / 255u);
}

static SIXELSTATUS
gdkpixbuf_copy_pixbuf_to_frame(sixel_frame_t *frame,
                               sixel_chunk_t const *pchunk,
                               GdkPixbuf *pixbuf,
                               unsigned char const *bgcolor)
{
    SIXELSTATUS status;
    unsigned char *pixels;
    unsigned char *source_pixels;
    size_t pixel_total;
    size_t buffer_size;
    size_t x;
    size_t y;
    size_t source_offset;
    size_t dest_offset;
    int width;
    int height;
    int rowstride;
    int source_depth;
    int has_alpha;
    int inspect_alpha;
    int preserve_alpha;
    unsigned int alpha;

    status = SIXEL_OK;
    pixels = NULL;
    source_pixels = NULL;
    pixel_total = 0u;
    buffer_size = 0u;
    x = 0u;
    y = 0u;
    source_offset = 0u;
    dest_offset = 0u;
    width = 0;
    height = 0;
    rowstride = 0;
    source_depth = 0;
    has_alpha = 0;
    inspect_alpha = 0;
    preserve_alpha = 0;
    alpha = 0u;
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

    has_alpha = gdk_pixbuf_get_has_alpha(pixbuf) ? 1 : 0;
    inspect_alpha = has_alpha && bgcolor == NULL;
    source_depth = has_alpha ? 4 : 3;
    rowstride = gdk_pixbuf_get_rowstride(pixbuf);

    if ((size_t)width > SIZE_MAX / (size_t)height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_total = (size_t)width * (size_t)height;
    if (pixel_total > SIZE_MAX / (inspect_alpha ? 4u : 3u)) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    buffer_size = pixel_total * (inspect_alpha ? 4u : 3u);

    sixel_frame_set_pixels(frame,
                           sixel_allocator_malloc(pchunk->allocator,
                                                  buffer_size));
    pixels = sixel_frame_get_pixels(frame);
    if (pixels == NULL) {
        sixel_helper_set_additional_message(
            "load_with_gdkpixbuf: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    source_pixels = gdk_pixbuf_get_pixels(pixbuf);
    if (source_pixels == NULL) {
        sixel_helper_set_additional_message(
            "load_with_gdkpixbuf: gdk_pixbuf_get_pixels() failed.");
        return SIXEL_GDK_ERROR;
    }

    for (y = 0u; y < (size_t)height; ++y) {
        for (x = 0u; x < (size_t)width; ++x) {
            source_offset = (size_t)rowstride * y + x * (size_t)source_depth;
            if (inspect_alpha) {
                alpha = source_pixels[source_offset + 3u];
                if (alpha != 255u) {
                    preserve_alpha = 1;
                }
                dest_offset = (y * (size_t)width + x) * 4u;
                pixels[dest_offset + 0u] = source_pixels[source_offset + 0u];
                pixels[dest_offset + 1u] = source_pixels[source_offset + 1u];
                pixels[dest_offset + 2u] = source_pixels[source_offset + 2u];
                pixels[dest_offset + 3u] = (unsigned char)alpha;
            } else {
                dest_offset = (y * (size_t)width + x) * 3u;
                if (has_alpha) {
                    alpha = source_pixels[source_offset + 3u];
                    if (bgcolor != NULL) {
                        pixels[dest_offset + 0u] = gdkpixbuf_blend_channel(
                            source_pixels[source_offset + 0u],
                            alpha,
                            bgcolor[0]);
                        pixels[dest_offset + 1u] = gdkpixbuf_blend_channel(
                            source_pixels[source_offset + 1u],
                            alpha,
                            bgcolor[1]);
                        pixels[dest_offset + 2u] = gdkpixbuf_blend_channel(
                            source_pixels[source_offset + 2u],
                            alpha,
                            bgcolor[2]);
                    } else {
                        pixels[dest_offset + 0u] =
                            source_pixels[source_offset + 0u];
                        pixels[dest_offset + 1u] =
                            source_pixels[source_offset + 1u];
                        pixels[dest_offset + 2u] =
                            source_pixels[source_offset + 2u];
                    }
                } else {
                    pixels[dest_offset + 0u] = source_pixels[source_offset + 0u];
                    pixels[dest_offset + 1u] = source_pixels[source_offset + 1u];
                    pixels[dest_offset + 2u] = source_pixels[source_offset + 2u];
                }
            }
        }
    }

    if (inspect_alpha && !preserve_alpha) {
        for (x = 0u; x < pixel_total; ++x) {
            pixels[x * 3u + 0u] = pixels[x * 4u + 0u];
            pixels[x * 3u + 1u] = pixels[x * 4u + 1u];
            pixels[x * 3u + 2u] = pixels[x * 4u + 2u];
        }
    }

    frame->width = width;
    frame->height = height;
    frame->pixelformat = preserve_alpha
        ? SIXEL_PIXELFORMAT_RGBA8888
        : SIXEL_PIXELFORMAT_RGB888;
    frame->colorspace = SIXEL_COLORSPACE_GAMMA;
    frame->transparent = -1;
    frame->alpha_zero_is_transparent = preserve_alpha ? 1 : 0;

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
    void *context                      /* private data for callback */
)
{
    SIXELSTATUS status = SIXEL_FALSE;
    GdkPixbuf *pixbuf;
    GdkPixbufLoader *loader = NULL;
    gboolean loader_closed = FALSE;  /* remember if loader was already closed */
    GError *loader_error;
    GdkPixbufAnimation *animation;
    GdkPixbufAnimationIter *it = NULL;
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif  /* HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS */
    GTimeVal time_val;
    GTimeVal start_time;
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
# pragma GCC diagnostic pop
#endif  /* HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS */
    sixel_frame_t *frame = NULL;
    unsigned char *frame_pixels;
    int delay_ms;
    gboolean use_animation = FALSE;
    gboolean is_sixel = FALSE;
    SIXELSTATUS probe_status;
    int start_frame_no;
    int resolved_start_frame_no;
    int animation_frame_count;
    int emit_callback;
    int source_frame_no;

    (void) fuse_palette;
    (void) reqcolors;
    (void) bgcolor;

    loader_error = NULL;
    start_frame_no = INT_MIN;
    resolved_start_frame_no = INT_MIN;
    animation_frame_count = 0;
    emit_callback = 1;
    source_frame_no = 0;

    if (start_frame_no_set) {
        start_frame_no = start_frame_no_override;
    } else {
        status = gdkpixbuf_parse_animation_start_frame_no(&start_frame_no);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    status = sixel_frame_new(&frame, pchunk->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

#if (! GLIB_CHECK_VERSION(2, 36, 0))
    g_type_init();
#endif
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif  /* HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS */
    g_get_current_time(&time_val);
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
# pragma GCC diagnostic pop
#endif  /* HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS */
    start_time = time_val;
    /*
     * Try the generic loader first. Fall back to the SIXEL-specific loader
     * only when the data looks like SIXEL and the generic path fails.
     */
    probe_status = sixel_probe_is_probable(pchunk->buffer, pchunk->size);
    if (probe_status == SIXEL_OK) {
        is_sixel = TRUE;
    }
    loader = gdk_pixbuf_loader_new();
    if (loader == NULL) {
        gdkpixbuf_set_error_message(
            "load_with_gdkpixbuf: gdk_pixbuf_loader_new failed.",
            loader_error);
        gdkpixbuf_clear_error(&loader_error);
        status = SIXEL_GDK_ERROR;
        goto end;
    }
    /*
     * feed the entire chunk; gdk-pixbuf will buffer as needed.  Always close
     * the loader to let gdk finalize the parse state before consuming.
     */
    if (! gdk_pixbuf_loader_write(
            loader, pchunk->buffer, pchunk->size, &loader_error)) {
        if (!is_sixel) {
            gdkpixbuf_set_error_message(
                "load_with_gdkpixbuf: generic loader write failed.",
                loader_error);
            gdkpixbuf_clear_error(&loader_error);
            status = SIXEL_GDK_ERROR;
            goto end;
        }
        gdkpixbuf_set_error_message(
            "load_with_gdkpixbuf: generic loader write failed before sixel fallback.",
            loader_error);
        gdkpixbuf_clear_error(&loader_error);
        g_object_unref(loader);
        loader = NULL;
        loader = gdk_pixbuf_loader_new_with_type("sixel", &loader_error);
        if (loader == NULL) {
            gdkpixbuf_set_error_message(
                "load_with_gdkpixbuf: sixel fallback loader creation failed.",
                loader_error);
            gdkpixbuf_clear_error(&loader_error);
            status = SIXEL_GDK_ERROR;
            goto end;
        }
        if (! gdk_pixbuf_loader_write(
                loader, pchunk->buffer, pchunk->size, &loader_error)) {
            gdkpixbuf_set_error_message(
                "load_with_gdkpixbuf: sixel fallback loader write failed.",
                loader_error);
            gdkpixbuf_clear_error(&loader_error);
            status = SIXEL_GDK_ERROR;
            goto end;
        }
    }
    if (! gdk_pixbuf_loader_close(loader, &loader_error)) {
        if (!is_sixel) {
            gdkpixbuf_set_error_message(
                "load_with_gdkpixbuf: generic loader close failed.",
                loader_error);
            gdkpixbuf_clear_error(&loader_error);
            status = SIXEL_GDK_ERROR;
            goto end;
        }
        gdkpixbuf_set_error_message(
            "load_with_gdkpixbuf: generic loader close failed before sixel fallback.",
            loader_error);
        gdkpixbuf_clear_error(&loader_error);
        g_object_unref(loader);
        loader = NULL;
        loader = gdk_pixbuf_loader_new_with_type("sixel", &loader_error);
        if (loader == NULL) {
            gdkpixbuf_set_error_message(
                "load_with_gdkpixbuf: sixel fallback loader creation failed.",
                loader_error);
            gdkpixbuf_clear_error(&loader_error);
            status = SIXEL_GDK_ERROR;
            goto end;
        }
        if (! gdk_pixbuf_loader_write(
                loader, pchunk->buffer, pchunk->size, &loader_error)) {
            gdkpixbuf_set_error_message(
                "load_with_gdkpixbuf: sixel fallback loader write failed.",
                loader_error);
            gdkpixbuf_clear_error(&loader_error);
            status = SIXEL_GDK_ERROR;
            goto end;
        }
        if (! gdk_pixbuf_loader_close(loader, &loader_error)) {
            gdkpixbuf_set_error_message(
                "load_with_gdkpixbuf: sixel fallback loader close failed.",
                loader_error);
            gdkpixbuf_clear_error(&loader_error);
            status = SIXEL_GDK_ERROR;
            goto end;
        }
    }
    loader_closed = TRUE;

#if HAVE_GDK_PIXBUF_244
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif  /* HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS */
    animation = gdk_pixbuf_loader_get_animation(loader);
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
# pragma GCC diagnostic pop
#endif  /* HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS */
#else
    animation = gdk_pixbuf_loader_get_animation(loader);
#endif
    if (animation == NULL) {
        gdkpixbuf_set_error_message(
            "load_with_gdkpixbuf: gdk_pixbuf_loader_get_animation failed.",
            NULL);
        status = SIXEL_GDK_ERROR;
        goto end;
    }

#if HAVE_GDK_PIXBUF_244
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif  /* HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS */
    /*
     * animation_iter_advance() returns whether a frame changed at a timestamp,
     * not whether the source is animated. Use the explicit static-image probe
     * so short-delay GIFs are not misclassified as still images.
     */
    use_animation = gdk_pixbuf_animation_is_static_image(animation) ? FALSE :
                    TRUE;
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
# pragma GCC diagnostic pop
#endif  /* HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS */
    /*
     * Keep animated sources on the iterator path even for -S so the loader
     * can honor SIXEL_LOADER_ANIMATION_START_FRAME_NO before selecting the
     * single still frame to emit.
     */
    if (!use_animation) {
        pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
        if (pixbuf == NULL) {
            gdkpixbuf_set_error_message(
                "load_with_gdkpixbuf: gdk_pixbuf_loader_get_pixbuf failed.",
                NULL);
            status = SIXEL_GDK_ERROR;
            goto end;
        }
        status = gdkpixbuf_copy_pixbuf_to_frame(frame,
                                                pchunk,
                                                pixbuf,
                                                bgcolor);
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
    } else {
        gboolean finished;

        if (start_frame_no != INT_MIN) {
            status = gdkpixbuf_count_animation_frames(animation,
                                                      g_get_real_time(),
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
        }

        time_val = start_time;
        frame->frame_no = 0;
        source_frame_no = 0;
        frame->loop_count = 0;

#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif  /* HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS */
        it = gdk_pixbuf_animation_get_iter(animation, &time_val);
        if (it == NULL) {
            gdkpixbuf_set_error_message(
                "load_with_gdkpixbuf: gdk_pixbuf_animation_get_iter failed.",
                NULL);
            status = SIXEL_GDK_ERROR;
            goto end;
        }

        /*
         * gdk-pixbuf 2.44 still exposes the iterator API under deprecation
         * guards. Keep warnings silenced while walking frames so we can reuse
         * the same code path for animation playback.
         */
        for (;;) {
            /* handle one logical loop of the animation */
            finished = FALSE;
            while (!gdkpixbuf_animation_iter_on_loading_frame(it)) {
                /* {{{ */
                pixbuf = gdkpixbuf_animation_iter_get_current_pixbuf(it);
                if (pixbuf == NULL) {
                    finished = TRUE;
                    break;
                }
                status = gdkpixbuf_copy_pixbuf_to_frame(frame,
                                                        pchunk,
                                                        pixbuf,
                                                        bgcolor);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
                delay_ms = gdkpixbuf_animation_iter_get_current_delay(it);
                if (delay_ms < 0) {
                    delay_ms = 0;
                }
                /*
                 * advance the synthetic clock before asking gdk to move
                 * forward
                 */
                g_time_val_add(&time_val, delay_ms * 1000);
                frame->delay = delay_ms / 10;
                frame->multiframe = fstatic ? 0 : 1;

                if (!gdk_pixbuf_animation_iter_advance(it, &time_val)) {
                    finished = TRUE;
                }

                emit_callback = 1;
                if (frame->loop_count == 0 &&
                    resolved_start_frame_no != INT_MIN &&
                    source_frame_no < resolved_start_frame_no) {
                    emit_callback = 0;
                }
                if (emit_callback) {
                    /*
                     * frame_no is consumed by the encoder to determine
                     * whether DECSC (first emitted frame) or DECRC
                     * (subsequent frame) should be written in tty scroll.
                     * Keep it as an emitted-frame index instead of the
                     * original animation index so start-frame skipping does
                     * not suppress DECSC on the first visible frame.
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
                /*
                 * Release the frame buffer only if it is still the one we
                 * attached. Downstream filters may replace frame->pixels,
                 * so always query the current pointer to avoid double free.
                 */
                frame_pixels = sixel_frame_get_pixels(frame);
                if (frame_pixels != NULL) {
                    sixel_allocator_free(pchunk->allocator, frame_pixels);
                    sixel_frame_set_pixels(frame, NULL);
                }
                source_frame_no++;

                if (finished) {
                    break;
                }
                /* }}} */
            }

            if (source_frame_no == 0) {
                break;
            }

            /* finished processing one full loop */
            ++frame->loop_count;

            if (loop_control == SIXEL_LOOP_DISABLE || source_frame_no == 1) {
                break;
            }
            /*
             * GdkPixbufAnimation does not expose a finite loop counter.
             * Loop handling therefore depends on loop_control only:
             *   - DISABLE: stop after one logical loop.
             *   - AUTO/FORCE: keep replaying until outer processing stops.
             */

            /* restart iteration from the beginning for the next pass */
            g_object_unref(it);
            time_val = start_time;
            it = gdk_pixbuf_animation_get_iter(animation, &time_val);
            if (it == NULL) {
                gdkpixbuf_set_error_message(
                    "load_with_gdkpixbuf: gdk_pixbuf_animation_get_iter failed.",
                    NULL);
                status = SIXEL_GDK_ERROR;
                goto end;
            }
            /* next pass starts counting frames from zero again */
            frame->frame_no = 0;
            source_frame_no = 0;
        }
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
# pragma GCC diagnostic pop
#endif  /* HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS */
    }
#else
    use_animation = gdk_pixbuf_animation_is_static_image(animation) ? FALSE :
                    TRUE;
    /*
     * Keep animated sources on the iterator path even for -S so the loader
     * can honor SIXEL_LOADER_ANIMATION_START_FRAME_NO before selecting the
     * single still frame to emit.
     */
    if (!use_animation) {
        pixbuf = gdk_pixbuf_animation_get_static_image(animation);
        if (pixbuf == NULL) {
            gdkpixbuf_set_error_message(
                "load_with_gdkpixbuf: gdk_pixbuf_animation_get_static_image failed.",
                NULL);
            status = SIXEL_GDK_ERROR;
            goto end;
        }
        status = gdkpixbuf_copy_pixbuf_to_frame(frame,
                                                pchunk,
                                                pixbuf,
                                                bgcolor);
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
    } else {
        gboolean finished;

        /* reset iterator to the beginning of the timeline */
        if (start_frame_no != INT_MIN) {
            status = gdkpixbuf_count_animation_frames(animation,
                                                      g_get_real_time(),
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
        }

        time_val = start_time;
        frame->frame_no = 0;
        source_frame_no = 0;
        frame->loop_count = 0;

#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif  /* HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS */
        it = gdk_pixbuf_animation_get_iter(animation, &time_val);
        if (it == NULL) {
            gdkpixbuf_set_error_message(
                "load_with_gdkpixbuf: gdk_pixbuf_animation_get_iter failed.",
                NULL);
            status = SIXEL_GDK_ERROR;
            goto end;
        }

        for (;;) {
            /* handle one logical loop of the animation */
            finished = FALSE;
            while (!gdkpixbuf_animation_iter_on_loading_frame(it)) {
                /* {{{ */
                pixbuf = gdkpixbuf_animation_iter_get_current_pixbuf(it);
                if (pixbuf == NULL) {
                    finished = TRUE;
                    break;
                }
                status = gdkpixbuf_copy_pixbuf_to_frame(frame,
                                                        pchunk,
                                                        pixbuf,
                                                        bgcolor);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
                delay_ms = gdkpixbuf_animation_iter_get_current_delay(it);
                if (delay_ms < 0) {
                    delay_ms = 0;
                }
                /*
                 * advance the synthetic clock before asking gdk to move
                 * forward
                 */
                g_time_val_add(&time_val, delay_ms * 1000);
                frame->delay = delay_ms / 10;
                frame->multiframe = fstatic ? 0 : 1;

                if (!gdk_pixbuf_animation_iter_advance(it, &time_val)) {
                    finished = TRUE;
                }

                emit_callback = 1;
                if (frame->loop_count == 0 &&
                    resolved_start_frame_no != INT_MIN &&
                    source_frame_no < resolved_start_frame_no) {
                    emit_callback = 0;
                }
                if (emit_callback) {
                    /*
                     * frame_no is consumed by the encoder to determine
                     * whether DECSC (first emitted frame) or DECRC
                     * (subsequent frame) should be written in tty scroll.
                     * Keep it as an emitted-frame index instead of the
                     * original animation index so start-frame skipping does
                     * not suppress DECSC on the first visible frame.
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
                /*
                 * Release the frame buffer only if it is still the one we
                 * attached. Downstream filters may replace frame->pixels,
                 * so always query the current pointer to avoid double free.
                 */
                frame_pixels = sixel_frame_get_pixels(frame);
                if (frame_pixels != NULL) {
                    sixel_allocator_free(pchunk->allocator, frame_pixels);
                    sixel_frame_set_pixels(frame, NULL);
                }
                source_frame_no++;

                if (finished) {
                    break;
                }
                /* }}} */
            }

            if (source_frame_no == 0) {
                break;
            }

            /* finished processing one full loop */
            ++frame->loop_count;

            if (loop_control == SIXEL_LOOP_DISABLE || source_frame_no == 1) {
                break;
            }
            /*
             * GdkPixbufAnimation does not expose a finite loop counter.
             * Loop handling therefore depends on loop_control only:
             *   - DISABLE: stop after one logical loop.
             *   - AUTO/FORCE: keep replaying until outer processing stops.
             */

            /* restart iteration from the beginning for the next pass */
            g_object_unref(it);
            time_val = start_time;
            it = gdk_pixbuf_animation_get_iter(animation, &time_val);
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
# pragma GCC diagnostic pop
#endif  /* HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS */
            if (it == NULL) {
                gdkpixbuf_set_error_message(
                    "load_with_gdkpixbuf: gdk_pixbuf_animation_get_iter failed.",
                    NULL);
                status = SIXEL_GDK_ERROR;
                goto end;
            }
            /* next pass starts counting frames from zero again */
            frame->frame_no = 0;
            source_frame_no = 0;
        }
    }
#endif

    status = SIXEL_OK;

end:
    if (frame) {
        /* drop the reference we obtained from sixel_frame_new() */
        sixel_frame_unref(frame);
    }
    if (it) {
        g_object_unref(it);
    }
    if (loader) {
        if (!loader_closed) {
            /* ensure the incremental loader is finalized even on error paths */
            gdk_pixbuf_loader_close(loader, NULL);
        }
        g_object_unref(loader);
    }
    gdkpixbuf_clear_error(&loader_error);

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
sixel_loader_gdkpixbuf2_load(sixel_loader_component_t *component,
                             sixel_chunk_t const *chunk,
                             sixel_load_image_function fn_load,
                             void *context)
{
    sixel_loader_gdkpixbuf_component_t *self;
    unsigned char *bgcolor;

    self = NULL;
    bgcolor = NULL;
    if (component == NULL || chunk == NULL || fn_load == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    self = (sixel_loader_gdkpixbuf_component_t *)component;
    if (self->has_bgcolor) {
        bgcolor = self->bgcolor;
    }

    return load_with_gdkpixbuf(chunk,
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
