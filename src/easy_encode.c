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

#include "easy_encode.h"
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
    /* unused */ (void) priv;

    return fwrite(data, 1, size, stdout);
}


static int
sixel_hex_write_callback(char *data, int size, void *priv)
{
    char hex[SIXEL_OUTPUT_PACKET_SIZE * 2];
    int i;
    int j;

    /* unused */ (void) priv;

    for (i = j = 0; i < size; ++i, ++j) {
        hex[j] = (data[i] >> 4) & 0xf;
        hex[j] += (hex[j] < 10 ? '0': ('a' - 10));
        hex[++j] = data[i] & 0xf;
        hex[j] += (hex[j] < 10 ? '0': ('a' - 10));
    }

    return fwrite(hex, 1, size * 2, stdout);
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
    unsigned char *bgcolor)
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
                                       &callback_context);
    if (ret != 0) {
        return NULL;
    }

    return callback_context.dither;
}


static sixel_dither_t *
prepare_palette(sixel_dither_t *former_dither,
                sixel_frame_t *frame,
                sixel_encode_settings_t *psettings)
{
    sixel_dither_t *dither;
    int ret;
    int histogram_colors;

    if (psettings->highcolor) {
        if (former_dither) {
            return former_dither;
        }
        dither = sixel_dither_create(-1);
    } else if (psettings->monochrome) {
        if (former_dither) {
            return former_dither;
        }
        dither = prepare_monochrome_palette(psettings->finvert);
    } else if (psettings->mapfile) {
        if (former_dither) {
            return former_dither;
        }
        dither = prepare_specified_palette(psettings->mapfile,
                                           psettings->reqcolors,
                                           psettings->bgcolor);
    } else if (psettings->builtin_palette) {
        if (former_dither) {
            return former_dither;
        }
        dither = prepare_builtin_palette(psettings->builtin_palette);
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
        dither = sixel_dither_create(psettings->reqcolors);
        if (!dither) {
            return NULL;
        }
        ret = sixel_dither_initialize(dither,
                                      sixel_frame_get_pixels(frame),
                                      sixel_frame_get_width(frame),
                                      sixel_frame_get_height(frame),
                                      sixel_frame_get_pixelformat(frame),
                                      psettings->method_for_largest,
                                      psettings->method_for_rep,
                                      psettings->quality_mode);
        if (ret != 0) {
            sixel_dither_unref(dither);
            return NULL;
        }
        histogram_colors = sixel_dither_get_num_of_histogram_colors(dither);
        if (histogram_colors <= psettings->reqcolors) {
            psettings->method_for_diffuse = DIFFUSE_NONE;
        }
        sixel_dither_set_pixelformat(dither, sixel_frame_get_pixelformat(frame));
    }
    return dither;
}


static int
do_resize(sixel_frame_t *frame, sixel_encode_settings_t *psettings)
{
    int nret;

    if (psettings->percentwidth > 0) {
        psettings->pixelwidth = sixel_frame_get_width(frame) * psettings->percentwidth / 100;
    }
    if (psettings->percentheight > 0) {
        psettings->pixelheight = sixel_frame_get_height(frame) * psettings->percentheight / 100;
    }
    if (psettings->pixelwidth > 0 && psettings->pixelheight <= 0) {
        psettings->pixelheight
            = sixel_frame_get_height(frame) * psettings->pixelwidth / sixel_frame_get_width(frame);
    }
    if (psettings->pixelheight > 0 && psettings->pixelwidth <= 0) {
        psettings->pixelwidth
            = sixel_frame_get_width(frame) * psettings->pixelheight / sixel_frame_get_height(frame);
    }

    if (psettings->pixelwidth > 0 && psettings->pixelheight > 0) {

        nret = sixel_frame_resize(frame,
                                  psettings->pixelwidth,
                                  psettings->pixelheight,
                                  psettings->method_for_resampling);
        if (nret != 0) {
            return nret;
        }
    }

    return 0;
}


