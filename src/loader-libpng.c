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
 * libpng-backed loader helpers extracted from loader.c. Concentrating PNG
 * handling here keeps the main registry lean and isolates libpng headers from
 * other backends while preserving the existing control flow and diagnostics.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if HAVE_LIBPNG

#if !defined(_POSIX_C_SOURCE)
# define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <setjmp.h>

#if HAVE_STRING_H
# include <string.h>
#endif
#if HAVE_ERRNO_H
# include <errno.h>
#endif
#if HAVE_LIMITS_H
# include <limits.h>
#endif

#include <png.h>

#include <sixel.h>

#include "allocator.h"
#include "chunk.h"
#include "loader-common.h"
#include "frame.h"
#include "loader-libpng.h"
#include "logger.h"

static void
read_png(png_structp png_ptr,
         png_bytep data,
         png_size_t length)
{
    sixel_chunk_t *pchunk;

    pchunk = (sixel_chunk_t *)png_get_io_ptr(png_ptr);
    if (length > pchunk->size) {
        length = pchunk->size;
    }
    if (length > 0) {
        memcpy(data, pchunk->buffer, length);
        pchunk->buffer += length;
        pchunk->size -= length;
    }
}

static void
read_palette(png_structp png_ptr,
             png_infop info_ptr,
             unsigned char *palette,
             int ncolors,
             png_color *png_palette,
             png_color_16 *pbackground,
             int *transparent)
{
    png_bytep trans;
    int num_trans;
    int i;

    trans = NULL;
    num_trans = 0;
    i = 0;

    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
        png_get_tRNS(png_ptr, info_ptr, &trans, &num_trans, NULL);
    }
    if (num_trans > 0) {
        *transparent = trans[0];
    }
    for (i = 0; i < ncolors; ++i) {
        if (pbackground && i < num_trans) {
            palette[i * 3 + 0] = ((0xff - trans[i]) * pbackground->red
                                   + trans[i] * png_palette[i].red) >> 8;
            palette[i * 3 + 1] = ((0xff - trans[i]) * pbackground->green
                                   + trans[i] * png_palette[i].green) >> 8;
            palette[i * 3 + 2] = ((0xff - trans[i]) * pbackground->blue
                                   + trans[i] * png_palette[i].blue) >> 8;
        } else {
            palette[i * 3 + 0] = png_palette[i].red;
            palette[i * 3 + 1] = png_palette[i].green;
            palette[i * 3 + 2] = png_palette[i].blue;
        }
    }
}

#if HAVE_SETJMP && HAVE_LONGJMP
static jmp_buf jmpbuf;
#endif  /* HAVE_SETJMP && HAVE_LONGJMP */

/* libpng error handler */
static void
png_error_callback(png_structp png_ptr, png_const_charp error_message)
{
    (void) png_ptr;

    sixel_helper_set_additional_message(error_message);
#if HAVE_SETJMP && HAVE_LONGJMP
    longjmp(jmpbuf, 1);
#endif  /* HAVE_SETJMP && HAVE_LONGJMP */
}

