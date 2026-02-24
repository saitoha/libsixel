/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * librsvg-backed SVG loader.  The backend rasterizes SVG into an RGB frame
 * through Cairo so the downstream pipeline keeps using the same pixel path
 * as the other decoders.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if HAVE_LIBRSVG

#if HAVE_STDINT_H
# include <stdint.h>
#endif
#if HAVE_STRING_H
# include <string.h>
#endif
#if HAVE_CTYPE_H
# include <ctype.h>
#endif

#include <librsvg/rsvg.h>
#include <cairo.h>

#include "allocator.h"
#include "frame.h"
#include "loader-common.h"
#include "loader-librsvg.h"

typedef struct sixel_loader_librsvg_component {
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
} sixel_loader_librsvg_component_t;

#define LIBRSVG_DEFAULT_WIDTH  300
#define LIBRSVG_DEFAULT_HEIGHT 150

/*
 * Try to identify SVG input quickly so the registry can skip this backend for
 * obvious raster formats.
 */
static int
chunk_is_svg_like(sixel_chunk_t const *chunk)
{
    size_t offset;
    size_t limit;
    size_t index;

    if (chunk == NULL || chunk->buffer == NULL || chunk->size == 0) {
        return 0;
    }

    offset = 0;
    if (chunk->size >= 3 &&
            chunk->buffer[0] == 0xef &&
            chunk->buffer[1] == 0xbb &&
            chunk->buffer[2] == 0xbf) {
        offset = 3;
    }

    while (offset < chunk->size &&
           isspace((unsigned char)chunk->buffer[offset]) != 0) {
        ++offset;
    }

    if (offset >= chunk->size) {
        return 0;
    }

    limit = chunk->size;
    if (limit > 4096) {
        limit = 4096;
    }

    if (offset + 4 < limit && chunk->buffer[offset] == '<') {
        if (chunk->buffer[offset + 1] == 's' &&
                chunk->buffer[offset + 2] == 'v' &&
                chunk->buffer[offset + 3] == 'g') {
            return 1;
        }
    }

    for (index = offset; index + 4 < limit; ++index) {
        if (chunk->buffer[index] == '<' &&
                chunk->buffer[index + 1] == 's' &&
                chunk->buffer[index + 2] == 'v' &&
                chunk->buffer[index + 3] == 'g') {
            return 1;
        }
    }

    return 0;
}

/*
 * Determine the raster viewport used for render_document().  librsvg may lack
 * explicit pixel dimensions when only a viewBox is provided, so this helper
 * falls back to the conventional SVG viewport size.
 */
static void
librsvg_pick_size(RsvgHandle *handle, int *pwidth, int *pheight)
{
    gboolean has_viewbox;
    RsvgRectangle viewbox;
    gdouble pixel_width;
    gdouble pixel_height;
    int width;
    int height;

    width = LIBRSVG_DEFAULT_WIDTH;
    height = LIBRSVG_DEFAULT_HEIGHT;
    pixel_width = 0.0;
    pixel_height = 0.0;
    has_viewbox = FALSE;
    viewbox.x = 0.0;
    viewbox.y = 0.0;
    viewbox.width = 0.0;
    viewbox.height = 0.0;

    if (rsvg_handle_get_intrinsic_size_in_pixels(handle,
                                                  &pixel_width,
                                                  &pixel_height)) {
        if (pixel_width >= 1.0 && pixel_width <= 32767.0) {
            width = (int)(pixel_width + 0.5);
        }
        if (pixel_height >= 1.0 && pixel_height <= 32767.0) {
            height = (int)(pixel_height + 0.5);
        }
    } else {
        rsvg_handle_get_intrinsic_dimensions(handle,
                                             NULL,
                                             NULL,
                                             NULL,
                                             NULL,
                                             &has_viewbox,
                                             &viewbox);
        if (has_viewbox && viewbox.width > 0.0 && viewbox.height > 0.0) {
            if (viewbox.width >= 1.0 && viewbox.width <= 32767.0) {
                width = (int)(viewbox.width + 0.5);
            }
            if (viewbox.height >= 1.0 && viewbox.height <= 32767.0) {
                height = (int)(viewbox.height + 0.5);
            }
        }
    }

    if (width <= 0) {
        width = LIBRSVG_DEFAULT_WIDTH;
    }
    if (height <= 0) {
        height = LIBRSVG_DEFAULT_HEIGHT;
    }

    *pwidth = width;
    *pheight = height;
}

