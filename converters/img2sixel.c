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

#if defined(HAVE_UNISTD_H)
# include <unistd.h>  /* getopt */
#endif

#if defined(HAVE_GETOPT_H)
# include <getopt.h>
#endif

#if defined(HAVE_INTTYPES_H)
# include <inttypes.h>
#endif

#include <sixel.h>

#define STBI_HEADER_FILE_ONLY

#if !defined(HAVE_MEMCPY)
# define memcpy(d, s, n) (bcopy ((s), (d), (n)))
#endif

#if !defined(HAVE_MEMMOVE)
# define memmove(d, s, n) (bcopy ((s), (d), (n)))
#endif

#include "stb_image.c"

#include "quant.h"

static int
convert_to_sixel(char const *filename, int reqcolors,
                 const char *mapfile, int monochrome)
{
    uint8_t *pixels = NULL;
    uint8_t *mappixels = NULL;
    uint8_t *palette = NULL;
    uint8_t *data = NULL;
    int ncolors;
    int origcolors;
    LSImagePtr im = NULL;
    LSOutputContextPtr context = NULL;
    int sx, sy, comp;
    int map_sx, map_sy, map_comp;
    int i;
    int nret = -1;
    enum methodForDiffuse method_for_diffuse = DIFFUSE_NONE;

    if ( reqcolors < 2 ) {
        reqcolors = 2;
    } else if ( reqcolors > PALETTE_MAX ) {
        reqcolors = PALETTE_MAX;
    }

    pixels = stbi_load(filename, &sx, &sy, &comp, STBI_rgb);
    if (pixels == NULL) {
        fprintf(stderr, "stbi_load('%s') failed.\n", filename);
        nret = -1;
        return (-1);
    }

    if (monochrome) {
        palette = malloc(6);
        palette[0] = 0x00;
        palette[1] = 0x00;
        palette[2] = 0x00;
        palette[3] = 0xff;
        palette[4] = 0xff;
        palette[5] = 0xff;
        ncolors = 2;
        method_for_diffuse = DIFFUSE_FS;
    } else if (mapfile) {
        mappixels = stbi_load(mapfile, &map_sx, &map_sy, &map_comp, STBI_rgb);
        if (!mappixels) {
            fprintf(stderr, "stbi_load('%s') failed.\n", mapfile);
            nret = -1;
            goto end;
        }
        palette = LSQ_MakePalette(mappixels, map_sx, map_sy, 3,
                                  reqcolors, &ncolors, &origcolors,
                                  LARGE_NORM, REP_CENTER_BOX);
        method_for_diffuse = DIFFUSE_FS;
    } else {
        palette = LSQ_MakePalette(pixels, sx, sy, 3,
                                  reqcolors, &ncolors, &origcolors,
                                  LARGE_NORM, REP_CENTER_BOX);
        if (origcolors > ncolors) {
            method_for_diffuse = DIFFUSE_FS;
        } else {
            method_for_diffuse = DIFFUSE_NONE;
        }
    }

    if (!palette) {
        nret = -1;
        goto end;
    }

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
    data = LSQ_ApplyPalette(pixels, sx, sy, 3,
                            palette, ncolors,
                            method_for_diffuse);
    if (!data) {
        nret = -1;
        goto end;
    }
    LSImage_setpixels(im, data);
    data = NULL;
    context = LSOutputContext_create(putchar, printf);
    LibSixel_LSImageToSixel(im, context);

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
    char *mapfile = NULL;
    int long_opt;
    int option_index;

    struct option long_options[] = {
        {"colors",       required_argument,  &long_opt, 'p'},
        {"mapfile",      required_argument,  &long_opt, 'm'},
        {"monochrome",   no_argument,        &long_opt, 'e'},
        {0, 0, 0, 0}
    };

    for (;;) {
        n = getopt_long(argc, argv, "p:m:e",
                        long_options, &option_index);
        if (n == -1) {
            break;
        }
        if (n == 0) {
            n = long_opt;
        }
        switch(n) {
        case 'p':
            ncolors = atoi(optarg);
            break;
        case 'm':
            mapfile = strdup(optarg);
            break;
        case 'e':
            monochrome = 1;
            break;
        case '?':
            goto argerr;
        default:
            goto argerr;
        }
    }

    if (ncolors != -1 && mapfile) {
        fprintf(stderr, "option -p, --colors conflicts with -m, --mapfile.\n");
        goto argerr;
    }
    if (mapfile && monochrome) {
        fprintf(stderr, "option -m, --mapfile conflicts with -e, --monochrome.\n");
        goto argerr;
    }
    if (monochrome && ncolors != -1) {
        fprintf(stderr, "option -e, --monochrome conflicts with -p, --colors.\n");
        goto argerr;
    }

    if (ncolors == -1) {
        ncolors = PALETTE_MAX;
    }

    if (optind == argc) {
        convert_to_sixel("/dev/stdin", ncolors, mapfile, monochrome);
    } else {
        for (n = optind; n < argc; n++) {
            convert_to_sixel(argv[n], ncolors, mapfile, monochrome);
        }
    }
    goto end;

argerr:
    fprintf(stderr,
            "Usage: img2sixel [Options] imagefiles\n"
            "       img2sixel [Options] < imagefile\n"
            "\n"
            "Options:\n"
            "-p, --colors       specify number of colors to reduce the image to\n"
            "-m, --mapfile      transform image colors to match this set of colorsspecify map\n"
            "-e, --monochrome   output monochrome sixel image\n");

end:
    if (mapfile) {
        free(mapfile);
    }
    return 0;
}

/* emacs, -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set expandtab ts=4 : */
/* EOF */
