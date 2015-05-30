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

#include "encoder.h"
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


static int
parse_x_colorspec(char const *s, unsigned char **bgcolor)
{
    char *p;
    unsigned char components[3];
    int index = 0;
    int ret = 0;
    unsigned long v;
    char *endptr;
    char *buf = NULL;

    if (s[0] == 'r' && s[1] == 'g' && s[2] == 'b' && s[3] == ':') {
        p = buf = arg_strdup(s + 4);
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
            ret = (-1);
            goto end;
        }
        *bgcolor = malloc(3);
        (*bgcolor)[0] = components[0];
        (*bgcolor)[1] = components[1];
        (*bgcolor)[2] = components[2];
    } else if (*s == '#') {
        buf = arg_strdup(s + 1);
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
                ret = (-1);
                goto end;
            }
        }
        if (endptr - p > 12) {
            ret = (-1);
            goto end;
        }
        *bgcolor = malloc(3);
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
            ret = (-1);
            goto end;
        }
    } else {
        ret = (-1);
        goto end;
    }

    ret = 0;

end:
    free(buf);

    return ret;
}


static int
sixel_write_callback(char *data, int size, void *priv)
{
    return write(*(int *)priv, data, size);
}


static int
sixel_hex_write_callback(char *data, int size, void *priv)
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


static sixel_dither_t *
prepare_monochrome_palette(int finvert)
{
    sixel_dither_t *dither;

    if (finvert) {
        dither = sixel_dither_get(BUILTIN_MONO_LIGHT);
    } else {
        dither = sixel_dither_get(BUILTIN_MONO_DARK);
    }
    if (dither == NULL) {
        return NULL;
    }

    return dither;
}


static sixel_dither_t *
prepare_builtin_palette(int builtin_palette)
{
    sixel_dither_t *dither;

    dither = sixel_dither_get(builtin_palette);

    if (dither == NULL) {
        return NULL;
    }

    return dither;
}


typedef struct sixel_callback_context_for_mapfile {
    int reqcolors;
    sixel_dither_t *dither;
} sixel_callback_context_for_mapfile_t;


static int
load_image_callback_for_palette(sixel_frame_t *frame, void *data)
{
    sixel_callback_context_for_mapfile_t *callback_context;
    int ret = (-1);

    callback_context = (sixel_callback_context_for_mapfile_t *)data;

    switch (sixel_frame_get_pixelformat(frame)) {
    case PIXELFORMAT_PAL1:
    case PIXELFORMAT_PAL2:
    case PIXELFORMAT_PAL4:
    case PIXELFORMAT_PAL8:
        if (sixel_frame_get_palette(frame) == NULL) {
            goto end;
        }
        callback_context->dither = sixel_dither_create(sixel_frame_get_ncolors(frame));
        if (callback_context->dither == NULL) {
            goto end;
        }
        sixel_dither_set_palette(callback_context->dither,
                                 sixel_frame_get_palette(frame));
        ret = 0;
        break;
    default:
        callback_context->dither = sixel_dither_create(callback_context->reqcolors);
        if (callback_context->dither == NULL) {
            goto end;
        }

        ret = sixel_dither_initialize(callback_context->dither,
                                      sixel_frame_get_pixels(frame),
                                      sixel_frame_get_width(frame),
                                      sixel_frame_get_height(frame),
                                      sixel_frame_get_pixelformat(frame),
                                      LARGE_NORM,
                                      REP_CENTER_BOX,
                                      QUALITY_HIGH);
        if (ret != 0) {
            sixel_dither_unref(callback_context->dither);
            goto end;
        }
        break;
    }

end:
    return ret;
}


static sixel_dither_t *
prepare_specified_palette(
    char const *mapfile,
    int reqcolors,
    unsigned char *bgcolor,
    int finsecure,
    int const *cancel_flag)
{
    int ret = (-1);

    sixel_callback_context_for_mapfile_t callback_context;

    callback_context.reqcolors = reqcolors;
    callback_context.dither = NULL;

    ret = sixel_helper_load_image_file(mapfile,
                                       1,   /* fstatic */
                                       1,   /* fuse_palette */
                                       256, /* reqcolors */
                                       bgcolor,
                                       LOOP_DISABLE,
                                       load_image_callback_for_palette,
                                       finsecure,
                                       cancel_flag,
                                       &callback_context);
    if (ret != 0) {
        return NULL;
    }

    return callback_context.dither;
}


