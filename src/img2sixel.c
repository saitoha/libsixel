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
convert_to_sixel(char const *filename, int reqcolors, const char *mapfile)
{
    uint8_t *pixels = NULL;
    uint8_t *mappixels = NULL;
    uint8_t *palette = NULL;
    uint8_t *data = NULL;
    int ncolors;
    LSImagePtr im = NULL;
    LSOutputContextPtr context = NULL;
    int sx, sy, comp;
    int map_sx, map_sy, map_comp;
    int i;
    int nret = -1;

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

    if (mapfile) {
        mappixels = stbi_load(mapfile, &map_sx, &map_sy, &map_comp, STBI_rgb);
        if (!mappixels) {
            fprintf(stderr, "stbi_load('%s') failed.\n", mapfile);
            nret = -1;
            goto end;
        }
        palette = LSQ_MakePalette(mappixels, map_sx, map_sy, 3, reqcolors, &ncolors, LARGE_NORM, REP_CENTER_BOX);
    } else {
        palette = LSQ_MakePalette(pixels, sx, sy, 3, reqcolors, &ncolors, LARGE_NORM, REP_CENTER_BOX);
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
        LSImage_setpalette(im, i, palette[i * 3], palette[i * 3 + 1], palette[i * 3 + 2]);
    }
    data = LSQ_ApplyPalette(pixels, sx, sy, 3, palette, ncolors, DIFFUSE_FS);
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
    char *mapfile = NULL;

    while ((n = getopt(argc, argv, "p:m:")) != -1) {
        switch(n) {
        case 'p':
            ncolors = atoi(optarg);
            break;
        case 'm':
            mapfile = strdup(optarg);
            break;
        default:
            goto argerr;
        }
    }

    if (ncolors != -1 && mapfile) {
        fprintf(stderr, "option -p conflicts with -m.\n");
        goto argerr;
    }

    if (ncolors == -1) {
        ncolors = PALETTE_MAX;
    }

    if (optind == argc) {
        convert_to_sixel("/dev/stdin", ncolors, mapfile);
    } else {
        for (n = optind; n < argc; n++) {
            convert_to_sixel(argv[n], ncolors, mapfile);
        }
    }
    goto end;

argerr:
    fprintf(stderr, "Usage: %s [-p MaxPalet] [-m PaletFile] <file name...>\n", argv[0]);

end:
    if (mapfile) {
        free(mapfile);
    }
    return 0;
}

/* emacs, -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set expandtab ts=4 : */
/* EOF */
