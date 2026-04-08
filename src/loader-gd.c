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
 * libgd-backed loader extracted from loader.c to isolate optional dependencies
 * and keep the central registry lightweight.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if HAVE_GD

#include <stdio.h>
#include <limits.h>
#include <stdlib.h>

#if HAVE_MATH_H
# include <math.h>
#endif

#if HAVE_STRING_H
# include <string.h>
#endif

#include <gd.h>

#include <sixel.h>

#include "chunk.h"
#include "frame.h"
#include "fromgif.h"
#include "loader-common.h"
#include "loader-gd.h"
#include "compat_stub.h"
#include "sixel_atomic.h"


typedef union sixel_loader_gd_fn_pointer {
    sixel_load_image_function fn;
    void *                    p;
} sixel_loader_gd_fn_pointer_t;

typedef struct sixel_loader_gd_component {
    sixel_loader_component_t base;
    sixel_atomic_u32_t ref;
    sixel_allocator_t *allocator;
    int fstatic;
    int fuse_palette;
    int reqcolors;
    int has_bgcolor;
    unsigned char bgcolor[3];
    int loop_control;
    int start_frame_no_set;
    int start_frame_no;
} sixel_loader_gd_component_t;

static SIXELSTATUS
gd_parse_animation_start_frame_no(int *start_frame_no)
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

    parsed = strtol(env_value, &endptr, 10);
    if (endptr == env_value || *endptr != '\0') {
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
gd_resolve_animation_start_frame_no(int start_frame_no,
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

static SIXELSTATUS
gd_count_gif_frames(sixel_chunk_t const *pchunk, int *frame_count)
{
    SIXELSTATUS status;
    unsigned char const *p;
    unsigned char const *end;
    size_t gct_size;
    size_t lct_size;
    unsigned char marker;
    unsigned char packed;
    unsigned char block_size;
    int count;

    status = SIXEL_OK;
    p = NULL;
    end = NULL;
    gct_size = 0;
    lct_size = 0;
    marker = 0;
    packed = 0;
    block_size = 0;
    count = 0;

    if (pchunk->size < 13) {
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    p = pchunk->buffer;
    end = pchunk->buffer + pchunk->size;
    if (memcmp(p, "GIF", 3) != 0) {
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    packed = p[10];
    p += 13;
    if ((packed & 0x80) != 0) {
        gct_size = (size_t)(2U << (packed & 0x07U)) * 3U;
        if ((size_t)(end - p) < gct_size) {
            status = SIXEL_BAD_INPUT;
            goto end;
        }
        p += gct_size;
    }

    while (p < end) {
        marker = *p++;
        if (marker == 0x3b) {
            break;
        }

        if (marker == 0x2c) {
            if ((size_t)(end - p) < 9) {
                status = SIXEL_BAD_INPUT;
                goto end;
            }

            packed = p[8];
            p += 9;
            if ((packed & 0x80) != 0) {
                lct_size = (size_t)(2U << (packed & 0x07U)) * 3U;
                if ((size_t)(end - p) < lct_size) {
                    status = SIXEL_BAD_INPUT;
                    goto end;
                }
                p += lct_size;
            }

            if (p >= end) {
                status = SIXEL_BAD_INPUT;
                goto end;
            }
            ++p;

            for (;;) {
                if (p >= end) {
                    status = SIXEL_BAD_INPUT;
                    goto end;
                }
                block_size = *p++;
                if (block_size == 0) {
                    break;
                }
                if ((size_t)(end - p) < block_size) {
                    status = SIXEL_BAD_INPUT;
                    goto end;
                }
                p += block_size;
            }
            ++count;
            continue;
        }

        if (marker != 0x21) {
            status = SIXEL_BAD_INPUT;
            goto end;
        }
        if (p >= end) {
            status = SIXEL_BAD_INPUT;
            goto end;
        }
        ++p;

        for (;;) {
            if (p >= end) {
                status = SIXEL_BAD_INPUT;
                goto end;
            }
            block_size = *p++;
            if (block_size == 0) {
                break;
            }
            if ((size_t)(end - p) < block_size) {
                status = SIXEL_BAD_INPUT;
                goto end;
            }
            p += block_size;
        }
    }

    if (count <= 0) {
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    *frame_count = count;

end:
    return status;
}

static double
gd_clamp_unit(double value)
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
gd_decode_srgb_unit(double value)
{
    value = gd_clamp_unit(value);
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
gd_build_srgb_decode_u8_lut(double lut[256])
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
        lut[index] = gd_decode_srgb_unit(unit);
    }
}

static unsigned int
gd_read_u32be(unsigned char const *bytes)
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
gd_parse_png_bit_depth(sixel_chunk_t const *pchunk, int *bit_depth)
{
    static unsigned char const png_signature[8] = {
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au
    };
    size_t offset;
    size_t chunk_length;
    unsigned char const *type_ptr;
    unsigned char const *data_ptr;

    offset = 0u;
    chunk_length = 0u;
    type_ptr = NULL;
    data_ptr = NULL;
    if (pchunk == NULL || bit_depth == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *bit_depth = 0;
    if (pchunk->buffer == NULL || pchunk->size < sizeof(png_signature)) {
        return SIXEL_FALSE;
    }
    if (memcmp(pchunk->buffer, png_signature, sizeof(png_signature)) != 0) {
        return SIXEL_FALSE;
    }

    offset = sizeof(png_signature);
    while (offset + 8u <= pchunk->size) {
        chunk_length = (size_t)gd_read_u32be(pchunk->buffer + offset);
        offset += 4u;
        if (offset + 4u + chunk_length + 4u > pchunk->size) {
            return SIXEL_FALSE;
        }

        type_ptr = pchunk->buffer + offset;
        offset += 4u;
        data_ptr = pchunk->buffer + offset;

        if (memcmp(type_ptr, "IHDR", 4u) == 0) {
            if (chunk_length < 13u) {
                return SIXEL_FALSE;
            }
            *bit_depth = (int)data_ptr[8];
            return SIXEL_OK;
        }

        offset += chunk_length + 4u;
    }

    return SIXEL_FALSE;
}

static int
gd_alpha7_to_alpha8(int alpha7)
{
    int clamped;
    int opaque;

    clamped = alpha7;
    opaque = 0;
    if (clamped < gdAlphaOpaque) {
        clamped = gdAlphaOpaque;
    }
    if (clamped > gdAlphaTransparent) {
        clamped = gdAlphaTransparent;
    }

    opaque = gdAlphaTransparent - clamped;
    return (opaque * 255 + gdAlphaTransparent / 2) / gdAlphaTransparent;
}

static float
gd_alpha7_to_unit(int alpha7)
{
    return (float)gd_alpha7_to_alpha8(alpha7) / 255.0f;
}

static unsigned char
gd_clamp_u8(int value)
{
    if (value < 0) {
        return 0u;
    }
    if (value > 255) {
        return 255u;
    }

    return (unsigned char)value;
}

static void
sixel_loader_gd_ref(sixel_loader_component_t *component)
{
    sixel_loader_gd_component_t *self;

    self = NULL;
    if (component == NULL) {
        return;
    }

    self = (sixel_loader_gd_component_t *)component;
    (void)sixel_atomic_fetch_add_u32(&self->ref, 1u);
}

static void
sixel_loader_gd_unref(sixel_loader_component_t *component)
{
    sixel_loader_gd_component_t *self;
    unsigned int previous;

    self = NULL;
    previous = 0u;
    if (component == NULL) {
        return;
    }

    self = (sixel_loader_gd_component_t *)component;
    previous = sixel_atomic_fetch_sub_u32(&self->ref, 1u);
    if (previous != 1u) {
        return;
    }

    sixel_allocator_unref(self->allocator);
    sixel_allocator_free(self->allocator, self);
}

static SIXELSTATUS
sixel_loader_gd_setopt(sixel_loader_component_t *component,
                       int option,
                       void const *value)
{
    sixel_loader_gd_component_t *self;
    int const *int_value;
    unsigned char const *bgcolor;

    self = NULL;
    int_value = NULL;
    bgcolor = NULL;
    if (component == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    self = (sixel_loader_gd_component_t *)component;

    switch (option) {
    case SIXEL_LOADER_OPTION_REQUIRE_STATIC:
        int_value = (int const *)value;
        self->fstatic = int_value != NULL ? *int_value : 0;
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_USE_PALETTE:
        int_value = (int const *)value;
        self->fuse_palette = int_value != NULL ? *int_value : 0;
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_REQCOLORS:
        int_value = (int const *)value;
        if (int_value != NULL) {
            self->reqcolors = *int_value;
        }
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_BGCOLOR:
        if (value == NULL) {
            self->has_bgcolor = 0;
            return SIXEL_OK;
        }
        bgcolor = (unsigned char const *)value;
        self->bgcolor[0] = bgcolor[0];
        self->bgcolor[1] = bgcolor[1];
        self->bgcolor[2] = bgcolor[2];
        self->has_bgcolor = 1;
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_LOOP_CONTROL:
        int_value = (int const *)value;
        if (int_value != NULL) {
            self->loop_control = *int_value;
        }
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_START_FRAME_NO:
        int_value = (int const *)value;
        if (int_value != NULL) {
            self->start_frame_no = *int_value;
            self->start_frame_no_set = 1;
        } else {
            self->start_frame_no_set = 0;
        }
        return SIXEL_OK;
    default:
        return SIXEL_OK;
    }
}

static SIXELSTATUS
sixel_loader_gd_load(sixel_loader_component_t *component,
                     sixel_chunk_t const *chunk,
                     sixel_load_image_function fn_load,
                     void *context)
{
    sixel_loader_gd_component_t *self;
    unsigned char *bgcolor;

    self = NULL;
    bgcolor = NULL;
    if (component == NULL || chunk == NULL || fn_load == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    self = (sixel_loader_gd_component_t *)component;
    if (self->has_bgcolor) {
        bgcolor = self->bgcolor;
    }

    return load_with_gd(chunk,
                        self->fstatic,
                        self->fuse_palette,
                        self->reqcolors,
                        bgcolor,
                        self->loop_control,
                        self->start_frame_no_set,
                        self->start_frame_no,
                        fn_load,
                        context);
}

static char const *
sixel_loader_gd_name(sixel_loader_component_t const *component)
{
    (void)component;
    return "gd";
}

static sixel_loader_component_vtbl_t const g_sixel_loader_gd_vtbl = {
    sixel_loader_gd_ref,
    sixel_loader_gd_unref,
    sixel_loader_gd_setopt,
    sixel_loader_gd_load,
    sixel_loader_gd_name,
};

SIXELSTATUS
sixel_loader_gd_new(sixel_allocator_t *allocator,
                    sixel_loader_component_t **ppcomponent)
{
    sixel_loader_gd_component_t *self;

    self = NULL;
    if (allocator == NULL || ppcomponent == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *ppcomponent = NULL;
    self = (sixel_loader_gd_component_t *)
        sixel_allocator_malloc(allocator, sizeof(*self));
    if (self == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    self->base.vtbl = &g_sixel_loader_gd_vtbl;
    self->ref = 1u;
    self->allocator = allocator;
    sixel_allocator_ref(allocator);
    self->fstatic = 0;
    self->fuse_palette = 0;
    self->reqcolors = SIXEL_PALETTE_MAX;
    self->has_bgcolor = 0;
    self->bgcolor[0] = 0;
    self->bgcolor[1] = 0;
    self->bgcolor[2] = 0;
    self->loop_control = SIXEL_LOOP_AUTO;
    self->start_frame_no_set = 0;
    self->start_frame_no = INT_MIN;

    *ppcomponent = &self->base;
    return SIXEL_OK;
}

SIXELSTATUS
load_with_gd(
    sixel_chunk_t const       /* in */     *pchunk,     /* image data */
    int                       /* in */     fstatic,     /* static */
    int                       /* in */     fuse_palette,
    int                       /* in */     reqcolors,
    unsigned char             /* in */     *bgcolor,    /* background */
                                                 /* color */
    int                       /* in */     loop_control,
    int                       /* in */     start_frame_no_set,
    int                       /* in */     start_frame_no_override,
    sixel_load_image_function /* in */     fn_load,     /* callback */
    void                      /* in/out */ *context     /* private */
                                                 /* data for callback */
)
{
    SIXELSTATUS status;
    sixel_frame_t *frame;
    gdImagePtr im;
    unsigned char *pixels_u8;
    float *pixels_f32;
    unsigned char *palette;
    unsigned char *mask;
    size_t pixel_count;
    size_t index;
    int y;
    int x;
    int c;
    int gif;
    int bmp;
    int wbmp;
    int tga;
    int tiff;
    int gd;
    int gd2;
    int webp;
    int start_frame_no;
    int resolved_start_frame_no;
    int frame_count;
    int *truecolor_row;
    unsigned char *palette_row;
    sixel_loader_gd_fn_pointer_t fnp;
    int width;
    int height;
    int png_bit_depth;
    int source_high_depth;
    int has_alpha_like;
    int has_partial_alpha;
    int has_keycolor_alpha;
    int has_bg_for_alpha;
    int promote_float32;
    int transparent_index;
    int reqcolors_clamped;
    int ncolors;
    int key_index;
    int alpha7;
    int alpha_u8;
    int use_pal8;
    int zero_alpha_count;
    int palette_index;
    unsigned char palette_sample;
    float alpha_unit;
    double decode_lut[256];
    double src_linear[3];
    double bg_linear[3];
    double out_linear[3];
    unsigned char r8;
    unsigned char g8;
    unsigned char b8;
    unsigned char zero_alpha_map[SIXEL_PALETTE_MAX];

    if (pchunk == NULL || pchunk->allocator == NULL || fn_load == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = SIXEL_FALSE;
    frame = NULL;
    im = NULL;
    pixels_u8 = NULL;
    pixels_f32 = NULL;
    palette = NULL;
    mask = NULL;
    pixel_count = 0u;
    index = 0u;
    y = 0;
    x = 0;
    c = 0;
    gif = gdSupportsFileType(".gif", 0);
    bmp = gdSupportsFileType(".bmp", 0);
    wbmp = gdSupportsFileType(".wbmp", 0);
    tga = gdSupportsFileType(".tga", 0);
    tiff = gdSupportsFileType(".tiff", 0);
    gd = gdSupportsFileType(".gd", 0);
    gd2 = gdSupportsFileType(".gd2", 0);
    webp = gdSupportsFileType(".webp", 0);
    (void) gd;
    (void) gd2;
    (void) webp;
    start_frame_no = INT_MIN;
    resolved_start_frame_no = INT_MIN;
    frame_count = 0;
    width = 0;
    height = 0;
    png_bit_depth = 0;
    source_high_depth = 0;
    has_alpha_like = 0;
    has_partial_alpha = 0;
    has_keycolor_alpha = 0;
    has_bg_for_alpha = 0;
    promote_float32 = 0;
    transparent_index = -1;
    reqcolors_clamped = 0;
    ncolors = 0;
    key_index = -1;
    alpha7 = 0;
    alpha_u8 = 0;
    use_pal8 = 0;
    zero_alpha_count = 0;
    palette_index = 0;
    palette_sample = 0u;
    alpha_unit = 0.0f;
    src_linear[0] = 0.0;
    src_linear[1] = 0.0;
    src_linear[2] = 0.0;
    bg_linear[0] = 0.0;
    bg_linear[1] = 0.0;
    bg_linear[2] = 0.0;
    out_linear[0] = 0.0;
    out_linear[1] = 0.0;
    out_linear[2] = 0.0;
    r8 = 0u;
    g8 = 0u;
    b8 = 0u;
    memset(zero_alpha_map, 0, sizeof(zero_alpha_map));
    fnp.fn = fn_load;

    if (gif && chunk_is_gif(pchunk)) {
        /*
         * GIF is the only animated path in this loader. Keep start-frame
         * validation scoped here so static decode ignores invalid env values.
         */
        if (start_frame_no_set) {
            start_frame_no = start_frame_no_override;
        } else {
            status = gd_parse_animation_start_frame_no(&start_frame_no);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
        }

        if (start_frame_no != INT_MIN) {
            status = gd_count_gif_frames(pchunk, &frame_count);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
            status = gd_resolve_animation_start_frame_no(
                start_frame_no,
                frame_count,
                &resolved_start_frame_no);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
        }

        status = load_gif(pchunk->buffer,
                          (int)pchunk->size,
                          bgcolor,
                          reqcolors,
                          fuse_palette,
                          fstatic,
                          loop_control,
                          resolved_start_frame_no,
                          fnp.p,
                          context,
                          pchunk->allocator);
        goto end;
    }

    if (im == NULL && chunk_is_png(pchunk)) {
        im = gdImageCreateFromPngPtr((int)pchunk->size, pchunk->buffer);
    }

    if (im == NULL && chunk_is_jpeg(pchunk)) {
        im = gdImageCreateFromJpegPtr((int)pchunk->size, pchunk->buffer);
    }

    if (im == NULL && bmp) {
        im = gdImageCreateFromBmpPtr((int)pchunk->size, pchunk->buffer);
    }

    if (im == NULL && chunk_is_bmp(pchunk)) {
        im = gdImageCreateFromBmpPtr((int)pchunk->size, pchunk->buffer);
    }

    if (im == NULL && tiff) {
        im = gdImageCreateFromTiffPtr((int)pchunk->size, pchunk->buffer);
    }

    if (im == NULL && wbmp) {
        im = gdImageCreateFromWBMPPtr((int)pchunk->size, pchunk->buffer);
    }

    if (im == NULL && tga) {
        im = gdImageCreateFromTgaPtr((int)pchunk->size, pchunk->buffer);
    }

#if HAVE_DECL_GDIMAGECREATEFROMGDPTR
    if (im == NULL && gd) {
        im = gdImageCreateFromGdPtr((int)pchunk->size, pchunk->buffer);
    }
#endif

#if HAVE_DECL_GDIMAGECREATEFROMGD2PTR
    if (im == NULL && gd2) {
        im = gdImageCreateFromGd2Ptr((int)pchunk->size, pchunk->buffer);
    }
#endif

#if HAVE_DECL_GDIMAGECREATEFROMWEBPPTR
    if (im == NULL && webp && chunk_is_webp(pchunk)) {
        im = gdImageCreateFromWebpPtr((int)pchunk->size, pchunk->buffer);
    }
#endif

    if (im == NULL) {
        /*
         * GD could not decode the input. Signal a backend-specific error so
         * the caller can report that GD rejected the buffer after sniffing.
         */
        status = SIXEL_GD_ERROR;
        goto end;
    }

    if (gdImageSX(im) <= 0 || gdImageSY(im) <= 0) {
        /*
         * GD returned a stub image without valid dimensions. Prevent
         * downstream allocations when the frame size is not usable.
         */
        status = SIXEL_GD_ERROR;
        goto end;
    }

    if (gdImageTrueColor(im)) {
        if (im->tpixels == NULL) {
            /*
             * Some malformed inputs make GD allocate an image shell without
             * tpixels. gdImageTrueColorPixel() would dereference this field,
             * so abort loading before accessing it.
             */
            status = SIXEL_GD_ERROR;
            goto end;
        }

        for (y = 0; y < gdImageSY(im); y++) {
            truecolor_row = im->tpixels[y];
            if (truecolor_row == NULL) {
                /*
                 * GD sometimes allocates the tpixels array but leaves one or
                 * more row pointers unset. The encoder would crash on such a
                 * row, so reject the image before iterating over its pixels.
                 */
                status = SIXEL_GD_ERROR;
                goto end;
            }
        }
    } else {
        if (im->pixels == NULL) {
            /*
             * Paletted images also rely on a populated pixel buffer. Reject
             * frames that omit it to avoid null dereferences when accessing
             * palette entries.
             */
            status = SIXEL_GD_ERROR;
            goto end;
        }

        for (y = 0; y < gdImageSY(im); y++) {
            palette_row = im->pixels[y];
            if (palette_row == NULL) {
                /*
                 * gdImageGetPixel() reads the row pointer directly. Guard
                 * against partially initialised rows before touching them.
                 */
                status = SIXEL_GD_ERROR;
                goto end;
            }
        }
    }

    status = gd_parse_png_bit_depth(pchunk, &png_bit_depth);
    if (status == SIXEL_OK && png_bit_depth > 8) {
        source_high_depth = 1;
    } else if (status != SIXEL_FALSE && SIXEL_FAILED(status)) {
        goto end;
    }

    width = gdImageSX(im);
    height = gdImageSY(im);
    if ((size_t)width > SIZE_MAX / (size_t)height) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto end;
    }
    pixel_count = (size_t)width * (size_t)height;

    status = sixel_frame_new(&frame, pchunk->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    frame->width = width;
    frame->height = height;
    frame->ncolors = -1;
    frame->transparent = -1;
    frame->alpha_zero_is_transparent = 0;

    reqcolors_clamped = reqcolors;
    if (reqcolors_clamped <= 0 || reqcolors_clamped > SIXEL_PALETTE_MAX) {
        reqcolors_clamped = SIXEL_PALETTE_MAX;
    }

    if (!gdImageTrueColor(im)) {
        /*
         * Paletted decode policy:
         *   - preserve indexed data when palette output is requested and
         *     the palette size fits reqcolors,
         *   - preserve binary key transparency as PAL8+transparent index,
         *   - otherwise fall back to RGB plus transparent mask.
         */
        ncolors = gdImageColorsTotal(im);
        if (ncolors <= 0 || ncolors > SIXEL_PALETTE_MAX) {
            status = SIXEL_GD_ERROR;
            goto end;
        }

        memset(zero_alpha_map, 0, sizeof(zero_alpha_map));
        transparent_index = gdImageGetTransparent(im);
        if (transparent_index < 0 || transparent_index >= ncolors) {
            transparent_index = -1;
        }

        has_partial_alpha = 0;
        has_keycolor_alpha = 0;
        has_alpha_like = 0;
        zero_alpha_count = 0;
        key_index = -1;

        for (c = 0; c < ncolors; ++c) {
            alpha7 = gdImageAlpha(im, c);
            if (alpha7 < gdAlphaOpaque) {
                alpha7 = gdAlphaOpaque;
            }
            if (alpha7 > gdAlphaTransparent) {
                alpha7 = gdAlphaTransparent;
            }
            if (c == transparent_index || alpha7 >= gdAlphaTransparent) {
                zero_alpha_map[c] = 1u;
                ++zero_alpha_count;
                continue;
            }
            if (alpha7 > gdAlphaOpaque) {
                has_partial_alpha = 1;
            }
        }

        has_keycolor_alpha = zero_alpha_count > 0 ? 1 : 0;
        has_alpha_like = (has_keycolor_alpha != 0 || has_partial_alpha != 0)
            ? 1
            : 0;

        if (has_keycolor_alpha != 0) {
            if (transparent_index >= 0 &&
                zero_alpha_map[transparent_index] != 0u) {
                key_index = transparent_index;
            } else {
                for (c = 0; c < ncolors; ++c) {
                    if (zero_alpha_map[c] != 0u) {
                        key_index = c;
                        break;
                    }
                }
            }
        }

        use_pal8 = fuse_palette != 0 &&
            ncolors <= reqcolors_clamped &&
            has_partial_alpha == 0 &&
            bgcolor == NULL;
        if (use_pal8 != 0) {
            if (pixel_count > SIXEL_ALLOCATE_BYTES_MAX) {
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }
            pixels_u8 = (unsigned char *)sixel_allocator_malloc(
                pchunk->allocator,
                pixel_count);
            if (pixels_u8 == NULL) {
                sixel_helper_set_additional_message(
                    "load_with_gd: sixel_allocator_malloc() failed.");
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }

            if ((size_t)ncolors > SIZE_MAX / 3u ||
                (size_t)ncolors * 3u > SIXEL_ALLOCATE_BYTES_MAX) {
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }
            palette = (unsigned char *)sixel_allocator_malloc(
                pchunk->allocator,
                (size_t)ncolors * 3u);
            if (palette == NULL) {
                sixel_helper_set_additional_message(
                    "load_with_gd: sixel_allocator_malloc() failed.");
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }

            index = 0u;
            for (y = 0; y < height; ++y) {
                palette_row = im->pixels[y];
                for (x = 0; x < width; ++x) {
                    palette_sample = palette_row[x];
                    if ((int)palette_sample >= ncolors) {
                        palette_sample = 0u;
                    }
                    if (has_keycolor_alpha != 0 &&
                        zero_alpha_count > 1 &&
                        key_index >= 0 &&
                        zero_alpha_map[palette_sample] != 0u &&
                        (int)palette_sample != key_index) {
                        palette_sample = (unsigned char)key_index;
                    }
                    pixels_u8[index++] = palette_sample;
                }
            }
            for (c = 0; c < ncolors; ++c) {
                palette[(size_t)c * 3u + 0u] = gd_clamp_u8(im->red[c]);
                palette[(size_t)c * 3u + 1u] = gd_clamp_u8(im->green[c]);
                palette[(size_t)c * 3u + 2u] = gd_clamp_u8(im->blue[c]);
            }

            frame->pixels.u8ptr = pixels_u8;
            pixels_u8 = NULL;
            frame->palette = palette;
            palette = NULL;
            frame->ncolors = ncolors;
            frame->pixelformat = SIXEL_PIXELFORMAT_PAL8;
            frame->colorspace = SIXEL_COLORSPACE_GAMMA;
            frame->transparent = has_keycolor_alpha != 0 ? key_index : -1;
            frame->alpha_zero_is_transparent = 0;
            goto emit_frame;
        }

        has_bg_for_alpha = (bgcolor != NULL && has_alpha_like != 0) ? 1 : 0;
        promote_float32 = (source_high_depth != 0 || has_bg_for_alpha != 0)
            ? 1
            : 0;
        if (promote_float32 != 0) {
            if (pixel_count > SIZE_MAX / (3u * sizeof(float)) ||
                pixel_count * 3u * sizeof(float) >
                    SIXEL_ALLOCATE_BYTES_MAX) {
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }
            pixels_f32 = (float *)sixel_allocator_malloc(
                pchunk->allocator,
                pixel_count * 3u * sizeof(float));
            if (pixels_f32 == NULL) {
                sixel_helper_set_additional_message(
                    "load_with_gd: sixel_allocator_malloc() failed.");
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }
            if (has_alpha_like != 0 && bgcolor == NULL) {
                if (pixel_count > SIXEL_ALLOCATE_BYTES_MAX) {
                    status = SIXEL_BAD_ALLOCATION;
                    goto end;
                }
                mask = (unsigned char *)sixel_allocator_malloc(
                    pchunk->allocator,
                    pixel_count);
                if (mask == NULL) {
                    sixel_helper_set_additional_message(
                        "load_with_gd: sixel_allocator_malloc() failed.");
                    status = SIXEL_BAD_ALLOCATION;
                    goto end;
                }
            }

            gd_build_srgb_decode_u8_lut(decode_lut);
            if (has_bg_for_alpha != 0) {
                bg_linear[0] = decode_lut[bgcolor[0]];
                bg_linear[1] = decode_lut[bgcolor[1]];
                bg_linear[2] = decode_lut[bgcolor[2]];
            }

            index = 0u;
            for (y = 0; y < height; ++y) {
                palette_row = im->pixels[y];
                for (x = 0; x < width; ++x) {
                    palette_sample = palette_row[x];
                    if ((int)palette_sample >= ncolors) {
                        palette_sample = 0u;
                    }
                    r8 = gd_clamp_u8(im->red[palette_sample]);
                    g8 = gd_clamp_u8(im->green[palette_sample]);
                    b8 = gd_clamp_u8(im->blue[palette_sample]);
                    if (has_alpha_like != 0) {
                        palette_index = (int)palette_sample;
                        if (zero_alpha_map[palette_index] != 0u) {
                            alpha7 = gdAlphaTransparent;
                        } else {
                            alpha7 = gdImageAlpha(im, palette_index);
                        }
                        alpha_unit = gd_alpha7_to_unit(alpha7);
                    } else {
                        alpha_unit = 1.0f;
                    }

                    src_linear[0] = decode_lut[r8];
                    src_linear[1] = decode_lut[g8];
                    src_linear[2] = decode_lut[b8];
                    if (has_bg_for_alpha != 0) {
                        out_linear[0] = src_linear[0] * (double)alpha_unit +
                            bg_linear[0] * (1.0 - (double)alpha_unit);
                        out_linear[1] = src_linear[1] * (double)alpha_unit +
                            bg_linear[1] * (1.0 - (double)alpha_unit);
                        out_linear[2] = src_linear[2] * (double)alpha_unit +
                            bg_linear[2] * (1.0 - (double)alpha_unit);
                    } else if (has_alpha_like != 0) {
                        out_linear[0] = src_linear[0] * (double)alpha_unit;
                        out_linear[1] = src_linear[1] * (double)alpha_unit;
                        out_linear[2] = src_linear[2] * (double)alpha_unit;
                        if (mask != NULL) {
                            mask[index] = alpha_unit <= 0.0f ? 1u : 0u;
                        }
                    } else {
                        out_linear[0] = src_linear[0];
                        out_linear[1] = src_linear[1];
                        out_linear[2] = src_linear[2];
                    }

                    pixels_f32[index * 3u + 0u] = (float)out_linear[0];
                    pixels_f32[index * 3u + 1u] = (float)out_linear[1];
                    pixels_f32[index * 3u + 2u] = (float)out_linear[2];
                    ++index;
                }
            }

            frame->pixels.f32ptr = pixels_f32;
            pixels_f32 = NULL;
            frame->pixelformat = SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
            frame->colorspace = SIXEL_COLORSPACE_LINEAR;
            if (mask != NULL) {
                frame->transparent_mask = mask;
                frame->transparent_mask_size = pixel_count;
                frame->alpha_zero_is_transparent = 1;
                mask = NULL;
            }
            goto emit_frame;
        }

        if (pixel_count > SIZE_MAX / 3u ||
            pixel_count * 3u > SIXEL_ALLOCATE_BYTES_MAX) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        pixels_u8 = (unsigned char *)sixel_allocator_malloc(
            pchunk->allocator,
            pixel_count * 3u);
        if (pixels_u8 == NULL) {
            sixel_helper_set_additional_message(
                "load_with_gd: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        if (has_alpha_like != 0) {
            if (pixel_count > SIXEL_ALLOCATE_BYTES_MAX) {
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }
            mask = (unsigned char *)sixel_allocator_malloc(
                pchunk->allocator,
                pixel_count);
            if (mask == NULL) {
                sixel_helper_set_additional_message(
                    "load_with_gd: sixel_allocator_malloc() failed.");
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }
        }

        index = 0u;
        for (y = 0; y < height; ++y) {
            palette_row = im->pixels[y];
            for (x = 0; x < width; ++x) {
                palette_sample = palette_row[x];
                if ((int)palette_sample >= ncolors) {
                    palette_sample = 0u;
                }
                r8 = gd_clamp_u8(im->red[palette_sample]);
                g8 = gd_clamp_u8(im->green[palette_sample]);
                b8 = gd_clamp_u8(im->blue[palette_sample]);
                if (has_alpha_like != 0) {
                    palette_index = (int)palette_sample;
                    if (zero_alpha_map[palette_index] != 0u) {
                        alpha7 = gdAlphaTransparent;
                    } else {
                        alpha7 = gdImageAlpha(im, palette_index);
                    }
                    alpha_u8 = gd_alpha7_to_alpha8(alpha7);
                    if (alpha_u8 <= 0) {
                        pixels_u8[index * 3u + 0u] = 0u;
                        pixels_u8[index * 3u + 1u] = 0u;
                        pixels_u8[index * 3u + 2u] = 0u;
                        mask[index] = 1u;
                    } else if (alpha_u8 >= 255) {
                        pixels_u8[index * 3u + 0u] = r8;
                        pixels_u8[index * 3u + 1u] = g8;
                        pixels_u8[index * 3u + 2u] = b8;
                        mask[index] = 0u;
                    } else {
                        pixels_u8[index * 3u + 0u] = (unsigned char)(
                            ((unsigned int)r8 * (unsigned int)alpha_u8 +
                             127u) / 255u);
                        pixels_u8[index * 3u + 1u] = (unsigned char)(
                            ((unsigned int)g8 * (unsigned int)alpha_u8 +
                             127u) / 255u);
                        pixels_u8[index * 3u + 2u] = (unsigned char)(
                            ((unsigned int)b8 * (unsigned int)alpha_u8 +
                             127u) / 255u);
                        mask[index] = 0u;
                    }
                } else {
                    pixels_u8[index * 3u + 0u] = r8;
                    pixels_u8[index * 3u + 1u] = g8;
                    pixels_u8[index * 3u + 2u] = b8;
                }
                ++index;
            }
        }

        frame->pixels.u8ptr = pixels_u8;
        pixels_u8 = NULL;
        frame->pixelformat = SIXEL_PIXELFORMAT_RGB888;
        frame->colorspace = SIXEL_COLORSPACE_GAMMA;
        if (mask != NULL) {
            frame->transparent_mask = mask;
            frame->transparent_mask_size = pixel_count;
            frame->alpha_zero_is_transparent = 1;
            mask = NULL;
        }
        goto emit_frame;
    }

    has_alpha_like = 0;
    for (y = 0; y < height && has_alpha_like == 0; ++y) {
        for (x = 0; x < width; ++x) {
            c = gdImageTrueColorPixel(im, x, y);
            if (gdTrueColorGetAlpha(c) > gdAlphaOpaque) {
                has_alpha_like = 1;
                break;
            }
        }
    }

    has_bg_for_alpha = (bgcolor != NULL && has_alpha_like != 0) ? 1 : 0;
    promote_float32 = (source_high_depth != 0 || has_bg_for_alpha != 0)
        ? 1
        : 0;

    if (promote_float32 != 0) {
        /*
         * Linear float32 output is used for high-depth hints and whenever
         * background composition is requested for alpha-bearing images.
         */
        if (pixel_count > SIZE_MAX / (3u * sizeof(float)) ||
            pixel_count * 3u * sizeof(float) > SIXEL_ALLOCATE_BYTES_MAX) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        pixels_f32 = (float *)sixel_allocator_malloc(
            pchunk->allocator,
            pixel_count * 3u * sizeof(float));
        if (pixels_f32 == NULL) {
            sixel_helper_set_additional_message(
                "load_with_gd: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        if (has_alpha_like != 0 && bgcolor == NULL) {
            if (pixel_count > SIXEL_ALLOCATE_BYTES_MAX) {
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }
            mask = (unsigned char *)sixel_allocator_malloc(
                pchunk->allocator,
                pixel_count);
            if (mask == NULL) {
                sixel_helper_set_additional_message(
                    "load_with_gd: sixel_allocator_malloc() failed.");
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }
        }

        gd_build_srgb_decode_u8_lut(decode_lut);
        if (has_bg_for_alpha != 0) {
            bg_linear[0] = decode_lut[bgcolor[0]];
            bg_linear[1] = decode_lut[bgcolor[1]];
            bg_linear[2] = decode_lut[bgcolor[2]];
        }

        index = 0u;
        for (y = 0; y < height; ++y) {
            for (x = 0; x < width; ++x) {
                c = gdImageTrueColorPixel(im, x, y);
                r8 = (unsigned char)gdTrueColorGetRed(c);
                g8 = (unsigned char)gdTrueColorGetGreen(c);
                b8 = (unsigned char)gdTrueColorGetBlue(c);
                if (has_alpha_like != 0) {
                    alpha7 = gdTrueColorGetAlpha(c);
                    alpha_unit = gd_alpha7_to_unit(alpha7);
                } else {
                    alpha_unit = 1.0f;
                }

                src_linear[0] = decode_lut[r8];
                src_linear[1] = decode_lut[g8];
                src_linear[2] = decode_lut[b8];
                if (has_bg_for_alpha != 0) {
                    out_linear[0] = src_linear[0] * (double)alpha_unit +
                        bg_linear[0] * (1.0 - (double)alpha_unit);
                    out_linear[1] = src_linear[1] * (double)alpha_unit +
                        bg_linear[1] * (1.0 - (double)alpha_unit);
                    out_linear[2] = src_linear[2] * (double)alpha_unit +
                        bg_linear[2] * (1.0 - (double)alpha_unit);
                } else if (has_alpha_like != 0) {
                    out_linear[0] = src_linear[0] * (double)alpha_unit;
                    out_linear[1] = src_linear[1] * (double)alpha_unit;
                    out_linear[2] = src_linear[2] * (double)alpha_unit;
                    if (mask != NULL) {
                        mask[index] = alpha_unit <= 0.0f ? 1u : 0u;
                    }
                } else {
                    out_linear[0] = src_linear[0];
                    out_linear[1] = src_linear[1];
                    out_linear[2] = src_linear[2];
                }

                pixels_f32[index * 3u + 0u] = (float)out_linear[0];
                pixels_f32[index * 3u + 1u] = (float)out_linear[1];
                pixels_f32[index * 3u + 2u] = (float)out_linear[2];
                ++index;
            }
        }

        frame->pixels.f32ptr = pixels_f32;
        pixels_f32 = NULL;
        frame->pixelformat = SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
        frame->colorspace = SIXEL_COLORSPACE_LINEAR;
        if (mask != NULL) {
            frame->transparent_mask = mask;
            frame->transparent_mask_size = pixel_count;
            frame->alpha_zero_is_transparent = 1;
            mask = NULL;
        }
        goto emit_frame;
    }

    if (has_alpha_like != 0) {
        /*
         * Background-less alpha output keeps the byte fast path while
         * carrying transparency as an external binary mask.
         */
        if (pixel_count > SIZE_MAX / 3u ||
            pixel_count * 3u > SIXEL_ALLOCATE_BYTES_MAX) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        pixels_u8 = (unsigned char *)sixel_allocator_malloc(
            pchunk->allocator,
            pixel_count * 3u);
        if (pixels_u8 == NULL) {
            sixel_helper_set_additional_message(
                "load_with_gd: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        if (pixel_count > SIXEL_ALLOCATE_BYTES_MAX) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        mask = (unsigned char *)sixel_allocator_malloc(
            pchunk->allocator,
            pixel_count);
        if (mask == NULL) {
            sixel_helper_set_additional_message(
                "load_with_gd: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }

        index = 0u;
        for (y = 0; y < height; ++y) {
            for (x = 0; x < width; ++x) {
                c = gdImageTrueColorPixel(im, x, y);
                alpha_u8 = gd_alpha7_to_alpha8(gdTrueColorGetAlpha(c));
                r8 = (unsigned char)gdTrueColorGetRed(c);
                g8 = (unsigned char)gdTrueColorGetGreen(c);
                b8 = (unsigned char)gdTrueColorGetBlue(c);
                if (alpha_u8 <= 0) {
                    pixels_u8[index * 3u + 0u] = 0u;
                    pixels_u8[index * 3u + 1u] = 0u;
                    pixels_u8[index * 3u + 2u] = 0u;
                    mask[index] = 1u;
                } else if (alpha_u8 >= 255) {
                    pixels_u8[index * 3u + 0u] = r8;
                    pixels_u8[index * 3u + 1u] = g8;
                    pixels_u8[index * 3u + 2u] = b8;
                    mask[index] = 0u;
                } else {
                    pixels_u8[index * 3u + 0u] = (unsigned char)(
                        ((unsigned int)r8 * (unsigned int)alpha_u8 +
                         127u) / 255u);
                    pixels_u8[index * 3u + 1u] = (unsigned char)(
                        ((unsigned int)g8 * (unsigned int)alpha_u8 +
                         127u) / 255u);
                    pixels_u8[index * 3u + 2u] = (unsigned char)(
                        ((unsigned int)b8 * (unsigned int)alpha_u8 +
                         127u) / 255u);
                    mask[index] = 0u;
                }
                ++index;
            }
        }

        frame->pixels.u8ptr = pixels_u8;
        pixels_u8 = NULL;
        frame->pixelformat = SIXEL_PIXELFORMAT_RGB888;
        frame->colorspace = SIXEL_COLORSPACE_GAMMA;
        frame->transparent_mask = mask;
        frame->transparent_mask_size = pixel_count;
        frame->alpha_zero_is_transparent = 1;
        mask = NULL;
        goto emit_frame;
    }

    /*
     * Opaque truecolor data remains on the 8-bit fast path.
     */
    if (pixel_count > SIZE_MAX / 3u || pixel_count * 3u >
            SIXEL_ALLOCATE_BYTES_MAX) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    pixels_u8 = (unsigned char *)sixel_allocator_malloc(
        pchunk->allocator,
        pixel_count * 3u);
    if (pixels_u8 == NULL) {
        sixel_helper_set_additional_message(
            "load_with_gd: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    index = 0u;
    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            c = gdImageTrueColorPixel(im, x, y);
            pixels_u8[index * 3u + 0u] = (unsigned char)gdTrueColorGetRed(c);
            pixels_u8[index * 3u + 1u] = (unsigned char)gdTrueColorGetGreen(c);
            pixels_u8[index * 3u + 2u] = (unsigned char)gdTrueColorGetBlue(c);
            ++index;
        }
    }

    frame->pixels.u8ptr = pixels_u8;
    pixels_u8 = NULL;
    frame->pixelformat = SIXEL_PIXELFORMAT_RGB888;
    frame->colorspace = SIXEL_COLORSPACE_GAMMA;

emit_frame:
    status = SIXEL_OK;

    status = fn_load(frame, context);

end:
    if (im != NULL) {
        gdImageDestroy(im);
    }
    if (frame != NULL) {
        sixel_frame_unref(frame);
    }
    sixel_allocator_free(pchunk->allocator, pixels_u8);
    sixel_allocator_free(pchunk->allocator, pixels_f32);
    sixel_allocator_free(pchunk->allocator, palette);
    sixel_allocator_free(pchunk->allocator, mask);
    return status;
}

#endif  /* HAVE_GD */

#if !HAVE_GD
/*
 * Avoid empty translation unit warnings when GD support is disabled.
 */
typedef int loader_gd_disabled;
#endif


/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
