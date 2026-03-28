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
#include "frame.h"
#include "loader-common.h"
#include "status.h"

typedef struct sixel_loader_librsvg_component {
    sixel_loader_component_t base;
    sixel_allocator_t *allocator;
    unsigned int ref;
    unsigned char bgcolor[3];
    int has_bgcolor;
} sixel_loader_librsvg_component_t;

#define LIBRSVG_DEFAULT_WIDTH  300
#define LIBRSVG_DEFAULT_HEIGHT 150
#define LIBRSVG_DEFAULT_DPI    90.0
#define LIBRSVG_MAX_DIMENSION  32767
#define LIBRSVG_MAX_IMAGE_PIXELS ((size_t)268435456u)
#define LIBRSVG_ENV_ALLOW_RELATIVE_RESOURCES \
    "SIXEL_LOADER_LIBRSVG_ALLOW_RELATIVE_RESOURCES"
#define LIBRSVG_ENV_ALLOW_STDIN_SVGZ \
    "SIXEL_LOADER_LIBRSVG_ALLOW_STDIN_SVGZ"
#define LIBRSVG_CONTEXT_PARSE_FILE \
    "librsvg_render_to_frame: unable to parse SVG file."
#define LIBRSVG_CONTEXT_PARSE_DATA \
    "librsvg_render_to_frame: unable to parse SVG data."
#define LIBRSVG_CONTEXT_PARSE_STDIN_SVGZ_TEMPFILE \
    "librsvg_render_to_frame: unable to parse stdin .svgz via " \
    "temporary file."
#define LIBRSVG_MESSAGE_STDIN_SVGZ_REJECTED \
    "librsvg_render_to_frame: gzip-compressed SVG (.svgz) " \
    "requires file-path decode, prior decompression, or " \
    "SIXEL_LOADER_LIBRSVG_ALLOW_STDIN_SVGZ=1."
#define LIBRSVG_MESSAGE_UNSUPPORTED_DECODE_MODE \
    "librsvg_render_to_frame: unsupported decode mode."

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
    size_t output_stride;
    size_t buffer_size;
} sixel_librsvg_surface_convert_plan_t;

typedef enum sixel_librsvg_setopt_action {
    SIXEL_LIBRSVG_SETOPT_ACTION_NOOP_SINGLE_FRAME,
    SIXEL_LIBRSVG_SETOPT_ACTION_LOG_IGNORED_INT,
    SIXEL_LIBRSVG_SETOPT_ACTION_BGCOLOR
} sixel_librsvg_setopt_action_t;