static sixel_dither_t *
prepare_palette(sixel_dither_t *former_dither,
                sixel_frame_t *frame,
                sixel_encoder_t *encoder)
{
    sixel_dither_t *dither;
    int ret;
    int histogram_colors;

    if (encoder->highcolor) {
        if (former_dither) {
            return former_dither;
        }
        dither = sixel_dither_create(-1);
    } else if (encoder->monochrome) {
        if (former_dither) {
            return former_dither;
        }
        dither = prepare_monochrome_palette(encoder->finvert);
    } else if (encoder->mapfile) {
        if (former_dither) {
            return former_dither;
        }
        dither = prepare_specified_palette(encoder->mapfile,
                                           encoder->reqcolors,
                                           encoder->bgcolor,
                                           encoder->finsecure,
                                           encoder->cancel_flag);
    } else if (encoder->builtin_palette) {
        if (former_dither) {
            return former_dither;
        }
        dither = prepare_builtin_palette(encoder->builtin_palette);
    } else if (sixel_frame_get_palette(frame) && (sixel_frame_get_pixelformat(frame) & FORMATTYPE_PALETTE)) {
        dither = sixel_dither_create(sixel_frame_get_ncolors(frame));
        if (!dither) {
            return NULL;
        }
        sixel_dither_set_palette(dither, sixel_frame_get_palette(frame));
        sixel_dither_set_pixelformat(dither, sixel_frame_get_pixelformat(frame));
        if (sixel_frame_get_transparent(frame) != (-1)) {
            sixel_dither_set_transparent(dither, sixel_frame_get_transparent(frame));
        }
    } else if (sixel_frame_get_pixelformat(frame) == PIXELFORMAT_G8) {
        dither = sixel_dither_create(-1);
        sixel_dither_set_pixelformat(dither, sixel_frame_get_pixelformat(frame));
    } else {
        if (former_dither) {
            sixel_dither_unref(former_dither);
        }
        dither = sixel_dither_create(encoder->reqcolors);
        if (!dither) {
            return NULL;
        }
        ret = sixel_dither_initialize(dither,
                                      sixel_frame_get_pixels(frame),
                                      sixel_frame_get_width(frame),
                                      sixel_frame_get_height(frame),
                                      sixel_frame_get_pixelformat(frame),
                                      encoder->method_for_largest,
                                      encoder->method_for_rep,
                                      encoder->quality_mode);
        if (ret != 0) {
            sixel_dither_unref(dither);
            return NULL;
        }
        histogram_colors = sixel_dither_get_num_of_histogram_colors(dither);
        if (histogram_colors <= encoder->reqcolors) {
            encoder->method_for_diffuse = DIFFUSE_NONE;
        }
        sixel_dither_set_pixelformat(dither, sixel_frame_get_pixelformat(frame));
    }
    return dither;
}


static int
do_resize(
    sixel_frame_t *frame,
    sixel_encoder_t *encoder
)
{
    int nret;

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

        nret = sixel_frame_resize(frame,
                                  encoder->pixelwidth,
                                  encoder->pixelheight,
                                  encoder->method_for_resampling);
        if (nret != 0) {
            return nret;
        }
    }

    return 0;
}


static int
do_crop(
    sixel_frame_t *frame,
    sixel_encoder_t *encoder
)
{
    int ret;
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
        ret = sixel_frame_clip(frame,
                               encoder->clipx,
                               encoder->clipy,
                               encoder->clipwidth,
                               encoder->clipheight);
        if (ret != 0) {
            return ret;
        }
    }

    return 0;
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


