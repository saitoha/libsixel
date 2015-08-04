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
#if HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#if HAVE_FCNTL_H
# include <fcntl.h>
#endif

#include <sixel.h>
#include "encoder.h"
#include "rgblookup.h"


static char *
arg_strdup(
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


static SIXELSTATUS
parse_x_colorspec(
    unsigned char       /* out */ **bgcolor,     /* destination buffer */
    char const          /* in */  *s,            /* source buffer */
    sixel_allocator_t   /* in */  *allocator)    /* allocator object for
                                                    destination buffer */
{
    SIXELSTATUS status = SIXEL_FALSE;
    char *p;
    unsigned char components[3];
    int index = 0;
    unsigned long v;
    char *endptr;
    char *buf = NULL;
    struct color const *pcolor;
    pcolor = lookup_rgb(s, strlen(s));
    if (pcolor) {
        *bgcolor = (unsigned char *)sixel_allocator_malloc(allocator, 3);
        if (*bgcolor == NULL) {
            sixel_helper_set_additional_message(
                "parse_x_colorspec: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        (*bgcolor)[0] = pcolor->r;
        (*bgcolor)[1] = pcolor->g;
        (*bgcolor)[2] = pcolor->b;
    } else if (s[0] == 'r' && s[1] == 'g' && s[2] == 'b' && s[3] == ':') {
        p = buf = arg_strdup(s + 4, allocator);
        if (buf == NULL) {
            sixel_helper_set_additional_message(
                "parse_x_colorspec: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        while (*p) {
            v = 0;
            for (endptr = p; endptr - p <= 12; ++endptr) {
                if (*endptr >= '0' && *endptr <= '9') {
                    v = (v << 4) | (*endptr - '0');
                } else if (*endptr >= 'a' && *endptr <= 'f') {
                    v = (v << 4) | (*endptr - 'a' + 10);
                } else if (*endptr >= 'A' && *endptr <= 'F') {
                    v = (v << 4) | (*endptr - 'A' + 10);
                } else {
                    break;
                }
            }
            if (endptr - p == 0) {
                break;
            }
            if (endptr - p > 4) {
                break;
            }
            v = v << ((4 - (endptr - p)) * 4) >> 8;
            components[index++] = (unsigned char)v;
            p = endptr;
            if (index == 3) {
                break;
            }
            if (*p == '\0') {
                break;
            }
            if (*p != '/') {
                break;
            }
            ++p;
        }
        if (index != 3 || *p != '\0' || *p == '/') {
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        *bgcolor = (unsigned char *)sixel_allocator_malloc(allocator, 3);
        if (*bgcolor == NULL) {
            sixel_helper_set_additional_message(
                "parse_x_colorspec: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        (*bgcolor)[0] = components[0];
        (*bgcolor)[1] = components[1];
        (*bgcolor)[2] = components[2];
    } else if (*s == '#') {
        buf = arg_strdup(s + 1, allocator);
        if (buf == NULL) {
            sixel_helper_set_additional_message(
                "parse_x_colorspec: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        for (p = endptr = buf; endptr - p <= 12; ++endptr) {
            if (*endptr >= '0' && *endptr <= '9') {
                *endptr -= '0';
            } else if (*endptr >= 'a' && *endptr <= 'f') {
                *endptr -= 'a' - 10;
            } else if (*endptr >= 'A' && *endptr <= 'F') {
                *endptr -= 'A' - 10;
            } else if (*endptr == '\0') {
                break;
            } else {
                status = SIXEL_BAD_ARGUMENT;
                goto end;
            }
        }
        if (endptr - p > 12) {
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        *bgcolor = (unsigned char *)sixel_allocator_malloc(allocator, 3);
        if (*bgcolor == NULL) {
            sixel_helper_set_additional_message(
                "parse_x_colorspec: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        switch (endptr - p) {
        case 3:
            (*bgcolor)[0] = (unsigned char)(p[0] << 4);
            (*bgcolor)[1] = (unsigned char)(p[1] << 4);
            (*bgcolor)[2] = (unsigned char)(p[2] << 4);
            break;
        case 6:
            (*bgcolor)[0] = (unsigned char)(p[0] << 4 | p[1]);
            (*bgcolor)[1] = (unsigned char)(p[2] << 4 | p[3]);
            (*bgcolor)[2] = (unsigned char)(p[4] << 4 | p[4]);
            break;
        case 9:
            (*bgcolor)[0] = (unsigned char)(p[0] << 4 | p[1]);
            (*bgcolor)[1] = (unsigned char)(p[3] << 4 | p[4]);
            (*bgcolor)[2] = (unsigned char)(p[6] << 4 | p[7]);
            break;
        case 12:
            (*bgcolor)[0] = (unsigned char)(p[0] << 4 | p[1]);
            (*bgcolor)[1] = (unsigned char)(p[4] << 4 | p[5]);
            (*bgcolor)[2] = (unsigned char)(p[8] << 4 | p[9]);
            break;
        default:
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
    } else {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    status = SIXEL_OK;
end:
    sixel_allocator_free(allocator, buf);

    return status;
}


/* generic writer function for passing to sixel_output_new() */
static int
sixel_write_callback(char *data, int size, void *priv)
{
    return write(*(int *)priv, data, size);
}


/* the writer function with hex-encoding for passing to sixel_output_new() */
static int
sixel_hex_write_callback(
    char    /* in */ *data,
    int     /* in */ size,
    void    /* in */ *priv)
{
    char hex[SIXEL_OUTPUT_PACKET_SIZE * 2];
    int i;
    int j;

    for (i = j = 0; i < size; ++i, ++j) {
        hex[j] = (data[i] >> 4) & 0xf;
        hex[j] += (hex[j] < 10 ? '0': ('a' - 10));
        hex[++j] = data[i] & 0xf;
        hex[j] += (hex[j] < 10 ? '0': ('a' - 10));
    }

    return write(*(int *)priv, hex, size * 2);
}


/* returns monochrome dithering context object */
static SIXELSTATUS
prepare_monochrome_palette(
    sixel_dither_t  /* out */ **dither,
     int            /* in */  finvert)
{
    SIXELSTATUS status = SIXEL_FALSE;

    if (finvert) {
        *dither = sixel_dither_get(SIXEL_BUILTIN_MONO_LIGHT);
    } else {
        *dither = sixel_dither_get(SIXEL_BUILTIN_MONO_DARK);
    }
    if (*dither == NULL) {
        sixel_helper_set_additional_message(
            "prepare_monochrome_palette: sixel_dither_get() failed.");
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }

    status = SIXEL_OK;

end:
    return status;
}


/* returns dithering context object with specified builtin palette */
static SIXELSTATUS
prepare_builtin_palette(
    sixel_dither_t /* out */ **dither,
    int            /* in */  builtin_palette)
{
    SIXELSTATUS status = SIXEL_FALSE;

    *dither = sixel_dither_get(builtin_palette);
    if (*dither == NULL) {
        sixel_helper_set_additional_message(
            "prepare_builtin_palette: sixel_dither_get() failed.");
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }

    status = SIXEL_OK;

end:
    return status;
}


typedef struct sixel_callback_context_for_mapfile {
    int reqcolors;
    sixel_dither_t *dither;
    sixel_allocator_t *allocator;
} sixel_callback_context_for_mapfile_t;


static SIXELSTATUS
load_image_callback_for_palette(sixel_frame_t *frame, void *data)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_callback_context_for_mapfile_t *callback_context;

    callback_context = (sixel_callback_context_for_mapfile_t *)data;

    switch (sixel_frame_get_pixelformat(frame)) {
    case SIXEL_PIXELFORMAT_PAL1:
    case SIXEL_PIXELFORMAT_PAL2:
    case SIXEL_PIXELFORMAT_PAL4:
    case SIXEL_PIXELFORMAT_PAL8:
        if (sixel_frame_get_palette(frame) == NULL) {
            status = SIXEL_LOGIC_ERROR;
            goto end;
        }
        status = sixel_dither_new(
            &callback_context->dither,
            sixel_frame_get_ncolors(frame),
            callback_context->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        sixel_dither_set_palette(callback_context->dither,
                                 sixel_frame_get_palette(frame));
        status = SIXEL_OK;
        break;
    case SIXEL_PIXELFORMAT_G1:
        callback_context->dither = sixel_dither_get(SIXEL_BUILTIN_G1);
        status = SIXEL_OK;
        break;
    case SIXEL_PIXELFORMAT_G2:
        callback_context->dither = sixel_dither_get(SIXEL_BUILTIN_G2);
        status = SIXEL_OK;
        break;
    case SIXEL_PIXELFORMAT_G4:
        callback_context->dither = sixel_dither_get(SIXEL_BUILTIN_G4);
        status = SIXEL_OK;
        break;
    case SIXEL_PIXELFORMAT_G8:
        callback_context->dither = sixel_dither_get(SIXEL_BUILTIN_G8);
        status = SIXEL_OK;
        break;
    default:
        status = sixel_dither_new(
            &callback_context->dither,
            callback_context->reqcolors,
            callback_context->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        status = sixel_dither_initialize(callback_context->dither,
                                         sixel_frame_get_pixels(frame),
                                         sixel_frame_get_width(frame),
                                         sixel_frame_get_height(frame),
                                         sixel_frame_get_pixelformat(frame),
                                         SIXEL_LARGE_NORM,
                                         SIXEL_REP_CENTER_BOX,
                                         SIXEL_QUALITY_HIGH);
        if (SIXEL_FAILED(status)) {
            sixel_dither_unref(callback_context->dither);
            goto end;
        }

        status = SIXEL_OK;

        break;
    }

end:
    return status;
}


static SIXELSTATUS
prepare_specified_palette(
    sixel_dither_t **dither,
    char const *mapfile,
    int reqcolors,
    unsigned char *bgcolor,
    int finsecure,
    int const *cancel_flag,
    sixel_allocator_t *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;

    sixel_callback_context_for_mapfile_t callback_context;

    callback_context.reqcolors = reqcolors;
    callback_context.dither = NULL;
    callback_context.allocator = allocator;

    status = sixel_helper_load_image_file(mapfile,
                                          1,   /* fstatic */
                                          1,   /* fuse_palette */
                                          256, /* reqcolors */
                                          bgcolor,
                                          SIXEL_LOOP_DISABLE,
                                          load_image_callback_for_palette,
                                          finsecure,
                                          cancel_flag,
                                          &callback_context,
                                          allocator);
    if (status != SIXEL_OK) {
        return status;
    }

    *dither = callback_context.dither;

    return status;
}


static SIXELSTATUS
prepare_palette(sixel_dither_t **dither,
                sixel_dither_t *former_dither,
                sixel_frame_t *frame,
                sixel_encoder_t *encoder)
{
    SIXELSTATUS status = SIXEL_FALSE;
    int histogram_colors;

    if (encoder->highcolor) {
        if (former_dither) {
            *dither = former_dither;
        } else {
            status = sixel_dither_new(dither, (-1), encoder->allocator);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
        }
    } else if (encoder->monochrome) {
        if (former_dither) {
            *dither = former_dither;
        } else {
            status = prepare_monochrome_palette(dither,
                                                encoder->finvert);
            if (status != SIXEL_OK) {
                goto end;
            }
        }
    } else if (encoder->mapfile) {
        if (former_dither) {
            *dither = former_dither;
        } else {
            status = prepare_specified_palette(dither,
                                               encoder->mapfile,
                                               encoder->reqcolors,
                                               encoder->bgcolor,
                                               encoder->finsecure,
                                               encoder->cancel_flag,
                                               encoder->allocator);
            if (status != SIXEL_OK) {
                goto end;
            }
        }
    } else if (encoder->builtin_palette) {
        if (former_dither) {
            *dither = former_dither;
        } else {
            status = prepare_builtin_palette(
                dither, encoder->builtin_palette);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
        }
    } else if (sixel_frame_get_palette(frame) &&
               (sixel_frame_get_pixelformat(frame) & SIXEL_FORMATTYPE_PALETTE)) {
        status = sixel_dither_new(dither, sixel_frame_get_ncolors(frame),
                                  encoder->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        sixel_dither_set_palette(*dither, sixel_frame_get_palette(frame));
        sixel_dither_set_pixelformat(*dither, sixel_frame_get_pixelformat(frame));
        if (sixel_frame_get_transparent(frame) != (-1)) {
            sixel_dither_set_transparent(*dither, sixel_frame_get_transparent(frame));
        }
    } else if (sixel_frame_get_pixelformat(frame) & SIXEL_FORMATTYPE_GRAYSCALE) {
        switch (sixel_frame_get_pixelformat(frame)) {
        case SIXEL_PIXELFORMAT_G1:
            *dither = sixel_dither_get(SIXEL_BUILTIN_G1);
            break;
        case SIXEL_PIXELFORMAT_G2:
            *dither = sixel_dither_get(SIXEL_BUILTIN_G2);
            break;
        case SIXEL_PIXELFORMAT_G4:
            *dither = sixel_dither_get(SIXEL_BUILTIN_G4);
            break;
        case SIXEL_PIXELFORMAT_G8:
            *dither = sixel_dither_get(SIXEL_BUILTIN_G8);
            break;
        default:
            status = SIXEL_LOGIC_ERROR;
            goto end;
        }
        sixel_dither_set_pixelformat(*dither, sixel_frame_get_pixelformat(frame));
    } else {
        if (former_dither) {
            sixel_dither_unref(former_dither);
        }
        status = sixel_dither_new(dither, encoder->reqcolors, encoder->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        status = sixel_dither_initialize(*dither,
                                         sixel_frame_get_pixels(frame),
                                         sixel_frame_get_width(frame),
                                         sixel_frame_get_height(frame),
                                         sixel_frame_get_pixelformat(frame),
                                         encoder->method_for_largest,
                                         encoder->method_for_rep,
                                         encoder->quality_mode);
        if (SIXEL_FAILED(status)) {
            sixel_dither_unref(*dither);
            goto end;
        }
        histogram_colors = sixel_dither_get_num_of_histogram_colors(*dither);
        if (histogram_colors <= encoder->reqcolors) {
            encoder->method_for_diffuse = SIXEL_DIFFUSE_NONE;
        }
        sixel_dither_set_pixelformat(*dither, sixel_frame_get_pixelformat(frame));
    }

    status = SIXEL_OK;

end:
    return status;
}


static SIXELSTATUS
do_resize(
    sixel_frame_t *frame,
    sixel_encoder_t *encoder
)
{
    SIXELSTATUS status = SIXEL_OK;

    if (encoder->percentwidth > 0) {
        encoder->pixelwidth = sixel_frame_get_width(frame) * encoder->percentwidth / 100;
    }
    if (encoder->percentheight > 0) {
        encoder->pixelheight = sixel_frame_get_height(frame) * encoder->percentheight / 100;
    }
    if (encoder->pixelwidth > 0 && encoder->pixelheight <= 0) {
        encoder->pixelheight
            = sixel_frame_get_height(frame) * encoder->pixelwidth / sixel_frame_get_width(frame);
    }
    if (encoder->pixelheight > 0 && encoder->pixelwidth <= 0) {
        encoder->pixelwidth
            = sixel_frame_get_width(frame) * encoder->pixelheight / sixel_frame_get_height(frame);
    }

    if (encoder->pixelwidth > 0 && encoder->pixelheight > 0) {

        status = sixel_frame_resize(frame,
                                    encoder->pixelwidth,
                                    encoder->pixelheight,
                                    encoder->method_for_resampling);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }

    return status;
}


static SIXELSTATUS
do_crop(
    sixel_frame_t *frame,
    sixel_encoder_t *encoder
)
{
    SIXELSTATUS status = SIXEL_OK;
    int width;
    int height;

    width = sixel_frame_get_width(frame);
    height = sixel_frame_get_height(frame);

    /* clipping */
    if (encoder->clipwidth + encoder->clipx > width) {
        if (encoder->clipx > width) {
            encoder->clipwidth = 0;
        } else {
            encoder->clipwidth = width - encoder->clipx;
        }
    }
    if (encoder->clipheight + encoder->clipy > height) {
        if (encoder->clipy > height) {
            encoder->clipheight = 0;
        } else {
            encoder->clipheight = height - encoder->clipy;
        }
    }
    if (encoder->clipwidth > 0 && encoder->clipheight > 0) {
        status = sixel_frame_clip(frame,
                                  encoder->clipx,
                                  encoder->clipy,
                                  encoder->clipwidth,
                                  encoder->clipheight);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }

    return status;
}


static void
print_palette(sixel_dither_t *dither)
{
    unsigned char *palette;
    int i;

    palette = sixel_dither_get_palette(dither);
    fprintf(stderr, "palette:\n");
    for (i = 0; i < sixel_dither_get_num_of_palette_colors(dither); ++i) {
        fprintf(stderr, "%d: #%02x%02x%02x\n", i,
                palette[i * 3 + 1],
                palette[i * 3 + 2],
                palette[i * 3 + 3]);
    }
}


static int
wait_stdin(int usec)
{
#if HAVE_SYS_SELECT_H
    fd_set rfds;
    struct timeval tv;
#endif  /* HAVE_SYS_SELECT_H */
    int ret = 0;

#if HAVE_SYS_SELECT_H
    tv.tv_sec = usec / 1000000;
    tv.tv_usec = usec % 1000000;
    FD_ZERO(&rfds);
    FD_SET(STDIN_FILENO, &rfds);
    ret = select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv);
#else
    (void) usec;
#endif  /* HAVE_SYS_SELECT_H */

    return ret;
}


static SIXELSTATUS
output_sixel_without_macro(
    unsigned char       /* in */ *buffer,
    int                 /* in */ width,
    int                 /* in */ height,
    int                 /* in */ pixelformat,
    int                 /* in */ delay,
    sixel_dither_t      /* in */ *dither,
    sixel_output_t      /* in */ *output,
    sixel_encoder_t     /* in */ *encoder)
{
    SIXELSTATUS status = SIXEL_OK;
    int dulation = 0;
    static unsigned char *p;
    int depth;
    char message[256];
    int nwrite;
#if HAVE_USLEEP
    int lag = 0;
# if HAVE_CLOCK
    clock_t start;
# endif
#endif

    if (encoder == NULL) {
        sixel_helper_set_additional_message(
            "output_sixel_without_macro: encoder object is null.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    
    if (!encoder->mapfile && !encoder->monochrome
            && !encoder->highcolor && !encoder->builtin_palette) {
        sixel_dither_set_optimize_palette(dither, 1);
    }

    depth = sixel_helper_compute_depth(pixelformat);
    if (depth < 0) {
        status = SIXEL_LOGIC_ERROR;
        nwrite = sprintf(message,
                         "output_sixel_without_macro: "
                         "sixel_helper_compute_depth(%08x) failed.",
                         pixelformat);
        if (nwrite > 0) {
            sixel_helper_set_additional_message(message);
        }
        goto end;
    }

    p = (unsigned char *)sixel_allocator_malloc(encoder->allocator, width * height * depth);
    if (p == NULL) {
        sixel_helper_set_additional_message(
            "output_sixel_without_macro: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
#if HAVE_USLEEP && HAVE_CLOCK
    start = clock();
#endif
#if HAVE_USLEEP
    if (!encoder->fignore_delay && delay > 0) {
# if HAVE_CLOCK
        dulation = (clock() - start) * 1000 * 1000 / CLOCKS_PER_SEC - lag;
        lag = 0;
# else
        dulation = 0;
# endif
        if (dulation < 10000 * delay) {
            usleep(10000 * delay - dulation);
        } else {
            lag = 10000 * delay - dulation;
        }
    }
#endif

    memcpy(p, buffer, width * height * depth);

    if (encoder->cancel_flag && *encoder->cancel_flag) {
        goto end;
    }

    status = sixel_encode(p, width, height, depth, dither, output);
    if (status != 0) {
        goto end;
    }

end:
    sixel_allocator_free(encoder->allocator, p);

    return status;
}


static SIXELSTATUS
output_sixel_with_macro(
    unsigned char *frame,
    int sx,
    int sy,
    int delay,
    int frame_no,
    int loop_count,
    sixel_dither_t *dither,
    sixel_output_t *output,
    sixel_encoder_t *encoder
)
{
    SIXELSTATUS status = SIXEL_OK;
    int dulation = 0;
    char buffer[256];
    int nwrite;
#if HAVE_USLEEP
    int lag = 0;
# if HAVE_CLOCK
    clock_t start;
# endif
#endif

#if HAVE_USLEEP && HAVE_CLOCK
    start = clock();
#endif
    if (loop_count == 0) {
        if (encoder->macro_number >= 0) {
            nwrite = sprintf(buffer, "\033P%d;0;1!z", encoder->macro_number);
        } else {
            nwrite = sprintf(buffer, "\033P%d;0;1!z", frame_no);
        }
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "output_sixel_with_macro: sprintf() failed.");
            goto end;
        }
        nwrite = sixel_write_callback(buffer, strlen(buffer), &encoder->outfd);
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "output_sixel_with_macro: sixel_write_callback() failed.");
            goto end;
        }

        status = sixel_encode(frame, sx, sy, /* unused */ 3, dither, output);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        nwrite = sixel_write_callback("\033\\", 2, &encoder->outfd);
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "output_sixel_with_macro: sixel_write_callback() failed.");
            goto end;
        }
    }
    if (encoder->macro_number < 0) {
        nwrite = sprintf(buffer, "\033[%d*z", frame_no);
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "output_sixel_with_macro: sprintf() failed.");
        }
        nwrite = sixel_write_callback(buffer, strlen(buffer), &encoder->outfd);
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "output_sixel_with_macro: sixel_write_callback() failed.");
            goto end;
        }
#if HAVE_USLEEP
        if (delay > 0 && !encoder->fignore_delay) {
# if HAVE_CLOCK
            dulation = (clock() - start) * 1000 * 1000 / CLOCKS_PER_SEC - lag;
            lag = 0;
# else
            dulation = 0;
# endif
            if (dulation < 10000 * delay) {
                usleep(10000 * delay - dulation);
            } else {
                lag = 10000 * delay - dulation;
            }
        }
#endif
    }

end:
    return status;
}


#if HAVE_TERMIOS_H && HAVE_SYS_IOCTL_H && HAVE_ISATTY
static SIXELSTATUS
tty_cbreak(struct termios *old_termios, struct termios *new_termios)
{
    SIXELSTATUS status = SIXEL_FALSE;
    int ret;

    /* set the terminal to cbreak mode */
    ret = tcgetattr(STDIN_FILENO, old_termios);
    if (ret != 0) {
        status = (SIXEL_LIBC_ERROR | (errno & 0xff));
        sixel_helper_set_additional_message(
            "tty_cbreak: tcgetattr() failed.");
        goto end;
    }

    (void) memcpy(new_termios, old_termios, sizeof(*old_termios));
    new_termios->c_lflag &= ~(ECHO | ICANON);
    new_termios->c_cc[VMIN] = 1;
    new_termios->c_cc[VTIME] = 0;

    ret = tcsetattr(STDIN_FILENO, TCSAFLUSH, new_termios);
    if (ret != 0) {
        status = (SIXEL_LIBC_ERROR | (errno & 0xff));
        sixel_helper_set_additional_message(
            "tty_cbreak: tcsetattr() failed.");
        goto end;
    }

    status = SIXEL_OK;

end:
    return status;
}
#endif  /* HAVE_TERMIOS_H && HAVE_SYS_IOCTL_H && HAVE_ISATTY */


#if HAVE_TERMIOS_H && HAVE_SYS_IOCTL_H && HAVE_ISATTY
static SIXELSTATUS
tty_restore(struct termios *old_termios)
{
    SIXELSTATUS status = SIXEL_FALSE;
    int ret;

    ret = tcsetattr(STDIN_FILENO, TCSAFLUSH, old_termios);
    if (ret != 0) {
        status = (SIXEL_LIBC_ERROR | (errno & 0xff));
        sixel_helper_set_additional_message(
            "tty_restore: tcsetattr() failed.");
        goto end;
    }

    status = SIXEL_OK;

end:
    return status;
}
#endif  /* HAVE_TERMIOS_H && HAVE_SYS_IOCTL_H && HAVE_ISATTY */


static SIXELSTATUS
scroll_on_demand(
    sixel_encoder_t /* in */ *encoder,
    sixel_frame_t   /* in */ *frame)
{
    SIXELSTATUS status = SIXEL_FALSE;
    int nwrite;
#if HAVE_TERMIOS_H && HAVE_SYS_IOCTL_H && HAVE_ISATTY
    struct winsize size = {0, 0, 0, 0};
    struct termios old_termios;
    struct termios new_termios;
    int row = 0;
    int col = 0;
    int pixelheight;
    int cellheight;
    int scroll;
    char buffer[256];
    int result;

    /* confirm I/O file descriptors are tty devices */
    if (!isatty(STDIN_FILENO) || !isatty(encoder->outfd)) {
        nwrite = sixel_write_callback("\033[H", 3, &encoder->outfd);
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "scroll_on_demand: sixel_write_callback() failed.");
            goto end;
        }
        status = SIXEL_OK;
        goto end;
    }

    /* request terminal size to tty device with TIOCGWINSZ ioctl */
    result = ioctl(encoder->outfd, TIOCGWINSZ, &size);
    if (result != 0) {
        status = (SIXEL_LIBC_ERROR | (errno & 0xff));
        sixel_helper_set_additional_message("ioctl() failed.");
        goto end;
    }

    /* if we can not retrieve terminal pixel size over TIOCGWINSZ ioctl,
       return immediatly */
    if (size.ws_ypixel <= 0) {
        nwrite = sixel_write_callback("\033[H", 3, &encoder->outfd);
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "scroll_on_demand: sixel_write_callback() failed.");
            goto end;
        }
        status = SIXEL_OK;
        goto end;
    }

    /* if input source is animation and frame No. is more than 1,
       output DECSC sequence */
    if (sixel_frame_get_loop_no(frame) != 0 ||
        sixel_frame_get_frame_no(frame) != 0) {
        nwrite = sixel_write_callback("\0338", 2, &encoder->outfd);
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "scroll_on_demand: sixel_write_callback() failed.");
            goto end;
        }
        status = SIXEL_OK;
        goto end;
    }

    /* set the terminal to cbreak mode */
    status = tty_cbreak(&old_termios, &new_termios);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    /* request cursor position report */
    nwrite = sixel_write_callback("\033[6n", 4, &encoder->outfd);
    if (nwrite < 0) {
        status = (SIXEL_LIBC_ERROR | (errno & 0xff));
        sixel_helper_set_additional_message(
            "scroll_on_demand: sixel_write_callback() failed.");
        goto end;
    }

    /* wait cursor position report */
    if (wait_stdin(1000 * 1000) == (-1)) { /* wait up to 1 sec */
        nwrite = sixel_write_callback("\033[H", 3, &encoder->outfd);
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "scroll_on_demand: sixel_write_callback() failed.");
            goto end;
        }
        status = SIXEL_OK;
        goto end;
    }

    /* scan cursor position report */
    if (scanf("\033[%d;%dR", &row, &col) != 2) {
        nwrite = sixel_write_callback("\033[H", 3, &encoder->outfd);
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "scroll_on_demand: sixel_write_callback() failed.");
            goto end;
        }
        status = SIXEL_OK;
        goto end;
    }

    /* restore the terminal mode */
    status = tty_restore(&old_termios);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    /* calculate scrolling amount in pixels */
    pixelheight = sixel_frame_get_height(frame);
    cellheight = pixelheight * size.ws_row / size.ws_ypixel + 1;
    scroll = cellheight + row - size.ws_row + 1;
    if (scroll > 0) {
        nwrite = sprintf(buffer, "\033[%dS\033[%dA", scroll, scroll);
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "scroll_on_demand: sprintf() failed.");
        }
        nwrite = sixel_write_callback(buffer, strlen(buffer), &encoder->outfd);
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "scroll_on_demand: sixel_write_callback() failed.");
            goto end;
        }
    }

    /* emit DECSC sequence */
    nwrite = sixel_write_callback("\0337", 2, &encoder->outfd);
    if (nwrite < 0) {
        status = (SIXEL_LIBC_ERROR | (errno & 0xff));
        sixel_helper_set_additional_message(
            "scroll_on_demand: sixel_write_callback() failed.");
        goto end;
    }
