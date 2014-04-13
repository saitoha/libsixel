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
#include <string.h>  /* strdup */

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
#include "stb_image_write.h"

enum
{
   STBI_default = 0, /* only used for req_comp */
   STBI_grey = 1,
   STBI_grey_alpha = 2,
   STBI_rgb = 3,
   STBI_rgb_alpha = 4
};

static int
sixel_to_png(const char *input, const char *output)
{
    uint8_t *data;
    LSImagePtr im;
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

    if (fp != stdout) {
        fclose(fp);
    }

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
    char *output = NULL;
    char *input = NULL;
    int long_opt;
    int option_index;

    struct option long_options[] = {
        {"output",       required_argument,  &long_opt, 'o'},
        {"input",        required_argument,  &long_opt, 'i'},
        {0, 0, 0, 0}
    };

    for (;;) {
        n = getopt_long(argc, argv, "i:o:",
                        long_options, &option_index);
        if (n == -1) {
            break;
        }
        if (n == 0) {
            n = long_opt;
        }
        switch(n) {
        case 'i':
            free(input);
            input = strdup(optarg);
            break;
        case 'o':
            free(output);
            output = strdup(optarg);
            break;
        case '?':
            goto argerr;
        default:
            goto argerr;
        }
        if (optind >= argc) {
            break;
        }
        optind++;
    }

    if (input == NULL && optind < argc) {
        input = argv[optind++];
    }
    if (output == NULL && optind < argc) {
        output = argv[optind++];
    }
    if (optind != argc) {
        goto argerr;
    }
    if (input == NULL) {
        input = strdup("/dev/stdin");
    }
    if (output == NULL) {
        output = strdup("/dev/stdout");
    }

    sixel_to_png(input, output);

    free(input);
    free(output);
    return 0;

argerr:
    if (input) {
        free(input);
    }
    if (output) {
        free(output);
    }
    fprintf(stderr,
            "Usage: sixel2png -i input.sixel -o output.png\n"
            "       sixel2png < input.sixel > output.png\n"
            "\n"
            "Options:\n"
            "-i, --input     specify input file\n"
            "-o, --output    specify output file\n");
    return 0;
}

/* emacs, -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set expandtab ts=4 : */
/* EOF */
