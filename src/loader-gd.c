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
#include <stdlib.h>

#if HAVE_LIMITS_H
# include <limits.h>
#endif

#if HAVE_STDINT_H
# include <stdint.h>
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

#include <gd.h>

#include <sixel.h>

#include "chunk.h"
#include "frame.h"
#include "loader-common.h"
#include "loader-gd.h"
#include "sixel_atomic.h"

typedef enum sixel_loader_gd_format {
    SIXEL_LOADER_GD_FORMAT_NONE = 0,
    SIXEL_LOADER_GD_FORMAT_GIF,
    SIXEL_LOADER_GD_FORMAT_PNG,
    SIXEL_LOADER_GD_FORMAT_JPEG,
    SIXEL_LOADER_GD_FORMAT_BMP,
    SIXEL_LOADER_GD_FORMAT_TIFF,
    SIXEL_LOADER_GD_FORMAT_WBMP,
    SIXEL_LOADER_GD_FORMAT_TGA,
    SIXEL_LOADER_GD_FORMAT_GD,
    SIXEL_LOADER_GD_FORMAT_GD2,
    SIXEL_LOADER_GD_FORMAT_WEBP
} sixel_loader_gd_format_t;

typedef struct sixel_loader_gd_component {
    sixel_loader_component_t base;
    sixel_atomic_u32_t ref;
    sixel_allocator_t *allocator;
    int fuse_palette;
    int reqcolors;
    int has_bgcolor;
    unsigned char bgcolor[3];
    int srgb_decode_lut_ready;
    double srgb_decode_lut[256];
} sixel_loader_gd_component_t;

typedef struct sixel_loader_gd_support_cache {
    int initialized;
    int bmp;
    int wbmp;
    int tga;
    int tiff;
    int gd;
    int gd2;
    int webp;
} sixel_loader_gd_support_cache_t;

static sixel_loader_gd_support_cache_t g_sixel_loader_gd_support_cache;

static void
gd_initialize_support_cache(void)
{
    if (g_sixel_loader_gd_support_cache.initialized != 0) {
        return;
    }

    g_sixel_loader_gd_support_cache.bmp = gdSupportsFileType(".bmp", 0);
    g_sixel_loader_gd_support_cache.wbmp = gdSupportsFileType(".wbmp", 0);
    g_sixel_loader_gd_support_cache.tga = gdSupportsFileType(".tga", 0);
    g_sixel_loader_gd_support_cache.tiff = gdSupportsFileType(".tiff", 0);
    g_sixel_loader_gd_support_cache.gd = gdSupportsFileType(".gd", 0);
    g_sixel_loader_gd_support_cache.gd2 = gdSupportsFileType(".gd2", 0);
    g_sixel_loader_gd_support_cache.webp = gdSupportsFileType(".webp", 0);
    g_sixel_loader_gd_support_cache.initialized = 1;
}

static int
gd_is_format_supported(sixel_loader_gd_format_t format)
{
    gd_initialize_support_cache();

    switch (format) {
    case SIXEL_LOADER_GD_FORMAT_NONE:
    case SIXEL_LOADER_GD_FORMAT_GIF:
        return 0;
    case SIXEL_LOADER_GD_FORMAT_PNG:
    case SIXEL_LOADER_GD_FORMAT_JPEG:
        return 1;
    case SIXEL_LOADER_GD_FORMAT_BMP:
        return g_sixel_loader_gd_support_cache.bmp;
    case SIXEL_LOADER_GD_FORMAT_TIFF:
        return g_sixel_loader_gd_support_cache.tiff;
    case SIXEL_LOADER_GD_FORMAT_WBMP:
        return g_sixel_loader_gd_support_cache.wbmp;
    case SIXEL_LOADER_GD_FORMAT_TGA:
        return g_sixel_loader_gd_support_cache.tga;
    case SIXEL_LOADER_GD_FORMAT_GD:
#if HAVE_DECL_GDIMAGECREATEFROMGDPTR
        return g_sixel_loader_gd_support_cache.gd;
#else
        return 0;
#endif
    case SIXEL_LOADER_GD_FORMAT_GD2:
#if HAVE_DECL_GDIMAGECREATEFROMGD2PTR
        return g_sixel_loader_gd_support_cache.gd2;
#else
        return 0;
#endif
    case SIXEL_LOADER_GD_FORMAT_WEBP:
#if HAVE_DECL_GDIMAGECREATEFROMWEBPPTR
        return g_sixel_loader_gd_support_cache.webp;
#else
        return 0;
#endif
    default:
        return 0;
    }
}

