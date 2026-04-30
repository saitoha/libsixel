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

#include "loader-librsvg.h"

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
#include "frame-private.h"
#include "frame-factory.h"
#include "loader-common.h"
#include "status.h"

typedef struct sixel_loader_librsvg_component {
    sixel_loader_component_t base;
    sixel_allocator_t *allocator;
    unsigned int ref;
    unsigned char bgcolor[3];
    int has_bgcolor;
} sixel_loader_librsvg_component_t;

typedef enum sixel_librsvg_decode_mode {
    SIXEL_LIBRSVG_DECODE_MODE_FILE,
    SIXEL_LIBRSVG_DECODE_MODE_DATA,
    SIXEL_LIBRSVG_DECODE_MODE_STDIN_SVGZ_TEMPFILE,
    SIXEL_LIBRSVG_DECODE_MODE_STDIN_SVGZ_REJECTED
} sixel_librsvg_decode_mode_t;

static int
loader_can_try_librsvg(sixel_chunk_t const *chunk);

#define LIBRSVG_DEFAULT_WIDTH  300
#define LIBRSVG_DEFAULT_HEIGHT 150
#define LIBRSVG_DEFAULT_DPI    90.0
#define LIBRSVG_MAX_DIMENSION  32767
#define LIBRSVG_MAX_IMAGE_PIXELS ((size_t)268435456u)
#define LIBRSVG_ENV_ALLOW_RELATIVE_RESOURCES \
    "SIXEL_LOADER_LIBRSVG_ALLOW_RELATIVE_RESOURCES"
#define LIBRSVG_ENV_ALLOW_STDIN_SVGZ \
    "SIXEL_LOADER_LIBRSVG_ALLOW_STDIN_SVGZ"
#define LIBRSVG_ENV_TEST_FAIL_TEMP_SVGZ_OPEN \
    "SIXEL_LOADER_LIBRSVG_TEST_FAIL_TEMP_SVGZ_OPEN"
#define LIBRSVG_ENV_TEST_FAIL_TEMP_SVGZ_WRITE \
    "SIXEL_LOADER_LIBRSVG_TEST_FAIL_TEMP_SVGZ_WRITE"
#define LIBRSVG_ENV_TEST_FAIL_TEMP_SVGZ_CLOSE \
    "SIXEL_LOADER_LIBRSVG_TEST_FAIL_TEMP_SVGZ_CLOSE"
#define LIBRSVG_CONTEXT_PARSE_FILE \
    "librsvg_render_to_frame: unable to parse SVG file."
#define LIBRSVG_CONTEXT_PARSE_DATA \
    "librsvg_render_to_frame: unable to parse SVG data."
#define LIBRSVG_CONTEXT_PARSE_STDIN_SVGZ_TEMPFILE \
    "librsvg_render_to_frame: unable to parse stdin .svgz via " \
    "temporary file."
#define LIBRSVG_CONTEXT_RENDER_DOCUMENT_FAILED \
    "librsvg_render_to_frame: rsvg_handle_render_document failed."
#define LIBRSVG_CONTEXT_TEMP_SVGZ_OPEN_FAILED \
    "librsvg_write_chunk_to_temp_svgz: g_file_open_tmp failed."
#define LIBRSVG_MESSAGE_STDIN_SVGZ_REJECTED \
    "librsvg_render_to_frame: gzip-compressed SVG (.svgz) " \
    "requires file-path decode, prior decompression, or " \
    "SIXEL_LOADER_LIBRSVG_ALLOW_STDIN_SVGZ=1."
#define LIBRSVG_MESSAGE_UNSUPPORTED_DECODE_MODE \
    "librsvg_render_to_frame: unsupported decode mode."
#define LIBRSVG_MESSAGE_TRACE_DECODE_MODE_UNKNOWN \
    "librsvg: decode_mode=unknown"
#define LIBRSVG_MESSAGE_TEMP_SVGZ_WRITE_FAILED \
    "librsvg_write_chunk_to_temp_svgz: " \
    "failed to write temporary .svgz file."
#define LIBRSVG_MESSAGE_TEMP_SVGZ_CLOSE_FAILED \
    "librsvg_write_chunk_to_temp_svgz: " \
    "failed to close temporary .svgz file."
#define LIBRSVG_MESSAGE_TEMP_SVGZ_EMPTY_INPUT \
    "librsvg_write_chunk_to_temp_svgz: empty input."
#define LIBRSVG_MESSAGE_INVALID_DIMENSIONS \
    "librsvg_render_to_frame: invalid dimensions."
#define LIBRSVG_MESSAGE_DIMENSIONS_EXCEED_LIMIT \
    "librsvg_render_to_frame: dimensions exceed limit."
#define LIBRSVG_MESSAGE_DIMENSIONS_OVERFLOW \
    "librsvg_render_to_frame: dimensions overflow pixel count."
#define LIBRSVG_MESSAGE_IMAGE_EXCEEDS_PIXEL_LIMIT \
    "librsvg_render_to_frame: image exceeds pixel limit."
#define LIBRSVG_MESSAGE_CAIRO_SURFACE_CREATE_FAILED \
    "librsvg_render_to_frame: cairo_image_surface_create failed."
#define LIBRSVG_MESSAGE_CAIRO_CREATE_FAILED \
    "librsvg_render_to_frame: cairo_create failed."
#define LIBRSVG_MESSAGE_CAIRO_SURFACE_ACCESS_FAILED \
    "librsvg_render_to_frame: cairo surface access failed."

typedef struct sixel_librsvg_open_result {
    RsvgHandle *handle;
    char *stdin_svgz_temp_path;
} sixel_librsvg_open_result_t;

typedef struct sixel_librsvg_decode_policy {
    int allow_relative_resources;
    int allow_stdin_svgz;
} sixel_librsvg_decode_policy_t;

typedef struct sixel_librsvg_render_context {
    sixel_librsvg_open_result_t open_result;
    cairo_surface_t *surface;
    cairo_t *cr;
    size_t pixel_total;
} sixel_librsvg_render_context_t;

typedef struct sixel_librsvg_render_request {
    sixel_chunk_t const *chunk;
    sixel_allocator_t *allocator;
    unsigned char const *bgcolor;
    sixel_librsvg_decode_policy_t const *policy;
} sixel_librsvg_render_request_t;

typedef struct sixel_librsvg_intrinsic_size_state {
    gboolean has_width;
    gboolean has_height;
    gboolean has_viewbox;
    RsvgLength width_length;
    RsvgLength height_length;
    RsvgRectangle viewbox;
    double width_from_length;
    double height_from_length;
    int has_positive_viewbox;
    int width_valid;
    int height_valid;
    int resolved_width;
    int resolved_height;
} sixel_librsvg_intrinsic_size_state_t;

typedef struct sixel_librsvg_surface_convert_plan {
    int inspect_alpha;
    size_t buffer_size;
} sixel_librsvg_surface_convert_plan_t;

typedef RsvgHandle *(*sixel_librsvg_handle_builder_t)(
    void const *source,
    size_t size,
    GError **gerror);

typedef SIXELSTATUS (*sixel_librsvg_open_result_fn_t)(
    sixel_chunk_t const *chunk,
    sixel_librsvg_open_result_t *open_result);