#else
    (void) frame;
    nwrite = sixel_write_callback("\033[H", 3, &encoder->outfd);
    if (nwrite < 0) {
        status = (SIXEL_LIBC_ERROR | (errno & 0xff));
        sixel_helper_set_additional_message(
            "scroll_on_demand: sixel_write_callback() failed.");
        goto end;
    }
#endif  /* HAVE_TERMIOS_H && HAVE_SYS_IOCTL_H && HAVE_ISATTY */

    status = SIXEL_OK;

end:
    return status;
}


/* called when image loader component load a image frame */
static SIXELSTATUS
load_image_callback(sixel_frame_t *frame, void *data)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_encoder_t *encoder;
    sixel_dither_t *dither = NULL;
    sixel_output_t *output = NULL;
    int nwrite;

    encoder = (sixel_encoder_t *)data;

    /* evaluate -w, -h, and -c option: crop/scale input source */
    if (encoder->clipfirst) {
        /* clipping */
        status = do_crop(frame, encoder);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        /* scaling */
        status = do_resize(frame, encoder);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    } else {
        /* scaling */
        status = do_resize(frame, encoder);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        /* clipping */
        status = do_crop(frame, encoder);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    /* prepare dither context */
    status = prepare_palette(&dither, encoder->dither_cache, frame, encoder);
    if (status != SIXEL_OK) {
        goto end;
    }

    if (encoder->dither_cache != NULL) {
        encoder->dither_cache = dither;
        sixel_dither_ref(dither);
    }

    /* evaluate -v option: print palette */
    if (encoder->verbose) {
        if (!(sixel_frame_get_pixelformat(frame) & SIXEL_FORMATTYPE_GRAYSCALE)) {
            print_palette(dither);
        }
    }

    /* evaluate -d option: set method for diffusion */
    sixel_dither_set_diffusion_type(dither, encoder->method_for_diffuse);

    /* evaluate -C option: set complexion score */
    if (encoder->complexion > 1) {
        sixel_dither_set_complexion_score(dither, encoder->complexion);
    }

    /* create output context */
    if (encoder->fuse_macro || encoder->macro_number >= 0) {
        /* -u or -n option */
        status = sixel_output_new(&output,
                                  sixel_hex_write_callback,
                                  &encoder->outfd,
                                  encoder->allocator);
    } else {
        status = sixel_output_new(&output,
                                  sixel_write_callback,
                                  &encoder->outfd,
                                  encoder->allocator);
    }
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    sixel_output_set_8bit_availability(output, encoder->f8bit);
    sixel_output_set_palette_type(output, encoder->palette_type);
    sixel_output_set_penetrate_multiplexer(
        output, encoder->penetrate_multiplexer);
    sixel_output_set_encode_policy(output, encoder->encode_policy);

    if (sixel_frame_get_multiframe(frame) && !encoder->fstatic) {
        (void) scroll_on_demand(encoder, frame);
    }

    if (encoder->cancel_flag && *encoder->cancel_flag) {
        status = SIXEL_INTERRUPTED;
        goto end;
    }

    /* output sixel: junction of multi-frame processing strategy */
    if (encoder->fuse_macro) {  /* -u option */
        /* use macro */
        status = output_sixel_with_macro(sixel_frame_get_pixels(frame),
                                         sixel_frame_get_width(frame),
                                         sixel_frame_get_height(frame),
                                         sixel_frame_get_delay(frame),
                                         sixel_frame_get_frame_no(frame),
                                         sixel_frame_get_loop_no(frame),
                                         dither,
                                         output,
                                         encoder);
    } else if (encoder->macro_number >= 0) { /* -n option */
        /* use macro */
        status = output_sixel_with_macro(sixel_frame_get_pixels(frame),
                                         sixel_frame_get_width(frame),
                                         sixel_frame_get_height(frame),
                                         sixel_frame_get_delay(frame),
                                         sixel_frame_get_frame_no(frame),
                                         sixel_frame_get_loop_no(frame),
                                         dither,
                                         output,
                                         encoder);
    } else {
        /* do not use macro */
        status = output_sixel_without_macro(sixel_frame_get_pixels(frame),
                                            sixel_frame_get_width(frame),
                                            sixel_frame_get_height(frame),
                                            sixel_frame_get_pixelformat(frame),
                                            sixel_frame_get_delay(frame),
                                            dither,
                                            output,
                                            encoder);
    }

    if (encoder->cancel_flag && *encoder->cancel_flag) {
        nwrite = sixel_write_callback("\x18\033\\", 3, &encoder->outfd);
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "load_image_callback: sixel_write_callback() failed.");
            goto end;
        }
        status = SIXEL_INTERRUPTED;
    }

    if (SIXEL_FAILED(status)) {
        goto end;
    }

