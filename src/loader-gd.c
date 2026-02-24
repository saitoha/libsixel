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
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_frame_t *frame = NULL;
    gdImagePtr im;
    unsigned char *p;
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

    (void) fstatic;
    (void) fuse_palette;
    (void) reqcolors;
    (void) bgcolor;
    (void) loop_control;

    frame = NULL;
    im = NULL;
    p = NULL;
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
    fnp.fn = fn_load;

    if (start_frame_no_set) {
        start_frame_no = start_frame_no_override;
    } else {
        status = gd_parse_animation_start_frame_no(&start_frame_no);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    if (gif && chunk_is_gif(pchunk)) {
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
        gdImageDestroy(im);
        goto end;
    }

    if (im->trueColor) {
        if (im->tpixels == NULL) {
            /*
             * Some malformed inputs make GD allocate an image shell without
             * tpixels. gdImageTrueColorPixel() would dereference this field,
             * so abort loading before accessing it.
             */
            status = SIXEL_GD_ERROR;
            gdImageDestroy(im);
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
                gdImageDestroy(im);
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
            gdImageDestroy(im);
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
                gdImageDestroy(im);
                goto end;
            }
        }

        status = SIXEL_GD_ERROR;
        gdImageDestroy(im);
        goto end;
    }

    status = sixel_frame_new(&frame, pchunk->allocator);
    if (SIXEL_FAILED(status)) {
        gdImageDestroy(im);
        goto end;
    }

    frame->width = gdImageSX(im);
    frame->height = gdImageSY(im);
    frame->pixelformat = SIXEL_PIXELFORMAT_RGB888;
    frame->colorspace = SIXEL_COLORSPACE_GAMMA;
    sixel_frame_set_pixels(frame,
                           sixel_allocator_malloc(
                               pchunk->allocator,
                               (size_t)(frame->width * frame->height * 3)));
    p = sixel_frame_get_pixels(frame);
    if (p == NULL) {
        sixel_helper_set_additional_message(
            "load_with_gd: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        gdImageDestroy(im);
        goto end;
    }
    for (y = 0; y < frame->height; y++) {
        for (x = 0; x < frame->width; x++) {
            c = gdImageTrueColorPixel(im, x, y);
            *p++ = gdTrueColorGetRed(c);
            *p++ = gdTrueColorGetGreen(c);
            *p++ = gdTrueColorGetBlue(c);
        }
    }
    gdImageDestroy(im);

    status = fn_load(frame, context);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    sixel_frame_unref(frame);

    status = SIXEL_OK;

end:
    return status;
}

#endif  /* HAVE_GD */

#if !HAVE_GD
/*
 * Avoid empty translation unit warnings when GD support is disabled.
 */
typedef int loader_gd_disabled;
#endif