static int
do_crop(sixel_frame_t *frame, sixel_encode_settings_t *psettings)
{
    int ret;
    int width;
    int height;

    width = sixel_frame_get_width(frame);
    height = sixel_frame_get_height(frame);

    /* clipping */
    if (psettings->clipwidth + psettings->clipx > width) {
        if (psettings->clipx > width) {
            psettings->clipwidth = 0;
        } else {
            psettings->clipwidth = width - psettings->clipx;
        }
    }
    if (psettings->clipheight + psettings->clipy > height) {
        if (psettings->clipy > height) {
            psettings->clipheight = 0;
        } else {
            psettings->clipheight = height - psettings->clipy;
        }
    }
    if (psettings->clipwidth > 0 && psettings->clipheight > 0) {
        ret = sixel_frame_clip(frame,
                               psettings->clipx,
                               psettings->clipy,
                               psettings->clipwidth,
                               psettings->clipheight);
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


#if HAVE_SYS_SELECT_H
static int
wait_stdin(int usec)
{
    fd_set rfds;
    struct timeval tv;
    int ret;

    tv.tv_sec = usec / 1000000;
    tv.tv_usec = usec % 1000000;
    FD_ZERO(&rfds);
    FD_SET(STDIN_FILENO, &rfds);
    ret = select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv);

    return ret;
}
#endif  /* HAVE_SYS_SELECT_H */


static int
output_sixel_without_macro(
    unsigned char *frame,
    int width,
    int height,
    int pixelformat,
    int delay,
    sixel_dither_t *dither,
    sixel_output_t *context,
    sixel_encode_settings_t *psettings
)
{
    int nret = 0;
    int dulation = 0;
    unsigned char *p;
    int depth;
#if HAVE_USLEEP
    int lag = 0;
# if HAVE_CLOCK
    clock_t start;
# endif
#endif
    if (!psettings->mapfile && !psettings->monochrome
            && !psettings->highcolor && !psettings->builtin_palette) {
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
    fflush(stdout);
#if HAVE_USLEEP
    if (!psettings->fignore_delay && delay > 0) {
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

    memcpy(p, frame, width * height * depth);
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
    sixel_encode_settings_t *psettings
)
{
    int nret = 0;
    int dulation = 0;
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
        if (psettings->macro_number >= 0) {
            printf("\033P%d;0;1!z", psettings->macro_number);
        } else {
            printf("\033P%d;0;1!z", frame_no);
        }

        nret = sixel_encode(frame, sx, sy, /* unused */ 3, dither, context);
        if (nret != 0) {
            goto end;
        }

        printf("\033\\");
    }
    if (psettings->macro_number < 0) {
        fflush(stdout);
        printf("\033[%d*z", frame_no);
#if HAVE_USLEEP
        if (delay > 0 && !psettings->fignore_delay) {
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


typedef struct sixel_callback_context {
    sixel_encode_settings_t *settings;
    int *cancel_flag;
} sixel_callback_context_t;


static void
scroll_on_demand(sixel_frame_t *frame)
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
                printf("\033[6n");
                fflush(stdout);
                if (wait_stdin(1000 * 1000) != (-1)) { /* wait 1 sec */
                    if (scanf("\033[%d;%dR", &row, &col) == 2) {
                        tcsetattr(STDIN_FILENO, TCSAFLUSH, &old_termios);
                         pixelheight = sixel_frame_get_height(frame);
                         cellheight = pixelheight * size.ws_row / size.ws_ypixel + 1;
                         scroll = cellheight + row - size.ws_row + 1;
                        if (scroll > 0) {
                            printf("\033[%dS\033[%dA", scroll, scroll);
                        }
                        printf("\0337");
                    } else {
                        printf("\033[H");
                    }
                } else {
                    printf("\033[H");
                }
            } else {
                printf("\0338");
            }
        } else {
            printf("\033[H");
        }
    } else {
        printf("\033[H");
    }
#else
    (void) frame;
    printf("\033[H");
#endif
}


static int
load_image_callback(sixel_frame_t *frame, void *data)
{
    int nret = SIXEL_FAILED;
    sixel_encode_settings_t *psettings;
    sixel_dither_t *dither = NULL;
    sixel_output_t *output = NULL;
    sixel_callback_context_t *callback_context = (sixel_callback_context_t *)data;

    psettings = callback_context->settings;

    /* evaluate -w, -h, and -c option: crop/scale input source */
    if (psettings->clipfirst) {
        /* clipping */
        nret = do_crop(frame, psettings);
        if (nret != 0) {
            goto end;
        }

        /* scaling */
        nret = do_resize(frame, psettings);
        if (nret != SIXEL_SUCCESS) {
            goto end;
        }
    } else {
        /* scaling */
        nret = do_resize(frame, psettings);
        if (nret != 0) {
            goto end;
        }

        /* clipping */
        nret = do_crop(frame, psettings);
        if (nret != 0) {
            goto end;
        }
    }

    /* prepare dither context */
    dither = prepare_palette(dither, frame, psettings);
    if (!dither) {
        nret = (-1);
        goto end;
    }

    /* evaluate -v option: print palette */
    if (psettings->verbose) {
        if (!(sixel_frame_get_pixelformat(frame) & FORMATTYPE_GRAYSCALE)) {
            print_palette(dither);
        }
    }

    /* evaluate -d option: set method for diffusion */
    sixel_dither_set_diffusion_type(dither, psettings->method_for_diffuse);

    /* evaluate -C option: set complexion score */
    if (psettings->complexion > 1) {
        sixel_dither_set_complexion_score(dither, psettings->complexion);
    }

    /* create output context */
    if (psettings->fuse_macro || psettings->macro_number >= 0) {
        /* -u or -n option */
        output = sixel_output_create(sixel_hex_write_callback, stdout);
    } else {
        output = sixel_output_create(sixel_write_callback, stdout);
    }
    sixel_output_set_8bit_availability(output, psettings->f8bit);
    sixel_output_set_palette_type(output, psettings->palette_type);
    sixel_output_set_penetrate_multiplexer(
        output, psettings->penetrate_multiplexer);
    sixel_output_set_encode_policy(output, psettings->encode_policy);

    if (sixel_frame_get_multiframe(frame) && !psettings->fstatic) {
        scroll_on_demand(frame);
    }

    /* output sixel: junction of multi-frame processing strategy */
    if (psettings->fuse_macro) {  /* -u option */
        /* use macro */
        nret = output_sixel_with_macro(sixel_frame_get_pixels(frame),
                                       sixel_frame_get_width(frame),
                                       sixel_frame_get_height(frame),
                                       sixel_frame_get_delay(frame),
                                       sixel_frame_get_frame_no(frame),
                                       sixel_frame_get_loop_no(frame),
                                       dither,
                                       output,
                                       psettings);
    } else if (psettings->macro_number >= 0) { /* -n option */
        /* use macro */
        nret = output_sixel_with_macro(sixel_frame_get_pixels(frame),
                                       sixel_frame_get_width(frame),
                                       sixel_frame_get_height(frame),
                                       sixel_frame_get_delay(frame),
                                       sixel_frame_get_frame_no(frame),
                                       sixel_frame_get_loop_no(frame),
                                       dither,
                                       output,
                                       psettings);
    } else {
        /* do not use macro */
        nret = output_sixel_without_macro(sixel_frame_get_pixels(frame),
                                          sixel_frame_get_width(frame),
                                          sixel_frame_get_height(frame),
                                          sixel_frame_get_pixelformat(frame),
                                          sixel_frame_get_delay(frame),
                                          dither,
                                          output,
                                          psettings);
    }

    if (callback_context->cancel_flag && *callback_context->cancel_flag) {
        printf("\x18\x1b\\");
        fflush(stdout);
        nret = SIXEL_INTERRUPTED;
    }

    if (nret != 0) {
        goto end;
    }

    fflush(stdout);

end:
    if (output) {
        sixel_output_unref(output);
    }
    if (dither) {
        sixel_dither_unref(dither);
    }

    return nret;
}


int
sixel_easy_encode(
    char const              /* in */ *filename,
    sixel_encode_settings_t /* in */ *psettings,
    int                     /* in */ *cancel_flag)
{
    int nret = (-1);
    int fuse_palette = 1;
    int loop_control;
    sixel_callback_context_t callback_context;

    if (psettings == NULL) {
        psettings = sixel_encode_settings_create();
    } else {
        sixel_encode_settings_ref(psettings);
    }

    loop_control = psettings->loop_mode;

    if (psettings->reqcolors == (-1)) {
        psettings->reqcolors = SIXEL_PALETTE_MAX;
    }

    if (psettings->reqcolors < 2) {
        psettings->reqcolors = 2;
    }

    if (psettings->palette_type == PALETTETYPE_AUTO) {
        psettings->palette_type = PALETTETYPE_RGB;
    }

    if (psettings->mapfile) {
        fuse_palette = 0;
    }

    if (psettings->monochrome > 0) {
        fuse_palette = 0;
    }

    if (psettings->highcolor > 0) {
        fuse_palette = 0;
    }

    if (psettings->builtin_palette > 0) {
        fuse_palette = 0;
    }

    if (psettings->percentwidth > 0 ||
        psettings->percentheight > 0 ||
        psettings->pixelwidth > 0 ||
        psettings->pixelheight > 0) {
        fuse_palette = 0;
    }

reload:
    callback_context.settings = psettings;
    callback_context.cancel_flag = cancel_flag;

    nret = sixel_helper_load_image_file(filename,
                                        psettings->fstatic,
                                        fuse_palette,
                                        psettings->reqcolors,
                                        psettings->bgcolor,
                                        loop_control,
                                        load_image_callback,
                                        &callback_context);

    if (nret == 0 && psettings->pipe_mode) {
#if HAVE_CLEARERR
        clearerr(stdin);
#endif  /* HAVE_FSEEK */
        while (!*cancel_flag) {
#if HAVE_SYS_SELECT_H
            nret = wait_stdin(1000000);
            if (nret == (-1)) {
                goto end;
            }
#endif  /* HAVE_SYS_SELECT_H */
            if (nret != 0) {
                break;
            }
        }
        if (!*cancel_flag) {
            goto reload;
        }
    }

end:
    sixel_encode_settings_unref(psettings);

    return nret;
}


int
sixel_easy_encode_setopt(
    sixel_encode_settings_t /* in */ *psettings,
    int                     /* in */ arg,
    char                    /* in */ *optarg)
{
    int number;
    int parsed;
    char unit[32];

    switch(arg) {
    case '7':
        psettings->f8bit = 0;
        break;
    case '8':
        psettings->f8bit = 1;
        break;
    case 'p':
        psettings->reqcolors = atoi(optarg);
        break;
    case 'm':
        if (psettings->mapfile) {
            free(psettings->mapfile);
        }
        psettings->mapfile = arg_strdup(optarg);
        break;
    case 'e':
        psettings->monochrome = 1;
        break;
    case 'I':
        psettings->highcolor = 1;
        break;
    case 'b':
        if (strcmp(optarg, "xterm16") == 0) {
            psettings->builtin_palette = BUILTIN_XTERM16;
        } else if (strcmp(optarg, "xterm256") == 0) {
            psettings->builtin_palette = BUILTIN_XTERM256;
        } else if (strcmp(optarg, "vt340mono") == 0) {
            psettings->builtin_palette = BUILTIN_VT340_MONO;
        } else if (strcmp(optarg, "vt340color") == 0) {
            psettings->builtin_palette = BUILTIN_VT340_COLOR;
        } else {
            fprintf(stderr,
                    "Cannot parse builtin palette option.\n");
            goto argerr;
        }
        break;
    case 'd':
        /* parse --diffusion option */
        if (strcmp(optarg, "auto") == 0) {
            psettings->method_for_diffuse = DIFFUSE_AUTO;
        } else if (strcmp(optarg, "none") == 0) {
            psettings->method_for_diffuse = DIFFUSE_NONE;
        } else if (strcmp(optarg, "fs") == 0) {
            psettings->method_for_diffuse = DIFFUSE_FS;
        } else if (strcmp(optarg, "atkinson") == 0) {
            psettings->method_for_diffuse = DIFFUSE_ATKINSON;
        } else if (strcmp(optarg, "jajuni") == 0) {
            psettings->method_for_diffuse = DIFFUSE_JAJUNI;
        } else if (strcmp(optarg, "stucki") == 0) {
            psettings->method_for_diffuse = DIFFUSE_STUCKI;
        } else if (strcmp(optarg, "burkes") == 0) {
            psettings->method_for_diffuse = DIFFUSE_BURKES;
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
                psettings->method_for_largest = LARGE_AUTO;
            } else if (strcmp(optarg, "norm") == 0) {
                psettings->method_for_largest = LARGE_NORM;
            } else if (strcmp(optarg, "lum") == 0) {
                psettings->method_for_largest = LARGE_LUM;
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
            psettings->method_for_rep = REP_AUTO;
        } else if (strcmp(optarg, "center") == 0) {
            psettings->method_for_rep = REP_CENTER_BOX;
        } else if (strcmp(optarg, "average") == 0) {
            psettings->method_for_rep = REP_AVERAGE_COLORS;
        } else if ((strcmp(optarg, "histogram") == 0) ||
                   (strcmp(optarg, "histgram") == 0)) {
            psettings->method_for_rep = REP_AVERAGE_PIXELS;
        } else {
            fprintf(stderr,
                    "Finding method '%s' is not supported.\n",
                    optarg);
            goto argerr;
        }
        break;
    case 'c':
        number = sscanf(optarg, "%dx%d+%d+%d",
                        &psettings->clipwidth, &psettings->clipheight,
                        &psettings->clipx, &psettings->clipy);
        if (number != 4) {
            goto argerr;
        }
        if (psettings->clipwidth <= 0 || psettings->clipheight <= 0) {
            goto argerr;
        }
        if (psettings->clipx < 0 || psettings->clipy < 0) {
            goto argerr;
        }
        psettings->clipfirst = 0;
        break;
    case 'w':
        parsed = sscanf(optarg, "%d%2s", &number, unit);
        if (parsed == 2 && strcmp(unit, "%") == 0) {
            psettings->pixelwidth = (-1);
            psettings->percentwidth = number;
        } else if (parsed == 1 || (parsed == 2 && strcmp(unit, "px") == 0)) {
            psettings->pixelwidth = number;
            psettings->percentwidth = (-1);
        } else if (strcmp(optarg, "auto") == 0) {
            psettings->pixelwidth = (-1);
            psettings->percentwidth = (-1);
        } else {
            fprintf(stderr,
                    "Cannot parse -w/--width option.\n");
            goto argerr;
        }
        if (psettings->clipwidth) {
            psettings->clipfirst = 1;
        }
        break;
    case 'h':
        parsed = sscanf(optarg, "%d%2s", &number, unit);
        if (parsed == 2 && strcmp(unit, "%") == 0) {
            psettings->pixelheight = (-1);
            psettings->percentheight = number;
        } else if (parsed == 1 || (parsed == 2 && strcmp(unit, "px") == 0)) {
            psettings->pixelheight = number;
            psettings->percentheight = (-1);
        } else if (strcmp(optarg, "auto") == 0) {
            psettings->pixelheight = (-1);
            psettings->percentheight = (-1);
        } else {
            fprintf(stderr,
                    "Cannot parse -h/--height option.\n");
            goto argerr;
        }
        if (psettings->clipheight) {
            psettings->clipfirst = 1;
        }
        break;
    case 'r':
        /* parse --resampling option */
        if (strcmp(optarg, "nearest") == 0) {
            psettings->method_for_resampling = RES_NEAREST;
        } else if (strcmp(optarg, "gaussian") == 0) {
            psettings->method_for_resampling = RES_GAUSSIAN;
        } else if (strcmp(optarg, "hanning") == 0) {
            psettings->method_for_resampling = RES_HANNING;
        } else if (strcmp(optarg, "hamming") == 0) {
            psettings->method_for_resampling = RES_HAMMING;
        } else if (strcmp(optarg, "bilinear") == 0) {
            psettings->method_for_resampling = RES_BILINEAR;
        } else if (strcmp(optarg, "welsh") == 0) {
            psettings->method_for_resampling = RES_WELSH;
        } else if (strcmp(optarg, "bicubic") == 0) {
            psettings->method_for_resampling = RES_BICUBIC;
        } else if (strcmp(optarg, "lanczos2") == 0) {
            psettings->method_for_resampling = RES_LANCZOS2;
        } else if (strcmp(optarg, "lanczos3") == 0) {
            psettings->method_for_resampling = RES_LANCZOS3;
        } else if (strcmp(optarg, "lanczos4") == 0) {
            psettings->method_for_resampling = RES_LANCZOS4;
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
            psettings->quality_mode = QUALITY_AUTO;
        } else if (strcmp(optarg, "high") == 0) {
            psettings->quality_mode = QUALITY_HIGH;
        } else if (strcmp(optarg, "low") == 0) {
            psettings->quality_mode = QUALITY_LOW;
        } else if (strcmp(optarg, "full") == 0) {
            psettings->quality_mode = QUALITY_FULL;
        } else {
            fprintf(stderr,
                    "Cannot parse quality option.\n");
            goto argerr;
        }
        break;
    case 'l':
        /* parse --loop-control option */
        if (strcmp(optarg, "auto") == 0) {
            psettings->loop_mode = LOOP_AUTO;
        } else if (strcmp(optarg, "force") == 0) {
            psettings->loop_mode = LOOP_FORCE;
        } else if (strcmp(optarg, "disable") == 0) {
            psettings->loop_mode = LOOP_DISABLE;
        } else {
            fprintf(stderr,
                    "Cannot parse loop-control option.\n");
            goto argerr;
        }
        break;
    case 't':
        /* parse --palette-type option */
        if (strcmp(optarg, "auto") == 0) {
            psettings->palette_type = PALETTETYPE_AUTO;
        } else if (strcmp(optarg, "hls") == 0) {
            psettings->palette_type = PALETTETYPE_HLS;
        } else if (strcmp(optarg, "rgb") == 0) {
            psettings->palette_type = PALETTETYPE_RGB;
        } else {
            fprintf(stderr,
                    "Cannot parse palette type option.\n");
            goto argerr;
        }
        break;
    case 'B':
        /* parse --bgcolor option */
        if (psettings->bgcolor) {
            free(psettings->bgcolor);
        }
        if (parse_x_colorspec(optarg, &psettings->bgcolor) == 0) {
            psettings->palette_type = PALETTETYPE_AUTO;
        } else {
            fprintf(stderr,
                    "Cannot parse bgcolor option.\n");
            goto argerr;
        }
        break;
    case 'i':
        psettings->finvert = 1;
        break;
    case 'u':
        psettings->fuse_macro = 1;
        break;
    case 'n':
        psettings->macro_number = atoi(optarg);
        if (psettings->macro_number < 0) {
            goto argerr;
        }
        break;
    case 'g':
        psettings->fignore_delay = 1;
        break;
    case 'v':
        psettings->verbose = 1;
        break;
    case 'S':
        psettings->fstatic = 1;
        break;
    case 'P':
        psettings->penetrate_multiplexer = 1;
        break;
    case 'E':
        if (strcmp(optarg, "auto") == 0) {
            psettings->encode_policy = ENCODEPOLICY_AUTO;
        } else if (strcmp(optarg, "fast") == 0) {
            psettings->encode_policy = ENCODEPOLICY_FAST;
        } else if (strcmp(optarg, "size") == 0) {
            psettings->encode_policy = ENCODEPOLICY_SIZE;
        } else {
            fprintf(stderr,
                    "Cannot parse encode policy option.\n");
            goto argerr;
        }
        break;
    case 'C':
        psettings->complexion = atoi(optarg);
        if (psettings->complexion < 1) {
            fprintf(stderr,
                    "complexion parameter must be 1 or more.\n");
            goto argerr;
        }
        break;
    case 'D':
        psettings->pipe_mode = 1;
        break;
    case 'V':
        psettings->show_version = 1;
        break;
    case 'H':
        psettings->show_help = 1;
        break;
    case '?':  /* unknown option */
    default:
        /* exit if unknown options are specified */
        fprintf(stderr,
                "Unknwon option '-%c' is specified.\n", arg);
        goto argerr;
    }

    /* detects arguments conflictions */
    if (psettings->reqcolors != -1 && psettings->mapfile) {
        fprintf(stderr, "option -p, --colors conflicts "
                        "with -m, --mapfile.\n");
        goto argerr;
    }
    if (psettings->mapfile && psettings->monochrome) {
        fprintf(stderr, "option -m, --mapfile conflicts "
                        "with -e, --monochrome.\n");
        goto argerr;
    }
    if (psettings->monochrome && psettings->reqcolors != (-1)) {
        fprintf(stderr, "option -e, --monochrome conflicts"
                        " with -p, --colors.\n");
        goto argerr;
    }
    if (psettings->monochrome && psettings->highcolor) {
        fprintf(stderr, "option -e, --monochrome conflicts"
                        " with -I, --high-color.\n");
        goto argerr;
    }
    if (psettings->reqcolors != (-1) && psettings->highcolor) {
        fprintf(stderr, "option -p, --colors conflicts"
                        " with -I, --high-color.\n");
        goto argerr;
    }
    if (psettings->mapfile && psettings->highcolor) {
        fprintf(stderr, "option -m, --mapfile conflicts"
                        " with -I, --high-color.\n");
        goto argerr;
    }
    if (psettings->builtin_palette && psettings->highcolor) {
        fprintf(stderr, "option -b, --builtin-palette conflicts"
                        " with -I, --high-color.\n");
        goto argerr;
    }
    if (psettings->monochrome && psettings->builtin_palette) {
        fprintf(stderr, "option -e, --monochrome conflicts"
                        " with -b, --builtin-palette.\n");
        goto argerr;
    }
    if (psettings->mapfile && psettings->builtin_palette) {
        fprintf(stderr, "option -m, --mapfile conflicts"
                        " with -b, --builtin-palette.\n");
        goto argerr;
    }
    if (psettings->reqcolors != (-1) && psettings->builtin_palette) {
        fprintf(stderr, "option -p, --colors conflicts"
                        " with -b, --builtin-palette.\n");
        goto argerr;
    }
    if (psettings->f8bit && psettings->penetrate_multiplexer) {
        fprintf(stderr, "option -8 --8bit-mode conflicts"
                        " with -P, --penetrate.\n");
        goto argerr;
    }

    return (0);

argerr:
    return (-1);
}


/* create settings object */
sixel_encode_settings_t *
sixel_encode_settings_create(void)
{
    sixel_encode_settings_t *settings;

    settings = malloc(sizeof(sixel_encode_settings_t));
    if (settings == NULL) {
        return NULL;
    }

    settings->ref                   = 1;
    settings->reqcolors             = (-1);
    settings->mapfile               = NULL;
    settings->monochrome            = 0;
    settings->highcolor             = 0;
    settings->builtin_palette       = 0;
    settings->method_for_diffuse    = DIFFUSE_AUTO;
    settings->method_for_largest    = LARGE_AUTO;
    settings->method_for_rep        = REP_AUTO;
    settings->quality_mode          = QUALITY_AUTO;
    settings->method_for_resampling = RES_BILINEAR;
    settings->loop_mode             = LOOP_AUTO;
    settings->palette_type          = PALETTETYPE_AUTO;
    settings->f8bit                 = 0;
    settings->finvert               = 0;
    settings->fuse_macro            = 0;
    settings->fignore_delay         = 0;
    settings->complexion            = 1;
    settings->fstatic               = 0;
    settings->pixelwidth            = -1;
    settings->pixelheight           = -1;
    settings->percentwidth          = -1;
    settings->percentheight         = -1;
    settings->clipx                 = 0;
    settings->clipy                 = 0;
    settings->clipwidth             = 0;
    settings->clipheight            = 0;
    settings->clipfirst             = 0;
    settings->macro_number          = -1;
    settings->verbose               = 0;
    settings->penetrate_multiplexer = 0;
    settings->encode_policy         = ENCODEPOLICY_AUTO;
    settings->pipe_mode             = 0;
    settings->show_version          = 0;
    settings->show_help             = 0;
    settings->bgcolor               = NULL;

    return settings;
}


void
sixel_encode_settings_destroy(sixel_encode_settings_t *settings)
{
    if (settings) {
        free(settings->mapfile);
        free(settings->bgcolor);
        free(settings);
    }
}


void
sixel_encode_settings_ref(sixel_encode_settings_t *settings)
{
    /* TODO: be thread safe */
    ++settings->ref;
}


void
sixel_encode_settings_unref(sixel_encode_settings_t *settings)
{
    /* TODO: be thread safe */
    if (settings != NULL && --settings->ref == 0) {
        sixel_encode_settings_destroy(settings);
    }
}


/* get show_version flag */
int
sixel_encode_settings_has_version(sixel_encode_settings_t *settings)
{
    return settings->show_version;
}


/* get show_help flag */
int
sixel_encode_settings_has_help(sixel_encode_settings_t *settings)
{
    return settings->show_help;
}

/* emacs, -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set expandtab ts=4 : */
/* EOF */
