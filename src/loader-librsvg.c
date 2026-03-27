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
 * librsvg-backed SVG loader. The backend rasterizes SVG through Cairo and
 * emits RGB/RGBA frames so the downstream pipeline can preserve
 * transparency while delegating palette generation to later quantization.
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
#if HAVE_LIMITS_H
# include <limits.h>
#endif

#include <librsvg/rsvg.h>
#include <cairo.h>

#include "allocator.h"
#include "compat_stub.h"
#include "frame.h"
#include "loader-common.h"
#include "loader-librsvg.h"
#include "status.h"

typedef struct sixel_loader_librsvg_component {
    sixel_loader_component_t base;
    sixel_allocator_t *allocator;
    unsigned int ref;
    int fstatic;
    unsigned char bgcolor[3];
    int has_bgcolor;
    int loop_control;
    int has_start_frame_no;
    int start_frame_no;
} sixel_loader_librsvg_component_t;

#define LIBRSVG_DEFAULT_WIDTH  300
#define LIBRSVG_DEFAULT_HEIGHT 150
#define LIBRSVG_DEFAULT_DPI    90.0
#define LIBRSVG_MAX_DIMENSION  32767
#define LIBRSVG_MAX_IMAGE_PIXELS ((size_t)268435456u)
#define LIBRSVG_ENV_ALLOW_RELATIVE_RESOURCES \
    "SIXEL_LOADER_LIBRSVG_ALLOW_RELATIVE_RESOURCES"

static void
librsvg_set_error_message(char const *context, GError const *gerror)
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

static int
librsvg_equals_nocase(char const *lhs, char const *rhs)
{
    unsigned char lch;
    unsigned char rch;

    lch = 0u;
    rch = 0u;
    if (lhs == NULL || rhs == NULL) {
        return 0;
    }

    while (*lhs != '\0' && *rhs != '\0') {
        lch = (unsigned char)*lhs;
        rch = (unsigned char)*rhs;
        if (tolower(lch) != tolower(rch)) {
            return 0;
        }
        ++lhs;
        ++rhs;
    }

    return *lhs == '\0' && *rhs == '\0';
}

static int
librsvg_env_is_enabled(char const *name)
{
    char const *value;
    size_t index;

    value = NULL;
    index = 0u;
    if (name == NULL) {
        return 0;
    }

    value = sixel_compat_getenv(name);
    if (value == NULL) {
        return 0;
    }
    while (value[index] != '\0' &&
           isspace((unsigned char)value[index]) != 0) {
        ++index;
    }
    if (value[index] == '\0') {
        return 0;
    }
    if (librsvg_equals_nocase(value + index, "0") ||
            librsvg_equals_nocase(value + index, "off") ||
            librsvg_equals_nocase(value + index, "false") ||
            librsvg_equals_nocase(value + index, "no")) {
        return 0;
    }
    return 1;
}

static int
librsvg_path_is_local_file(char const *path)
{
    if (path == NULL || path[0] == '\0' || strcmp(path, "-") == 0) {
        return 0;
    }
    if (strstr(path, "://") != NULL) {
        return 0;
    }
    return 1;
}

static int
librsvg_path_has_suffix_nocase(char const *path, char const *suffix)
{
    size_t path_len;
    size_t suffix_len;
    size_t index;

    path_len = 0u;
    suffix_len = 0u;
    index = 0u;
    if (path == NULL || suffix == NULL) {
        return 0;
    }

    path_len = strlen(path);
    suffix_len = strlen(suffix);
    if (path_len < suffix_len) {
        return 0;
    }

    for (index = 0u; index < suffix_len; ++index) {
        if (tolower((unsigned char)path[path_len - suffix_len + index]) !=
                tolower((unsigned char)suffix[index])) {
            return 0;
        }
    }

    return 1;
}

static int
librsvg_is_svgz_chunk(sixel_chunk_t const *chunk)
{
    if (chunk == NULL || chunk->buffer == NULL || chunk->size < 2u) {
        return 0;
    }
    return chunk->buffer[0] == 0x1fu && chunk->buffer[1] == 0x8bu;
}

