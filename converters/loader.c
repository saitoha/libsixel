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

#define STBI_HEADER_FILE_ONLY 1

#if !defined(HAVE_MEMCPY)
# define memcpy(d, s, n) (bcopy ((s), (d), (n)))
#endif

#if !defined(HAVE_MEMMOVE)
# define memmove(d, s, n) (bcopy ((s), (d), (n)))
#endif

#if !defined(O_BINARY) && defined(_O_BINARY)
# define O_BINARY _O_BINARY
#endif  /* !defined(O_BINARY) && !defined(_O_BINARY) */

#include "stb_image.c"

#ifdef HAVE_GDK_PIXBUF2
# include <gdk-pixbuf/gdk-pixbuf.h>
#endif

#ifdef HAVE_GD
# include <gd.h>
#endif

#ifdef HAVE_LIBCURL
# include <curl/curl.h>
#endif

#include "loader.h"

typedef struct chunk
{
    unsigned char* buffer;
    size_t size;
    size_t max_size;
} chunk_t;


#ifdef HAVE_GDK_PIXBUF2
static size_t
loader_write(void *data, size_t size, size_t len, void *loader)
{
    gdk_pixbuf_loader_write(loader, data, len, NULL) ;
    return len;
}
#endif

size_t
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
get_chunk_from_file(char const *filename, chunk_t *chunk)
{
    FILE *f;
    int n;

    f = open_binary_file(filename);
    if (!f) {
        return (-1);
    }

    chunk->size = 0;
    chunk->max_size = 64 * 1024;

    if ((chunk->buffer = (unsigned char *)malloc(chunk->max_size)) == NULL) {
        return (-1);
    }

    for (;;) {
        if ((chunk->max_size - chunk->size) < 4096) {
            chunk->max_size *= 2;
            if ((chunk->buffer = (unsigned char *)realloc(chunk->buffer, chunk->max_size)) == NULL) {
                return (-1);
            }
        }
        if ((n = fread(chunk->buffer + chunk->size, 1, 4096, f)) <= 0) {
            break;
        }
        chunk->size += n;
    }

    if (f != stdout) {
        fclose(f);
    }

    return 0;
}


static int
chunk_is_sixel(chunk_t const *chunk)
{
    unsigned char *p;
    unsigned char *end;
    int result;

    result = 0;
    p = chunk->buffer;

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


static unsigned char *
load_with_stbi(char const *filename, int *psx, int *psy,
               int *pcomp, int *pstride)
{
    FILE *f;
    unsigned char *result;
    chunk_t chunk;
# ifdef HAVE_LIBCURL
    CURL *curl;
    CURLcode code;

    if (strstr(filename, "://")) {
        chunk.max_size = 1024;
        chunk.size = 0;
        chunk.buffer = malloc(chunk.max_size);
        curl = curl_easy_init();
        curl_easy_setopt(curl, CURLOPT_URL, filename);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        if (strncmp(filename, "https://", 8) == 0) {
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        }
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, memory_write);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        if ((code = curl_easy_perform(curl))) {
            fprintf(stderr, "curl_easy_perform('%s') failed.\n" "code: %d.\n",
                    filename, code);
            return NULL;
        }
        curl_easy_cleanup(curl);
    }
    else
# endif  /* HAVE_LIBCURL */
    {
        if (get_chunk_from_file(filename, &chunk) != 0) {
#if _ERRNO_H
            fprintf(stderr, "get_chunk_from_file('%s') failed.\n" "readon: %s.\n",
                    filename, strerror(errno));
#endif  /* HAVE_ERRNO_H */
            return NULL;
        }
    }

    if (chunk_is_sixel(&chunk)) {
        /* sixel */
    } else if (chunk_is_pnm(&chunk)) {
        /* pnm */
    } else {
        result = stbi_load_from_memory(chunk.buffer, chunk.size, psx, psy, pcomp, STBI_rgb);
        if (!result) {
            fprintf(stderr, "stbi_load_from_file('%s') failed.\n" "reason: %s.\n",
                    filename, stbi_failure_reason());
            return NULL;
        }
    }
    free(chunk.buffer);

    /* 4 is set in *pcomp when source image is GIF. we reset it to 3. */
    *pcomp = 3;

    *pstride = *pcomp * *psx;
    return result;
}


#ifdef HAVE_GDK_PIXBUF2
static unsigned char *
load_with_gdkpixbuf(char const *filename, int *psx, int *psy, int *pcomp, int *pstride)
{
    GdkPixbuf *pixbuf;
    unsigned char *pixels;

# ifdef HAVE_LIBCURL
    if (strstr(filename, "://")) {
        CURL *curl;
        GdkPixbufLoader *loader;

        loader = gdk_pixbuf_loader_new();

        curl = curl_easy_init();
        curl_easy_setopt(curl, CURLOPT_URL, filename);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        if (strncmp(filename, "https://", 8) == 0) {
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        }
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, loader_write);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, loader);
        curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        gdk_pixbuf_loader_close(loader, NULL);

        if ((pixbuf = gdk_pixbuf_loader_get_pixbuf(loader))) {
            g_object_ref(pixbuf);
        }

        g_object_unref(loader);
    }
    else