static sixel_loader_gd_format_t
gd_sniff_format(sixel_chunk_t const *pchunk)
{
    if (pchunk == NULL) {
        return SIXEL_LOADER_GD_FORMAT_NONE;
    }
    if (chunk_is_gif(pchunk)) {
        return SIXEL_LOADER_GD_FORMAT_GIF;
    }
    if (chunk_is_png(pchunk)) {
        return SIXEL_LOADER_GD_FORMAT_PNG;
    }
    if (chunk_is_jpeg(pchunk)) {
        return SIXEL_LOADER_GD_FORMAT_JPEG;
    }
    if (chunk_is_bmp(pchunk)) {
        return SIXEL_LOADER_GD_FORMAT_BMP;
    }
    if (chunk_is_tiff(pchunk)) {
        return SIXEL_LOADER_GD_FORMAT_TIFF;
    }
    if (chunk_is_wbmp(pchunk)) {
        return SIXEL_LOADER_GD_FORMAT_WBMP;
    }
    if (chunk_is_tga(pchunk)) {
        return SIXEL_LOADER_GD_FORMAT_TGA;
    }
    if (chunk_is_gd2(pchunk)) {
        return SIXEL_LOADER_GD_FORMAT_GD2;
    }
    if (chunk_is_gd(pchunk)) {
        return SIXEL_LOADER_GD_FORMAT_GD;
    }
    if (chunk_is_webp(pchunk)) {
        return SIXEL_LOADER_GD_FORMAT_WEBP;
    }

    return SIXEL_LOADER_GD_FORMAT_NONE;
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

static double const *
gd_get_srgb_decode_u8_lut(int *cache_ready,
                          double cache_lut[256],
                          double fallback_lut[256])
{
    if (cache_ready != NULL && cache_lut != NULL) {
        if (*cache_ready == 0) {
            gd_build_srgb_decode_u8_lut(cache_lut);
            *cache_ready = 1;
        }
        return cache_lut;
    }

    if (fallback_lut == NULL) {
        return NULL;
    }
    gd_build_srgb_decode_u8_lut(fallback_lut);
    return fallback_lut;
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
gd_parse_png_ihdr(sixel_chunk_t const *pchunk,
                  int *bit_depth,
                  int *interlace_method)
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
    if (pchunk == NULL || bit_depth == NULL || interlace_method == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *bit_depth = 0;
    *interlace_method = 0;
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
            *interlace_method = (int)data_ptr[12];
            return SIXEL_OK;
        }

        offset += chunk_length + 4u;
    }

    return SIXEL_FALSE;
}

