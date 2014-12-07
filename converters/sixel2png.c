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

#if HAVE_ERRNO_H
# include <errno.h>
#endif

#if HAVE_SETJMP_H
# include <setjmp.h>
#endif

#if HAVE_LIBPNG
# include <png.h>
#else
# include "stb_image_write.h"
#endif

#include <sixel.h>

#if !defined(O_BINARY) && defined(_O_BINARY)
# define O_BINARY _O_BINARY
#endif  /* !defined(O_BINARY) && !defined(_O_BINARY) */

#if !HAVE_LIBPNG
unsigned char *
stbi_write_png_to_mem(unsigned char *pixels, int stride_bytes,
                      int x, int y, int n, int *out_len);
#endif


static int
sixel_to_png(const char *input, const char *output)
{
    unsigned char *raw_data, *png_data = NULL;
    int sx, sy;
    int raw_len;
    int max;
    int n;
    FILE *input_fp = NULL, *output_fp = NULL;
    unsigned char *indexed_pixels;
    unsigned char *palette;
    int ncolors;
    unsigned char *pixels;
    int x, y;
    int ret = 0;
#if HAVE_LIBPNG
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    unsigned char **rows = NULL;
#else
    int png_len;
    int write_len;
#endif  /* HAVE_LIBPNG */

    if (strcmp(input, "-") == 0) {
        /* for windows */
#if defined(O_BINARY)
# if HAVE__SETMODE
        _setmode(fileno(stdin), O_BINARY);
# elif HAVE_SETMODE
        setmode(fileno(stdin), O_BINARY);
# endif  /* HAVE_SETMODE */
#endif  /* defined(O_BINARY) */
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
                fprintf(stderr, "realloc(raw_data, %d) failed.\n"
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

    ret = sixel_decode(raw_data, raw_len, &indexed_pixels,
                       &sx, &sy, &palette, &ncolors, malloc);

    if (ret != 0) {
        fprintf(stderr, "sixel_decode failed.\n");
        goto end;
    }

    pixels = malloc(sx * sy * 3);
    for (y = 0; y < sy; ++y) {
        for (x = 0; x < sx; ++x) {
            n = indexed_pixels[sx * y + x];
            pixels[sx * 3 * y + x * 3 + 0] = palette[n * 4 + 0];
            pixels[sx * 3 * y + x * 3 + 1] = palette[n * 4 + 1];
            pixels[sx * 3 * y + x * 3 + 2] = palette[n * 4 + 2];
        }
    }

    if (strcmp(output, "-") == 0) {
#if defined(O_BINARY)
# if HAVE__SETMODE
        _setmode(fileno(stdout), O_BINARY);
# elif HAVE_SETMODE
        setmode(fileno(stdout), O_BINARY);
# endif  /* HAVE_SETMODE */
#endif  /* defined(O_BINARY) */
        output_fp = stdout;
    } else {
        output_fp = fopen(output, "wb");
        if (!output_fp) {
#if HAVE_ERRNO_H
            fprintf(stderr, "fopen('%s') failed.\n" "reason: %s.\n",
                    output, strerror(errno));
#endif  /* HAVE_ERRNO_H */
            ret = -1;
            goto end;
        }
    }

#if HAVE_LIBPNG
    rows = malloc(sy * sizeof(unsigned char *));
    for (y = 0; y < sy; ++y) {
        rows[y] = pixels + sx * 3 * y;
    }
    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
        ret = (-1);
        goto end;
    }
    info_ptr = png_create_info_struct(png_ptr);
    if (!png_ptr) {
        ret = (-1);
        goto end;
    }
# if USE_SETJMP && HAVE_SETJMP
    if (setjmp(png_jmpbuf(png_ptr))) {
        ret = (-1);
        goto end;
    }
# endif
    png_init_io(png_ptr, output_fp);
    png_set_IHDR(png_ptr, info_ptr, sx, sy,
                 /* bit_depth */ 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
    png_write_info(png_ptr, info_ptr);
    png_write_image(png_ptr, rows);
    png_write_end(png_ptr, NULL);
#else
    png_data = stbi_write_png_to_mem(pixels, sx * 3,
                                     sx, sy, /* STBI_rgb */ 3, &png_len);

    if (!png_data) {
        fprintf(stderr, "stbi_write_png_to_mem failed.\n");
        goto end;
    }
    write_len = fwrite(png_data, 1, png_len, output_fp);
    if (write_len < 0) {
# if HAVE_ERRNO_H
        fprintf(stderr, "fwrite failed.\n" "reason: %s.\n",
                strerror(errno));
# endif  /* HAVE_ERRNO_H */
        ret = -1;
        goto end;
    }
#endif  /* HAVE_LIBPNG */

end:
    if (output_fp && output_fp != stdout) {
        fclose(output_fp);
    }
    free(png_data);
#if HAVE_LIBPNG
    free(rows);
    png_destroy_write_struct (&png_ptr, &info_ptr);
#endif  /* HAVE_LIBPNG */
    return ret;
}


static
void show_version(void)
{
    printf("sixel2png " PACKAGE_VERSION "\n"
           "Copyright (C) 2014 Hayaki Saito <user@zuse.jp>.\n"
           "\n"
           "Permission is hereby granted, free of charge, to any person obtaining a copy of\n"
           "this software and associated documentation files (the \"Software\"), to deal in\n"
           "the Software without restriction, including without limitation the rights to\n"
           "use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of\n"
           "the Software, and to permit persons to whom the Software is furnished to do so,\n"
           "subject to the following conditions:\n"
           "\n"
           "The above copyright notice and this permission notice shall be included in all\n"
           "copies or substantial portions of the Software.\n"
           "\n"
           "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n"
           "IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS\n"
           "FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR\n"
           "COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER\n"
           "IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN\n"
           "CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.\n"
          );
}


static void
show_help(void)
{
    fprintf(stderr,
            "Usage: sixel2png -i input.sixel -o output.png\n"
            "       sixel2png < input.sixel > output.png\n"
            "\n"
            "Options:\n"
            "-i, --input     specify input file\n"
            "-o, --output    specify output file\n"
            "-V, --version   show version and license information\n"
            "-H, --help      show this help\n"
           );
}


#if HAVE_STRDUP
# define wrap_strdup(s) strdup(s)
#else
static char *
wrap_strdup(char const *s)
{
    char *p = malloc(strlen(s) + 1);
    if (p) {
        strcpy(p, s);
    }
    return p;
}
#endif


int
main(int argc, char *argv[])
{
    int n;
    char *output = wrap_strdup("-");
    char *input = wrap_strdup("-");
#if HAVE_GETOPT_LONG
    int long_opt;
    int option_index;
#endif  /* HAVE_GETOPT_LONG */
    int nret = 0;
    char const *optstring = "i:o:VH";

#if HAVE_GETOPT_LONG
    struct option long_options[] = {
        {"input",        required_argument,  &long_opt, 'i'},
        {"output",       required_argument,  &long_opt, 'o'},
        {"version",      no_argument,        &long_opt, 'V'},
        {"help",         no_argument,        &long_opt, 'H'},
        {0, 0, 0, 0}
    };
#endif  /* HAVE_GETOPT_LONG */
    for (;;) {
#if HAVE_GETOPT_LONG
        n = getopt_long(argc, argv, optstring,
                        long_options, &option_index);
#else
        n = getopt(argc, argv, optstring);
#endif  /* HAVE_GETOPT_LONG */

        if (n == -1) {
            nret = (-1);
            break;
        }
#if HAVE_GETOPT_LONG
        if (n == 0) {
            n = long_opt;
        }
#endif  /* HAVE_GETOPT_LONG */
        switch(n) {
        case 'i':
            free(input);
            input = wrap_strdup(optarg);
            break;
        case 'o':
            free(output);
            output = wrap_strdup(optarg);
            break;
        case 'V':
            show_version();
            goto end;
        case 'H':
            show_help();
            goto end;
        case '?':
        default:
            nret = (-1);
            goto argerr;
        }
        if (optind >= argc) {
            break;
        }
    }

    if (strcmp(input, "-") == 0 && optind < argc) {
        free(input);
        input = wrap_strdup(argv[optind++]);
    }
    if (strcmp(output, "-") == 0 && optind < argc) {
        free(output);
        output = wrap_strdup(argv[optind++]);
    }
    if (optind != argc) {
        nret = (-1);
        goto argerr;
    }

    nret = sixel_to_png(input, output);
    goto end;

argerr:
    show_help();

end:
    free(input);
    free(output);
    return nret;
}

/* emacs, -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set expandtab ts=4 : */
/* EOF */
