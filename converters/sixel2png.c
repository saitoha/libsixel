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
#include <string.h>  /* strdup */

#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#if HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif

#if HAVE_FCNTL_H
# include <fcntl.h>
#endif

#if HAVE_UNISTD_H
# include <unistd.h>  /* getopt */
#endif

#if HAVE_GETOPT_H
# include <getopt.h>
#endif

#if HAVE_INTTYPES_H
# include <inttypes.h>
#endif

#if defined(HAVE_ERRNO_H)
# include <errno.h>
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
    unsigned char *raw_data, *png_data;
    LSImagePtr im;
    int sx, sy, comp;
    int raw_len;
    int png_len;
    int i;
    int max;
    int n;
    FILE *input_fp, *output_fp;

    if (input == NULL || strcmp(input, "-") == 0) {
        /* for windows */
#if HAVE_O_BINARY
# if HAVE__SETMODE
        _setmode(fileno(stdin), O_BINARY);
# elif HAVE_SETMODE
        setmode(fileno(stdin), O_BINARY);
# endif  /* HAVE_SETMODE */
#endif  /* HAVE_O_BINARY */
        input_fp = stdin;
    } else {
        input_fp = fopen(input, "rb");
        if (!input_fp) {
#if HAVE_ERRNO_H
            fprintf(stderr, "fopen('%s') failed.\n" "reason: %s.\n",
                    input, strerror(errno));
#endif  /* HAVE_ERRNO_H */
            return (-1);
        }
    }

    raw_len = 0;
    max = 64 * 1024;

    if ((raw_data = (unsigned char *)malloc(max)) == NULL) {
#if HAVE_ERRNO_H
        fprintf(stderr, "malloc(%d) failed.\n" "reason: %s.\n",
                max, strerror(errno));
#endif  /* HAVE_ERRNO_H */
        return (-1);
    }

    for (;;) {
        if ((max - raw_len) < 4096) {
            max *= 2;
            if ((raw_data = (unsigned char *)realloc(raw_data, max)) == NULL) {
#if HAVE_ERRNO_H
                fprintf(stderr, "reaalloc(raw_data, %d) failed.\n"
                                "reason: %s.\n",
                        max, strerror(errno));
#endif  /* HAVE_ERRNO_H */
                return (-1);
            }
        }
        if ((n = fread(raw_data + raw_len, 1, 4096, input_fp)) <= 0)
            break;
        raw_len += n;
    }

    if (input_fp != stdout) {
        fclose(input_fp);
    }

    im = LibSixel_SixelToLSImage(raw_data, raw_len);
    if (!im) {
        fprintf(stderr, "LibSixel_SixelToLSImage failed.\n");
        return (-1);
    }

    png_data = stbi_write_png_to_mem(im->pixels, im->sx * 3,
                                     im->sx, im->sy, STBI_rgb, &png_len);
    LSImage_destroy(im);
    if (!png_data) {
        fprintf(stderr, "stbi_write_png_to_mem failed.\n");
        return (-1);
    }
    if (input == NULL || strcmp(output, "-") == 0) {
#if HAVE_O_BINARY
# if HAVE__SETMODE
        _setmode(fileno(stdout), O_BINARY);
# elif HAVE_SETMODE
        setmode(fileno(stdout), O_BINARY);
# endif  /* HAVE_SETMODE */
#endif  /* HAVE_O_BINARY */
        output_fp = stdout;
    } else {
        output_fp = fopen(output, "wb");
        if (!output_fp) {
#if HAVE_ERRNO_H
            fprintf(stderr, "fopen('%s') failed.\n" "reason: %s.\n",
                    output, strerror(errno));
#endif  /* HAVE_ERRNO_H */
            free(png_data);
            return (-1);
        }
    }
    fwrite(png_data, 1, png_len, output_fp);
    fclose(output_fp);
    free(png_data);

    return 0;
}

int main(int argc, char *argv[])
{
    int n;
    char *output = NULL;
    char *input = NULL;
    int long_opt;
    int option_index;
    int nret = 0;

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

    nret = sixel_to_png(input, output);
    goto end;

argerr:
    fprintf(stderr,
            "Usage: sixel2png -i input.sixel -o output.png\n"
            "       sixel2png < input.sixel > output.png\n"
            "\n"
            "Options:\n"
            "-i, --input     specify input file\n"
            "-o, --output    specify output file\n");
end:
    if (input) {
        free(input);
    }
    if (output) {
        free(output);
    }
    return nret;
}

/* emacs, -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set expandtab ts=4 : */
/* EOF */
