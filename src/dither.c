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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <limits.h>

#if defined(HAVE_INTTYPES_H)
# include <inttypes.h>
#endif

#include "dither.h"
#include "quant.h"
#include "sixel.h"


static const unsigned char pal_mono_dark[] = {
    0x00, 0x00, 0x00, 0xff, 0xff, 0xff
};

static const unsigned char pal_mono_light[] = {
    0xff, 0xff, 0xff, 0x00, 0x00, 0x00
};

static const unsigned char pal_xterm256[] = {
    0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x80, 0x80, 0x00,
    0x00, 0x00, 0x80, 0x80, 0x00, 0x80, 0x00, 0x80, 0x80, 0xc0, 0xc0, 0xc0,
    0x80, 0x80, 0x80, 0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0xff, 0xff, 0x00,
    0x00, 0x00, 0xff, 0xff, 0x00, 0xff, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x5f, 0x00, 0x00, 0x87, 0x00, 0x00, 0xaf,
    0x00, 0x00, 0xd7, 0x00, 0x00, 0xff, 0x00, 0x5f, 0x00, 0x00, 0x5f, 0x5f,
    0x00, 0x5f, 0x87, 0x00, 0x5f, 0xaf, 0x00, 0x5f, 0xd7, 0x00, 0x5f, 0xff,
    0x00, 0x87, 0x00, 0x00, 0x87, 0x5f, 0x00, 0x87, 0x87, 0x00, 0x87, 0xaf,
    0x00, 0x87, 0xd7, 0x00, 0x87, 0xff, 0x00, 0xaf, 0x00, 0x00, 0xaf, 0x5f,
    0x00, 0xaf, 0x87, 0x00, 0xaf, 0xaf, 0x00, 0xaf, 0xd7, 0x00, 0xaf, 0xff,
    0x00, 0xd7, 0x00, 0x00, 0xd7, 0x5f, 0x00, 0xd7, 0x87, 0x00, 0xd7, 0xaf,
    0x00, 0xd7, 0xd7, 0x00, 0xd7, 0xff, 0x00, 0xff, 0x00, 0x00, 0xff, 0x5f,
    0x00, 0xff, 0x87, 0x00, 0xff, 0xaf, 0x00, 0xff, 0xd7, 0x00, 0xff, 0xff,
    0x5f, 0x00, 0x00, 0x5f, 0x00, 0x5f, 0x5f, 0x00, 0x87, 0x5f, 0x00, 0xaf,
    0x5f, 0x00, 0xd7, 0x5f, 0x00, 0xff, 0x5f, 0x5f, 0x00, 0x5f, 0x5f, 0x5f,
    0x5f, 0x5f, 0x87, 0x5f, 0x5f, 0xaf, 0x5f, 0x5f, 0xd7, 0x5f, 0x5f, 0xff,
    0x5f, 0x87, 0x00, 0x5f, 0x87, 0x5f, 0x5f, 0x87, 0x87, 0x5f, 0x87, 0xaf,
    0x5f, 0x87, 0xd7, 0x5f, 0x87, 0xff, 0x5f, 0xaf, 0x00, 0x5f, 0xaf, 0x5f,
    0x5f, 0xaf, 0x87, 0x5f, 0xaf, 0xaf, 0x5f, 0xaf, 0xd7, 0x5f, 0xaf, 0xff,
    0x5f, 0xd7, 0x00, 0x5f, 0xd7, 0x5f, 0x5f, 0xd7, 0x87, 0x5f, 0xd7, 0xaf,
    0x5f, 0xd7, 0xd7, 0x5f, 0xd7, 0xff, 0x5f, 0xff, 0x00, 0x5f, 0xff, 0x5f,
    0x5f, 0xff, 0x87, 0x5f, 0xff, 0xaf, 0x5f, 0xff, 0xd7, 0x5f, 0xff, 0xff,
    0x87, 0x00, 0x00, 0x87, 0x00, 0x5f, 0x87, 0x00, 0x87, 0x87, 0x00, 0xaf,
    0x87, 0x00, 0xd7, 0x87, 0x00, 0xff, 0x87, 0x5f, 0x00, 0x87, 0x5f, 0x5f,
    0x87, 0x5f, 0x87, 0x87, 0x5f, 0xaf, 0x87, 0x5f, 0xd7, 0x87, 0x5f, 0xff,
    0x87, 0x87, 0x00, 0x87, 0x87, 0x5f, 0x87, 0x87, 0x87, 0x87, 0x87, 0xaf,
    0x87, 0x87, 0xd7, 0x87, 0x87, 0xff, 0x87, 0xaf, 0x00, 0x87, 0xaf, 0x5f,
    0x87, 0xaf, 0x87, 0x87, 0xaf, 0xaf, 0x87, 0xaf, 0xd7, 0x87, 0xaf, 0xff,
    0x87, 0xd7, 0x00, 0x87, 0xd7, 0x5f, 0x87, 0xd7, 0x87, 0x87, 0xd7, 0xaf,
    0x87, 0xd7, 0xd7, 0x87, 0xd7, 0xff, 0x87, 0xff, 0x00, 0x87, 0xff, 0x5f,
    0x87, 0xff, 0x87, 0x87, 0xff, 0xaf, 0x87, 0xff, 0xd7, 0x87, 0xff, 0xff,
    0xaf, 0x00, 0x00, 0xaf, 0x00, 0x5f, 0xaf, 0x00, 0x87, 0xaf, 0x00, 0xaf,
    0xaf, 0x00, 0xd7, 0xaf, 0x00, 0xff, 0xaf, 0x5f, 0x00, 0xaf, 0x5f, 0x5f,
    0xaf, 0x5f, 0x87, 0xaf, 0x5f, 0xaf, 0xaf, 0x5f, 0xd7, 0xaf, 0x5f, 0xff,
    0xaf, 0x87, 0x00, 0xaf, 0x87, 0x5f, 0xaf, 0x87, 0x87, 0xaf, 0x87, 0xaf,
    0xaf, 0x87, 0xd7, 0xaf, 0x87, 0xff, 0xaf, 0xaf, 0x00, 0xaf, 0xaf, 0x5f,
    0xaf, 0xaf, 0x87, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xd7, 0xaf, 0xaf, 0xff,
    0xaf, 0xd7, 0x00, 0xaf, 0xd7, 0x5f, 0xaf, 0xd7, 0x87, 0xaf, 0xd7, 0xaf,
    0xaf, 0xd7, 0xd7, 0xaf, 0xd7, 0xff, 0xaf, 0xff, 0x00, 0xaf, 0xff, 0x5f,
    0xaf, 0xff, 0x87, 0xaf, 0xff, 0xaf, 0xaf, 0xff, 0xd7, 0xaf, 0xff, 0xff,
    0xd7, 0x00, 0x00, 0xd7, 0x00, 0x5f, 0xd7, 0x00, 0x87, 0xd7, 0x00, 0xaf,
    0xd7, 0x00, 0xd7, 0xd7, 0x00, 0xff, 0xd7, 0x5f, 0x00, 0xd7, 0x5f, 0x5f,
    0xd7, 0x5f, 0x87, 0xd7, 0x5f, 0xaf, 0xd7, 0x5f, 0xd7, 0xd7, 0x5f, 0xff,
    0xd7, 0x87, 0x00, 0xd7, 0x87, 0x5f, 0xd7, 0x87, 0x87, 0xd7, 0x87, 0xaf,
    0xd7, 0x87, 0xd7, 0xd7, 0x87, 0xff, 0xd7, 0xaf, 0x00, 0xd7, 0xaf, 0x5f,
    0xd7, 0xaf, 0x87, 0xd7, 0xaf, 0xaf, 0xd7, 0xaf, 0xd7, 0xd7, 0xaf, 0xff,
    0xd7, 0xd7, 0x00, 0xd7, 0xd7, 0x5f, 0xd7, 0xd7, 0x87, 0xd7, 0xd7, 0xaf,
    0xd7, 0xd7, 0xd7, 0xd7, 0xd7, 0xff, 0xd7, 0xff, 0x00, 0xd7, 0xff, 0x5f,
    0xd7, 0xff, 0x87, 0xd7, 0xff, 0xaf, 0xd7, 0xff, 0xd7, 0xd7, 0xff, 0xff,
    0xff, 0x00, 0x00, 0xff, 0x00, 0x5f, 0xff, 0x00, 0x87, 0xff, 0x00, 0xaf,
    0xff, 0x00, 0xd7, 0xff, 0x00, 0xff, 0xff, 0x5f, 0x00, 0xff, 0x5f, 0x5f,
    0xff, 0x5f, 0x87, 0xff, 0x5f, 0xaf, 0xff, 0x5f, 0xd7, 0xff, 0x5f, 0xff,
    0xff, 0x87, 0x00, 0xff, 0x87, 0x5f, 0xff, 0x87, 0x87, 0xff, 0x87, 0xaf,
    0xff, 0x87, 0xd7, 0xff, 0x87, 0xff, 0xff, 0xaf, 0x00, 0xff, 0xaf, 0x5f,
    0xff, 0xaf, 0x87, 0xff, 0xaf, 0xaf, 0xff, 0xaf, 0xd7, 0xff, 0xaf, 0xff,
    0xff, 0xd7, 0x00, 0xff, 0xd7, 0x5f, 0xff, 0xd7, 0x87, 0xff, 0xd7, 0xaf,
    0xff, 0xd7, 0xd7, 0xff, 0xd7, 0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0x5f,
    0xff, 0xff, 0x87, 0xff, 0xff, 0xaf, 0xff, 0xff, 0xd7, 0xff, 0xff, 0xff,
    0x08, 0x08, 0x08, 0x12, 0x12, 0x12, 0x1c, 0x1c, 0x1c, 0x26, 0x26, 0x26,
    0x30, 0x30, 0x30, 0x3a, 0x3a, 0x3a, 0x44, 0x44, 0x44, 0x4e, 0x4e, 0x4e,
    0x58, 0x58, 0x58, 0x62, 0x62, 0x62, 0x6c, 0x6c, 0x6c, 0x76, 0x76, 0x76,
    0x80, 0x80, 0x80, 0x8a, 0x8a, 0x8a, 0x94, 0x94, 0x94, 0x9e, 0x9e, 0x9e,
    0xa8, 0xa8, 0xa8, 0xb2, 0xb2, 0xb2, 0xbc, 0xbc, 0xbc, 0xc6, 0xc6, 0xc6,
    0xd0, 0xd0, 0xd0, 0xda, 0xda, 0xda, 0xe4, 0xe4, 0xe4, 0xee, 0xee, 0xee,
};