static SIXELSTATUS
librsvg_render_to_frame(sixel_frame_t *frame,
                        sixel_chunk_t const *chunk,
                        unsigned char const *bgcolor)
{
    SIXELSTATUS status;
    RsvgHandle *handle;
    cairo_surface_t *surface;
    cairo_t *cr;
    cairo_status_t cairo_stat;
    GError *gerror;
    RsvgRectangle viewport;
    unsigned char *pixels;
    unsigned char const *row;
    size_t row_stride;
    int x;
    int y;
    uint32_t const *src;
    uint32_t pixel;
    size_t dst;

    status = SIXEL_BAD_INPUT;
    handle = NULL;
    surface = NULL;
    cr = NULL;
    gerror = NULL;
    viewport.x = 0.0;
    viewport.y = 0.0;
    viewport.width = 0.0;
    viewport.height = 0.0;
    pixels = NULL;
    row = NULL;
    row_stride = 0;

    handle = rsvg_handle_new_from_data(chunk->buffer, chunk->size, &gerror);
    if (handle == NULL) {
        sixel_helper_set_additional_message(
            "librsvg_render_to_frame: unable to parse SVG data.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    librsvg_pick_size(handle, &frame->width, &frame->height);
    if (frame->width <= 0 || frame->height <= 0) {
        sixel_helper_set_additional_message(
            "librsvg_render_to_frame: invalid dimensions.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                         frame->width,
                                         frame->height);
    cairo_stat = cairo_surface_status(surface);
    if (cairo_stat != CAIRO_STATUS_SUCCESS) {
        sixel_helper_set_additional_message(
            "librsvg_render_to_frame: cairo_image_surface_create failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    cr = cairo_create(surface);
    cairo_stat = cairo_status(cr);
    if (cairo_stat != CAIRO_STATUS_SUCCESS) {
        sixel_helper_set_additional_message(
            "librsvg_render_to_frame: cairo_create failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    cairo_set_source_rgb(cr,
                         ((double)bgcolor[0]) / 255.0,
                         ((double)bgcolor[1]) / 255.0,
                         ((double)bgcolor[2]) / 255.0);
    cairo_paint(cr);

    viewport.width = (double)frame->width;
    viewport.height = (double)frame->height;
    if (!rsvg_handle_render_document(handle, cr, &viewport, &gerror)) {
        sixel_helper_set_additional_message(
            "librsvg_render_to_frame: rsvg_handle_render_document failed.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    cairo_surface_flush(surface);
    row = cairo_image_surface_get_data(surface);
    row_stride = (size_t)cairo_image_surface_get_stride(surface);

    if ((size_t)frame->width > SIZE_MAX / 3 ||
            (size_t)frame->height > SIZE_MAX / ((size_t)frame->width * 3)) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto end;
    }

    pixels = (unsigned char *)sixel_allocator_malloc(
        chunk->allocator,
        (size_t)frame->width * (size_t)frame->height * 3);
    if (pixels == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    for (y = 0; y < frame->height; ++y) {
        src = (uint32_t const *)(row + (size_t)y * row_stride);
        for (x = 0; x < frame->width; ++x) {
            pixel = src[x];
            dst = ((size_t)y * (size_t)frame->width + (size_t)x) * 3;
            pixels[dst + 0] = (unsigned char)((pixel >> 16) & 0xff);
            pixels[dst + 1] = (unsigned char)((pixel >> 8) & 0xff);
            pixels[dst + 2] = (unsigned char)(pixel & 0xff);
        }
    }

    frame->pixelformat = SIXEL_PIXELFORMAT_RGB888;
    sixel_frame_set_pixels(frame, pixels);
    pixels = NULL;

    status = SIXEL_OK;

end:
    if (pixels != NULL) {
        sixel_allocator_free(chunk->allocator, pixels);
    }
    if (gerror != NULL) {
        g_error_free(gerror);
    }
    if (cr != NULL) {
        cairo_destroy(cr);
    }
    if (surface != NULL) {
        cairo_surface_destroy(surface);
    }
    if (handle != NULL) {
        g_object_unref(handle);
    }

    return status;
}

static SIXELSTATUS
load_with_librsvg(
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
    unsigned char opaque_bg[3];

    status = SIXEL_FALSE;
    frame = NULL;
    opaque_bg[0] = 0;
    opaque_bg[1] = 0;
    opaque_bg[2] = 0;

    (void)fstatic;
    (void)fuse_palette;
    (void)reqcolors;
    (void)loop_control;
    (void)start_frame_no_set;
    (void)start_frame_no;

    if (bgcolor != NULL) {
        opaque_bg[0] = bgcolor[0];
        opaque_bg[1] = bgcolor[1];
        opaque_bg[2] = bgcolor[2];
    }

    status = sixel_frame_new(&frame, pchunk->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = librsvg_render_to_frame(frame, pchunk, opaque_bg);
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
sixel_loader_librsvg_ref(sixel_loader_component_t *component)
{
    sixel_loader_librsvg_component_t *self;

    self = NULL;
    if (component == NULL) {
        return;
    }

    self = (sixel_loader_librsvg_component_t *)component;
    ++self->ref;
}

static void
sixel_loader_librsvg_unref(sixel_loader_component_t *component)
{
    sixel_loader_librsvg_component_t *self;
    sixel_allocator_t *allocator;

    self = NULL;
    allocator = NULL;
    if (component == NULL) {
        return;
    }

    self = (sixel_loader_librsvg_component_t *)component;
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
sixel_loader_librsvg_setopt(sixel_loader_component_t *component,
                            int option,
                            void const *value)
{
    sixel_loader_librsvg_component_t *self;
    int const *flag;
    unsigned char const *color;

    self = NULL;
    flag = NULL;
    color = NULL;
    if (component == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    self = (sixel_loader_librsvg_component_t *)component;
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
sixel_loader_librsvg_load(sixel_loader_component_t *component,
                          sixel_chunk_t const *chunk,
                          sixel_load_image_function fn_load,
                          void *context)
{
    sixel_loader_librsvg_component_t *self;
    unsigned char *bgcolor;

    self = NULL;
    bgcolor = NULL;
    if (component == NULL || chunk == NULL || fn_load == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    self = (sixel_loader_librsvg_component_t *)component;
    if (self->has_bgcolor) {
        bgcolor = self->bgcolor;
    }

    return load_with_librsvg(chunk,
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
sixel_loader_librsvg_name(sixel_loader_component_t const *component)
{
    (void)component;
    return "librsvg";
}

static sixel_loader_component_vtbl_t const g_sixel_loader_librsvg_vtbl = {
    sixel_loader_librsvg_ref,
    sixel_loader_librsvg_unref,
    sixel_loader_librsvg_setopt,
    sixel_loader_librsvg_load,
    sixel_loader_librsvg_name
};

SIXELSTATUS
sixel_loader_librsvg_new(sixel_allocator_t *allocator,
                         sixel_loader_component_t **ppcomponent)
{
    sixel_loader_librsvg_component_t *self;

    self = NULL;
    if (allocator == NULL || ppcomponent == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *ppcomponent = NULL;
    self = (sixel_loader_librsvg_component_t *)
        sixel_allocator_malloc(allocator, sizeof(*self));
    if (self == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    memset(self, 0, sizeof(*self));
    self->base.vtbl = &g_sixel_loader_librsvg_vtbl;
    self->allocator = allocator;
    self->ref = 1u;
    self->reqcolors = SIXEL_PALETTE_MAX;
    self->loop_control = SIXEL_LOOP_AUTO;
    self->start_frame_no = INT_MIN;
    sixel_allocator_ref(allocator);
    *ppcomponent = &self->base;
    return SIXEL_OK;
}

int
loader_can_try_librsvg(sixel_chunk_t const *chunk)
{
    return chunk_is_svg_like(chunk);
}

#else  /* !HAVE_LIBRSVG */

typedef int loader_librsvg_disabled;

#endif  /* HAVE_LIBRSVG */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
