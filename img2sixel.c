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
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include "sixel.h"

enum
{
   STBI_default = 0, /* only used for req_comp */
   STBI_grey = 1,
   STBI_grey_alpha = 2,
   STBI_rgb = 3,
   STBI_rgb_alpha = 4
};

extern uint8_t *
stbi_load(char const *filename, int *x, int *y, int *comp, int req_comp);
extern uint8_t *
stbi_load_from_file(FILE *f, int *x, int *y, int *comp, int req_comp);

extern void
stbi_image_free(void *retval_from_stbi_load);

extern uint8_t *
make_palette(uint8_t *data, int x, int y, int n, int c);

extern uint8_t *
apply_palette(uint8_t *data,
              int width, int height, int depth,
              uint8_t *palette, int ncolors);

static int
convert_to_sixel(char const *filename, int ncolors)
{
    uint8_t *pixels = NULL;
    uint8_t *palette = NULL;
    uint8_t *data = NULL;
    LSImagePtr im = NULL;
    LSOutputContextPtr context;
    int sx, sy, comp;
    int i;
    int nret = -1;

    if ( ncolors < 2 ) {
        ncolors = 2;
    } else if ( ncolors > PALETTE_MAX ) {
        ncolors = PALETTE_MAX;
    }

    pixels = stbi_load(filename, &sx, &sy, &comp, STBI_rgb);
    if (pixels == NULL) {
        return (-1);
    }

    palette = make_palette(pixels, sx, sy, 3, ncolors);
    if (!palette) {
        goto end;
    }
    im = LSImage_create(sx, sy, 1, ncolors);
    if (!im) {
        goto end;
    }
    for (i = 0; i < ncolors; i++) {
        LSImage_setpalette(im, i,
                           palette[i * 3 + 0],
                           palette[i * 3 + 1],
                           palette[i * 3 + 2]);
    }
    data = apply_palette(pixels, sx, sy, 3, palette, ncolors);
    if (!data) {
        goto end;
    }
    LSImage_setpixels(im, data);
    context = LSOutputContext_new();
    LibSixel_LSImageToSixel(im, context);
    LSOutputContext_free(context);

end:
    if (pixels) {
        stbi_image_free(pixels);
    }
    if (palette) {
        free(palette);
    }
    if (im) {
        LSImage_destroy(im);
    }
    return 0;
}

int main(int argc, char *argv[])
{
    int n;
    int filecount = 1;
    int ncolors = PALETTE_MAX;

    while ((n = getopt(argc, argv, "p:")) != -1) {
        switch(n) {
        case 'p':
            ncolors = atoi(optarg);
            break;
        default:
            goto argerr;
        }
    }

    if (optind == argc) {
        convert_to_sixel("/dev/stdin", ncolors);
    } else {
        for (n = optind; n < argc; n++) {
            convert_to_sixel(argv[n], ncolors);
        }
    }
    return 0;

argerr:
    fprintf(stderr, "Usage: %s [-p MaxPalet] <file name...>\n", argv[0]);
    return 0;
}

/* emacs, -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set expandtab ts=4 : */
/* EOF */