end:
    if (output) {
        sixel_output_unref(output);
    }
    if (dither) {
        sixel_dither_unref(dither);
    }

    return status;
}


/* create encoder object */
SIXELAPI SIXELSTATUS
sixel_encoder_new(
    sixel_encoder_t     /* out */ **ppencoder, /* encoder object to be created */
    sixel_allocator_t   /* in */  *allocator)  /* allocator, null if you use
                                                  default allocator */
{
    SIXELSTATUS status = SIXEL_FALSE;
    char const *env_default_bgcolor;
    char const *env_default_ncolors;
    int ncolors;

    if (allocator == NULL) {
        status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    } else {
        sixel_allocator_ref(allocator);
    }

    *ppencoder
        = (sixel_encoder_t *)sixel_allocator_malloc(allocator,
                                                    sizeof(sixel_encoder_t));
    if (*ppencoder == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encoder_new: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        sixel_allocator_unref(allocator);
        goto end;
    }

    (*ppencoder)->ref                   = 1;
    (*ppencoder)->reqcolors             = (-1);
    (*ppencoder)->mapfile               = NULL;
    (*ppencoder)->monochrome            = 0;
    (*ppencoder)->highcolor             = 0;
    (*ppencoder)->builtin_palette       = 0;
    (*ppencoder)->method_for_diffuse    = SIXEL_DIFFUSE_AUTO;
    (*ppencoder)->method_for_largest    = SIXEL_LARGE_AUTO;
    (*ppencoder)->method_for_rep        = SIXEL_REP_AUTO;
    (*ppencoder)->quality_mode          = SIXEL_QUALITY_AUTO;
    (*ppencoder)->method_for_resampling = SIXEL_RES_BILINEAR;
    (*ppencoder)->loop_mode             = SIXEL_LOOP_AUTO;
    (*ppencoder)->palette_type          = SIXEL_PALETTETYPE_AUTO;
    (*ppencoder)->f8bit                 = 0;
    (*ppencoder)->finvert               = 0;
    (*ppencoder)->fuse_macro            = 0;
    (*ppencoder)->fignore_delay         = 0;
    (*ppencoder)->complexion            = 1;
    (*ppencoder)->fstatic               = 0;
    (*ppencoder)->pixelwidth            = -1;
    (*ppencoder)->pixelheight           = -1;
    (*ppencoder)->percentwidth          = -1;
    (*ppencoder)->percentheight         = -1;
    (*ppencoder)->clipx                 = 0;
    (*ppencoder)->clipy                 = 0;
    (*ppencoder)->clipwidth             = 0;
    (*ppencoder)->clipheight            = 0;
    (*ppencoder)->clipfirst             = 0;
    (*ppencoder)->macro_number          = -1;
    (*ppencoder)->verbose               = 0;
    (*ppencoder)->penetrate_multiplexer = 0;
    (*ppencoder)->encode_policy         = SIXEL_ENCODEPOLICY_AUTO;
    (*ppencoder)->pipe_mode             = 0;
    (*ppencoder)->bgcolor               = NULL;
    (*ppencoder)->outfd                 = STDOUT_FILENO;
    (*ppencoder)->finsecure             = 0;
    (*ppencoder)->cancel_flag           = NULL;
    (*ppencoder)->dither_cache          = NULL;
    (*ppencoder)->allocator             = allocator;

    env_default_bgcolor = getenv("SIXEL_BGCOLOR");
    if (env_default_bgcolor) {
        status = parse_x_colorspec(&(*ppencoder)->bgcolor,
                                   env_default_bgcolor,
                                   allocator);
        if (SIXEL_FAILED(status)) {
            sixel_allocator_free(allocator, *ppencoder);
            sixel_allocator_unref(allocator);
            *ppencoder = NULL;
            goto end;
        }
    }

    env_default_ncolors = getenv("SIXEL_COLORS");
    if (env_default_ncolors) {
        ncolors = atoi(env_default_ncolors);
        if (ncolors > 1 && ncolors <= 256) {
            (*ppencoder)->reqcolors = ncolors;
        }
    }

    sixel_allocator_ref(allocator);

    status = SIXEL_OK;

end:
    return status;
}