typedef struct sixel_librsvg_open_dispatch {
    sixel_librsvg_decode_mode_t mode;
    sixel_librsvg_open_result_fn_t fn;
} sixel_librsvg_open_dispatch_t;

typedef SIXELSTATUS (*sixel_librsvg_setopt_handler_t)(
    sixel_loader_librsvg_component_t *self,
    void const *value,
    char const *name,
    char const *detail);

typedef struct sixel_librsvg_setopt_spec {
    int option;
    sixel_librsvg_setopt_handler_t handler;
    char const *name;
    char const *detail;
} sixel_librsvg_setopt_spec_t;

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

static SIXELSTATUS
librsvg_fail_with_gerror(SIXELSTATUS status,
                         char const *context,
                         GError const *gerror)
{
    librsvg_set_error_message(context, gerror);
    return status;
}

static void
librsvg_free_gerror(GError **gerror)
{
    if (gerror == NULL || *gerror == NULL) {
        return;
    }
    g_error_free(*gerror);
    *gerror = NULL;
}

static void
librsvg_unref_handle(RsvgHandle **handle)
{
    if (handle == NULL || *handle == NULL) {
        return;
    }
    g_object_unref(*handle);
    *handle = NULL;
}

static void
librsvg_destroy_cairo_context(cairo_t **cr)
{
    if (cr == NULL || *cr == NULL) {
        return;
    }
    cairo_destroy(*cr);
    *cr = NULL;
}

static void
librsvg_destroy_cairo_surface(cairo_surface_t **surface)
{
    if (surface == NULL || *surface == NULL) {
        return;
    }
    cairo_surface_destroy(*surface);
    *surface = NULL;
}

static int
librsvg_span_equals_nocase(char const *value,
                           size_t begin,
                           size_t end,
                           char const *token)
{
    size_t index;
    size_t token_length;

    index = 0u;
    token_length = 0u;
    if (value == NULL || token == NULL || end < begin) {
        return 0;
    }
    token_length = strlen(token);
    if (end - begin != token_length) {
        return 0;
    }

    for (index = 0u; index < token_length; ++index) {
        if (tolower((unsigned char)value[begin + index]) !=
                tolower((unsigned char)token[index])) {
            return 0;
        }
    }
    return 1;
}

/*
 * Treat common textual false values as disabled flags.
 */
static int
librsvg_span_is_falsey(char const *value, size_t begin, size_t end)
{
    if (value == NULL || end <= begin) {
        return 0;
    }
    if (librsvg_span_equals_nocase(value, begin, end, "0") ||
            librsvg_span_equals_nocase(value, begin, end, "off") ||
            librsvg_span_equals_nocase(value, begin, end, "false") ||
            librsvg_span_equals_nocase(value, begin, end, "no")) {
        return 1;
    }
    return 0;
}

