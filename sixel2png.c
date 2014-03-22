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

extern int
stbi_write_png(char const *filename,
               int w, int h, int comp,
               const void *data, int stride_in_bytes);

static int
sixel_to_png(const char *input, const char *output)
{
    uint8_t *data;
    LSImagePtr im;
    LSOutputContext context = { putchar, puts, printf };
    int sx, sy, comp;
    int len;
    int i;
    int max;
    int n;
    FILE *fp;

    if (input != NULL && (fp = fopen(input, "r")) == NULL) {
        return (-1);
    }

    len = 0;
    max = 64 * 1024;

    if ((data = (uint8_t *)malloc(max)) == NULL) {
        return (-1);
    }

    for (;;) {
        if ((max - len) < 4096) {
            max *= 2;
            if ((data = (uint8_t *)realloc(data, max)) == NULL)
                return (-1);
        }
        if ((n = fread(data + len, 1, 4096, fp)) <= 0)
            break;
        len += n;
    }

    if (fp != stdout)
            fclose(fp);

    im = LibSixel_SixelToLSImage(data, len);
    if (!im) {
      return 1;
    }
    stbi_write_png(output, im->sx, im->sy,
                   STBI_rgb, im->pixels, im->sx * 3);

    LSImage_destroy(im);

    return 0;
}

int main(int argc, char *argv[])
{
    int n;
    int filecount = 1;
    char *output = strdup("/dev/stdout");
    char *input = strdup("/dev/stdin");
    const char *usage = "Usage: %s -i input.sixel -o output.png\n"
                        "       %s < input.sixel > output.png\n";

    for (;;) {
        while ((n = getopt(argc, argv, "o:i:")) != EOF) {
            switch(n) {
            case 'i':
                free(input);
                input = strdup(optarg);
                break;
            case 'o':
                free(output);
                output = strdup(optarg);
                break;
            default:
                fprintf(stderr, usage, argv[0], argv[0]);
                exit(0);
            }
        }
        if (optind >= argc) {
            break;
        }
        optind++;
    }

    sixel_to_png(input, output);

    return 0;
}

/* emacs, -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set expandtab ts=4 : */
/* EOF */
