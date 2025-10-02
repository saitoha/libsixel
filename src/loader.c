/*
 * Copyright (c) 2021-2025 libsixel developers. See `AUTHORS`.
 * Copyright (c) 2014-2019 Hayaki Saito
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

#include "config.h"

/* STDC_HEADERS */
#include <stdio.h>
#include <stdlib.h>

#if HAVE_STRING_H
# include <string.h>
#endif
#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#if HAVE_ERRNO_H
# include <errno.h>
#endif
#if HAVE_LIMITS_H
# include <limits.h>
#endif
#ifdef HAVE_GDK_PIXBUF2
# if HAVE_DIAGNOSTIC_TYPEDEF_REDEFINITION
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wtypedef-redefinition"
# endif
# include <gdk-pixbuf/gdk-pixbuf.h>
# include <gdk-pixbuf/gdk-pixbuf-simple-anim.h>
# if HAVE_DIAGNOSTIC_TYPEDEF_REDEFINITION
#   pragma GCC diagnostic pop
# endif
#endif
#if HAVE_GD
# include <gd.h>
#endif
#if HAVE_LIBPNG
# include <png.h>
#endif  /* HAVE_LIBPNG */
#if HAVE_JPEG
# include <jpeglib.h>
#endif  /* HAVE_JPEG */
#if HAVE_COREGRAPHICS
# include <ApplicationServices/ApplicationServices.h>
# include <ImageIO/ImageIO.h>
#endif  /* HAVE_COREGRAPHICS */

#if !defined(HAVE_MEMCPY)
# define memcpy(d, s, n) (bcopy ((s), (d), (n)))
#endif

#include <sixel.h>
#include "loader.h"
#include "frame.h"
#include "chunk.h"
#include "frompnm.h"
#include "fromgif.h"
#include "allocator.h"

static int loader_trace_enabled;

void
sixel_helper_set_loader_trace(int enable)
{
    loader_trace_enabled = enable ? 1 : 0;
}

static void
loader_trace_try(char const *name)
{
    if (loader_trace_enabled) {
        fprintf(stderr, "libsixel: trying %s loader\n", name);
    }
}

static void
loader_trace_result(char const *name, SIXELSTATUS status)
{
    if (!loader_trace_enabled) {
        return;
    }
    if (SIXEL_SUCCEEDED(status)) {
        fprintf(stderr, "libsixel: loader %s succeeded\n", name);
    } else {
        fprintf(stderr, "libsixel: loader %s failed (%s)\n",
                name, sixel_helper_format_error(status));
    }
}

sixel_allocator_t *stbi_allocator;

void *
stbi_malloc(size_t n)
{
    return sixel_allocator_malloc(stbi_allocator, n);
}

void *
stbi_realloc(void *p, size_t n)
{
    return sixel_allocator_realloc(stbi_allocator, p, n);
}

void
stbi_free(void *p)
{
    sixel_allocator_free(stbi_allocator, p);
}

#define STBI_MALLOC stbi_malloc
#define STBI_REALLOC stbi_realloc
#define STBI_FREE stbi_free

#define STBI_NO_STDIO 1
#define STB_IMAGE_IMPLEMENTATION 1
#define STBI_FAILURE_USERMSG 1
#if defined(_WIN32)
# define STBI_NO_THREAD_LOCALS 1  /* no tls */
#endif
#define STBI_NO_GIF
#define STBI_NO_PNM

#if HAVE_DIAGNOSTIC_SIGN_CONVERSION
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#if HAVE_DIAGNOSTIC_STRICT_OVERFLOW
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wstrict-overflow"
#endif
#if HAVE_DIAGNOSTIC_SWITCH_DEFAULT
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wswitch-default"
#endif
#if HAVE_DIAGNOSTIC_SHADOW
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wshadow"
#endif
#if HAVE_DIAGNOSTIC_DOUBLE_PROMOTION
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdouble-promotion"
#endif
# if HAVE_DIAGNOSTIC_UNUSED_FUNCTION
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wunused-function"
#endif
# if HAVE_DIAGNOSTIC_UNUSED_BUT_SET_VARIABLE
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif
#include "stb_image.h"
#if HAVE_DIAGNOSTIC_UNUSED_BUT_SET_VARIABLE
# pragma GCC diagnostic pop
#endif
#if HAVE_DIAGNOSTIC_UNUSED_FUNCTION
# pragma GCC diagnostic pop
#endif
#if HAVE_DIAGNOSTIC_DOUBLE_PROMOTION
# pragma GCC diagnostic pop
#endif
#if HAVE_DIAGNOSTIC_SHADOW
# pragma GCC diagnostic pop
#endif
#if HAVE_DIAGNOSTIC_SWITCH_DEFAULT
# pragma GCC diagnostic pop
#endif
#if HAVE_DIAGNOSTIC_STRICT_OVERFLOW
# pragma GCC diagnostic pop
#endif
#if HAVE_DIAGNOSTIC_SIGN_CONVERSION
# pragma GCC diagnostic pop
#endif


