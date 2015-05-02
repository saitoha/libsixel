/*
 * Copyright (c) 2014,2015 Hayaki Saito
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

#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#if HAVE_SYS_UNISTD_H
# include <sys/unistd.h>
#endif
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#if HAVE_TIME_H
# include <time.h>
#endif
#if HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#if HAVE_INTTYPES_H
# include <inttypes.h>
#endif
#if HAVE_ERRNO_H
# include <errno.h>
#endif
#if HAVE_TERMIOS_H
# include <termios.h>
#endif
#if HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif

#include "easy_decode.h"
#include <sixel.h>


static char *
arg_strdup(char const *s)
{
    char *p;

    p = malloc(strlen(s) + 1);
    if (p) {
        strcpy(p, s);
    }
    return p;
}


int
sixel_easy_decode(
    sixel_decode_settings_t /* in */ *psettings)
{
    unsigned char *raw_data;
    int sx, sy;
    int raw_len;
    int max;
    int n;
    FILE *input_fp = NULL;
    unsigned char *indexed_pixels;
    unsigned char *palette;
    int ncolors;
    unsigned char *pixels = NULL;
    int ret = 0;

    if (strcmp(psettings->input, "-") == 0) {
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
        input_fp = fopen(psettings->input, "rb");
        if (!input_fp) {
#if HAVE_ERRNO_H
            fprintf(stderr, "fopen('%s') failed.\n" "reason: %s.\n",
                    psettings->input, strerror(errno));
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

    ret = sixel_helper_write_image_file(indexed_pixels, sx, sy, palette,
                                        PIXELFORMAT_PAL8,
                                        psettings->output,
                                        FORMAT_PNG);

end:
    free(pixels);
    return ret;
}


int
sixel_easy_decode_setopt(
    sixel_decode_settings_t /* in */ *settings,
    int                     /* in */ arg,
    char                    /* in */ *optarg
)
{
    switch(arg) {
    case 'i':
        free(settings->input);
        settings->input = arg_strdup(optarg);
        break;
    case 'o':
        free(settings->output);
        settings->output = arg_strdup(optarg);
        break;
    case 'V':
        settings->show_version = 1;
        break;
    case 'H':
        settings->show_help = 1;
        break;
    case '?':
    default:
        return (-1);
    }

    return 0;
}


/* create settings object */
sixel_decode_settings_t *
sixel_decode_settings_create(void)
{
    sixel_decode_settings_t *settings;

    settings = malloc(sizeof(sixel_decode_settings_t));

    settings->ref          = 1;
    settings->output       = arg_strdup("-");
    settings->input        = arg_strdup("-");
    settings->show_version = 0;
    settings->show_help    = 0;

    return settings;
}


void
sixel_decode_settings_destroy(sixel_decode_settings_t *settings)
{
    if (settings) {
        free(settings->input);
        free(settings->output);
        free(settings);
    }
}


void
sixel_decode_settings_ref(sixel_decode_settings_t *settings)
{
    /* TODO: be thread safe */
    ++settings->ref;
}


void
sixel_decode_settings_unref(sixel_decode_settings_t *settings)
{
    /* TODO: be thread safe */
    if (settings != NULL && --settings->ref == 0) {
        sixel_decode_settings_destroy(settings);
    }
}


/* get show_version flag */
int
sixel_decode_settings_has_version(sixel_decode_settings_t *settings)
{
    return settings->show_version;
}


/* get show_help flag */
int
sixel_decode_settings_has_help(sixel_decode_settings_t *settings)
{
    return settings->show_help;
}


/* emacs, -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set expandtab ts=4 : */
/* EOF */
