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
#include "malloc_stub.h"

#include <stdlib.h>
#include <string.h>

#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#if HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif

#if HAVE_FCNTL_H
# include <fcntl.h>
#endif

#if !defined(HAVE_MEMCPY)
# define memcpy(d, s, n) (bcopy ((s), (d), (n)))
#endif

#if !defined(HAVE_MEMMOVE)
# define memmove(d, s, n) (bcopy ((s), (d), (n)))
#endif

#if !defined(O_BINARY) && defined(_O_BINARY)
# define O_BINARY _O_BINARY
#endif  /* !defined(O_BINARY) && !defined(_O_BINARY) */

#include <assert.h>

#define STBI_NO_STDIO 1
#include "stb_image.h"

#ifdef HAVE_GDK_PIXBUF2
# include <gdk-pixbuf/gdk-pixbuf.h>
#endif

#ifdef HAVE_GD
# include <gd.h>
#endif

#ifdef HAVE_LIBCURL
# include <curl/curl.h>
#endif

#include <stdio.h>
#include "frompnm.h"
#include "loader.h"

#define STBI_NO_STDIO 1
#define STB_IMAGE_IMPLEMENTATION 1
#include "stb_image.h"

typedef struct chunk
{
    unsigned char* buffer;
    size_t size;
    size_t max_size;
} chunk_t;


static void
chunk_init(chunk_t * const pchunk, size_t initial_size)
{
    pchunk->max_size = initial_size;
    pchunk->size = 0;
    pchunk->buffer = malloc(pchunk->max_size);
}

static size_t
memory_write(void* ptr, size_t size, size_t len, void* memory)
{
    size_t nbytes;
    chunk_t* chunk;

    nbytes = size * len;
    if (nbytes == 0) {
        return 0;
    }

    chunk = (chunk_t*)memory;

    if (chunk->max_size <= chunk->size + nbytes) {
        do {
            chunk->max_size *= 2;
        } while (chunk->max_size <= chunk->size + nbytes);
        chunk->buffer = (unsigned char*)realloc(chunk->buffer, chunk->max_size);
    }

    memcpy(chunk->buffer + chunk->size, ptr, nbytes);
    chunk->size += nbytes;

    return nbytes;
}


static FILE *
open_binary_file(char const *filename)
{
    FILE *f;

    if (filename == NULL || strcmp(filename, "-") == 0) {
        /* for windows */
#if defined(O_BINARY)
# if HAVE__SETMODE
        _setmode(fileno(stdin), O_BINARY);
# elif HAVE_SETMODE
        setmode(fileno(stdin), O_BINARY);
# endif  /* HAVE_SETMODE */
#endif  /* defined(O_BINARY) */
        return stdin;
    }
    f = fopen(filename, "rb");
    if (!f) {
#if _ERRNO_H
        fprintf(stderr, "fopen('%s') failed.\n" "reason: %s.\n",
                filename, strerror(errno));
#endif  /* HAVE_ERRNO_H */
        return NULL;
    }
    return f;
}


static int
get_chunk_from_file(char const *filename, chunk_t *pchunk)
{
    FILE *f;
    int n;

    f = open_binary_file(filename);
    if (!f) {
        return (-1);
    }

    chunk_init(pchunk, 64 * 1024);
    if (pchunk->buffer == NULL) {
#if _ERRNO_H
        fprintf(stderr, "get_chunk_from_file('%s'): malloc failed.\n" "reason: %s.\n",
                filename, strerror(errno));
#endif  /* HAVE_ERRNO_H */
        return (-1);
    }

    for (;;) {
        if ((pchunk->max_size - pchunk->size) < 4096) {
            pchunk->max_size *= 2;
            if ((pchunk->buffer = (unsigned char *)realloc(pchunk->buffer, pchunk->max_size)) == NULL) {
#if _ERRNO_H
                fprintf(stderr, "get_chunk_from_file('%s'): relloc failed.\n" "reason: %s.\n",
                        filename, strerror(errno));
#endif  /* HAVE_ERRNO_H */
                return (-1);
            }
        }
        if ((n = fread(pchunk->buffer + pchunk->size, 1, 4096, f)) <= 0) {
            break;
        }
        pchunk->size += n;
    }

    if (f != stdout) {
        fclose(f);
    }

    return 0;
}