# if HAVE_JPEG
/* import from @uobikiemukot's sdump loader.h */
static SIXELSTATUS
load_jpeg(unsigned char **result,
          unsigned char *data,
          size_t datasize,
          int *pwidth,
          int *pheight,
          int *ppixelformat,
          sixel_allocator_t *allocator)
{
    SIXELSTATUS status = SIXEL_JPEG_ERROR;
    JDIMENSION row_stride;
    size_t size;
    JSAMPARRAY buffer;
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr pub;

    cinfo.err = jpeg_std_error(&pub);

    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, data, datasize);
    jpeg_read_header(&cinfo, TRUE);

    /* disable colormap (indexed color), grayscale -> rgb */
    cinfo.quantize_colors = FALSE;
    cinfo.out_color_space = JCS_RGB;
    jpeg_start_decompress(&cinfo);

    if (cinfo.output_components != 3) {
        sixel_helper_set_additional_message(
            "load_jpeg: unknown pixel format.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    *ppixelformat = SIXEL_PIXELFORMAT_RGB888;

    if (cinfo.output_width > INT_MAX || cinfo.output_height > INT_MAX) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto end;
    }
    *pwidth = (int)cinfo.output_width;
    *pheight = (int)cinfo.output_height;

    size = (size_t)(*pwidth * *pheight * cinfo.output_components);
    *result = (unsigned char *)sixel_allocator_malloc(allocator, size);
    if (*result == NULL) {
        sixel_helper_set_additional_message(
            "load_jpeg: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    row_stride = cinfo.output_width * (unsigned int)cinfo.output_components;
    buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);

    while (cinfo.output_scanline < cinfo.output_height) {
        jpeg_read_scanlines(&cinfo, buffer, 1);
        if (cinfo.err->num_warnings > 0) {
            sixel_helper_set_additional_message(
                "jpeg_read_scanlines: error/warining occuered.");
            status = SIXEL_BAD_INPUT;
            goto end;
        }
        memcpy(*result + (cinfo.output_scanline - 1) * row_stride, buffer[0], row_stride);
    }

    status = SIXEL_OK;

end:
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    return status;
}
# endif  /* HAVE_JPEG */


# if HAVE_LIBPNG
static void
read_png(png_structp png_ptr,
         png_bytep data,
         png_size_t length)
{
    sixel_chunk_t *pchunk = (sixel_chunk_t *)png_get_io_ptr(png_ptr);
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
    png_bytep trans = NULL;
    int num_trans = 0;
    int i;

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

jmp_buf jmpbuf;

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
        fprintf(stderr, "background color is specified [%02x, %02x, %02x]\n",
                bgcolor[0], bgcolor[1], bgcolor[2]);
#  endif
        background.red = bgcolor[0];
        background.green = bgcolor[1];
        background.blue = bgcolor[2];
        background.gray = (bgcolor[0] + bgcolor[1] + bgcolor[2]) / 3;
    } else if (png_get_bKGD(png_ptr, info_ptr, &default_background) == PNG_INFO_bKGD) {
        memcpy(&background, default_background, sizeof(background));
#  if HAVE_DEBUG
        fprintf(stderr, "background color is found [%02x, %02x, %02x]\n",
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
                *ppalette = (unsigned char *)sixel_allocator_malloc(allocator, (size_t)*pncolors * 3);
                if (*ppalette == NULL) {
                    sixel_helper_set_additional_message(
                        "load_png: sixel_allocator_malloc() failed.");
                    status = SIXEL_BAD_ALLOCATION;
                    goto cleanup;
                }
                read_palette(png_ptr, info_ptr, *ppalette,
                             *pncolors, png_palette, &background, transparent);
                *pixelformat = SIXEL_PIXELFORMAT_PAL1;
                break;
            case 2:
                *ppalette = (unsigned char *)sixel_allocator_malloc(allocator, (size_t)*pncolors * 3);
                if (*ppalette == NULL) {
                    sixel_helper_set_additional_message(
                        "load_png: sixel_allocator_malloc() failed.");
                    status = SIXEL_BAD_ALLOCATION;
                    goto cleanup;
                }
                read_palette(png_ptr, info_ptr, *ppalette,
                             *pncolors, png_palette, &background, transparent);
                *pixelformat = SIXEL_PIXELFORMAT_PAL2;
                break;
            case 4:
                *ppalette = (unsigned char *)sixel_allocator_malloc(allocator, (size_t)*pncolors * 3);
                if (*ppalette == NULL) {
                    sixel_helper_set_additional_message(
                        "load_png: sixel_allocator_malloc() failed.");
                    status = SIXEL_BAD_ALLOCATION;
                    goto cleanup;
                }
                read_palette(png_ptr, info_ptr, *ppalette,
                             *pncolors, png_palette, &background, transparent);
                *pixelformat = SIXEL_PIXELFORMAT_PAL4;
                break;
            case 8:
                *ppalette = (unsigned char *)sixel_allocator_malloc(allocator, (size_t)*pncolors * 3);
                if (*ppalette == NULL) {
                    sixel_helper_set_additional_message(
                        "load_png: sixel_allocator_malloc() failed.");
                    status = SIXEL_BAD_ALLOCATION;
                    goto cleanup;
                }
                read_palette(png_ptr, info_ptr, *ppalette,
                             *pncolors, png_palette, &background, transparent);
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
    *result = (unsigned char *)sixel_allocator_malloc(allocator, (size_t)(*psx * *psy * depth));
    if (*result == NULL) {
        sixel_helper_set_additional_message(
            "load_png: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto cleanup;
    }
    rows = (unsigned char **)sixel_allocator_malloc(allocator, (size_t)*psy * sizeof(unsigned char *));
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

# endif  /* HAVE_LIBPNG */


static SIXELSTATUS
load_sixel(unsigned char        /* out */ **result,
           unsigned char        /* in */  *buffer,
           int                  /* in */  size,
           int                  /* out */ *psx,
           int                  /* out */ *psy,
           unsigned char        /* out */ **ppalette,
           int                  /* out */ *pncolors,
           int                  /* in */  reqcolors,
           int                  /* out */ *ppixelformat,
           sixel_allocator_t    /* in */  *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    unsigned char *p = NULL;
    unsigned char *palette = NULL;
    int colors;
    int i;

    /* sixel */
    status = sixel_decode_raw(buffer, size,
                              &p, psx, psy,
                              &palette, &colors, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (ppalette == NULL || colors > reqcolors) {
        *ppixelformat = SIXEL_PIXELFORMAT_RGB888;
        *result = (unsigned char *)sixel_allocator_malloc(allocator, (size_t)(*psx * *psy * 3));
        if (*result == NULL) {
            sixel_helper_set_additional_message(
                "load_sixel: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        for (i = 0; i < *psx * *psy; ++i) {
            (*result)[i * 3 + 0] = palette[p[i] * 3 + 0];
            (*result)[i * 3 + 1] = palette[p[i] * 3 + 1];
            (*result)[i * 3 + 2] = palette[p[i] * 3 + 2];
        }
    } else {
        *ppixelformat = SIXEL_PIXELFORMAT_PAL8;
        *result = p;
        *ppalette = palette;
        *pncolors = colors;
        p = NULL;
        palette = NULL;
    }

end:
    sixel_allocator_free(allocator, palette);
    sixel_allocator_free(allocator, p);

    return status;
}


/* detect whether given chunk is sixel stream */
static int
chunk_is_sixel(sixel_chunk_t const *chunk)
{
    unsigned char *p;
    unsigned char *end;

    p = chunk->buffer;
    end = p + chunk->size;

    if (chunk->size < 3) {
        return 0;
    }

    p++;
    if (p >= end) {
        return 0;
    }
    if (*(p - 1) == 0x90 || (*(p - 1) == 0x1b && *p == 0x50)) {
        while (p++ < end) {
            if (*p == 0x71) {
                return 1;
            } else if (*p == 0x18 || *p == 0x1a) {
                return 0;
            } else if (*p < 0x20) {
                continue;
            } else if (*p < 0x30) {
                return 0;
            } else if (*p < 0x40) {
                continue;
            }
        }
    }
    return 0;
}


/* detect whether given chunk is PNM stream */
static int
chunk_is_pnm(sixel_chunk_t const *chunk)
{
    if (chunk->size < 2) {
        return 0;
    }
    if (chunk->buffer[0] == 'P' &&
        chunk->buffer[1] >= '1' &&
        chunk->buffer[1] <= '6') {
        return 1;
    }
    return 0;
}


#if HAVE_LIBPNG
/* detect whether given chunk is PNG stream */
static int
chunk_is_png(sixel_chunk_t const *chunk)
{
    if (chunk->size < 8) {
        return 0;
    }
    if (png_check_sig(chunk->buffer, 8)) {
        return 1;
    }
    return 0;
}
#endif  /* HAVE_LIBPNG */


/* detect whether given chunk is GIF stream */
static int
chunk_is_gif(sixel_chunk_t const *chunk)
{
    if (chunk->size < 6) {
        return 0;
    }
    if (chunk->buffer[0] == 'G' &&
        chunk->buffer[1] == 'I' &&
        chunk->buffer[2] == 'F' &&
        chunk->buffer[3] == '8' &&
        (chunk->buffer[4] == '7' || chunk->buffer[4] == '9') &&
        chunk->buffer[5] == 'a') {
        return 1;
    }
    return 0;
}


#if HAVE_JPEG
/* detect whether given chunk is JPEG stream */
static int
chunk_is_jpeg(sixel_chunk_t const *chunk)
{
    if (chunk->size < 2) {
        return 0;
    }
    if (memcmp("\xFF\xD8", chunk->buffer, 2) == 0) {
        return 1;
    }
    return 0;
}
#endif  /* HAVE_JPEG */

typedef union _fn_pointer {
    sixel_load_image_function fn;
    void *                    p;
} fn_pointer;

/* load images using builtin image loaders */
static SIXELSTATUS
load_with_builtin(
    sixel_chunk_t const       /* in */     *pchunk,      /* image data */
    int                       /* in */     fstatic,      /* static */
    int                       /* in */     fuse_palette, /* whether to use palette if possible */
    int                       /* in */     reqcolors,    /* reqcolors */
    unsigned char             /* in */     *bgcolor,     /* background color */
    int                       /* in */     loop_control, /* one of enum loop_control */
    sixel_load_image_function /* in */     fn_load,      /* callback */
    void                      /* in/out */ *context      /* private data for callback */
)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_frame_t *frame = NULL;
    char message[256];
    int nwrite;
    fn_pointer fnp;

    if (chunk_is_sixel(pchunk)) {
        status = sixel_frame_new(&frame, pchunk->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        if (pchunk->size > INT_MAX) {
            status = SIXEL_BAD_INTEGER_OVERFLOW;
            goto end;
        }
        status = load_sixel(&frame->pixels,
                            pchunk->buffer,
                            (int)pchunk->size,
                            &frame->width,
                            &frame->height,
                            fuse_palette ? &frame->palette: NULL,
                            &frame->ncolors,
                            reqcolors,
                            &frame->pixelformat,
                            pchunk->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    } else if (chunk_is_pnm(pchunk)) {
        status = sixel_frame_new(&frame, pchunk->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        if (pchunk->size > INT_MAX) {
            status = SIXEL_BAD_INTEGER_OVERFLOW;
            goto end;
        }
        /* pnm */
        status = load_pnm(pchunk->buffer,
                          (int)pchunk->size,
                          frame->allocator,
                          &frame->pixels,
                          &frame->width,
                          &frame->height,
                          fuse_palette ? &frame->palette: NULL,
                          &frame->ncolors,
                          &frame->pixelformat);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }
#if HAVE_JPEG
    else if (chunk_is_jpeg(pchunk)) {
        status = sixel_frame_new(&frame, pchunk->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        status = load_jpeg(&frame->pixels,
                           pchunk->buffer,
                           pchunk->size,
                           &frame->width,
                           &frame->height,
                           &frame->pixelformat,
                           pchunk->allocator);

        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }
#endif  /* HAVE_JPEG */
#if HAVE_LIBPNG
    else if (chunk_is_png(pchunk)) {
        status = sixel_frame_new(&frame, pchunk->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        status = load_png(&frame->pixels,
                          pchunk->buffer,
                          pchunk->size,
                          &frame->width,
                          &frame->height,
                          fuse_palette ? &frame->palette: NULL,
                          &frame->ncolors,
                          reqcolors,
                          &frame->pixelformat,
                          bgcolor,
                          &frame->transparent,
                          pchunk->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }
#endif  /* HAVE_LIBPNG */
    else if (chunk_is_gif(pchunk)) {
        fnp.fn = fn_load;
        if (pchunk->size > INT_MAX) {
            status = SIXEL_BAD_INTEGER_OVERFLOW;
            goto end;
        }
        status = load_gif(pchunk->buffer,
                          (int)pchunk->size,
                          bgcolor,
                          reqcolors,
                          fuse_palette,
                          fstatic,
                          loop_control,
                          fnp.p,
                          context,
                          pchunk->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        goto end;
    } else {
        stbi__context s;
        int depth;

        status = sixel_frame_new(&frame, pchunk->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        stbi_allocator = pchunk->allocator;
        if (pchunk->size > INT_MAX) {
            status = SIXEL_BAD_INTEGER_OVERFLOW;
            goto end;
        }
        stbi__start_mem(&s, pchunk->buffer, (int)pchunk->size);
        frame->pixels = stbi__load_and_postprocess_8bit(&s, &frame->width, &frame->height, &depth, 3);
        if (!frame->pixels) {
            sixel_helper_set_additional_message(stbi_failure_reason());
            status = SIXEL_STBI_ERROR;
            goto end;
        }
        frame->loop_count = 1;

        switch (depth) {
        case 1:
        case 3:
        case 4:
            frame->pixelformat = SIXEL_PIXELFORMAT_RGB888;
            break;
        default:
            nwrite = sprintf(message,
                             "load_with_builtin() failed.\n"
                             "reason: unknown pixel-format.(depth: %d)\n",
                             depth);
            if (nwrite > 0) {
                sixel_helper_set_additional_message(message);
            }
            goto end;
        }
    }

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


#ifdef HAVE_GDK_PIXBUF2
/*
 * Loader backed by gdk-pixbuf2. The entire animation is consumed via
 * GdkPixbufLoader, each frame is copied into a temporary buffer and forwarded as
 * a sixel_frame_t. Loop attributes provided by gdk-pixbuf are reconciled with
 * libsixel's loop control settings.
 */
static SIXELSTATUS
load_with_gdkpixbuf(
    sixel_chunk_t const       /* in */     *pchunk,      /* image data */
    int                       /* in */     fstatic,      /* static */
    int                       /* in */     fuse_palette, /* whether to use palette if possible */
    int                       /* in */     reqcolors,    /* reqcolors */
    unsigned char             /* in */     *bgcolor,     /* background color */
    int                       /* in */     loop_control, /* one of enum loop_control */
    sixel_load_image_function /* in */     fn_load,      /* callback */
    void                      /* in/out */ *context      /* private data for callback */
)
{
    SIXELSTATUS status = SIXEL_FALSE;
    GdkPixbuf *pixbuf;
    GdkPixbufAnimation *animation;
    GdkPixbufLoader *loader = NULL;
    GdkPixbufAnimationIter *it = NULL;
    gboolean loader_closed = FALSE;  /* remember if loader was already closed */
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
    int i;
    int depth;
    int anim_loop_count = (-1);  /* (-1): infinite, >=0: finite loop count */

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
    /* feed the whole blob and close so the animation metadata becomes available */
    if (! gdk_pixbuf_loader_write(loader, pchunk->buffer, pchunk->size, NULL)) {
        status = SIXEL_GDK_ERROR;
        goto end;
    }
    if (! gdk_pixbuf_loader_close(loader, NULL)) {
        status = SIXEL_GDK_ERROR;
        goto end;
    }
    loader_closed = TRUE;
    animation = gdk_pixbuf_loader_get_animation(loader);
    if (animation) {
        /* inspect animation object to determine built-in loop semantics */
        if (GDK_IS_PIXBUF_SIMPLE_ANIM(animation)) {
            anim_loop_count = gdk_pixbuf_simple_anim_get_loop(
                                 GDK_PIXBUF_SIMPLE_ANIM(animation)) ? (-1) : 1;
        } else {
            GParamSpec *loop_pspec = g_object_class_find_property(
                G_OBJECT_GET_CLASS(animation), "loop");
            if (loop_pspec == NULL) {
                loop_pspec = g_object_class_find_property(
                    G_OBJECT_GET_CLASS(animation), "loop-count");
            }
            if (loop_pspec) {
                GValue loop_value = G_VALUE_INIT;
                g_value_init(&loop_value, loop_pspec->value_type);
                g_object_get_property(G_OBJECT(animation),
                                      g_param_spec_get_name(loop_pspec),
                                      &loop_value);
                if (G_VALUE_HOLDS_BOOLEAN(&loop_value)) {
                    /* TRUE means "loop forever" for these properties */
                    anim_loop_count = g_value_get_boolean(&loop_value) ? (-1) : 1;
                } else if (G_VALUE_HOLDS_INT(&loop_value)) {
                    int loop_int = g_value_get_int(&loop_value);
                    /* GIF spec treats zero as infinite repetition */
                    anim_loop_count = (loop_int <= 0) ? (-1) : loop_int;
                } else if (G_VALUE_HOLDS_UINT(&loop_value)) {
                    guint loop_uint = g_value_get_uint(&loop_value);
                    if (loop_uint == 0U) {
                        anim_loop_count = (-1);
                    } else {
                        anim_loop_count = loop_uint > (guint)INT_MAX
                                            ? INT_MAX
                                            : (int)loop_uint;
                    }
                }
                g_value_unset(&loop_value);
            }
        }
    }
    if (!animation || fstatic || gdk_pixbuf_animation_is_static_image(animation)) {
        /* fall back to single frame decoding */
        pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
        if (pixbuf == NULL) {
            goto end;
        }
        frame->frame_no = 0;
        frame->width = gdk_pixbuf_get_width(pixbuf);
        frame->height = gdk_pixbuf_get_height(pixbuf);
        stride = gdk_pixbuf_get_rowstride(pixbuf);
        frame->pixels = sixel_allocator_malloc(pchunk->allocator, (size_t)(frame->height * stride));
        if (frame->pixels == NULL) {
            sixel_helper_set_additional_message(
                "load_with_gdkpixbuf: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        if (stride / frame->width == 4) {
            frame->pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
            depth = 4;
        } else {
            frame->pixelformat = SIXEL_PIXELFORMAT_RGB888;
            depth = 3;
        }
        p = gdk_pixbuf_get_pixels(pixbuf);
        if (stride == frame->width * depth) {
            memcpy(frame->pixels, gdk_pixbuf_get_pixels(pixbuf),
                   (size_t)(frame->height * stride));
        } else {
            for (i = 0; i < frame->height; ++i) {
                memcpy(frame->pixels + frame->width * depth * i,
                       p + stride * i,
                       (size_t)(frame->width * depth));
            }
        }
        status = fn_load(frame, context);
        if (status != SIXEL_OK) {
            goto end;
        }
        /* scratch buffer no longer needed after callback */
        sixel_allocator_free(pchunk->allocator, frame->pixels);
        frame->pixels = NULL;
    } else {
        gboolean finished;

        /* reset iterator to the beginning of the timeline */
        time_val = start_time;
        frame->frame_no = 0;
        frame->loop_count = 0;

        it = gdk_pixbuf_animation_get_iter(animation, &time_val);
        if (it == NULL) {
            status = SIXEL_GDK_ERROR;
            goto end;
        }

        for (;;) {
            /* handle one logical loop of the animation */
            finished = FALSE;
            while (!gdk_pixbuf_animation_iter_on_currently_loading_frame(it)) {
                int delay_ms;

                pixbuf = gdk_pixbuf_animation_iter_get_pixbuf(it);
                if (pixbuf == NULL) {
                    finished = TRUE;
                    break;
                }
                /* allocate a scratch copy of the current frame */
                frame->width = gdk_pixbuf_get_width(pixbuf);
                frame->height = gdk_pixbuf_get_height(pixbuf);
                stride = gdk_pixbuf_get_rowstride(pixbuf);
                frame->pixels = sixel_allocator_malloc(
                    pchunk->allocator,
                    (size_t)(frame->height * stride));
                if (frame->pixels == NULL) {
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
                    memcpy(frame->pixels, p,
                           (size_t)(frame->height * stride));
                } else {
                    for (i = 0; i < frame->height; ++i) {
                        memcpy(frame->pixels + frame->width * depth * i,
                               p + stride * i,
                               (size_t)(frame->width * depth));
                    }
                }
                delay_ms = gdk_pixbuf_animation_iter_get_delay_time(it);
                if (delay_ms < 0) {
                    delay_ms = 0;
                }
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif  /* HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS */
                /* advance the synthetic clock before asking gdk to move forward */
                g_time_val_add(&time_val, delay_ms * 1000);
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
# pragma GCC diagnostic pop
#endif  /* HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS */
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
                sixel_allocator_free(pchunk->allocator, frame->pixels);
                frame->pixels = NULL;
                frame->frame_no++;

                if (finished) {
                    break;
                }
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

#ifdef HAVE_COREGRAPHICS
static SIXELSTATUS
load_with_coregraphics(
    sixel_chunk_t const       /* in */     *pchunk,
    int                       /* in */     fstatic,
    int                       /* in */     fuse_palette,
    int                       /* in */     reqcolors,
    unsigned char             /* in */     *bgcolor,
    int                       /* in */     loop_control,
    sixel_load_image_function /* in */     fn_load,
    void                      /* in/out */ *context)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_frame_t *frame = NULL;
    CFDataRef data = NULL;
    CGImageSourceRef source = NULL;
    CGImageRef image = NULL;
    CGColorSpaceRef color_space = NULL;
    CGContextRef ctx = NULL;
    size_t stride;
    size_t frame_count;
    int anim_loop_count = (-1);
    CFDictionaryRef props = NULL;
    CFDictionaryRef anim_dict;
    CFNumberRef loop_num;
    CFDictionaryRef frame_props;
    CFDictionaryRef frame_anim_dict;
    CFNumberRef delay_num;
    double delay_sec;
    size_t i;

    (void) fuse_palette;
    (void) reqcolors;
    (void) bgcolor;

    status = sixel_frame_new(&frame, pchunk->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    data = CFDataCreate(kCFAllocatorDefault,
                        pchunk->buffer,
                        (CFIndex)pchunk->size);
    if (! data) {
        status = SIXEL_FALSE;
        goto end;
    }

    source = CGImageSourceCreateWithData(data, NULL);
    if (! source) {
        status = SIXEL_FALSE;
        goto end;
    }

    frame_count = CGImageSourceGetCount(source);
    if (! frame_count) {
        status = SIXEL_FALSE;
        goto end;
    }
    if (fstatic) {
        frame_count = 1;
    }

    props = CGImageSourceCopyProperties(source, NULL);
    if (props) {
        anim_dict = (CFDictionaryRef)CFDictionaryGetValue(
            props, kCGImagePropertyGIFDictionary);
        if (anim_dict) {
            loop_num = (CFNumberRef)CFDictionaryGetValue(
                anim_dict, kCGImagePropertyGIFLoopCount);
            if (loop_num) {
                CFNumberGetValue(loop_num, kCFNumberIntType, &anim_loop_count);
            }
        }
        CFRelease(props);
    }

    color_space = CGColorSpaceCreateDeviceRGB();
    if (! color_space) {
        status = SIXEL_FALSE;
        goto end;
    }

    frame->loop_count = 0;

    for (;;) {
        frame->frame_no = 0;
        for (i = 0; i < frame_count; ++i) {
            delay_sec = 0.0;
            frame_props = CGImageSourceCopyPropertiesAtIndex(
                source, (CFIndex)i, NULL);
            if (frame_props) {
                frame_anim_dict = (CFDictionaryRef)CFDictionaryGetValue(
                    frame_props, kCGImagePropertyGIFDictionary);
                if (frame_anim_dict) {
                    delay_num = (CFNumberRef)CFDictionaryGetValue(
                        frame_anim_dict, kCGImagePropertyGIFUnclampedDelayTime);
                    if (! delay_num) {
                        delay_num = (CFNumberRef)CFDictionaryGetValue(
                            frame_anim_dict, kCGImagePropertyGIFDelayTime);
                    }
                    if (delay_num) {
                        CFNumberGetValue(delay_num,
                                         kCFNumberDoubleType,
                                         &delay_sec);
                    }
                }
                CFRelease(frame_props);
            }
            frame->delay = (int)(delay_sec * 100);

            image = CGImageSourceCreateImageAtIndex(source, (CFIndex)i, NULL);
            if (! image) {
                status = SIXEL_FALSE;
                goto end;
            }

            frame->width = (int)CGImageGetWidth(image);
            frame->height = (int)CGImageGetHeight(image);
            frame->pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
            stride = (size_t)frame->width * 4;
            frame->pixels = sixel_allocator_malloc(
                pchunk->allocator, (size_t)(frame->height * stride));

            if (frame->pixels == NULL) {
                sixel_helper_set_additional_message(
                    "load_with_coregraphics: sixel_allocator_malloc() failed.");
                status = SIXEL_BAD_ALLOCATION;
                CGImageRelease(image);
                goto end;
            }

            ctx = CGBitmapContextCreate(frame->pixels,
                                        frame->width,
                                        frame->height,
                                        8,
                                        stride,
                                        color_space,
                                        kCGImageAlphaPremultipliedLast |
                                            kCGBitmapByteOrder32Big);
            if (!ctx) {
                CGImageRelease(image);
                goto end;
            }

            CGContextDrawImage(ctx,
                               CGRectMake(0, 0, frame->width, frame->height),
                               image);
            CGContextRelease(ctx);
            ctx = NULL;

            frame->multiframe = (frame_count > 1);
            status = fn_load(frame, context);
            sixel_allocator_free(pchunk->allocator, frame->pixels);
            frame->pixels = NULL;
            CGImageRelease(image);
            image = NULL;
            if (status != SIXEL_OK) {
                goto end;
            }
            ++frame->frame_no;
        }

        ++frame->loop_count;

        if (frame_count <= 1) {
            break;
        }
        if (loop_control == SIXEL_LOOP_DISABLE) {
            break;
        }
        if (loop_control == SIXEL_LOOP_AUTO) {
            if (anim_loop_count < 0) {
                break;
            }
            if (anim_loop_count > 0 && frame->loop_count >= anim_loop_count) {
                break;
            }
            continue;
        }
    }

    status = SIXEL_OK;

end:
    if (ctx) {
        CGContextRelease(ctx);
    }
    if (color_space) {
        CGColorSpaceRelease(color_space);
    }
    if (image) {
        CGImageRelease(image);
    }
    if (source) {
        CFRelease(source);
    }
    if (data) {
        CFRelease(data);
    }
    if (frame) {
        sixel_allocator_free(pchunk->allocator, frame->pixels);
        sixel_allocator_free(pchunk->allocator, frame->palette);
        sixel_allocator_free(pchunk->allocator, frame);
    }
    return status;
}
#endif  /* HAVE_COREGRAPHICS */

#if HAVE_GD
static int
detect_file_format(int len, unsigned char *data)
{
    if (len > 18 && memcmp("TRUEVISION", data + len - 18, 10) == 0) {
        return SIXEL_FORMAT_TGA;
    }

    if (len > 3 && memcmp("GIF", data, 3) == 0) {
        return SIXEL_FORMAT_GIF;
    }

    if (len > 8 && memcmp("\x89\x50\x4E\x47\x0D\x0A\x1A\x0A", data, 8) == 0) {
        return SIXEL_FORMAT_PNG;
    }

    if (len > 2 && memcmp("BM", data, 2) == 0) {
        return SIXEL_FORMAT_BMP;
    }

    if (len > 2 && memcmp("\xFF\xD8", data, 2) == 0) {
        return SIXEL_FORMAT_JPG;
    }

    if (len > 2 && memcmp("\x00\x00", data, 2) == 0) {
        return SIXEL_FORMAT_WBMP;
    }

    if (len > 2 && memcmp("\x4D\x4D", data, 2) == 0) {
        return SIXEL_FORMAT_TIFF;
    }

    if (len > 2 && memcmp("\x49\x49", data, 2) == 0) {
        return SIXEL_FORMAT_TIFF;
    }

    if (len > 2 && memcmp("\033P", data, 2) == 0) {
        return SIXEL_FORMAT_SIXEL;
    }

    if (len > 2 && data[0] == 0x90  && (data[len - 1] == 0x9C || data[len - 2] == 0x9C)) {
        return SIXEL_FORMAT_SIXEL;
    }

    if (len > 1 && data[0] == 'P' && data[1] >= '1' && data[1] <= '6') {
        return SIXEL_FORMAT_PNM;
    }

    if (len > 3 && memcmp("gd2", data, 3) == 0) {
        return SIXEL_FORMAT_GD2;
    }

    if (len > 4 && memcmp("8BPS", data, 4) == 0) {
        return SIXEL_FORMAT_PSD;
    }

    if (len > 11 && memcmp("#?RADIANCE\n", data, 11) == 0) {
        return SIXEL_FORMAT_HDR;
    }

    return (-1);
}
#endif /* HAVE_GD */

#if HAVE_GD

static SIXELSTATUS
load_with_gd(
    sixel_chunk_t const       /* in */     *pchunk,      /* image data */
    int                       /* in */     fstatic,      /* static */
    int                       /* in */     fuse_palette, /* whether to use palette if possible */
    int                       /* in */     reqcolors,    /* reqcolors */
    unsigned char             /* in */     *bgcolor,     /* background color */
    int                       /* in */     loop_control, /* one of enum loop_control */
    sixel_load_image_function /* in */     fn_load,      /* callback */
    void                      /* in/out */ *context      /* private data for callback */
)
{
    SIXELSTATUS status = SIXEL_FALSE;
    unsigned char *p;
    gdImagePtr im = NULL;
    int x, y;
    int c;
    sixel_frame_t *frame = NULL;
    int format;

    (void) fstatic;
    (void) fuse_palette;
    (void) reqcolors;
    (void) bgcolor;
    (void) loop_control;

    format = detect_file_format(pchunk->size, pchunk->buffer);

    if (format == SIXEL_FORMAT_GIF) {
#if HAVE_DECL_GDIMAGECREATEFROMGIFANIMPTR
        gdImagePtr *ims = NULL;
        int frames = 0;
        int i;
        int *delays = NULL;

        ims = gdImageCreateFromGifAnimPtr(pchunk->size, pchunk->buffer,
                                          &frames, &delays);
        if (ims == NULL) {
            status = SIXEL_GD_ERROR;
            goto end;
        }

        for (i = 0; i < frames; i++) {
            im = ims[i];
            if (!gdImageTrueColor(im)) {
# if HAVE_DECL_GDIMAGEPALETTETOTRUECOLOR
                if (!gdImagePaletteToTrueColor(im)) {
                    status = SIXEL_GD_ERROR;
                    goto gif_end;
                }
# else
                status = SIXEL_GD_ERROR;
                goto gif_end;
# endif
            }

            status = sixel_frame_new(&frame, pchunk->allocator);
            if (SIXEL_FAILED(status)) {
                frame = NULL;
                goto gif_end;
            }

            frame->width = gdImageSX(im);
            frame->height = gdImageSY(im);
            frame->pixelformat = SIXEL_PIXELFORMAT_RGB888;
            p = frame->pixels = sixel_allocator_malloc(
                pchunk->allocator,
                (size_t)(frame->width * frame->height * 3));
            if (frame->pixels == NULL) {
                sixel_helper_set_additional_message(
                    "load_with_gd: sixel_allocator_malloc() failed.");
                status = SIXEL_BAD_ALLOCATION;
                sixel_frame_unref(frame);
                frame = NULL;
                goto gif_end;
            }
            for (y = 0; y < frame->height; y++) {
                for (x = 0; x < frame->width; x++) {
                    c = gdImageTrueColorPixel(im, x, y);
                    *p++ = gdTrueColorGetRed(c);
                    *p++ = gdTrueColorGetGreen(c);
                    *p++ = gdTrueColorGetBlue(c);
                }
            }

            if (delays) {
                frame->delay.tv_sec = delays[i] / 100;
                frame->delay.tv_nsec = (delays[i] % 100) * 10000000L;
            }

            status = fn_load(frame, context);
            sixel_frame_unref(frame);
            frame = NULL;
            gdImageDestroy(im);
            ims[i] = NULL;
            if (SIXEL_FAILED(status)) {
                goto gif_end;
            }
        }

        status = SIXEL_OK;

gif_end:
        if (delays) {
            gdFree(delays);
        }
        if (ims) {
            for (i = 0; i < frames; i++) {
                if (ims[i]) {
                    gdImageDestroy(ims[i]);
                }
            }
            gdFree(ims);
        }
        goto end;
#else
        status = SIXEL_GD_ERROR;
        goto end;
#endif
    }

    switch (format) {
#if HAVE_DECL_GDIMAGECREATEFROMPNGPTR
        case SIXEL_FORMAT_PNG:
            im = gdImageCreateFromPngPtr(pchunk->size, pchunk->buffer);
            break;
#endif  /* HAVE_DECL_GDIMAGECREATEFROMPNGPTR */
#if HAVE_DECL_GDIMAGECREATEFROMBMPPTR
        case SIXEL_FORMAT_BMP:
            im = gdImageCreateFromBmpPtr(pchunk->size, pchunk->buffer);
            break;
#endif  /* HAVE_DECL_GDIMAGECREATEFROMBMPPTR */
        case SIXEL_FORMAT_JPG:
#if HAVE_DECL_GDIMAGECREATEFROMJPEGPTREX
            im = gdImageCreateFromJpegPtrEx(pchunk->size, pchunk->buffer, 1);
#elif HAVE_DECL_GDIMAGECREATEFROMJPEGPTR
            im = gdImageCreateFromJpegPtr(pchunk->size, pchunk->buffer);
#endif  /* HAVE_DECL_GDIMAGECREATEFROMJPEGPTREX */
            break;
#if HAVE_DECL_GDIMAGECREATEFROMTGAPTR
        case SIXEL_FORMAT_TGA:
            im = gdImageCreateFromTgaPtr(pchunk->size, pchunk->buffer);
            break;
#endif  /* HAVE_DECL_GDIMAGECREATEFROMTGAPTR */
#if HAVE_DECL_GDIMAGECREATEFROMWBMPPTR
        case SIXEL_FORMAT_WBMP:
            im = gdImageCreateFromWBMPPtr(pchunk->size, pchunk->buffer);
            break;
#endif  /* HAVE_DECL_GDIMAGECREATEFROMWBMPPTR */
#if HAVE_DECL_GDIMAGECREATEFROMTIFFPTR
        case SIXEL_FORMAT_TIFF:
            im = gdImageCreateFromTiffPtr(pchunk->size, pchunk->buffer);
            break;
#endif  /* HAVE_DECL_GDIMAGECREATEFROMTIFFPTR */
#if HAVE_DECL_GDIMAGECREATEFROMGD2PTR
        case SIXEL_FORMAT_GD2:
            im = gdImageCreateFromGd2Ptr(pchunk->size, pchunk->buffer);
            break;
#endif  /* HAVE_DECL_GDIMAGECREATEFROMGD2PTR */
        default:
            status = SIXEL_GD_ERROR;
            sixel_helper_set_additional_message(
                "unexpected image format detected.");
            goto end;
    }

    if (im == NULL) {
        status = SIXEL_GD_ERROR;
        /* TODO: retrieve error detail */
        goto end;
    }

    if (!gdImageTrueColor(im)) {
#if HAVE_DECL_GDIMAGEPALETTETOTRUECOLOR
        if (!gdImagePaletteToTrueColor(im)) {
            gdImageDestroy(im);
            status = SIXEL_GD_ERROR;
            /* TODO: retrieve error detail */
            goto end;
        }
#else
        status = SIXEL_GD_ERROR;
        /* TODO: retrieve error detail */
        goto end;
#endif
    }

    status = sixel_frame_new(&frame, pchunk->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    frame->width = gdImageSX(im);
    frame->height = gdImageSY(im);
    frame->pixelformat = SIXEL_PIXELFORMAT_RGB888;
    p = frame->pixels = sixel_allocator_malloc(
        pchunk->allocator, (size_t)(frame->width * frame->height * 3));
    if (frame->pixels == NULL) {
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

#if HAVE_WIC

#include <windows.h>
#include <wincodec.h>

SIXELSTATUS
load_with_wic(
    sixel_chunk_t const       /* in */     *pchunk,      /* image data */
    int                       /* in */     fstatic,      /* static */
    int                       /* in */     fuse_palette, /* whether to use palette if possible */
    int                       /* in */     reqcolors,    /* reqcolors */
    unsigned char             /* in */     *bgcolor,     /* background color */
    int                       /* in */     loop_control, /* one of enum loop_control */
    sixel_load_image_function /* in */     fn_load,      /* callback */
    void                      /* in/out */ *context      /* private data for callback */
)
{
    HRESULT                 hr         = E_FAIL;
    SIXELSTATUS             status     = SIXEL_FALSE;
    IWICImagingFactory     *factory    = NULL;
    IWICStream             *stream     = NULL;
    IWICBitmapDecoder      *decoder    = NULL;
    IWICBitmapFrameDecode  *wicframe   = NULL;
    IWICFormatConverter    *conv       = NULL;
    IWICBitmapSource       *src        = NULL;
    IWICPalette            *wicpalette = NULL;
    WICColor               *wiccolors  = NULL;
    IWICMetadataQueryReader *qdecoder  = NULL;
    IWICMetadataQueryReader *qframe    = NULL;
    UINT                    ncolors    = 0;
    sixel_frame_t          *frame      = NULL;
    int                     comp       = 4;
    UINT                    actual     = 0;
    UINT                    i;
    UINT                    frame_count = 0;
    int                     anim_loop_count = (-1);
    int                     is_gif;
    WICColor                c;

    (void) reqcolors;
    (void) bgcolor;

    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        return status;
    }

    status = sixel_frame_new(&frame, pchunk->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IWICImagingFactory, (void**)&factory);
    if (FAILED(hr)) {
        goto end;
    }

    hr = factory->lpVtbl->CreateStream(factory, &stream);
    if (FAILED(hr)) {
        goto end;
    }

    hr = stream->lpVtbl->InitializeFromMemory(stream,
                                              (BYTE*)pchunk->buffer,
                                              (DWORD)pchunk->size);
    if (FAILED(hr)) {
        goto end;
    }

    hr = factory->lpVtbl->CreateDecoderFromStream(factory,
                                                  (IStream*)stream,
                                                  NULL,
                                                  WICDecodeMetadataCacheOnDemand,
                                                  &decoder);
    if (FAILED(hr)) {
        goto end;
    }

    is_gif = (memcmp("GIF", pchunk->buffer, 3) == 0);

    if (is_gif) {
        hr = decoder->lpVtbl->GetFrameCount(decoder, &frame_count);
        if (FAILED(hr)) {
            goto end;
        }
        if (fstatic) {
            frame_count = 1;
        }

        hr = decoder->lpVtbl->GetMetadataQueryReader(decoder, &qdecoder);
        if (SUCCEEDED(hr)) {
            PROPVARIANT pv;
            PropVariantInit(&pv);
            hr = qdecoder->lpVtbl->GetMetadataByName(
                qdecoder,
                L"/appext/Application/NETSCAPE2.0/Loop",
                &pv);
            if (SUCCEEDED(hr) && pv.vt == VT_UI2) {
                anim_loop_count = pv.uiVal;
            }
            PropVariantClear(&pv);
            qdecoder->lpVtbl->Release(qdecoder);
            qdecoder = NULL;
        }

        frame->loop_count = 0;
        for (;;) {
            frame->frame_no = 0;
            for (i = 0; i < frame_count; ++i) {
                hr = decoder->lpVtbl->GetFrame(decoder, i, &wicframe);
                if (FAILED(hr)) {
                    goto end;
                }

                hr = factory->lpVtbl->CreateFormatConverter(factory, &conv);
                if (FAILED(hr)) {
                    goto end;
                }
                hr = conv->lpVtbl->Initialize(conv,
                                              (IWICBitmapSource*)wicframe,
                                              &GUID_WICPixelFormat32bppRGBA,
                                              WICBitmapDitherTypeNone,
                                              NULL,
                                              0.0,
                                              WICBitmapPaletteTypeCustom);
                if (FAILED(hr)) {
                    goto end;
                }

                src = (IWICBitmapSource*)conv;
                comp = 4;
                frame->pixelformat = SIXEL_PIXELFORMAT_RGBA8888;

                hr = src->lpVtbl->GetSize(
                    src,
                    (UINT *)&frame->width,
                    (UINT *)&frame->height);
                if (FAILED(hr)) {
                    goto end;
                }

                if (frame->width <= 0 || frame->height <= 0 ||
                    frame->width > SIXEL_WIDTH_LIMIT ||
                    frame->height > SIXEL_HEIGHT_LIMIT) {
                    sixel_helper_set_additional_message(
                        "load_with_wic: an invalid width or height parameter detected.");
                    status = SIXEL_BAD_INPUT;
                    goto end;
                }

                frame->pixels = sixel_allocator_malloc(
                    pchunk->allocator,
                    (size_t)(frame->height * frame->width * comp));
                if (frame->pixels == NULL) {
                    hr = E_OUTOFMEMORY;
                    goto end;
                }

                {
                    WICRect rc = { 0, 0, (INT)frame->width, (INT)frame->height };
                    hr = src->lpVtbl->CopyPixels(
                        src,
                        &rc,
                        frame->width * comp,
                        (UINT)frame->width * frame->height * comp,
                        frame->pixels);
                    if (FAILED(hr)) {
                        goto end;
                    }
                }

                frame->delay = 0;
                hr = wicframe->lpVtbl->GetMetadataQueryReader(wicframe, &qframe);
                if (SUCCEEDED(hr)) {
                    PROPVARIANT pv;
                    PropVariantInit(&pv);
                    hr = qframe->lpVtbl->GetMetadataByName(
                        qframe,
                        L"/grctlext/Delay",
                        &pv);
                    if (SUCCEEDED(hr) && pv.vt == VT_UI2) {
                        frame->delay = (int)(pv.uiVal) * 10;
                    }
                    PropVariantClear(&pv);
                    qframe->lpVtbl->Release(qframe);
                    qframe = NULL;
                }

                frame->multiframe = 1;
                status = fn_load(frame, context);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
                frame->pixels = NULL;
                frame->palette = NULL;

                if (conv) {
                    conv->lpVtbl->Release(conv);
                    conv = NULL;
                }
                if (wicframe) {
                    wicframe->lpVtbl->Release(wicframe);
                    wicframe = NULL;
                }

                frame->frame_no++;
            }

            ++frame->loop_count;

            if (anim_loop_count < 0) {
                break;
            }
            if (loop_control == SIXEL_LOOP_DISABLE || frame->frame_no == 1) {
                break;
            }
            if (loop_control == SIXEL_LOOP_AUTO &&
                frame->loop_count == anim_loop_count) {
                break;
            }
        }

        status = SIXEL_OK;
        goto end;
    }

    hr = decoder->lpVtbl->GetFrame(decoder, 0, &wicframe);
    if (FAILED(hr)) {
        goto end;
    }

    if (fuse_palette) {
        hr = factory->lpVtbl->CreatePalette(factory, &wicpalette);
        if (SUCCEEDED(hr)) {
            hr = wicframe->lpVtbl->CopyPalette(wicframe, wicpalette);
        }
        if (SUCCEEDED(hr)) {
            hr = wicpalette->lpVtbl->GetColorCount(wicpalette, &ncolors);
        }
        if (SUCCEEDED(hr) && ncolors > 0 && ncolors <= 256) {
            hr = factory->lpVtbl->CreateFormatConverter(factory, &conv);
            if (SUCCEEDED(hr)) {
                hr = conv->lpVtbl->Initialize(conv,
                                              (IWICBitmapSource*)wicframe,
                                              &GUID_WICPixelFormat8bppIndexed,
                                              WICBitmapDitherTypeNone,
                                              wicpalette,
                                              0.0,
                                              WICBitmapPaletteTypeCustom);
            }
            if (SUCCEEDED(hr)) {
                src = (IWICBitmapSource*)conv;
                comp = 1;
                frame->pixelformat = SIXEL_PIXELFORMAT_PAL8;
                frame->palette = sixel_allocator_malloc(
                    pchunk->allocator,
                    (size_t)ncolors * 3);
                if (frame->palette == NULL) {
                    hr = E_OUTOFMEMORY;
                } else {
                    wiccolors = (WICColor *)sixel_allocator_malloc(
                        pchunk->allocator,
                        (size_t)ncolors * sizeof(WICColor));
                    if (wiccolors == NULL) {
                        hr = E_OUTOFMEMORY;
                    } else {
                        actual = 0;
                        hr = wicpalette->lpVtbl->GetColors(
                            wicpalette, ncolors, wiccolors, &actual);
                        if (SUCCEEDED(hr) && actual == ncolors) {
                            for (i = 0; i < ncolors; ++i) {
                                c = wiccolors[i];
                                frame->palette[i * 3 + 0] = (unsigned char)((c >> 16) & 0xFF);
                                frame->palette[i * 3 + 1] = (unsigned char)((c >> 8) & 0xFF);
                                frame->palette[i * 3 + 2] = (unsigned char)(c & 0xFF);
                            }
                            frame->ncolors = (int)ncolors;
                        } else {
                            hr = E_FAIL;
                        }
                    }
                }
            }
            if (FAILED(hr)) {
                if (conv) {
                    conv->lpVtbl->Release(conv);
                    conv = NULL;
                }
                sixel_allocator_free(pchunk->allocator, frame->palette);
                frame->palette = NULL;
                sixel_allocator_free(pchunk->allocator, wiccolors);
                wiccolors = NULL;
                src = NULL;
            }
        }
    }

    if (src == NULL) {
        hr = factory->lpVtbl->CreateFormatConverter(factory, &conv);
        if (FAILED(hr)) {
            goto end;
        }

        hr = conv->lpVtbl->Initialize(conv, (IWICBitmapSource*)wicframe,
                                      &GUID_WICPixelFormat32bppRGBA,
                                      WICBitmapDitherTypeNone, NULL, 0.0,
                                      WICBitmapPaletteTypeCustom);
        if (FAILED(hr)) {
            goto end;
        }

        src = (IWICBitmapSource*)conv;
        comp = 4;
        frame->pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
    }

    hr = src->lpVtbl->GetSize(
        src, (UINT *)&frame->width, (UINT *)&frame->height);
    if (FAILED(hr)) {
        goto end;
    }

    /* check size */
    if (frame->width <= 0) {
        sixel_helper_set_additional_message(
            "load_with_wic: an invalid width parameter detected.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (frame->height <= 0) {
        sixel_helper_set_additional_message(
            "load_with_wic: an invalid width parameter detected.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (frame->width > SIXEL_WIDTH_LIMIT) {
        sixel_helper_set_additional_message(
            "load_with_wic: given width parameter is too huge.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (frame->height > SIXEL_HEIGHT_LIMIT) {
        sixel_helper_set_additional_message(
            "load_with_wic: given height parameter is too huge.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    frame->pixels = sixel_allocator_malloc(
        pchunk->allocator,
        (size_t)(frame->height * frame->width * comp));

    {
        WICRect rc = { 0, 0, (INT)frame->width, (INT)frame->height };
        hr = src->lpVtbl->CopyPixels(
            src,
            &rc,                                        /* prc */
            frame->width * comp,                        /* cbStride */
            (UINT)frame->width * frame->height * comp,  /* cbBufferSize */
            frame->pixels);                             /* pbBuffer */
        if (FAILED(hr)) {
            goto end;
        }
    }

    status = fn_load(frame, context);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

end:
    if (conv) {
         conv->lpVtbl->Release(conv);
    }
    if (wicpalette) {
         wicpalette->lpVtbl->Release(wicpalette);
    }
    if (wiccolors) {
         sixel_allocator_free(pchunk->allocator, wiccolors);
    }
    if (wicframe) {
         wicframe->lpVtbl->Release(wicframe);
    }
    if (qdecoder) {
         qdecoder->lpVtbl->Release(qdecoder);
    }
    if (qframe) {
         qframe->lpVtbl->Release(qframe);
    }
    if (stream) {
         stream->lpVtbl->Release(stream);
    }
    if (factory) {
         factory->lpVtbl->Release(factory);
    }
    sixel_frame_unref(frame);

    CoUninitialize();

    if (FAILED(hr)) {
        return SIXEL_FALSE;
    }

    return SIXEL_OK;
}

#endif /* HAVE_WIC */

/* load image from file */

SIXELAPI SIXELSTATUS
sixel_helper_load_image_file(
    char const                /* in */     *filename,     /* source file name */
    int                       /* in */     fstatic,       /* whether to extract static image from animated gif */
    int                       /* in */     fuse_palette,  /* whether to use paletted image, set non-zero value to try to get paletted image */
    int                       /* in */     reqcolors,     /* requested number of colors, should be equal or less than SIXEL_PALETTE_MAX */
    unsigned char             /* in */     *bgcolor,      /* background color, may be NULL */
    int                       /* in */     loop_control,  /* one of enum loopControl */
    sixel_load_image_function /* in */     fn_load,       /* callback */
    int                       /* in */     finsecure,     /* true if do not verify SSL */
    int const                 /* in */     *cancel_flag,  /* cancel flag, may be NULL */
    void                      /* in/out */ *context,      /* private data which is passed to callback function as an argument, may be NULL */
    sixel_allocator_t         /* in */     *allocator     /* allocator object, may be NULL */
)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_chunk_t *pchunk = NULL;

    /* normalize reqested colors */
    if (reqcolors > SIXEL_PALETTE_MAX) {
        reqcolors = SIXEL_PALETTE_MAX;
    }

    /* create new chunk object from file */
    status = sixel_chunk_new(&pchunk, filename, finsecure, cancel_flag, allocator);
    if (status != SIXEL_OK) {
        goto end;
    }

    /* if input date is empty or 1 byte LF, ignore it and return successfully */
    if (pchunk->size == 0 || (pchunk->size == 1 && *pchunk->buffer == '\n')) {
        status = SIXEL_OK;
        goto end;
    }

    /* assertion */
    if (pchunk->buffer == NULL || pchunk->max_size == 0) {
        status = SIXEL_LOGIC_ERROR;
        goto end;
    }

    status = SIXEL_FALSE;
#ifdef HAVE_WIC
    if (SIXEL_FAILED(status) && !chunk_is_gif(pchunk)) {
        loader_trace_try("wic");
        status = load_with_wic(pchunk,
                               fstatic,
                               fuse_palette,
                               reqcolors,
                               bgcolor,
                               loop_control,
                               fn_load,
                               context);
        loader_trace_result("wic", status);
    }
#endif  /* HAVE_WIC */
#ifdef HAVE_COREGRAPHICS
    if (SIXEL_FAILED(status)) {
        loader_trace_try("coregraphics");
        status = load_with_coregraphics(pchunk,
                                        fstatic,
                                        fuse_palette,
                                        reqcolors,
                                        bgcolor,
                                        loop_control,
                                        fn_load,
                                        context);
        loader_trace_result("coregraphics", status);
    }
#endif  /* HAVE_COREGRAPHICS */
#ifdef HAVE_GDK_PIXBUF2
    if (SIXEL_FAILED(status)) {
        loader_trace_try("gdk-pixbuf2");
        status = load_with_gdkpixbuf(pchunk,
                                     fstatic,
                                     fuse_palette,
                                     reqcolors,
                                     bgcolor,
                                     loop_control,
                                     fn_load,
                                     context);
        loader_trace_result("gdk-pixbuf2", status);
    }
#endif  /* HAVE_GDK_PIXBUF2 */
#if HAVE_GD
    if (SIXEL_FAILED(status)) {
        loader_trace_try("gd");
        status = load_with_gd(pchunk,
                              fstatic,
                              fuse_palette,
                              reqcolors,
                              bgcolor,
                              loop_control,
                              fn_load,
                              context);
        loader_trace_result("gd", status);
    }
#endif  /* HAVE_GD */
    if (SIXEL_FAILED(status)) {
        loader_trace_try("builtin");
        status = load_with_builtin(pchunk,
                                   fstatic,
                                   fuse_palette,
                                   reqcolors,
                                   bgcolor,
                                   loop_control,
                                   fn_load,
                                   context);
        loader_trace_result("builtin", status);
    }
    if (SIXEL_FAILED(status)) {
        goto end;
    }

end:
    sixel_chunk_destroy(pchunk);

    return status;
}


#if HAVE_TESTS
static int
test1(void)
{
    int nret = EXIT_FAILURE;
    unsigned char *ptr = malloc(16);

    nret = EXIT_SUCCESS;
    goto error;

    nret = EXIT_SUCCESS;

error:
    free(ptr);
    return nret;
}


SIXELAPI int
sixel_loader_tests_main(void)
{
    int nret = EXIT_FAILURE;
    size_t i;
    typedef int (* testcase)(void);

    static testcase const testcases[] = {
        test1,
    };

    for (i = 0; i < sizeof(testcases) / sizeof(testcase); ++i) {
        nret = testcases[i]();
        if (nret != EXIT_SUCCESS) {
            goto error;
        }
    }

    nret = EXIT_SUCCESS;

error:
    return nret;
}
#endif  /* HAVE_TESTS */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
