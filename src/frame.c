/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 * Copyright (c) 2014-2020 Hayaki Saito
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

/* STDC_HEADERS */
#include <float.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#if HAVE_MATH_H
# include <math.h>
#endif  /* HAVE_MATH_H */
#if HAVE_LIMITS_H
# include <limits.h>
#endif  /* HAVE_LIMITS_H */
#if HAVE_INTTYPES_H
# include <inttypes.h>
#endif  /* HAVE_INTTYPES_H */

#include "frame-private.h"
#include "cms.h"
#include "frame-normalize.h"
#include "pixelformat.h"
#include "colorspace.h"
#include "compat_stub.h"
#include "loader-common.h"
#include "options.h"
#include "scale.h"
#include "sixel_atomic.h"

/* Stream metadata is shared by frames and clones. Parsing an ICC profile
 * belongs to definition creation, never to capture-buffer admission. */
struct sixel_ycbcr_def {
    sixel_atomic_u32_t ref;
    int encoding, range, chroma_x_half, chroma_y_half;
    int nearest_chroma;
    sixel_color_def_t *rgb;
};

static SIXELSTATUS
sixel_frame_convert_to_rgb888(sixel_frame_t /*in */ *frame);
static SIXELSTATUS
sixel_frame_promote_to_float32(sixel_frame_t *frame, int target_space);
static void
sixel_frame_assign_layout(sixel_frame_t *frame, int layout, int coordinates);
static void *
sixel_frame_pixel_storage(sixel_frame_t const *frame);
static void
sixel_frame_release_owned_pixels(sixel_frame_t *frame);
static void
sixel_frame_release_owned_palette(sixel_frame_t *frame);
static void
sixel_frame_release_replaced_pixels(sixel_frame_t *frame,
                                    void *replacement);
static void
sixel_frame_release_replaced_palette(sixel_frame_t *frame,
                                     unsigned char *replacement,
                                     int storage_owned);
static int sixel_frame_unpacked_format(int format);

static SIXELSTATUS
sixel_frame_init_common(sixel_frame_t *frame,
                        void *pixels,
                        int width,
                        int height,
                        int pixelformat,
                        unsigned char *palette,
                        int ncolors,
                        int is_float,
                        int owns_pixels,
                        int owns_palette);
static SIXELSTATUS
sixel_frame_validate_size(char const *context,
                          int width,
                          int height,
                          int pixelformat,
                          size_t *pixel_total,
                          size_t *byte_total,
                          int *depth_bytes);
static SIXELSTATUS
sixel_frame_resize_transparent_mask(
    sixel_frame_t *frame,
    int src_width,
    int src_height,
    int dst_width,
    int dst_height,
    int method_for_resampling,
    unsigned char **pmask_out,
    size_t *pmask_size_out,
    int *phas_transparent_out);
static SIXELSTATUS
clip(unsigned char *pixels,
     int sx,
     int sy,
     int pixelformat,
     int cx,
     int cy,
     int cw,
     int ch);
static sixel_frame_t *
sixel_frame_from_interface(sixel_frame_interface_t *frame);
static sixel_frame_t const *
sixel_frame_from_interface_const(sixel_frame_interface_t const *frame);
static SIXELSTATUS
sixel_frame_get_pixels_view(sixel_frame_t const *frame,
                            sixel_frame_pixels_view_t *view);
static SIXELSTATUS
sixel_frame_measure_storage(sixel_frame_t const *frame,
                            size_t *storage_bytes);
/* Options for materializing a packed source or sharing a decoded raster.
 * A NULL options pointer requests the ordinary deep clone. */
typedef struct sixel_frame_clone_options {
    int step;
    sixel_frame_t *owner;
    sixel_frame_t const *grid;
    int grid_step;
} sixel_frame_clone_options_t;

static SIXELSTATUS
sixel_frame_clone(sixel_frame_t const *frame,
                  sixel_allocator_t *allocator,
                  sixel_frame_t **frame_out,
                  sixel_frame_clone_options_t const *options);
static SIXELSTATUS
sixel_frame_decode_ycbcr422_into(sixel_frame_t const *frame,
                                sixel_frame_t *destination, int step,
                                sixel_frame_t const *grid, int grid_step);
static void
sixel_frame_vtbl_ref(sixel_frame_interface_t *frame);
static void
sixel_frame_vtbl_unref(sixel_frame_interface_t *frame);
static SIXELSTATUS
sixel_frame_vtbl_init_pixels(
    sixel_frame_interface_t *frame,
    sixel_frame_pixels_request_t const *request);
static SIXELSTATUS
sixel_frame_vtbl_get_pixels(sixel_frame_interface_t *frame,
                            sixel_frame_pixels_view_t *view);
static SIXELSTATUS
sixel_frame_vtbl_set_pixelformat(sixel_frame_interface_t *frame,
                                 int pixelformat);
static SIXELSTATUS
sixel_frame_vtbl_get_timeline(sixel_frame_interface_t *frame,
                              sixel_frame_timeline_t *timeline);
static SIXELSTATUS
sixel_frame_vtbl_set_timeline(
    sixel_frame_interface_t *frame,
    sixel_frame_timeline_t const *timeline);
static SIXELSTATUS
sixel_frame_vtbl_get_transparency(
    sixel_frame_interface_t *frame,
    sixel_frame_transparency_t *transparency);
static SIXELSTATUS
sixel_frame_vtbl_set_transparency(
    sixel_frame_interface_t *frame,
    sixel_frame_transparency_t const *transparency);
static SIXELSTATUS
sixel_frame_vtbl_resize(sixel_frame_interface_t *frame,
                        int width,
                        int height,
                        int method_for_resampling);
static SIXELSTATUS
sixel_frame_vtbl_resize_float32(sixel_frame_interface_t *frame,
                                int width,
                                int height,
                                int method_for_resampling);
static SIXELSTATUS
sixel_frame_vtbl_clip(sixel_frame_interface_t *frame,
                      int x,
                      int y,
                      int width,
                      int height);
static SIXELSTATUS
sixel_frame_vtbl_measure_storage(sixel_frame_interface_t *frame,
                                 size_t *storage_bytes);
static SIXELSTATUS
sixel_frame_vtbl_clone(sixel_frame_interface_t *frame,
                       sixel_allocator_t *allocator,
                       sixel_frame_interface_t **frame_out);
static SIXELSTATUS
sixel_frame_vtbl_get_alpha(sixel_frame_interface_t *frame,
                           sixel_frame_alpha_t *alpha);
static SIXELSTATUS
sixel_frame_vtbl_set_alpha(sixel_frame_interface_t *frame,
                           sixel_frame_alpha_t const *alpha);

static SIXELSTATUS
sixel_frame_vtbl_set_rgb_definition(sixel_frame_interface_t *iface,
                                    sixel_color_def_t const *definition);

static sixel_frame_vtbl_t const g_sixel_frame_vtbl = {
    sixel_frame_vtbl_ref,
    sixel_frame_vtbl_unref,
    sixel_frame_vtbl_init_pixels,
    sixel_frame_vtbl_get_pixels,
    sixel_frame_vtbl_set_pixelformat,
    sixel_frame_vtbl_get_timeline,
    sixel_frame_vtbl_set_timeline,
    sixel_frame_vtbl_get_transparency,
    sixel_frame_vtbl_set_transparency,
    sixel_frame_vtbl_resize,
    sixel_frame_vtbl_resize_float32,
    sixel_frame_vtbl_clip,
    sixel_frame_vtbl_measure_storage,
    sixel_frame_vtbl_clone,
    sixel_frame_vtbl_get_alpha,
    sixel_frame_vtbl_set_alpha,
    sixel_frame_vtbl_set_rgb_definition
};


/*
 * Validate that a frame of the requested size stays within allocation
 * limits. Callers receive optional pixel and byte counts so they can reuse
 * the calculation without recomputing the totals when allocating buffers.
 */