static int
librsvg_env_is_enabled(char const *name)
{
    char const *value;
    size_t begin;
    size_t end;

    value = NULL;
    begin = 0u;
    end = 0u;
    if (name == NULL) {
        return 0;
    }

    value = sixel_compat_getenv(name);
    if (value == NULL) {
        return 0;
    }
    while (value[begin] != '\0' &&
           isspace((unsigned char)value[begin]) != 0) {
        ++begin;
    }
    end = begin;
    while (value[end] != '\0') {
        ++end;
    }
    while (end > begin && isspace((unsigned char)value[end - 1u]) != 0) {
        --end;
    }
    if (end <= begin) {
        return 0;
    }
    if (librsvg_span_is_falsey(value, begin, end)) {
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
 * Resolve missing width/height from viewBox while preserving explicit values.
 */
static void
librsvg_fill_missing_dimensions_from_viewbox(
    double viewbox_width,
    double viewbox_height,
    double *width_from_length,
    double *height_from_length,
    int *width_valid,
    int *height_valid)
{
    if (width_from_length == NULL ||
            height_from_length == NULL ||
            width_valid == NULL ||
            height_valid == NULL) {
        return;
    }
    if (*width_valid && !*height_valid) {
        *height_from_length =
            *width_from_length * viewbox_height / viewbox_width;
        *height_valid = *height_from_length >= 1.0 ? 1 : 0;
        return;
    }
    if (!*width_valid && *height_valid) {
        *width_from_length =
            *height_from_length * viewbox_width / viewbox_height;
        *width_valid = *width_from_length >= 1.0 ? 1 : 0;
        return;
    }
    if (!*width_valid && !*height_valid) {
        *width_from_length = viewbox_width;
        *height_from_length = viewbox_height;
        *width_valid = *width_from_length >= 1.0 ? 1 : 0;
        *height_valid = *height_from_length >= 1.0 ? 1 : 0;
    }
}

/*
 * If rounded explicit values become invalid, recover from positive viewBox.
 */
static void
librsvg_recover_invalid_dimensions_from_viewbox(
    int has_positive_viewbox,
    RsvgRectangle const *viewbox,
    int *width,
    int *height)
{
    int viewbox_width;
    int viewbox_height;

    viewbox_width = 0;
    viewbox_height = 0;
    if (viewbox == NULL || width == NULL || height == NULL) {
        return;
    }
    if (*width > 0 && *height > 0) {
        return;
    }
    if (!has_positive_viewbox) {
        return;
    }

    viewbox_width = librsvg_rounded_dimension(viewbox->width);
    viewbox_height = librsvg_rounded_dimension(viewbox->height);
    if (viewbox_width > 0) {
        *width = viewbox_width;
    }
    if (viewbox_height > 0) {
        *height = viewbox_height;
    }
}

/*
 * Apply conventional SVG default viewport when computed values are invalid.
 */
static void
librsvg_apply_default_dimensions(int *width, int *height)
{
    if (width == NULL || height == NULL) {
        return;
    }
    if (*width <= 0) {
        *width = LIBRSVG_DEFAULT_WIDTH;
    }
    if (*height <= 0) {
        *height = LIBRSVG_DEFAULT_HEIGHT;
    }
}

static int
librsvg_buffer_has_svg_tag(unsigned char const *buffer,
                           size_t offset,
                           size_t limit)
{
    size_t index;

    index = 0u;
    if (buffer == NULL || offset >= limit) {
        return 0;
    }

    for (index = offset; index + 4 < limit; ++index) {
        if (buffer[index] == '<' &&
                buffer[index + 1] == 's' &&
                buffer[index + 2] == 'v' &&
                buffer[index + 3] == 'g') {
            return 1;
        }
    }

    return 0;
}

/*
 * Try direct intrinsic pixel size first.  Return 1 when librsvg reports
 * intrinsic pixels and updates width/height, otherwise return 0.
 */
static int
librsvg_try_pick_size_from_intrinsic_pixels(RsvgHandle *handle,
                                             int *width,
                                             int *height)
{
    gdouble pixel_width;
    gdouble pixel_height;
    int has_intrinsic_pixels;

    pixel_width = 0.0;
    pixel_height = 0.0;
    has_intrinsic_pixels = 0;
    if (handle == NULL || width == NULL || height == NULL) {
        return 0;
    }

    has_intrinsic_pixels = rsvg_handle_get_intrinsic_size_in_pixels(
        handle,
        &pixel_width,
        &pixel_height);
    if (!has_intrinsic_pixels) {
        return 0;
    }

    *width = librsvg_rounded_dimension(pixel_width);
    *height = librsvg_rounded_dimension(pixel_height);
    return 1;
}

static void
librsvg_intrinsic_size_state_init(
    sixel_librsvg_intrinsic_size_state_t *state,
    int base_width,
    int base_height)
{
    if (state == NULL) {
        return;
    }

    state->has_width = FALSE;
    state->has_height = FALSE;
    state->has_viewbox = FALSE;
    state->width_length.length = 0.0;
    state->width_length.unit = RSVG_UNIT_PX;
    state->height_length.length = 0.0;
    state->height_length.unit = RSVG_UNIT_PX;
    state->viewbox.x = 0.0;
    state->viewbox.y = 0.0;
    state->viewbox.width = 0.0;
    state->viewbox.height = 0.0;
    state->width_from_length = 0.0;
    state->height_from_length = 0.0;
    state->has_positive_viewbox = 0;
    state->width_valid = 0;
    state->height_valid = 0;
    state->resolved_width = base_width;
    state->resolved_height = base_height;
}

static void
librsvg_update_intrinsic_length_validity(
    sixel_librsvg_intrinsic_size_state_t *state)
{
    if (state == NULL) {
        return;
    }

    if (state->has_width &&
            librsvg_length_to_pixels(&state->width_length,
                                     &state->width_from_length) &&
            state->width_from_length >= 1.0) {
        state->width_valid = 1;
    }
    if (state->has_height &&
            librsvg_length_to_pixels(&state->height_length,
                                     &state->height_from_length) &&
            state->height_from_length >= 1.0) {
        state->height_valid = 1;
    }
}

static void
librsvg_apply_viewbox_constraints_to_intrinsic_state(
    sixel_librsvg_intrinsic_size_state_t *state)
{
    if (state == NULL) {
        return;
    }

    state->has_positive_viewbox = state->has_viewbox &&
        state->viewbox.width > 0.0 &&
        state->viewbox.height > 0.0;
    if (!state->has_positive_viewbox) {
        return;
    }

    librsvg_fill_missing_dimensions_from_viewbox(
        state->viewbox.width,
        state->viewbox.height,
        &state->width_from_length,
        &state->height_from_length,
        &state->width_valid,
        &state->height_valid);
}

static void
librsvg_resolve_intrinsic_dimensions(
    sixel_librsvg_intrinsic_size_state_t *state)
{
    if (state == NULL) {
        return;
    }

    if (state->width_valid) {
        state->resolved_width = librsvg_rounded_dimension(
            state->width_from_length);
    }
    if (state->height_valid) {
        state->resolved_height = librsvg_rounded_dimension(
            state->height_from_length);
    }
    librsvg_recover_invalid_dimensions_from_viewbox(
        state->has_positive_viewbox,
        &state->viewbox,
        &state->resolved_width,
        &state->resolved_height);
}

/*
 * Resolve dimensions from intrinsic length units and viewBox relationships.
 */
static void
librsvg_pick_size_from_intrinsic_dimensions(RsvgHandle *handle,
                                            int *width,
                                            int *height)
{
    sixel_librsvg_intrinsic_size_state_t state;

    if (handle == NULL || width == NULL || height == NULL) {
        return;
    }

    librsvg_intrinsic_size_state_init(&state, *width, *height);
    rsvg_handle_get_intrinsic_dimensions(handle,
                                         &state.has_width,
                                         &state.width_length,
                                         &state.has_height,
                                         &state.height_length,
                                         &state.has_viewbox,
                                         &state.viewbox);
    librsvg_update_intrinsic_length_validity(&state);
    librsvg_apply_viewbox_constraints_to_intrinsic_state(&state);
    librsvg_resolve_intrinsic_dimensions(&state);

    *width = state.resolved_width;
    *height = state.resolved_height;
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

    return librsvg_buffer_has_svg_tag(chunk->buffer, offset, limit);
}

/*
 * Determine the raster viewport used for render_document().  librsvg may lack
 * explicit pixel dimensions when only a viewBox is provided, so this helper
 * falls back to the conventional SVG viewport size.
 */
static void
librsvg_pick_size(RsvgHandle *handle, int *pwidth, int *pheight)
{
    int has_intrinsic_pixels;
    int width;
    int height;

    has_intrinsic_pixels = 0;
    width = LIBRSVG_DEFAULT_WIDTH;
    height = LIBRSVG_DEFAULT_HEIGHT;
    has_intrinsic_pixels = librsvg_try_pick_size_from_intrinsic_pixels(
        handle,
        &width,
        &height);
    if (!has_intrinsic_pixels) {
        librsvg_pick_size_from_intrinsic_dimensions(handle, &width, &height);
    }

    librsvg_apply_default_dimensions(&width, &height);

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
librsvg_write_buffer_to_fd(int fd,
                           unsigned char const *buffer,
                           size_t size)
{
    size_t offset;
    ssize_t written;

    offset = 0u;
    written = 0;
    if (fd < 0 || buffer == NULL || size == 0u) {
        return SIXEL_BAD_ARGUMENT;
    }
    /*
     * Test-only failpoint for deterministic stdin .svgz write-path coverage.
     */
    if (librsvg_env_is_enabled(LIBRSVG_ENV_TEST_FAIL_TEMP_SVGZ_WRITE)) {
        sixel_helper_set_additional_message(
            LIBRSVG_MESSAGE_TEMP_SVGZ_WRITE_FAILED);
        return SIXEL_LIBC_ERROR;
    }

    while (offset < size) {
        written = sixel_compat_write(fd, buffer + offset, size - offset);
        if (written <= 0) {
            sixel_helper_set_additional_message(
                LIBRSVG_MESSAGE_TEMP_SVGZ_WRITE_FAILED);
            return SIXEL_LIBC_ERROR;
        }
        offset += (size_t)written;
    }

    return SIXEL_OK;
}

static SIXELSTATUS
librsvg_close_temp_svgz_fd(int *fd)
{
    if (fd == NULL || *fd < 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    /*
     * Test-only failpoint for deterministic stdin .svgz close-path coverage.
     */
    if (librsvg_env_is_enabled(LIBRSVG_ENV_TEST_FAIL_TEMP_SVGZ_CLOSE)) {
        sixel_helper_set_additional_message(
            LIBRSVG_MESSAGE_TEMP_SVGZ_CLOSE_FAILED);
        *fd = (-1);
        return SIXEL_LIBC_ERROR;
    }

    if (sixel_compat_close(*fd) != 0) {
        sixel_helper_set_additional_message(
            LIBRSVG_MESSAGE_TEMP_SVGZ_CLOSE_FAILED);
        *fd = (-1);
        return SIXEL_LIBC_ERROR;
    }
    *fd = (-1);
    return SIXEL_OK;
}

static void
librsvg_dispose_temp_path(char **path)
{
    if (path == NULL || *path == NULL) {
        return;
    }
    (void)sixel_compat_unlink(*path);
    g_free(*path);
    *path = NULL;
}

/*
 * Create a temporary .svgz file path and keep the descriptor open so callers
 * can stream data before closing.
 */
static SIXELSTATUS
librsvg_open_temp_svgz_file(int *fd_out, char **path_out)
{
    GError *gerror;
    int fd;
    char *path;

    gerror = NULL;
    fd = (-1);
    path = NULL;
    if (fd_out == NULL || path_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *fd_out = (-1);
    *path_out = NULL;
    /*
     * Test-only failpoint for deterministic stdin .svgz open-path coverage.
     */
    if (librsvg_env_is_enabled(LIBRSVG_ENV_TEST_FAIL_TEMP_SVGZ_OPEN)) {
        sixel_helper_set_additional_message(
            LIBRSVG_CONTEXT_TEMP_SVGZ_OPEN_FAILED);
        return SIXEL_LIBC_ERROR;
    }

    fd = g_file_open_tmp("libsixel-librsvg-XXXXXX.svgz", &path, &gerror);
    if (fd < 0 || path == NULL) {
        librsvg_set_error_message(LIBRSVG_CONTEXT_TEMP_SVGZ_OPEN_FAILED,
                                  gerror);
        if (fd >= 0) {
            (void)sixel_compat_close(fd);
        }
        librsvg_dispose_temp_path(&path);
        librsvg_free_gerror(&gerror);
        return SIXEL_LIBC_ERROR;
    }

    *fd_out = fd;
    *path_out = path;
    librsvg_free_gerror(&gerror);
    return SIXEL_OK;
}

static SIXELSTATUS
librsvg_write_chunk_to_temp_svgz(sixel_chunk_t const *chunk, char **path_out)
{
    SIXELSTATUS status;
    int fd;
    char *path;

    fd = (-1);
    path = NULL;
    if (chunk == NULL || path_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *path_out = NULL;
    if (chunk->buffer == NULL || chunk->size == 0u) {
        sixel_helper_set_additional_message(
            LIBRSVG_MESSAGE_TEMP_SVGZ_EMPTY_INPUT);
        return SIXEL_BAD_ARGUMENT;
    }

    status = librsvg_open_temp_svgz_file(&fd, &path);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    status = librsvg_write_buffer_to_fd(fd, chunk->buffer, chunk->size);
    if (SIXEL_SUCCEEDED(status)) {
        status = librsvg_close_temp_svgz_fd(&fd);
    }
    if (SIXEL_SUCCEEDED(status)) {
        *path_out = path;
        path = NULL;
        status = SIXEL_OK;
    }
    if (fd >= 0) {
        (void)sixel_compat_close(fd);
    }
    librsvg_dispose_temp_path(&path);

    return status;
}

static char const *
librsvg_decode_mode_name(sixel_librsvg_decode_mode_t mode)
{
    switch (mode) {
    case SIXEL_LIBRSVG_DECODE_MODE_FILE:
        return "file";
    case SIXEL_LIBRSVG_DECODE_MODE_DATA:
        return "data";
    case SIXEL_LIBRSVG_DECODE_MODE_STDIN_SVGZ_TEMPFILE:
        return "stdin_svgz_tempfile";
    case SIXEL_LIBRSVG_DECODE_MODE_STDIN_SVGZ_REJECTED:
        return "stdin_svgz_rejected";
    default:
        return "unknown";
    }
}

static void
librsvg_trace_decode_mode(sixel_librsvg_decode_mode_t mode)
{
    char message[64];
    char const *mode_name;
    int written;

    mode_name = librsvg_decode_mode_name(mode);
    written = sixel_compat_snprintf(message,
                                    sizeof(message),
                                    "librsvg: decode_mode=%s",
                                    mode_name);
    if (written < 0) {
        sixel_trace_topic_message("loader",
                                  LIBRSVG_MESSAGE_TRACE_DECODE_MODE_UNKNOWN);
        return;
    }
    message[sizeof(message) - 1] = '\0';
    sixel_trace_topic_message("loader", message);
}

/*
 * Prefer file-based decode when we explicitly allow relative external
 * resources, or when the local input is .svgz and must be parsed from a file
 * path by librsvg.
 */
static int
librsvg_should_decode_from_file(sixel_chunk_t const *chunk,
                                int allow_relative_resources,
                                int input_is_svgz)
{
    if (chunk == NULL) {
        return 0;
    }
    if (!librsvg_path_is_local_file(chunk->source_path)) {
        return 0;
    }
    if (allow_relative_resources) {
        return 1;
    }
    if (input_is_svgz &&
            librsvg_path_has_suffix_nocase(chunk->source_path, ".svgz")) {
        return 1;
    }
    return 0;
}

static sixel_librsvg_decode_mode_t
librsvg_pick_decode_mode(sixel_chunk_t const *chunk,
                         sixel_librsvg_decode_policy_t const *policy)
{
    int input_is_svgz;
    int use_source_file;

    input_is_svgz = 0;
    use_source_file = 0;
    /*
     * Callers should validate arguments before use.  The fallback keeps the
     * function safe for defensive use from future call sites.
     */
    if (chunk == NULL || policy == NULL) {
        return SIXEL_LIBRSVG_DECODE_MODE_DATA;
    }

    input_is_svgz = librsvg_is_svgz_chunk(chunk);
    use_source_file = librsvg_should_decode_from_file(
        chunk,
        policy->allow_relative_resources,
        input_is_svgz);
    if (use_source_file) {
        return SIXEL_LIBRSVG_DECODE_MODE_FILE;
    }
    if (!input_is_svgz) {
        return SIXEL_LIBRSVG_DECODE_MODE_DATA;
    }
    if (!policy->allow_stdin_svgz) {
        return SIXEL_LIBRSVG_DECODE_MODE_STDIN_SVGZ_REJECTED;
    }
    return SIXEL_LIBRSVG_DECODE_MODE_STDIN_SVGZ_TEMPFILE;
}

static RsvgHandle *
librsvg_build_handle_from_file_source(void const *source,
                                      size_t size,
                                      GError **gerror)
{
    char const *path;

    (void)size;
    path = NULL;
    if (source == NULL) {
        return NULL;
    }
    path = (char const *)source;
    return rsvg_handle_new_from_file(path, gerror);
}

static RsvgHandle *
librsvg_build_handle_from_data_source(void const *source,
                                      size_t size,
                                      GError **gerror)
{
    unsigned char const *buffer;

    buffer = NULL;
    if (source == NULL) {
        return NULL;
    }
    buffer = (unsigned char const *)source;
    return rsvg_handle_new_from_data(buffer, size, gerror);
}

static SIXELSTATUS
librsvg_new_handle_common(void const *source,
                          size_t size,
                          char const *context,
                          RsvgHandle **handle_out,
                          sixel_librsvg_handle_builder_t builder)
{
    SIXELSTATUS status;
    GError *gerror;
    RsvgHandle *handle;

    gerror = NULL;
    handle = NULL;
    if (source == NULL ||
            context == NULL ||
            handle_out == NULL ||
            builder == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *handle_out = NULL;

    handle = builder(source, size, &gerror);
    if (handle != NULL) {
        *handle_out = handle;
        handle = NULL;
        status = SIXEL_OK;
    } else {
        status = librsvg_fail_with_gerror(SIXEL_BAD_INPUT, context, gerror);
    }
    librsvg_unref_handle(&handle);
    librsvg_free_gerror(&gerror);

    return status;
}

static SIXELSTATUS
librsvg_new_handle_from_file(char const *path,
                             char const *context,
                             RsvgHandle **handle_out)
{
    if (path == NULL || context == NULL || handle_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    return librsvg_new_handle_common(path,
                                     0u,
                                     context,
                                     handle_out,
                                     librsvg_build_handle_from_file_source);
}

static SIXELSTATUS
librsvg_new_handle_from_data(unsigned char const *buffer,
                             size_t size,
                             char const *context,
                             RsvgHandle **handle_out)
{
    if (buffer == NULL ||
            size == 0u ||
            context == NULL ||
            handle_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    return librsvg_new_handle_common(buffer,
                                     size,
                                     context,
                                     handle_out,
                                     librsvg_build_handle_from_data_source);
}

static SIXELSTATUS
librsvg_open_handle_from_stdin_svgz_tempfile(sixel_chunk_t const *chunk,
                                             RsvgHandle **handle_out,
                                             char **temp_path_out)
{
    SIXELSTATUS status;
    char *temp_path;

    temp_path = NULL;
    if (chunk == NULL || handle_out == NULL || temp_path_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *handle_out = NULL;
    *temp_path_out = NULL;

    status = librsvg_write_chunk_to_temp_svgz(chunk, &temp_path);
    if (SIXEL_SUCCEEDED(status)) {
        status = librsvg_new_handle_from_file(
            temp_path,
            LIBRSVG_CONTEXT_PARSE_STDIN_SVGZ_TEMPFILE,
            handle_out);
    }
    if (SIXEL_SUCCEEDED(status)) {
        *temp_path_out = temp_path;
        temp_path = NULL;
    }
    librsvg_dispose_temp_path(&temp_path);

    return status;
}

static SIXELSTATUS
librsvg_open_result_from_source_file(
    sixel_chunk_t const *chunk,
    sixel_librsvg_open_result_t *open_result)
{
    return librsvg_new_handle_from_file(chunk->source_path,
                                        LIBRSVG_CONTEXT_PARSE_FILE,
                                        &open_result->handle);
}

static SIXELSTATUS
librsvg_open_result_from_data_chunk(
    sixel_chunk_t const *chunk,
    sixel_librsvg_open_result_t *open_result)
{
    return librsvg_new_handle_from_data(chunk->buffer,
                                        chunk->size,
                                        LIBRSVG_CONTEXT_PARSE_DATA,
                                        &open_result->handle);
}

static SIXELSTATUS
librsvg_open_result_from_stdin_svgz_tempfile(
    sixel_chunk_t const *chunk,
    sixel_librsvg_open_result_t *open_result)
{
    return librsvg_open_handle_from_stdin_svgz_tempfile(
        chunk,
        &open_result->handle,
        &open_result->stdin_svgz_temp_path);
}

static SIXELSTATUS
librsvg_decode_mode_error(sixel_librsvg_decode_mode_t decode_mode)
{
    if (decode_mode == SIXEL_LIBRSVG_DECODE_MODE_STDIN_SVGZ_REJECTED) {
        sixel_helper_set_additional_message(
            LIBRSVG_MESSAGE_STDIN_SVGZ_REJECTED);
        return SIXEL_BAD_INPUT;
    }

    sixel_helper_set_additional_message(
        LIBRSVG_MESSAGE_UNSUPPORTED_DECODE_MODE);
    return SIXEL_BAD_ARGUMENT;
}

static sixel_librsvg_open_dispatch_t const g_librsvg_open_dispatch[] = {
    { SIXEL_LIBRSVG_DECODE_MODE_FILE,
      librsvg_open_result_from_source_file },
    { SIXEL_LIBRSVG_DECODE_MODE_STDIN_SVGZ_TEMPFILE,
      librsvg_open_result_from_stdin_svgz_tempfile },
    { SIXEL_LIBRSVG_DECODE_MODE_DATA,
      librsvg_open_result_from_data_chunk }
};

static sixel_librsvg_open_result_fn_t
librsvg_find_open_result_fn(sixel_librsvg_decode_mode_t decode_mode)
{
    size_t index;

    index = 0u;
    for (index = 0u;
            index < sizeof(g_librsvg_open_dispatch) /
                    sizeof(g_librsvg_open_dispatch[0]);
            ++index) {
        if (g_librsvg_open_dispatch[index].mode == decode_mode) {
            return g_librsvg_open_dispatch[index].fn;
        }
    }

    return NULL;
}

static SIXELSTATUS
librsvg_open_handle_by_mode(sixel_librsvg_decode_mode_t decode_mode,
                            sixel_chunk_t const *chunk,
                            sixel_librsvg_open_result_t *open_result)
{
    sixel_librsvg_open_result_fn_t open_result_fn;

    if (chunk == NULL || open_result == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    open_result_fn = librsvg_find_open_result_fn(decode_mode);
    if (open_result_fn != NULL) {
        return open_result_fn(chunk, open_result);
    }
    return librsvg_decode_mode_error(decode_mode);
}

static void
librsvg_open_result_init(sixel_librsvg_open_result_t *open_result)
{
    if (open_result == NULL) {
        return;
    }
    open_result->handle = NULL;
    open_result->stdin_svgz_temp_path = NULL;
}

static SIXELSTATUS
librsvg_open_handle(sixel_chunk_t const *chunk,
                    sixel_librsvg_decode_policy_t const *policy,
                    sixel_librsvg_open_result_t *open_result)
{
    sixel_librsvg_decode_mode_t decode_mode;

    if (chunk == NULL ||
            policy == NULL ||
            open_result == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    librsvg_open_result_init(open_result);
    decode_mode = librsvg_pick_decode_mode(chunk, policy);
    librsvg_trace_decode_mode(decode_mode);
    return librsvg_open_handle_by_mode(decode_mode, chunk, open_result);
}

static void
librsvg_open_result_cleanup(sixel_librsvg_open_result_t *open_result)
{
    if (open_result == NULL) {
        return;
    }
    librsvg_unref_handle(&open_result->handle);
    librsvg_dispose_temp_path(&open_result->stdin_svgz_temp_path);
}

static void
librsvg_render_context_init(sixel_librsvg_render_context_t *render_ctx)
{
    if (render_ctx == NULL) {
        return;
    }
    librsvg_open_result_init(&render_ctx->open_result);
    render_ctx->surface = NULL;
    render_ctx->cr = NULL;
    render_ctx->pixel_total = 0u;
}

static void
librsvg_render_context_cleanup(sixel_librsvg_render_context_t *render_ctx)
{
    if (render_ctx == NULL) {
        return;
    }
    librsvg_destroy_cairo_context(&render_ctx->cr);
    librsvg_destroy_cairo_surface(&render_ctx->surface);
    librsvg_open_result_cleanup(&render_ctx->open_result);
    render_ctx->pixel_total = 0u;
}

static SIXELSTATUS
librsvg_render_request_init(
    sixel_librsvg_render_request_t *request,
    sixel_chunk_t const *chunk,
    sixel_allocator_t *allocator,
    unsigned char const *bgcolor,
    sixel_librsvg_decode_policy_t const *policy)
{
    if (request == NULL ||
            chunk == NULL ||
            allocator == NULL ||
            policy == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    request->chunk = chunk;
    request->allocator = allocator;
    request->bgcolor = bgcolor;
    request->policy = policy;
    return SIXEL_OK;
}

static void
librsvg_decode_policy_init(
    sixel_librsvg_decode_policy_t *policy,
    int allow_relative_resources,
    int allow_stdin_svgz)
{
    if (policy == NULL) {
        return;
    }

    policy->allow_relative_resources = allow_relative_resources ? 1 : 0;
    policy->allow_stdin_svgz = allow_stdin_svgz ? 1 : 0;
}

static void
librsvg_decode_policy_init_from_env(sixel_librsvg_decode_policy_t *policy)
{
    librsvg_decode_policy_init(
        policy,
        librsvg_env_is_enabled(LIBRSVG_ENV_ALLOW_RELATIVE_RESOURCES),
        librsvg_env_is_enabled(LIBRSVG_ENV_ALLOW_STDIN_SVGZ));
}

static SIXELSTATUS
librsvg_validate_canvas_size(int width, int height, size_t *pixel_total_out)
{
    size_t pixel_total;

    pixel_total = 0u;
    if (pixel_total_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (width <= 0 || height <= 0) {
        sixel_helper_set_additional_message(
            LIBRSVG_MESSAGE_INVALID_DIMENSIONS);
        return SIXEL_BAD_INPUT;
    }
    if (width > LIBRSVG_MAX_DIMENSION || height > LIBRSVG_MAX_DIMENSION) {
        sixel_helper_set_additional_message(
            LIBRSVG_MESSAGE_DIMENSIONS_EXCEED_LIMIT);
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    if ((size_t)width > SIZE_MAX / (size_t)height) {
        sixel_helper_set_additional_message(
            LIBRSVG_MESSAGE_DIMENSIONS_OVERFLOW);
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    pixel_total = (size_t)width * (size_t)height;
    if (pixel_total > LIBRSVG_MAX_IMAGE_PIXELS) {
        sixel_helper_set_additional_message(
            LIBRSVG_MESSAGE_IMAGE_EXCEEDS_PIXEL_LIMIT);
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    *pixel_total_out = pixel_total;
    return SIXEL_OK;
}

/*
 * Initialize the cairo target either as transparent or with a solid
 * background color before rendering SVG contents.
 */
static void
librsvg_prepare_background(cairo_t *cr, unsigned char const *bgcolor)
{
    if (cr == NULL) {
        return;
    }

    if (bgcolor == NULL) {
        cairo_save(cr);
        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
        cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0);
        cairo_paint(cr);
        cairo_restore(cr);
        return;
    }

    cairo_set_source_rgb(cr,
                         ((double)bgcolor[0]) / 255.0,
                         ((double)bgcolor[1]) / 255.0,
                         ((double)bgcolor[2]) / 255.0);
    cairo_paint(cr);
}

static SIXELSTATUS
librsvg_prepare_render_surface(cairo_surface_t **surface_out,
                               cairo_t **cr_out,
                               int width,
                               int height,
                               unsigned char const *bgcolor)
{
    cairo_surface_t *surface;
    cairo_t *cr;
    cairo_status_t cairo_stat;

    surface = NULL;
    cr = NULL;
    cairo_stat = CAIRO_STATUS_SUCCESS;
    if (surface_out == NULL || cr_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (width <= 0 || height <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    *surface_out = NULL;
    *cr_out = NULL;

    surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cairo_stat = cairo_surface_status(surface);
    if (cairo_stat != CAIRO_STATUS_SUCCESS) {
        sixel_helper_set_additional_message(
            LIBRSVG_MESSAGE_CAIRO_SURFACE_CREATE_FAILED);
        librsvg_destroy_cairo_surface(&surface);
        return SIXEL_BAD_ALLOCATION;
    }

    cr = cairo_create(surface);
    cairo_stat = cairo_status(cr);
    if (cairo_stat != CAIRO_STATUS_SUCCESS) {
        sixel_helper_set_additional_message(
            LIBRSVG_MESSAGE_CAIRO_CREATE_FAILED);
        librsvg_destroy_cairo_context(&cr);
        librsvg_destroy_cairo_surface(&surface);
        return SIXEL_BAD_ALLOCATION;
    }

    librsvg_prepare_background(cr, bgcolor);

    *surface_out = surface;
    *cr_out = cr;
    return SIXEL_OK;
}

static SIXELSTATUS
librsvg_render_document(RsvgHandle *handle, cairo_t *cr, int width, int height)
{
    SIXELSTATUS status;
    GError *gerror;
    RsvgRectangle viewport;

    gerror = NULL;
    if (handle == NULL || cr == NULL || width <= 0 || height <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    viewport.x = 0.0;
    viewport.y = 0.0;
    viewport.width = (double)width;
    viewport.height = (double)height;
    if (!rsvg_handle_render_document(handle, cr, &viewport, &gerror)) {
        status = librsvg_fail_with_gerror(
            SIXEL_BAD_INPUT,
            LIBRSVG_CONTEXT_RENDER_DOCUMENT_FAILED,
            gerror);
    } else {
        status = SIXEL_OK;
    }
    librsvg_free_gerror(&gerror);

    return status;
}

static SIXELSTATUS
librsvg_unpack_surface_argb32_pixel(uint32_t pixel,
                                    int inspect_alpha,
                                    unsigned char *dst,
                                    int *has_non_opaque_alpha_out)
{
    unsigned int alpha;
    unsigned int red;
    unsigned int green;
    unsigned int blue;

    alpha = 0u;
    red = 0u;
    green = 0u;
    blue = 0u;
    if (dst == NULL || has_non_opaque_alpha_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    alpha = (pixel >> 24) & 0xffu;
    red = (pixel >> 16) & 0xffu;
    green = (pixel >> 8) & 0xffu;
    blue = pixel & 0xffu;
    if (inspect_alpha != 0) {
        if (alpha != 255u) {
            *has_non_opaque_alpha_out = 1;
        }
        dst[0] = librsvg_unpremultiply_channel(red, alpha);
        dst[1] = librsvg_unpremultiply_channel(green, alpha);
        dst[2] = librsvg_unpremultiply_channel(blue, alpha);
        dst[3] = (unsigned char)alpha;
        return SIXEL_OK;
    }

    dst[0] = (unsigned char)red;
    dst[1] = (unsigned char)green;
    dst[2] = (unsigned char)blue;
    return SIXEL_OK;
}

static SIXELSTATUS
librsvg_unpack_surface_pixels(unsigned char *pixels,
                              unsigned char const *row,
                              size_t row_stride,
                              int width,
                              int height,
                              int inspect_alpha,
                              int *has_non_opaque_alpha_out)
{
    size_t pixel_index;
    size_t output_stride;
    int x;
    int y;
    uint32_t const *src;
    uint32_t pixel;
    size_t dst;
    int has_non_opaque_alpha;
    SIXELSTATUS status;

    pixel_index = 0u;
    output_stride = 0u;
    x = 0;
    y = 0;
    src = NULL;
    pixel = 0u;
    dst = 0u;
    has_non_opaque_alpha = 0;
    status = SIXEL_FALSE;
    if (pixels == NULL ||
            row == NULL ||
            width <= 0 ||
            height <= 0 ||
            has_non_opaque_alpha_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    output_stride = inspect_alpha != 0 ? 4u : 3u;

    for (y = 0; y < height; ++y) {
        src = (uint32_t const *)(row + (size_t)y * row_stride);
        for (x = 0; x < width; ++x) {
            pixel_index = (size_t)y * (size_t)width + (size_t)x;
            pixel = src[x];
            dst = pixel_index * output_stride;
            status = librsvg_unpack_surface_argb32_pixel(
                pixel,
                inspect_alpha,
                pixels + dst,
                &has_non_opaque_alpha);
            if (SIXEL_FAILED(status)) {
                return status;
            }
        }
    }

    *has_non_opaque_alpha_out = has_non_opaque_alpha;
    return SIXEL_OK;
}

static void
librsvg_collapse_opaque_rgba_to_rgb(unsigned char *pixels, size_t pixel_total)
{
    size_t pixel_index;

    pixel_index = 0u;
    if (pixels == NULL) {
        return;
    }

    for (pixel_index = 0u; pixel_index < pixel_total; ++pixel_index) {
        pixels[pixel_index * 3u + 0u] = pixels[pixel_index * 4u + 0u];
        pixels[pixel_index * 3u + 1u] = pixels[pixel_index * 4u + 1u];
        pixels[pixel_index * 3u + 2u] = pixels[pixel_index * 4u + 2u];
    }
}

static SIXELSTATUS
librsvg_build_surface_convert_plan(
    unsigned char const *bgcolor,
    size_t pixel_total,
    sixel_librsvg_surface_convert_plan_t *plan)
{
    int inspect_alpha;
    size_t output_stride;

    if (plan == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    inspect_alpha = bgcolor == NULL ? 1 : 0;
    output_stride = inspect_alpha ? 4u : 3u;
    if (pixel_total > SIZE_MAX / output_stride) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    plan->inspect_alpha = inspect_alpha;
    plan->buffer_size = pixel_total * output_stride;
    return SIXEL_OK;
}

static void
librsvg_commit_frame_pixels(sixel_frame_t *frame,
                            unsigned char *pixels,
                            size_t pixel_total,
                            int inspect_alpha,
                            int has_non_opaque_alpha)
{
    int preserve_alpha;

    preserve_alpha = 0;
    if (frame == NULL || pixels == NULL) {
        return;
    }

    preserve_alpha = inspect_alpha && has_non_opaque_alpha;
    if (inspect_alpha && !has_non_opaque_alpha) {
        /*
         * Keep the single allocated buffer: collapse the temporary RGBA view
         * to RGB when the rendered image is fully opaque.
         */
        librsvg_collapse_opaque_rgba_to_rgb(pixels, pixel_total);
    }

    frame->pixelformat = preserve_alpha
        ? SIXEL_PIXELFORMAT_RGBA8888
        : SIXEL_PIXELFORMAT_RGB888;
    frame->colorspace = SIXEL_COLORSPACE_GAMMA;
    frame->transparent = -1;
    frame->alpha_zero_is_transparent = preserve_alpha ? 1 : 0;
    sixel_frame_set_pixels(frame, pixels);
}

/*
 * Expose cairo image bytes and stride for pixel unpacking.
 */
static SIXELSTATUS
librsvg_get_surface_data(cairo_surface_t *surface,
                         unsigned char const **row_out,
                         size_t *row_stride_out)
{
    unsigned char const *row;
    size_t row_stride;

    row = NULL;
    row_stride = 0u;
    if (surface == NULL || row_out == NULL || row_stride_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    cairo_surface_flush(surface);
    row = cairo_image_surface_get_data(surface);
    row_stride = (size_t)cairo_image_surface_get_stride(surface);
    if (row == NULL || row_stride == 0u) {
        sixel_helper_set_additional_message(
            LIBRSVG_MESSAGE_CAIRO_SURFACE_ACCESS_FAILED);
        return SIXEL_BAD_INPUT;
    }

    *row_out = row;
    *row_stride_out = row_stride;
    return SIXEL_OK;
}

static SIXELSTATUS
librsvg_convert_surface_to_frame_pixels(sixel_frame_t *frame,
                                        sixel_allocator_t *allocator,
                                        cairo_surface_t *surface,
                                        unsigned char const *bgcolor,
                                        size_t pixel_total)
{
    SIXELSTATUS status;
    unsigned char *pixels;
    unsigned char const *row;
    size_t row_stride;
    sixel_librsvg_surface_convert_plan_t plan;
    int has_non_opaque_alpha;

    pixels = NULL;
    row = NULL;
    row_stride = 0u;
    plan.inspect_alpha = 0;
    plan.buffer_size = 0u;
    has_non_opaque_alpha = 0;
    if (frame == NULL ||
            allocator == NULL ||
            surface == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (frame->width <= 0 || frame->height <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    status = librsvg_get_surface_data(surface, &row, &row_stride);
    if (SIXEL_SUCCEEDED(status)) {
        status = librsvg_build_surface_convert_plan(bgcolor,
                                                    pixel_total,
                                                    &plan);
    }
    if (SIXEL_SUCCEEDED(status)) {
        pixels = (unsigned char *)sixel_allocator_malloc(
            allocator,
            plan.buffer_size);
        if (pixels == NULL) {
            status = SIXEL_BAD_ALLOCATION;
        }
    }

    if (SIXEL_SUCCEEDED(status)) {
        status = librsvg_unpack_surface_pixels(pixels,
                                               row,
                                               row_stride,
                                               frame->width,
                                               frame->height,
                                               plan.inspect_alpha,
                                               &has_non_opaque_alpha);
    }

    if (SIXEL_SUCCEEDED(status)) {
        librsvg_commit_frame_pixels(frame,
                                    pixels,
                                    pixel_total,
                                    plan.inspect_alpha,
                                    has_non_opaque_alpha);
        pixels = NULL;
        status = SIXEL_OK;
    }

    if (pixels != NULL) {
        sixel_allocator_free(allocator, pixels);
    }

    return status;
}

static SIXELSTATUS
librsvg_prepare_frame_geometry(sixel_frame_t *frame,
                               RsvgHandle *handle,
                               size_t *pixel_total_out)
{
    if (frame == NULL || handle == NULL || pixel_total_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    librsvg_pick_size(handle, &frame->width, &frame->height);
    return librsvg_validate_canvas_size(frame->width,
                                        frame->height,
                                        pixel_total_out);
}

static SIXELSTATUS
librsvg_prepare_frame_surface(sixel_frame_t const *frame,
                              unsigned char const *bgcolor,
                              sixel_librsvg_render_context_t *render_ctx)
{
    if (frame == NULL || render_ctx == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    return librsvg_prepare_render_surface(&render_ctx->surface,
                                          &render_ctx->cr,
                                          frame->width,
                                          frame->height,
                                          bgcolor);
}

/*
 * Render pipeline stages:
 * 1) open source into RsvgHandle
 * 2) resolve geometry and validate canvas
 * 3) prepare cairo surface/context with optional background
 * 4) render SVG into cairo surface
 * 5) convert cairo pixels into frame RGB/RGBA buffer
 */
static SIXELSTATUS
librsvg_render_to_frame(sixel_frame_t *frame,
                        sixel_chunk_t const *chunk,
                        unsigned char const *bgcolor,
                        sixel_librsvg_decode_policy_t const *policy)
{
    SIXELSTATUS status;
    sixel_librsvg_render_context_t render_ctx;
    sixel_librsvg_render_request_t request = { NULL, NULL, NULL, NULL };

    if (frame == NULL || chunk == NULL || policy == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    librsvg_render_context_init(&render_ctx);
    status = librsvg_render_request_init(&request,
                                         chunk,
                                         chunk->allocator,
                                         bgcolor,
                                         policy);
    if (SIXEL_SUCCEEDED(status)) {
        status = librsvg_open_handle(request.chunk,
                                     request.policy,
                                     &render_ctx.open_result);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = librsvg_prepare_frame_geometry(frame,
                                                render_ctx.open_result.handle,
                                                &render_ctx.pixel_total);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = librsvg_prepare_frame_surface(frame,
                                               request.bgcolor,
                                               &render_ctx);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = librsvg_render_document(render_ctx.open_result.handle,
                                         render_ctx.cr,
                                         frame->width,
                                         frame->height);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = librsvg_convert_surface_to_frame_pixels(
            frame,
            request.allocator,
            render_ctx.surface,
            request.bgcolor,
            render_ctx.pixel_total);
    }
    librsvg_render_context_cleanup(&render_ctx);

    return status;
}

static SIXELSTATUS
load_with_librsvg(
    sixel_chunk_t const       /* in */     *pchunk,
    unsigned char const       /* in */     *bgcolor,
    sixel_load_image_function /* in */     fn_load,
    void                      /* in/out */ *context)
{
    SIXELSTATUS status;
    sixel_frame_t *frame;
    sixel_librsvg_decode_policy_t policy;

    frame = NULL;
    if (pchunk == NULL || fn_load == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    librsvg_decode_policy_init_from_env(&policy);

    status = sixel_frame_create_from_factory(&frame, pchunk->allocator);
    if (SIXEL_SUCCEEDED(status)) {
        status = librsvg_render_to_frame(frame,
                                         pchunk,
                                         bgcolor,
                                         &policy);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = fn_load(frame, context);
    }
    sixel_frame_unref(frame);

    return status;
}


static void
sixel_loader_librsvg_ref(sixel_loader_component_t *component)
{
    sixel_loader_librsvg_component_t *self;

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

/*
 * API compatibility only: SVG decode is single-frame in this backend.
 */
static SIXELSTATUS
librsvg_setopt_noop_single_frame(sixel_loader_librsvg_component_t *self,
                                 void const *value,
                                 char const *name,
                                 char const *detail)
{
    (void)self;
    (void)value;
    (void)name;
    (void)detail;
    return SIXEL_OK;
}

static SIXELSTATUS
librsvg_setopt_bgcolor(sixel_loader_librsvg_component_t *self,
                       void const *value,
                       char const *name,
                       char const *detail)
{
    unsigned char const *color;

    (void)name;
    (void)detail;
    if (self == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
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
}

static void
librsvg_debug_ignored_int_option(char const *name,
                                 int const *value,
                                 char const *detail)
{
    sixel_debugf("librsvg loader: %s=%d ignored; %s",
                 name != NULL ? name : "option",
                 value != NULL ? *value : 0,
                 detail != NULL ? detail : "");
}

/*
 * Keep option-level API compatibility while routing palette semantics to
 * quantization instead of this loader.
 */
static SIXELSTATUS
librsvg_setopt_log_ignored_int(sixel_loader_librsvg_component_t *self,
                               void const *value,
                               char const *name,
                               char const *detail)
{
    int const *flag;

    (void)self;
    flag = (int const *)value;
    librsvg_debug_ignored_int_option(name, flag, detail);
    return SIXEL_OK;
}

static sixel_librsvg_setopt_spec_t const g_librsvg_setopt_specs[] = {
    { SIXEL_LOADER_OPTION_REQUIRE_STATIC,
      librsvg_setopt_noop_single_frame,
      NULL,
      NULL },
    { SIXEL_LOADER_OPTION_LOOP_CONTROL,
      librsvg_setopt_noop_single_frame,
      NULL,
      NULL },
    { SIXEL_LOADER_OPTION_START_FRAME_NO,
      librsvg_setopt_noop_single_frame,
      NULL,
      NULL },
    { SIXEL_LOADER_OPTION_USE_PALETTE,
      librsvg_setopt_log_ignored_int,
      "USE_PALETTE",
      "output remains RGB/RGBA." },
    { SIXEL_LOADER_OPTION_REQCOLORS,
      librsvg_setopt_log_ignored_int,
      "REQCOLORS",
      "palette limits apply during quantization." },
    { SIXEL_LOADER_OPTION_BGCOLOR,
      librsvg_setopt_bgcolor,
      NULL,
      NULL }
};

static sixel_librsvg_setopt_spec_t const *
librsvg_find_setopt_spec(int option)
{
    size_t index;

    index = 0u;
    for (index = 0u;
            index < sizeof(g_librsvg_setopt_specs) /
                    sizeof(g_librsvg_setopt_specs[0]);
            ++index) {
        if (g_librsvg_setopt_specs[index].option == option) {
            return &g_librsvg_setopt_specs[index];
        }
    }
    return NULL;
}

static SIXELSTATUS
sixel_loader_librsvg_setopt(sixel_loader_component_t *component,
                            int option,
                            void const *value)
{
    sixel_loader_librsvg_component_t *self;
    sixel_librsvg_setopt_spec_t const *spec;

    if (component == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    self = (sixel_loader_librsvg_component_t *)component;
    spec = librsvg_find_setopt_spec(option);
    if (spec == NULL) {
        return SIXEL_OK;
    }
    if (spec->handler == NULL) {
        return SIXEL_OK;
    }
    return spec->handler(self, value, spec->name, spec->detail);
}

static SIXELSTATUS
sixel_loader_librsvg_load(sixel_loader_component_t *component,
                          sixel_chunk_t const *chunk,
                          sixel_load_image_function fn_load,
                          void *context)
{
    sixel_loader_librsvg_component_t *self;
    unsigned char const *bgcolor;
    SIXELSTATUS status;
    int header_job_id;
    int decode_job_id;
    sixel_loader_timeline_callback_state_t timeline_state;

    if (component == NULL || chunk == NULL || fn_load == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = SIXEL_FALSE;
    header_job_id = -1;
    decode_job_id = -1;
    self = (sixel_loader_librsvg_component_t *)component;
    bgcolor = self->has_bgcolor ? self->bgcolor : NULL;

    header_job_id = loader_timeline_phase_start("header/read");
    decode_job_id = loader_timeline_phase_start("decode/pixels");
    loader_timeline_callback_state_init(&timeline_state,
                                        fn_load,
                                        context,
                                        header_job_id,
                                        decode_job_id);

    status = load_with_librsvg(chunk,
                               bgcolor,
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
sixel_loader_librsvg_name(sixel_loader_component_t const *component)
{
    (void)component;
    return "librsvg";
}

static int
sixel_loader_librsvg_predicate(sixel_loader_component_t *component,
                               sixel_chunk_t const *chunk)
{
    (void)component;
    return loader_can_try_librsvg(chunk);
}

static sixel_loader_component_vtbl_t const g_sixel_loader_librsvg_vtbl = {
    sixel_loader_librsvg_ref,
    sixel_loader_librsvg_unref,
    sixel_loader_librsvg_setopt,
    sixel_loader_librsvg_load,
    sixel_loader_librsvg_name,
    sixel_loader_librsvg_predicate
};

SIXELSTATUS
sixel_loader_librsvg_new(sixel_allocator_t *allocator,
                         void **ppcomponent)
{
    sixel_loader_librsvg_component_t *self;

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
    sixel_allocator_ref(allocator);
    *ppcomponent = &self->base;
    return SIXEL_OK;
}

static int
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
