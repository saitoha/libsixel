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

#include <stdio.h>
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

#if defined(HAVE_UNISTD_H)
# include <unistd.h>  /* getopt */
#endif

#if defined(HAVE_GETOPT_H)
# include <getopt.h>
#endif

#if defined(HAVE_INTTYPES_H)
# include <inttypes.h>
#endif

#if defined(HAVE_ERRNO_H)
# include <errno.h>
#endif

#include <sixel.h>

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
#include "scale.h"
#include "quant.h"

#ifdef HAVE_GDK_PIXBUF2
# include <gdk-pixbuf/gdk-pixbuf.h>
#endif

#ifdef HAVE_LIBCURL
# include <curl/curl.h>
#endif

#ifdef HAVE_GDK_PIXBUF2
static size_t
loader_write(void *data, size_t size, size_t len, void *loader)
{
    gdk_pixbuf_loader_write(loader, data, len, NULL) ;
    return len;
}
#endif

typedef struct chunk
{
    unsigned char* buffer;
    size_t size;
    size_t max_size;
} chunk_t;


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


static unsigned char *
load_with_stbi(char const *filename, int *psx, int *psy,
               int *pcomp, int *pstride)
{
    FILE *f;
    unsigned char *result;
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

        result = stbi_load_from_memory(chunk.buffer, chunk.size, psx, psy, pcomp, STBI_rgb);
        free(chunk.buffer);
    }
    else
# endif  /* HAVE_LIBCURL */
    {
        f = open_binary_file(filename);
        if (!f) {
            return NULL;
        }
        result = stbi_load_from_file(f, psx, psy, pcomp, STBI_rgb);
        if (!result) {
            fprintf(stderr, "stbi_load_from_file('%s') failed.\n" "reason: %s.\n",
                    filename, stbi_failure_reason());
            return NULL;
        }
        fclose(f);
    }

    *pstride = *pcomp * *psx;
    return result;
}


#ifdef HAVE_GDK_PIXBUF2
static unsigned char *
load_with_gdk_and_curl(char const *filename, int *psx, int *psy, int *pcomp, int *pstride)
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
#include <gd.h>

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

static int
detect_file_format(int len, unsigned char *data)
{
    if (memcmp("TRUEVISION", data + len - 18, 10) == 0)
        return FMT_TGA;

    if (memcmp("GIF", data, 3) == 0)
        return FMT_GIF;

    if (memcmp("\x89\x50\x4E\x47\x0D\x0A\x1A\x0A", data, 8) == 0)
        return FMT_PNG;

    if (memcmp("BM", data, 2) == 0)
        return FMT_BMP;

    if (memcmp("\xFF\xD8", data, 2) == 0)
        return FMT_JPG;

    if (memcmp("\x00\x00", data, 2) == 0)
        return FMT_WBMP;

    if (memcmp("\x4D\x4D", data, 2) == 0)
        return FMT_TIFF;

    if (memcmp("\x49\x49", data, 2) == 0)
        return FMT_TIFF;

    if (memcmp("\033P", data, 2) == 0)
        return FMT_SIXEL;

    if (data[0] == 0x90  && (data[len-1] == 0x9C || data[len-2] == 0x9C))
        return FMT_SIXEL;

    if (data[0] == 'P' && data[1] >= '1' && data[1] <= '6')
        return FMT_PNM;

    if (memcmp("gd2", data, 3) == 0)
        return FMT_GD2;

    return (-1);
}