# ifdef HAVE_LIBCURL
static int
get_chunk_from_url(char const *url, chunk_t *pchunk)
{
    CURL *curl;
    CURLcode code;
 
    chunk_init(pchunk, 1024);
    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    if (strncmp(url, "https://", 8) == 0) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, memory_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)pchunk);
    if ((code = curl_easy_perform(curl))) {
        fprintf(stderr, "curl_easy_perform('%s') failed.\n" "code: %d.\n",
                url, code);
        curl_easy_cleanup(curl);
        return (-1);
    }
    curl_easy_cleanup(curl);
    return 0;
}
# endif  /* HAVE_LIBCURL */


static int
get_chunk(char const *filename, chunk_t *pchunk)
{
    if (filename != NULL && strstr(filename, "://")) {
# ifdef HAVE_LIBCURL
        return get_chunk_from_url(filename, pchunk);
# else
        fprintf(stderr, "To specify URI schemes, you have to "
                        "configure this program with --with-libcurl "
                        "option at compile time.\n");
        return (-1);
# endif  /* HAVE_LIBCURL */
    }
    return get_chunk_from_file(filename, pchunk);
}


static int
chunk_is_sixel(chunk_t const *chunk)
{
    unsigned char *p;
    unsigned char *end;
    int result;

    result = 0;
    p = chunk->buffer;
    end = p + chunk->size;

    p++;
    p++;
    if (p >= end) {
        return 0;
    }
    if (*(p - 1) == 0x90 ||
        (*(p - 1) == 0x1b && *p == 0x50)) {
        while (p++ < end) {
            if (*p == 0x70) {
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


static int
chunk_is_pnm(chunk_t const *chunk)
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


static int
chunk_is_gif(chunk_t const *chunk)
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


static unsigned char *
load_with_builtin(chunk_t const *pchunk, int *psx, int *psy,
                  int *pcomp, int *pstride,
                  int *pframe_count, int *ploop_count, int **ppdelay)
{
    FILE *f;
    unsigned char *p;
    unsigned char *pixels;
    static stbi__context s;
    static stbi__gif g;
    chunk_t frames;
    chunk_t delays;

    if (chunk_is_sixel(pchunk)) {
        /* sixel */
        *pframe_count = 1;
        *ploop_count = 1;
    } else if (chunk_is_pnm(pchunk)) {
        /* pnm */
        pixels = load_pnm(pchunk->buffer, pchunk->size,
                          psx, psy, pcomp, pstride);
        if (!pixels) {
#if _ERRNO_H
            fprintf(stderr, "load_pnm failed.\n" "reason: %s.\n",
                    strerror(errno));
#endif  /* HAVE_ERRNO_H */
            return NULL;
        }
        *pframe_count = 1;
        *ploop_count = 1;
    } else if (chunk_is_gif(pchunk)) {
        chunk_init(&frames, 1024);
        chunk_init(&delays, 1024);
        stbi__start_mem(&s, pchunk->buffer, pchunk->size);
        *pframe_count = 0;
        memset(&g, 0, sizeof(g));

        for (;;) {
            p = stbi__gif_load_next(&s, &g, pcomp, 4);
            if (p == (void *) 1) {
                /* end of animated gif marker */
                break;
            }
            if (p == 0) {
                free(frames.buffer);
                pixels = NULL;
                break;
            }
            *psx = g.w;
            *psy = g.h;
            memory_write((void *)p, 1, *psx * *psy * 4, (void *)&frames);
            memory_write((void *)&g.delay, sizeof(g.delay), 1, (void *)&delays);
            ++*pframe_count;
            pixels = frames.buffer;
        }
        *ploop_count = g.loop_count;
        *ppdelay = (int *)delays.buffer;

        if (!pixels) {
            fprintf(stderr, "stbi_load_from_file failed.\n" "reason: %s.\n",
                    stbi_failure_reason());
            return NULL;
        }
    } else {
        stbi__start_mem(&s, pchunk->buffer, pchunk->size);
        pixels = stbi_load_main(&s, psx, psy, pcomp, 3);
        if (!pixels) {
            fprintf(stderr, "stbi_load_from_file failed.\n" "reason: %s.\n",
                    stbi_failure_reason());
            return NULL;
        }
        *pframe_count = 1;
        *ploop_count = 1;
        *pcomp = 3;  /* reset component to 3 */
    }

    *pstride = *pcomp * *psx;
    return pixels;
}


#ifdef HAVE_GDK_PIXBUF2
static unsigned char *
load_with_gdkpixbuf(chunk_t const *pchunk, int *psx, int *psy,
                    int *pcomp, int *pstride, int *pframe_count,
                    int *ploop_count, int **ppdelay)
{
    GdkPixbuf *pixbuf;
    GdkPixbufAnimation *animation;
    unsigned char *pixels;
    unsigned char *p;
    GdkPixbufLoader *loader;
    chunk_t frames;
    chunk_t delays;
#if 1
    GdkPixbufAnimationIter *it;
    GTimeVal time;
    int delay;
#endif

    chunk_init(&frames, 1024);

#if (!GLIB_CHECK_VERSION(2, 36, 0))
    g_type_init();
#endif
    g_get_current_time(&time);
    loader = gdk_pixbuf_loader_new();
    gdk_pixbuf_loader_write(loader, pchunk->buffer, pchunk->size, NULL);
    animation = gdk_pixbuf_loader_get_animation(loader);
    if (gdk_pixbuf_animation_is_static_image(animation)) {
        pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
        if (pixbuf == NULL) {
            return NULL;
        }
        p = gdk_pixbuf_get_pixels(pixbuf);
        *psx = gdk_pixbuf_get_width(pixbuf);
        *psy = gdk_pixbuf_get_height(pixbuf);
        *pcomp = gdk_pixbuf_get_has_alpha(pixbuf) ? 4: 3;
        *pstride = gdk_pixbuf_get_rowstride(pixbuf);
        memory_write((void *)p, 1, *psx * *psy * *pcomp, (void *)&frames);
        pixels = frames.buffer;
    } else {
        chunk_init(&delays, 1024);
        g_get_current_time(&time);

        it = gdk_pixbuf_animation_get_iter(animation, &time);
        *pframe_count = 0;
        while (!gdk_pixbuf_animation_iter_on_currently_loading_frame(it)) {
            delay = gdk_pixbuf_animation_iter_get_delay_time(it);
            g_time_val_add(&time, delay * 1000);
            pixbuf = gdk_pixbuf_animation_iter_get_pixbuf(it);
            p = gdk_pixbuf_get_pixels(pixbuf);

            if (pixbuf == NULL) {
                pixels = NULL;
                break;
            }
            *psx = gdk_pixbuf_get_width(pixbuf);
            *psy = gdk_pixbuf_get_height(pixbuf);
            *pcomp = gdk_pixbuf_get_has_alpha(pixbuf) ? 4: 3;
            *pstride = gdk_pixbuf_get_rowstride(pixbuf);
            memory_write((void *)p, 1, *psx * *psy * *pcomp, (void *)&frames);
            delay /= 10;
            memory_write((void *)&delay, sizeof(delay), 1, (void *)&delays);
            ++*pframe_count;
            gdk_pixbuf_animation_iter_advance(it, &time);
            pixels = frames.buffer;
        }
        *ppdelay = (int *)delays.buffer;
        /* TODO: get loop property */
        *ploop_count = 0;
    }
    gdk_pixbuf_loader_close(loader, NULL);
    g_object_unref(loader);
    return pixels;
}
#endif  /* HAVE_GDK_PIXBUF2 */

#ifdef HAVE_GD

#define        FMT_GIF     0
#define        FMT_PNG     1
#define        FMT_BMP     2
#define        FMT_JPG     3
#define        FMT_TGA     4
#define        FMT_WBMP    5
#define        FMT_TIFF    6
#define        FMT_SIXEL   7
#define        FMT_PNM     8
#define        FMT_GD2     9
#define        FMT_PSD     10
#define        FMT_HDR     11

static int
detect_file_format(int len, unsigned char *data)
{
    if (memcmp("TRUEVISION", data + len - 18, 10) == 0) {
        return FMT_TGA;
    }

    if (memcmp("GIF", data, 3) == 0) {
        return FMT_GIF;
    }

    if (memcmp("\x89\x50\x4E\x47\x0D\x0A\x1A\x0A", data, 8) == 0) {
        return FMT_PNG;
    }

    if (memcmp("BM", data, 2) == 0) {
        return FMT_BMP;
    }

    if (memcmp("\xFF\xD8", data, 2) == 0) {
        return FMT_JPG;
    }

    if (memcmp("\x00\x00", data, 2) == 0) {
        return FMT_WBMP;
    }

    if (memcmp("\x4D\x4D", data, 2) == 0) {
        return FMT_TIFF;
    }

    if (memcmp("\x49\x49", data, 2) == 0) {
        return FMT_TIFF;
    }

    if (memcmp("\033P", data, 2) == 0) {
        return FMT_SIXEL;
    }

    if (data[0] == 0x90  && (data[len-1] == 0x9C || data[len-2] == 0x9C)) {
        return FMT_SIXEL;
    }

    if (data[0] == 'P' && data[1] >= '1' && data[1] <= '6') {
        return FMT_PNM;
    }

    if (memcmp("gd2", data, 3) == 0) {
        return FMT_GD2;
    }

    if (memcmp("8BPS", data, 4) == 0) {
        return FMT_PSD;
    }

    if (memcmp("#?RADIANCE\n", data, 11) == 0) {
        return FMT_HDR;
    }

    return (-1);
}


static unsigned char *
load_with_gd(chunk_t const *pchunk, int *psx, int *psy, int *pcomp, int *pstride)
{
    unsigned char *pixels, *p;
    int n, max;
    gdImagePtr im;
    FILE *f;
    int x, y;
    int c;

    switch(detect_file_format(pchunk->size, pchunk->buffer)) {
#if 0
# if HAVE_DECL_GDIMAGECREATEFROMGIFPTR
        case FMT_GIF:
            im = gdImageCreateFromGifPtr(pchunk->size, pchunk->buffer);
            break;
# endif  /* HAVE_DECL_GDIMAGECREATEFROMGIFPTR */
#endif
#if HAVE_DECL_GDIMAGECREATEFROMPNGPTR
        case FMT_PNG:
            im = gdImageCreateFromPngPtr(pchunk->size, pchunk->buffer);
            break;
#endif  /* HAVE_DECL_GDIMAGECREATEFROMPNGPTR */
#if HAVE_DECL_GDIMAGECREATEFROMBMPPTR
        case FMT_BMP:
            im = gdImageCreateFromBmpPtr(pchunk->size, pchunk->buffer);
            break;
#endif  /* HAVE_DECL_GDIMAGECREATEFROMBMPPTR */
        case FMT_JPG:
#if HAVE_DECL_GDIMAGECREATEFROMJPEGPTREX
            im = gdImageCreateFromJpegPtrEx(pchunk->size, pchunk->buffer, 1);
#elif HAVE_DECL_GDIMAGECREATEFROMJPEGPTR
            im = gdImageCreateFromJpegPtr(pchunk->size, pchunk->buffer);
#endif  /* HAVE_DECL_GDIMAGECREATEFROMJPEGPTREX */
            break;
#if HAVE_DECL_GDIMAGECREATEFROMTGAPTR
        case FMT_TGA:
            im = gdImageCreateFromTgaPtr(pchunk->size, pchunk->buffer);
            break;
#endif  /* HAVE_DECL_GDIMAGECREATEFROMTGAPTR */
#if HAVE_DECL_GDIMAGECREATEFROMWBMPPTR
        case FMT_WBMP:
            im = gdImageCreateFromWBMPPtr(pchunk->size, pchunk->buffer);
            break;
#endif  /* HAVE_DECL_GDIMAGECREATEFROMWBMPPTR */
#if HAVE_DECL_GDIMAGECREATEFROMTIFFPTR
        case FMT_TIFF:
            im = gdImageCreateFromTiffPtr(pchunk->size, pchunk->buffer);
            break;
#endif  /* HAVE_DECL_GDIMAGECREATEFROMTIFFPTR */
#if HAVE_DECL_GDIMAGECREATEFROMGD2PTR
        case FMT_GD2:
            im = gdImageCreateFromGd2Ptr(pchunk->size, pchunk->buffer);
            break;
#endif  /* HAVE_DECL_GDIMAGECREATEFROMGD2PTR */
        default:
            return NULL;
    }

    if (im == NULL) {
        return NULL;
    }

    if (!gdImageTrueColor(im)) {
#if HAVE_DECL_GDIMAGEPALETTETOTRUECOLOR
        if (!gdImagePaletteToTrueColor(im)) {
            return NULL;
        }
#else
        return NULL;
#endif
    }

    *psx = gdImageSX(im);
    *psy = gdImageSY(im);
    *pcomp = 3;
    *pstride = *psx * *pcomp;
    p = pixels = malloc(*pstride * *psy);
    if (p == NULL) {
#if _ERRNO_H
        fprintf(stderr, "load_with_gd failed.\n" "reason: %s.\n",
                strerror(errno));
#endif  /* HAVE_ERRNO_H */
        gdImageDestroy(im);
        return NULL;
    }
    for (y = 0; y < *psy; y++) {
        for (x = 0; x < *psx; x++) {
            c = gdImageTrueColorPixel(im, x, y);
            *p++ = gdTrueColorGetRed(c);
            *p++ = gdTrueColorGetGreen(c);
            *p++ = gdTrueColorGetBlue(c);
        }
    }
    gdImageDestroy(im);
    return pixels;
}

#endif  /* HAVE_GD */


static void
arrange_pixelformat(unsigned char *pixels, int width, int height,
                    int comp, int stride)
{
    int x;
    int y;
    unsigned char *src;
    unsigned char *dst;
    size_t new_rowstride;

    src = dst = pixels;
    if (comp == 4) {
        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x++) {
                *(dst++) = *(src++);   /* R */
                *(dst++) = *(src++);   /* G */
                *(dst++) = *(src++);   /* B */
                src++;   /* A */
            }
        }
    }
    else {
        new_rowstride = width * 3;
        for (y = 1; y < height; y++) {
            memmove(dst += new_rowstride, src += stride, new_rowstride);
        }
    }
}


unsigned char *
load_image_file(char const *filename, int *psx, int *psy,
                int *pframe_count, int *ploop_count, int **ppdelay)
{
    unsigned char *pixels;
    int comp;
    int stride;
    chunk_t chunk;

    pixels = NULL;

    if (get_chunk(filename, &chunk) != 0) {
        return NULL;
    }

#ifdef HAVE_GDK_PIXBUF2
    if (!pixels) {
        pixels = load_with_gdkpixbuf(&chunk, psx, psy, &comp, &stride,
                                     pframe_count, ploop_count,ppdelay);
    }
#endif  /* HAVE_GDK_PIXBUF2 */
#if HAVE_GD
    if (!pixels) {
        pixels = load_with_gd(&chunk, psx, psy, &comp, &stride);
        *pframe_count = 1;
    }
#endif  /* HAVE_GD */
    if (!pixels) {
        pixels = load_with_builtin(&chunk, psx, psy, &comp, &stride,
                                   pframe_count, ploop_count, ppdelay);
    }
    free(chunk.buffer);

    if (pixels) {
        arrange_pixelformat(pixels, *psx, *psy * *pframe_count, comp, stride);
    }

    return pixels;
}

/* emacs, -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set expandtab ts=4 : */
/* EOF */