static int
output_sixel_without_macro(
    unsigned char *buffer,
    int width,
    int height,
    int pixelformat,
    int delay,
    sixel_dither_t *dither,
    sixel_output_t *context,
    sixel_encoder_t *encoder
)
{
    int nret = 0;
    int dulation = 0;
    static unsigned char *p;
    int depth;
#if HAVE_USLEEP
    int lag = 0;
# if HAVE_CLOCK
    clock_t start;
# endif
#endif
    if (!encoder->mapfile && !encoder->monochrome
            && !encoder->highcolor && !encoder->builtin_palette) {
        sixel_dither_set_optimize_palette(dither, 1);
    }

    depth = sixel_helper_compute_depth(pixelformat);
    if (depth == (-1)) {
        nret = (-1);
        goto end;
    }

    p = malloc(width * height * depth);
    if (nret != 0) {
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

    nret = sixel_encode(p, width, height, depth, dither, context);
    if (nret != 0) {
        goto end;
    }

end:
    free(p);
    return nret;
}


static int
output_sixel_with_macro(
    unsigned char *frame,
    int sx,
    int sy,
    int delay,
    int frame_no,
    int loop_count,
    sixel_dither_t *dither,
    sixel_output_t *context,
    sixel_encoder_t *encoder
)
{
    int nret = 0;
    int dulation = 0;
    char buffer[256];
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
            sprintf(buffer, "\033P%d;0;1!z", encoder->macro_number);
        } else {
            sprintf(buffer, "\033P%d;0;1!z", frame_no);
        }
        sixel_write_callback(buffer, strlen(buffer), &encoder->outfd);

        nret = sixel_encode(frame, sx, sy, /* unused */ 3, dither, context);
        if (nret != 0) {
            goto end;
        }

        sixel_write_callback("\033\\", 2, &encoder->outfd);
    }
    if (encoder->macro_number < 0) {
        sprintf(buffer, "\033[%d*z", frame_no);
        sixel_write_callback(buffer, strlen(buffer), &encoder->outfd);
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
    return nret;
}


static void
scroll_on_demand(sixel_encoder_t *encoder, sixel_frame_t *frame)
{
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

    if (isatty(STDIN_FILENO) && isatty(STDOUT_FILENO)) {
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &size);
        if (size.ws_ypixel > 0) {
            if (sixel_frame_get_loop_no(frame) == 0 && sixel_frame_get_frame_no(frame) == 0) {
                /* set the terminal to cbreak mode */
                tcgetattr(STDIN_FILENO, &old_termios);
                memcpy(&new_termios, &old_termios, sizeof(old_termios));
                new_termios.c_lflag &= ~(ECHO | ICANON);
                new_termios.c_cc[VMIN] = 1;
                new_termios.c_cc[VTIME] = 0;
                tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_termios);

                /* request cursor position report */
                sixel_write_callback("\033[6n", 4, &encoder->outfd);
                if (wait_stdin(1000 * 1000) != (-1)) { /* wait 1 sec */
                    if (scanf("\033[%d;%dR", &row, &col) == 2) {
                        tcsetattr(STDIN_FILENO, TCSAFLUSH, &old_termios);
                         pixelheight = sixel_frame_get_height(frame);
                         cellheight = pixelheight * size.ws_row / size.ws_ypixel + 1;
                         scroll = cellheight + row - size.ws_row + 1;
                        if (scroll > 0) {
                            sprintf(buffer, "\033[%dS\033[%dA", scroll, scroll);
                            sixel_write_callback(buffer, strlen(buffer), &encoder->outfd);
                        }
                        sixel_write_callback("\0337", 2, &encoder->outfd);
                    } else {
                        sixel_write_callback("\033[H", 3, &encoder->outfd);
                    }
                } else {
                    sixel_write_callback("\033[H", 3, &encoder->outfd);
                }
            } else {
                sixel_write_callback("\0338", 2, &encoder->outfd);
            }
        } else {
            sixel_write_callback("\033[H", 3, &encoder->outfd);
        }
    } else {
        sixel_write_callback("\033[H", 3, &encoder->outfd);
    }
#else
    (void) frame;
    sixel_write_callback("\033[H", 3, &encoder->outfd);
#endif
}


