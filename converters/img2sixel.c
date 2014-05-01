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

#include "quant.h"
#include "stb_image.c"


static int
convert_to_sixel(char const *filename, int reqcolors,
                 const char *mapfile, int monochrome,
                 const char *diffusion, int f8bit)
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
    FILE *f;

    if (reqcolors < 2) {
        reqcolors = 2;
    } else if (reqcolors > PALETTE_MAX) {
        reqcolors = PALETTE_MAX;
    }

    if (filename == NULL || strcmp(filename, "-") == 0) {
        /* for windows */
#if HAVE_O_BINARY
# if HAVE__SETMODE
        _setmode(fileno(stdin), O_BINARY);
# elif HAVE_SETMODE
        setmode(fileno(stdin), O_BINARY);
# endif  /* HAVE_SETMODE */
#endif  /* HAVE_O_BINARY */
        f = stdin;
    } else {
        f = fopen(filename, "rb");
        if (!f) {
#if HAVE_ERRNO_H
            fprintf(stderr, "fopen('%s') failed.\n" "reason: %s.\n",
                    filename, strerror(errno));
#endif  /* HAVE_ERRNO_H */
            nret = -1;
            goto end;
        }
    }
    pixels = stbi_load_from_file(f, &sx, &sy, &comp, STBI_rgb);
    fclose(f);
    if (pixels == NULL) {
        fprintf(stderr, "stbi_load_from_file('%s') failed.\n" "reason: %s.\n",
                filename, stbi_failure_reason());
        nret = -1;
        goto end;
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
        if (strcmp(mapfile, "-") == 0) {
            /* for windows */
            setmode(fileno(stdin), _O_BINARY);
            f = stdin;
        } else {
            f = fopen(mapfile, "rb");
        }
        if (!f) {
#if HAVE_ERRNO_H
            fprintf(stderr, "fopen('%s') failed.\n" "reason: %s.\n",
                    mapfile, strerror(errno));
#endif  /* HAVE_ERRNO_H */
            nret = -1;
            goto end;
        }
        mappixels = stbi_load_from_file(f, &map_sx, &map_sy, &map_comp, STBI_rgb);
        fclose(f);
        if (!mappixels) {
            fprintf(stderr, "stbi_load('%s') failed.\n" "reason: %s.\n",
                    mapfile, stbi_failure_reason());
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

    if (diffusion) {
        if (strcmp(diffusion, "auto") == 0) {
            // do nothing
        } else if (strcmp(diffusion, "none") == 0) {
            method_for_diffuse = DIFFUSE_NONE;
        } else if (strcmp(diffusion, "fs") == 0) {
            method_for_diffuse = DIFFUSE_FS;
        } else if (strcmp(diffusion, "jajuni") == 0) {
            method_for_diffuse = DIFFUSE_JAJUNI;
        } else {
            fprintf(stderr,
                    "Diffusion method '%s' is not supported.\n",
                    diffusion);
            nret = -1;
            goto end;
        }
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
    char *diffusion = NULL;
    char *mapfile = NULL;
    int long_opt;
    int option_index;
    int ret;
    int exit_code;
    int f8bit;

    f8bit = 0;

    struct option long_options[] = {
        {"7bit-mode",    no_argument,        &long_opt, '7'},
        {"8bit-mode",    no_argument,        &long_opt, '8'},
        {"colors",       required_argument,  &long_opt, 'p'},
        {"mapfile",      required_argument,  &long_opt, 'm'},
        {"monochrome",   no_argument,        &long_opt, 'e'},
        {"diffusion",    required_argument,  &long_opt, 'd'},
        {0, 0, 0, 0}
    };

    for (;;) {
        n = getopt_long(argc, argv, "78p:m:ed:",
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
            diffusion = strdup(optarg);
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
        ret = convert_to_sixel(NULL, ncolors, mapfile,
                               monochrome, diffusion, f8bit);
        if (ret != 0) {
            exit_code = EXIT_FAILURE;
            goto end;
        }
    } else {
        for (n = optind; n < argc; n++) {
            ret = convert_to_sixel(argv[n], ncolors, mapfile,
                                   monochrome, diffusion, f8bit);
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
            "-7, --7bit-mode            generate a sixel image for 8bit terminal\n"
            "                           or printer (default)\n"
            "-8, --8bit-mode            generate a sixel image for 8bit terminal\n"
            "                           or printer\n"
            "-p COLORS, --colors=COLORS specify number of colors to reduce the\n"
            "                           image to (default=256)\n"
            "-m FILE, --mapfile=FILE    transform image colors to match this set\n"
            "                           of colorsspecify map\n"
            "-e, --monochrome           output monochrome sixel image\n"
            "-d TYPE, --diffusion=TYPE  choose diffusion method which used with\n"
            "                           color reduction\n"
            "                           TYPE is one of them:\n"
            "                               auto   -> choose diffusion type\n"
            "                                         automatically (default)\n"
            "                               none   -> do not diffusion\n"
            "                               fs     -> Floyd-Steinberg method\n"
            "                               jajuni -> Jarvis, Judice & Ninke\n"
            );

end:
    if (mapfile) {
        free(mapfile);
    }
    if (diffusion) {
        free(diffusion);
    }
    return exit_code;
}

/* emacs, -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set expandtab ts=4 : */
/* EOF */