sixel_dither_t *
sixel_dither_create(int ncolors)
{
    sixel_dither_t *dither;
    int headsize;
    int datasize;
    int wholesize;
    int quality_mode;

    if (ncolors == -1) {
        ncolors = 256;
        quality_mode = QUALITY_HIGHCOLOR;
    }
    else {
        if (ncolors > SIXEL_PALETTE_MAX) {
            ncolors = 256;
        } else if (ncolors < 2) {
            ncolors = 2;
        }
        quality_mode = QUALITY_LOW;
    }
    headsize = sizeof(sixel_dither_t);
    datasize = ncolors * 3;
    wholesize = headsize + datasize;// + cachesize;

    dither = malloc(wholesize);
    if (dither == NULL) {
        return NULL;
    }
    dither->ref = 1;
    dither->palette = (unsigned char*)(dither + 1);
    dither->cachetable = NULL;
    dither->reqcolors = ncolors;
    dither->ncolors = ncolors;
    dither->origcolors = (-1);
    dither->keycolor = (-1);
    dither->optimized = 0;
    dither->optimize_palette = 0;
    dither->complexion = 1;
    dither->bodyonly = 0;
    dither->method_for_largest = LARGE_NORM;
    dither->method_for_rep = REP_CENTER_BOX;
    dither->method_for_diffuse = DIFFUSE_FS;
    dither->quality_mode = quality_mode;
    dither->pixelformat = COLOR_RGB888;

    return dither;
}