# endif  /* HAVE_LIBCURL */
    {
        pixbuf = gdk_pixbuf_new_from_file(filename, NULL);
    }

    if (pixbuf == NULL) {
        pixels = NULL;
    }
    else {
        *psx = gdk_pixbuf_get_width(pixbuf);
        *psy = gdk_pixbuf_get_height(pixbuf);
        *pcomp = gdk_pixbuf_get_has_alpha(pixbuf) ? 4: 3;
        *pstride = gdk_pixbuf_get_rowstride(pixbuf);
        pixels = gdk_pixbuf_get_pixels(pixbuf);
    }
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
load_with_gd(char const *filename, int *psx, int *psy, int *pcomp, int *pstride)
{
    unsigned char *pixels, *p;
    int n, len, max;
    unsigned char *data;
    gdImagePtr im;
    FILE *f;
    int x, y;
    int c;
# ifdef HAVE_LIBCURL
    CURL *curl;
    CURLcode code;
    chunk_t chunk;

    if (strstr(filename, "://")) {
        chunk.max_size = 1024;
        chunk.size = 0;
        chunk.buffer = malloc(chunk.max_size);
        curl = curl_easy_init();
        curl_easy_setopt(curl, CURLOPT_URL, filename);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        if (strncmp(filename, "https://", 8) == 0) {
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        }
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, memory_write);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        if ((code = curl_easy_perform(curl))) {
            fprintf(stderr, "curl_easy_perform('%s') failed.\n" "code: %d.\n",
                    filename, code);
            return NULL;
        }
        curl_easy_cleanup(curl);
    }
    else
# endif  /* HAVE_LIBCURL */
    {
        if (get_chunk_from_file(filename, &chunk) != 0) {
#if _ERRNO_H
            fprintf(stderr, "get_chunk_from_file('%s') failed.\n" "readon: %s.\n",
                    filename, strerror(errno));
#endif  /* HAVE_ERRNO_H */
            return NULL;
        }
    }

    switch(detect_file_format(chunk.size, chunk.buffer)) {
        case FMT_GIF:
            im = gdImageCreateFromGifPtr(len, data);
            break;
        case FMT_PNG:
            im = gdImageCreateFromPngPtr(len, data);
            break;
        case FMT_BMP:
            im = gdImageCreateFromBmpPtr(len, data);
            break;
        case FMT_JPG:
            im = gdImageCreateFromJpegPtrEx(len, data, 1);
            break;
        case FMT_TGA:
            im = gdImageCreateFromTgaPtr(len, data);
            break;
        case FMT_WBMP:
            im = gdImageCreateFromWBMPPtr(len, data);
            break;
        case FMT_TIFF:
            im = gdImageCreateFromTiffPtr(len, data);
            break;
        case FMT_GD2:
            im = gdImageCreateFromGd2Ptr(len, data);
            break;
        default:
            return NULL;
    }

    free(data);

    if (im == NULL) {
        return NULL;
    }

    if (!gdImageTrueColor(im)) {
        if (!gdImagePaletteToTrueColor(im)) {
            return NULL;
        }
    }

    *psx = gdImageSX(im);
    *psy = gdImageSY(im);
    *pcomp = 3;
    *pstride = *psx * *pcomp;
    p = pixels = malloc(*pstride * *psy);
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


unsigned char *
load_image_file(char const *filename, int *psx, int *psy)
{
    unsigned char *pixels;
    size_t new_rowstride;
    unsigned char *src;
    unsigned char *dst;
    int comp;
    int stride;
    int x;
    int y;

    pixels = NULL;
#ifdef HAVE_GDK_PIXBUF2
    if (!pixels) {
        pixels = load_with_gdkpixbuf(filename, psx, psy, &comp, &stride);
    }
#endif  /* HAVE_GDK_PIXBUF2 */
#if HAVE_GD
    if (!pixels) {
        pixels = load_with_gd(filename, psx, psy, &comp, &stride);
    }
#endif  /* HAVE_GD */
    if (!pixels) {
        pixels = load_with_stbi(filename, psx, psy, &comp, &stride);
    }

    src = dst = pixels;
    if (comp == 4) {
        for (y = 0; y < *psy; y++) {
            for (x = 0; x < *psx; x++) {
                *(dst++) = *(src++);   /* R */
                *(dst++) = *(src++);   /* G */
                *(dst++) = *(src++);   /* B */
                src++;   /* A */
            }
        }
    }
    else {
        new_rowstride = *psx * 3;
        for (y = 1; y < *psy; y++) {
            memmove(dst += new_rowstride, src += stride, new_rowstride);
        }
    }
    return pixels;
}

/* emacs, -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set expandtab ts=4 : */
/* EOF */