static unsigned char *
load_with_gd(char const *filename, int *psx, int *psy, int *pcomp, int *pstride)
{
    unsigned char *pixels, *p;
    int n, len, max;
    unsigned char *data;
    gdImagePtr im = NULL;
    FILE *f;
    int x, y;
    int c;

    f = open_binary_file(filename);
    if (!f) {
        return NULL;
    }

    len = 0;
    max = 64 * 1024;

    if ((data = (unsigned char *)malloc(max)) == NULL) {
        return NULL;
    }

    for (;;) {
        if ((max - len) < 4096) {
            max *= 2;
            if ((data = (unsigned char *)realloc(data, max)) == NULL) {
                return NULL;
            }
        }
        if ((n = fread(data + len, 1, 4096, f)) <= 0) {
            break;
        }
        len += n;
    }

    if (f != stdout) {
        fclose(f);
    }

    switch(detect_file_format(len, data)) {
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

static unsigned char *
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

#ifdef HAVE_GDK_PIXBUF2
    pixels = load_with_gdk_and_curl(filename, psx, psy, &comp, &stride);
#elif HAVE_GD
    pixels = load_with_gd(filename, psx, psy, &comp, &stride);
#else
    pixels = load_with_stbi(filename, psx, psy, &comp, &stride);
#endif  /* HAVE_GDK_PIXBUF2 */

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


static unsigned char *
prepare_monochrome_palette(finvert)
{
    unsigned char *palette;

    palette = malloc(6);
    if (finvert) {
        palette[0] = 0xff;
        palette[1] = 0xff;
        palette[2] = 0xff;
        palette[3] = 0x00;
        palette[4] = 0x00;
        palette[5] = 0x00;
    } else {
        palette[0] = 0x00;
        palette[1] = 0x00;
        palette[2] = 0x00;
        palette[3] = 0xff;
        palette[4] = 0xff;
        palette[5] = 0xff;
    }

    return palette;
}


static unsigned char *
prepare_specified_palette(char const *mapfile, int reqcolors, int *pncolors)
{
    FILE *f;
    unsigned char *mappixels;
    unsigned char *palette;
    int origcolors;
    int map_sx;
    int map_sy;

    mappixels = load_image_file(mapfile, &map_sx, &map_sy);
    if (!mappixels) {
        return NULL;
    }
    palette = LSQ_MakePalette(mappixels, map_sx, map_sy, 3,
                              reqcolors, pncolors, &origcolors,
                              LARGE_NORM, REP_CENTER_BOX, QUALITY_HIGH);
    return palette;
}


static int
convert_to_sixel(char const *filename, int reqcolors,
                 char const *mapfile, int monochrome,
                 enum methodForDiffuse method_for_diffuse,
                 enum methodForLargest method_for_largest,
                 enum methodForRep method_for_rep,
                 enum qualityMode quality_mode,
                 enum methodForResampling const method_for_resampling,
                 int f8bit, int finvert,
                 int pixelwidth, int pixelheight,
                 int percentwidth, int percentheight)
{
    unsigned char *pixels = NULL;
    unsigned char *scaled_pixels = NULL;
    unsigned char *mappixels = NULL;
    unsigned char *palette = NULL;
    unsigned char *data = NULL;
    int ncolors;
    int origcolors;
    LSImagePtr im = NULL;
    LSOutputContextPtr context = NULL;
    int sx, sy, comp;
    int i;
    int nret = -1;
    FILE *f;

    if (reqcolors < 2) {
        reqcolors = 2;
    } else if (reqcolors > PALETTE_MAX) {
        reqcolors = PALETTE_MAX;
    }

    pixels = load_image_file(filename, &sx, &sy);
    if (pixels == NULL) {
        nret = -1;
        goto end;
    }
    /* scaling */
    if (percentwidth > 0) {
        pixelwidth = sx * percentwidth / 100;
    }
    if (percentheight > 0) {
        pixelheight = sy * percentheight / 100;
    }
    if (pixelwidth > 0 && pixelheight <= 0) {
        pixelheight = sy * pixelwidth / sx;
    }
    if (pixelheight > 0 && pixelwidth <= 0) {
        pixelwidth = sx * pixelheight / sy;
    }

    if (pixelwidth > 0 && pixelheight > 0) {
        scaled_pixels = LSS_scale(pixels, sx, sy, 3,
                                  pixelwidth, pixelheight,
                                  method_for_resampling);
        sx = pixelwidth;
        sy = pixelheight;

        free(pixels);
        pixels = scaled_pixels;
    }

    /* prepare palette */
    if (monochrome) {
        palette = prepare_monochrome_palette(finvert);
        ncolors = 2;
    } else if (mapfile) {
        palette = prepare_specified_palette(mapfile, reqcolors, &ncolors);
    } else {
        if (method_for_largest == LARGE_AUTO) {
            method_for_largest = LARGE_NORM;
        }
        if (method_for_rep == REP_AUTO) {
            method_for_rep = REP_CENTER_BOX;
        }
        if (quality_mode == QUALITY_AUTO) {
            quality_mode = reqcolors <= 8 ? QUALITY_HIGH: QUALITY_LOW;
        }
        palette = LSQ_MakePalette(pixels, sx, sy, 3,
                                  reqcolors, &ncolors, &origcolors,
                                  method_for_largest,
                                  method_for_rep,
                                  quality_mode);
        if (origcolors <= ncolors) {
            method_for_diffuse = DIFFUSE_NONE;
        }
    }

    if (!palette) {
        nret = -1;
        goto end;
    }

    /* apply palette */
    if (method_for_diffuse == DIFFUSE_AUTO) {
        method_for_diffuse = DIFFUSE_FS;
    }
    data = LSQ_ApplyPalette(pixels, sx, sy, 3,
                            palette, ncolors,
                            method_for_diffuse,
                            /* foptimize */ 1);

    if (!data) {
        nret = -1;
        goto end;
    }

    /* create intermidiate bitmap image */
    im = LSImage_create(sx, sy, 3, ncolors);
    if (!im) {
        nret = -1;
        goto end;
    }
    for (i = 0; i < ncolors; i++) {
        LSImage_setpalette(im, i,
                           palette[i * 3],
                           palette[i * 3 + 1],
                           palette[i * 3 + 2]);
    }
    if (monochrome) {
        im->keycolor = 0;
    } else {
        im->keycolor = -1;
    }
    LSImage_setpixels(im, data);

    data = NULL;

    /* convert image object into sixel */
    context = LSOutputContext_create(putchar, printf);
    context->has_8bit_control = f8bit;
    LibSixel_LSImageToSixel(im, context);

    nret = 0;

end:
    if (data) {
        free(data);
    }
    if (pixels) {
        stbi_image_free(pixels);
    }
    if (mappixels) {
        stbi_image_free(mappixels);
    }
    if (palette) {
        LSQ_FreePalette(palette);
    }
    if (im) {
        LSImage_destroy(im);
    }
    if (context) {
        LSOutputContext_destroy(context);
    }
    return nret;
}


int main(int argc, char *argv[])
{
    int n;
    int filecount = 1;
    int ncolors = -1;
    int monochrome = 0;
    enum methodForResampling method_for_resampling = RES_BILINEAR;
    enum methodForDiffuse method_for_diffuse = DIFFUSE_AUTO;
    enum methodForLargest method_for_largest = LARGE_AUTO;
    enum methodForRep method_for_rep = REP_AUTO;
    enum qualityMode quality_mode = QUALITY_AUTO;
    char *mapfile = NULL;
    int long_opt;
    int option_index;
    int ret;
    int exit_code;
    int f8bit;
    int finvert;
    int number;
    char unit[32];
    int parsed;
    int pixelwidth;
    int pixelheight;
    int percentwidth;
    int percentheight;

    f8bit = 0;
    finvert = 0;
    pixelwidth = -1;
    pixelheight = -1;
    percentwidth = -1;
    percentheight = -1;

    struct option long_options[] = {
        {"7bit-mode",    no_argument,        &long_opt, '7'},
        {"8bit-mode",    no_argument,        &long_opt, '8'},
        {"colors",       required_argument,  &long_opt, 'p'},
        {"mapfile",      required_argument,  &long_opt, 'm'},
        {"monochrome",   no_argument,        &long_opt, 'e'},
        {"diffusion",    required_argument,  &long_opt, 'd'},
        {"find-largest", required_argument,  &long_opt, 'f'},
        {"select-color", required_argument,  &long_opt, 's'},
        {"width",        required_argument,  &long_opt, 'w'},
        {"height",       required_argument,  &long_opt, 'h'},
        {"resampling",   required_argument,  &long_opt, 'r'},
        {"quality",      required_argument,  &long_opt, 'q'},
        {"invert",       required_argument,  &long_opt, 'i'},
        {0, 0, 0, 0}
    };

    for (;;) {
        n = getopt_long(argc, argv, "78p:m:ed:f:s:w:h:r:q:i",
                        long_options, &option_index);
        if (n == -1) {
            break;
        }
        if (n == 0) {
            n = long_opt;
        }
        switch(n) {
        case '7':
            f8bit = 0;
            break;
        case '8':
            f8bit = 1;
            break;
        case 'p':
            ncolors = atoi(optarg);
            break;
        case 'm':
            mapfile = strdup(optarg);
            break;
        case 'e':
            monochrome = 1;
            break;
        case 'd':
            /* parse --diffusion option */
            if (optarg) {
                if (strcmp(optarg, "auto") == 0) {
                    method_for_diffuse = DIFFUSE_AUTO;
                } else if (strcmp(optarg, "none") == 0) {
                    method_for_diffuse = DIFFUSE_NONE;
                } else if (strcmp(optarg, "fs") == 0) {
                    method_for_diffuse = DIFFUSE_FS;
                } else if (strcmp(optarg, "atkinson") == 0) {
                    method_for_diffuse = DIFFUSE_ATKINSON;
                } else if (strcmp(optarg, "jajuni") == 0) {
                    method_for_diffuse = DIFFUSE_JAJUNI;
                } else if (strcmp(optarg, "stucki") == 0) {
                    method_for_diffuse = DIFFUSE_STUCKI;
                } else if (strcmp(optarg, "burkes") == 0) {
                    method_for_diffuse = DIFFUSE_BURKES;
                } else {
                    fprintf(stderr,
                            "Diffusion method '%s' is not supported.\n",
                            optarg);
                    goto argerr;
                }
            }
            break;
        case 'f':
            /* parse --find-largest option */
            if (optarg) {
                if (strcmp(optarg, "auto") == 0) {
                    method_for_largest = LARGE_AUTO;
                } else if (strcmp(optarg, "norm") == 0) {
                    method_for_largest = LARGE_NORM;
                } else if (strcmp(optarg, "lum") == 0) {
                    method_for_largest = LARGE_LUM;
                } else {
                    fprintf(stderr,
                            "Finding method '%s' is not supported.\n",
                            optarg);
                    goto argerr;
                }
            }
            break;
        case 's':
            /* parse --select-color option */
            if (optarg) {
                if (strcmp(optarg, "auto") == 0) {
                    method_for_rep = REP_AUTO;
                } else if (strcmp(optarg, "center") == 0) {
                    method_for_rep = REP_CENTER_BOX;
                } else if (strcmp(optarg, "average") == 0) {
                    method_for_rep = REP_AVERAGE_COLORS;
                } else if (strcmp(optarg, "histgram") == 0) {
                    method_for_rep = REP_AVERAGE_PIXELS;
                } else {
                    fprintf(stderr,
                            "Finding method '%s' is not supported.\n",
                            optarg);
                    goto argerr;
                }
            }
            break;
        case 'w':
            parsed = sscanf(optarg, "%d%s", &number, unit);
            if (parsed == 2 && strcmp(unit, "%") == 0) {
                pixelwidth = -1;
                percentwidth = number;
            } else if (parsed == 1 || (parsed == 2 && strcmp(unit, "px") == 0)) {
                pixelwidth = number;
                percentwidth = -1;
            } else if (strcmp(optarg, "auto") == 0) {
                pixelwidth = -1;
                percentwidth = -1;
            } else {
                fprintf(stderr,
                        "Cannot parse -w/--width option.\n");
                goto argerr;
            }
            break;
        case 'h':
            parsed = sscanf(optarg, "%d%s", &number, unit);
            if (parsed == 2 && strcmp(unit, "%") == 0) {
                pixelheight = -1;
                percentheight = number;
            } else if (parsed == 1 || (parsed == 2 && strcmp(unit, "px") == 0)) {
                pixelheight = number;
                percentheight = -1;
            } else if (strcmp(optarg, "auto") == 0) {
                pixelheight = -1;
                percentheight = -1;
            } else {
                fprintf(stderr,
                        "Cannot parse -h/--height option.\n");
                goto argerr;
            }
            break;
        case 'r':
            /* parse --resampling option */
            if (!optarg) {  /* default */
                method_for_resampling = RES_BILINEAR;
            } else if (strcmp(optarg, "nearest") == 0) {
                method_for_resampling = RES_NEAREST;
            } else if (strcmp(optarg, "gaussian") == 0) {
                method_for_resampling = RES_GAUSSIAN;
            } else if (strcmp(optarg, "hanning") == 0) {
                method_for_resampling = RES_HANNING;
            } else if (strcmp(optarg, "hamming") == 0) {
                method_for_resampling = RES_HAMMING;
            } else if (strcmp(optarg, "bilinear") == 0) {
                method_for_resampling = RES_BILINEAR;
            } else if (strcmp(optarg, "welsh") == 0) {
                method_for_resampling = RES_WELSH;
            } else if (strcmp(optarg, "bicubic") == 0) {
                method_for_resampling = RES_BICUBIC;
            } else if (strcmp(optarg, "lanczos2") == 0) {
                method_for_resampling = RES_LANCZOS2;
            } else if (strcmp(optarg, "lanczos3") == 0) {
                method_for_resampling = RES_LANCZOS3;
            } else if (strcmp(optarg, "lanczos4") == 0) {
                method_for_resampling = RES_LANCZOS4;
            } else {
                fprintf(stderr,
                        "Resampling method '%s' is not supported.\n",
                        optarg);
                goto argerr;
            }
            break;
        case 'q':
            /* parse --quality option */
            if (optarg) {
                if (strcmp(optarg, "auto") == 0) {
                    quality_mode = QUALITY_AUTO;
                } else if (strcmp(optarg, "high") == 0) {
                    quality_mode = QUALITY_HIGH;
                } else if (strcmp(optarg, "hanning") == 0) {
                    quality_mode = QUALITY_LOW;
                } else {
                    fprintf(stderr,
                            "Cannot parse quality option.\n");
                    goto argerr;
                }
            }
            break;
        case 'i':
            finvert = 1;
            break;
        case '?':
            goto argerr;
        default:
            goto argerr;
        }
    }
    if (ncolors != -1 && mapfile) {
        fprintf(stderr, "option -p, --colors conflicts "
                        "with -m, --mapfile.\n");
        goto argerr;
    }
    if (mapfile && monochrome) {
        fprintf(stderr, "option -m, --mapfile conflicts "
                        "with -e, --monochrome.\n");
        goto argerr;
    }
    if (monochrome && ncolors != -1) {
        fprintf(stderr, "option -e, --monochrome conflicts"
                        " with -p, --colors.\n");
        goto argerr;
    }

    if (ncolors == -1) {
        ncolors = PALETTE_MAX;
    }

    if (optind == argc) {
        ret = convert_to_sixel(NULL, ncolors, mapfile,
                               monochrome,
                               method_for_diffuse,
                               method_for_largest,
                               method_for_rep,
                               quality_mode,
                               method_for_resampling,
                               f8bit, finvert,
                               pixelwidth, pixelheight,
                               percentwidth, percentheight);
        if (ret != 0) {
            exit_code = EXIT_FAILURE;
            goto end;
        }
    } else {
        for (n = optind; n < argc; n++) {
            ret = convert_to_sixel(argv[n], ncolors, mapfile,
                                   monochrome,
                                   method_for_diffuse,
                                   method_for_largest,
                                   method_for_rep,
                                   quality_mode,
                                   method_for_resampling,
                                   f8bit, finvert,
                                   pixelwidth, pixelheight,
                                   percentwidth, percentheight);
            if (ret != 0) {
                exit_code = EXIT_FAILURE;
                goto end;
            }
        }
    }
    exit_code = EXIT_SUCCESS;
    goto end;

argerr:
    exit_code = EXIT_FAILURE;
    fprintf(stderr,
            "Usage: img2sixel [Options] imagefiles\n"
            "       img2sixel [Options] < imagefile\n"
            "\n"
            "Options:\n"
            "-7, --7bit-mode            generate a sixel image for 7bit\n"
            "                           terminals or printers (default)\n"
            "-8, --8bit-mode            generate a sixel image for 8bit\n"
            "                           terminals or printers\n"
            "-p COLORS, --colors=COLORS specify number of colors to reduce\n"
            "                           the image to (default=256)\n"
            "-m FILE, --mapfile=FILE    transform image colors to match this\n"
            "                           set of colorsspecify map\n"
            "-e, --monochrome           output monochrome sixel image\n"
            "                           this option assumes the terminal \n"
            "                           background color is black\n"
            "-i, --invert               assume the terminal background color\n"
            "                           is white, make sense only when -e\n"
            "                           option is given.\n"
            "-d DIFFUSIONTYPE, --diffusion=DIFFUSIONTYPE\n"
            "                           choose diffusion method which used\n"
            "                           with -p option (color reduction)\n"
            "                           DIFFUSIONTYPE is one of them:\n"
            "                             auto     -> choose diffusion type\n"
            "                                         automatically (default)\n"
            "                             none     -> do not diffuse\n"
            "                             fs       -> Floyd-Steinberg method\n"
            "                             atkinson -> Bill Atkinson's method\n"
            "                             jajuni   -> Jarvis, Judice & Ninke\n"
            "                             stucki   -> Stucki's method\n"
            "                             burkes   -> Burkes' method\n"
            "-f FINDTYPE, --find-largest=FINDTYPE\n"
            "                           choose method for finding the largest\n"
            "                           dimention of median cut boxes for\n"
            "                           splitting, make sense only when -p\n"
            "                           option (color reduction) is\n"
            "                           specified\n"
            "                           FINDTYPE is one of them:\n"
            "                             auto -> choose finding method\n"
            "                                     automatically (default)\n"
            "                             norm -> simply comparing the\n"
            "                                     range in RGB space\n"
            "                             lum  -> transforming into\n"
            "                                     luminosities before the\n"
            "                                     comparison\n"
            "-s SELECTTYPE, --select-color=SELECTTYPE\n"
            "                           choose the method for selecting\n"
            "                           representative color from each\n"
            "                           median-cut box, make sense only\n"
            "                           when -p option (color reduction) is\n"
            "                           specified\n"
            "                           SELECTTYPE is one of them:\n"
            "                             auto     -> choose selecting\n"
            "                                         method automatically\n"
            "                                         (default)\n"
            "                             center   -> choose the center of\n"
            "                                         the box\n"
            "                             average  -> caclulate the color\n"
            "                                         average into the box\n"
            "                             histgram -> similar with average\n"
            "                                         but considers color\n"
            "                                         histgram\n"
            "-w WIDTH, --width=WIDTH    resize image to specific width\n"
            "                           WIDTH is represented by the\n"
            "                           following syntax\n"
            "                             auto       -> preserving aspect\n"
            "                                           ratio (default)\n"
            "                             <number>%%  -> scale width with\n"
            "                                           given percentage\n"
            "                             <number>   -> scale width with\n"
            "                                           pixel counts\n"
            "                             <number>px -> scale width with\n"
            "                                           pixel counts\n"
            "-h HEIGHT, --height=HEIGHT resize image to specific height\n"
            "                           HEIGHT is represented by the\n"
            "                           following syntax\n"
            "                             auto       -> preserving aspect\n"
            "                                           ratio (default)\n"
            "                             <number>%%  -> scale height with\n"
            "                                           given percentage\n"
            "                             <number>   -> scale height with\n"
            "                                           pixel counts\n"
            "                             <number>px -> scale height with\n"
            "                                           pixel counts\n"
            "-r RESAMPLINGTYPE, --resampling=RESAMPLINGTYPE\n"
            "                           choose resampling filter used\n"
            "                           with -w or -h option (scaling)\n"
            "                           RESAMPLINGTYPE is one of them:\n"
            "                             nearest  -> Nearest-Neighbor\n"
            "                                         method\n"
            "                             gaussian -> Gaussian filter\n"
            "                             hanning  -> Hanning filter\n"
            "                             hamming  -> Hamming filter\n"
            "                             bilinear -> Bilinear filter\n"
            "                                         (default)\n"
            "                             welsh    -> Welsh filter\n"
            "                             bicubic  -> Bicubic filter\n"
            "                             lanczos2 -> Lanczos-2 filter\n"
            "                             lanczos3 -> Lanczos-3 filter\n"
            "                             lanczos4 -> Lanczos-4 filter\n"
            "-q QUALITYMODE, --quality=QUALITYMODE\n"
            "                           select quality of color\n"
            "                           quanlization.\n"
            "                             auto -> decide quality mode\n"
            "                                     automatically (default)\n"
            "                             high -> high quality and low\n"
            "                                     speed mode\n"
            "                             low  -> low quality and high\n"
            "                                     speed mode\n"
            );

end:
    if (mapfile) {
        free(mapfile);
    }
    return exit_code;
}

/* emacs, -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set expandtab ts=4 : */
/* EOF */