void
sixel_dither_destroy(sixel_dither_t *dither)
{
    if (dither) {
        free(dither->cachetable);
        free(dither);
    }
}


void
sixel_dither_ref(sixel_dither_t *dither)
{
    /* TODO: be thread safe */
    ++dither->ref;
}


void
sixel_dither_unref(sixel_dither_t *dither)
{
    /* TODO: be thread safe */
    if (dither != NULL && --dither->ref == 0) {
        sixel_dither_destroy(dither);
    }
}


sixel_dither_t *
sixel_dither_get(int builtin_dither)
{
    unsigned char *palette;
    int ncolors;
    int keycolor;
    sixel_dither_t *dither;

    switch (builtin_dither) {
    case BUILTIN_MONO_DARK:
        ncolors = 2;
        palette = (unsigned char *)pal_mono_dark;
        keycolor = 0;
        break;
    case BUILTIN_MONO_LIGHT:
        ncolors = 2;
        palette = (unsigned char *)pal_mono_light;
        keycolor = 0;
        break;
    case BUILTIN_XTERM16:
        ncolors = 16;
        palette = (unsigned char *)pal_xterm256;
        keycolor = (-1);
        break;
    case BUILTIN_XTERM256:
        ncolors = 256;
        palette = (unsigned char *)pal_xterm256;
        keycolor = (-1);
        break;
    default:
        return NULL;
    }

    dither = sixel_dither_create(ncolors);
    if (dither) {
        dither->palette = palette;
        dither->keycolor = keycolor;
        dither->optimized = 1;
        dither->optimize_palette = 0;
    }

    return dither;
}


