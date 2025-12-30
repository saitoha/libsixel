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

#if HAVE_STRING_H
# include <string.h>
#endif

#ifdef HAVE_GDK_PIXBUF2
# if HAVE_DIAGNOSTIC_TYPEDEF_REDEFINITION
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wtypedef-redefinition"
# endif
# include <gdk-pixbuf/gdk-pixbuf.h>
# include <gdk-pixbuf/gdk-pixbuf-simple-anim.h>
# if HAVE_DIAGNOSTIC_TYPEDEF_REDEFINITION
#  pragma GCC diagnostic pop
# endif
#endif

#include <sixel.h>

#include "allocator.h"
#include "frame.h"
#include "loader-gdk-pixbuf2.h"

/*
 * Loader backed by gdk-pixbuf2. The entire animation is consumed via
 * GdkPixbufLoader, each frame is copied into a temporary buffer and forwarded
 * as a sixel_frame_t. Loop attributes provided by gdk-pixbuf are reconciled
 * with libsixel's loop control settings.
 */
SIXELSTATUS
load_with_gdkpixbuf(
    sixel_chunk_t const *pchunk,      /* image data */
    int fstatic,                       /* static */
    int fuse_palette,                  /* whether to use palette if possible */
    int reqcolors,                     /* reqcolors */
    unsigned char *bgcolor,            /* background color */
    int loop_control,                  /* one of enum loop_control */
    sixel_load_image_function fn_load, /* callback */
    void *context                      /* private data for callback */
)
{
    SIXELSTATUS status = SIXEL_FALSE;
    GdkPixbuf *pixbuf;
    GdkPixbufLoader *loader = NULL;
    gboolean loader_closed = FALSE;  /* remember if loader was already closed */
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
    int stride;
    unsigned char *p;
    unsigned char *pixels;
    int i;
    int depth;
    int anim_loop_count = (-1);  /* (-1): infinite, >=0: finite loop count */
    int delay_ms;
    gboolean use_animation = FALSE;
    GError *error = NULL;

    (void) fuse_palette;
    (void) reqcolors;
    (void) bgcolor;

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
    loader = gdk_pixbuf_loader_new();
    if (loader == NULL) {
        status = SIXEL_GDK_ERROR;
        goto end;
    }
    /*
     * feed the entire chunk; gdk-pixbuf will buffer as needed.  Always close
     * the loader to let gdk finalize the parse state before consuming.
     */
    if (! gdk_pixbuf_loader_write(loader, pchunk->buffer, pchunk->size, &error)) {
        sixel_helper_set_additional_message(error->message);
        status = SIXEL_GDK_ERROR;
        goto end;
    }
    if (! gdk_pixbuf_loader_close(loader, &error)) {
        sixel_helper_set_additional_message(error->message);
        status = SIXEL_GDK_ERROR;
        goto end;
    }
    loader_closed = TRUE;
    animation = gdk_pixbuf_loader_get_animation(loader);
    if (animation == NULL) {
        status = SIXEL_GDK_ERROR;
        goto end;
    }

    use_animation = gdk_pixbuf_animation_is_static_image(animation) ? FALSE :
                    TRUE;
    if (fstatic || !use_animation) {
        pixbuf = gdk_pixbuf_animation_get_static_image(animation);
        if (pixbuf == NULL) {
            status = SIXEL_GDK_ERROR;
            goto end;
        }
        frame->width = gdk_pixbuf_get_width(pixbuf);
        frame->height = gdk_pixbuf_get_height(pixbuf);
        stride = gdk_pixbuf_get_rowstride(pixbuf);
        sixel_frame_set_pixels(
            frame,
            sixel_allocator_malloc(
                pchunk->allocator,
                (size_t)(frame->height * stride)));
        pixels = sixel_frame_get_pixels(frame);
        if (pixels == NULL) {
            sixel_helper_set_additional_message(
                "load_with_gdkpixbuf: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        if (gdk_pixbuf_get_has_alpha(pixbuf)) {
            frame->pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
            depth = 4;
        } else {
            frame->pixelformat = SIXEL_PIXELFORMAT_RGB888;
            depth = 3;
        }
        p = gdk_pixbuf_get_pixels(pixbuf);
        if (stride == frame->width * depth) {
            memcpy(pixels, p, (size_t)(frame->height * stride));
        } else {
            for (i = 0; i < frame->height; ++i) {
                memcpy(pixels + frame->width * depth * i,
                       p + stride * i,
                       (size_t)(frame->width * depth));
            }
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
        time_val = start_time;
        frame->frame_no = 0;
        frame->loop_count = 0;

#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
# if defined(__GNUC__) && !defined(__PCC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
# endif
#endif  /* HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS */
        it = gdk_pixbuf_animation_get_iter(animation, &time_val);
        if (it == NULL) {
            status = SIXEL_GDK_ERROR;
            goto end;
        }

        for (;;) {
            /* handle one logical loop of the animation */
            finished = FALSE;
            while (!gdk_pixbuf_animation_iter_on_currently_loading_frame(it)) {
                /* {{{ */
                pixbuf = gdk_pixbuf_animation_iter_get_pixbuf(it);
                if (pixbuf == NULL) {
                    finished = TRUE;
                    break;
                }
                /* allocate a scratch copy of the current frame */
                frame->width = gdk_pixbuf_get_width(pixbuf);
                frame->height = gdk_pixbuf_get_height(pixbuf);
                stride = gdk_pixbuf_get_rowstride(pixbuf);
                sixel_frame_set_pixels(
                    frame,
                    sixel_allocator_malloc(
                        pchunk->allocator,
                        (size_t)(frame->height * stride)));
                pixels = sixel_frame_get_pixels(frame);
                if (pixels == NULL) {
                    sixel_helper_set_additional_message(
                        "load_with_gdkpixbuf: "
                        "sixel_allocator_malloc() failed.");
                    status = SIXEL_BAD_ALLOCATION;
                    goto end;
                }
                if (gdk_pixbuf_get_has_alpha(pixbuf)) {
                    frame->pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
                    depth = 4;
                } else {
                    frame->pixelformat = SIXEL_PIXELFORMAT_RGB888;
                    depth = 3;
                }
                p = gdk_pixbuf_get_pixels(pixbuf);
                if (stride == frame->width * depth) {
                    memcpy(pixels, p,
                           (size_t)(frame->height * stride));
                } else {
                    for (i = 0; i < frame->height; ++i) {
                        memcpy(pixels + frame->width * depth * i,
                               p + stride * i,
                               (size_t)(frame->width * depth));
                    }
                }
                delay_ms = gdk_pixbuf_animation_iter_get_delay_time(it);
                if (delay_ms < 0) {
                    delay_ms = 0;
                }
                /*
                 * advance the synthetic clock before asking gdk to move
                 * forward
                 */
                g_time_val_add(&time_val, delay_ms * 1000);
                frame->delay = delay_ms / 10;
                frame->multiframe = 1;

                if (!gdk_pixbuf_animation_iter_advance(it, &time_val)) {
                    finished = TRUE;
                }
                status = fn_load(frame, context);
                if (status != SIXEL_OK) {
                    goto end;
                }
                /* release scratch pixels before decoding the next frame */
                sixel_allocator_free(pchunk->allocator, pixels);
                sixel_frame_set_pixels(frame, NULL);
                frame->frame_no++;

                if (finished) {
                    break;
                }
                /* }}} */
            }

            if (frame->frame_no == 0) {
                break;
            }

            /* finished processing one full loop */
            ++frame->loop_count;

            if (loop_control == SIXEL_LOOP_DISABLE || frame->frame_no == 1) {
                break;
            }
            if (loop_control == SIXEL_LOOP_AUTO) {
                /* obey header-provided loop count when AUTO */
                if (anim_loop_count >= 0 &&
                    frame->loop_count >= anim_loop_count) {
                    break;
                }
            } else if (loop_control != SIXEL_LOOP_FORCE &&
                       anim_loop_count > 0 &&
                       frame->loop_count >= anim_loop_count) {
                break;
            }

            /* restart iteration from the beginning for the next pass */
            g_object_unref(it);
            time_val = start_time;
            it = gdk_pixbuf_animation_get_iter(animation, &time_val);
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
# if defined(__GNUC__) && !defined(__PCC__)
#  pragma GCC diagnostic pop
# endif
#endif  /* HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS */
            if (it == NULL) {
                status = SIXEL_GDK_ERROR;
                goto end;
            }
            /* next pass starts counting frames from zero again */
            frame->frame_no = 0;
        }
    }

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

    return status;

}

#endif  /* HAVE_GDK_PIXBUF2 */

#if !HAVE_GDK_PIXBUF2
/*
 * Keep this compilation unit non-empty even when gdk-pixbuf support is
 * disabled so pedantic compilers remain quiet.
 */
typedef int loader_gdkpixbuf2_disabled;
#endif