typedef struct sixel_librsvg_setopt_spec {
    int option;
    sixel_librsvg_setopt_action_t action;
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

/*
 * Treat common textual false values as disabled flags.
 */
static int
librsvg_string_is_falsey(char const *value)
{
    if (value == NULL) {
        return 0;
    }
    if (librsvg_equals_nocase(value, "0") ||
            librsvg_equals_nocase(value, "off") ||
            librsvg_equals_nocase(value, "false") ||
            librsvg_equals_nocase(value, "no")) {
        return 1;
    }
    return 0;
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
    if (librsvg_string_is_falsey(value + index)) {
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
    if (state.has_width && librsvg_length_to_pixels(&state.width_length,
                                                    &state.width_from_length) &&
        state.width_from_length >= 1.0) {
        state.width_valid = 1;
    }
    if (state.has_height &&
            librsvg_length_to_pixels(&state.height_length,
                                     &state.height_from_length) &&
            state.height_from_length >= 1.0) {
        state.height_valid = 1;
    }
    state.has_positive_viewbox = state.has_viewbox &&
        state.viewbox.width > 0.0 && state.viewbox.height > 0.0;
    if (state.has_positive_viewbox) {
        librsvg_fill_missing_dimensions_from_viewbox(
            state.viewbox.width,
            state.viewbox.height,
            &state.width_from_length,
            &state.height_from_length,
            &state.width_valid,
            &state.height_valid);
    }
    if (state.width_valid) {
        state.resolved_width = librsvg_rounded_dimension(
            state.width_from_length);
    }
    if (state.height_valid) {
        state.resolved_height = librsvg_rounded_dimension(
            state.height_from_length);
    }
    librsvg_recover_invalid_dimensions_from_viewbox(
        state.has_positive_viewbox,
        &state.viewbox,
        &state.resolved_width,
        &state.resolved_height);

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
librsvg_write_chunk_to_temp_svgz(sixel_chunk_t const *chunk, char **path_out)
{
    SIXELSTATUS status;
    GError *gerror;
    int fd;
    char *path;
    size_t offset;
    ssize_t written;

    status = SIXEL_FALSE;
    gerror = NULL;
    fd = (-1);
    path = NULL;
    offset = 0u;
    written = 0;
    if (chunk == NULL || path_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *path_out = NULL;
    if (chunk->buffer == NULL || chunk->size == 0u) {
        sixel_helper_set_additional_message(
            "librsvg_write_chunk_to_temp_svgz: empty input.");
        return SIXEL_BAD_ARGUMENT;
    }

    fd = g_file_open_tmp("libsixel-librsvg-XXXXXX.svgz", &path, &gerror);
    if (fd < 0 || path == NULL) {
        librsvg_set_error_message(
            "librsvg_write_chunk_to_temp_svgz: g_file_open_tmp failed.",
            gerror);
        status = SIXEL_LIBC_ERROR;
        goto end;
    }

    while (offset < chunk->size) {
        written = sixel_compat_write(fd,
                                     chunk->buffer + offset,
                                     chunk->size - offset);
        if (written <= 0) {
            sixel_helper_set_additional_message(
                "librsvg_write_chunk_to_temp_svgz: "
                "failed to write temporary .svgz file.");
            status = SIXEL_LIBC_ERROR;
            goto end;
        }
        offset += (size_t)written;
    }

    if (sixel_compat_close(fd) != 0) {
        sixel_helper_set_additional_message(
            "librsvg_write_chunk_to_temp_svgz: "
            "failed to close temporary .svgz file.");
        status = SIXEL_LIBC_ERROR;
        fd = (-1);
        goto end;
    }
    fd = (-1);

    *path_out = path;
    path = NULL;
    status = SIXEL_OK;

end:
    if (fd >= 0) {
        (void)sixel_compat_close(fd);
    }
    if (path != NULL) {
        (void)sixel_compat_unlink(path);
        g_free(path);
    }
    if (gerror != NULL) {
        g_error_free(gerror);
    }

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

    message[0] = '\0';
    mode_name = librsvg_decode_mode_name(mode);
    written = 0;
    written = sixel_compat_snprintf(message,
                                    sizeof(message),
                                    "librsvg: decode_mode=%s",
                                    mode_name);
    if (written < 0) {
        sixel_trace_topic_message("loader", "librsvg: decode_mode=unknown");
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

sixel_librsvg_decode_mode_t
sixel_loader_librsvg_pick_decode_mode_for_test(
    sixel_chunk_t const *chunk,
    int allow_relative_resources,
    int allow_stdin_svgz)
{
    sixel_librsvg_decode_policy_t policy;

    policy.allow_relative_resources = allow_relative_resources ? 1 : 0;
    policy.allow_stdin_svgz = allow_stdin_svgz ? 1 : 0;
    return librsvg_pick_decode_mode(chunk, &policy);
}

static SIXELSTATUS
librsvg_new_handle_from_file(char const *path,
                             char const *context,
                             RsvgHandle **handle_out)
{
    SIXELSTATUS status;
    GError *gerror;
    RsvgHandle *handle;

    status = SIXEL_FALSE;
    gerror = NULL;
    handle = NULL;
    if (path == NULL || context == NULL || handle_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *handle_out = NULL;

    handle = rsvg_handle_new_from_file(path, &gerror);
    if (handle == NULL) {
        status = librsvg_fail_with_gerror(SIXEL_BAD_INPUT, context, gerror);
        goto end;
    }

    *handle_out = handle;
    handle = NULL;
    status = SIXEL_OK;

end:
    if (handle != NULL) {
        g_object_unref(handle);
    }
    if (gerror != NULL) {
        g_error_free(gerror);
    }

    return status;
}

static SIXELSTATUS
librsvg_new_handle_from_data(unsigned char const *buffer,
                             size_t size,
                             char const *context,
                             RsvgHandle **handle_out)
{
    SIXELSTATUS status;
    GError *gerror;
    RsvgHandle *handle;

    status = SIXEL_FALSE;
    gerror = NULL;
    handle = NULL;
    if (buffer == NULL ||
            size == 0u ||
            context == NULL ||
            handle_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *handle_out = NULL;

    handle = rsvg_handle_new_from_data(buffer, size, &gerror);
    if (handle == NULL) {
        status = librsvg_fail_with_gerror(SIXEL_BAD_INPUT, context, gerror);
        goto end;
    }

    *handle_out = handle;
    handle = NULL;
    status = SIXEL_OK;

end:
    if (handle != NULL) {
        g_object_unref(handle);
    }
    if (gerror != NULL) {
        g_error_free(gerror);
    }

    return status;
}

static SIXELSTATUS
librsvg_open_handle_from_source_file(sixel_chunk_t const *chunk,
                                     RsvgHandle **handle_out)
{
    if (chunk == NULL || handle_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    return librsvg_new_handle_from_file(chunk->source_path,
                                        LIBRSVG_CONTEXT_PARSE_FILE,
                                        handle_out);
}

static SIXELSTATUS
librsvg_open_handle_from_data_chunk(sixel_chunk_t const *chunk,
                                    RsvgHandle **handle_out)
{
    if (chunk == NULL || handle_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    return librsvg_new_handle_from_data(chunk->buffer,
                                        chunk->size,
                                        LIBRSVG_CONTEXT_PARSE_DATA,
                                        handle_out);
}

static SIXELSTATUS
librsvg_open_handle_from_stdin_svgz_tempfile(sixel_chunk_t const *chunk,
                                             RsvgHandle **handle_out,
                                             char **temp_path_out)
{
    SIXELSTATUS status;
    char *temp_path;

    status = SIXEL_FALSE;
    temp_path = NULL;
    if (chunk == NULL || handle_out == NULL || temp_path_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *handle_out = NULL;
    *temp_path_out = NULL;

    status = librsvg_write_chunk_to_temp_svgz(chunk, &temp_path);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = librsvg_new_handle_from_file(
        temp_path,
        LIBRSVG_CONTEXT_PARSE_STDIN_SVGZ_TEMPFILE,
        handle_out);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    *temp_path_out = temp_path;
    temp_path = NULL;
    status = SIXEL_OK;

end:
    if (temp_path != NULL) {
        (void)sixel_compat_unlink(temp_path);
        g_free(temp_path);
    }

    return status;
}

static SIXELSTATUS
librsvg_open_handle_by_mode(sixel_librsvg_decode_mode_t decode_mode,
                            sixel_chunk_t const *chunk,
                            sixel_librsvg_open_result_t *open_result)
{
    if (chunk == NULL || open_result == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    switch (decode_mode) {
    case SIXEL_LIBRSVG_DECODE_MODE_FILE:
        return librsvg_open_handle_from_source_file(chunk,
                                                    &open_result->handle);
    case SIXEL_LIBRSVG_DECODE_MODE_STDIN_SVGZ_TEMPFILE:
        return librsvg_open_handle_from_stdin_svgz_tempfile(
            chunk,
            &open_result->handle,
            &open_result->stdin_svgz_temp_path);
    case SIXEL_LIBRSVG_DECODE_MODE_DATA:
        return librsvg_open_handle_from_data_chunk(chunk,
                                                   &open_result->handle);
    case SIXEL_LIBRSVG_DECODE_MODE_STDIN_SVGZ_REJECTED:
        sixel_helper_set_additional_message(
            LIBRSVG_MESSAGE_STDIN_SVGZ_REJECTED);
        return SIXEL_BAD_INPUT;
    default:
        sixel_helper_set_additional_message(
            LIBRSVG_MESSAGE_UNSUPPORTED_DECODE_MODE);
        return SIXEL_BAD_ARGUMENT;
    }
}

static SIXELSTATUS
librsvg_open_handle(sixel_chunk_t const *chunk,
                    sixel_librsvg_decode_policy_t const *policy,
                    sixel_librsvg_open_result_t *open_result)
{
    sixel_librsvg_decode_mode_t decode_mode;

    decode_mode = SIXEL_LIBRSVG_DECODE_MODE_DATA;
    if (chunk == NULL ||
            policy == NULL ||
            open_result == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    open_result->handle = NULL;
    open_result->stdin_svgz_temp_path = NULL;
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
    if (open_result->handle != NULL) {
        g_object_unref(open_result->handle);
        open_result->handle = NULL;
    }
    if (open_result->stdin_svgz_temp_path != NULL) {
        (void)sixel_compat_unlink(open_result->stdin_svgz_temp_path);
        g_free(open_result->stdin_svgz_temp_path);
        open_result->stdin_svgz_temp_path = NULL;
    }
}

static void
librsvg_render_context_init(sixel_librsvg_render_context_t *render_ctx)
{
    if (render_ctx == NULL) {
        return;
    }
    render_ctx->open_result.handle = NULL;
    render_ctx->open_result.stdin_svgz_temp_path = NULL;
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
    if (render_ctx->cr != NULL) {
        cairo_destroy(render_ctx->cr);
        render_ctx->cr = NULL;
    }
    if (render_ctx->surface != NULL) {
        cairo_surface_destroy(render_ctx->surface);
        render_ctx->surface = NULL;
    }
    librsvg_open_result_cleanup(&render_ctx->open_result);
    render_ctx->pixel_total = 0u;
}

static void
librsvg_decode_policy_init_from_env(sixel_librsvg_decode_policy_t *policy)
{
    if (policy == NULL) {
        return;
    }

    policy->allow_relative_resources =
        librsvg_env_is_enabled(LIBRSVG_ENV_ALLOW_RELATIVE_RESOURCES);
    policy->allow_stdin_svgz =
        librsvg_env_is_enabled(LIBRSVG_ENV_ALLOW_STDIN_SVGZ);
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
            "librsvg_render_to_frame: invalid dimensions.");
        return SIXEL_BAD_INPUT;
    }
    if (width > LIBRSVG_MAX_DIMENSION || height > LIBRSVG_MAX_DIMENSION) {
        sixel_helper_set_additional_message(
            "librsvg_render_to_frame: dimensions exceed limit.");
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    if ((size_t)width > SIZE_MAX / (size_t)height) {
        sixel_helper_set_additional_message(
            "librsvg_render_to_frame: dimensions overflow pixel count.");
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    pixel_total = (size_t)width * (size_t)height;
    if (pixel_total > LIBRSVG_MAX_IMAGE_PIXELS) {
        sixel_helper_set_additional_message(
            "librsvg_render_to_frame: image exceeds pixel limit.");
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    *pixel_total_out = pixel_total;
    return SIXEL_OK;
}

static SIXELSTATUS
librsvg_prepare_render_surface(cairo_surface_t **surface_out,
                               cairo_t **cr_out,
                               int width,
                               int height,
                               unsigned char const *bgcolor)
{
    SIXELSTATUS status;
    cairo_surface_t *surface;
    cairo_t *cr;
    cairo_status_t cairo_stat;

    status = SIXEL_FALSE;
    surface = NULL;
    cr = NULL;
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

    if (bgcolor == NULL) {
        cairo_save(cr);
        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
        cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0);
        cairo_paint(cr);
        cairo_restore(cr);
    } else {
        cairo_set_source_rgb(cr,
                             ((double)bgcolor[0]) / 255.0,
                             ((double)bgcolor[1]) / 255.0,
                             ((double)bgcolor[2]) / 255.0);
        cairo_paint(cr);
    }

    *surface_out = surface;
    *cr_out = cr;
    surface = NULL;
    cr = NULL;
    status = SIXEL_OK;

end:
    if (cr != NULL) {
        cairo_destroy(cr);
    }
    if (surface != NULL) {
        cairo_surface_destroy(surface);
    }

    return status;
}

static SIXELSTATUS
librsvg_render_document(RsvgHandle *handle, cairo_t *cr, int width, int height)
{
    SIXELSTATUS status;
    GError *gerror;
    RsvgRectangle viewport;

    status = SIXEL_FALSE;
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
            "librsvg_render_to_frame: rsvg_handle_render_document failed.",
            gerror);
        goto end;
    }
    status = SIXEL_OK;

end:
    if (gerror != NULL) {
        g_error_free(gerror);
    }

    return status;
}

static SIXELSTATUS
librsvg_unpack_surface_pixels_rgba(unsigned char *pixels,
                                   unsigned char const *row,
                                   size_t row_stride,
                                   int width,
                                   int height,
                                   int *has_non_opaque_alpha_out)
{
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
    int has_non_opaque_alpha;

    pixel_index = 0u;
    x = 0;
    y = 0;
    src = NULL;
    pixel = 0u;
    dst = 0u;
    alpha = 0u;
    red = 0u;
    green = 0u;
    blue = 0u;
    has_non_opaque_alpha = 0;
    if (pixels == NULL ||
            row == NULL ||
            width <= 0 ||
            height <= 0 ||
            has_non_opaque_alpha_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    for (y = 0; y < height; ++y) {
        src = (uint32_t const *)(row + (size_t)y * row_stride);
        for (x = 0; x < width; ++x) {
            pixel_index = (size_t)y * (size_t)width + (size_t)x;
            pixel = src[x];
            alpha = (pixel >> 24) & 0xffu;
            red = (pixel >> 16) & 0xffu;
            green = (pixel >> 8) & 0xffu;
            blue = pixel & 0xffu;
            if (alpha != 255u) {
                has_non_opaque_alpha = 1;
            }
            dst = pixel_index * 4u;
            pixels[dst + 0] = librsvg_unpremultiply_channel(red, alpha);
            pixels[dst + 1] = librsvg_unpremultiply_channel(green, alpha);
            pixels[dst + 2] = librsvg_unpremultiply_channel(blue, alpha);
            pixels[dst + 3] = (unsigned char)alpha;
        }
    }

    *has_non_opaque_alpha_out = has_non_opaque_alpha;
    return SIXEL_OK;
}

static SIXELSTATUS
librsvg_unpack_surface_pixels_rgb(unsigned char *pixels,
                                  unsigned char const *row,
                                  size_t row_stride,
                                  int width,
                                  int height)
{
    size_t pixel_index;
    int x;
    int y;
    uint32_t const *src;
    uint32_t pixel;
    size_t dst;
    unsigned int red;
    unsigned int green;
    unsigned int blue;

    pixel_index = 0u;
    x = 0;
    y = 0;
    src = NULL;
    pixel = 0u;
    dst = 0u;
    red = 0u;
    green = 0u;
    blue = 0u;
    if (pixels == NULL ||
            row == NULL ||
            width <= 0 ||
            height <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }

    for (y = 0; y < height; ++y) {
        src = (uint32_t const *)(row + (size_t)y * row_stride);
        for (x = 0; x < width; ++x) {
            pixel_index = (size_t)y * (size_t)width + (size_t)x;
            pixel = src[x];
            red = (pixel >> 16) & 0xffu;
            green = (pixel >> 8) & 0xffu;
            blue = pixel & 0xffu;
            dst = pixel_index * 3u;
            pixels[dst + 0] = (unsigned char)red;
            pixels[dst + 1] = (unsigned char)green;
            pixels[dst + 2] = (unsigned char)blue;
        }
    }

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
    if (pixels == NULL ||
            row == NULL ||
            width <= 0 ||
            height <= 0 ||
            has_non_opaque_alpha_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (inspect_alpha) {
        return librsvg_unpack_surface_pixels_rgba(
            pixels,
            row,
            row_stride,
            width,
            height,
            has_non_opaque_alpha_out);
    }

    *has_non_opaque_alpha_out = 0;
    return librsvg_unpack_surface_pixels_rgb(pixels,
                                             row,
                                             row_stride,
                                             width,
                                             height);
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

    inspect_alpha = 0;
    output_stride = 0u;
    if (plan == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    inspect_alpha = bgcolor == NULL ? 1 : 0;
    output_stride = inspect_alpha ? 4u : 3u;
    if (pixel_total > SIZE_MAX / output_stride) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    plan->inspect_alpha = inspect_alpha;
    plan->output_stride = output_stride;
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

    status = SIXEL_FALSE;
    pixels = NULL;
    row = NULL;
    row_stride = 0u;
    plan.inspect_alpha = 0;
    plan.output_stride = 0u;
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
    cairo_surface_flush(surface);
    row = cairo_image_surface_get_data(surface);
    row_stride = (size_t)cairo_image_surface_get_stride(surface);
    if (row == NULL || row_stride == 0u) {
        sixel_helper_set_additional_message(
            "librsvg_render_to_frame: cairo surface access failed.");
        return SIXEL_BAD_INPUT;
    }

    status = librsvg_build_surface_convert_plan(bgcolor, pixel_total, &plan);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    pixels = (unsigned char *)sixel_allocator_malloc(
        allocator,
        plan.buffer_size);
    if (pixels == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    status = librsvg_unpack_surface_pixels(pixels,
                                           row,
                                           row_stride,
                                           frame->width,
                                           frame->height,
                                           plan.inspect_alpha,
                                           &has_non_opaque_alpha);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    librsvg_commit_frame_pixels(frame,
                                pixels,
                                pixel_total,
                                plan.inspect_alpha,
                                has_non_opaque_alpha);
    pixels = NULL;

    status = SIXEL_OK;

end:
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
    SIXELSTATUS status;

    status = SIXEL_BAD_INPUT;
    if (frame == NULL || handle == NULL || pixel_total_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    librsvg_pick_size(handle, &frame->width, &frame->height);
    status = librsvg_validate_canvas_size(frame->width,
                                          frame->height,
                                          pixel_total_out);
    return status;
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

static SIXELSTATUS
librsvg_prepare_render_context(
    sixel_frame_t *frame,
    sixel_chunk_t const *chunk,
    unsigned char const *bgcolor,
    sixel_librsvg_decode_policy_t const *policy,
    sixel_librsvg_render_context_t *render_ctx)
{
    SIXELSTATUS status;

    status = SIXEL_BAD_INPUT;
    if (frame == NULL ||
            chunk == NULL ||
            policy == NULL ||
            render_ctx == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = librsvg_open_handle(chunk,
                                 policy,
                                 &render_ctx->open_result);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    status = librsvg_prepare_frame_geometry(frame,
                                            render_ctx->open_result.handle,
                                            &render_ctx->pixel_total);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    status = librsvg_prepare_frame_surface(frame, bgcolor, render_ctx);
    return status;
}

static SIXELSTATUS
librsvg_render_context_to_frame_pixels(
    sixel_frame_t *frame,
    sixel_chunk_t const *chunk,
    unsigned char const *bgcolor,
    sixel_librsvg_render_context_t *render_ctx)
{
    SIXELSTATUS status;

    status = SIXEL_BAD_INPUT;
    if (frame == NULL || chunk == NULL || render_ctx == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = librsvg_render_document(render_ctx->open_result.handle,
                                     render_ctx->cr,
                                     frame->width,
                                     frame->height);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    status = librsvg_convert_surface_to_frame_pixels(frame,
                                                     chunk->allocator,
                                                     render_ctx->surface,
                                                     bgcolor,
                                                     render_ctx->pixel_total);
    return status;
}

static SIXELSTATUS
librsvg_render_to_frame(sixel_frame_t *frame,
                        sixel_chunk_t const *chunk,
                        unsigned char const *bgcolor,
                        sixel_librsvg_decode_policy_t const *policy)
{
    SIXELSTATUS status;
    sixel_librsvg_render_context_t render_ctx;

    status = SIXEL_BAD_INPUT;
    librsvg_render_context_init(&render_ctx);
    if (frame == NULL || chunk == NULL || policy == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    status = librsvg_prepare_render_context(frame,
                                            chunk,
                                            bgcolor,
                                            policy,
                                            &render_ctx);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = librsvg_render_context_to_frame_pixels(frame,
                                                    chunk,
                                                    bgcolor,
                                                    &render_ctx);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = SIXEL_OK;

end:
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

    status = SIXEL_FALSE;
    frame = NULL;
    if (pchunk == NULL || fn_load == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    librsvg_decode_policy_init_from_env(&policy);

    status = sixel_frame_new(&frame, pchunk->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = librsvg_render_to_frame(frame,
                                     pchunk,
                                     bgcolor,
                                     &policy);
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

/*
 * API compatibility only: SVG decode is single-frame in this backend.
 */
static SIXELSTATUS
librsvg_setopt_noop_single_frame(void const *value)
{
    (void)value;
    return SIXEL_OK;
}

static SIXELSTATUS
librsvg_setopt_bgcolor(sixel_loader_librsvg_component_t *self,
                       void const *value)
{
    unsigned char const *color;

    color = NULL;
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

static sixel_librsvg_setopt_spec_t const g_librsvg_setopt_specs[] = {
    { SIXEL_LOADER_OPTION_REQUIRE_STATIC,
      SIXEL_LIBRSVG_SETOPT_ACTION_NOOP_SINGLE_FRAME,
      NULL,
      NULL },
    { SIXEL_LOADER_OPTION_LOOP_CONTROL,
      SIXEL_LIBRSVG_SETOPT_ACTION_NOOP_SINGLE_FRAME,
      NULL,
      NULL },
    { SIXEL_LOADER_OPTION_START_FRAME_NO,
      SIXEL_LIBRSVG_SETOPT_ACTION_NOOP_SINGLE_FRAME,
      NULL,
      NULL },
    { SIXEL_LOADER_OPTION_USE_PALETTE,
      SIXEL_LIBRSVG_SETOPT_ACTION_LOG_IGNORED_INT,
      "USE_PALETTE",
      "output remains RGB/RGBA." },
    { SIXEL_LOADER_OPTION_REQCOLORS,
      SIXEL_LIBRSVG_SETOPT_ACTION_LOG_IGNORED_INT,
      "REQCOLORS",
      "palette limits apply during quantization." },
    { SIXEL_LOADER_OPTION_BGCOLOR,
      SIXEL_LIBRSVG_SETOPT_ACTION_BGCOLOR,
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
    int const *flag;

    self = NULL;
    spec = NULL;
    flag = NULL;
    if (component == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    self = (sixel_loader_librsvg_component_t *)component;
    spec = librsvg_find_setopt_spec(option);
    if (spec == NULL) {
        return SIXEL_OK;
    }

    switch (spec->action) {
    case SIXEL_LIBRSVG_SETOPT_ACTION_NOOP_SINGLE_FRAME:
        return librsvg_setopt_noop_single_frame(value);
    case SIXEL_LIBRSVG_SETOPT_ACTION_LOG_IGNORED_INT:
        flag = (int const *)value;
        librsvg_debug_ignored_int_option(spec->name,
                                         flag,
                                         spec->detail);
        return SIXEL_OK;
    case SIXEL_LIBRSVG_SETOPT_ACTION_BGCOLOR:
        return librsvg_setopt_bgcolor(self, value);
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
    unsigned char const *bgcolor;

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
                             bgcolor,
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

sixel_librsvg_decode_mode_t
sixel_loader_librsvg_pick_decode_mode_for_test(
    sixel_chunk_t const *chunk,
    int allow_relative_resources,
    int allow_stdin_svgz)
{
    (void)chunk;
    (void)allow_relative_resources;
    (void)allow_stdin_svgz;
    return SIXEL_LIBRSVG_DECODE_MODE_DATA;
}

#endif  /* HAVE_LIBRSVG */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