static void
get_rgb(unsigned char *data, int const pixelformat, int depth,
        unsigned char *r, unsigned char *g, unsigned char *b)
{
    unsigned int pixels = 0, low, high;
	int count = 0;

	while (count < depth) {
		pixels = *(data + count) | (pixels << 8);
		count++;
	}

	/* XXX: we should swap bytes (only necessary on LSByte first hardware?) */
	if (depth == 2) {
		low    = pixels & 0xFF;
		high   = (pixels >> 8) & 0xFF;
		pixels = (low << 8) | high;
	}

    switch (pixelformat) {
    case COLOR_RGB555:
        *r = ((pixels >> 10) & 0x1F) << 3;
        *g = ((pixels >>  5) & 0x1F) << 3;
        *b = ((pixels >>  0) & 0x1F) << 3;
        break;
    case COLOR_RGB565:
        *r = ((pixels >> 11) & 0x1F) << 3;
        *g = ((pixels >>  5) & 0x3F) << 2;
        *b = ((pixels >>  0) & 0x1F) << 3;
        break;
    case COLOR_RGBA8888:
        *r = (pixels >> 24) & 0xFF;
        *g = (pixels >> 16) & 0xFF;
        *b = (pixels >>  8) & 0xFF;
        break;
    case COLOR_ARGB8888:
        *r = (pixels >> 16) & 0xFF;
        *g = (pixels >>  8) & 0xFF;
        *b = (pixels >>  0) & 0xFF;
        break;
    case GRAYSCALE_GA88:
        *r = *g = *b = (pixels >> 8) & 0xFF;
        break;
    case GRAYSCALE_G8:
    case GRAYSCALE_AG88:
        *r = *g = *b = pixels & 0xFF;
        break;
    default:
        *r = *g = *b = 0;
        break;
    }
}


void
sixel_normalize_pixelformat(unsigned char *dst, unsigned char *src,
                            int width, int height,
                            int const pixelformat)
{
    int x, y, dst_offset, src_offset, depth;
    unsigned char r, g, b;

    if (pixelformat == GRAYSCALE_G8)
        depth = 1;
    else if (pixelformat == COLOR_RGB565 || pixelformat == COLOR_RGB555
             || pixelformat == GRAYSCALE_GA88 || pixelformat == GRAYSCALE_AG88)
        depth = 2;
    else /* COLOR_RGBA8888 or COLOR_ARGB8888 */
        depth = 4;

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            src_offset = depth * (y * width + x);
            dst_offset = 3 * (y * width + x);
            get_rgb(src + src_offset, pixelformat, depth, &r, &g, &b);

            *(dst + dst_offset + 0) = r;
            *(dst + dst_offset + 1) = g;
            *(dst + dst_offset + 2) = b;
        }
    }
}


static void
sixel_dither_set_method_for_largest(sixel_dither_t *dither, int method_for_largest)
{
    if (method_for_largest == LARGE_AUTO) {
        method_for_largest = LARGE_NORM;
    }
    dither->method_for_largest = method_for_largest;
}


static void
sixel_dither_set_method_for_rep(sixel_dither_t *dither, int method_for_rep)
{
    if (method_for_rep == REP_AUTO) {
        method_for_rep = REP_CENTER_BOX;
    }
    dither->method_for_rep = method_for_rep;
}


static void
sixel_dither_set_quality_mode(sixel_dither_t *dither, int quality_mode)
{
    if (quality_mode == QUALITY_AUTO) {
        if (dither->ncolors <= 8) {
            quality_mode = QUALITY_HIGH;
        } else {
            quality_mode = QUALITY_LOW;
        }
    }
    dither->quality_mode = quality_mode;
}


int
sixel_dither_initialize(sixel_dither_t *dither, unsigned char *data,
                        int width, int height, int const pixelformat,
                        int method_for_largest, int method_for_rep,
                        int quality_mode)
{
    unsigned char *buf = NULL;
    unsigned char *normalized_pixels = NULL;
    unsigned char *input_pixels;

    /* normalize pixelformat */
    normalized_pixels = malloc(width * height * 3);
    if (normalized_pixels == NULL) {
        return (-1);
    }

    if (pixelformat != COLOR_RGB888) {
        sixel_normalize_pixelformat(normalized_pixels, data,
                                    width, height, pixelformat);
        input_pixels = normalized_pixels;
    } else {
        input_pixels = data;
    }

    sixel_dither_set_method_for_largest(dither, method_for_largest);
    sixel_dither_set_method_for_rep(dither, method_for_rep);
    sixel_dither_set_quality_mode(dither, quality_mode);

    buf = sixel_quant_make_palette(input_pixels,
                                   width * height * 3,
                                   COLOR_RGB888,
                                   dither->reqcolors, &dither->ncolors,
                                   &dither->origcolors,
                                   dither->method_for_largest,
                                   dither->method_for_rep,
                                   dither->quality_mode);
    if (buf == NULL) {
        free(normalized_pixels);
        return (-1);
    }
    memcpy(dither->palette, buf, dither->ncolors * 3);

    dither->optimized = 1;
    if (dither->origcolors <= dither->ncolors) {
        dither->method_for_diffuse = DIFFUSE_NONE;
    }

    free(normalized_pixels);
    sixel_quant_free_palette(buf);

    return 0;
}


