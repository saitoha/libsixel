/*
 * Copyright (c) 2014-2018 Hayaki Saito
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

/* STDC_HEADERS */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if HAVE_UNISTD_H
# include <unistd.h>
#elif HAVE_SYS_UNISTD_H
# include <sys/unistd.h>
#endif  /* HAVE_UNISTD_H */
#if HAVE_ERRNO_H
# include <errno.h>
#endif /* HAVE_ERRNO_H */

#include "decoder.h"


/* original version of strdup(3) with allocator object */
static char *
strdup_with_allocator(
    char const          /* in */ *s,          /* source buffer */
    sixel_allocator_t   /* in */ *allocator)  /* allocator object for
                                                 destination buffer */
{
    char *p;

    p = (char *)sixel_allocator_malloc(allocator, (size_t)(strlen(s) + 1));
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
        status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    } else {
        sixel_allocator_ref(allocator);
    }

    *ppdecoder = sixel_allocator_malloc(allocator, sizeof(sixel_decoder_t));
    if (*ppdecoder == NULL) {
        sixel_allocator_unref(allocator);
        sixel_helper_set_additional_message(
            "sixel_decoder_new: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    (*ppdecoder)->ref          = 1;
    (*ppdecoder)->output       = strdup_with_allocator("-", allocator);
    (*ppdecoder)->input        = strdup_with_allocator("-", allocator);
    (*ppdecoder)->allocator    = allocator;
    (*ppdecoder)->dequantize_method = SIXEL_DEQUANTIZE_NONE;

    if ((*ppdecoder)->output == NULL || (*ppdecoder)->input == NULL) {
        sixel_decoder_unref(*ppdecoder);
        *ppdecoder = NULL;
        sixel_helper_set_additional_message(
            "sixel_decoder_new: strdup_with_allocator() failed.");
        status = SIXEL_BAD_ALLOCATION;
        sixel_allocator_unref(allocator);
        goto end;
    }

    status = SIXEL_OK;

end:
    return status;
}


/* deprecated version of sixel_decoder_new() */
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


/* destroy a decoder object */
static void
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


/* increase reference count of decoder object (thread-unsafe) */
SIXELAPI void
sixel_decoder_ref(sixel_decoder_t *decoder)
{
    /* TODO: be thread safe */
    ++decoder->ref;
}


/* decrease reference count of decoder object (thread-unsafe) */
SIXELAPI void
sixel_decoder_unref(sixel_decoder_t *decoder)
{
    /* TODO: be thread safe */
    if (decoder != NULL && --decoder->ref == 0) {
        sixel_decoder_destroy(decoder);
    }
}


static inline int
sixel_int_round_positive(double value)
{
    return (int)(value + 0.5);
}

static SIXELSTATUS
sixel_dequantize_floyd_steinberg(unsigned char *indexed_pixels,
                                 int width,
                                 int height,
                                 unsigned char *palette,
                                 int ncolors,
                                 sixel_allocator_t *allocator,
                                 unsigned char **output)
{
    SIXELSTATUS status = SIXEL_FALSE;
    unsigned char *rgb = NULL;
    double *error_curr = NULL;
    double *error_next = NULL;
    size_t width3;
    size_t num_pixels;
    int y;
    int x;

    if (width <= 0 || height <= 0) {
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    width3 = (size_t)width * 3;
    num_pixels = (size_t)width * (size_t)height;

    rgb = (unsigned char *)sixel_allocator_malloc(allocator, num_pixels * 3);
    if (rgb == NULL) {
        sixel_helper_set_additional_message(
            "sixel_dequantize_floyd_steinberg: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    error_curr = (double *)sixel_allocator_calloc(allocator, width3, sizeof(double));
    if (error_curr == NULL) {
        sixel_helper_set_additional_message(
            "sixel_dequantize_floyd_steinberg: sixel_allocator_calloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    error_next = (double *)sixel_allocator_calloc(allocator, width3, sizeof(double));
    if (error_next == NULL) {
        sixel_helper_set_additional_message(
            "sixel_dequantize_floyd_steinberg: sixel_allocator_calloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    for (y = 0; y < height; ++y) {
        memset(error_next, 0, width3 * sizeof(double));

        for (x = 0; x < width; ++x) {
            int palette_index = indexed_pixels[y * width + x];
            int base_index = x * 3;
            size_t out_index = (size_t)(y * width + x) * 3;
            double error[3];
            int output_pixel[3];
            int channel;

            if (palette_index < 0 || palette_index >= ncolors) {
                palette_index = 0;
            }

            for (channel = 0; channel < 3; ++channel) {
                double quantized = palette[palette_index * 3 + channel];
                double accumulated = quantized + error_curr[base_index + channel];
                double reconstructed = accumulated;

                error_curr[base_index + channel] = 0.0;

                if (reconstructed < 0.0) {
                    reconstructed = 0.0;
                } else if (reconstructed > 255.0) {
                    reconstructed = 255.0;
                }

                output_pixel[channel] = sixel_int_round_positive(reconstructed);
                if (output_pixel[channel] < 0) {
                    output_pixel[channel] = 0;
                }
                if (output_pixel[channel] > 255) {
                    output_pixel[channel] = 255;
                }
                error[channel] = (double)output_pixel[channel] - quantized;
                rgb[out_index + channel] = (unsigned char)output_pixel[channel];
            }

            if (x + 1 < width) {
                int right = base_index + 3;
                error_curr[right + 0] += error[0] * (7.0 / 16.0);
                error_curr[right + 1] += error[1] * (7.0 / 16.0);
                error_curr[right + 2] += error[2] * (7.0 / 16.0);
            }

            if (y + 1 < height) {
                if (x > 0) {
                    int down_left = base_index - 3;
                    error_next[down_left + 0] += error[0] * (3.0 / 16.0);
                    error_next[down_left + 1] += error[1] * (3.0 / 16.0);
                    error_next[down_left + 2] += error[2] * (3.0 / 16.0);
                }

                error_next[base_index + 0] += error[0] * (5.0 / 16.0);
                error_next[base_index + 1] += error[1] * (5.0 / 16.0);
                error_next[base_index + 2] += error[2] * (5.0 / 16.0);

                if (x + 1 < width) {
                    int down_right = base_index + 3;
                    error_next[down_right + 0] += error[0] * (1.0 / 16.0);
                    error_next[down_right + 1] += error[1] * (1.0 / 16.0);
                    error_next[down_right + 2] += error[2] * (1.0 / 16.0);
                }
            }
        }

        {
            double *tmp = error_curr;
            error_curr = error_next;
            error_next = tmp;
        }
    }

    *output = rgb;
    rgb = NULL;
    status = SIXEL_OK;

end:
    sixel_allocator_free(allocator, rgb);
    sixel_allocator_free(allocator, error_curr);
    sixel_allocator_free(allocator, error_next);
    return status;
}

/* set an option flag to decoder object */
SIXELAPI SIXELSTATUS
sixel_decoder_setopt(
    sixel_decoder_t /* in */ *decoder,
    int             /* in */ arg,
    char const      /* in */ *value
)
{
    SIXELSTATUS status = SIXEL_FALSE;

    sixel_decoder_ref(decoder);

    switch(arg) {
    case SIXEL_OPTFLAG_INPUT:  /* i */
        free(decoder->input);
        decoder->input = strdup_with_allocator(value, decoder->allocator);
        if (decoder->input == NULL) {
            sixel_helper_set_additional_message(
                "sixel_decoder_setopt: strdup_with_allocator() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_OUTPUT:  /* o */
        free(decoder->output);
        decoder->output = strdup_with_allocator(value, decoder->allocator);
        if (decoder->output == NULL) {
            sixel_helper_set_additional_message(
                "sixel_decoder_setopt: strdup_with_allocator() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        break;
    case SIXEL_DECODER_OPTFLAG_DEQUANTIZE: /* d */
        if (value == NULL || value[0] == '\0') {
            decoder->dequantize_method = SIXEL_DEQUANTIZE_FS;
        } else if (strcmp(value, "fs") == 0) {
            decoder->dequantize_method = SIXEL_DEQUANTIZE_FS;
        } else if (strcmp(value, "none") == 0) {
            decoder->dequantize_method = SIXEL_DEQUANTIZE_NONE;
        } else {
            sixel_helper_set_additional_message(
                "sixel_decoder_setopt: unsupported dequantize method.");
            status = SIXEL_BAD_ARGUMENT;
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


/* load source data from stdin or the file specified with
   SIXEL_OPTFLAG_INPUT flag, and decode it */
SIXELAPI SIXELSTATUS
sixel_decoder_decode(
    sixel_decoder_t /* in */ *decoder)
{
    SIXELSTATUS status = SIXEL_FALSE;
    unsigned char *raw_data = NULL;
    int sx;
    int sy;
    int raw_len;
    int max;
    int n;
    FILE *input_fp = NULL;
    unsigned char *indexed_pixels = NULL;
    unsigned char *palette = NULL;
    unsigned char *rgb_pixels = NULL;
    unsigned char *output_pixels;
    unsigned char *output_palette;
    int output_pixelformat;
    int ncolors;

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

    raw_data = (unsigned char *)sixel_allocator_malloc(decoder->allocator, (size_t)max);
    if (raw_data == NULL) {
        sixel_helper_set_additional_message(
            "sixel_decoder_decode: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    for (;;) {
        if ((max - raw_len) < 4096) {
            max *= 2;
            raw_data = (unsigned char *)sixel_allocator_realloc(decoder->allocator, raw_data, (size_t)max);
            if (raw_data == NULL) {
                sixel_helper_set_additional_message(
                    "sixel_decoder_decode: sixel_allocator_realloc() failed.");
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }
        }
        if ((n = (int)fread(raw_data + raw_len, 1, 4096, input_fp)) <= 0) {
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

    if (sx > SIXEL_WIDTH_LIMIT || sy > SIXEL_HEIGHT_LIMIT) {
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    output_pixels = indexed_pixels;
    output_palette = palette;
    output_pixelformat = SIXEL_PIXELFORMAT_PAL8;

    if (decoder->dequantize_method == SIXEL_DEQUANTIZE_FS) {
        status = sixel_dequantize_floyd_steinberg(indexed_pixels,
                                                  sx,
                                                  sy,
                                                  palette,
                                                  ncolors,
                                                  decoder->allocator,
                                                  &rgb_pixels);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        output_pixels = rgb_pixels;
        output_palette = NULL;
        output_pixelformat = SIXEL_PIXELFORMAT_RGB888;
    }

    status = sixel_helper_write_image_file(output_pixels, sx, sy, output_palette,
                                           output_pixelformat,
                                           decoder->output,
                                           SIXEL_FORMAT_PNG,
                                           decoder->allocator);

    if (SIXEL_FAILED(status)) {
        goto end;
    }

end:
    sixel_allocator_free(decoder->allocator, raw_data);
    sixel_allocator_free(decoder->allocator, indexed_pixels);
    sixel_allocator_free(decoder->allocator, palette);
    sixel_allocator_free(decoder->allocator, rgb_pixels);
    sixel_decoder_unref(decoder);

    return status;
}


#if HAVE_TESTS
static int
test1(void)
{
    int nret = EXIT_FAILURE;
    sixel_decoder_t *decoder = NULL;

#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    decoder = sixel_decoder_create();
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
#  pragma GCC diagnostic pop
#endif
    if (decoder == NULL) {
        goto error;
    }
    sixel_decoder_ref(decoder);
    sixel_decoder_unref(decoder);
    nret = EXIT_SUCCESS;

error:
    sixel_decoder_unref(decoder);
    return nret;
}


static int
test2(void)
{
    int nret = EXIT_FAILURE;
    sixel_decoder_t *decoder = NULL;
    SIXELSTATUS status;

    status = sixel_decoder_new(&decoder, NULL);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    sixel_decoder_ref(decoder);
    sixel_decoder_unref(decoder);
    nret = EXIT_SUCCESS;

error:
    sixel_decoder_unref(decoder);
    return nret;
}


static int
test3(void)
{
    int nret = EXIT_FAILURE;
    sixel_decoder_t *decoder = NULL;
    sixel_allocator_t *allocator = NULL;
    SIXELSTATUS status;

    sixel_debug_malloc_counter = 1;

    status = sixel_allocator_new(&allocator, sixel_bad_malloc, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_decoder_new(&decoder, allocator);
    if (status != SIXEL_BAD_ALLOCATION) {
        goto error;
    }

    nret = EXIT_SUCCESS;

error:
    return nret;
}


static int
test4(void)
{
    int nret = EXIT_FAILURE;
    sixel_decoder_t *decoder = NULL;
    sixel_allocator_t *allocator = NULL;
    SIXELSTATUS status;

    sixel_debug_malloc_counter = 2;

    status = sixel_allocator_new(&allocator, sixel_bad_malloc, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_decoder_new(&decoder, allocator);
    if (status != SIXEL_BAD_ALLOCATION) {
        goto error;
    }

    nret = EXIT_SUCCESS;

error:
    return nret;
}


static int
test5(void)
{
    int nret = EXIT_FAILURE;
    sixel_decoder_t *decoder = NULL;
    sixel_allocator_t *allocator = NULL;
    SIXELSTATUS status;

    sixel_debug_malloc_counter = 4;

    status = sixel_allocator_new(&allocator, sixel_bad_malloc, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto error;
    }
    status = sixel_decoder_new(&decoder, allocator);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_decoder_setopt(decoder, SIXEL_OPTFLAG_INPUT, "/");
    if (status != SIXEL_BAD_ALLOCATION) {
        goto error;
    }

    nret = EXIT_SUCCESS;

error:
    return nret;
}


static int
test6(void)
{
    int nret = EXIT_FAILURE;
    sixel_decoder_t *decoder = NULL;
    sixel_allocator_t *allocator = NULL;
    SIXELSTATUS status;

    sixel_debug_malloc_counter = 4;

    status = sixel_allocator_new(&allocator, sixel_bad_malloc, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_decoder_new(&decoder, allocator);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_decoder_setopt(decoder, SIXEL_OPTFLAG_OUTPUT, "/");
    if (status != SIXEL_BAD_ALLOCATION) {
        goto error;
    }

    nret = EXIT_SUCCESS;

error:
    return nret;
}


static int
test7(void)
{
    int nret = EXIT_FAILURE;
    sixel_decoder_t *decoder = NULL;
    sixel_allocator_t *allocator = NULL;
    SIXELSTATUS status;

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_decoder_new(&decoder, allocator);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_decoder_setopt(decoder, SIXEL_OPTFLAG_INPUT, "../images/file");
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_decoder_decode(decoder);
    if ((status >> 8) != (SIXEL_LIBC_ERROR >> 8)) {
        goto error;
    }

    nret = EXIT_SUCCESS;

error:
    return nret;
}


static int
test8(void)
{
    int nret = EXIT_FAILURE;
    sixel_decoder_t *decoder = NULL;
    sixel_allocator_t *allocator = NULL;
    SIXELSTATUS status;

    sixel_debug_malloc_counter = 5;

    status = sixel_allocator_new(&allocator, sixel_bad_malloc, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_decoder_new(&decoder, allocator);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_decoder_setopt(decoder, SIXEL_OPTFLAG_INPUT, "../images/map8.six");
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_decoder_decode(decoder);
    if (status != SIXEL_BAD_ALLOCATION) {
        goto error;
    }

    nret = EXIT_SUCCESS;

error:
    return nret;
}


SIXELAPI int
sixel_decoder_tests_main(void)
{
    int nret = EXIT_FAILURE;
    size_t i;
    typedef int (* testcase)(void);

    static testcase const testcases[] = {
        test1,
        test2,
        test3,
        test4,
        test5,
        test6,
        test7,
        test8
    };

    for (i = 0; i < sizeof(testcases) / sizeof(testcase); ++i) {
        nret = testcases[i]();
        if (nret != EXIT_SUCCESS) {
            goto error;
        }
    }

    nret = EXIT_SUCCESS;

error:
    return nret;
}
#endif  /* HAVE_TESTS */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */

