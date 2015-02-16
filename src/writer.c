/*
 * Copyright (c) 2014 Hayaki Saito
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if HAVE_SETJMP_H
# include <setjmp.h>
#endif

#if HAVE_LIBPNG
# include <png.h>
#else
# include "stb_image_write.h"
#endif

#include <sixel.h>
#include <sixel-imageio.h>

#if !defined(HAVE_MEMCPY)
# define memcpy(d, s, n) (bcopy ((s), (d), (n)))
#endif

#if !defined(HAVE_MEMMOVE)
# define memmove(d, s, n) (bcopy ((s), (d), (n)))
#endif

#if !defined(O_BINARY) && defined(_O_BINARY)
# define O_BINARY _O_BINARY
#endif  /* !defined(O_BINARY) && !defined(_O_BINARY) */


#if !HAVE_LIBPNG
unsigned char *
stbi_write_png_to_mem(unsigned char *pixels, int stride_bytes,
                      int x, int y, int n, int *out_len);
#endif

static int
write_png_to_file(
    unsigned char  /* in */ *data,         /* source pixel data */
    int            /* in */ width,         /* source data width */
    int            /* in */ height,        /* source data height */
    unsigned char  /* in */ *palette,      /* palette of source data */
    int            /* in */ pixelformat,   /* source pixelFormat */
    char const     /* in */ *filename)     /* destination filename */
{
    int ret = 0;
    FILE *output_fp = NULL;
    unsigned char *pixels = NULL;
    unsigned char *new_pixels = NULL;
#if HAVE_LIBPNG
    int y;
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    unsigned char **rows = NULL;
#else
    unsigned char *png_data = NULL;
    int png_len;
    int write_len;
#endif  /* HAVE_LIBPNG */
    int i;
    unsigned char *src;
    unsigned char *dst;

    switch (pixelformat) {
    case PIXELFORMAT_PAL1:
    case PIXELFORMAT_PAL2:
    case PIXELFORMAT_PAL4:
        new_pixels = malloc(width * height * 4);
        src = new_pixels + width * height * 3;
        dst = pixels = new_pixels;
        ret = sixel_helper_normalize_pixelformat(src, data,
                                                 width, height,
                                                 pixelformat);
        if (ret != 0) {
            goto end;
        }
        for (i = 0; i < width * height; ++i, ++src) {
            *dst++ = *(palette + *src * 3 + 0);
            *dst++ = *(palette + *src * 3 + 1);
            *dst++ = *(palette + *src * 3 + 2);
        }
        break;
    case PIXELFORMAT_PAL8:
        src = data;
        dst = pixels = new_pixels = malloc(width * height * 3);
        for (i = 0; i < width * height; ++i, ++src) {
            *dst++ = *(palette + *src * 3 + 0);
            *dst++ = *(palette + *src * 3 + 1);
            *dst++ = *(palette + *src * 3 + 2);
        }
        break;
    case PIXELFORMAT_RGB888:
        pixels = data;
        break;
    case PIXELFORMAT_G8:
    case PIXELFORMAT_RGB565:
    case PIXELFORMAT_RGB555:
    case PIXELFORMAT_BGR565:
    case PIXELFORMAT_BGR555:
    case PIXELFORMAT_GA88:
    case PIXELFORMAT_AG88:
    case PIXELFORMAT_BGR888:
    case PIXELFORMAT_RGBA8888:
    case PIXELFORMAT_ARGB8888:
        pixels = new_pixels = malloc(width * height * 3);
        ret = sixel_helper_normalize_pixelformat(pixels, data,
                                                 width, height,
                                                 pixelformat);
        if (ret != 0) {
            goto end;
        }
        break;
    }

    if (strcmp(filename, "-") == 0) {
#if defined(O_BINARY)
# if HAVE__SETMODE
        _setmode(fileno(stdout), O_BINARY);
# elif HAVE_SETMODE
        setmode(fileno(stdout), O_BINARY);
# endif  /* HAVE_SETMODE */
#endif  /* defined(O_BINARY) */
        output_fp = stdout;
    } else {
        output_fp = fopen(filename, "wb");
        if (!output_fp) {
#if HAVE_ERRNO_H
            perror("fopen() failed.");
#endif  /* HAVE_ERRNO_H */
            ret = -1;
            goto end;
        }
    }

#if HAVE_LIBPNG
    rows = malloc(height * sizeof(unsigned char *));
    for (y = 0; y < height; ++y) {
        rows[y] = pixels + width * 3 * y;
    }
    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
        ret = (-1);
        goto end;
    }
    info_ptr = png_create_info_struct(png_ptr);
    if (!png_ptr) {
        ret = (-1);
        goto end;
    }
# if USE_SETJMP && HAVE_SETJMP
    if (setjmp(png_jmpbuf(png_ptr))) {
        ret = (-1);
        goto end;
    }
# endif
    png_init_io(png_ptr, output_fp);
    png_set_IHDR(png_ptr, info_ptr, width, height,
                 /* bit_depth */ 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
    png_write_info(png_ptr, info_ptr);
    png_write_image(png_ptr, rows);
    png_write_end(png_ptr, NULL);
#else
    png_data = stbi_write_png_to_mem(pixels, width * 3,
                                     width, height,
                                     /* STBI_rgb */ 3, &png_len);
    if (!png_data) {
        fprintf(stderr, "stbi_write_png_to_mem failed.\n");
        goto end;
    }
    write_len = fwrite(png_data, 1, png_len, output_fp);
    if (write_len < 0) {
# if HAVE_ERRNO_H
        perror("fwrite failed.");
# endif  /* HAVE_ERRNO_H */
        ret = -1;
        goto end;
    }
#endif  /* HAVE_LIBPNG */

    ret = 0;

end:
    if (output_fp && output_fp != stdout) {
        fclose(output_fp);
    }
#if HAVE_LIBPNG
    free(rows);
    png_destroy_write_struct (&png_ptr, &info_ptr);
#else
    free(png_data);
#endif  /* HAVE_LIBPNG */
    free(new_pixels);

    return ret;
}


int
sixel_helper_write_image_file(
    unsigned char  /* in */ *data,        /* source pixel data */
    int            /* in */ width,        /* source data width */
    int            /* in */ height,       /* source data height */
    unsigned char  /* in */ *palette,     /* palette of source data */
    int            /* in */ pixelformat,  /* source pixelFormat */
    char const     /* in */ *filename,    /* destination filename */
    int            /* in */ imageformat)  /* destination imageformat */
{
    int nret = (-1);

    switch (imageformat) {
    case FORMAT_PNG:
        nret = write_png_to_file(data, width, height, palette,
                                 pixelformat, filename);
        break;
    case FORMAT_GIF:
    case FORMAT_BMP:
    case FORMAT_JPG:
    case FORMAT_TGA:
    case FORMAT_WBMP:
    case FORMAT_TIFF:
    case FORMAT_SIXEL:
    case FORMAT_PNM:
    case FORMAT_GD2:
    case FORMAT_PSD:
    case FORMAT_HDR:
    default:
        nret = (-1);
        break;
    }

    return nret;
}


/* emacs, -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set expandtab ts=4 : */
/* EOF */