static int
librsvg_length_to_pixels(RsvgLength const *length, double *pixels)
{
    if (length == NULL || pixels == NULL) {
        return 0;
    }

    switch (length->unit) {
    case RSVG_UNIT_PX:
        *pixels = length->length;
        return 1;
    case RSVG_UNIT_IN:
        *pixels = length->length * LIBRSVG_DEFAULT_DPI;
        return 1;
    case RSVG_UNIT_CM:
        *pixels = length->length * LIBRSVG_DEFAULT_DPI / 2.54;
        return 1;
    case RSVG_UNIT_MM:
        *pixels = length->length * LIBRSVG_DEFAULT_DPI / 25.4;
        return 1;
    case RSVG_UNIT_PT:
        *pixels = length->length * LIBRSVG_DEFAULT_DPI / 72.0;
        return 1;
    case RSVG_UNIT_PC:
        *pixels = length->length * LIBRSVG_DEFAULT_DPI / 6.0;
        return 1;
    default:
        return 0;
    }
}

static int
librsvg_rounded_dimension(double value)
{
    if (!(value >= 1.0) || value > (double)INT_MAX) {
        return 0;
    }

    return (int)(value + 0.5);
}

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
    if (chunk->source_path != NULL &&
            librsvg_path_has_suffix_nocase(chunk->source_path, ".svgz")) {
        return 1;
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
    gboolean has_width;
    gboolean has_height;
    gboolean has_viewbox;
    RsvgLength width_length;
    RsvgLength height_length;
    RsvgRectangle viewbox;
    gdouble pixel_width;
    gdouble pixel_height;
    double width_from_length;
    double height_from_length;
    int width_valid;
    int height_valid;
    int width;
    int height;

    has_width = FALSE;
    has_height = FALSE;
    width = LIBRSVG_DEFAULT_WIDTH;
    height = LIBRSVG_DEFAULT_HEIGHT;
    pixel_width = 0.0;
    pixel_height = 0.0;
    width_length.length = 0.0;
    width_length.unit = RSVG_UNIT_PX;
    height_length.length = 0.0;
    height_length.unit = RSVG_UNIT_PX;
    has_viewbox = FALSE;
    viewbox.x = 0.0;
    viewbox.y = 0.0;
    viewbox.width = 0.0;
    viewbox.height = 0.0;
    width_from_length = 0.0;
    height_from_length = 0.0;
    width_valid = 0;
    height_valid = 0;

    if (rsvg_handle_get_intrinsic_size_in_pixels(handle,
                                                  &pixel_width,
                                                  &pixel_height)) {
        width = librsvg_rounded_dimension(pixel_width);
        height = librsvg_rounded_dimension(pixel_height);
    } else {
        rsvg_handle_get_intrinsic_dimensions(handle,
                                             &has_width,
                                             &width_length,
                                             &has_height,
                                             &height_length,
                                             &has_viewbox,
                                             &viewbox);
        if (has_width && librsvg_length_to_pixels(&width_length,
                                                  &width_from_length) &&
            width_from_length >= 1.0) {
            width_valid = 1;
        }
        if (has_height && librsvg_length_to_pixels(&height_length,
                                                   &height_from_length) &&
            height_from_length >= 1.0) {
            height_valid = 1;
        }
        if (has_viewbox && viewbox.width > 0.0 && viewbox.height > 0.0) {
            if (width_valid && !height_valid) {
                height_from_length =
                    width_from_length * viewbox.height / viewbox.width;
                height_valid = height_from_length >= 1.0 ? 1 : 0;
            } else if (!width_valid && height_valid) {
                width_from_length =
                    height_from_length * viewbox.width / viewbox.height;
                width_valid = width_from_length >= 1.0 ? 1 : 0;
            } else if (!width_valid && !height_valid) {
                width_from_length = viewbox.width;
                height_from_length = viewbox.height;
                width_valid = width_from_length >= 1.0 ? 1 : 0;
                height_valid = height_from_length >= 1.0 ? 1 : 0;
            }
        }
        if (width_valid) {
            width = librsvg_rounded_dimension(width_from_length);
        }
        if (height_valid) {
            height = librsvg_rounded_dimension(height_from_length);
        }
        if (width <= 0 || height <= 0) {
            if (has_viewbox && viewbox.width > 0.0 && viewbox.height > 0.0) {
                int viewbox_width;
                int viewbox_height;

                viewbox_width = librsvg_rounded_dimension(viewbox.width);
                viewbox_height = librsvg_rounded_dimension(viewbox.height);
                if (viewbox_width > 0) {
                    width = viewbox_width;
                }
                if (viewbox_height > 0) {
                    height = viewbox_height;
                }
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

static unsigned char
librsvg_unpremultiply_channel(unsigned int value, unsigned int alpha)
{
    unsigned int unpremultiplied;

    if (alpha == 0u) {
        return 0u;
    }
    if (alpha >= 255u) {
        return (unsigned char)value;
    }

    unpremultiplied = (value * 255u + alpha / 2u) / alpha;
    if (unpremultiplied > 255u) {
        unpremultiplied = 255u;
    }

    return (unsigned char)unpremultiplied;
}

static SIXELSTATUS
librsvg_render_to_frame(sixel_frame_t *frame,
                        sixel_chunk_t const *chunk,
                        unsigned char const *bgcolor,
                        int allow_relative_resources)
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
    size_t output_stride;
    size_t pixel_total;
    size_t buffer_size;
    size_t pixel_index;
    int x;
    int y;
    uint32_t const *src;
    uint32_t pixel;
    size_t dst;
    unsigned int alpha;
    unsigned int red;
    unsigned int green;
    unsigned int blue;
    int preserve_alpha;
    int inspect_alpha;
    int has_non_opaque_alpha;
    int use_source_file;
    int input_is_svgz;

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
    output_stride = 0u;
    pixel_total = 0u;
    buffer_size = 0u;
    pixel_index = 0u;
    alpha = 0u;
    red = 0u;
    green = 0u;
    blue = 0u;
    preserve_alpha = 0;
    inspect_alpha = 0;
    has_non_opaque_alpha = 0;
    use_source_file = 0;
    input_is_svgz = 0;

    input_is_svgz = librsvg_is_svgz_chunk(chunk);
    use_source_file = librsvg_path_is_local_file(chunk->source_path) &&
                      (allow_relative_resources ||
                       (input_is_svgz &&
                        librsvg_path_has_suffix_nocase(chunk->source_path,
                                                       ".svgz")));

    if (use_source_file) {
        handle = rsvg_handle_new_from_file(chunk->source_path, &gerror);
        if (handle == NULL) {
            librsvg_set_error_message(
                "librsvg_render_to_frame: unable to parse SVG file.",
                gerror);
            status = SIXEL_BAD_INPUT;
            goto end;
        }
    } else {
        if (input_is_svgz) {
            sixel_helper_set_additional_message(
                "librsvg_render_to_frame: gzip-compressed SVG (.svgz) "
                "requires file-path decode or prior decompression.");
            status = SIXEL_BAD_INPUT;
            goto end;
        }

        handle = rsvg_handle_new_from_data(chunk->buffer, chunk->size, &gerror);
        if (handle == NULL) {
            librsvg_set_error_message(
                "librsvg_render_to_frame: unable to parse SVG data.",
                gerror);
            status = SIXEL_BAD_INPUT;
            goto end;
        }
    }

    librsvg_pick_size(handle, &frame->width, &frame->height);
    if (frame->width <= 0 || frame->height <= 0) {
        sixel_helper_set_additional_message(
            "librsvg_render_to_frame: invalid dimensions.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (frame->width > LIBRSVG_MAX_DIMENSION ||
            frame->height > LIBRSVG_MAX_DIMENSION) {
        sixel_helper_set_additional_message(
            "librsvg_render_to_frame: dimensions exceed limit.");
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto end;
    }
    if ((size_t)frame->width > SIZE_MAX / (size_t)frame->height) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto end;
    }
    pixel_total = (size_t)frame->width * (size_t)frame->height;
    if (pixel_total > LIBRSVG_MAX_IMAGE_PIXELS) {
        sixel_helper_set_additional_message(
            "librsvg_render_to_frame: image exceeds pixel limit.");
        status = SIXEL_BAD_INTEGER_OVERFLOW;
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

    if (bgcolor != NULL) {
        cairo_set_source_rgb(cr,
                             ((double)bgcolor[0]) / 255.0,
                             ((double)bgcolor[1]) / 255.0,
                             ((double)bgcolor[2]) / 255.0);
        cairo_paint(cr);
    } else {
        cairo_save(cr);
        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
        cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0);
        cairo_paint(cr);
        cairo_restore(cr);
    }

    viewport.width = (double)frame->width;
    viewport.height = (double)frame->height;
    if (!rsvg_handle_render_document(handle, cr, &viewport, &gerror)) {
        librsvg_set_error_message(
            "librsvg_render_to_frame: rsvg_handle_render_document failed.",
            gerror);
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    cairo_surface_flush(surface);
    row = cairo_image_surface_get_data(surface);
    row_stride = (size_t)cairo_image_surface_get_stride(surface);

    inspect_alpha = bgcolor == NULL ? 1 : 0;
    output_stride = inspect_alpha ? 4u : 3u;

    if (pixel_total > SIZE_MAX / output_stride) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto end;
    }
    buffer_size = pixel_total * output_stride;

    pixels = (unsigned char *)sixel_allocator_malloc(
        chunk->allocator,
        buffer_size);
    if (pixels == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    for (y = 0; y < frame->height; ++y) {
        src = (uint32_t const *)(row + (size_t)y * row_stride);
        for (x = 0; x < frame->width; ++x) {
            pixel_index = (size_t)y * (size_t)frame->width + (size_t)x;
            pixel = src[x];
            alpha = (pixel >> 24) & 0xffu;
            red = (pixel >> 16) & 0xffu;
            green = (pixel >> 8) & 0xffu;
            blue = pixel & 0xffu;
            if (inspect_alpha) {
                if (alpha != 255u) {
                    has_non_opaque_alpha = 1;
                }
                dst = pixel_index * 4u;
                pixels[dst + 0] = librsvg_unpremultiply_channel(red, alpha);
                pixels[dst + 1] = librsvg_unpremultiply_channel(green, alpha);
                pixels[dst + 2] = librsvg_unpremultiply_channel(blue, alpha);
                pixels[dst + 3] = (unsigned char)alpha;
            } else {
                dst = pixel_index * 3u;
                pixels[dst + 0] = (unsigned char)red;
                pixels[dst + 1] = (unsigned char)green;
                pixels[dst + 2] = (unsigned char)blue;
            }
        }
    }

    preserve_alpha = inspect_alpha && has_non_opaque_alpha;
    if (inspect_alpha && !has_non_opaque_alpha) {
        for (pixel_index = 0u; pixel_index < pixel_total; ++pixel_index) {
            pixels[pixel_index * 3u + 0u] = pixels[pixel_index * 4u + 0u];
            pixels[pixel_index * 3u + 1u] = pixels[pixel_index * 4u + 1u];
            pixels[pixel_index * 3u + 2u] = pixels[pixel_index * 4u + 2u];
        }
    }

    frame->pixelformat = preserve_alpha
        ? SIXEL_PIXELFORMAT_RGBA8888
        : SIXEL_PIXELFORMAT_RGB888;
    frame->colorspace = SIXEL_COLORSPACE_GAMMA;
    frame->transparent = -1;
    frame->alpha_zero_is_transparent = preserve_alpha ? 1 : 0;
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
    unsigned char             /* in */     *bgcolor,
    int                       /* in */     loop_control,
    int                       /* in */     start_frame_no_set,
    int                       /* in */     start_frame_no,
    sixel_load_image_function /* in */     fn_load,
    void                      /* in/out */ *context)
{
    SIXELSTATUS status;
    sixel_frame_t *frame;
    int allow_relative_resources;

    status = SIXEL_FALSE;
    frame = NULL;
    allow_relative_resources =
        librsvg_env_is_enabled(LIBRSVG_ENV_ALLOW_RELATIVE_RESOURCES);

    (void)fstatic;
    (void)loop_control;
    (void)start_frame_no_set;
    (void)start_frame_no;

    status = sixel_frame_new(&frame, pchunk->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = librsvg_render_to_frame(frame,
                                     pchunk,
                                     bgcolor,
                                     allow_relative_resources);
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
        sixel_debugf("librsvg loader: USE_PALETTE=%d ignored; "
                     "output remains RGB/RGBA.",
                     flag != NULL ? *flag : 0);
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_REQCOLORS:
        flag = (int const *)value;
        sixel_debugf("librsvg loader: REQCOLORS=%d ignored; "
                     "palette limits apply during quantization.",
                     flag != NULL ? *flag : 0);
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
