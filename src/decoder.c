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
#if HAVE_IO_H
# include <io.h>
#endif

#include "decoder.h"


static char *
strdup_with_allocator(
    char const          /* in */ *s,          /* source buffer */
    sixel_allocator_t   /* in */ *allocator)  /* allocator object for
                                                 destination buffer */
{
    char *p;

    p = (char *)sixel_allocator_malloc(allocator, strlen(s) + 1);
    if (p) {
        strcpy(p, s);
    }
    return p;
}


/* create decoder object */
SIXELAPI SIXELSTATUS
sixel_decoder_new(
    sixel_decoder_t    /* out */ **ppdecoder,  /* decoder object to be created */
    sixel_allocator_t  /* in */  *allocator)   /* allocator, null if you use
                                                  default allocator */
{
    SIXELSTATUS status = SIXEL_FALSE;

    if (allocator == NULL) {
        status = sixel_allocator_new(&allocator, malloc, calloc, realloc, free);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    } else {
        sixel_allocator_ref(allocator);
    }

    *ppdecoder = sixel_allocator_malloc(allocator, sizeof(sixel_decoder_t));
    if (*ppdecoder == NULL) {
        sixel_allocator_unref(allocator);
        goto end;
    }

    (*ppdecoder)->ref          = 1;
    (*ppdecoder)->output       = strdup_with_allocator("-", allocator);
    (*ppdecoder)->input        = strdup_with_allocator("-", allocator);
    (*ppdecoder)->allocator    = allocator;

    if ((*ppdecoder)->output == NULL || (*ppdecoder)->input == NULL) {
        sixel_decoder_unref(*ppdecoder);
        *ppdecoder = NULL;
        sixel_helper_set_additional_message(
            "sixel_decoder_new: strdup_with_allocator() failed.");
        status = SIXEL_BAD_ALLOCATION;
        sixel_allocator_unref(allocator);
        goto end;
    }

end:
    return status;
}


SIXELAPI /* deprecated */ sixel_decoder_t *
sixel_decoder_create(void)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_decoder_t *decoder = NULL;

    status = sixel_decoder_new(&decoder, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

end:
    return decoder;
}


SIXELAPI void
sixel_decoder_destroy(sixel_decoder_t *decoder)
{
    sixel_allocator_t *allocator;

    if (decoder) {
        allocator = decoder->allocator;
        sixel_allocator_free(allocator, decoder->input);
        sixel_allocator_free(allocator, decoder->output);
        sixel_allocator_free(allocator, decoder);
        sixel_allocator_unref(allocator);
    }
}


SIXELAPI void
sixel_decoder_ref(sixel_decoder_t *decoder)
{
    /* TODO: be thread safe */
    ++decoder->ref;
}


SIXELAPI void
sixel_decoder_unref(sixel_decoder_t *decoder)
{
    /* TODO: be thread safe */
    if (decoder != NULL && --decoder->ref == 0) {
        sixel_decoder_destroy(decoder);
    }
}


SIXELAPI SIXELSTATUS
sixel_decoder_setopt(
    sixel_decoder_t /* in */ *decoder,
    int             /* in */ arg,
    char const      /* in */ *optarg
)
{
    SIXELSTATUS status = SIXEL_FALSE;

    sixel_decoder_ref(decoder);

    switch(arg) {
    case 'i':
        free(decoder->input);
        decoder->input = strdup_with_allocator(optarg, decoder->allocator);
        if (decoder->input == NULL) {
            sixel_helper_set_additional_message(
                "sixel_decoder_setopt: strdup_with_allocator() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        break;
    case 'o':
        free(decoder->output);
        decoder->output = strdup_with_allocator(optarg, decoder->allocator);
        if (decoder->input == NULL) {
            sixel_helper_set_additional_message(
                "sixel_decoder_setopt: strdup_with_allocator() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        break;
    case '?':
    default:
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    status = SIXEL_OK;

end:
    sixel_decoder_unref(decoder);

    return status;
}


SIXELAPI SIXELSTATUS
sixel_decoder_decode(
    sixel_decoder_t /* in */ *decoder)
{
    SIXELSTATUS status = SIXEL_FALSE;
    unsigned char *raw_data;
    int sx;
    int sy;
    int raw_len;
    int max;
    int n;
    FILE *input_fp = NULL;
    unsigned char *indexed_pixels;
    unsigned char *palette;
    int ncolors;
    unsigned char *pixels = NULL;

    sixel_decoder_ref(decoder);

    if (strcmp(decoder->input, "-") == 0) {
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
        input_fp = fopen(decoder->input, "rb");
        if (!input_fp) {
            sixel_helper_set_additional_message(
                "sixel_decoder_decode: fopen() failed.");
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            goto end;
        }
    }

    raw_len = 0;
    max = 64 * 1024;

    raw_data = (unsigned char *)sixel_allocator_malloc(decoder->allocator, max);
    if (raw_data == NULL) {
        sixel_helper_set_additional_message(
            "sixel_decoder_decode: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    for (;;) {
        if ((max - raw_len) < 4096) {
            max *= 2;
            raw_data = (unsigned char *)sixel_allocator_realloc(decoder->allocator, raw_data, max);
            if (raw_data == NULL) {
                sixel_helper_set_additional_message(
                    "sixel_decoder_decode: sixel_allocator_realloc() failed.");
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }
        }
        if ((n = fread(raw_data + raw_len, 1, 4096, input_fp)) <= 0) {
            break;
        }
        raw_len += n;
    }

    if (input_fp != stdout) {
        fclose(input_fp);
    }

    status = sixel_decode_raw(
        raw_data,
        raw_len,
        &indexed_pixels,
        &sx,
        &sy,
        &palette,
        &ncolors,
        decoder->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_helper_write_image_file(indexed_pixels, sx, sy, palette,
                                           SIXEL_PIXELFORMAT_PAL8,
                                           decoder->output,
                                           SIXEL_FORMAT_PNG,
                                           decoder->allocator);

    if (SIXEL_FAILED(status)) {
        goto end;
    }

end:
    sixel_allocator_free(decoder->allocator, pixels);
    sixel_decoder_ref(decoder);

    return status;
}

/* emacs, -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set expandtab ts=4 : */
/* EOF */