/* create encoder object */
SIXELAPI /* deprecated */ sixel_encoder_t *
sixel_encoder_create(void)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_encoder_t *encoder = NULL;

    status = sixel_encoder_new(&encoder, NULL);
    if (SIXEL_FAILED(status)) {
        return NULL;
    }

    return encoder;
}


static void
sixel_encoder_destroy(sixel_encoder_t *encoder)
{
    sixel_allocator_t *allocator;

    if (encoder) {
        allocator = encoder->allocator;
        sixel_allocator_free(allocator, encoder->mapfile);
        sixel_allocator_free(allocator, encoder->bgcolor);
        sixel_dither_unref(encoder->dither_cache);
        if (encoder->outfd
            && encoder->outfd != STDOUT_FILENO
            && encoder->outfd != STDERR_FILENO) {
            close(encoder->outfd);
        }
        sixel_allocator_free(allocator, encoder);
        sixel_allocator_unref(allocator);
    }
}


SIXELAPI void
sixel_encoder_ref(sixel_encoder_t *encoder)
{
    /* TODO: be thread safe */
    ++encoder->ref;
}


SIXELAPI void
sixel_encoder_unref(sixel_encoder_t *encoder)
{
    /* TODO: be thread safe */
    if (encoder != NULL && --encoder->ref == 0) {
        sixel_encoder_destroy(encoder);
    }
}