static SIXELSTATUS
sixel_frame_validate_size(char const *context,
                          int width,
                          int height,
                          int pixelformat,
                          size_t *pixel_total,
                          size_t *byte_total,
                          int *depth_bytes)
{
    SIXELSTATUS status;
    char message[128];
    size_t pixels;
    size_t bytes;
    int depth;

    status = SIXEL_FALSE;
    pixels = 0u;
    bytes = 0u;
    depth = 0;
    if (context == NULL) {
        context = "sixel frame validation";
    }

    depth = sixel_helper_compute_depth(pixelformat);
    if (depth <= 0) {
        sixel_compat_snprintf(message,
                              sizeof(message),
                              "%s: pixelformat depth is invalid.",
                              context);
        sixel_helper_set_additional_message(message);
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    pixels = (size_t)width * (size_t)height;
    if (pixels > SIZE_MAX / (size_t)depth) {
        sixel_compat_snprintf(message,
                              sizeof(message),
                              "%s: buffer size overflow.",
                              context);
        sixel_helper_set_additional_message(message);
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    bytes = pixels * (size_t)depth;
    if (bytes > SIXEL_ALLOCATE_BYTES_MAX) {
        sixel_compat_snprintf(message,
                              sizeof(message),
                              "%s: frame exceeds "
                              "SIXEL_ALLOCATE_BYTES_MAX.",
                              context);
        sixel_helper_set_additional_message(message);
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    if (pixel_total != NULL) {
        *pixel_total = pixels;
    }
    if (byte_total != NULL) {
        *byte_total = bytes;
    }
    if (depth_bytes != NULL) {
        *depth_bytes = depth;
    }

    status = SIXEL_OK;

end:
    return status;
}

static sixel_frame_t *
sixel_frame_from_interface(sixel_frame_interface_t *frame)
{
    return (sixel_frame_t *)frame;
}

static sixel_frame_t const *
sixel_frame_from_interface_const(sixel_frame_interface_t const *frame)
{
    return (sixel_frame_t const *)frame;
}

static void *
sixel_frame_pixel_storage(sixel_frame_t const *frame)
{
    if (frame == NULL) {
        return NULL;
    }
    if (frame->format.layout == SIXEL_PIXELFORMAT_RGBFLOAT32) {
        return (void *)frame->pixels.f32ptr;
    }

    return (void *)frame->pixels.u8ptr;
}

static void
sixel_frame_release_owned_pixels(sixel_frame_t *frame)
{
    void *pixels;

    if (frame == NULL) {
        return;
    }

    pixels = sixel_frame_pixel_storage(frame);
    if (frame->owns_pixels != 0) {
        sixel_allocator_free(frame->allocator, pixels);
    }
    if (frame->pixel_owner != NULL) {
        sixel_frame_unref(frame->pixel_owner);
        frame->pixel_owner = NULL;
    }
    frame->pixels.u8ptr = NULL;
    frame->pixel_extent = 0;
    frame->owns_pixels = 1;
}

static void
sixel_frame_release_owned_palette(sixel_frame_t *frame)
{
    if (frame == NULL) {
        return;
    }

    if (frame->owns_palette != 0) {
        sixel_allocator_free(frame->allocator, frame->palette);
    }
    frame->palette = NULL;
    frame->owns_palette = 1;
}

static void
sixel_frame_release_replaced_pixels(sixel_frame_t *frame,
                                    void *replacement)
{
    void *pixels;

    if (frame == NULL) {
        return;
    }

    pixels = sixel_frame_pixel_storage(frame);
    /*
     * Some loader paths write newly allocated storage into the private frame
     * first and then call init_pixels() with that same pointer to publish the
     * metadata.  Treat an identical pointer as an ownership-state refresh, not
     * as replaced storage, or the initializer would free the buffer it is about
     * to keep using.
     *
     * For real replacement, the free decision belongs only to the old storage
     * ownership bit.  The ownership of the incoming request is applied after
     * the pointer has been installed.
     */
    if (pixels != replacement) {
        sixel_frame_release_owned_pixels(frame);
    }
}

static void
sixel_frame_release_replaced_palette(sixel_frame_t *frame,
                                     unsigned char *replacement,
                                     int storage_owned)
{
    if (frame == NULL) {
        return;
    }

    /*
     * Palette loaders commonly convert into frame->palette before routing the
     * result through IFrame.init_pixels().  Preserve that just-installed
     * palette when the request points back to the same storage.  When storage
     * really is being replaced, free it only if the old ownership bit says the
     * frame owns it.
     */
    if (frame->palette != replacement) {
        if (storage_owned != 0) {
            sixel_allocator_free(frame->allocator, frame->palette);
        }
        frame->palette = NULL;
        frame->palette_extent = 0;
        frame->owns_palette = 1;
    }
}

static void
sixel_frame_vtbl_ref(sixel_frame_interface_t *frame)
{
    sixel_frame_t *storage;

    storage = sixel_frame_from_interface(frame);
    sixel_frame_ref(storage);
}

static void
sixel_frame_vtbl_unref(sixel_frame_interface_t *frame)
{
    sixel_frame_t *storage;

    storage = sixel_frame_from_interface(frame);
    sixel_frame_unref(storage);
}

static SIXELSTATUS
sixel_frame_init_pixels(sixel_frame_t *frame,
                       sixel_frame_pixels_request_t const *request,
                       int owns_storage)
{
    SIXELSTATUS status;

    if (frame == NULL || request == NULL ||
        request->colorspace == SIXEL_FRAME_COLORSPACE_PROFILE_RGB) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_frame_init_common(frame,
                                     request->pixels,
                                     request->width,
                                     request->height,
                                     request->pixelformat,
                                     request->palette,
                                     request->ncolors,
                                     request->kind ==
                                         SIXEL_FRAME_PIXELS_FLOAT32,
                                     owns_storage,
                                     owns_storage);
    if (SIXEL_SUCCEEDED(status) && request->colorspace >= 0) {
        frame->format.coordinates = request->colorspace;
    }

    return status;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_frame_borrow_pixels(sixel_frame_t *frame,
                         sixel_frame_pixels_request_t const *request)
{
    return sixel_frame_init_pixels(frame, request, 0);
}

static SIXELSTATUS
sixel_frame_vtbl_init_pixels(
    sixel_frame_interface_t *frame,
    sixel_frame_pixels_request_t const *request)
{
    return sixel_frame_init_pixels(sixel_frame_from_interface(frame),
                                   request, 1);
}

static SIXELSTATUS
sixel_frame_vtbl_get_pixels(sixel_frame_interface_t *frame,
                            sixel_frame_pixels_view_t *view)
{
    sixel_frame_t const *storage;

    if (frame == NULL || view == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    storage = sixel_frame_from_interface_const(frame);
    return sixel_frame_get_pixels_view(storage, view);
}

static SIXELSTATUS
sixel_frame_vtbl_set_pixelformat(sixel_frame_interface_t *frame,
                                 int pixelformat)
{
    sixel_frame_t *storage;

    if (frame == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    storage = sixel_frame_from_interface(frame);
    return sixel_frame_set_pixelformat(storage, pixelformat);
}

static SIXELSTATUS
sixel_frame_vtbl_get_timeline(sixel_frame_interface_t *frame,
                              sixel_frame_timeline_t *timeline)
{
    sixel_frame_t const *storage;

    if (frame == NULL || timeline == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    storage = sixel_frame_from_interface_const(frame);
    timeline->delay = storage->delay;
    timeline->frame_no = storage->frame_no;
    timeline->loop_count = storage->loop_count;
    timeline->multiframe = storage->multiframe;
    timeline->handoff_shareable = storage->handoff_shareable;

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_frame_vtbl_set_timeline(
    sixel_frame_interface_t *frame,
    sixel_frame_timeline_t const *timeline)
{
    sixel_frame_t *storage;

    if (frame == NULL || timeline == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    storage = sixel_frame_from_interface(frame);
    storage->delay = timeline->delay;
    storage->frame_no = timeline->frame_no;
    storage->loop_count = timeline->loop_count;
    storage->multiframe = timeline->multiframe;
    storage->handoff_shareable = timeline->handoff_shareable != 0 ? 1 : 0;

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_frame_vtbl_get_transparency(
    sixel_frame_interface_t *frame,
    sixel_frame_transparency_t *transparency)
{
    sixel_frame_t const *storage;

    if (frame == NULL || transparency == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    storage = sixel_frame_from_interface_const(frame);
    transparency->transparent = storage->transparent;
    transparency->alpha_zero_is_transparent =
        storage->alpha_zero_is_transparent;
    transparency->transparent_mask = storage->transparent_mask;
    transparency->transparent_mask_size = storage->transparent_mask_size;

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_frame_vtbl_set_transparency(
    sixel_frame_interface_t *frame,
    sixel_frame_transparency_t const *transparency)
{
    sixel_frame_t *storage;

    if (frame == NULL || transparency == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    storage = sixel_frame_from_interface(frame);
    if (storage->alpha != NULL &&
        (transparency->transparent >= 0 ||
         transparency->transparent_mask != NULL ||
         transparency->alpha_zero_is_transparent != 0)) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (storage->transparent_mask != NULL &&
        storage->transparent_mask != transparency->transparent_mask) {
        sixel_allocator_free(storage->allocator, storage->transparent_mask);
        storage->coverage_extent = 0;
    }
    storage->transparent = transparency->transparent;
    storage->alpha_zero_is_transparent =
        transparency->alpha_zero_is_transparent;
    storage->transparent_mask = transparency->transparent_mask;
    storage->transparent_mask_size = transparency->transparent_mask_size;

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_frame_vtbl_get_alpha(sixel_frame_interface_t *frame,
                           sixel_frame_alpha_t *alpha)
{
    sixel_frame_t const *storage;

    if (frame == NULL || alpha == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    storage = sixel_frame_from_interface_const(frame);
    alpha->values = storage->alpha;
    alpha->count = storage->alpha_count;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_frame_vtbl_set_alpha(sixel_frame_interface_t *frame,
                           sixel_frame_alpha_t const *alpha)
{
    sixel_frame_t *storage;
    size_t count;
    size_t i;

    if (frame == NULL || alpha == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    storage = sixel_frame_from_interface(frame);
    if (alpha->values == NULL) {
        if (alpha->count != 0u) {
            return SIXEL_BAD_ARGUMENT;
        }
    } else {
        if (storage->width <= 0 || storage->height <= 0 ||
            (size_t)storage->width > SIZE_MAX / (size_t)storage->height ||
            (sixel_frame_format(storage) != SIXEL_PIXELFORMAT_RGBFLOAT32 &&
             sixel_frame_format(storage) !=
                 SIXEL_PIXELFORMAT_LINEARRGBFLOAT32) ||
            storage->transparent_mask != NULL || storage->transparent >= 0) {
            return SIXEL_BAD_ARGUMENT;
        }
        count = (size_t)storage->width * (size_t)storage->height;
        if (alpha->count != count || count > SIZE_MAX / sizeof(float)) {
            return SIXEL_BAD_ARGUMENT;
        }
        /* Validate first; failure preserves both owners. */
        for (i = 0u; i < count; ++i) {
            if (!isfinite(alpha->values[i]) || alpha->values[i] < 0.0f ||
                alpha->values[i] > 1.0f) {
                return SIXEL_BAD_INPUT;
            }
        }
    }
    if (storage->alpha != alpha->values) {
        sixel_allocator_free(storage->allocator, storage->alpha);
        storage->coverage_extent = 0;
    }
    storage->alpha = alpha->values;
    storage->alpha_count = alpha->count;
    return SIXEL_OK;
}

/* The plane survives animation composition; delivery resolves it once. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_frame_resolve_alpha(sixel_frame_t *frame,
                          float const *background, int preserve_zero)
{
    float *pixels;
    unsigned char *mask;
    size_t count;
    size_t i;
    int c;
    int hidden;
    int has_hidden;
    double alpha;
    double value;

    if (frame == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (frame->alpha == NULL) {
        return SIXEL_OK;
    }
    if (frame->format.layout != SIXEL_PIXELFORMAT_RGBFLOAT32 ||
        frame->format.coordinates != SIXEL_COLORSPACE_SRGB_LINEAR) {
        return SIXEL_BAD_ARGUMENT;
    }
    count = frame->alpha_count;
    if (count > SIZE_MAX / (3u * sizeof(float))) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    for (c = 0; background != NULL && c < 3; ++c) {
        if (!isfinite(background[c])) {
            return SIXEL_BAD_INPUT;
        }
    }
    pixels = (float *)sixel_allocator_malloc(frame->allocator,
                                             count * 3u * sizeof(float));
    mask = (unsigned char *)sixel_allocator_malloc(frame->allocator, count);
    if (pixels == NULL || mask == NULL) {
        sixel_allocator_free(frame->allocator, pixels);
        sixel_allocator_free(frame->allocator, mask);
        return SIXEL_BAD_ALLOCATION;
    }
    has_hidden = 0;
    for (i = 0u; i < count; ++i) {
        alpha = frame->alpha[i];
        hidden = alpha == 0.0 && (background == NULL || preserve_zero);
        mask[i] = (unsigned char)hidden;
        has_hidden |= hidden;
        for (c = 0; c < 3; ++c) {
            value = frame->pixels.f32ptr[i * 3u + c];
            if (!isfinite(value) || !isfinite(alpha) ||
                alpha < 0.0 || alpha > 1.0) {
                goto invalid;
            }
            if (background != NULL && !hidden) {
                value = value * alpha + background[c] * (1.0 - alpha);
            }
            pixels[i * 3u + c] = (float)value;
            if (!isfinite(pixels[i * 3u + c])) {
                goto invalid;
            }
        }
    }
    sixel_frame_release_owned_pixels(frame);
    frame->pixels.f32ptr = pixels;
    frame->pixel_extent = count * 3u * sizeof(float);
    frame->owns_pixels = 1;
    sixel_allocator_free(frame->allocator, frame->alpha);
    frame->alpha = NULL;
    frame->alpha_count = 0u;
    frame->coverage_extent = 0;
    sixel_allocator_free(frame->allocator, frame->transparent_mask);
    if (!has_hidden) {
        sixel_allocator_free(frame->allocator, mask);
        mask = NULL;
    }
    frame->transparent_mask = mask;
    frame->transparent_mask_size = has_hidden ? count : 0u;
    frame->alpha_zero_is_transparent = has_hidden;
    return SIXEL_OK;

invalid:
    sixel_allocator_free(frame->allocator, pixels);
    sixel_allocator_free(frame->allocator, mask);
    return SIXEL_BAD_INPUT;
}

/* Byte locations in an interleaved pixel. The gray layouts use the same
 * sample for all three output channels. */
typedef struct sixel_frame_alpha_layout {
    int stride;
    int alpha;
    int rgb[3];
} sixel_frame_alpha_layout_t;

static sixel_frame_alpha_layout_t const *
sixel_frame_alpha_layout(int format)
{
    static sixel_frame_alpha_layout_t const rgba = {4, 3, {0, 1, 2}};
    static sixel_frame_alpha_layout_t const argb = {4, 0, {1, 2, 3}};
    static sixel_frame_alpha_layout_t const bgra = {4, 3, {2, 1, 0}};
    static sixel_frame_alpha_layout_t const abgr = {4, 0, {3, 2, 1}};
    static sixel_frame_alpha_layout_t const ga = {2, 1, {0, 0, 0}};
    static sixel_frame_alpha_layout_t const ag = {2, 0, {1, 1, 1}};

    switch (format) {
    case SIXEL_PIXELFORMAT_RGBA8888: return &rgba;
    case SIXEL_PIXELFORMAT_ARGB8888: return &argb;
    case SIXEL_PIXELFORMAT_BGRA8888: return &bgra;
    case SIXEL_PIXELFORMAT_ABGR8888: return &abgr;
    case SIXEL_PIXELFORMAT_GA88: return &ga;
    case SIXEL_PIXELFORMAT_AG88: return &ag;
    default: return NULL;
    }
}

/* Packed inputs have byte coordinates, but resolving fractional coverage is
 * arithmetic, not a byte-storage operation. Publish both new planes together
 * so the encoder can plan float work without losing holes or borrowing data
 * that a later conversion frees. Loader-resolved RGB never enters here. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_frame_unpack_alpha(sixel_frame_t *frame,
                          unsigned char const *background,
                          int require_float)
{
    unsigned char const *source;
    void *pixels;
    unsigned char *mask;
    size_t count;
    size_t i;
    sixel_frame_alpha_layout_t const *layout;
    int depth;
    int channel;
    int interpret;
    int fractional;
    int hidden;
    unsigned int alpha;
    unsigned int bg[3];
    double value;

    if (frame == NULL || frame->pixels.u8ptr == NULL ||
            frame->format.coordinates != SIXEL_COLORSPACE_SRGB ||
            frame->alpha != NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    layout = sixel_frame_alpha_layout(frame->format.layout);
    if (layout == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    depth = layout->stride;
    if (frame->width <= 0 || frame->height <= 0 ||
            (size_t)frame->width > SIZE_MAX / (size_t)frame->height) {
        return SIXEL_BAD_INPUT;
    }
    count = (size_t)frame->width * (size_t)frame->height;
    if (count > SIZE_MAX / (3u * sizeof(float))) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    if ((frame->transparent_mask == NULL) !=
            (frame->transparent_mask_size == 0u) ||
            (frame->transparent_mask != NULL &&
             frame->transparent_mask_size < count)) {
        return SIXEL_BAD_INPUT;
    }
    interpret = frame->alpha_zero_is_transparent != 0;
    fractional = 0;
    source = frame->pixels.u8ptr;
    for (i = 0; interpret && i < count; ++i) {
        alpha = source[i * (size_t)depth + layout->alpha];
        if (alpha > 0u && alpha < 255u &&
                (frame->transparent_mask == NULL ||
                 frame->transparent_mask[i] == 0u)) {
            fractional = 1;
            break;
        }
    }
    require_float = require_float || fractional;
    for (channel = 0; channel < 3; ++channel) {
        bg[channel] = background == NULL ? 0u : background[channel];
    }
    if (depth == 2) {
        /* Match the packed gray compatibility contract before expansion. */
        bg[0] = (299u * bg[0] + 587u * bg[1] + 114u * bg[2] + 500u)
            / 1000u;
        bg[1] = bg[2] = bg[0];
    }
    pixels = sixel_allocator_malloc(frame->allocator,
        count * 3u * (require_float ? sizeof(float) : 1u));
    mask = interpret || frame->transparent_mask != NULL
        ? sixel_allocator_malloc(frame->allocator, count) : NULL;
    if (pixels == NULL ||
            ((interpret || frame->transparent_mask != NULL) && mask == NULL)) {
        sixel_allocator_free(frame->allocator, pixels);
        sixel_allocator_free(frame->allocator, mask);
        return SIXEL_BAD_ALLOCATION;
    }
    for (i = 0; i < count; ++i) {
        alpha = source[i * (size_t)depth + layout->alpha];
        hidden = (interpret && alpha == 0u) ||
            (frame->transparent_mask != NULL &&
             frame->transparent_mask[i] != 0u);
        if (mask != NULL) {
            mask[i] = (unsigned char)hidden;
        }
        for (channel = 0; channel < 3; ++channel) {
            value = source[i * (size_t)depth + layout->rgb[channel]];
            if (interpret && !hidden && alpha < 255u) {
                value = (value * alpha + bg[channel] * (255u - alpha))
                    / 255.0;
            }
            if (require_float) {
                ((float *)pixels)[i * 3u + channel] = (float)(value / 255.0);
            } else {
                ((unsigned char *)pixels)[i * 3u + channel] =
                    (unsigned char)value;
            }
        }
    }
    sixel_frame_release_owned_pixels(frame);
    frame->pixels.u8ptr = pixels;
    frame->pixel_extent = count * 3u * (require_float ? sizeof(float) : 1u);
    frame->owns_pixels = 1;
    sixel_allocator_free(frame->allocator, frame->transparent_mask);
    frame->transparent_mask = mask;
    frame->transparent_mask_size = mask != NULL ? count : 0u;
    frame->coverage_extent = 0;
    sixel_frame_assign_format(frame, require_float
        ? SIXEL_PIXELFORMAT_RGBFLOAT32 : SIXEL_PIXELFORMAT_RGB888);
    return SIXEL_OK;
}


static SIXELSTATUS
sixel_frame_vtbl_resize(sixel_frame_interface_t *frame,
                        int width,
                        int height,
                        int method_for_resampling)
{
    sixel_frame_t *storage;

    if (frame == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    storage = sixel_frame_from_interface(frame);
    return sixel_frame_resize(storage, width, height, method_for_resampling);
}

static SIXELSTATUS
sixel_frame_vtbl_resize_float32(sixel_frame_interface_t *frame,
                                int width,
                                int height,
                                int method_for_resampling)
{
    sixel_frame_t *storage;

    if (frame == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    storage = sixel_frame_from_interface(frame);
    return sixel_frame_resize_float32(storage,
                                      width,
                                      height,
                                      method_for_resampling);
}

static SIXELSTATUS
sixel_frame_vtbl_clip(sixel_frame_interface_t *frame,
                      int x,
                      int y,
                      int width,
                      int height)
{
    sixel_frame_t *storage;

    if (frame == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    storage = sixel_frame_from_interface(frame);
    return sixel_frame_clip(storage, x, y, width, height);
}

static SIXELSTATUS
sixel_frame_vtbl_measure_storage(sixel_frame_interface_t *frame,
                                 size_t *storage_bytes)
{
    sixel_frame_t const *storage;

    if (frame == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    storage = sixel_frame_from_interface_const(frame);
    return sixel_frame_measure_storage(storage, storage_bytes);
}

static SIXELSTATUS
sixel_frame_vtbl_clone(sixel_frame_interface_t *frame,
                       sixel_allocator_t *allocator,
                       sixel_frame_interface_t **frame_out)
{
    SIXELSTATUS status;
    sixel_frame_t const *storage;
    sixel_frame_t *clone;

    status = SIXEL_FALSE;
    storage = NULL;
    clone = NULL;

    if (frame == NULL || frame_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *frame_out = NULL;
    storage = sixel_frame_from_interface_const(frame);
    status = sixel_frame_clone(storage, allocator, &clone, NULL);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    *frame_out = sixel_frame_as_interface(clone);
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_frame_get_pixels_view(sixel_frame_t const *frame,
                            sixel_frame_pixels_view_t *view)
{
    if (frame == NULL || view == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (frame->format.layout == SIXEL_PIXELFORMAT_RGBFLOAT32) {
        view->pixels = NULL;
        view->pixels_float32 = frame->pixels.f32ptr;
    } else {
        view->pixels = frame->pixels.u8ptr;
        view->pixels_float32 = NULL;
    }
    view->palette = frame->palette;
    view->width = frame->width;
    view->height = frame->height;
    view->pixelformat = frame->format.layout;
    view->colorspace = frame->format.coordinates;
    view->ncolors = frame->ncolors;
    view->rgb_definition = frame->format.rgb;

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_frame_get_timeline(sixel_frame_t const *frame,
                         sixel_frame_timeline_t *timeline)
{
    if (frame == NULL || timeline == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    timeline->delay = frame->delay;
    timeline->frame_no = frame->frame_no;
    timeline->loop_count = frame->loop_count;
    timeline->multiframe = frame->multiframe;
    timeline->handoff_shareable = frame->handoff_shareable;

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_frame_get_transparency(
    sixel_frame_t const *frame,
    sixel_frame_transparency_t *transparency)
{
    if (frame == NULL || transparency == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    transparency->transparent = frame->transparent;
    transparency->alpha_zero_is_transparent =
        frame->alpha_zero_is_transparent;
    transparency->transparent_mask = frame->transparent_mask;
    transparency->transparent_mask_size = frame->transparent_mask_size;

    return SIXEL_OK;
}

/* The SIXEL loader preserves the declared color count in the public view.
 * Cloning and expanding the storage also need its reserved unpainted slot. */
static int
sixel_frame_palette_entries(sixel_frame_t const *frame)
{
    int entries, bits, mask, x, y;
    unsigned char const *row;
    int index;

    entries = frame->palette_extent > frame->ncolors
        ? frame->palette_extent : frame->ncolors;
    if (frame->ncolors != -1 || frame->palette == NULL ||
        frame->pixels.u8ptr == NULL) {
        return entries;
    }
    /* With no declared count, copy through the highest referenced entry.
     * Unused padding bits and unused registers do not establish capacity. */
    switch (frame->format.layout) {
    case SIXEL_PIXELFORMAT_PAL1: bits = 1; break;
    case SIXEL_PIXELFORMAT_PAL2: bits = 2; break;
    case SIXEL_PIXELFORMAT_PAL4: bits = 4; break;
    case SIXEL_PIXELFORMAT_PAL8: bits = 8; break;
    default: return entries;
    }
    mask = (1 << bits) - 1;
    row = frame->pixels.u8ptr;
    for (y = 0; y < frame->height && entries <= mask; ++y) {
        for (x = 0; x < frame->width && entries <= mask; ++x) {
            index = (row[x / (8 / bits)] >>
                (8 - bits - x % (8 / bits) * bits)) & mask;
            if (index >= entries) {
                entries = index + 1;
            }
        }
        row += ((size_t)frame->width + 8 / bits - 1) / (8 / bits);
    }
    return entries;
}

/* A raw SIXEL image can use 255 for an unpainted pixel even when the
 * public palette count is smaller. In that case its undeclared register
 * has the decoder's default gray (last of 24 steps of 11). A caller may
 * have copied only the declared entries, so do not index past them. */
static unsigned char const *
sixel_frame_indexed_color(sixel_frame_t const *frame, int entries,
                          unsigned char index)
{
    static unsigned char const unpainted[3] = {253, 253, 253};

    if (entries > 0 && index >= entries) {
        return index == 255 && frame->format.layout == SIXEL_PIXELFORMAT_PAL8
            ? unpainted : NULL;
    }
    return frame->palette + 3u * index;
}

/*
 * Return the byte footprint owned by the frame storage itself.  Replay caches
 * use this to decide whether keeping a decoded frame by reference is bounded.
 */
static SIXELSTATUS
sixel_frame_measure_storage(sixel_frame_t const *frame,
                            size_t *storage_bytes)
{
    SIXELSTATUS status;
    sixel_frame_pixels_view_t view;
    sixel_frame_transparency_t transparency;
    int entries;
    size_t pixel_bytes;
    size_t palette_bytes;
    size_t coverage_bytes;
    size_t total;

    status = SIXEL_BAD_ARGUMENT;
    memset(&view, 0, sizeof(view));
    memset(&transparency, 0, sizeof(transparency));
    pixel_bytes = 0u;
    palette_bytes = 0u;
    coverage_bytes = 0u;
    total = 0u;

    if (frame == NULL || storage_bytes == NULL) {
        return status;
    }
    *storage_bytes = 0u;

    status = sixel_frame_get_pixels_view(frame, &view);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    status = sixel_frame_get_transparency(frame, &transparency);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    pixel_bytes = frame->pixel_extent;
    entries = view.ncolors == -1 && frame->palette_extent > 0
        ? frame->palette_extent : sixel_frame_palette_entries(frame);
    if (view.palette != NULL && entries > 0) {
        if (entries > SIXEL_PALETTE_MAX) {
            return SIXEL_BAD_INPUT;
        }
        palette_bytes = (size_t)entries * 3u;
    }
    if (transparency.transparent_mask != NULL &&
        transparency.transparent_mask_size > 0u) {
        coverage_bytes = transparency.transparent_mask_size;
    }

    total = frame->ycbcr != NULL
        ? sixel_cdef_storage(frame->ycbcr->rgb)
        : sixel_cdef_storage(view.rgb_definition);
    if (frame->ycbcr != NULL) {
        if (total > SIZE_MAX - sizeof(*frame->ycbcr)) {
            return SIXEL_BAD_INTEGER_OVERFLOW;
        }
        total += sizeof(*frame->ycbcr);
    }
    if (pixel_bytes > SIZE_MAX - total) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    total += pixel_bytes;
    if (total > SIZE_MAX - palette_bytes) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    total += palette_bytes;
    if (frame->alpha_count > (SIZE_MAX - coverage_bytes) / sizeof(float)) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    coverage_bytes += frame->alpha_count * sizeof(float);
    if (coverage_bytes < frame->coverage_extent) {
        coverage_bytes = frame->coverage_extent;
    }
    if (total > SIZE_MAX - coverage_bytes) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    total += coverage_bytes;

    *storage_bytes = total;
    return SIXEL_OK;
}

/* Copy frame metadata and coverage for a private worker frame. Its pixels
 * can be copied, decoded from packed rows, or shared with a retained
 * immutable source. Keep handoff metadata private in all cases. */
static SIXELSTATUS
sixel_frame_clone(sixel_frame_t const *frame,
                  sixel_allocator_t *allocator,
                  sixel_frame_t **frame_out,
                  sixel_frame_clone_options_t const *options)
{
    SIXELSTATUS status;
    sixel_frame_t *clone;
    sixel_frame_pixels_view_t view;
    sixel_frame_timeline_t timeline;
    sixel_frame_transparency_t transparency;
    unsigned char const *source_pixels;
    unsigned char *pixels;
    unsigned char *palette;
    unsigned char *mask;
    int entries;
    size_t pixel_bytes;
    size_t palette_bytes;
    size_t mask_bytes;
    int row, column, decode_step;

    decode_step = options == NULL ? 0 : options->step;
    status = SIXEL_BAD_ARGUMENT;
    clone = NULL;
    source_pixels = NULL;
    pixels = NULL;
    palette = NULL;
    mask = NULL;
    pixel_bytes = 0u;
    palette_bytes = 0u;
    mask_bytes = 0u;
    memset(&view, 0, sizeof(view));
    memset(&timeline, 0, sizeof(timeline));
    memset(&transparency, 0, sizeof(transparency));

    if (frame == NULL || frame_out == NULL ||
        (decode_step && frame->ycbcr == NULL)) {
        return status;
    }
    *frame_out = NULL;

    status = sixel_frame_get_pixels_view(frame, &view);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    status = sixel_frame_get_timeline(frame, &timeline);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    status = sixel_frame_get_transparency(frame, &transparency);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    status = sixel_frame_new(&clone, allocator);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    /*
     * Keep the constructor contract explicit. GCC's analyzer loses the
     * relation between the returned status and the out pointer across
     * sixel_frame_new(), and this also protects future constructor changes.
     */
    if (clone == NULL) {
        sixel_helper_set_additional_message(
            "sixel_frame_clone: sixel_frame_new() returned null frame.");
        return SIXEL_BAD_ALLOCATION;
    }

    clone->width = view.width;
    clone->height = view.height;
    clone->format = frame->format;
    clone->cms_history = frame->cms_history;
    if (frame->format.rgb != NULL) {
        clone->format.rgb = sixel_cdef_clone(frame->format.rgb);
        if (clone->format.rgb == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto error;
        }
    }
    clone->ncolors = view.ncolors;
    clone->palette_extent = frame->palette_extent;
    clone->transparent = transparency.transparent;
    clone->alpha_zero_is_transparent =
        transparency.alpha_zero_is_transparent;
    clone->transparent_mask = NULL;
    clone->transparent_mask_size = 0u;
    clone->delay = timeline.delay;
    clone->frame_no = timeline.frame_no;
    clone->loop_count = timeline.loop_count;
    clone->multiframe = timeline.multiframe;
    clone->handoff_shareable = 0;
    clone->ycbcr_counts = frame->ycbcr_counts;

    entries = sixel_frame_palette_entries(frame);
    if (view.palette != NULL && entries > 0) {
        if (entries > SIXEL_PALETTE_MAX) {
            status = SIXEL_BAD_INPUT;
            goto error;
        }
        palette_bytes = (size_t)entries * 3u;
        palette = (unsigned char *)sixel_allocator_malloc(clone->allocator,
                                                          palette_bytes);
        if (palette == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto error;
        }
        memcpy(palette, view.palette, palette_bytes);
        clone->palette_extent = entries;
        clone->palette = palette;
        clone->owns_palette = 1;
        palette = NULL;
    }

    if (view.width < 0 || view.height < 0) {
        status = SIXEL_BAD_INPUT;
        goto error;
    }

    if (decode_step) {
        clone->width = (view.width - 1) / decode_step + 1;
        clone->height = (view.height - 1) / decode_step + 1;
        status = sixel_frame_decode_ycbcr422_into(frame, clone, decode_step,
            options->grid, options->grid_step);
        if (SIXEL_FAILED(status)) {
            goto error;
        }
    } else if (options != NULL && options->owner != NULL) {
        clone->pixel_owner = options->owner;
        sixel_frame_ref(clone->pixel_owner);
        clone->pixels.f32ptr = frame->pixels.f32ptr;
        clone->pixel_extent = frame->pixel_extent;
        clone->owns_pixels = 0;
    } else if (view.width > 0 && view.height > 0) {
        if (frame->ycbcr != NULL) {
            /* Dimension setters keep this rectangle inside the original
             * active rows. Drop all row padding when making an owned copy. */
            clone->ycbcr_plane.stride_bytes = (ptrdiff_t)view.width * 2;
            pixel_bytes = (size_t)clone->ycbcr_plane.stride_bytes * view.height;
            clone->packed_width = view.width;
            clone->packed_height = view.height;
            clone->ycbcr = frame->ycbcr;
            (void)sixel_atomic_fetch_add_u32(&clone->ycbcr->ref, 1U);
        } else {
            status = sixel_pixelformat_buffer_bytes(view.width, view.height,
                view.pixelformat, &pixel_bytes);
            if (SIXEL_FAILED(status)) {
                status = SIXEL_BAD_INPUT;
                goto error;
            }
        }
        if (SIXEL_WORKFORMAT_IS_FLOAT32(view.pixelformat)) {
            source_pixels =
                (unsigned char const *)(void const *)view.pixels_float32;
        } else {
            source_pixels = view.pixels;
        }
        if (source_pixels == NULL) {
            status = SIXEL_BAD_INPUT;
            goto error;
        }
        pixels = (unsigned char *)sixel_allocator_malloc(clone->allocator,
                                                         pixel_bytes);
        if (pixels == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto error;
        }
        if (frame->ycbcr_counts != NULL) {
            if (frame->ycbcr != NULL) {
                (void)sixel_atomic_fetch_add_u32(
                    &frame->ycbcr_counts->packed_copies, 1U);
            } else if (SIXEL_WORKFORMAT_IS_FLOAT32(view.pixelformat)) {
                (void)sixel_atomic_fetch_add_u32(
                    &frame->ycbcr_counts->rgb_copies, 1U);
            }
        }
        if (frame->ycbcr != NULL &&
                frame->ycbcr_plane.stride_bytes !=
                    clone->ycbcr_plane.stride_bytes) {
            for (row = 0; row < view.height; ++row) {
                memcpy(pixels + row * clone->ycbcr_plane.stride_bytes,
                       source_pixels + row * frame->ycbcr_plane.stride_bytes,
                       (size_t)clone->ycbcr_plane.stride_bytes);
            }
        } else {
            memcpy(pixels, source_pixels, pixel_bytes);
        }
        if (SIXEL_WORKFORMAT_IS_FLOAT32(view.pixelformat)) {
            clone->pixels.f32ptr = (float *)(void *)pixels;
        } else {
            clone->pixels.u8ptr = pixels;
        }
        if (clone->ycbcr != NULL) {
            clone->ycbcr_plane.base = pixels;
            clone->ycbcr_plane.buffer_bytes = pixel_bytes;
        }
        clone->pixel_extent = pixel_bytes;
        clone->owns_pixels = 1;
        pixels = NULL;
    }

    if (transparency.transparent_mask != NULL &&
        transparency.transparent_mask_size > 0u) {
        if (decode_step > 1 && transparency.transparent_mask_size !=
                (size_t)view.width * (size_t)view.height) {
            status = SIXEL_BAD_ARGUMENT;
            goto error;
        }
        mask_bytes = decode_step > 1
            ? (size_t)clone->width * (size_t)clone->height
            : transparency.transparent_mask_size;
        mask = (unsigned char *)sixel_allocator_malloc(clone->allocator,
                                                       mask_bytes);
        if (mask == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto error;
        }
        if (decode_step > 1) {
            for (row = 0; row < clone->height; ++row) {
                for (column = 0; column < clone->width; ++column) {
                    mask[(size_t)row * clone->width + column] =
                        transparency.transparent_mask[
                            (size_t)row * decode_step * view.width +
                            (size_t)column * decode_step];
                }
            }
        } else {
            memcpy(mask, transparency.transparent_mask, mask_bytes);
        }
        clone->transparent_mask = mask;
        clone->transparent_mask_size = mask_bytes;
        mask = NULL;
    }

    if (frame->alpha != NULL) {
        clone->alpha = (float *)sixel_allocator_malloc(clone->allocator,
            frame->alpha_count * sizeof(float));
        if (clone->alpha == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto error;
        }
        memcpy(clone->alpha, frame->alpha,
               frame->alpha_count * sizeof(float));
        clone->alpha_count = frame->alpha_count;
    }

    *frame_out = clone;
    return SIXEL_OK;

error:
    sixel_allocator_free(clone->allocator, pixels);
    sixel_allocator_free(clone->allocator, palette);
    sixel_allocator_free(clone->allocator, mask);
    sixel_frame_unref(clone);
    return status;
}

SIXEL_INTERNAL_API void
sixel_frame_observe_ycbcr422(sixel_frame_t *frame,
                            sixel_ycbcr422_counts_t *counts)
{
    frame->ycbcr_counts = counts;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_frame_decode_ycbcr422_copy(sixel_frame_t const *frame,
                                sixel_allocator_t *allocator,
                                sixel_frame_t **frame_out)
{
    return sixel_frame_sample_ycbcr422(frame, allocator, 1, NULL, 0, frame_out);
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_frame_view_profile_rgb(sixel_frame_t *frame,
    sixel_allocator_t *allocator, sixel_frame_t **result)
{
    sixel_frame_clone_options_t options = {0};

    if (frame == NULL || frame->format.rgb == NULL ||
            frame->format.layout != SIXEL_PIXELFORMAT_RGBFLOAT32 ||
            frame->pixels.f32ptr == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    options.owner = frame;
    return sixel_frame_clone(frame, allocator, result, &options);
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_frame_sample_ycbcr422(sixel_frame_t const *frame,
    sixel_allocator_t *allocator, int step, sixel_frame_t const *grid,
    int grid_step, sixel_frame_t **frame_out)
{
    sixel_frame_clone_options_t options = {0};

    if (frame == NULL || frame->ycbcr == NULL || step < 1 ||
            (grid != NULL && (step != 1 || grid_step < 2 ||
             grid->format.layout != SIXEL_PIXELFORMAT_RGBFLOAT32 ||
             grid->pixels.f32ptr == NULL ||
             grid->format.rgb != frame->ycbcr->rgb ||
             grid->width != (frame->width - 1) / grid_step + 1 ||
             grid->height != (frame->height - 1) / grid_step + 1))) {
        return SIXEL_BAD_ARGUMENT;
    }
    options.step = step;
    options.grid = grid;
    options.grid_step = grid_step;
    return sixel_frame_clone(frame, allocator, frame_out, &options);
}

/* constructor of frame object */
SIXELAPI SIXELSTATUS
sixel_frame_new(
    sixel_frame_t       /* out */ **ppframe,    /* frame object to be created */
    sixel_allocator_t   /* in */  *allocator)   /* allocator, null if you use
                                                   default allocator */
{
    SIXELSTATUS status = SIXEL_FALSE;
    int created_allocator;

    created_allocator = 0;

    if (allocator == NULL) {
        status = sixel_allocator_new(&allocator, malloc, calloc, realloc, free);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        created_allocator = 1;
    }

    /*
     * Zero-initialize the frame to avoid MSan reports when fields are read
     * before explicit assignment in loader paths.
     */
    *ppframe = (sixel_frame_t *)sixel_allocator_calloc(
        allocator,
        1,
        sizeof(sixel_frame_t));
    if (*ppframe == NULL) {
        sixel_helper_set_additional_message(
            "sixel_frame_resize: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    (*ppframe)->frame_interface.vtbl = &g_sixel_frame_vtbl;
    (*ppframe)->ref = 1U;
    (*ppframe)->pixels.u8ptr = NULL;
    (*ppframe)->palette = NULL;
    /*
     * Most loader paths still assign freshly allocated storage directly into
     * the private frame. Keep direct-assignment storage owned by default; the
     * explicit borrowed initializer flips these flags for caller-owned memory.
     */
    (*ppframe)->owns_pixels = 1;
    (*ppframe)->owns_palette = 1;
    (*ppframe)->width = 0;
    (*ppframe)->height = 0;
    (*ppframe)->ncolors = (-1);
    (*ppframe)->palette_extent = 0;
    (*ppframe)->cms_history = 0u;
    (*ppframe)->format.rgb = NULL;
    sixel_frame_assign_format(*ppframe, SIXEL_PIXELFORMAT_RGB888);
    (*ppframe)->delay = 0;
    (*ppframe)->frame_no = 0;
    (*ppframe)->loop_count = 0;
    (*ppframe)->multiframe = 0;
    (*ppframe)->handoff_shareable = 0;
    (*ppframe)->transparent = (-1);
    (*ppframe)->alpha_zero_is_transparent = 0;
    (*ppframe)->transparent_mask = NULL;
    (*ppframe)->transparent_mask_size = 0u;
    (*ppframe)->allocator = allocator;

    sixel_allocator_ref(allocator);

    /* Normalize between byte and float pipelines when buffers are present. */
    status = SIXEL_OK;

end:
    /* The frame retains its own reference on success. Release the temporary
     * constructor reference on both success and allocation failure. */
    if (created_allocator) {
        sixel_allocator_unref(allocator);
    }
    return status;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_frame_factory_new(sixel_allocator_t *allocator, void **object)
{
    SIXELSTATUS status;
    sixel_frame_t *frame;

    status = SIXEL_FALSE;
    frame = NULL;

    if (object == NULL) {
        sixel_helper_set_additional_message(
            "sixel_frame_factory_new: object is null.");
        return SIXEL_BAD_ARGUMENT;
    }

    *object = NULL;
    status = sixel_frame_new(&frame, allocator);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    *object = sixel_frame_as_interface(frame);
    return SIXEL_OK;
}


SIXELAPI /* deprecated */ sixel_frame_t *
sixel_frame_create(void)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_frame_t *frame = NULL;

    status = sixel_frame_new(&frame, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

end:
    return frame;
}


static void
sixel_frame_destroy(sixel_frame_t /* in */ *frame)
{
    sixel_allocator_t *allocator = NULL;

    if (frame) {
        allocator = frame->allocator;
        sixel_frame_release_owned_pixels(frame);
        sixel_frame_release_owned_palette(frame);
        sixel_allocator_free(allocator, frame->transparent_mask);
        sixel_allocator_free(allocator, frame->alpha);
        sixel_cdef_free(frame->format.rgb);
        sixel_ycbcr_def_free(frame->ycbcr);
        sixel_allocator_free(allocator, frame);
        sixel_allocator_unref(allocator);
    }
}


/* increase reference count of frame object (thread-safe) */
SIXELAPI void
sixel_frame_ref(sixel_frame_t *frame)
{
    if (frame == NULL) {
        return;
    }

    (void)sixel_atomic_fetch_add_u32(&frame->ref, 1U);
}


/* decrease reference count of frame object (thread-safe) */
SIXELAPI void
sixel_frame_unref(sixel_frame_t *frame)
{
    unsigned int previous;

    if (frame == NULL) {
        return;
    }

    previous = sixel_atomic_fetch_sub_u32(&frame->ref, 1U);
    if (previous == 1U) {
        sixel_frame_destroy(frame);
    }
}


/* Publish fully validated storage. No fallible operations after this point. */
static void
sixel_frame_install_pixels(sixel_frame_t *frame, void *pixels,
    int width, int height, int pixelformat, unsigned char *palette,
    int ncolors, int is_float, int owns_pixels, int owns_palette,
    size_t pixel_bytes)
{
    int old_pixels_owned;
    int old_palette_owned;

    old_pixels_owned = frame->owns_pixels;
    old_palette_owned = frame->owns_palette;
    sixel_frame_release_replaced_pixels(frame, pixels);
    if (is_float != 0) {
        frame->pixels.f32ptr = (float *)pixels;
    } else {
        frame->pixels.u8ptr = (unsigned char *)pixels;
    }
    /* Retain an already owned allocation on same-pointer initialization.
     * A borrow declares a view; adoption records the transferred extent. */
    if (pixels == NULL) {
        frame->pixel_extent = 0u;
    } else if (!owns_pixels || !old_pixels_owned ||
               frame->pixel_extent == 0u) {
        frame->pixel_extent = pixel_bytes;
    }
    frame->owns_pixels = owns_pixels != 0 ? 1 : 0;
    frame->width = width;
    frame->height = height;
    sixel_frame_assign_format(frame, pixelformat);
    frame->cms_history = 0u;
    sixel_frame_release_replaced_palette(frame, palette, old_palette_owned);
    frame->palette = palette;
    frame->owns_palette = owns_palette != 0 ? 1 : 0;
    sixel_frame_set_ncolors(frame, ncolors);
    if (ncolors == -1 && palette != NULL) {
        /* The initial indices bound the implicit palette. Later logical
         * dimensions can hide entries without releasing their storage. */
        frame->palette_extent = sixel_frame_palette_entries(frame);
    }
    frame->alpha_zero_is_transparent = 0;
    if (frame->transparent_mask != NULL) {
        sixel_allocator_free(frame->allocator, frame->transparent_mask);
        frame->transparent_mask = NULL;
    }
    frame->transparent_mask_size = 0u;
    sixel_allocator_free(frame->allocator, frame->alpha);
    frame->alpha = NULL;
    frame->alpha_count = 0u;
    frame->coverage_extent = 0;
}

/*
 * Shared initializer for both byte and float32 buffers.  Keeping the
 * validation and field updates in one place avoids diverging logic
 * between the two entry points while making the stored pointer type
 * explicit.
 */
static SIXELSTATUS
sixel_frame_init_common(
    sixel_frame_t   *frame,
    void            *pixels,
    int              width,
    int              height,
    int              pixelformat,
    unsigned char   *palette,
    int              ncolors,
    int              is_float,
    int              owns_pixels,
    int              owns_palette)
{
    SIXELSTATUS status = SIXEL_FALSE;
    size_t pixel_bytes;

    if (frame == NULL) {
        sixel_helper_set_additional_message(
            "sixel_frame_init: frame is null.");
        return SIXEL_BAD_ARGUMENT;
    }

    sixel_frame_ref(frame);

    if (sixel_pixelformat_is_ycbcr422(pixelformat)) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    /* check parameters */
    if (width <= 0) {
        sixel_helper_set_additional_message(
            "sixel_frame_init: an invalid width parameter detected.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    status = sixel_frame_validate_size("sixel_frame_init",
                                       width,
                                       height,
                                       pixelformat,
                                       NULL,
                                       &pixel_bytes,
                                       NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (height <= 0) {
        sixel_helper_set_additional_message(
            "sixel_frame_init: an invalid width parameter detected.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (width > SIXEL_WIDTH_LIMIT) {
        sixel_helper_set_additional_message(
            "sixel_frame_init: given width parameter is too huge.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (height > SIXEL_HEIGHT_LIMIT) {
        sixel_helper_set_additional_message(
            "sixel_frame_init: given height parameter is too huge.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (is_float != 0 && !SIXEL_WORKFORMAT_IS_FLOAT32(pixelformat)) {
        sixel_helper_set_additional_message(
            "sixel_frame_init: pixelformat must be float32 when "
            "supplying float pixels.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    /* Bit-packed input has padding at the end of each row. */
    if (sixel_frame_unpacked_format(pixelformat) != pixelformat) {
        status = sixel_pixelformat_buffer_bytes(width, height, pixelformat,
                                               &pixel_bytes);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }
    sixel_frame_install_pixels(frame, pixels, width, height, pixelformat,
        palette, ncolors, is_float, owns_pixels, owns_palette, pixel_bytes);
    status = SIXEL_OK;

end:
    sixel_frame_unref(frame);

    return status;
}

/* initialize frame object with a pixel buffer */
SIXELAPI SIXELSTATUS
sixel_frame_init(
    sixel_frame_t   /* in */ *frame,
    unsigned char   /* in */ *pixels,
    int             /* in */ width,
    int             /* in */ height,
    int             /* in */ pixelformat,
    unsigned char   /* in */ *palette,
    int             /* in */ ncolors)
{
    return sixel_frame_init_common(frame,
                                   pixels,
                                   width,
                                   height,
                                   pixelformat,
                                   palette,
                                   ncolors,
                                   0,
                                   1,
                                   1);
}

/* initialize frame object with caller-owned mutable pixel storage */
SIXELAPI SIXELSTATUS
sixel_frame_init_borrowed(
    sixel_frame_t   /* in */ *frame,
    void            /* in */ *pixels,
    int             /* in */ width,
    int             /* in */ height,
    int             /* in */ pixelformat,
    unsigned char   /* in */ *palette,
    int             /* in */ ncolors)
{
    return sixel_frame_init_common(
        frame,
        pixels,
        width,
        height,
        pixelformat,
        palette,
        ncolors,
        SIXEL_WORKFORMAT_IS_FLOAT32(pixelformat) ? 1 : 0,
        0,
        0);
}

SIXELAPI sixel_color_def_t *
sixel_color_def_new_named(char const *name)
{
    sixel_color_def_t *srgb;
    sixel_color_def_t *linear;

    if (name == NULL || strcmp(name, "srgb-linear") != 0) {
        return sixel_cdef_name(name);
    }
    srgb = sixel_cdef_name("srgb");
    linear = srgb != NULL ? sixel_cdef_linearized(srgb) : NULL;
    sixel_cdef_free(srgb);
    return linear;
}

SIXELAPI sixel_color_def_t *
sixel_color_def_new_icc(void const *bytes, size_t length)
{
    return sixel_cdef_icc(bytes, length);
}

SIXELAPI void
sixel_color_def_free(sixel_color_def_t *definition)
{
    sixel_cdef_free(definition);
}

SIXELAPI SIXELSTATUS
sixel_frame_init_rgbf32_borrowed(sixel_frame_t *frame, float *pixels,
                               int width, int height,
                               sixel_color_def_t const *definition)
{
    sixel_color_def_t *copy;
    SIXELSTATUS status;
    uintptr_t storage;
    uintptr_t input;

    if (frame == NULL || pixels == NULL || definition == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    /* Reinitialization releases owned storage, including any borrowed slice. */
    storage = (uintptr_t)sixel_frame_pixel_storage(frame);
    input = (uintptr_t)pixels;
    if (frame->owns_pixels && input >= storage &&
        (input == storage || input - storage < frame->pixel_extent)) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (!sixel_cdef_frame_input_ready(definition)) {
        sixel_helper_set_additional_message(
            "RGB definition has no frame input path.");
        return SIXEL_BAD_INPUT;
    }
    /* Prepare the color definition before replacing live frame storage. */
    copy = sixel_cdef_clone(definition);
    if (copy == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    status = sixel_frame_init_common(frame, pixels, width, height,
                                     SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, -1,
                                     1, 0, 0);
    if (SIXEL_FAILED(status)) {
        sixel_cdef_free(copy);
        return status;
    }
    frame->format.coordinates = SIXEL_FRAME_COLORSPACE_PROFILE_RGB;
    frame->format.rgb = copy;
    frame->transparent = -1;
    return SIXEL_OK;
}

SIXELAPI sixel_ycbcr_def_t *
sixel_ycbcr_def_new(sixel_ycbcr_def_config_t const *config)
{
    sixel_ycbcr_def_t *definition;
    sixel_suboption_value_t chroma;
    char diagnostic[96] = {0};

    if (config == NULL || config->struct_size < sizeof(*config) ||
        (config->encoding != SIXEL_YCBCR_ENCODING_BT601 &&
         config->encoding != SIXEL_YCBCR_ENCODING_BT709 &&
         config->encoding != SIXEL_YCBCR_ENCODING_BT2020_NCL) ||
        (config->range != SIXEL_YCBCR_RANGE_LIMITED &&
         config->range != SIXEL_YCBCR_RANGE_FULL) ||
        (config->chroma_x_half != 0 && config->chroma_x_half != 1) ||
        config->chroma_y_half != 0 ||
        !sixel_cdef_frame_input_ready(config->rgb)) {
        return NULL;
    }
    /* Read the stream's policy once before any frame is initialized. */
    chroma.int_value = 0;
    if (sixel_option_resolve_scalar_environment(
            SIXEL_OPTION_SCHEMA_YCBCR_CHROMA, &chroma,
            diagnostic, sizeof(diagnostic)) < SIXEL_OPTION_ENVIRONMENT_UNSET) {
        sixel_helper_set_additional_message(diagnostic);
        return NULL;
    }
    definition = malloc(sizeof(*definition));
    if (definition != NULL) {
        definition->rgb = sixel_cdef_clone(config->rgb);
        if (definition->rgb == NULL) {
            free(definition);
            return NULL;
        }
        definition->ref = 1U;
        definition->encoding = config->encoding;
        definition->range = config->range;
        definition->chroma_x_half = config->chroma_x_half;
        definition->chroma_y_half = config->chroma_y_half;
        definition->nearest_chroma = chroma.int_value;
    }
    return definition;
}

SIXELAPI SIXELSTATUS
sixel_ycbcr_def_get(sixel_ycbcr_def_t const *definition,
                   sixel_ycbcr_def_config_t *config)
{
    if (definition == NULL || config == NULL ||
            config->struct_size < sizeof(*config)) {
        return SIXEL_BAD_ARGUMENT;
    }
    config->encoding = definition->encoding;
    config->range = definition->range;
    config->chroma_x_half = definition->chroma_x_half;
    config->chroma_y_half = definition->chroma_y_half;
    config->rgb = definition->rgb;
    return SIXEL_OK;
}

SIXELAPI void
sixel_ycbcr_def_free(sixel_ycbcr_def_t *definition)
{
    if (definition != NULL &&
        sixel_atomic_fetch_sub_u32(&definition->ref, 1U) == 1U) {
        sixel_cdef_free(definition->rgb);
        free(definition);
    }
}

/* Reinitialization releases every owned plane, including slices that a
 * caller may have obtained through the getters. Avoid end-address overflow. */
static int
sixel_frame_ranges_overlap(void const *a, size_t na, void const *b, size_t nb)
{
    uintptr_t x, y;

    if (a == NULL || b == NULL) {
        return 0;
    }
    x = (uintptr_t)a;
    y = (uintptr_t)b;
    return x == y || (x < y ? y - x < na : x - y < nb);
}

SIXELAPI SIXELSTATUS
sixel_frame_init_ycbcr_borrowed(sixel_frame_t *frame,
    sixel_ycbcr_frame_view_t const *view)
{
    sixel_frame_plane_view_t plane;
    sixel_ycbcr_def_t *definition;
    size_t row, pitch, available, span, palette_bytes, mask_bytes, alpha_bytes;
    void *pixels;

    if (frame == NULL || view == NULL || view->struct_size < sizeof(*view) ||
        view->definition == NULL || view->plane_count != 1 ||
        !sixel_pixelformat_is_ycbcr422(view->pixelformat) ||
        view->width <= 0 || view->width > SIXEL_WIDTH_LIMIT ||
        (view->width & 1) != 0 ||
        view->height <= 0 || view->height > SIXEL_HEIGHT_LIMIT) {
        return SIXEL_BAD_ARGUMENT;
    }
    plane = view->planes[0];
    row = (size_t)view->width * 2u;
    if (plane.base == NULL || plane.buffer_bytes < row ||
        plane.buffer_bytes > UINTPTR_MAX - (uintptr_t)plane.base ||
        plane.row0_offset > plane.buffer_bytes - row) {
        return SIXEL_BAD_ARGUMENT;
    }
    /* Bound row travel before multiplication. This also handles the
     * signed minimum stride without negating it in its signed type. */
    pitch = plane.stride_bytes < 0
        ? (size_t)(-(plane.stride_bytes + 1)) + 1u
        : (size_t)plane.stride_bytes;
    available = plane.stride_bytes < 0 ? plane.row0_offset
        : plane.buffer_bytes - row - plane.row0_offset;
    if (pitch < row || (size_t)(view->height - 1) > available / pitch) {
        return SIXEL_BAD_ARGUMENT;
    }
    span = (size_t)(view->height - 1) * pitch + row;
    if (span > SIXEL_ALLOCATE_BYTES_MAX || span - row > PTRDIFF_MAX) {
        return SIXEL_BAD_ALLOCATION;
    }

    palette_bytes = frame->palette_extent > 0
        ? (size_t)frame->palette_extent * 3u : 0u;
    mask_bytes = frame->transparent_mask_size;
    alpha_bytes = frame->alpha_count * sizeof(float);
    /* Cropping may leave a retained tail on the owned coverage plane. */
    if (frame->transparent_mask != NULL &&
        mask_bytes < frame->coverage_extent) {
        mask_bytes = frame->coverage_extent;
    }
    if (frame->alpha != NULL && alpha_bytes < frame->coverage_extent) {
        alpha_bytes = frame->coverage_extent;
    }
    if (((frame->owns_pixels || frame->pixel_owner != NULL) &&
         sixel_frame_ranges_overlap(plane.base, plane.buffer_bytes,
            sixel_frame_pixel_storage(frame), frame->pixel_extent)) ||
        (frame->owns_palette &&
         sixel_frame_ranges_overlap(plane.base, plane.buffer_bytes,
            frame->palette, palette_bytes)) ||
        sixel_frame_ranges_overlap(plane.base, plane.buffer_bytes,
                                   frame->transparent_mask, mask_bytes) ||
        sixel_frame_ranges_overlap(plane.base, plane.buffer_bytes,
                                   frame->alpha, alpha_bytes)) {
        return SIXEL_BAD_ARGUMENT;
    }

    definition = (sixel_ycbcr_def_t *)view->definition;
    pixels = (unsigned char *)plane.base + plane.row0_offset;
    (void)sixel_atomic_fetch_add_u32(&definition->ref, 1U);
    sixel_frame_install_pixels(frame, pixels, view->width, view->height,
        view->pixelformat, NULL, -1, 0, 0, 0, plane.buffer_bytes);
    frame->format.coordinates = SIXEL_FRAME_COLORSPACE_YCBCR;
    frame->ycbcr = definition;
    frame->ycbcr_plane = plane;
    frame->packed_width = view->width;
    frame->packed_height = view->height;
    frame->transparent = -1;
    return SIXEL_OK;
}

SIXELAPI SIXELSTATUS
sixel_frame_get_ycbcr(sixel_frame_t const *frame,
                     sixel_ycbcr_frame_view_t *view)
{
    if (frame == NULL || frame->ycbcr == NULL || view == NULL ||
            view->struct_size < sizeof(*view)) {
        return SIXEL_BAD_ARGUMENT;
    }
    view->pixelformat = frame->format.layout;
    view->width = frame->width;
    view->height = frame->height;
    view->plane_count = 1;
    memset(view->planes, 0, sizeof(view->planes));
    view->planes[0] = frame->ycbcr_plane;
    view->definition = frame->ycbcr;
    return SIXEL_OK;
}

SIXEL_INTERNAL_API int
sixel_frame_ycbcr422_matches(sixel_frame_t const *frame,
                            sixel_frame_t const *source)
{
    sixel_ycbcr_def_t const *a = frame->ycbcr;
    sixel_ycbcr_def_t const *b = source->ycbcr;

    return a != NULL && b != NULL &&
        frame->format.layout == source->format.layout &&
        (a == b || (a->encoding == b->encoding && a->range == b->range &&
                    a->chroma_x_half == b->chroma_x_half &&
                    a->chroma_y_half == b->chroma_y_half &&
                    a->nearest_chroma == b->nearest_chroma &&
                    sixel_cdef_same_rgb(a->rgb, b->rgb)));
}

/* get pixels */
SIXELAPI unsigned char *
sixel_frame_get_pixels(sixel_frame_t /* in */ *frame)  /* frame object */
{
    return frame->pixels.u8ptr;
}


/* get palette */
SIXELAPI unsigned char *
sixel_frame_get_palette(sixel_frame_t /* in */ *frame)  /* frame object */
{
    return frame->palette;
}


/* set palette */
SIXELAPI void
sixel_frame_set_palette(
    sixel_frame_t  /* in */ *frame,
    unsigned char  /* in */ *palette)
{
    if (frame->palette != palette) {
        /* A caller may declare the first palette's size before attaching
         * its pointer. A replacement can be smaller than its predecessor. */
        frame->palette_extent = frame->palette == NULL && palette != NULL &&
            frame->ncolors > 0 && frame->ncolors <= SIXEL_PALETTE_MAX ?
            frame->ncolors : 0;
    }
    frame->palette = palette;
}


/* get width */
SIXELAPI int
sixel_frame_get_width(sixel_frame_t /* in */ *frame)  /* frame object */
{
    return frame->width;
}


/* set width */
SIXELAPI void
sixel_frame_set_width(sixel_frame_t /* in */ *frame, int /* in */ width)
{
    if (frame->ycbcr == NULL || (width > 0 && (width & 1) == 0 &&
            width <= frame->packed_width)) {
        frame->width = width;
    }
}


/* get height */
SIXELAPI int
sixel_frame_get_height(sixel_frame_t /* in */ *frame)  /* frame object */
{
    return frame->height;
}


/* set height */
SIXELAPI void
sixel_frame_set_height(sixel_frame_t /* in */ *frame, int /* in */ height)
{
    if (frame->ycbcr == NULL || (height > 0 &&
            height <= frame->packed_height)) {
        frame->height = height;
    }
}


/* get ncolors */
SIXELAPI int
sixel_frame_get_ncolors(sixel_frame_t /* in */ *frame)  /* frame object */
{
    return frame->ncolors;
}


/* set ncolors */
SIXELAPI void
sixel_frame_set_ncolors(
    sixel_frame_t  /* in */ *frame,
    int            /* in */ ncolors)
{
    /* A count change does not replace the palette allocation. Remember
     * its declared capacity until the pointer itself is replaced. */
    if (frame->palette != NULL && ncolors <= SIXEL_PALETTE_MAX &&
        ncolors > frame->palette_extent) {
        frame->palette_extent = ncolors;
    }
    frame->ncolors = ncolors;
}


/* get pixelformat */
SIXELAPI int
sixel_frame_get_pixelformat(sixel_frame_t /* in */ *frame)  /* frame object */
{
    return frame->format.layout;
}


/* Publish a byte view only after every original float triplet has converted.
 * The storage normalizer handles channel layouts, not color interpretation:
 * using it first would quantize linear or opponent coordinates before their
 * transfer/matrix conversion. A separate byte buffer also makes allocation
 * and numeric failures atomic for both owned and borrowed source storage. */
static SIXELSTATUS
sixel_frame_render_fixed_rgb888(sixel_frame_t const *frame,
                                unsigned char **result)
{
    SIXELSTATUS status;
    unsigned char *pixels;
    double input[3];
    double output[3];
    double value;
    size_t count;
    size_t bytes;
    size_t i;
    int c;
    int converted;

    if (frame->width <= 0 || frame->height <= 0 ||
        (size_t)frame->width > SIZE_MAX / (size_t)frame->height) {
        return SIXEL_BAD_INPUT;
    }
    count = (size_t)frame->width * (size_t)frame->height;
    if (count > SIZE_MAX / (3u * sizeof(float))) {
        return SIXEL_BAD_INPUT;
    }
    status = sixel_frame_validate_size("sixel_frame_set_pixelformat",
        frame->width, frame->height, SIXEL_PIXELFORMAT_RGB888,
        &count, &bytes, NULL);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    pixels = (unsigned char *)sixel_allocator_malloc(frame->allocator, bytes);
    if (pixels == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    for (i = 0u; i < count; ++i) {
        for (c = 0; c < 3; ++c) {
            input[c] = frame->pixels.f32ptr[i * 3u + c];
        }
        if (frame->format.coordinates == SIXEL_COLORSPACE_SRGB) {
            /* Gamma storage already has the requested coordinates. Keep
             * exact rounding ties and avoid a redundant transfer roundtrip. */
            memcpy(output, input, sizeof(output));
            converted = isfinite(input[0]) && isfinite(input[1]) &&
                        isfinite(input[2]);
        } else if (frame->format.coordinates == SIXEL_COLORSPACE_SMPTEC) {
            converted = sixel_color_from_output(output, input,
                SIXEL_COLORSPACE_SMPTEC, SIXEL_COLORSPACE_SRGB);
        } else {
            converted = sixel_color_convert(output, input,
                frame->format.coordinates, SIXEL_COLORSPACE_SRGB);
        }
        if (!converted) {
            sixel_allocator_free(frame->allocator, pixels);
            sixel_helper_set_additional_message(
                "frame byte conversion: nonfinite or domain error.");
            return SIXEL_BAD_INPUT;
        }
        for (c = 0; c < 3; ++c) {
            /* Only this final gamma byte boundary restricts the gamut.
             * Widening source floats preserves their exact stored value. */
            value = output[c] < 0.0 ? 0.0 :
                    output[c] > 1.0 ? 1.0 : output[c];
            pixels[i * 3u + c] = (unsigned char)(value * 255.0 + 0.5);
        }
    }
    *result = pixels;
    return SIXEL_OK;
}


static int
sixel_frame_apply_shared_matrix(sixel_color_def_t const *source,
                               double const m[9], int const anchor[3],
                               double const input[3],
                               double out[3], int space, unsigned int flags)
{
    double linear[3], rgb[3];
    int c, a, b, ref, unchanged;

    if (!sixel_cdef_transfer(source, input, linear, 0)) {
        return 0;
    }
    for (c = 0; c < 3; ++c) {
        ref = anchor[c];
        a = (ref + 1) % 3;
        b = (ref + 2) % 3;
        /* Relative whites give a unit row sum. When every contributing
         * source has the same value, preserve it exactly. A permutation
         * row has just one contributor, possibly at a different index. */
        unchanged = (m[3 * c + a] == 0.0 || linear[a] == linear[ref]) &&
                    (m[3 * c + b] == 0.0 || linear[b] == linear[ref]);
        rgb[c] = unchanged ? linear[ref] :
            m[3 * c] * linear[0] + m[3 * c + 1] * linear[1] +
            m[3 * c + 2] * linear[2];
        if (space == SIXEL_COLORSPACE_SRGB) {
            out[c] = unchanged && (flags & SIXEL_CDEF_SRGB_NEUTRAL) ?
                input[ref] : sixel_color_encode_srgb(rgb[c]);
        }
    }
    if (space == SIXEL_COLORSPACE_SRGB) {
        return isfinite(out[0]) && isfinite(out[1]) && isfinite(out[2]);
    }
    return sixel_color_convert(out, rgb, SIXEL_COLORSPACE_SRGB_LINEAR, space);
}

/* Render float coordinates into independent storage. Byte resize also
 * needs this buffer to keep the original frame intact until scaling ends. */
static SIXELSTATUS
sixel_frame_render_float(sixel_frame_t const *frame, int target,
                         int space, unsigned char **result)
{
    sixel_color_def_t *srgb;
    SIXELSTATUS status;
    unsigned char *pixels;
    float *floats;
    size_t count;
    size_t bytes;
    size_t i;
    int c;
    unsigned int invariants;
    float const *sample;
    double shared_matrix[9];
    int anchor[3];
    int shared;
    double rgb[3];
    double output[3];
    double v;

    if (frame->format.rgb == NULL) {
        return target == SIXEL_PIXELFORMAT_RGB888 ?
            sixel_frame_render_fixed_rgb888(frame, result) : SIXEL_BAD_ARGUMENT;
    }
    if (target != SIXEL_PIXELFORMAT_RGB888 &&
        !SIXEL_WORKFORMAT_IS_FLOAT32(target)) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (frame->width <= 0 || frame->height <= 0 ||
        (size_t)frame->width > SIZE_MAX / (size_t)frame->height) {
        return SIXEL_BAD_INPUT;
    }
    status = sixel_frame_validate_size("profiled frame", frame->width,
        frame->height, target, &count, &bytes, NULL);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    /* Exact sRGB coordinates must reach the byte-rounding boundary intact. */
    srgb = NULL;
    if (!sixel_cdef_same_rgb(frame->format.rgb, NULL)) {
        srgb = sixel_cdef_name("srgb");
        if (srgb == NULL) {
            return SIXEL_BAD_ALLOCATION;
        }
    }
    invariants = sixel_cdef_srgb_matrix(frame->format.rgb, srgb,
                                       shared_matrix);
    shared = srgb != NULL && (invariants & SIXEL_CDEF_SRGB_MATRIX);
    if (shared) {
        for (c = 0; c < 3; ++c) {
            anchor[c] = shared_matrix[3 * c] != 0.0 ? 0 :
                        shared_matrix[3 * c + 1] != 0.0 ? 1 : 2;
        }
    }
    pixels = sixel_allocator_malloc(frame->allocator, bytes);
    if (pixels == NULL) {
        sixel_cdef_free(srgb);
        return SIXEL_BAD_ALLOCATION;
    }
    floats = (float *)(void *)pixels;
    for (i = 0; i < count; ++i) {
        sample = frame->pixels.f32ptr + 3 * i;
        for (c = 0; c < 3; ++c) {
            rgb[c] = sample[c];
        }
        /* Prefer relative PCS or its matrix/TRC path. A frame has no
         * exclusive rendering intent; a forward-only A2B0 profile can
         * supply PCS when there is no usable relative path. */
        if (shared) {
            if (!sixel_frame_apply_shared_matrix(frame->format.rgb,
                    shared_matrix, anchor, rgb, output, space, invariants)) {
                goto invalid;
            }
        } else if (srgb != NULL &&
                   !((invariants & SIXEL_CDEF_SRGB_NEUTRAL) &&
                     rgb[0] == rgb[1] && rgb[1] == rgb[2])) {
            if (!sixel_cdef_frame_sample(frame->format.rgb, srgb, rgb,
                    space, target != SIXEL_PIXELFORMAT_RGB888, output)) {
                goto invalid;
            }
        } else if (space == SIXEL_COLORSPACE_SRGB) {
            memcpy(output, rgb, sizeof(output));
        } else if (!sixel_color_convert(output, rgb,
                       SIXEL_COLORSPACE_SRGB, space)) {
            goto invalid;
        }
        for (c = 0; c < 3; ++c) {
            v = output[c];
            if (!isfinite(v)) {
                goto invalid;
            }
            if (target == SIXEL_PIXELFORMAT_RGB888) {
                v = v < 0.0 ? 0.0 : v > 1.0 ? 1.0 : v;
                pixels[3 * i + c] = (unsigned char)(255.0 * v + 0.5);
            } else {
                if (fabs(v) > FLT_MAX) {
                    goto invalid;
                }
                floats[3 * i + c] = (float)v;
            }
        }
    }
    *result = pixels;
    sixel_cdef_free(srgb);
    return SIXEL_OK;

invalid:
    sixel_allocator_free(frame->allocator, pixels);
    sixel_cdef_free(srgb);
    return SIXEL_BAD_INPUT;
}


/* Exchange red and blue in owned byte triplets without another buffer. */
static void
sixel_frame_swap_rb(unsigned char *pixels, size_t count)
{
    unsigned char blue;

    for (; count != 0; --count, pixels += 3) {
        blue = pixels[2];
        pixels[2] = pixels[0];
        pixels[0] = blue;
    }
}

static SIXELSTATUS
sixel_frame_convert_float(sixel_frame_t *frame, int target, int space)
{
    unsigned char *pixels;
    SIXELSTATUS status;

    status = sixel_frame_render_float(frame,
        target == SIXEL_PIXELFORMAT_BGR888 ? SIXEL_PIXELFORMAT_RGB888 : target,
        space, &pixels);
    if (SIXEL_SUCCEEDED(status)) {
        sixel_frame_release_owned_pixels(frame);
        if (target == SIXEL_PIXELFORMAT_BGR888) {
            sixel_frame_swap_rb(pixels,
                (size_t)frame->width * (size_t)frame->height);
        }
        if (!SIXEL_WORKFORMAT_IS_FLOAT32(target)) {
            frame->pixels.u8ptr = pixels;
        } else {
            frame->pixels.f32ptr = (float *)(void *)pixels;
        }
        frame->pixel_extent = (size_t)frame->width * frame->height * 3u *
            (SIXEL_WORKFORMAT_IS_FLOAT32(target) ? sizeof(float) : 1u);
        frame->owns_pixels = 1;
        sixel_frame_assign_layout(frame, target, space);
    }
    return status;
}


/* Repack byte or packed RGB without changing the color coordinates.
 * Owned storage can be compacted in place, including for video frames. */
static SIXELSTATUS
sixel_frame_pack_rgb(sixel_frame_t *frame, int target, int dst_shift)
{
    unsigned char *pixels;
    size_t bytes;
    int source, src_shift;
    SIXELSTATUS status;

    source = frame->format.layout;
    src_shift = sixel_pixelformat_packed_rgb_shift(source);
    if (src_shift == 0 && source != SIXEL_PIXELFORMAT_RGB888 &&
                         source != SIXEL_PIXELFORMAT_BGR888) {
        return SIXEL_BAD_ARGUMENT;
    }
    status = sixel_pixelformat_buffer_bytes(frame->width, frame->height,
                                           target, &bytes);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    pixels = frame->pixels.u8ptr;
    if (!frame->owns_pixels) {
        pixels = sixel_allocator_malloc(frame->allocator, bytes);
        if (pixels == NULL) {
            return SIXEL_BAD_ALLOCATION;
        }
    }
    /* Keep the contiguous exchange loop for the same-width fast path. */
    if (src_shift == dst_shift) {
        sixel_pixelformat_swap_packed_rb(pixels, frame->pixels.u8ptr,
                                        bytes, dst_shift);
    } else {
        sixel_pixelformat_pack_rgb(pixels, frame->pixels.u8ptr, bytes / 2u,
                                   source, target, src_shift, dst_shift);
    }
    if (!frame->owns_pixels) {
        sixel_frame_release_owned_pixels(frame);
        frame->pixels.u8ptr = pixels;
        frame->pixel_extent = bytes;
        frame->owns_pixels = 1;
    }
    frame->format.layout = target;
    return SIXEL_OK;
}

/* Unpack each padded row before a byte-addressed operation. Palette
 * samples remain indices and gray samples expand across the full G8 range. */
static SIXELSTATUS
sixel_frame_unpack_rows(sixel_frame_t *frame, int target)
{
    unsigned char *pixels;
    SIXELSTATUS status;

    pixels = sixel_allocator_malloc(frame->allocator,
        (size_t)frame->width * (size_t)frame->height);
    if (pixels == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    if (target == SIXEL_PIXELFORMAT_G8) {
        sixel_pixelformat_expand_gray8(pixels, frame->pixels.u8ptr,
            frame->width, frame->height, frame->format.layout);
    } else {
        status = sixel_helper_normalize_pixelformat(pixels, &target,
            frame->pixels.u8ptr, frame->format.layout,
            frame->width, frame->height);
        if (SIXEL_FAILED(status)) {
            sixel_allocator_free(frame->allocator, pixels);
            return status;
        }
    }
    sixel_frame_release_owned_pixels(frame);
    frame->pixels.u8ptr = pixels;
    frame->pixel_extent = (size_t)frame->width * frame->height;
    frame->owns_pixels = 1;
    frame->format.layout = target;
    return SIXEL_OK;
}

static int
sixel_frame_unpacked_format(int format)
{
    switch (format) {
    case SIXEL_PIXELFORMAT_PAL1:
    case SIXEL_PIXELFORMAT_PAL2:
    case SIXEL_PIXELFORMAT_PAL4: return SIXEL_PIXELFORMAT_PAL8;
    case SIXEL_PIXELFORMAT_G1:
    case SIXEL_PIXELFORMAT_G2:
    case SIXEL_PIXELFORMAT_G4: return SIXEL_PIXELFORMAT_G8;
    default: return format;
    }
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_frame_convert_pixels(sixel_frame_t *frame, int layout,
                           int target_colorspace)
{
    SIXELSTATUS status;
    int source_colorspace;
    int working_pixelformat;
    int depth, packed_shift;
    size_t pixel_total;
    size_t pixel_size;
    unsigned char *pixels;

    if (frame == NULL) {
        sixel_helper_set_additional_message(
            "sixel_frame_set_pixelformat: frame is null.");
        return SIXEL_BAD_ARGUMENT;
    }
    if (frame->ycbcr != NULL || sixel_pixelformat_is_ycbcr422(layout)) {
        return SIXEL_NOT_IMPLEMENTED;
    }
    /* A delivery boundary must resolve alpha before reducing RGB storage. */
    if (frame->alpha != NULL &&
        (layout != SIXEL_PIXELFORMAT_RGBFLOAT32 ||
         (target_colorspace != SIXEL_COLORSPACE_SRGB &&
          target_colorspace != SIXEL_COLORSPACE_SRGB_LINEAR))) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (frame->format.rgb != NULL && frame->pixels.f32ptr != NULL) {
        return sixel_frame_convert_float(frame, layout, target_colorspace);
    }
    /* SMPTE-C shares an RGB storage code with gamma sRGB. A request for
     * sRGB must resolve the coordinates even when the layout matches. */
    if (layout == frame->format.layout &&
        (target_colorspace == frame->format.coordinates ||
         (layout != SIXEL_PIXELFORMAT_RGBFLOAT32 &&
          layout != SIXEL_PIXELFORMAT_RGB888 &&
          layout != SIXEL_PIXELFORMAT_BGR888))) {
        return SIXEL_OK;
    }
    if (frame->pixels.u8ptr == NULL) {
        sixel_frame_assign_layout(frame, layout, target_colorspace);
        return SIXEL_OK;
    }
    /* Packed 555/565 codes are adjacent to RGB888 in the legacy ABI. */
    if (layout >= SIXEL_PIXELFORMAT_RGB555 &&
        layout <= SIXEL_PIXELFORMAT_BGR565 &&
        layout != SIXEL_PIXELFORMAT_RGB888) {
        packed_shift = sixel_pixelformat_packed_rgb_shift(layout);
        return sixel_frame_pack_rgb(frame, layout, packed_shift);
    }
    if (layout != SIXEL_PIXELFORMAT_RGBFLOAT32 &&
        frame->format.layout == SIXEL_PIXELFORMAT_RGBFLOAT32) {
        return sixel_frame_convert_float(frame, layout, target_colorspace);
    }

    status = SIXEL_OK;
    working_pixelformat = sixel_frame_format(frame);
    if (layout == SIXEL_PIXELFORMAT_RGBFLOAT32 &&
        frame->format.layout != SIXEL_PIXELFORMAT_RGBFLOAT32) {
        return sixel_frame_promote_to_float32(frame, target_colorspace);
    }
    if (layout == SIXEL_PIXELFORMAT_BGR888) {
        /* Normalize the source layout and transfer first. Own the triplets
         * before exchanging channels so a borrowed RGB source stays intact.
         * Other layouts already acquire owned storage during normalization. */
        if (!frame->owns_pixels &&
            working_pixelformat == SIXEL_PIXELFORMAT_RGB888) {
            status = sixel_frame_validate_size("BGR888", frame->width,
                frame->height, layout, &pixel_total, &pixel_size, NULL);
            if (SIXEL_FAILED(status)) {
                return status;
            }
            pixels = sixel_allocator_malloc(frame->allocator, pixel_size);
            if (pixels == NULL) {
                return SIXEL_BAD_ALLOCATION;
            }
            memcpy(pixels, frame->pixels.u8ptr, pixel_size);
            status = sixel_helper_convert_colorspace(pixels, pixel_size,
                working_pixelformat, frame->format.coordinates,
                SIXEL_COLORSPACE_SRGB);
            if (SIXEL_FAILED(status)) {
                sixel_allocator_free(frame->allocator, pixels);
                return status;
            }
            frame->format.coordinates = SIXEL_COLORSPACE_SRGB;
            frame->pixels.u8ptr = pixels;
            frame->pixel_extent = pixel_size;
            frame->owns_pixels = 1;
        }
        status = sixel_frame_set_pixelformat(frame, SIXEL_PIXELFORMAT_RGB888);
        if (SIXEL_SUCCEEDED(status)) {
            sixel_frame_swap_rb(frame->pixels.u8ptr,
                                (size_t)frame->width * frame->height);
            sixel_frame_assign_layout(frame, layout, target_colorspace);
        }
        return status;
    }
    if (working_pixelformat != layout &&
        sixel_frame_unpacked_format(working_pixelformat) == layout) {
        status = sixel_frame_unpack_rows(frame, layout);
        /* PAL8 is a layout request for indices. The palette still
         * describes the same colors in the original coordinates. */
        if (layout == SIXEL_PIXELFORMAT_PAL8) {
            return status;
        }
    } else if (layout == SIXEL_PIXELFORMAT_RGB888 &&
        working_pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        status = sixel_frame_convert_to_rgb888(frame);
    } else if (layout != SIXEL_PIXELFORMAT_RGBFLOAT32
            && !SIXEL_WORKFORMAT_IS_FLOAT32(working_pixelformat)
            && (working_pixelformat & SIXEL_FORMATTYPE_PALETTE)) {
        status = sixel_frame_convert_to_rgb888(frame);
    }

    if (SIXEL_FAILED(status)) {
        return status;
    }

    working_pixelformat = sixel_frame_format(frame);
    source_colorspace = frame->format.coordinates;

    if (target_colorspace != source_colorspace) {
        /*
         * Convert in-place so callers can request alternate transfer
         * curves or OKLab buffers without mutating the frame twice.
         */
        if (frame->width <= 0 || frame->height <= 0) {
            sixel_helper_set_additional_message(
                "sixel_frame_set_pixelformat: invalid frame size.");
            return SIXEL_BAD_INPUT;
        }

        pixel_total = (size_t)frame->width * (size_t)frame->height;
        if (pixel_total / (size_t)frame->width != (size_t)frame->height) {
            sixel_helper_set_additional_message(
                "sixel_frame_set_pixelformat: buffer overflow risk.");
            return SIXEL_BAD_INPUT;
        }

        depth = sixel_helper_compute_depth(working_pixelformat);
        if (depth <= 0) {
            sixel_helper_set_additional_message(
                "sixel_frame_set_pixelformat: invalid layout depth.");
            return SIXEL_BAD_INPUT;
        }
        if (pixel_total > SIZE_MAX / (size_t)depth) {
            sixel_helper_set_additional_message(
                "sixel_frame_set_pixelformat: buffer size overflow.");
            return SIXEL_BAD_INPUT;
        }
        pixel_size = pixel_total * (size_t)depth;

        pixels = frame->pixels.u8ptr;
        if (SIXEL_WORKFORMAT_IS_FLOAT32(working_pixelformat)) {
            /*
             * Float frames carry intermediate coordinates, including
             * colors outside sRGB. Share the public float transaction and
             * its scoped worker budget without clipping to a byte cube.
             * Failed conversion preserves all pixels and the old tags.
             */
            status = sixel_color_convert_frame_rgbf32(frame->pixels.f32ptr,
                                                pixel_total,
                                                source_colorspace,
                                                target_colorspace);
        } else {
            status = sixel_helper_convert_colorspace(pixels,
                                                     pixel_size,
                                                     working_pixelformat,
                                                     source_colorspace,
                                                     target_colorspace);
        }
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }

    sixel_frame_assign_layout(frame, layout, target_colorspace);
    return SIXEL_OK;
}


SIXEL_INTERNAL_API SIXELSTATUS
sixel_frame_convert_workformat(sixel_frame_t *frame, int pixelformat)
{
    return sixel_frame_convert_pixels(frame,
        SIXEL_WORKFORMAT_IS_FLOAT32(pixelformat)
            ? SIXEL_PIXELFORMAT_RGBFLOAT32 : pixelformat,
        sixel_pixelformat_get_colorspace(pixelformat));
}


/* Reconstruct one RGB triplet without applying the source RGB transfer. */
static void
sixel_frame_decode_ycbcr_sample(float *out, float y, float cb, float cr,
                              float const coefficients[4])
{
    out[0] = y + coefficients[0] * cr;
    out[1] = y - coefficients[1] * cb - coefficients[2] * cr;
    out[2] = y + coefficients[3] * cb;
}

/* Read packed rows directly into the RGB allocation. The source and
 * destination may be the same frame for explicit layout conversion. */
static SIXELSTATUS
sixel_frame_decode_ycbcr422_into(sixel_frame_t const *frame,
                                sixel_frame_t *destination, int step,
                                sixel_frame_t const *grid, int grid_step)
{
    /* Luma weights from the 601, 709 and 2020 NCL encodings. */
    static double const weights[][2] = {
        {0.2990, 0.1140}, {0.2126, 0.0722}, {0.2627, 0.0593}
    };
    sixel_ycbcr_def_t const *definition;
    sixel_color_def_t *rgb;
    unsigned char const *row, *pair, *previous, *next;
    float *pixels, *out;
    float coefficients[4];
    double kr, kb, kg, chroma_divisor;
    float luma_scale, luma_offset, left_weight, right_weight;
    float cb, cr, odd_cb, odd_cr;
    size_t bytes;
    int y, x, uyvy, cb_offset, cr_offset, limited, centered;
    int pair_x, odd, grid_x, grid_y;
    size_t converted;
    float const *grid_row;
    SIXELSTATUS status;

    status = sixel_frame_validate_size("packed YCbCr conversion",
        destination->width, destination->height, SIXEL_PIXELFORMAT_RGBFLOAT32,
        NULL, &bytes, NULL);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    pixels = sixel_allocator_malloc(destination->allocator, bytes);
    if (pixels == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    definition = frame->ycbcr;
    rgb = sixel_cdef_clone(definition->rgb);
    if (rgb == NULL) {
        sixel_allocator_free(destination->allocator, pixels);
        return SIXEL_BAD_ALLOCATION;
    }
    kr = weights[definition->encoding - 1][0];
    kb = weights[definition->encoding - 1][1];
    kg = 1.0 - kr - kb;
    limited = definition->range == SIXEL_YCBCR_RANGE_LIMITED;
    luma_scale = limited ? 1.0f / 219.0f : 1.0f / 255.0f;
    luma_offset = limited ? 16.0f : 0.0f;
    chroma_divisor = limited ? 224.0 : 255.0;
    coefficients[0] = (float)(2.0 * (1.0 - kr) / chroma_divisor);
    coefficients[1] = (float)(2.0 * kb * (1.0 - kb) / (kg * chroma_divisor));
    coefficients[2] = (float)(2.0 * kr * (1.0 - kr) / (kg * chroma_divisor));
    coefficients[3] = (float)(2.0 * (1.0 - kb) / chroma_divisor);
    uyvy = frame->format.layout == SIXEL_PIXELFORMAT_UYVY;
    cb_offset = 1 - uyvy;
    cr_offset = 3 - uyvy;
    centered = definition->chroma_x_half == 1;
    left_weight = centered ? 0.25f : 0.0f;
    right_weight = centered ? 0.25f : 0.5f;

    if (frame->ycbcr_counts != NULL) {
        converted = bytes / (3u * sizeof(float));
        if (grid != NULL) {
            converted -= (size_t)grid->width * grid->height;
        }
        (void)sixel_atomic_fetch_add_u32(
            &frame->ycbcr_counts->rgb_conversions, 1U);
        (void)sixel_atomic_fetch_add_u32(&frame->ycbcr_counts->rgb_pixels,
            (unsigned int)converted);
    }
    out = pixels;
    if (step > 1 || grid != NULL) {
        /* Select in the source raster before reconstructing chroma. The
         * adjacent sample is still the original adjacent pair, not the
         * next grid point. Only the selected luma is converted. */
        grid_y = 0;
        for (y = 0; y < frame->height; y += step) {
            row = frame->pixels.u8ptr + y * frame->ycbcr_plane.stride_bytes;
            grid_row = NULL;
            if (grid != NULL && y == grid_y) {
                grid_row = grid->pixels.f32ptr +
                    (size_t)(y / grid_step) * grid->width * 3u;
                grid_y += grid_step;
            }
            grid_x = 0;
            for (x = 0; x < frame->width; x += step, out += 3) {
                if (grid_row != NULL && x == grid_x) {
                    memcpy(out, grid_row, 3u * sizeof(float));
                    grid_row += 3;
                    grid_x += grid_step;
                    continue;
                }
                odd = x & 1;
                pair_x = x - odd;
                pair = row + (size_t)pair_x * 2u;
                next = pair;
                if (!definition->nearest_chroma) {
                    if (odd && pair_x + 2 < frame->width) {
                        next = pair + 4;
                    } else if (!odd && centered && pair_x > 0) {
                        next = pair - 4;
                    }
                }
                cb = pair[cb_offset] - 128.0f;
                cr = pair[cr_offset] - 128.0f;
                cb += (odd ? right_weight : left_weight) *
                    (next[cb_offset] - pair[cb_offset]);
                cr += (odd ? right_weight : left_weight) *
                    (next[cr_offset] - pair[cr_offset]);
                sixel_frame_decode_ycbcr_sample(out,
                    (pair[uyvy + odd * 2] - luma_offset) * luma_scale,
                    cb, cr, coefficients);
            }
        }
    } else {
        for (y = 0; y < frame->height; ++y) {
            row = frame->pixels.u8ptr + y * frame->ycbcr_plane.stride_bytes;
            pair = row;
            for (x = 0; x < frame->width; x += 2, pair += 4, out += 6) {
                /* Even/odd luma samples fall on different sides of a centered
                 * chroma sample. Extend the edge sample within each row. */
                cb = pair[cb_offset] - 128.0f;
                cr = pair[cr_offset] - 128.0f;
                odd_cb = cb;
                odd_cr = cr;
                if (!definition->nearest_chroma) {
                    previous = centered && x > 0 ? pair - 4 : pair;
                    next = x + 2 < frame->width ? pair + 4 : pair;
                    cb += left_weight *
                        (previous[cb_offset] - pair[cb_offset]);
                    cr += left_weight *
                        (previous[cr_offset] - pair[cr_offset]);
                    odd_cb += right_weight *
                        (next[cb_offset] - pair[cb_offset]);
                    odd_cr += right_weight *
                        (next[cr_offset] - pair[cr_offset]);
                }
                /* For left-sited chroma, a nearest-neighbor tie uses the
                 * preceding sample, so both lumas share this pair's chroma. */
                sixel_frame_decode_ycbcr_sample(out,
                    (pair[uyvy] - luma_offset) * luma_scale,
                    cb, cr, coefficients);
                sixel_frame_decode_ycbcr_sample(out + 3,
                    (pair[uyvy + 2] - luma_offset) * luma_scale,
                    odd_cb, odd_cr, coefficients);
            }
        }
    }

    sixel_frame_release_owned_pixels(destination);
    sixel_frame_assign_layout(destination, SIXEL_PIXELFORMAT_RGBFLOAT32,
                              SIXEL_FRAME_COLORSPACE_PROFILE_RGB);
    destination->pixels.f32ptr = pixels;
    destination->pixel_extent = bytes;
    destination->format.rgb = rgb;
    return SIXEL_OK;
}

SIXELAPI SIXELSTATUS
sixel_frame_set_pixelformat(sixel_frame_t *frame, int pixelformat)
{
    if (frame != NULL && pixelformat == SIXEL_PIXELFORMAT_RGBFLOAT32) {
        if (frame->ycbcr != NULL) {
            return sixel_frame_decode_ycbcr422_into(frame, frame, 1, NULL, 0);
        }
        if (frame->format.layout == pixelformat) {
            return SIXEL_OK;
        }
    }
    return sixel_frame_convert_workformat(frame, pixelformat);
}


SIXELAPI int
sixel_frame_get_colorspace(sixel_frame_t /* in */ *frame)  /* frame object */
{
    return frame->format.coordinates;
}


/* get transparent */
SIXELAPI int
sixel_frame_get_transparent(sixel_frame_t /* in */ *frame)  /* frame object */
{
    return frame->transparent;
}


/* set transparent */
SIXELAPI void
sixel_frame_set_transparent(
    sixel_frame_t  /* in */ *frame,
    int            /* in */ transparent)
{
    frame->transparent = transparent;
}


/* get transparent */
SIXELAPI int
sixel_frame_get_multiframe(sixel_frame_t /* in */ *frame)  /* frame object */
{
    return frame->multiframe;
}


/* set multiframe */
SIXELAPI void
sixel_frame_set_multiframe(
    sixel_frame_t  /* in */ *frame,
    int            /* in */ multiframe)
{
    frame->multiframe = multiframe;
}


/* get delay */
SIXELAPI int
sixel_frame_get_delay(sixel_frame_t /* in */ *frame)  /* frame object */
{
    return frame->delay;
}


/* set delay */
SIXELAPI void
sixel_frame_set_delay(sixel_frame_t /* in */ *frame, int /* in */ delay)
{
    frame->delay = delay;
}


/* get frame no */
SIXELAPI int
sixel_frame_get_frame_no(sixel_frame_t /* in */ *frame)  /* frame object */
{
    return frame->frame_no;
}


/* set frame index */
SIXELAPI void
sixel_frame_set_frame_no(
    sixel_frame_t  /* in */ *frame,
    int            /* in */ frame_no)
{
    frame->frame_no = frame_no;
}


/* increment frame index */
SIXELAPI void
sixel_frame_increment_frame_no(sixel_frame_t /* in */ *frame)
{
    ++frame->frame_no;
}


/* reset frame index */
SIXELAPI void
sixel_frame_reset_frame_no(sixel_frame_t /* in */ *frame)
{
    frame->frame_no = 0;
}


/* get loop no */
SIXELAPI int
sixel_frame_get_loop_no(sixel_frame_t /* in */ *frame)  /* frame object */
{
    return frame->loop_count;
}


/* set loop count */
SIXELAPI void
sixel_frame_set_loop_count(
    sixel_frame_t  /* in */ *frame,
    int            /* in */ loop_count)
{
    frame->loop_count = loop_count;
}


/* increment loop count */
SIXELAPI void
sixel_frame_increment_loop_count(sixel_frame_t /* in */ *frame)
{
    ++frame->loop_count;
}


/* strip alpha from RGBA/ARGB/BGRA/ABGR formatted pixbuf */
SIXELAPI SIXELSTATUS
sixel_frame_strip_alpha(
    sixel_frame_t  /* in */ *frame,
    unsigned char  /* in */ *bgcolor
)
{
    sixel_frame_alpha_layout_t const *layout;
    unsigned char *src, *dst;
    unsigned int alpha, inverse;
    unsigned char r, g, b;
    size_t count, i;

    loader_timeline_optional_mark("post/background");
    layout = sixel_frame_alpha_layout(frame->format.layout);
    if (layout == NULL || layout->stride != 4) {
        return SIXEL_OK;
    }

    src = dst = frame->pixels.u8ptr;
    count = (size_t)frame->width * (size_t)frame->height;
    for (i = 0; i < count; ++i) {
        /* Read all components first: the first RGB destination overlaps
         * its RGBA source. An opaque pixel keeps its exact channel values. */
        r = src[layout->rgb[0]];
        g = src[layout->rgb[1]];
        b = src[layout->rgb[2]];
        if (bgcolor != NULL) {
            alpha = src[layout->alpha];
            inverse = 255u - alpha;
            r = (unsigned char)((r * alpha + bgcolor[0] * inverse + 127u)
                                / 255u);
            g = (unsigned char)((g * alpha + bgcolor[1] * inverse + 127u)
                                / 255u);
            b = (unsigned char)((b * alpha + bgcolor[2] * inverse + 127u)
                                / 255u);
        }
        *dst++ = r;
        *dst++ = g;
        *dst++ = b;
        src += 4;
    }
    if (count != 0u) {
        sixel_frame_finish_rgba_compaction(frame);
    }
    return SIXEL_OK;
}


/* Merge keyed palette or packed alpha coverage before RGB replaces the
 * source samples. Run after all fallible color conversions to keep retries
 * atomic. Opaque images need no mask allocation. */
static SIXELSTATUS
sixel_frame_merge_key(sixel_frame_t *frame, unsigned char const *samples,
                      size_t count, size_t stride, int key)
{
    size_t i;
    unsigned char *mask;

    if (key < 0 || key > 255) {
        return SIXEL_OK;
    }
    mask = frame->transparent_mask;
    if ((mask == NULL) != (frame->transparent_mask_size == 0u) ||
        (mask != NULL && frame->transparent_mask_size < count)) {
        return SIXEL_BAD_INPUT;
    }
    for (i = 0; i < count; ++i) {
        if (samples[i * stride] == key) {
            break;
        }
    }
    if (i == count) {
        return SIXEL_OK;
    }
    if (mask == NULL) {
        mask = sixel_allocator_calloc(frame->allocator, count, 1u);
        if (mask == NULL) {
            return SIXEL_BAD_ALLOCATION;
        }
        frame->transparent_mask = mask;
        frame->transparent_mask_size = count;
    }
    for (; i < count; ++i) {
        if (samples[i * stride] == key) {
            mask[i] = 1;
        }
    }
    frame->alpha_zero_is_transparent = 1;
    return SIXEL_OK;
}

/* A packed alpha sample is lost when either byte or float RGB replaces
 * its storage. Materialize zero coverage only when the caller requested it. */
static SIXELSTATUS
sixel_frame_merge_packed_alpha(sixel_frame_t *frame, size_t count)
{
    sixel_frame_alpha_layout_t const *layout;

    if (!frame->alpha_zero_is_transparent) {
        return SIXEL_OK;
    }
    layout = sixel_frame_alpha_layout(frame->format.layout);
    if (layout == NULL) {
        return SIXEL_OK;
    }
    return sixel_frame_merge_key(frame, frame->pixels.u8ptr + layout->alpha,
                                 count, (size_t)layout->stride, 0);
}

static SIXELSTATUS
sixel_frame_convert_to_rgb888(sixel_frame_t /*in */ *frame)
{
    SIXELSTATUS status = SIXEL_FALSE;
    unsigned char *normalized_pixels = NULL;
    size_t size;
    size_t pixel_count;
    unsigned char *dst;
    unsigned char const *color;
    size_t i;
    int format;
    int entries;
    unsigned char *src;
    unsigned char *raw_pixels;
    unsigned char const *source_pixels;
    float *float_pixels;

    if (frame->format.rgb != NULL) {
        return sixel_frame_convert_float(frame, SIXEL_PIXELFORMAT_RGB888,
                                         SIXEL_COLORSPACE_SRGB);
    }

    if (frame->ycbcr != NULL) {
        return SIXEL_NOT_IMPLEMENTED;
    }
    sixel_frame_ref(frame);

    raw_pixels = frame->pixels.u8ptr;
    float_pixels = frame->pixels.f32ptr;
    source_pixels = raw_pixels;
    pixel_count = (size_t)frame->width * (size_t)frame->height;

    switch (sixel_frame_format(frame)) {
    case SIXEL_PIXELFORMAT_PAL1:
    case SIXEL_PIXELFORMAT_PAL2:
    case SIXEL_PIXELFORMAT_PAL4:
    case SIXEL_PIXELFORMAT_PAL8:
        if (frame->palette == NULL) {
            status = SIXEL_BAD_INPUT;
            goto end;
        }
        entries = sixel_frame_palette_entries(frame);
        size = pixel_count * (frame->format.layout == SIXEL_PIXELFORMAT_PAL8
                              ? 3u : 4u);
        normalized_pixels = sixel_allocator_malloc(frame->allocator, size);
        if (normalized_pixels == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        src = raw_pixels;
        if (frame->format.layout != SIXEL_PIXELFORMAT_PAL8) {
            src = normalized_pixels + pixel_count * 3u;
            status = sixel_helper_normalize_pixelformat(src, &format,
                raw_pixels, frame->format.layout, frame->width, frame->height);
            if (SIXEL_FAILED(status)) {
                sixel_allocator_free(frame->allocator, normalized_pixels);
                goto end;
            }
        }
        dst = normalized_pixels;
        for (i = 0; i < pixel_count; ++i) {
            color = sixel_frame_indexed_color(frame, entries, src[i]);
            if (color == NULL) {
                sixel_allocator_free(frame->allocator, normalized_pixels);
                status = SIXEL_BAD_INPUT;
                goto end;
            }
            memcpy(dst, color, 3u);
            dst += 3;
        }
        status = sixel_frame_merge_key(frame, src, pixel_count, 1,
                                       frame->transparent);
        if (SIXEL_FAILED(status)) {
            sixel_allocator_free(frame->allocator, normalized_pixels);
            goto end;
        }
        frame->transparent = -1;
        frame->palette_extent = entries;
        sixel_frame_release_owned_pixels(frame);
        frame->pixels.u8ptr = normalized_pixels;
        frame->pixel_extent = size;
        frame->owns_pixels = 1;
        /* A palette entry uses the source frame's transfer function. */
        frame->format.layout = SIXEL_PIXELFORMAT_RGB888;
        break;
    case SIXEL_PIXELFORMAT_G1:
    case SIXEL_PIXELFORMAT_G2:
    case SIXEL_PIXELFORMAT_G4:
        size = pixel_count * 3u;
        normalized_pixels = sixel_allocator_malloc(frame->allocator, size);
        if (normalized_pixels == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        /* Use the last third as G8 staging. Forward triplet writes only
         * overwrite samples already read, including the last pixel. */
        src = normalized_pixels + pixel_count * 2u;
        sixel_pixelformat_expand_gray8(src, raw_pixels, frame->width,
                                      frame->height, frame->format.layout);
        for (i = 0; i < pixel_count; ++i) {
            memset(normalized_pixels + i * 3u, src[i], 3u);
        }
        sixel_frame_release_owned_pixels(frame);
        frame->pixels.u8ptr = normalized_pixels;
        frame->pixel_extent = size;
        frame->owns_pixels = 1;
        /* Only the storage changed. The caller still has to convert the
         * original gray transfer function to the requested RGB space. */
        frame->format.layout = SIXEL_PIXELFORMAT_RGB888;
        break;
    case SIXEL_PIXELFORMAT_RGB888:
        break;
    case SIXEL_PIXELFORMAT_BGR888:
        /* encode_bytes already copied these triplets. Reorder owned
         * storage in place and keep the transfer function unchanged. */
        if (frame->owns_pixels) {
            sixel_frame_swap_rb(raw_pixels, pixel_count);
            frame->format.layout = SIXEL_PIXELFORMAT_RGB888;
            break;
        }
        /* Borrowed storage uses the out-of-place normalizer below. */
        /* FALLTHROUGH */
    case SIXEL_PIXELFORMAT_G8:
    case SIXEL_PIXELFORMAT_GA88:
    case SIXEL_PIXELFORMAT_AG88:
    case SIXEL_PIXELFORMAT_RGB555:
    case SIXEL_PIXELFORMAT_RGB565:
    case SIXEL_PIXELFORMAT_BGR555:
    case SIXEL_PIXELFORMAT_BGR565:
    case SIXEL_PIXELFORMAT_RGBX8888:
    case SIXEL_PIXELFORMAT_RGBA8888:
    case SIXEL_PIXELFORMAT_XRGB8888:
    case SIXEL_PIXELFORMAT_ARGB8888:
    case SIXEL_PIXELFORMAT_BGRX8888:
    case SIXEL_PIXELFORMAT_BGRA8888:
    case SIXEL_PIXELFORMAT_XBGR8888:
    case SIXEL_PIXELFORMAT_ABGR8888:
    case SIXEL_PIXELFORMAT_RGBFLOAT32:
    case SIXEL_PIXELFORMAT_LINEARRGBFLOAT32:
    case SIXEL_PIXELFORMAT_OKLABFLOAT32:
    case SIXEL_PIXELFORMAT_CIELABFLOAT32:
    case SIXEL_PIXELFORMAT_DIN99DFLOAT32:
        /* normalize pixelformat */
        size = pixel_count * 3u;
        normalized_pixels = (unsigned char *)
            sixel_allocator_malloc(frame->allocator, size);
        if (normalized_pixels == NULL) {
            sixel_helper_set_additional_message(
                "sixel_frame_convert_to_rgb888: "
                "sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        if (frame->format.layout == SIXEL_PIXELFORMAT_RGBFLOAT32) {
            source_pixels = (unsigned char const *)float_pixels;
        }
        status = sixel_helper_normalize_pixelformat(normalized_pixels,
                                                    &format,
                                                    source_pixels,
                                                    sixel_frame_format(frame),
                                                    frame->width,
                                                    frame->height);
        if (SIXEL_SUCCEEDED(status)) {
            status = sixel_frame_merge_packed_alpha(frame, pixel_count);
        }
        if (SIXEL_FAILED(status)) {
            sixel_allocator_free(frame->allocator, normalized_pixels);
            goto end;
        }
        sixel_frame_release_owned_pixels(frame);
        frame->pixels.u8ptr = normalized_pixels;
        frame->pixel_extent = size;
        frame->format.layout = format;
        frame->owns_pixels = 1;
        break;
    default:
        status = SIXEL_LOGIC_ERROR;
        sixel_helper_set_additional_message(
            "sixel_frame_convert_to_rgb888: invalid pixelformat.");
        goto end;
    }

    status = SIXEL_OK;

end:
    sixel_frame_unref(frame);

    return status;
}

static void
sixel_frame_assign_layout(sixel_frame_t *frame, int layout, int coordinates)
{
    sixel_cdef_free(frame->format.rgb);
    frame->format.rgb = NULL;
    sixel_ycbcr_def_free(frame->ycbcr);
    frame->ycbcr = NULL;
    frame->format.layout = layout;
    frame->format.coordinates = coordinates;
}

SIXEL_INTERNAL_API void
sixel_frame_assign_format(sixel_frame_t *frame, int pixelformat)
{
    sixel_frame_assign_layout(frame,
        SIXEL_WORKFORMAT_IS_FLOAT32(pixelformat)
            ? SIXEL_PIXELFORMAT_RGBFLOAT32 : pixelformat,
        sixel_pixelformat_get_colorspace(pixelformat));
}

#if HAVE_DIAGNOSTIC_UNUSED_FUNCTION
# if defined(__GNUC__) && !defined(__PCC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunused-function"
# endif
#endif

/*
 * Select the float pixelformat that matches the frame's current colorspace
 * so downstream conversions interpret each channel correctly.  OKLab uses
 * a [-0.5, 0.5] range for a/b, while gamma/linear share the 0-1 interval.
 */
SIXEL_INTERNAL_API int
sixel_frame_format(sixel_frame_t const *frame)
{
    return frame->format.layout == SIXEL_PIXELFORMAT_RGBFLOAT32
        ? sixel_pixelformat_for_colorspace(
            frame->format.coordinates, 1) : frame->format.layout;
}

static SIXELSTATUS
sixel_frame_vtbl_set_rgb_definition(sixel_frame_interface_t *iface,
                                    sixel_color_def_t const *definition)
{
    sixel_color_def_t *copy;
    sixel_frame_t *frame;

    frame = sixel_frame_from_interface(iface);
    if (frame == NULL || definition == NULL ||
        frame->format.layout != SIXEL_PIXELFORMAT_RGBFLOAT32 ||
        (frame->format.coordinates != SIXEL_COLORSPACE_SRGB &&
         frame->format.coordinates != SIXEL_COLORSPACE_SRGB_LINEAR &&
         frame->format.coordinates != SIXEL_FRAME_COLORSPACE_PROFILE_RGB)) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (!sixel_cdef_frame_input_ready(definition)) {
        sixel_helper_set_additional_message(
            "RGB definition has no frame input path.");
        return SIXEL_BAD_INPUT;
    }
    copy = sixel_cdef_clone(definition);
    if (copy == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    sixel_cdef_free(frame->format.rgb);
    frame->format.coordinates = SIXEL_FRAME_COLORSPACE_PROFILE_RGB;
    frame->format.rgb = copy;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_frame_resize_transparent_mask(
    sixel_frame_t *frame,
    int src_width,
    int src_height,
    int dst_width,
    int dst_height,
    int method_for_resampling,
    unsigned char **pmask_out,
    size_t *pmask_size_out,
    int *phas_transparent_out)
{
    SIXELSTATUS status;
    unsigned char *src_mask;
    float *src_rgb;
    float *dst_rgb;
    unsigned char *dst_mask;
    size_t src_pixel_count;
    size_t dst_pixel_count;
    size_t src_rgb_size;
    size_t dst_rgb_size;
    size_t index;
    float value;
    int has_transparent;

    status = SIXEL_OK;
    src_mask = NULL;
    src_rgb = NULL;
    dst_rgb = NULL;
    dst_mask = NULL;
    src_pixel_count = 0u;
    dst_pixel_count = 0u;
    src_rgb_size = 0u;
    dst_rgb_size = 0u;
    index = 0u;
    value = 0.0f;
    has_transparent = 0;

    if (pmask_out == NULL || pmask_size_out == NULL
        || phas_transparent_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *pmask_out = NULL;
    *pmask_size_out = 0u;
    *phas_transparent_out = 0;

    if (frame == NULL || frame->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (frame->transparent_mask == NULL || frame->transparent_mask_size == 0u) {
        return SIXEL_OK;
    }

    if (src_width <= 0 || src_height <= 0 ||
        dst_width <= 0 || dst_height <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    if ((size_t)src_width > SIZE_MAX / (size_t)src_height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    if ((size_t)dst_width > SIZE_MAX / (size_t)dst_height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    src_pixel_count = (size_t)src_width * (size_t)src_height;
    dst_pixel_count = (size_t)dst_width * (size_t)dst_height;

    /*
     * The mask must have one byte per source pixel. If the size drifted from
     * geometry metadata, discard the stale mask and continue without alpha.
     */
    if (frame->transparent_mask_size != src_pixel_count) {
        sixel_allocator_free(frame->allocator, frame->transparent_mask);
        frame->transparent_mask = NULL;
        frame->transparent_mask_size = 0u;
        frame->coverage_extent = 0;
        frame->alpha_zero_is_transparent = 0;
        return SIXEL_OK;
    }
    if (src_pixel_count > SIZE_MAX / (3u * sizeof(float)) ||
        dst_pixel_count > SIZE_MAX / (3u * sizeof(float))) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    src_rgb_size = src_pixel_count * 3u;
    dst_rgb_size = dst_pixel_count * 3u;

    src_rgb_size *= sizeof(float);
    dst_rgb_size *= sizeof(float);
    src_rgb = (float *)sixel_allocator_malloc(frame->allocator,
                                               src_rgb_size);
    if (src_rgb == NULL) {
        sixel_helper_set_additional_message(
            "sixel_frame_resize_transparent_mask: "
            "sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    dst_rgb = (float *)sixel_allocator_malloc(frame->allocator,
                                               dst_rgb_size);
    if (dst_rgb == NULL) {
        sixel_helper_set_additional_message(
            "sixel_frame_resize_transparent_mask: "
            "sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    dst_mask = (unsigned char *)sixel_allocator_malloc(frame->allocator,
                                                       dst_pixel_count);
    if (dst_mask == NULL) {
        sixel_helper_set_additional_message(
            "sixel_frame_resize_transparent_mask: "
            "sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    src_mask = frame->transparent_mask;
    for (index = 0u; index < src_pixel_count; ++index) {
        value = src_mask[index] != 0u ? 1.0f : 0.0f;
        src_rgb[index * 3u + 0u] = value;
        src_rgb[index * 3u + 1u] = value;
        src_rgb[index * 3u + 2u] = value;
    }

    status = sixel_helper_scale_image_float32(
        dst_rgb,
        src_rgb,
        src_width,
        src_height,
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
        dst_width,
        dst_height,
        method_for_resampling,
        frame->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    for (index = 0u; index < dst_pixel_count; ++index) {
        value = dst_rgb[index * 3u + 0u];
        /* Keep exact half coverage independent of byte SIMD rounding. */
        if (value >= 0.5f) {
            dst_mask[index] = 1u;
            has_transparent = 1;
        } else {
            dst_mask[index] = 0u;
        }
    }

    if (has_transparent != 0) {
        *pmask_out = dst_mask;
        *pmask_size_out = dst_pixel_count;
        *phas_transparent_out = 1;
        dst_mask = NULL;
    }
    status = SIXEL_OK;

end:
    sixel_allocator_free(frame->allocator, src_rgb);
    sixel_allocator_free(frame->allocator, dst_rgb);
    sixel_allocator_free(frame->allocator, dst_mask);

    return status;
}

static SIXELSTATUS
sixel_frame_promote_to_float32(sixel_frame_t *frame, int target_space)
{
    float *float_pixels;
    unsigned char *byte_pixels;
    unsigned char const *pixel;
    size_t pixel_total;
    size_t bytes;
    size_t index;
    size_t base;
    int float_pixelformat;
    int source_space;
    int step;
    int index_r;
    int index_g;
    int index_b;
    SIXELSTATUS status;
    int index_format;
    int palette_entries;
    unsigned char *unpacked;
    unsigned char const *indices;
    unsigned char r8;
    unsigned char g8;
    unsigned char b8;
    sixel_rgb_reader_t packed_reader = NULL;

    step = 0;
    index_r = 0;
    index_g = 0;
    index_b = 0;

    /*
     * Derive the byte stride and per-channel offsets instead of coercing the
     * frame to RGB888 first.  OKLab buffers keep their signed A/B buckets in
     * the order dictated by pixelformat, so preserving that layout avoids the
     * blue casts reported when we reinterpreted them as gamma RGB.
     */
    switch (sixel_frame_format(frame)) {
    case SIXEL_PIXELFORMAT_PAL1:
    case SIXEL_PIXELFORMAT_PAL2:
    case SIXEL_PIXELFORMAT_PAL4:
    case SIXEL_PIXELFORMAT_PAL8:
        if (frame->palette == NULL) {
            return SIXEL_BAD_INPUT;
        }
        index_g = 1;
        index_b = 2;
        break;
    case SIXEL_PIXELFORMAT_RGB555:
    case SIXEL_PIXELFORMAT_BGR555:
    case SIXEL_PIXELFORMAT_RGB565:
    case SIXEL_PIXELFORMAT_BGR565:
        packed_reader = sixel_pixelformat_rgb_reader(frame->format.layout);
        step = 2;
        break;
    case SIXEL_PIXELFORMAT_RGB888:
        step = 3;
        index_r = 0;
        index_g = 1;
        index_b = 2;
        break;
    case SIXEL_PIXELFORMAT_BGR888:
        step = 3;
        index_r = 2;
        index_g = 1;
        index_b = 0;
        break;
    case SIXEL_PIXELFORMAT_RGBX8888:
    case SIXEL_PIXELFORMAT_RGBA8888:
        step = 4;
        index_r = 0;
        index_g = 1;
        index_b = 2;
        break;
    case SIXEL_PIXELFORMAT_XRGB8888:
    case SIXEL_PIXELFORMAT_ARGB8888:
        step = 4;
        index_r = 1;
        index_g = 2;
        index_b = 3;
        break;
    case SIXEL_PIXELFORMAT_BGRX8888:
    case SIXEL_PIXELFORMAT_BGRA8888:
        step = 4;
        index_r = 2;
        index_g = 1;
        index_b = 0;
        break;
    case SIXEL_PIXELFORMAT_XBGR8888:
    case SIXEL_PIXELFORMAT_ABGR8888:
        step = 4;
        index_r = 3;
        index_g = 2;
        index_b = 1;
        break;
    case SIXEL_PIXELFORMAT_G1:
    case SIXEL_PIXELFORMAT_G2:
    case SIXEL_PIXELFORMAT_G4:
    case SIXEL_PIXELFORMAT_G8:
        step = 1;
        index_r = 0;
        index_g = 0;
        index_b = 0;
        break;
    case SIXEL_PIXELFORMAT_GA88:
        step = 2;
        index_r = 0;
        index_g = 0;
        index_b = 0;
        break;
    case SIXEL_PIXELFORMAT_AG88:
        step = 2;
        index_r = 1;
        index_g = 1;
        index_b = 1;
        break;
    default:
        sixel_helper_set_additional_message(
            "sixel_frame_promote_to_float32: unsupported pixelformat.");
        return SIXEL_BAD_INPUT;
    }

    if (frame->width <= 0 || frame->height <= 0 ||
        (size_t)frame->width > SIZE_MAX / (size_t)frame->height) {
        sixel_helper_set_additional_message(
            "sixel_frame_promote_to_float32: overflow.");
        return SIXEL_BAD_INPUT;
    }

    pixel_total = (size_t)frame->width * (size_t)frame->height;
    if (pixel_total > SIZE_MAX / (3U * sizeof(float))) {
        sixel_helper_set_additional_message(
            "sixel_frame_promote_to_float32: buffer too large.");
        return SIXEL_BAD_INPUT;
    }
    bytes = pixel_total * 3U * sizeof(float);
    float_pixels = (float *)sixel_allocator_malloc(frame->allocator, bytes);
    if (float_pixels == NULL) {
        sixel_helper_set_additional_message(
            "sixel_frame_promote_to_float32: "
            "sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    byte_pixels = frame->pixels.u8ptr;
    source_space = frame->format.coordinates;
    float_pixelformat = sixel_pixelformat_for_colorspace(
        source_space, 1);

    /* Unpack indexed rows into scratch. Neither this step nor a later
     * coordinate conversion may publish a partial change to the frame. */
    unpacked = NULL;
    indices = NULL;
    palette_entries = sixel_frame_palette_entries(frame);
    if (step == 1 && frame->format.layout != SIXEL_PIXELFORMAT_G8) {
        unpacked = sixel_allocator_malloc(frame->allocator, pixel_total);
        if (unpacked == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto failed;
        }
        sixel_pixelformat_expand_gray8(unpacked, byte_pixels, frame->width,
                                      frame->height, frame->format.layout);
        byte_pixels = unpacked;
    }
    if (step == 0) {
        indices = byte_pixels;
        if (frame->format.layout != SIXEL_PIXELFORMAT_PAL8) {
            unpacked = sixel_allocator_malloc(frame->allocator, pixel_total);
            if (unpacked == NULL) {
                sixel_allocator_free(frame->allocator, float_pixels);
                return SIXEL_BAD_ALLOCATION;
            }
            status = sixel_helper_normalize_pixelformat(unpacked,
                &index_format, byte_pixels, frame->format.layout,
                frame->width, frame->height);
            if (SIXEL_FAILED(status)) {
                goto failed;
            }
            indices = unpacked;
        }
    }

    for (index = 0U; index < pixel_total; ++index) {
        if (indices != NULL) {
            pixel = sixel_frame_indexed_color(frame, palette_entries,
                                              indices[index]);
            if (pixel == NULL) {
                status = SIXEL_BAD_INPUT;
                goto failed;
            }
        } else {
            pixel = byte_pixels + index * (size_t)step;
        }
        if (packed_reader != NULL) {
            packed_reader(pixel, &r8, &g8, &b8);
        } else {
            r8 = pixel[index_r];
            g8 = pixel[index_g];
            b8 = pixel[index_b];
        }

        base = index * 3U;
        float_pixels[base + 0U] =
            sixel_pixelformat_byte_to_float(float_pixelformat, 0, r8);
        float_pixels[base + 1U] =
            sixel_pixelformat_byte_to_float(float_pixelformat, 1, g8);
        float_pixels[base + 2U] =
            sixel_pixelformat_byte_to_float(float_pixelformat, 2, b8);
    }

    if (source_space != target_space) {
        status = sixel_color_convert_frame_rgbf32(float_pixels, pixel_total,
                                                  source_space, target_space);
        if (SIXEL_FAILED(status)) {
            goto failed;
        }
    }
    if (indices != NULL) {
        status = sixel_frame_merge_key(frame, indices, pixel_total, 1,
                                       frame->transparent);
        if (SIXEL_FAILED(status)) {
            goto failed;
        }
        frame->transparent = -1;
        frame->palette_extent = palette_entries;
    } else {
        status = sixel_frame_merge_packed_alpha(frame, pixel_total);
        if (SIXEL_FAILED(status)) {
            goto failed;
        }
    }
    sixel_allocator_free(frame->allocator, unpacked);
    sixel_frame_release_owned_pixels(frame);
    frame->pixels.f32ptr = float_pixels;
    frame->pixel_extent = bytes;
    frame->owns_pixels = 1;
    sixel_frame_assign_layout(frame, SIXEL_PIXELFORMAT_RGBFLOAT32,
                              target_space);
    return SIXEL_OK;

failed:
    sixel_allocator_free(frame->allocator, unpacked);
    sixel_allocator_free(frame->allocator, float_pixels);
    return status;
}

#if HAVE_DIAGNOSTIC_UNUSED_FUNCTION
# if defined(__GNUC__) && !defined(__PCC__)
#  pragma GCC diagnostic pop
# endif
#endif

/* resize a frame to given size with specified resampling filter */
SIXELAPI SIXELSTATUS
sixel_frame_resize(
    sixel_frame_t *frame,
    int width,
    int height,
    int method_for_resampling
)
{
    SIXELSTATUS status = SIXEL_FALSE;
    size_t size;
    unsigned char *scaled_frame = NULL;
    unsigned char *rendered_rgb = NULL;
    unsigned char const *source;
    unsigned char *resized_mask;
    size_t unused_pixel_total;
    size_t resized_mask_size;
    int unused_depth_bytes;
    int has_transparent;

    if (frame->ycbcr != NULL) {
        return SIXEL_NOT_IMPLEMENTED;
    }
    /* Continuous coverage needs an alpha-aware resampler. Current loader
     * delivery resolves it before encoder geometry; never resize RGB alone. */
    if (frame->alpha != NULL &&
        (width != frame->width || height != frame->height)) {
        return SIXEL_BAD_ARGUMENT;
    }

    sixel_frame_ref(frame);

    size = 0u;
    resized_mask = NULL;
    unused_pixel_total = 0u;
    resized_mask_size = 0u;
    unused_depth_bytes = 0;
    has_transparent = 0;

    /* check parameters */
    if (width <= 0) {
        sixel_helper_set_additional_message(
            "sixel_frame_resize: an invalid width parameter detected.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (height <= 0) {
        sixel_helper_set_additional_message(
            "sixel_frame_resize: an invalid width parameter detected.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (width > SIXEL_WIDTH_LIMIT) {
        sixel_helper_set_additional_message(
            "sixel_frame_resize: given width parameter is too huge.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (height > SIXEL_HEIGHT_LIMIT) {
        sixel_helper_set_additional_message(
            "sixel_frame_resize: given height parameter is too huge.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    status = sixel_frame_validate_size("sixel_frame_resize",
                                       width,
                                       height,
                                       SIXEL_PIXELFORMAT_RGB888,
                                       &unused_pixel_total,
                                       &size,
                                       &unused_depth_bytes);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    if (width == frame->width && height == frame->height) {
        /* nothing to do */
        goto out;
    }

    if (frame->format.layout == SIXEL_PIXELFORMAT_RGBFLOAT32) {
        status = sixel_frame_render_float(frame, SIXEL_PIXELFORMAT_RGB888,
                                          SIXEL_COLORSPACE_SRGB,
                                          &rendered_rgb);
        source = rendered_rgb;
    } else {
        status = sixel_frame_convert_to_rgb888(frame);
        source = frame->pixels.u8ptr;
    }
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    scaled_frame = (unsigned char *)
        sixel_allocator_malloc(frame->allocator, size);
    if (scaled_frame == NULL) {
        sixel_helper_set_additional_message(
            "sixel_frame_resize: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    status = sixel_helper_scale_image(
        scaled_frame,
        source,
        frame->width,
        frame->height,
        3,
        width,
        height,
        method_for_resampling,
        frame->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_frame_resize_transparent_mask(
        frame,
        frame->width,
        frame->height,
        width,
        height,
        method_for_resampling,
        &resized_mask,
        &resized_mask_size,
        &has_transparent);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    sixel_frame_release_owned_pixels(frame);
    if (rendered_rgb != NULL) {
        sixel_frame_assign_format(frame, SIXEL_PIXELFORMAT_RGB888);
    }
    frame->pixels.u8ptr = scaled_frame;
    frame->pixel_extent = size;
    frame->owns_pixels = 1;
    scaled_frame = NULL;
    if (frame->transparent_mask != NULL) {
        sixel_allocator_free(frame->allocator, frame->transparent_mask);
        frame->transparent_mask = NULL;
        frame->transparent_mask_size = 0u;
    }
    frame->transparent_mask = resized_mask;
    frame->transparent_mask_size = resized_mask_size;
    frame->coverage_extent = 0;
    frame->alpha_zero_is_transparent = has_transparent != 0 ? 1 : 0;
    resized_mask = NULL;
    resized_mask_size = 0u;
    frame->width = width;
    frame->height = height;

out:
    status = SIXEL_OK;

end:
    sixel_allocator_free(frame->allocator, rendered_rgb);
    sixel_allocator_free(frame->allocator, resized_mask);
    if (SIXEL_FAILED(status)) {
        sixel_allocator_free(frame->allocator, scaled_frame);
    }
    sixel_frame_unref(frame);

    return status;
}

/*
 * Resize a frame using float buffers. Callers must supply RGB float32 input
 * that already matches the intended resampling basis (linear RGB). The
 * planner is responsible for any required color conversions prior to calling
 * this routine. Unexpected pixelformats are rejected so that order mistakes
 * surface early.
 */
SIXELAPI SIXELSTATUS
sixel_frame_resize_float32(
    sixel_frame_t *frame,
    int width,
    int height,
    int method_for_resampling)
{
    SIXELSTATUS status;
    size_t pixel_total;
    size_t size;
    float *scaled_frame;
    unsigned char *resized_mask;
    int depth;
    int depth_bytes;
    int target_pixelformat;
    size_t resized_mask_size;
    int has_transparent;

    status = SIXEL_FALSE;
    scaled_frame = NULL;
    resized_mask = NULL;
    pixel_total = 0u;
    size = 0u;
    depth = 0;
    depth_bytes = 0;
    target_pixelformat = sixel_frame_format(frame);
    resized_mask_size = 0u;
    has_transparent = 0;

    if (frame->ycbcr != NULL) {
        return SIXEL_NOT_IMPLEMENTED;
    }
    /* Continuous coverage needs an alpha-aware resampler. Current loader
     * delivery resolves it before encoder geometry; never resize RGB alone. */
    if (frame->alpha != NULL &&
        (width != frame->width || height != frame->height)) {
        return SIXEL_BAD_ARGUMENT;
    }

    sixel_frame_ref(frame);

    if (width <= 0) {
        sixel_helper_set_additional_message(
            "sixel_frame_resize_float32: "
            "an invalid width parameter detected.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (height <= 0) {
        sixel_helper_set_additional_message(
            "sixel_frame_resize_float32: "
            "an invalid width parameter detected.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (width > SIXEL_WIDTH_LIMIT) {
        sixel_helper_set_additional_message(
            "sixel_frame_resize_float32: "
            "given width parameter is too huge.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (height > SIXEL_HEIGHT_LIMIT) {
        sixel_helper_set_additional_message(
            "sixel_frame_resize_float32: "
            "given height parameter is too huge.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    if (frame->format.layout != SIXEL_PIXELFORMAT_RGBFLOAT32 ||
        frame->format.coordinates == SIXEL_COLORSPACE_LAB_D65 ||
        frame->format.coordinates == SIXEL_COLORSPACE_OKLAB ||
        frame->format.coordinates == SIXEL_COLORSPACE_DIN99D) {
        sixel_helper_set_additional_message(
            "sixel_frame_resize_float32: "
            "resize expects RGB float32 input.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    if (width == frame->width && height == frame->height) {
        goto out;
    }

    status = sixel_frame_validate_size("sixel_frame_resize_float32",
                                       width,
                                       height,
                                       frame->format.layout,
                                       &pixel_total,
                                       &size,
                                       &depth_bytes);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    /*
     * sixel_frame_validate_size() returns bytes per pixel. Convert the value
     * to channels for validation and reuse the byte count for buffer sizing
     * to avoid overflow on float formats.
     */
    if (depth_bytes % (int)sizeof(float) != 0) {
        sixel_helper_set_additional_message(
            "sixel_frame_resize_float32: "
            "pixelformat depth is not float-aligned.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    depth = depth_bytes / (int)sizeof(float);

    if (depth != 3) {
        sixel_helper_set_additional_message(
            "sixel_frame_resize_float32: "
            "unsupported channel count.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    scaled_frame = (float *)sixel_allocator_malloc(frame->allocator, size);
    if (scaled_frame == NULL) {
        sixel_helper_set_additional_message(
            "sixel_frame_resize_float32: "
            "sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    status = sixel_scale_image_rgbf32(
        scaled_frame,
        frame->pixels.f32ptr,
        frame->width,
        frame->height,
        target_pixelformat,
        width,
        height,
        method_for_resampling,
        frame->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_frame_resize_transparent_mask(
        frame,
        frame->width,
        frame->height,
        width,
        height,
        method_for_resampling,
        &resized_mask,
        &resized_mask_size,
        &has_transparent);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    sixel_frame_release_owned_pixels(frame);
    frame->pixels.f32ptr = scaled_frame;
    frame->pixel_extent = size;
    frame->owns_pixels = 1;
    scaled_frame = NULL;
    if (frame->transparent_mask != NULL) {
        sixel_allocator_free(frame->allocator, frame->transparent_mask);
        frame->transparent_mask = NULL;
        frame->transparent_mask_size = 0u;
    }
    frame->transparent_mask = resized_mask;
    frame->transparent_mask_size = resized_mask_size;
    frame->coverage_extent = 0;
    frame->alpha_zero_is_transparent = has_transparent != 0 ? 1 : 0;
    resized_mask = NULL;
    resized_mask_size = 0u;
    frame->width = width;
    frame->height = height;

out:
    status = SIXEL_OK;

end:
    sixel_allocator_free(frame->allocator, resized_mask);
    if (SIXEL_FAILED(status)) {
        sixel_allocator_free(frame->allocator, scaled_frame);
    }
    sixel_frame_unref(frame);

    return status;
}


static SIXELSTATUS
sixel_frame_clip_transparent_mask(
    sixel_frame_t *frame,
    int src_width,
    int src_height,
    int clip_x,
    int clip_y,
    int clip_width,
    int clip_height)
{
    SIXELSTATUS status;
    size_t src_pixel_count;
    size_t dst_pixel_count;
    size_t index;
    int has_transparent;

    status = SIXEL_OK;
    src_pixel_count = 0u;
    dst_pixel_count = 0u;
    index = 0u;
    has_transparent = 0;

    if (frame == NULL || frame->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (frame->transparent_mask == NULL || frame->transparent_mask_size == 0u) {
        goto done;
    }
    if (src_width <= 0 || src_height <= 0
        || clip_width <= 0 || clip_height <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    if ((size_t)src_width > SIZE_MAX / (size_t)src_height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    if ((size_t)clip_width > SIZE_MAX / (size_t)clip_height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    src_pixel_count = (size_t)src_width * (size_t)src_height;
    dst_pixel_count = (size_t)clip_width * (size_t)clip_height;

    if (frame->transparent_mask_size != src_pixel_count) {
        sixel_allocator_free(frame->allocator, frame->transparent_mask);
        frame->transparent_mask = NULL;
        frame->transparent_mask_size = 0u;
        frame->coverage_extent = 0;
        goto done;
    }

    status = clip(frame->transparent_mask,
                  src_width,
                  src_height,
                  SIXEL_PIXELFORMAT_G8,
                  clip_x,
                  clip_y,
                  clip_width,
                  clip_height);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    for (index = 0u; index < dst_pixel_count; ++index) {
        if (frame->transparent_mask[index] != 0u) {
            has_transparent = 1;
            break;
        }
    }

    if (has_transparent == 0) {
        sixel_allocator_free(frame->allocator, frame->transparent_mask);
        frame->transparent_mask = NULL;
        frame->transparent_mask_size = 0u;
        frame->coverage_extent = 0;
    } else {
        if (frame->coverage_extent < src_pixel_count) {
            frame->coverage_extent = src_pixel_count;
        }
        frame->transparent_mask_size = dst_pixel_count;
    }

done:
    /* A packed alpha plane can still contain holes after the separate
     * mask has been cropped away. Its interpretation survives the crop. */
    if (sixel_frame_alpha_layout(frame->format.layout) == NULL) {
        frame->alpha_zero_is_transparent = has_transparent;
    }
    return SIXEL_OK;
}

static SIXELSTATUS
clip(unsigned char *pixels,
     int sx,
     int sy,
     int pixelformat,
     int cx,
     int cy,
     int cw,
     int ch)
{
    SIXELSTATUS status = SIXEL_FALSE;
    int y;
    unsigned char *src;
    unsigned char *dst;
    int depth;
    char message[256];
    int nwrite;

    /* unused */ (void) sx;
    /* unused */ (void) sy;
    /* unused */ (void) cx;

    depth = sixel_helper_compute_depth(pixelformat);
    if (depth < 0) {
        status = SIXEL_BAD_ARGUMENT;
        /*
         * The compat helper keeps the format string bounded for MSVC while
         * reporting the offending pixelformat to the caller.
         */
        nwrite = sixel_compat_snprintf(
            message,
            sizeof(message),
            "clip: invalid pixelformat(%08x) is specified.",
            pixelformat);
        if (nwrite > 0) {
            sixel_helper_set_additional_message(message);
        }
        goto end;
    }

    dst = pixels;
    src = pixels + cy * sx * depth + cx * depth;
    for (y = 0; y < ch; y++) {
        memmove(dst, src, (size_t)(cw * depth));
        dst += (cw * depth);
        src += (sx * depth);
    }

    status = SIXEL_OK;

end:
    return status;
}


/* clip frame */
SIXELAPI SIXELSTATUS
sixel_frame_clip(
    sixel_frame_t *frame,
    int x,
    int y,
    int width,
    int height
)
{
    SIXELSTATUS status = SIXEL_FALSE;
    int unpacked_format;
    unsigned char *raw_pixels;
    int src_width;
    int src_height;
    int alpha_y;

    if (frame->ycbcr != NULL) {
        return SIXEL_NOT_IMPLEMENTED;
    }
    sixel_frame_ref(frame);

    raw_pixels = frame->pixels.u8ptr;
    src_width = frame->width;
    src_height = frame->height;

    /*
     * Float crop is an exact storage move, including nonfinite samples.
     * Validate its rectangle before moving either pixels or the mask. The
     * CLI filter already trims oversized requests and skips empty regions;
     * direct frame callers must supply a rectangle inside the source.
     * Subtraction after checking the origin avoids signed addition overflow.
     */
    if ((frame->format.layout == SIXEL_PIXELFORMAT_RGBFLOAT32)
            && (x < 0 || y < 0 || x >= src_width || y >= src_height
                || width > src_width - x || height > src_height - y)) {
        sixel_helper_set_additional_message(
            "sixel_frame_clip: float crop rectangle is outside the frame.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    /* check parameters */
    if (width <= 0) {
        sixel_helper_set_additional_message(
            "sixel_frame_clip: an invalid width parameter detected.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (height <= 0) {
        sixel_helper_set_additional_message(
            "sixel_frame_clip: an invalid width parameter detected.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (width > SIXEL_WIDTH_LIMIT) {
        sixel_helper_set_additional_message(
            "sixel_frame_clip: given width parameter is too huge.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (height > SIXEL_HEIGHT_LIMIT) {
        sixel_helper_set_additional_message(
            "sixel_frame_clip: given height parameter is too huge.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    unpacked_format = sixel_frame_unpacked_format(frame->format.layout);
    if (unpacked_format != frame->format.layout) {
        status = sixel_frame_unpack_rows(frame, unpacked_format);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        raw_pixels = frame->pixels.u8ptr;
    }

    /* The crop can remove the largest index while retaining its palette. */
    if (frame->format.layout == SIXEL_PIXELFORMAT_PAL8 && frame->ncolors < 0) {
        frame->palette_extent = sixel_frame_palette_entries(frame);
    }
    status = clip(raw_pixels,
                  frame->width,
                  frame->height,
                  sixel_frame_format(frame),
                  x,
                  y,
                  width,
                  height);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_frame_clip_transparent_mask(frame,
                                               src_width,
                                               src_height,
                                               x,
                                               y,
                                               width,
                                               height);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (frame->alpha != NULL) {
        /* RGB crop is validated above; alpha samples move without arithmetic.
         */
        for (alpha_y = 0; alpha_y < height; ++alpha_y) {
            memmove(frame->alpha + (size_t)alpha_y * width,
                frame->alpha + ((size_t)y + alpha_y) * src_width + x,
                (size_t)width * sizeof(float));
        }
        if (frame->coverage_extent < frame->alpha_count * sizeof(float)) {
            frame->coverage_extent = frame->alpha_count * sizeof(float);
        }
        frame->alpha_count = (size_t)width * (size_t)height;
    }
    frame->width = width;
    frame->height = height;

    status = SIXEL_OK;

end:
    sixel_frame_unref(frame);

    return status;
}


/* Ensure legacy frame constructor and refcounting work. */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