static int
load_image_callback(sixel_frame_t *frame, void *data)
{
    int nret = SIXEL_FAILED;
    sixel_encoder_t *encoder;
    sixel_dither_t *dither = NULL;
    sixel_output_t *output = NULL;

    encoder = (sixel_encoder_t *)data;

    /* evaluate -w, -h, and -c option: crop/scale input source */
    if (encoder->clipfirst) {
        /* clipping */
        nret = do_crop(frame, encoder);
        if (nret != 0) {
            goto end;
        }

        /* scaling */
        nret = do_resize(frame, encoder);
        if (nret != SIXEL_SUCCESS) {
            goto end;
        }
    } else {
        /* scaling */
        nret = do_resize(frame, encoder);
        if (nret != 0) {
            goto end;
        }

        /* clipping */
        nret = do_crop(frame, encoder);
        if (nret != 0) {
            goto end;
        }
    }

    /* prepare dither context */
    dither = prepare_palette(encoder->dither_cache, frame, encoder);
    if (!dither) {
        nret = (-1);
        goto end;
    }

    if (encoder->dither_cache != NULL) {
        encoder->dither_cache = dither;
        sixel_dither_ref(dither);
    }

    /* evaluate -v option: print palette */
    if (encoder->verbose) {
        if (!(sixel_frame_get_pixelformat(frame) & FORMATTYPE_GRAYSCALE)) {
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
        output = sixel_output_create(sixel_hex_write_callback,
                                     &encoder->outfd);
    } else {
        output = sixel_output_create(sixel_write_callback,
                                     &encoder->outfd);
    }
    sixel_output_set_8bit_availability(output, encoder->f8bit);
    sixel_output_set_palette_type(output, encoder->palette_type);
    sixel_output_set_penetrate_multiplexer(
        output, encoder->penetrate_multiplexer);
    sixel_output_set_encode_policy(output, encoder->encode_policy);

    if (sixel_frame_get_multiframe(frame) && !encoder->fstatic) {
        scroll_on_demand(encoder, frame);
    }

    if (encoder->cancel_flag && *encoder->cancel_flag) {
        nret = SIXEL_INTERRUPTED;
        goto end;
    }

    /* output sixel: junction of multi-frame processing strategy */
    if (encoder->fuse_macro) {  /* -u option */
        /* use macro */
        nret = output_sixel_with_macro(sixel_frame_get_pixels(frame),
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
        nret = output_sixel_with_macro(sixel_frame_get_pixels(frame),
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
        nret = output_sixel_without_macro(sixel_frame_get_pixels(frame),
                                          sixel_frame_get_width(frame),
                                          sixel_frame_get_height(frame),
                                          sixel_frame_get_pixelformat(frame),
                                          sixel_frame_get_delay(frame),
                                          dither,
                                          output,
                                          encoder);
    }

    if (encoder->cancel_flag && *encoder->cancel_flag) {
        sixel_write_callback("\x18\033\\", 3, &encoder->outfd);
        nret = SIXEL_INTERRUPTED;
    }

    if (nret != 0) {
        goto end;
    }

end:
    if (output) {
        sixel_output_unref(output);
    }
    if (dither) {
        sixel_dither_unref(dither);
    }

    return nret;
}


/* create encoder object */
SIXELAPI sixel_encoder_t *
sixel_encoder_create(void)
{
    sixel_encoder_t *encoder;

    encoder = malloc(sizeof(sixel_encoder_t));
    if (encoder == NULL) {
        return NULL;
    }

    encoder->ref                   = 1;
    encoder->reqcolors             = (-1);
    encoder->mapfile               = NULL;
    encoder->monochrome            = 0;
    encoder->highcolor             = 0;
    encoder->builtin_palette       = 0;
    encoder->method_for_diffuse    = DIFFUSE_AUTO;
    encoder->method_for_largest    = LARGE_AUTO;
    encoder->method_for_rep        = REP_AUTO;
    encoder->quality_mode          = QUALITY_AUTO;
    encoder->method_for_resampling = RES_BILINEAR;
    encoder->loop_mode             = LOOP_AUTO;
    encoder->palette_type          = PALETTETYPE_AUTO;
    encoder->f8bit                 = 0;
    encoder->finvert               = 0;
    encoder->fuse_macro            = 0;
    encoder->fignore_delay         = 0;
    encoder->complexion            = 1;
    encoder->fstatic               = 0;
    encoder->pixelwidth            = -1;
    encoder->pixelheight           = -1;
    encoder->percentwidth          = -1;
    encoder->percentheight         = -1;
    encoder->clipx                 = 0;
    encoder->clipy                 = 0;
    encoder->clipwidth             = 0;
    encoder->clipheight            = 0;
    encoder->clipfirst             = 0;
    encoder->macro_number          = -1;
    encoder->verbose               = 0;
    encoder->penetrate_multiplexer = 0;
    encoder->encode_policy         = ENCODEPOLICY_AUTO;
    encoder->pipe_mode             = 0;
    encoder->bgcolor               = NULL;
    encoder->outfd                 = STDOUT_FILENO;
    encoder->finsecure             = 0;
    encoder->cancel_flag           = NULL;
    encoder->dither_cache          = NULL;

    return encoder;
}


SIXELAPI void
sixel_encoder_destroy(sixel_encoder_t *encoder)
{
    if (encoder) {
        free(encoder->mapfile);
        free(encoder->bgcolor);
        sixel_dither_unref(encoder->dither_cache);
        if (encoder->outfd
            && encoder->outfd != STDOUT_FILENO
            && encoder->outfd != STDERR_FILENO) {
            close(encoder->outfd);
        }
        free(encoder);
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


SIXELAPI int
sixel_encoder_set_cancel_flag(
    sixel_encoder_t /* in */ *encoder,
    int             /* in */ *cancel_flag
)
{
    encoder->cancel_flag = cancel_flag;
    return 0;
}


SIXELAPI int
sixel_encoder_setopt(
    sixel_encoder_t /* in */ *encoder,
    int             /* in */ arg,
    char const      /* in */ *optarg)
{
    int number;
    int parsed;
    char unit[32];

    switch(arg) {
    case 'o':
        if (*optarg == '\0') {
            fprintf(stderr, "Invalid file name.\n");
            goto argerr;
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
    case '7':
        encoder->f8bit = 0;
        break;
    case '8':
        encoder->f8bit = 1;
        break;
    case 'p':
        encoder->reqcolors = atoi(optarg);
        break;
    case 'm':
        if (encoder->mapfile) {
            free(encoder->mapfile);
        }
        encoder->mapfile = arg_strdup(optarg);
        break;
    case 'e':
        encoder->monochrome = 1;
        break;
    case 'I':
        encoder->highcolor = 1;
        break;
    case 'b':
        if (strcmp(optarg, "xterm16") == 0) {
            encoder->builtin_palette = BUILTIN_XTERM16;
        } else if (strcmp(optarg, "xterm256") == 0) {
            encoder->builtin_palette = BUILTIN_XTERM256;
        } else if (strcmp(optarg, "vt340mono") == 0) {
            encoder->builtin_palette = BUILTIN_VT340_MONO;
        } else if (strcmp(optarg, "vt340color") == 0) {
            encoder->builtin_palette = BUILTIN_VT340_COLOR;
        } else {
            fprintf(stderr,
                    "Cannot parse builtin palette option.\n");
            goto argerr;
        }
        break;
    case 'd':
        /* parse --diffusion option */
        if (strcmp(optarg, "auto") == 0) {
            encoder->method_for_diffuse = DIFFUSE_AUTO;
        } else if (strcmp(optarg, "none") == 0) {
            encoder->method_for_diffuse = DIFFUSE_NONE;
        } else if (strcmp(optarg, "fs") == 0) {
            encoder->method_for_diffuse = DIFFUSE_FS;
        } else if (strcmp(optarg, "atkinson") == 0) {
            encoder->method_for_diffuse = DIFFUSE_ATKINSON;
        } else if (strcmp(optarg, "jajuni") == 0) {
            encoder->method_for_diffuse = DIFFUSE_JAJUNI;
        } else if (strcmp(optarg, "stucki") == 0) {
            encoder->method_for_diffuse = DIFFUSE_STUCKI;
        } else if (strcmp(optarg, "burkes") == 0) {
            encoder->method_for_diffuse = DIFFUSE_BURKES;
        } else {
            fprintf(stderr,
                    "Diffusion method '%s' is not supported.\n",
                    optarg);
            goto argerr;
        }
        break;
    case 'f':
        /* parse --find-largest option */
        if (optarg) {
            if (strcmp(optarg, "auto") == 0) {
                encoder->method_for_largest = LARGE_AUTO;
            } else if (strcmp(optarg, "norm") == 0) {
                encoder->method_for_largest = LARGE_NORM;
            } else if (strcmp(optarg, "lum") == 0) {
                encoder->method_for_largest = LARGE_LUM;
            } else {
                fprintf(stderr,
                        "Finding method '%s' is not supported.\n",
                        optarg);
                goto argerr;
            }
        }
        break;
    case 's':
        /* parse --select-color option */
        if (strcmp(optarg, "auto") == 0) {
            encoder->method_for_rep = REP_AUTO;
        } else if (strcmp(optarg, "center") == 0) {
            encoder->method_for_rep = REP_CENTER_BOX;
        } else if (strcmp(optarg, "average") == 0) {
            encoder->method_for_rep = REP_AVERAGE_COLORS;
        } else if ((strcmp(optarg, "histogram") == 0) ||
                   (strcmp(optarg, "histgram") == 0)) {
            encoder->method_for_rep = REP_AVERAGE_PIXELS;
        } else {
            fprintf(stderr,
                    "Finding method '%s' is not supported.\n",
                    optarg);
            goto argerr;
        }
        break;
    case 'c':
        number = sscanf(optarg, "%dx%d+%d+%d",
                        &encoder->clipwidth, &encoder->clipheight,
                        &encoder->clipx, &encoder->clipy);
        if (number != 4) {
            goto argerr;
        }
        if (encoder->clipwidth <= 0 || encoder->clipheight <= 0) {
            goto argerr;
        }
        if (encoder->clipx < 0 || encoder->clipy < 0) {
            goto argerr;
        }
        encoder->clipfirst = 0;
        break;
    case 'w':
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
            fprintf(stderr,
                    "Cannot parse -w/--width option.\n");
            goto argerr;
        }
        if (encoder->clipwidth) {
            encoder->clipfirst = 1;
        }
        break;
    case 'h':
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
            fprintf(stderr,
                    "Cannot parse -h/--height option.\n");
            goto argerr;
        }
        if (encoder->clipheight) {
            encoder->clipfirst = 1;
        }
        break;
    case 'r':
        /* parse --resampling option */
        if (strcmp(optarg, "nearest") == 0) {
            encoder->method_for_resampling = RES_NEAREST;
        } else if (strcmp(optarg, "gaussian") == 0) {
            encoder->method_for_resampling = RES_GAUSSIAN;
        } else if (strcmp(optarg, "hanning") == 0) {
            encoder->method_for_resampling = RES_HANNING;
        } else if (strcmp(optarg, "hamming") == 0) {
            encoder->method_for_resampling = RES_HAMMING;
        } else if (strcmp(optarg, "bilinear") == 0) {
            encoder->method_for_resampling = RES_BILINEAR;
        } else if (strcmp(optarg, "welsh") == 0) {
            encoder->method_for_resampling = RES_WELSH;
        } else if (strcmp(optarg, "bicubic") == 0) {
            encoder->method_for_resampling = RES_BICUBIC;
        } else if (strcmp(optarg, "lanczos2") == 0) {
            encoder->method_for_resampling = RES_LANCZOS2;
        } else if (strcmp(optarg, "lanczos3") == 0) {
            encoder->method_for_resampling = RES_LANCZOS3;
        } else if (strcmp(optarg, "lanczos4") == 0) {
            encoder->method_for_resampling = RES_LANCZOS4;
        } else {
            fprintf(stderr,
                    "Resampling method '%s' is not supported.\n",
                    optarg);
            goto argerr;
        }
        break;
    case 'q':
        /* parse --quality option */
        if (strcmp(optarg, "auto") == 0) {
            encoder->quality_mode = QUALITY_AUTO;
        } else if (strcmp(optarg, "high") == 0) {
            encoder->quality_mode = QUALITY_HIGH;
        } else if (strcmp(optarg, "low") == 0) {
            encoder->quality_mode = QUALITY_LOW;
        } else if (strcmp(optarg, "full") == 0) {
            encoder->quality_mode = QUALITY_FULL;
        } else {
            fprintf(stderr,
                    "Cannot parse quality option.\n");
            goto argerr;
        }
        break;
    case 'l':
        /* parse --loop-control option */
        if (strcmp(optarg, "auto") == 0) {
            encoder->loop_mode = LOOP_AUTO;
        } else if (strcmp(optarg, "force") == 0) {
            encoder->loop_mode = LOOP_FORCE;
        } else if (strcmp(optarg, "disable") == 0) {
            encoder->loop_mode = LOOP_DISABLE;
        } else {
            fprintf(stderr,
                    "Cannot parse loop-control option.\n");
            goto argerr;
        }
        break;
    case 't':
        /* parse --palette-type option */
        if (strcmp(optarg, "auto") == 0) {
            encoder->palette_type = PALETTETYPE_AUTO;
        } else if (strcmp(optarg, "hls") == 0) {
            encoder->palette_type = PALETTETYPE_HLS;
        } else if (strcmp(optarg, "rgb") == 0) {
            encoder->palette_type = PALETTETYPE_RGB;
        } else {
            fprintf(stderr,
                    "Cannot parse palette type option.\n");
            goto argerr;
        }
        break;
    case 'B':
        /* parse --bgcolor option */
        if (encoder->bgcolor) {
            free(encoder->bgcolor);
        }
        if (parse_x_colorspec(optarg, &encoder->bgcolor) == 0) {
            encoder->palette_type = PALETTETYPE_AUTO;
        } else {
            fprintf(stderr,
                    "Cannot parse bgcolor option.\n");
            goto argerr;
        }
        break;
    case 'k':
        encoder->finsecure = 1;
        break;
    case 'i':
        encoder->finvert = 1;
        break;
    case 'u':
        encoder->fuse_macro = 1;
        break;
    case 'n':
        encoder->macro_number = atoi(optarg);
        if (encoder->macro_number < 0) {
            goto argerr;
        }
        break;
    case 'g':
        encoder->fignore_delay = 1;
        break;
    case 'v':
        encoder->verbose = 1;
        break;
    case 'S':
        encoder->fstatic = 1;
        break;
    case 'P':
        encoder->penetrate_multiplexer = 1;
        break;
    case 'E':
        if (strcmp(optarg, "auto") == 0) {
            encoder->encode_policy = ENCODEPOLICY_AUTO;
        } else if (strcmp(optarg, "fast") == 0) {
            encoder->encode_policy = ENCODEPOLICY_FAST;
        } else if (strcmp(optarg, "size") == 0) {
            encoder->encode_policy = ENCODEPOLICY_SIZE;
        } else {
            fprintf(stderr,
                    "Cannot parse encode policy option.\n");
            goto argerr;
        }
        break;
    case 'C':
        encoder->complexion = atoi(optarg);
        if (encoder->complexion < 1) {
            fprintf(stderr,
                    "complexion parameter must be 1 or more.\n");
            goto argerr;
        }
        break;
    case 'D':
        encoder->pipe_mode = 1;
        break;
    case '?':  /* unknown option */
    default:
        /* exit if unknown options are specified */
        fprintf(stderr,
                "Unknwon option '-%c' is specified.\n", arg);
        goto argerr;
    }

    /* detects arguments conflictions */
    if (encoder->reqcolors != -1 && encoder->mapfile) {
        fprintf(stderr, "option -p, --colors conflicts "
                        "with -m, --mapfile.\n");
        goto argerr;
    }
    if (encoder->mapfile && encoder->monochrome) {
        fprintf(stderr, "option -m, --mapfile conflicts "
                        "with -e, --monochrome.\n");
        goto argerr;
    }
    if (encoder->monochrome && encoder->reqcolors != (-1)) {
        fprintf(stderr, "option -e, --monochrome conflicts"
                        " with -p, --colors.\n");
        goto argerr;
    }
    if (encoder->monochrome && encoder->highcolor) {
        fprintf(stderr, "option -e, --monochrome conflicts"
                        " with -I, --high-color.\n");
        goto argerr;
    }
    if (encoder->reqcolors != (-1) && encoder->highcolor) {
        fprintf(stderr, "option -p, --colors conflicts"
                        " with -I, --high-color.\n");
        goto argerr;
    }
    if (encoder->mapfile && encoder->highcolor) {
        fprintf(stderr, "option -m, --mapfile conflicts"
                        " with -I, --high-color.\n");
        goto argerr;
    }
    if (encoder->builtin_palette && encoder->highcolor) {
        fprintf(stderr, "option -b, --builtin-palette conflicts"
                        " with -I, --high-color.\n");
        goto argerr;
    }
    if (encoder->monochrome && encoder->builtin_palette) {
        fprintf(stderr, "option -e, --monochrome conflicts"
                        " with -b, --builtin-palette.\n");
        goto argerr;
    }
    if (encoder->mapfile && encoder->builtin_palette) {
        fprintf(stderr, "option -m, --mapfile conflicts"
                        " with -b, --builtin-palette.\n");
        goto argerr;
    }
    if (encoder->reqcolors != (-1) && encoder->builtin_palette) {
        fprintf(stderr, "option -p, --colors conflicts"
                        " with -b, --builtin-palette.\n");
        goto argerr;
    }
    if (encoder->f8bit && encoder->penetrate_multiplexer) {
        fprintf(stderr, "option -8 --8bit-mode conflicts"
                        " with -P, --penetrate.\n");
        goto argerr;
    }

    return (0);

argerr:
    return (-1);
}


SIXELAPI int
sixel_encoder_encode(
    sixel_encoder_t /* in */ *encoder,
    char const      /* in */ *filename)
{
    int nret = (-1);
    int fuse_palette = 1;

    if (encoder == NULL) {
        encoder = sixel_encoder_create();
    } else {
        sixel_encoder_ref(encoder);
    }

    if (encoder->reqcolors == (-1)) {
        encoder->reqcolors = SIXEL_PALETTE_MAX;
    }

    if (encoder->reqcolors < 2) {
        encoder->reqcolors = 2;
    }

    if (encoder->palette_type == PALETTETYPE_AUTO) {
        encoder->palette_type = PALETTETYPE_RGB;
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
    nret = sixel_helper_load_image_file(filename,
                                        encoder->fstatic,
                                        fuse_palette,
                                        encoder->reqcolors,
                                        encoder->bgcolor,
                                        encoder->loop_mode,
                                        load_image_callback,
                                        encoder->finsecure,
                                        encoder->cancel_flag,
                                        (void *)encoder);

    if (nret != 0) {
        goto end;
    }

    if (encoder->pipe_mode) {
#if HAVE_CLEARERR
        clearerr(stdin);
#endif  /* HAVE_FSEEK */
        while (encoder->cancel_flag && !*encoder->cancel_flag) {
            nret = wait_stdin(1000000);
            if (nret == (-1)) {
                goto end;
            }
            if (nret != 0) {
                break;
            }
        }
        if (!encoder->cancel_flag || !*encoder->cancel_flag) {
            goto reload;
        }
    }

end:
    sixel_encoder_unref(encoder);

    return nret;
}


#if HAVE_TESTS
static int
test1(void)
{
    sixel_encoder_t *encoder = NULL;
    int nret = EXIT_FAILURE;

    encoder = sixel_encoder_create();
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


int
sixel_encoder_tests_main(void)
{
    int nret = EXIT_FAILURE;
    size_t i;
    typedef int (* testcase)(void);

    static testcase const testcases[] = {
        test1,
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