SIXELAPI SIXELSTATUS
sixel_encoder_set_cancel_flag(
    sixel_encoder_t /* in */ *encoder,
    int             /* in */ *cancel_flag
)
{
    SIXELSTATUS status = SIXEL_OK;

    encoder->cancel_flag = cancel_flag;

    return status;
}


SIXELAPI SIXELSTATUS
sixel_encoder_setopt(
    sixel_encoder_t /* in */ *encoder,
    int             /* in */ arg,
    char const      /* in */ *optarg)
{
    SIXELSTATUS status = SIXEL_FALSE;
    int number;
    int parsed;
    char unit[32];

    sixel_encoder_ref(encoder);

    switch(arg) {
    case SIXEL_OPTFLAG_OUTFILE:  /* o */
        if (*optarg == '\0') {
            sixel_helper_set_additional_message(
                "no file name specified.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        if (strcmp(optarg, "-") != 0) {
            if (encoder->outfd && encoder->outfd != STDOUT_FILENO) {
                close(encoder->outfd);
            }
            encoder->outfd = open(optarg,
                                  O_RDWR|O_CREAT,
                                  S_IREAD|S_IWRITE);
        }
        break;
    case SIXEL_OPTFLAG_7BIT_MODE:  /* 7 */
        encoder->f8bit = 0;
        break;
    case SIXEL_OPTFLAG_8BIT_MODE:  /* 8 */
        encoder->f8bit = 1;
        break;
    case SIXEL_OPTFLAG_COLORS:  /* p */
        encoder->reqcolors = atoi(optarg);
        break;
    case SIXEL_OPTFLAG_MAPFILE:  /* m */
        if (encoder->mapfile) {
            sixel_allocator_free(encoder->allocator, encoder->mapfile);
        }
        encoder->mapfile = arg_strdup(optarg, encoder->allocator);
        if (encoder->mapfile == NULL) {
            sixel_helper_set_additional_message(
                "sixel_encoder_setopt: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_MONOCHROME:  /* e */
        encoder->monochrome = 1;
        break;
    case SIXEL_OPTFLAG_HIGH_COLOR:  /* I */
        encoder->highcolor = 1;
        break;
    case SIXEL_OPTFLAG_BUILTIN_PALETTE:  /* b */
        if (strcmp(optarg, "xterm16") == 0) {
            encoder->builtin_palette = SIXEL_BUILTIN_XTERM16;
        } else if (strcmp(optarg, "xterm256") == 0) {
            encoder->builtin_palette = SIXEL_BUILTIN_XTERM256;
        } else if (strcmp(optarg, "vt340mono") == 0) {
            encoder->builtin_palette = SIXEL_BUILTIN_VT340_MONO;
        } else if (strcmp(optarg, "vt340color") == 0) {
            encoder->builtin_palette = SIXEL_BUILTIN_VT340_COLOR;
        } else if (strcmp(optarg, "gray1") == 0) {
            encoder->builtin_palette = SIXEL_BUILTIN_G1;
        } else if (strcmp(optarg, "gray2") == 0) {
            encoder->builtin_palette = SIXEL_BUILTIN_G2;
        } else if (strcmp(optarg, "gray4") == 0) {
            encoder->builtin_palette = SIXEL_BUILTIN_G4;
        } else if (strcmp(optarg, "gray8") == 0) {
            encoder->builtin_palette = SIXEL_BUILTIN_G8;
        } else {
            sixel_helper_set_additional_message(
                    "cannot parse builtin palette option.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_DIFFUSION:  /* d */
        /* parse --diffusion option */
        if (strcmp(optarg, "auto") == 0) {
            encoder->method_for_diffuse = SIXEL_DIFFUSE_AUTO;
        } else if (strcmp(optarg, "none") == 0) {
            encoder->method_for_diffuse = SIXEL_DIFFUSE_NONE;
        } else if (strcmp(optarg, "fs") == 0) {
            encoder->method_for_diffuse = SIXEL_DIFFUSE_FS;
        } else if (strcmp(optarg, "atkinson") == 0) {
            encoder->method_for_diffuse = SIXEL_DIFFUSE_ATKINSON;
        } else if (strcmp(optarg, "jajuni") == 0) {
            encoder->method_for_diffuse = SIXEL_DIFFUSE_JAJUNI;
        } else if (strcmp(optarg, "stucki") == 0) {
            encoder->method_for_diffuse = SIXEL_DIFFUSE_STUCKI;
        } else if (strcmp(optarg, "burkes") == 0) {
            encoder->method_for_diffuse = SIXEL_DIFFUSE_BURKES;
        } else {
            sixel_helper_set_additional_message(
                "specified diffusion method is not supported.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_FIND_LARGEST:  /* f */
        /* parse --find-largest option */
        if (optarg) {
            if (strcmp(optarg, "auto") == 0) {
                encoder->method_for_largest = SIXEL_LARGE_AUTO;
            } else if (strcmp(optarg, "norm") == 0) {
                encoder->method_for_largest = SIXEL_LARGE_NORM;
            } else if (strcmp(optarg, "lum") == 0) {
                encoder->method_for_largest = SIXEL_LARGE_LUM;
            } else {
                sixel_helper_set_additional_message(
                    "specified finding method is not supported.");
                status = SIXEL_BAD_ARGUMENT;
                goto end;
            }
        }
        break;
    case SIXEL_OPTFLAG_SELECT_COLOR:  /* s */
        /* parse --select-color option */
        if (strcmp(optarg, "auto") == 0) {
            encoder->method_for_rep = SIXEL_REP_AUTO;
        } else if (strcmp(optarg, "center") == 0) {
            encoder->method_for_rep = SIXEL_REP_CENTER_BOX;
        } else if (strcmp(optarg, "average") == 0) {
            encoder->method_for_rep = SIXEL_REP_AVERAGE_COLORS;
        } else if ((strcmp(optarg, "histogram") == 0) ||
                   (strcmp(optarg, "histgram") == 0)) {
            encoder->method_for_rep = SIXEL_REP_AVERAGE_PIXELS;
        } else {
            sixel_helper_set_additional_message(
                "specified finding method is not supported.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_CROP:  /* c */
        number = sscanf(optarg, "%dx%d+%d+%d",
                        &encoder->clipwidth, &encoder->clipheight,
                        &encoder->clipx, &encoder->clipy);
        if (number != 4) {
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        if (encoder->clipwidth <= 0 || encoder->clipheight <= 0) {
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        if (encoder->clipx < 0 || encoder->clipy < 0) {
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        encoder->clipfirst = 0;
        break;
    case SIXEL_OPTFLAG_WIDTH:  /* w */
        parsed = sscanf(optarg, "%d%2s", &number, unit);
        if (parsed == 2 && strcmp(unit, "%") == 0) {
            encoder->pixelwidth = (-1);
            encoder->percentwidth = number;
        } else if (parsed == 1 || (parsed == 2 && strcmp(unit, "px") == 0)) {
            encoder->pixelwidth = number;
            encoder->percentwidth = (-1);
        } else if (strcmp(optarg, "auto") == 0) {
            encoder->pixelwidth = (-1);
            encoder->percentwidth = (-1);
        } else {
            sixel_helper_set_additional_message(
                "cannot parse -w/--width option.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        if (encoder->clipwidth) {
            encoder->clipfirst = 1;
        }
        break;
    case SIXEL_OPTFLAG_HEIGHT:  /* h */
        parsed = sscanf(optarg, "%d%2s", &number, unit);
        if (parsed == 2 && strcmp(unit, "%") == 0) {
            encoder->pixelheight = (-1);
            encoder->percentheight = number;
        } else if (parsed == 1 || (parsed == 2 && strcmp(unit, "px") == 0)) {
            encoder->pixelheight = number;
            encoder->percentheight = (-1);
        } else if (strcmp(optarg, "auto") == 0) {
            encoder->pixelheight = (-1);
            encoder->percentheight = (-1);
        } else {
            sixel_helper_set_additional_message(
                "cannot parse -h/--height option.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        if (encoder->clipheight) {
            encoder->clipfirst = 1;
        }
        break;
    case SIXEL_OPTFLAG_RESAMPLING:  /* r */
        /* parse --resampling option */
        if (strcmp(optarg, "nearest") == 0) {
            encoder->method_for_resampling = SIXEL_RES_NEAREST;
        } else if (strcmp(optarg, "gaussian") == 0) {
            encoder->method_for_resampling = SIXEL_RES_GAUSSIAN;
        } else if (strcmp(optarg, "hanning") == 0) {
            encoder->method_for_resampling = SIXEL_RES_HANNING;
        } else if (strcmp(optarg, "hamming") == 0) {
            encoder->method_for_resampling = SIXEL_RES_HAMMING;
        } else if (strcmp(optarg, "bilinear") == 0) {
            encoder->method_for_resampling = SIXEL_RES_BILINEAR;
        } else if (strcmp(optarg, "welsh") == 0) {
            encoder->method_for_resampling = SIXEL_RES_WELSH;
        } else if (strcmp(optarg, "bicubic") == 0) {
            encoder->method_for_resampling = SIXEL_RES_BICUBIC;
        } else if (strcmp(optarg, "lanczos2") == 0) {
            encoder->method_for_resampling = SIXEL_RES_LANCZOS2;
        } else if (strcmp(optarg, "lanczos3") == 0) {
            encoder->method_for_resampling = SIXEL_RES_LANCZOS3;
        } else if (strcmp(optarg, "lanczos4") == 0) {
            encoder->method_for_resampling = SIXEL_RES_LANCZOS4;
        } else {
            sixel_helper_set_additional_message(
                "specified desampling method is not supported.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_QUALITY:  /* q */
        /* parse --quality option */
        if (strcmp(optarg, "auto") == 0) {
            encoder->quality_mode = SIXEL_QUALITY_AUTO;
        } else if (strcmp(optarg, "high") == 0) {
            encoder->quality_mode = SIXEL_QUALITY_HIGH;
        } else if (strcmp(optarg, "low") == 0) {
            encoder->quality_mode = SIXEL_QUALITY_LOW;
        } else if (strcmp(optarg, "full") == 0) {
            encoder->quality_mode = SIXEL_QUALITY_FULL;
        } else {
            sixel_helper_set_additional_message(
                "cannot parse quality option.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_LOOPMODE:  /* l */
        /* parse --loop-control option */
        if (strcmp(optarg, "auto") == 0) {
            encoder->loop_mode = SIXEL_LOOP_AUTO;
        } else if (strcmp(optarg, "force") == 0) {
            encoder->loop_mode = SIXEL_LOOP_FORCE;
        } else if (strcmp(optarg, "disable") == 0) {
            encoder->loop_mode = SIXEL_LOOP_DISABLE;
        } else {
            sixel_helper_set_additional_message(
                "cannot parse loop-control option.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_PALETTE_TYPE:  /* t */
        /* parse --palette-type option */
        if (strcmp(optarg, "auto") == 0) {
            encoder->palette_type = SIXEL_PALETTETYPE_AUTO;
        } else if (strcmp(optarg, "hls") == 0) {
            encoder->palette_type = SIXEL_PALETTETYPE_HLS;
        } else if (strcmp(optarg, "rgb") == 0) {
            encoder->palette_type = SIXEL_PALETTETYPE_RGB;
        } else {
            sixel_helper_set_additional_message(
                "cannot parse palette type option.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_BGCOLOR:  /* B */
        /* parse --bgcolor option */
        if (encoder->bgcolor) {
            sixel_allocator_free(encoder->allocator, encoder->bgcolor);
        }
        status = parse_x_colorspec(&encoder->bgcolor,
                                   optarg,
                                   encoder->allocator);
        if (SIXEL_FAILED(status)) {
            sixel_helper_set_additional_message(
                "cannot parse bgcolor option.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_INSECURE:  /* k */
        encoder->finsecure = 1;
        break;
    case SIXEL_OPTFLAG_INVERT:  /* i */
        encoder->finvert = 1;
        break;
    case SIXEL_OPTFLAG_USE_MACRO:  /* u */
        encoder->fuse_macro = 1;
        break;
    case SIXEL_OPTFLAG_MACRO_NUMBER:  /* n */
        encoder->macro_number = atoi(optarg);
        if (encoder->macro_number < 0) {
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_IGNORE_DELAY:  /* g */
        encoder->fignore_delay = 1;
        break;
    case SIXEL_OPTFLAG_VERBOSE:  /* v */
        encoder->verbose = 1;
        break;
    case SIXEL_OPTFLAG_STATIC:  /* S */
        encoder->fstatic = 1;
        break;
    case SIXEL_OPTFLAG_PENETRATE:  /* P */
        encoder->penetrate_multiplexer = 1;
        break;
    case SIXEL_OPTFLAG_ENCODE_POLICY:  /* E */
        if (strcmp(optarg, "auto") == 0) {
            encoder->encode_policy = SIXEL_ENCODEPOLICY_AUTO;
        } else if (strcmp(optarg, "fast") == 0) {
            encoder->encode_policy = SIXEL_ENCODEPOLICY_FAST;
        } else if (strcmp(optarg, "size") == 0) {
            encoder->encode_policy = SIXEL_ENCODEPOLICY_SIZE;
        } else {
            sixel_helper_set_additional_message(
                "cannot parse encode policy option.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_COMPLEXION_SCORE:  /* C */
        encoder->complexion = atoi(optarg);
        if (encoder->complexion < 1) {
            sixel_helper_set_additional_message(
                "complexion parameter must be 1 or more.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_PIPE_MODE:  /* D */
        encoder->pipe_mode = 1;
        break;
    case '?':  /* unknown option */
    default:
        /* exit if unknown options are specified */
        sixel_helper_set_additional_message(
            "unknwon option is specified.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    /* detects arguments conflictions */
    if (encoder->reqcolors != -1 && encoder->mapfile) {
        sixel_helper_set_additional_message(
            "option -p, --colors conflicts "
            "with -m, --mapfile.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    if (encoder->mapfile && encoder->monochrome) {
        sixel_helper_set_additional_message(
            "option -m, --mapfile conflicts "
            "with -e, --monochrome.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    if (encoder->monochrome && encoder->reqcolors != (-1)) {
        sixel_helper_set_additional_message(
            "option -e, --monochrome conflicts"
            " with -p, --colors.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    if (encoder->monochrome && encoder->highcolor) {
        sixel_helper_set_additional_message(
            "option -e, --monochrome conflicts"
            " with -I, --high-color.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    if (encoder->reqcolors != (-1) && encoder->highcolor) {
        sixel_helper_set_additional_message(
            "option -p, --colors conflicts"
            " with -I, --high-color.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    if (encoder->mapfile && encoder->highcolor) {
        sixel_helper_set_additional_message(
            "option -m, --mapfile conflicts"
            " with -I, --high-color.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    if (encoder->builtin_palette && encoder->highcolor) {
        sixel_helper_set_additional_message(
            "option -b, --builtin-palette conflicts"
            " with -I, --high-color.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    if (encoder->monochrome && encoder->builtin_palette) {
        sixel_helper_set_additional_message(
            "option -e, --monochrome conflicts"
            " with -b, --builtin-palette.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    if (encoder->mapfile && encoder->builtin_palette) {
        sixel_helper_set_additional_message(
            "option -m, --mapfile conflicts"
            " with -b, --builtin-palette.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    if (encoder->reqcolors != (-1) && encoder->builtin_palette) {
        sixel_helper_set_additional_message(
            "option -p, --colors conflicts"
            " with -b, --builtin-palette.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    if (encoder->f8bit && encoder->penetrate_multiplexer) {
        sixel_helper_set_additional_message(
            "option -8 --8bit-mode conflicts"
            " with -P, --penetrate.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    status = SIXEL_OK;

end:
    sixel_encoder_unref(encoder);

    return status;
}


SIXELAPI SIXELSTATUS
sixel_encoder_encode(
    sixel_encoder_t /* in */ *encoder,
    char const      /* in */ *filename)
{
    SIXELSTATUS status = SIXEL_FALSE;
    int fuse_palette = 1;

    if (encoder == NULL) {
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
        encoder = sixel_encoder_create();
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
#  pragma GCC diagnostic pop
#endif
        if (encoder == NULL) {
            sixel_helper_set_additional_message(
                "sixel_encoder_encode: sixel_encoder_create() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
    } else {
        sixel_encoder_ref(encoder);
    }

    if (encoder->reqcolors == (-1)) {
        encoder->reqcolors = SIXEL_PALETTE_MAX;
    }

    if (encoder->reqcolors < 2) {
        encoder->reqcolors = 2;
    }

    if (encoder->palette_type == SIXEL_PALETTETYPE_AUTO) {
        encoder->palette_type = SIXEL_PALETTETYPE_RGB;
    }

    if (encoder->mapfile) {
        fuse_palette = 0;
    }

    if (encoder->monochrome > 0) {
        fuse_palette = 0;
    }

    if (encoder->highcolor > 0) {
        fuse_palette = 0;
    }

    if (encoder->builtin_palette > 0) {
        fuse_palette = 0;
    }

    if (encoder->percentwidth > 0 ||
        encoder->percentheight > 0 ||
        encoder->pixelwidth > 0 ||
        encoder->pixelheight > 0) {
        fuse_palette = 0;
    }

reload:
    status = sixel_helper_load_image_file(filename,
                                          encoder->fstatic,
                                          fuse_palette,
                                          encoder->reqcolors,
                                          encoder->bgcolor,
                                          encoder->loop_mode,
                                          load_image_callback,
                                          encoder->finsecure,
                                          encoder->cancel_flag,
                                          (void *)encoder,
                                          encoder->allocator);
    if (status != SIXEL_OK) {
        goto end;
    }

    if (encoder->pipe_mode) {
#if HAVE_CLEARERR
        clearerr(stdin);
#endif  /* HAVE_FSEEK */
        while (encoder->cancel_flag && !*encoder->cancel_flag) {
            status = wait_stdin(1000000);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
            if (status != SIXEL_OK) {
                break;
            }
        }
        if (!encoder->cancel_flag || !*encoder->cancel_flag) {
            goto reload;
        }
    }

end:
    sixel_encoder_unref(encoder);

    return status;
}


#if HAVE_TESTS
static int
test1(void)
{
    int nret = EXIT_FAILURE;
    sixel_encoder_t *encoder = NULL;

#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    encoder = sixel_encoder_create();
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
#  pragma GCC diagnostic pop
#endif
    if (encoder == NULL) {
        goto error;
    }
    sixel_encoder_ref(encoder);
    sixel_encoder_unref(encoder);
    nret = EXIT_SUCCESS;

error:
    sixel_encoder_unref(encoder);
    return nret;
}


static int
test2(void)
{
    int nret = EXIT_FAILURE;
    SIXELSTATUS status;
    sixel_encoder_t *encoder = NULL;
    sixel_frame_t *frame = NULL;
    unsigned char *buffer;

#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    encoder = sixel_encoder_create();
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
#  pragma GCC diagnostic pop
#endif
    if (encoder == NULL) {
        goto error;
    }

#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    frame = sixel_frame_create();
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
#  pragma GCC diagnostic pop
#endif
    if (encoder == NULL) {
        goto error;
    }

    buffer = (unsigned char *)sixel_allocator_malloc(encoder->allocator, 3);
    if (buffer == NULL) {
        goto error;
    }
    status = sixel_frame_init(frame,
                              buffer,
                              1,
                              1,
                              SIXEL_PIXELFORMAT_RGB888,
                              NULL,
                              0);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = scroll_on_demand(encoder, frame);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    nret = EXIT_SUCCESS;

error:
    sixel_encoder_unref(encoder);
    sixel_frame_unref(frame);
    return nret;
}


static int
test3(void)
{
    int nret = EXIT_FAILURE;
    int result;

    result = wait_stdin(1000);
    if (result != 0) {
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
    sixel_encoder_t *encoder = NULL;
    SIXELSTATUS status;

#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    encoder = sixel_encoder_create();
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
#  pragma GCC diagnostic pop
#endif
    if (encoder == NULL) {
        goto error;
    }

    status = sixel_encoder_setopt(encoder,
                                  SIXEL_OPTFLAG_LOOPMODE,
                                  "force");
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_encoder_setopt(encoder,
                                  SIXEL_OPTFLAG_PIPE_MODE,
                                  "force");
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    nret = EXIT_SUCCESS;

error:
    sixel_encoder_unref(encoder);
    return nret;
}


static int
test5(void)
{
    int nret = EXIT_FAILURE;
    sixel_encoder_t *encoder = NULL;
    sixel_allocator_t *allocator = NULL;
    SIXELSTATUS status;

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_encoder_new(&encoder, allocator);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    sixel_encoder_ref(encoder);
    sixel_encoder_unref(encoder);
    nret = EXIT_SUCCESS;

error:
    sixel_encoder_unref(encoder);
    return nret;
}



int
sixel_encoder_tests_main(void)
{
    int nret = EXIT_FAILURE;
    size_t i;
    typedef int (* testcase)(void);

    static testcase const testcases[] = {
        test1,
        test2,
        test3,
        test4,
        test5
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


/* emacs, -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set expandtab ts=4 : */
/* EOF */