static gdImagePtr
gd_decode_with_format(sixel_loader_gd_format_t format,
                      sixel_chunk_t const *pchunk)
{
    gdImagePtr im;

    im = NULL;
    if (pchunk == NULL) {
        return NULL;
    }

    switch (format) {
    case SIXEL_LOADER_GD_FORMAT_PNG:
        im = gdImageCreateFromPngPtr((int)pchunk->size, pchunk->buffer);
        break;
    case SIXEL_LOADER_GD_FORMAT_JPEG:
        im = gdImageCreateFromJpegPtr((int)pchunk->size, pchunk->buffer);
        break;
    case SIXEL_LOADER_GD_FORMAT_BMP:
        im = gdImageCreateFromBmpPtr((int)pchunk->size, pchunk->buffer);
        break;
    case SIXEL_LOADER_GD_FORMAT_TIFF:
        im = gdImageCreateFromTiffPtr((int)pchunk->size, pchunk->buffer);
        break;
    case SIXEL_LOADER_GD_FORMAT_WBMP:
        im = gdImageCreateFromWBMPPtr((int)pchunk->size, pchunk->buffer);
        break;
    case SIXEL_LOADER_GD_FORMAT_TGA:
        im = gdImageCreateFromTgaPtr((int)pchunk->size, pchunk->buffer);
        break;
    case SIXEL_LOADER_GD_FORMAT_GD:
#if HAVE_DECL_GDIMAGECREATEFROMGDPTR
        im = gdImageCreateFromGdPtr((int)pchunk->size, pchunk->buffer);
#endif
        break;
    case SIXEL_LOADER_GD_FORMAT_GD2:
#if HAVE_DECL_GDIMAGECREATEFROMGD2PTR
        im = gdImageCreateFromGd2Ptr((int)pchunk->size, pchunk->buffer);
#endif
        break;
    case SIXEL_LOADER_GD_FORMAT_WEBP:
#if HAVE_DECL_GDIMAGECREATEFROMWEBPPTR
        im = gdImageCreateFromWebpPtr((int)pchunk->size, pchunk->buffer);
#endif
        break;
    case SIXEL_LOADER_GD_FORMAT_NONE:
    case SIXEL_LOADER_GD_FORMAT_GIF:
    default:
        break;
    }

    return im;
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
        (void)value;
        /*
         * GIF is delegated to lower-priority loaders, so GD ignores
         * animation/static controls.
         */
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
        (void)value;
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_START_FRAME_NO:
        (void)value;
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
    int *srgb_decode_lut_ready;
    double *srgb_decode_lut;

    self = NULL;
    bgcolor = NULL;
    srgb_decode_lut_ready = NULL;
    srgb_decode_lut = NULL;
    if (component == NULL || chunk == NULL || fn_load == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    self = (sixel_loader_gd_component_t *)component;
    if (self->has_bgcolor) {
        bgcolor = self->bgcolor;
    }
    srgb_decode_lut_ready = &self->srgb_decode_lut_ready;
    srgb_decode_lut = self->srgb_decode_lut;

    return load_with_gd(chunk,
                        self->fuse_palette,
                        self->reqcolors,
                        bgcolor,
                        srgb_decode_lut_ready,
                        srgb_decode_lut,
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
    self->fuse_palette = 0;
    self->reqcolors = SIXEL_PALETTE_MAX;
    self->has_bgcolor = 0;
    self->bgcolor[0] = 0;
    self->bgcolor[1] = 0;
    self->bgcolor[2] = 0;
    self->srgb_decode_lut_ready = 0;
    memset(self->srgb_decode_lut, 0, sizeof(self->srgb_decode_lut));

    *ppcomponent = &self->base;
    return SIXEL_OK;
}

int
loader_can_try_gd(sixel_chunk_t const *chunk)
{
    sixel_loader_gd_format_t sniffed_format;
    SIXELSTATUS png_ihdr_status;
    int png_bit_depth;
    int png_interlace_method;

    sniffed_format = SIXEL_LOADER_GD_FORMAT_NONE;
    png_ihdr_status = SIXEL_FALSE;
    png_bit_depth = 0;
    png_interlace_method = 0;
    if (chunk == NULL) {
        return 0;
    }

    sniffed_format = gd_sniff_format(chunk);
    if (sniffed_format == SIXEL_LOADER_GD_FORMAT_NONE ||
            sniffed_format == SIXEL_LOADER_GD_FORMAT_GIF) {
        return 0;
    }

    if (sniffed_format == SIXEL_LOADER_GD_FORMAT_PNG) {
        png_ihdr_status = gd_parse_png_ihdr(chunk,
                                            &png_bit_depth,
                                            &png_interlace_method);
        if (png_ihdr_status == SIXEL_OK && png_interlace_method != 0) {
            return 0;
        }
        if (png_ihdr_status != SIXEL_OK &&
                png_ihdr_status != SIXEL_FALSE &&
                SIXEL_FAILED(png_ihdr_status)) {
            return 0;
        }
    }

    return gd_is_format_supported(sniffed_format);
}

SIXELSTATUS
load_with_gd(
    sixel_chunk_t const       /* in */     *pchunk,     /* image data */
    int                       /* in */     fuse_palette,
    int                       /* in */     reqcolors,
    unsigned char             /* in */     *bgcolor,    /* background */
                                                 /* color */
    int                       /* in/out */ *srgb_decode_lut_ready,
    double                    /* in/out */ srgb_decode_lut[256],
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
    int png_interlace_method;
    int format_supported;
    int *truecolor_row;
    unsigned char *palette_row;
    sixel_loader_gd_format_t sniffed_format;
    int width;
    int height;
    SIXELSTATUS png_ihdr_status;
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
    double const *decode_lut;
    double decode_lut_local[256];
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
    png_interlace_method = 0;
    format_supported = 0;
    sniffed_format = SIXEL_LOADER_GD_FORMAT_NONE;
    width = 0;
    height = 0;
    png_ihdr_status = SIXEL_FALSE;
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
    decode_lut = NULL;
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

    /*
     * Return code policy:
     *   - SIXEL_FALSE: let lower-priority loaders continue.
     *   - SIXEL_GD_ERROR: GD targeted this format but decode failed.
     */
    sniffed_format = gd_sniff_format(pchunk);
    if (sniffed_format == SIXEL_LOADER_GD_FORMAT_NONE ||
            sniffed_format == SIXEL_LOADER_GD_FORMAT_GIF) {
        status = SIXEL_FALSE;
        goto end;
    }

    if (sniffed_format == SIXEL_LOADER_GD_FORMAT_PNG) {
        png_ihdr_status = gd_parse_png_ihdr(
            pchunk,
            &png_bit_depth,
            &png_interlace_method);
        if (png_ihdr_status == SIXEL_OK && png_interlace_method != 0) {
            status = SIXEL_FALSE;
            goto end;
        }
        if (png_ihdr_status != SIXEL_OK &&
                png_ihdr_status != SIXEL_FALSE &&
                SIXEL_FAILED(png_ihdr_status)) {
            status = png_ihdr_status;
            goto end;
        }
    }

    format_supported = gd_is_format_supported(sniffed_format);
    if (format_supported == 0) {
        status = SIXEL_FALSE;
        goto end;
    }

    if (pchunk->size > (size_t)INT_MAX) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto end;
    }
    im = gd_decode_with_format(sniffed_format, pchunk);
    if (im == NULL) {
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

    if (sniffed_format == SIXEL_LOADER_GD_FORMAT_PNG &&
            png_ihdr_status == SIXEL_OK &&
            png_bit_depth > 8) {
        source_high_depth = 1;
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

            decode_lut = gd_get_srgb_decode_u8_lut(
                srgb_decode_lut_ready,
                srgb_decode_lut,
                decode_lut_local);
            if (decode_lut == NULL) {
                status = SIXEL_BAD_ARGUMENT;
                goto end;
            }
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

        decode_lut = gd_get_srgb_decode_u8_lut(
            srgb_decode_lut_ready,
            srgb_decode_lut,
            decode_lut_local);
        if (decode_lut == NULL) {
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
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