void
sixel_dither_set_diffusion_type(sixel_dither_t *dither, int method_for_diffuse)
{
    if (method_for_diffuse == DIFFUSE_AUTO) {
        if (dither->ncolors > 16) {
            method_for_diffuse = DIFFUSE_FS;
        } else {
            method_for_diffuse = DIFFUSE_ATKINSON;
        }
    }
    dither->method_for_diffuse = method_for_diffuse;
}


int
sixel_dither_get_num_of_palette_colors(sixel_dither_t *dither)
{
    return dither->ncolors;
}


int
sixel_dither_get_num_of_histgram_colors(sixel_dither_t *dither)
{
    return dither->origcolors;
}


unsigned char *
sixel_dither_get_palette(sixel_dither_t /* in */ *dither)  /* dither context object */
{
    return dither->palette;
}


void
sixel_dither_set_complexion_score(sixel_dither_t /* in */ *dither,  /* dither context object */
                                  int            /* in */ score)    /* complexion score (>= 1) */
{
    dither->complexion = score;
}


void
sixel_dither_set_body_only(sixel_dither_t /* in */ *dither,     /* dither context object */
                           int            /* in */ bodyonly)    /* 0: output palette section
                                                                   1: do not output palette section  */
{
    dither->bodyonly = bodyonly;
}


void
sixel_dither_set_optimize_palette(
    sixel_dither_t /* in */ *dither,   /* dither context object */
    int            /* in */ do_opt)    /* 0: optimize palette size
                                          1: don't optimize palette size */
{
    dither->optimize_palette = do_opt;
}


void
sixel_dither_set_pixelformat(
    sixel_dither_t /* in */ *dither,     /* dither context object */
    int            /* in */ pixelformat) /* one of enum pixelFormat */
{
    dither->pixelformat = pixelformat;
}


unsigned char *
sixel_dither_apply_palette(sixel_dither_t *dither,
                           unsigned char *pixels,
                           int width, int height)
{
    int ret;
    int bufsize;
    int cachesize;
    unsigned char *dest;
    int ncolors;
    unsigned char *normalized_pixels = NULL;
    unsigned char *input_pixels;

    bufsize = width * height * sizeof(unsigned char);
    dest = malloc(bufsize);
    if (dest == NULL) {
        return NULL;
    }

    /* if quality_mode is full, do not use palette caching */
    if (dither->quality_mode == QUALITY_FULL) {
        dither->optimized = 0;
    }

    if (dither->cachetable == NULL && dither->optimized) {
        if (dither->palette != pal_mono_dark && dither->palette != pal_mono_light) {
            cachesize = (1 << 3 * 5) * sizeof(unsigned short);
#if HAVE_CALLOC
            dither->cachetable = calloc(cachesize, 1);
#else
            dither->cachetable = malloc(cachesize);
            memset(dither->cachetable, 0, cachesize);
#endif
        }
    }

    if (dither->pixelformat != COLOR_RGB888) {
        /* normalize pixelformat */
        normalized_pixels = malloc(width * height * 3);
        if (normalized_pixels == NULL) {
            goto end;
        }
        sixel_normalize_pixelformat(normalized_pixels,
                                    pixels,
                                    width, height,
                                    dither->pixelformat);
        input_pixels = normalized_pixels;
    } else {
        input_pixels = pixels;
    }

    ret = sixel_quant_apply_palette(input_pixels,
                                    width, height, 3,
                                    dither->palette,
                                    dither->ncolors,
                                    dither->method_for_diffuse,
                                    dither->optimized,
                                    dither->optimize_palette,
                                    dither->complexion,
                                    dither->cachetable,
                                    &ncolors,
                                    dest);
    if (ret != 0) {
        free(dest);
        dest = NULL;
    }

    dither->ncolors = ncolors;

end:
    free(normalized_pixels);
    return dest;
}

/* emacs, -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set expandtab ts=4 : */
/* EOF */