static SIXELSTATUS
load_png(unsigned char      /* out */ **result,
         unsigned char      /* in */  *buffer,
         size_t             /* in */  size,
         int                /* out */ *psx,
         int                /* out */ *psy,
         unsigned char      /* out */ **ppalette,
         int                /* out */ *pncolors,
         int                /* in */  reqcolors,
         int                /* out */ *pixelformat,
         unsigned char      /* out */ *bgcolor,
         int                /* out */ *transparent,
         sixel_allocator_t  /* in */  *allocator)
{
    SIXELSTATUS status;
    sixel_chunk_t read_chunk;
    png_uint_32 bitdepth;
    png_uint_32 png_status;
    png_structp png_ptr;
    png_infop info_ptr;
#ifdef HAVE_DIAGNOSTIC_CLOBBERED
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wclobbered"
#endif
    unsigned char **rows = NULL;
    png_color *png_palette = NULL;
    png_color_16 background;
    png_color_16p default_background;
    png_uint_32 width;
    png_uint_32 height;
    int i;
    int depth;

#if HAVE_SETJMP && HAVE_LONGJMP
    if (setjmp(jmpbuf) != 0) {
        sixel_allocator_free(allocator, *result);
        *result = NULL;
        status = SIXEL_PNG_ERROR;
        goto cleanup;
    }
#endif  /* HAVE_SETJMP && HAVE_LONGJMP */

    status = SIXEL_FALSE;
    *result = NULL;

    png_ptr = png_create_read_struct(
        PNG_LIBPNG_VER_STRING, NULL, &png_error_callback, NULL);
    if (!png_ptr) {
        sixel_helper_set_additional_message(
            "png_create_read_struct() failed.");
        status = SIXEL_PNG_ERROR;
        goto cleanup;
    }

    /*
     * The minimum valid PNG is 67 bytes.
     * https://garethrees.org/2007/11/14/pngcrush/
     */
    if (size < 67) {
        sixel_helper_set_additional_message("PNG data too small to be valid!");
        status = SIXEL_PNG_ERROR;
        goto cleanup;
    }

#if HAVE_SETJMP
    if (setjmp(png_jmpbuf(png_ptr)) != 0) {
        sixel_allocator_free(allocator, *result);
        *result = NULL;
        status = SIXEL_PNG_ERROR;
        goto cleanup;
    }
#endif  /* HAVE_SETJMP */

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        sixel_helper_set_additional_message(
            "png_create_info_struct() failed.");
        status = SIXEL_PNG_ERROR;
        png_destroy_read_struct(&png_ptr, (png_infopp)0, (png_infopp)0);
        goto cleanup;
    }
    read_chunk.buffer = buffer;
    read_chunk.size = size;

    png_set_read_fn(png_ptr,(png_voidp)&read_chunk, read_png);
    png_read_info(png_ptr, info_ptr);

    width = png_get_image_width(png_ptr, info_ptr);
    height = png_get_image_height(png_ptr, info_ptr);

    if (width > INT_MAX || height > INT_MAX) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto cleanup;
    }

    *psx = (int)width;
    *psy = (int)height;

    bitdepth = png_get_bit_depth(png_ptr, info_ptr);
    if (bitdepth == 16) {
#  if HAVE_DEBUG
        fprintf(stderr, "bitdepth: %u\n", (unsigned int)bitdepth);
        fprintf(stderr, "stripping to 8bit...\n");
#  endif
        png_set_strip_16(png_ptr);
        bitdepth = 8;
    }

    if (bgcolor) {
#  if HAVE_DEBUG
        fprintf(stderr,
                "background color is specified [%02x, %02x, %02x]\n",
                bgcolor[0], bgcolor[1], bgcolor[2]);
#  endif
        background.red = bgcolor[0];
        background.green = bgcolor[1];
        background.blue = bgcolor[2];
        background.gray = (bgcolor[0] + bgcolor[1] + bgcolor[2]) / 3;
    } else if (png_get_bKGD(png_ptr, info_ptr, &default_background)
            == PNG_INFO_bKGD) {
        memcpy(&background, default_background, sizeof(background));
#  if HAVE_DEBUG
        fprintf(stderr,
                "background color is found [%02x, %02x, %02x]\n",
                background.red, background.green, background.blue);
#  endif
    } else {
        background.red = 0;
        background.green = 0;
        background.blue = 0;
        background.gray = 0;
    }

    switch (png_get_color_type(png_ptr, info_ptr)) {
    case PNG_COLOR_TYPE_PALETTE:
#  if HAVE_DEBUG
        fprintf(stderr, "paletted PNG(PNG_COLOR_TYPE_PALETTE)\n");
#  endif
        png_status = png_get_PLTE(png_ptr, info_ptr,
                                  &png_palette, pncolors);
        if (png_status != PNG_INFO_PLTE || png_palette == NULL) {
            sixel_helper_set_additional_message(
                "PLTE chunk not found");
            status = SIXEL_PNG_ERROR;
            goto cleanup;
        }
#  if HAVE_DEBUG
        fprintf(stderr, "palette colors: %d\n", *pncolors);
        fprintf(stderr, "bitdepth: %u\n", (unsigned int)bitdepth);
#  endif
        if (ppalette == NULL || *pncolors > reqcolors) {
#  if HAVE_DEBUG
            fprintf(stderr, "detected more colors than reqired(>%d).\n",
                    reqcolors);
            fprintf(stderr, "expand to RGB format...\n");
#  endif
            png_set_background(png_ptr, &background,
                               PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
            png_set_palette_to_rgb(png_ptr);
            png_set_strip_alpha(png_ptr);
            *pixelformat = SIXEL_PIXELFORMAT_RGB888;
        } else {
            switch (bitdepth) {
            case 1:
                *ppalette = (unsigned char *)
                    sixel_allocator_malloc(allocator,
                                           (size_t)*pncolors * 3);
                if (*ppalette == NULL) {
                    sixel_helper_set_additional_message(
                        "load_png: sixel_allocator_malloc() failed.");
                    status = SIXEL_BAD_ALLOCATION;
                    goto cleanup;
                }
                read_palette(png_ptr, info_ptr, *ppalette,
                             *pncolors,
                             png_palette,
                             &background,
                             transparent);
                *pixelformat = SIXEL_PIXELFORMAT_PAL1;
                break;
            case 2:
                *ppalette = (unsigned char *)
                    sixel_allocator_malloc(allocator,
                                           (size_t)*pncolors * 3);
                if (*ppalette == NULL) {
                    sixel_helper_set_additional_message(
                        "load_png: sixel_allocator_malloc() failed.");
                    status = SIXEL_BAD_ALLOCATION;
                    goto cleanup;
                }
                read_palette(png_ptr, info_ptr, *ppalette,
                             *pncolors,
                             png_palette,
                             &background,
                             transparent);
                *pixelformat = SIXEL_PIXELFORMAT_PAL2;
                break;
            case 4:
                *ppalette = (unsigned char *)
                    sixel_allocator_malloc(allocator,
                                           (size_t)*pncolors * 3);
                if (*ppalette == NULL) {
                    sixel_helper_set_additional_message(
                        "load_png: sixel_allocator_malloc() failed.");
                    status = SIXEL_BAD_ALLOCATION;
                    goto cleanup;
                }
                read_palette(png_ptr, info_ptr, *ppalette,
                             *pncolors,
                             png_palette,
                             &background,
                             transparent);
                *pixelformat = SIXEL_PIXELFORMAT_PAL4;
                break;
            case 8:
                *ppalette = (unsigned char *)
                    sixel_allocator_malloc(allocator,
                                           (size_t)*pncolors * 3);
                if (*ppalette == NULL) {
                    sixel_helper_set_additional_message(
                        "load_png: sixel_allocator_malloc() failed.");
                    status = SIXEL_BAD_ALLOCATION;
                    goto cleanup;
                }
                read_palette(png_ptr, info_ptr, *ppalette,
                             *pncolors,
                             png_palette,
                             &background,
                             transparent);
                *pixelformat = SIXEL_PIXELFORMAT_PAL8;
                break;
            default:
                png_set_background(png_ptr, &background,
                                   PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
                png_set_palette_to_rgb(png_ptr);
                *pixelformat = SIXEL_PIXELFORMAT_RGB888;
                break;
            }
        }
        break;
    case PNG_COLOR_TYPE_GRAY:
#  if HAVE_DEBUG
        fprintf(stderr, "grayscale PNG(PNG_COLOR_TYPE_GRAY)\n");
        fprintf(stderr, "bitdepth: %u\n", (unsigned int)bitdepth);
#  endif
        if (1 << bitdepth > reqcolors) {
#  if HAVE_DEBUG
            fprintf(stderr, "detected more colors than reqired(>%d).\n",
                    reqcolors);
            fprintf(stderr, "expand into RGB format...\n");
#  endif
            png_set_background(png_ptr, &background,
                               PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
            png_set_gray_to_rgb(png_ptr);
            *pixelformat = SIXEL_PIXELFORMAT_RGB888;
        } else {
            switch (bitdepth) {
            case 1:
            case 2:
            case 4:
                if (ppalette) {
#  if HAVE_DECL_PNG_SET_EXPAND_GRAY_1_2_4_TO_8
#   if HAVE_DEBUG
                    fprintf(stderr, "expand %u bpp to 8bpp format...\n",
                            (unsigned int)bitdepth);
#   endif
                    png_set_expand_gray_1_2_4_to_8(png_ptr);
                    *pixelformat = SIXEL_PIXELFORMAT_G8;
#  elif HAVE_DECL_PNG_SET_GRAY_1_2_4_TO_8
#   if HAVE_DEBUG
                    fprintf(stderr, "expand %u bpp to 8bpp format...\n",
                            (unsigned int)bitdepth);
#   endif
                    png_set_gray_1_2_4_to_8(png_ptr);
                    *pixelformat = SIXEL_PIXELFORMAT_G8;
#  else
#   if HAVE_DEBUG
                    fprintf(stderr, "expand into RGB format...\n");
#   endif
                    png_set_background(png_ptr, &background,
                                       PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
                    png_set_gray_to_rgb(png_ptr);
                    *pixelformat = SIXEL_PIXELFORMAT_RGB888;
#  endif
                } else {
                    png_set_background(png_ptr, &background,
                                       PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
                    png_set_gray_to_rgb(png_ptr);
                    *pixelformat = SIXEL_PIXELFORMAT_RGB888;
                }
                break;
            case 8:
                if (ppalette) {
                    *pixelformat = SIXEL_PIXELFORMAT_G8;
                } else {
#  if HAVE_DEBUG
                    fprintf(stderr, "expand into RGB format...\n");
#  endif
                    png_set_background(png_ptr, &background,
                                       PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
                    png_set_gray_to_rgb(png_ptr);
                    *pixelformat = SIXEL_PIXELFORMAT_RGB888;
                }
                break;
            default:
#  if HAVE_DEBUG
                fprintf(stderr, "expand into RGB format...\n");
#  endif
                png_set_background(png_ptr, &background,
                                   PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
                png_set_gray_to_rgb(png_ptr);
                *pixelformat = SIXEL_PIXELFORMAT_RGB888;
                break;
            }
        }
        break;
    case PNG_COLOR_TYPE_GRAY_ALPHA:
#  if HAVE_DEBUG
        fprintf(stderr, "grayscale-alpha PNG(PNG_COLOR_TYPE_GRAY_ALPHA)\n");
        fprintf(stderr, "bitdepth: %u\n", (unsigned int)bitdepth);
        fprintf(stderr, "expand to RGB format...\n");
#  endif
        png_set_background(png_ptr, &background,
                           PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
        png_set_gray_to_rgb(png_ptr);
        *pixelformat = SIXEL_PIXELFORMAT_RGB888;
        break;
    case PNG_COLOR_TYPE_RGB_ALPHA:
#  if HAVE_DEBUG
        fprintf(stderr, "RGBA PNG(PNG_COLOR_TYPE_RGB_ALPHA)\n");
        fprintf(stderr, "bitdepth: %u\n", (unsigned int)bitdepth);
        fprintf(stderr, "expand to RGB format...\n");
#  endif
        png_set_background(png_ptr, &background,
                           PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
        *pixelformat = SIXEL_PIXELFORMAT_RGB888;
        break;
    case PNG_COLOR_TYPE_RGB:
#  if HAVE_DEBUG
        fprintf(stderr, "RGB PNG(PNG_COLOR_TYPE_RGB)\n");
        fprintf(stderr, "bitdepth: %u\n", (unsigned int)bitdepth);
#  endif
        png_set_background(png_ptr, &background,
                           PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
        *pixelformat = SIXEL_PIXELFORMAT_RGB888;
        break;
    default:
        /* unknown format */
        goto cleanup;
    }
    depth = sixel_helper_compute_depth(*pixelformat);
    *result = (unsigned char *)
        sixel_allocator_malloc(allocator,
                               (size_t)(*psx * *psy * depth));
    if (*result == NULL) {
        sixel_helper_set_additional_message(
            "load_png: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto cleanup;
    }
    rows = (unsigned char **)sixel_allocator_malloc(
        allocator,
        (size_t)*psy * sizeof(unsigned char *));
    if (rows == NULL) {
        sixel_helper_set_additional_message(
            "load_png: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto cleanup;
    }
    switch (*pixelformat) {
    case SIXEL_PIXELFORMAT_PAL1:
    case SIXEL_PIXELFORMAT_PAL2:
    case SIXEL_PIXELFORMAT_PAL4:
        for (i = 0; i < *psy; ++i) {
            rows[i] = *result + (depth * *psx * (int)bitdepth + 7) / 8 * i;
        }
        break;
    default:
        for (i = 0; i < *psy; ++i) {
            rows[i] = *result + depth * *psx * i;
        }
        break;
    }

    png_read_image(png_ptr, rows);

    status = SIXEL_OK;

cleanup:
    png_destroy_read_struct(&png_ptr, &info_ptr,(png_infopp)0);

    if (rows != NULL) {
        sixel_allocator_free(allocator, rows);
    }

    return status;
}
#ifdef HAVE_DIAGNOSTIC_CLOBBERED
# pragma GCC diagnostic pop
#endif

/*
 * Dedicated libpng loader for precise PNG decoding.
 *
 *    +-----------+     +------------------+     +--------------------+
 *    | PNG chunk | --> | libpng decode    | --> | sixel frame emit   |
 *    +-----------+     +------------------+     +--------------------+
 */
SIXELSTATUS
load_with_libpng(
    sixel_chunk_t const       /* in */     *pchunk,
    int                       /* in */     fstatic,
    int                       /* in */     fuse_palette,
    int                       /* in */     reqcolors,
    unsigned char             /* in */     *bgcolor,
    int                       /* in */     loop_control,
    sixel_load_image_function /* in */     fn_load,
    void                      /* in/out */ *context)
{
    SIXELSTATUS status;
    sixel_frame_t *frame;
    unsigned char *pixels;

    status = SIXEL_FALSE;
    frame = NULL;
    pixels = NULL;

    (void)fstatic;
    (void)loop_control;

    status = sixel_frame_new(&frame, pchunk->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = load_png(&pixels,
                      pchunk->buffer,
                      pchunk->size,
                      &frame->width,
                      &frame->height,
                      &frame->palette,
                      &frame->ncolors,
                      fuse_palette ? reqcolors : 0,
                      &frame->pixelformat,
                      bgcolor,
                      &frame->transparent,
                      pchunk->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    sixel_frame_set_pixels(frame, pixels);

    status = sixel_frame_strip_alpha(frame, bgcolor);
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

int
loader_can_try_libpng(sixel_chunk_t const *chunk)
{
    if (chunk == NULL) {
        return 0;
    }

    return chunk_is_png(chunk);
}

#else  /* !HAVE_LIBPNG */

/*
 * Provide a dummy symbol so that pedantic compilers do not flag the unit as
 * empty when libpng support is disabled at configure time.
 */
enum { sixel_loader_libpng_placeholder = 0 };

#if defined(__GNUC__) || defined(__clang__)
# define SIXEL_LIBPNG_PLACEHOLDER_UNUSED __attribute__((unused))
#else
# define SIXEL_LIBPNG_PLACEHOLDER_UNUSED
#endif

static void
sixel_loader_libpng_placeholder_function(void)
    SIXEL_LIBPNG_PLACEHOLDER_UNUSED;

static void
sixel_loader_libpng_placeholder_function(void)
{
    /*
     * Tie the placeholder enum to a symbol so MSVC does not warn about an
     * empty translation unit when libpng is disabled.
     */
    (void)sixel_loader_libpng_placeholder;
}

#undef SIXEL_LIBPNG_PLACEHOLDER_UNUSED

#endif  /* HAVE_LIBPNG */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
