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
#if HAVE_GETOPT_H
# include <getopt.h>
#endif

#if HAVE_INTTYPES_H
# include <inttypes.h>
#endif

#if HAVE_ERRNO_H
# include <errno.h>
#endif

#if HAVE_SIGNAL_H
# include <signal.h>
#endif
#if HAVE_SYS_SIGNAL_H
# include <sys/signal.h>
#endif

#include <sixel.h>
#include "scale.h"
#include "loader.h"


/* loop modes */
enum loopMode {
    LOOP_AUTO,       /* honer the setting of GIF header */
    LOOP_FORCE,      /* always enable loop */
    LOOP_DISABLE,    /* always disable loop */
};


static int
sixel_write_callback(char *data, int size, void *priv)
{
    /* unused */ (void) priv;

    return fwrite(data, 1, size, stdout);
}


static int
sixel_hex_write_callback(char *data, int size, void *priv)
{
    /* unused */ (void) priv;

    char hex[SIXEL_OUTPUT_PACKET_SIZE * 2];
    int i;
    int j;

    for (i = j = 0; i < size; ++i, ++j) {
        hex[j] = (data[i] >> 4) & 0xf;
        hex[j] += (hex[j] < 10 ? '0' : ('a' - 10));
        hex[++j] = data[i] & 0xf;
        hex[j] += (hex[j] < 10 ? '0' : ('a' - 10));
    }
    return fwrite(hex, 1, size * 2, stdout);
    return size;
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


static sixel_dither_t *
prepare_specified_palette(char const *mapfile, int reqcolors)
{
    unsigned char *mappixels;
    sixel_dither_t *dither = NULL;
    int map_sx = (-1);
    int map_sy = (-1);
    int frame_count;
    int loop_count;
    int ret = (-1);
    int *delays;
    int ncolors = 0;
    unsigned char *palette = NULL;
    int pixelformat = PIXELFORMAT_RGB888;

    delays = NULL;

    ret = load_image_file(mapfile, &map_sx, &map_sy,
                          &palette, &ncolors, &pixelformat,
                          &frame_count, &loop_count,
                          &delays, /* fstatic */ 1,
                          /* reqcolors */ 256,
                          &mappixels);
    if (ret != 0 || mappixels == NULL || map_sx * map_sy == 0) {
        goto end;
    }
    free(delays);

    switch (pixelformat) {
    case PIXELFORMAT_PAL8:
        if (palette == NULL) {
            goto end;
        }
        dither = sixel_dither_create(ncolors);
        if (dither == NULL) {
            goto end;
        }
        sixel_dither_set_palette(dither, palette);
        break;
    default:
        dither = sixel_dither_create(reqcolors);
        if (dither == NULL) {
            goto end;
        }

        ret = sixel_dither_initialize(dither, mappixels, map_sx, map_sy,
                                      pixelformat,
                                      LARGE_NORM, REP_CENTER_BOX, QUALITY_HIGH);
        if (ret != 0) {
            sixel_dither_unref(dither);
            goto end;
        }
        break;
    }

end:
    free(palette);

    return dither;
}


#if HAVE_SIGNAL

static volatile int signaled = 0;

static void
signal_handler(int sig)
{
    signaled = sig;
}

#endif

typedef struct Settings {
    int reqcolors;
    char *mapfile;
    int monochrome;
    int highcolor;
    int builtin_palette;
    enum methodForDiffuse method_for_diffuse;
    enum methodForLargest method_for_largest;
    enum methodForRep method_for_rep;
    enum qualityMode quality_mode;
    enum methodForResampling method_for_resampling;
    enum loopMode loop_mode;
    enum paletteType palette_type;
    int f8bit;
    int finvert;
    int fuse_macro;
    int fignore_delay;
    int complexion;
    int fstatic;
    int pixelwidth;
    int pixelheight;
    int percentwidth;
    int percentheight;
    int clipx;
    int clipy;
    int clipwidth;
    int clipheight;
    int clipfirst;
    int macro_number;
    int penetrate_multiplexer;
    int encode_policy;
    int pipe_mode;
    int verbose;
    int show_version;
    int show_help;
} settings_t;


static sixel_dither_t *
prepare_palette(sixel_dither_t *former_dither,
                unsigned char *frame, int sx, int sy,
                unsigned char *palette, int ncolors,
                int pixelformat,
                settings_t *psettings)
{
    sixel_dither_t *dither;
    int ret;

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
                                           psettings->reqcolors);
    } else if (psettings->builtin_palette) {
        if (former_dither) {
            return former_dither;
        }
        dither = prepare_builtin_palette(psettings->builtin_palette);
    } else if (palette && pixelformat & FORMATTYPE_PALETTE) {
        dither = sixel_dither_create(ncolors);
        if (!dither) {
            return NULL;
        }
        sixel_dither_set_palette(dither, palette);
        sixel_dither_set_pixelformat(dither, pixelformat);
    } else if (pixelformat == PIXELFORMAT_G8) {
        dither = sixel_dither_create(-1);
        sixel_dither_set_pixelformat(dither, pixelformat);
    } else {
        if (former_dither) {
            sixel_dither_unref(former_dither);
        }
        dither = sixel_dither_create(psettings->reqcolors);
        if (!dither) {
            return NULL;
        }
        ret = sixel_dither_initialize(dither, frame, sx, sy,
                                      pixelformat,
                                      psettings->method_for_largest,
                                      psettings->method_for_rep,
                                      psettings->quality_mode);
        if (ret != 0) {
            sixel_dither_unref(dither);
            return NULL;
        }
        sixel_dither_set_pixelformat(dither, pixelformat);
    }
    return dither;
}


static int
do_resize(unsigned char **ppixels,
          unsigned char **frames, int frame_count,
          int /* in,out */ *psx,
          int /* in,out */ *psy,
          int pixelformat,
          settings_t *psettings)
{
    unsigned char *p;
    int size;
    int n;
    unsigned char *scaled_frame = NULL;

    if (psettings->percentwidth > 0) {
        psettings->pixelwidth = *psx * psettings->percentwidth / 100;
    }
    if (psettings->percentheight > 0) {
        psettings->pixelheight = *psy * psettings->percentheight / 100;
    }
    if (psettings->pixelwidth > 0 && psettings->pixelheight <= 0) {
        psettings->pixelheight = *psy * psettings->pixelwidth / *psx;
    }
    if (psettings->pixelheight > 0 && psettings->pixelwidth <= 0) {
        psettings->pixelwidth = *psx * psettings->pixelheight / *psy;
    }

    if (psettings->pixelwidth > 0 && psettings->pixelheight > 0) {

        if (pixelformat != PIXELFORMAT_RGB888) {
            /* TODO: convert pixelformat to RGB888 */
            return (-1);
        }

        size = psettings->pixelwidth * psettings->pixelheight * 3;
        p = malloc(size * frame_count);

        if (p == NULL) {
            return (-1);
        }

        for (n = 0; n < frame_count; ++n) {
            scaled_frame = LSS_scale(frames[n], *psx, *psy, 3,
                                     psettings->pixelwidth,
                                     psettings->pixelheight,
                                     psettings->method_for_resampling);
            if (scaled_frame == NULL) {
                return (-1);
            }
            memcpy(p + size * n, scaled_frame, size);
            free(scaled_frame);
        }
        for (n = 0; n < frame_count; ++n) {
            frames[n] = p + size * n;
        }
        free(*ppixels);
        *ppixels = p;
        *psx = psettings->pixelwidth;
        *psy = psettings->pixelheight;
    }

    return 0;
}


static int
clip(unsigned char *pixels,
     int sx, int sy,
     int cx, int cy,
     int cw, int ch,
     int pixelformat)
{
    int y;
    unsigned char *src;
    unsigned char *dst;

    /* unused */ (void) sx;
    /* unused */ (void) sy;
    /* unused */ (void) cx;

    switch (pixelformat) {
    case PIXELFORMAT_PAL8:
    case PIXELFORMAT_G8:
        dst = pixels;
        src = pixels + cy * sx * 1;
        for (y = 0; y < ch; y++) {
            memmove(dst, src, cw * 1);
            dst += (cw * 1);
            src += (sx * 1);
        }
        break;
    case PIXELFORMAT_RGB888:
        dst = pixels;
        src = pixels + cy * sx * 3;
        for (y = 0; y < ch; y++) {
            memmove(dst, src, cw * 3);
            dst += (cw * 3);
            src += (sx * 3);
        }
        break;
    default:
        return (-1);
    }

    return 0;
}


static int do_crop(unsigned char **frames, int frame_count,
                   int *psx, int *psy, int pixelformat,
                   settings_t *psettings)
{
    int n;
    int ret;

    /* clipping */
    if (psettings->clipwidth + psettings->clipx > *psx) {
        psettings->clipwidth = (psettings->clipx > *psx) ? 0 : *psx - psettings->clipx;
    }
    if (psettings->clipheight + psettings->clipy > *psy) {
        psettings->clipheight = (psettings->clipy > *psy) ? 0 : *psy - psettings->clipy;
    }
    if (psettings->clipwidth > 0 && psettings->clipheight > 0) {
        for (n = 0; n < frame_count; ++n) {
            ret = clip(frames[n], *psx, *psy, psettings->clipx, psettings->clipy,
                       psettings->clipwidth, psettings->clipheight, pixelformat);
            if (ret != 0) {
                return ret;
            }
        }
        *psx = psettings->clipwidth;
        *psy = psettings->clipheight;
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
wait_stdin(void)
{
    fd_set rfds;
    struct timeval tv;
    int ret;

    tv.tv_sec = 1;
    tv.tv_usec = 0;
    FD_ZERO(&rfds);
    FD_SET(STDIN_FILENO, &rfds);
    ret = select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv);

    return ret;
}
#endif  /* HAVE_SYS_SELECT_H */


static int
compute_depth_from_pixelformat(int pixelformat)
{
    int depth = (-1);  /* unknown */

    switch (pixelformat) {
        case PIXELFORMAT_ARGB8888:
        case PIXELFORMAT_RGBA8888:
            depth = 4;
            break;
        case PIXELFORMAT_RGB888:
        case PIXELFORMAT_BGR888:
            depth = 3;
            break;
        case PIXELFORMAT_RGB555:
        case PIXELFORMAT_RGB565:
        case PIXELFORMAT_BGR555:
        case PIXELFORMAT_BGR565:
        case PIXELFORMAT_AG88:
        case PIXELFORMAT_GA88:
            depth = 2;
            break;
        case PIXELFORMAT_G8:
        case PIXELFORMAT_PAL8:
            depth = 1;
            break;
        default:
            break;
    }

    return depth;
}


static int
output_sixel_without_macro(
    unsigned char **frames,
    int sx, int sy,
    int depth,
    int loop_count,
    int frame_count,
    int *delays,
    sixel_dither_t *dither,
    sixel_output_t *context,
    settings_t *psettings
)
{
    int nret = 0;
    int dulation = 0;
    int lag = 0;
    int c;
    int n;
    unsigned char *frame;
#if HAVE_USLEEP && HAVE_CLOCK
    clock_t start;
#endif

    /* create output context */
    if (!context) {
        context = sixel_output_create(sixel_write_callback, stdout);
    }
    sixel_output_set_8bit_availability(context, psettings->f8bit);
    sixel_output_set_palette_type(context, psettings->palette_type);
    sixel_output_set_penetrate_multiplexer(context, psettings->penetrate_multiplexer);
    sixel_output_set_encode_policy(context, psettings->encode_policy);

    if (frame_count == 1 && !psettings->mapfile && !psettings->monochrome
            && !psettings->highcolor && !psettings->builtin_palette) {
        sixel_dither_set_optimize_palette(dither, 1);
    }

    frame = malloc(sx * sy * depth);
    if (nret != 0) {
        goto end;
    }
    for (c = 0; c != loop_count; ++c) {
        for (n = 0; n < frame_count; ++n) {
            if (frame_count > 1) {
#if HAVE_USLEEP && HAVE_CLOCK
                start = clock();
#endif
                printf("\033[H");
                fflush(stdout);
#if HAVE_USLEEP
                if (delays != NULL && !psettings->fignore_delay) {
# if HAVE_CLOCK
                    dulation = (clock() - start) * 1000000 / CLOCKS_PER_SEC - lag;
                    lag = 0;
# else
                    dulation = 0;
# endif
                    if (dulation < 10000 * delays[n]) {
                        usleep(10000 * delays[n] - dulation);
                    } else {
                        lag = 10000 * delays[n] - dulation;
                    }
                }
#endif
            }

            memcpy(frame, frames[n], sx * sy * depth);
            nret = sixel_encode(frame, sx, sy, depth, dither, context);
            if (nret != 0) {
                goto end;
            }

#if HAVE_SIGNAL
            if (signaled) {
                break;
            }
#endif
        }
#if HAVE_SIGNAL
        if (signaled) {
            break;
        }
#endif
    }
    if (signaled) {
        if (sixel_output_get_8bit_availability(context)) {
            printf("\x9c");
        } else {
            printf("\x1b\\");
        }
    }

end:
    return nret;
}


static int
output_sixel_with_macro(
    unsigned char **frames,
    int sx, int sy,
    int loop_count,
    int frame_count,
    int *delays,
    sixel_dither_t *dither,
    sixel_output_t *context,
    settings_t *psettings
)
{
    int nret = 0;
    int dulation = 0;
    int lag = 0;
    int c;
    int n;
#if HAVE_USLEEP && HAVE_CLOCK
    clock_t start;
#endif

    if (!context) {
        context = sixel_output_create(sixel_hex_write_callback, stdout);
    }
    sixel_output_set_8bit_availability(context, psettings->f8bit);
    sixel_output_set_palette_type(context, psettings->palette_type);
    sixel_output_set_penetrate_multiplexer(context, psettings->penetrate_multiplexer);
    sixel_output_set_encode_policy(context, psettings->encode_policy);

    for (n = 0; n < frame_count; ++n) {
#if HAVE_USLEEP && HAVE_CLOCK
        start = clock();
#endif
        if (frame_count == 1 && psettings->macro_number >= 0) {
            printf("\033P%d;0;1!z", psettings->macro_number);
        } else {
            printf("\033P%d;0;1!z", n);
        }

        nret = sixel_encode(frames[n], sx, sy, /* unused */ 3, dither, context);
        if (nret != 0) {
            goto end;
        }

        printf("\033\\");
        if (loop_count == -1) {
            printf("\033[H");
            if (frame_count != 1 || psettings->macro_number < 0) {
                printf("\033[%d*z", n);
            }
        }
#if HAVE_USLEEP
        if (delays != NULL && !psettings->fignore_delay) {
# if HAVE_CLOCK
            dulation = (clock() - start) * 1000000 / CLOCKS_PER_SEC - lag;
            lag = 0;
# else
            dulation = 0;
# endif
            if (dulation < 10000 * delays[n]) {
                usleep(10000 * delays[n] - dulation);
            } else {
                lag = 10000 * delays[n] - dulation;
            }
        }
#endif
#if HAVE_SIGNAL
        if (signaled) {
            break;
        }
#endif
    }
    if (signaled) {
        if (psettings->f8bit) {
            printf("\x9c");
        } else {
            printf("\x1b\\");
        }
    }
    if (frame_count > 1 || psettings->macro_number < 0) {
        for (c = 0; c != loop_count; ++c) {
            for (n = 0; n < frame_count; ++n) {
#if HAVE_USLEEP && HAVE_CLOCK
                start = clock();
#endif
                printf("\033[H");
                printf("\033[%d*z", n);
                fflush(stdout);
#if HAVE_USLEEP
                if (delays != NULL && !psettings->fignore_delay) {
# if HAVE_CLOCK
                    dulation = (clock() - start) * 1000000 / CLOCKS_PER_SEC - lag;
                    lag = 0;
# else
                    dulation = 0;
# endif
                    if (dulation < 10000 * delays[n]) {
                        usleep(10000 * delays[n] - dulation);
                    } else {
                        lag = 10000 * delays[n] - dulation;
                    }
                }
#endif
#if HAVE_SIGNAL
                if (signaled) {
                    break;
                }
#endif
            }
#if HAVE_SIGNAL
            if (signaled) {
                break;
            }
#endif
        }
    }

end:
    return nret;
}


static int
convert_to_sixel(char const *filename, settings_t *psettings)
{
    unsigned char *pixels;
    unsigned char **frames;
    unsigned char *p;
    unsigned char *frame;
    sixel_output_t *context = NULL;
    sixel_dither_t *dither = NULL;
    int sx, sy;
    int frame_count = 1;
    int loop_count = 1;
    int *delays;
    int n;
    int nret = (-1);
    int depth;
    unsigned char *palette = NULL;
    unsigned char **ppalette = &palette;
    int ncolors = 0;
    int pixelformat = PIXELFORMAT_RGB888;

    if (psettings->reqcolors < 2) {
        psettings->reqcolors = 2;
    }

    if (psettings->palette_type == PALETTETYPE_AUTO) {
        psettings->palette_type = PALETTETYPE_RGB;
    }

    if (psettings->mapfile) {
        ppalette = NULL;
    }

    if (psettings->monochrome > 0) {
        ppalette = NULL;
    }

    if (psettings->highcolor > 0) {
        ppalette = NULL;
    }

    if (psettings->builtin_palette > 0) {
        ppalette = NULL;
    }

    if (psettings->percentwidth > 0 ||
        psettings->percentheight > 0 ||
        psettings->pixelwidth > 0 ||
        psettings->pixelheight > 0) {
        ppalette = NULL;
    }

reload:
    pixels = NULL;
    frames = NULL;
    frame = NULL;
    delays = NULL;
    nret = load_image_file(filename, &sx, &sy,
                           ppalette, &ncolors, &pixelformat,
                           &frame_count, &loop_count,
                           &delays, psettings->fstatic,
                           psettings->reqcolors,
                           &pixels);

    if (nret != 0 || pixels == NULL || sx * sy == 0) {
        goto end;
    }

    depth = compute_depth_from_pixelformat(pixelformat);
    if (depth == (-1)) {
        nret = (-1);
        goto end;
    }

    frames = malloc(sizeof(unsigned char *) * frame_count);
    if (frames == NULL) {
        nret = (-1);
        goto end;
    }

    p = pixels;
    for (n = 0; n < frame_count; ++n) {
        frames[n] = p;
        p += sx * sy * depth;
    }

    /* evaluate -w, -h, and -c option: crop/scale input source */
    if (psettings->clipfirst) {
        /* clipping */
        nret = do_crop(frames, frame_count,
                       &sx, &sy, pixelformat, psettings);
        if (nret != 0) {
            goto end;
        }

        /* scaling */
        nret = do_resize(&pixels, frames, frame_count,
                         &sx, &sy, pixelformat, psettings);
        if (nret != 0) {
            goto end;
        }
    } else {
        /* scaling */
        nret = do_resize(&pixels, frames, frame_count,
                         &sx, &sy, pixelformat, psettings);
        if (nret != 0) {
            goto end;
        }

        /* clipping */
        nret = do_crop(frames, frame_count,
                       &sx, &sy, pixelformat, psettings);
        if (nret != 0) {
            goto end;
        }
    }

    /* prepare dither context */
    dither = prepare_palette(dither, pixels, sx, sy * frame_count,
                             palette, ncolors, pixelformat, psettings);
    if (!dither) {
        nret = (-1);
        goto end;
    }

    /* evaluate -v option: print palette */
    if (psettings->verbose) {
        if (!(pixelformat & FORMATTYPE_GRAYSCALE)) {
            print_palette(dither);
        }
    }

    /* evaluate -d option: set method for diffusion */
    sixel_dither_set_diffusion_type(dither, psettings->method_for_diffuse);

    /* evaluate -C option: set complexion score */
    if (psettings->complexion > 1) {
        sixel_dither_set_complexion_score(dither, psettings->complexion);
    }

    /* evaluate -l option: set loop count */
    switch (psettings->loop_mode) {
    case LOOP_FORCE:
        loop_count = (-1);  /* infinite */
        break;
    case LOOP_DISABLE:
        loop_count = 1;  /* do not loop */
        break;
    case LOOP_AUTO:
    default:
        if (frame_count == 1) {
            loop_count = 1;
        } else if (loop_count == 0) {
            loop_count = (-1);
        }
#ifdef HAVE_GDK_PIXBUF2
        /* do not trust loop_count report of gdk-pixbuf loader */
        if (loop_count == (-1)) {
            loop_count = 1;
        }
#endif
        break;
    }

    /* set signal handler to handle SIGINT/SIGTERM/SIGHUP */
#if HAVE_SIGNAL
# if HAVE_DECL_SIGINT
    signal(SIGINT, signal_handler);
# endif
# if HAVE_DECL_SIGTERM
    signal(SIGTERM, signal_handler);
# endif
# if HAVE_DECL_SIGHUP
    signal(SIGHUP, signal_handler);
# endif
#endif

    /* output sixel: junction of multi-frame processing strategy */
    if ((psettings->fuse_macro && frame_count > 1)) {  /* -u option */
        /* use macro */
        nret = output_sixel_with_macro(frames, sx, sy,
                                       loop_count, frame_count, delays,
                                       dither, context, psettings);
    } else if (psettings->macro_number >= 0) { /* -n option */
        /* use macro */
        nret = output_sixel_with_macro(frames, sx, sy,
                                       loop_count, frame_count, delays,
                                       dither, context, psettings);
    } else { /* do not use macro */
        nret = output_sixel_without_macro(frames, sx, sy, depth,
                                          loop_count, frame_count, delays,
                                          dither, context, psettings);
    }

    if (nret != 0) {
        goto end;
    }
    nret = 0;
    fflush(stdout);

end:
    free(frames);
    free(pixels);
    free(delays);
    free(frame);

    if (nret == 0 && psettings->pipe_mode) {
#if HAVE_CLEARERR
        clearerr(stdin);
#endif  /* HAVE_FSEEK */
        while (!signaled) {
#if HAVE_SYS_SELECT_H
            nret = wait_stdin();
            if (nret == -1) {
                return nret;
            }
#endif  /* HAVE_SYS_SELECT_H */
            if (nret != 0) {
                break;
            }
        }
        if (!signaled) {
            goto reload;
        }
    }

    if (context) {
        sixel_output_unref(context);
    }
    if (dither) {
        sixel_dither_unref(dither);
    }
    return nret;
}


static
void show_version(void)
{
    printf("img2sixel " PACKAGE_VERSION "\n"
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


static
void show_help(void)
{
    fprintf(stdout,
            "Usage: img2sixel [Options] imagefiles\n"
            "       img2sixel [Options] < imagefile\n"
            "\n"
            "Options:\n"
            "-7, --7bit-mode            generate a sixel image for 7bit\n"
            "                           terminals or printers (default)\n"
            "-8, --8bit-mode            generate a sixel image for 8bit\n"
            "                           terminals or printers\n"
            "-p COLORS, --colors=COLORS specify number of colors to reduce\n"
            "                           the image to (default=256)\n"
            "-m FILE, --mapfile=FILE    transform image colors to match this\n"
            "                           set of colorsspecify map\n"
            "-e, --monochrome           output monochrome sixel image\n"
            "                           this option assumes the terminal\n"
            "                           background color is black\n"
            "-i, --invert               assume the terminal background color\n"
            "                           is white, make sense only when -e\n"
            "                           option is given\n"
            "-I, --high-color           output 15bpp sixel image\n"
            "-u, --use-macro            use DECDMAC and DEVINVM sequences to\n"
            "                           optimize GIF animation rendering\n"
            "-n MACRONO, --macro-number=MACRONO\n"
            "                           specify an number argument for\n"
            "                           DECDMAC and make terminal memorize\n"
            "                           SIXEL image. No image is shown if this\n"
            "                           option is specified\n"
            "-C COMPLEXIONSCORE, --complexion-score=COMPLEXIONSCORE\n"
            "                           specify an number argument for the\n"
            "                           score of complexion correction.\n"
            "                           COMPLEXIONSCORE must be 1 or more.\n"
            "-g, --ignore-delay         render GIF animation without delay\n"
            "-S, --static               render animated GIF as a static image\n"
            "-d DIFFUSIONTYPE, --diffusion=DIFFUSIONTYPE\n"
            "                           choose diffusion method which used\n"
            "                           with -p option (color reduction)\n"
            "                           DIFFUSIONTYPE is one of them:\n"
            "                             auto     -> choose diffusion type\n"
            "                                         automatically (default)\n"
            "                             none     -> do not diffuse\n"
            "                             fs       -> Floyd-Steinberg method\n"
            "                             atkinson -> Bill Atkinson's method\n"
            "                             jajuni   -> Jarvis, Judice & Ninke\n"
            "                             stucki   -> Stucki's method\n"
            "                             burkes   -> Burkes' method\n"
            "-f FINDTYPE, --find-largest=FINDTYPE\n"
            "                           choose method for finding the largest\n"
            "                           dimension of median cut boxes for\n"
            "                           splitting, make sense only when -p\n"
            "                           option (color reduction) is\n"
            "                           specified\n"
            "                           FINDTYPE is one of them:\n"
            "                             auto -> choose finding method\n"
            "                                     automatically (default)\n"
            "                             norm -> simply comparing the\n"
            "                                     range in RGB space\n"
            "                             lum  -> transforming into\n"
            "                                     luminosities before the\n"
            "                                     comparison\n"
            "-s SELECTTYPE, --select-color=SELECTTYPE\n"
            "                           choose the method for selecting\n"
            "                           representative color from each\n"
            "                           median-cut box, make sense only\n"
            "                           when -p option (color reduction) is\n"
            "                           specified\n"
            "                           SELECTTYPE is one of them:\n"
            "                             auto      -> choose selecting\n"
            "                                          method automatically\n"
            "                                          (default)\n"
            "                             center    -> choose the center of\n"
            "                                          the box\n"
            "                             average    -> calculate the color\n"
            "                                          average into the box\n"
            "                             histogram -> similar with average\n"
            "                                          but considers color\n"
            "                                          histogram\n"
            "-c REGION, --crop=REGION   crop source image to fit the\n"
            "                           specified geometry. REGION should\n"
            "                           be formatted as '%%dx%%d+%%d+%%d'\n"
            "-w WIDTH, --width=WIDTH    resize image to specified width\n"
            "                           WIDTH is represented by the\n"
            "                           following syntax\n"
            "                             auto       -> preserving aspect\n"
            "                                           ratio (default)\n"
            "                             <number>%%  -> scale width with\n"
            "                                           given percentage\n"
            "                             <number>   -> scale width with\n"
            "                                           pixel counts\n"
            "                             <number>px -> scale width with\n"
            "                                           pixel counts\n"
            "-h HEIGHT, --height=HEIGHT resize image to specified height\n"
            "                           HEIGHT is represented by the\n"
            "                           following syntax\n"
            "                             auto       -> preserving aspect\n"
            "                                           ratio (default)\n"
            "                             <number>%%  -> scale height with\n"
            "                                           given percentage\n"
            "                             <number>   -> scale height with\n"
            "                                           pixel counts\n"
            "                             <number>px -> scale height with\n"
            "                                           pixel counts\n"
            "-r RESAMPLINGTYPE, --resampling=RESAMPLINGTYPE\n"
            "                           choose resampling filter used\n"
            "                           with -w or -h option (scaling)\n"
            "                           RESAMPLINGTYPE is one of them:\n"
            "                             nearest  -> Nearest-Neighbor\n"
            "                                         method\n"
            "                             gaussian -> Gaussian filter\n"
            "                             hanning  -> Hanning filter\n"
            "                             hamming  -> Hamming filter\n"
            "                             bilinear -> Bilinear filter\n"
            "                                         (default)\n"
            "                             welsh    -> Welsh filter\n"
            "                             bicubic  -> Bicubic filter\n"
            "                             lanczos2 -> Lanczos-2 filter\n"
            "                             lanczos3 -> Lanczos-3 filter\n"
            "                             lanczos4 -> Lanczos-4 filter\n"
            "-q QUALITYMODE, --quality=QUALITYMODE\n"
            "                           select quality of color\n"
            "                           quanlization.\n"
            "                             auto -> decide quality mode\n"
            "                                     automatically (default)\n"
            "                             low  -> low quality and high\n"
            "                                     speed mode\n"
            "                             high -> high quality and low\n"
            "                                     speed mode\n"
            "                             full -> full quality and careful\n"
            "                                     speed mode\n"
            "-l LOOPMODE, --loop-control=LOOPMODE\n"
            "                           select loop control mode for GIF\n"
            "                           animation.\n"
            "                             auto    -> honor the setting of\n"
            "                                        GIF header (default)\n"
            "                             force   -> always enable loop\n"
            "                             disable -> always disable loop\n"
            "-t PALETTETYPE, --palette-type=PALETTETYPE\n"
            "                           select palette color space type\n"
            "                             auto -> choose palette type\n"
            "                                     automatically (default)\n"
            "                             hls  -> use HLS color space\n"
            "                             rgb  -> use RGB color space\n"
            "-b BUILTINPALETTE, --builtin-palette=BUILTINPALETTE\n"
            "                           select built-in palette type\n"
            "                             xterm16    -> X default 16 color map\n"
            "                             xterm256   -> X default 256 color map\n"
            "                             vt340mono  -> VT340 monochrome map\n"
            "                             vt340color -> VT340 color map\n"
            "-E ENCODEPOLICY, --encode-policy=ENCODEPOLICY\n"
            "                           select encoding policy\n"
            "                             auto -> choose encoding policy\n"
            "                                     automatically (default)\n"
            "                             fast -> encode as fast as possible\n"
            "                             size -> encode to as small sixel\n"
            "                                     sequence as possible\n"
            "-P, --penetrate            penetrate GNU Screen using DCS\n"
            "                           pass-through sequence\n"
            "-D, --pipe-mode            read source images from stdin\n"
            "                           continuously\n"
            "-v, --verbose              show debugging info\n"
            "-V, --version              show version and license info\n"
            "-H, --help                 show this help\n"
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
    int unknown_opt = 0;
#if HAVE_GETOPT_LONG
    int long_opt;
    int option_index;
#endif  /* HAVE_GETOPT_LONG */
    int ret;
    int exit_code;
    int number;
    char unit[32];
    int parsed;
    char const *optstring = "78p:m:eb:Id:f:s:c:w:h:r:q:il:t:ugvSn:PE:C:DVH";

    settings_t settings = {
        -1,                 /* reqcolors */
        NULL,               /* mapfile */
        0,                  /* monochrome */
        0,                  /* highcolor */
        0,                  /* builtin_palette */
        DIFFUSE_AUTO,       /* method_for_diffuse */
        LARGE_AUTO,         /* method_for_largest */
        REP_AUTO,           /* method_for_rep */
        QUALITY_AUTO,       /* quality_mode */
        RES_BILINEAR,       /* method_for_resampling */
        LOOP_AUTO,          /* loop_mode */
        PALETTETYPE_AUTO,   /* palette_type */
        0,                  /* f8bit */
        0,                  /* finvert */
        0,                  /* fuse_macro */
        0,                  /* fignore_delay */
        1,                  /* complexion */
        0,                  /* static */
        -1,                 /* pixelwidth */
        -1,                 /* pixelheight */
        -1,                 /* percentwidth */
        -1,                 /* percentheight */
        0,                  /* clipx */
        0,                  /* clipy */
        0,                  /* clipwidth */
        0,                  /* clipheight */
        0,                  /* clipfirst */
        -1,                 /* macro_number */
        0,                  /* verbose */
        0,                  /* penetrate_multiplexer */
        ENCODEPOLICY_AUTO,  /* encode_policy */
        0,                  /* pipe_mode */
        0,                  /* show_version */
        0,                  /* show_help */
    };

#if HAVE_GETOPT_LONG
    struct option long_options[] = {
        {"7bit-mode",        no_argument,        &long_opt, '7'},
        {"8bit-mode",        no_argument,        &long_opt, '8'},
        {"colors",           required_argument,  &long_opt, 'p'},
        {"mapfile",          required_argument,  &long_opt, 'm'},
        {"monochrome",       no_argument,        &long_opt, 'e'},
        {"high-color",       no_argument,        &long_opt, 'I'},
        {"builtin-palette",  required_argument,  &long_opt, 'b'},
        {"diffusion",        required_argument,  &long_opt, 'd'},
        {"find-largest",     required_argument,  &long_opt, 'f'},
        {"select-color",     required_argument,  &long_opt, 's'},
        {"crop",             required_argument,  &long_opt, 'c'},
        {"width",            required_argument,  &long_opt, 'w'},
        {"height",           required_argument,  &long_opt, 'h'},
        {"resampling",       required_argument,  &long_opt, 'r'},
        {"quality",          required_argument,  &long_opt, 'q'},
        {"palette-type",     required_argument,  &long_opt, 't'},
        {"invert",           no_argument,        &long_opt, 'i'},
        {"loop-control",     required_argument,  &long_opt, 'l'},
        {"use-macro",        no_argument,        &long_opt, 'u'},
        {"ignore-delay",     no_argument,        &long_opt, 'g'},
        {"verbose",          no_argument,        &long_opt, 'v'},
        {"static",           no_argument,        &long_opt, 'S'},
        {"macro-number",     required_argument,  &long_opt, 'n'},
        {"penetrate",        no_argument,        &long_opt, 'P'},
        {"encode-policy",    required_argument,  &long_opt, 'E'},
        {"complexion-score", required_argument,  &long_opt, 'C'},
        {"pipe-mode",        no_argument,        &long_opt, 'D'},
        {"version",          no_argument,        &long_opt, 'V'},
        {"help",             no_argument,        &long_opt, 'H'},
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
            break;
        }
#if HAVE_GETOPT_LONG
        if (n == 0) {
            n = long_opt;
        }
#endif  /* HAVE_GETOPT_LONG */
        switch(n) {
        case '7':
            settings.f8bit = 0;
            break;
        case '8':
            settings.f8bit = 1;
            break;
        case 'p':
            settings.reqcolors = atoi(optarg);
            break;
        case 'm':
            settings.mapfile = wrap_strdup(optarg);
            break;
        case 'e':
            settings.monochrome = 1;
            break;
        case 'I':
            settings.highcolor = 1;
            break;
        case 'b':
            if (strcmp(optarg, "xterm16") == 0) {
                settings.builtin_palette = BUILTIN_XTERM16;
            } else if (strcmp(optarg, "xterm256") == 0) {
                settings.builtin_palette = BUILTIN_XTERM256;
            } else if (strcmp(optarg, "vt340mono") == 0) {
                settings.builtin_palette = BUILTIN_VT340_MONO;
            } else if (strcmp(optarg, "vt340color") == 0) {
                settings.builtin_palette = BUILTIN_VT340_COLOR;
            } else {
                fprintf(stderr,
                        "Cannot parse builtin palette option.\n");
                goto argerr;
            }
            break;
        case 'd':
            /* parse --diffusion option */
            if (strcmp(optarg, "auto") == 0) {
                settings.method_for_diffuse = DIFFUSE_AUTO;
            } else if (strcmp(optarg, "none") == 0) {
                settings.method_for_diffuse = DIFFUSE_NONE;
            } else if (strcmp(optarg, "fs") == 0) {
                settings.method_for_diffuse = DIFFUSE_FS;
            } else if (strcmp(optarg, "atkinson") == 0) {
                settings.method_for_diffuse = DIFFUSE_ATKINSON;
            } else if (strcmp(optarg, "jajuni") == 0) {
                settings.method_for_diffuse = DIFFUSE_JAJUNI;
            } else if (strcmp(optarg, "stucki") == 0) {
                settings.method_for_diffuse = DIFFUSE_STUCKI;
            } else if (strcmp(optarg, "burkes") == 0) {
                settings.method_for_diffuse = DIFFUSE_BURKES;
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
                    settings.method_for_largest = LARGE_AUTO;
                } else if (strcmp(optarg, "norm") == 0) {
                    settings.method_for_largest = LARGE_NORM;
                } else if (strcmp(optarg, "lum") == 0) {
                    settings.method_for_largest = LARGE_LUM;
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
                settings.method_for_rep = REP_AUTO;
            } else if (strcmp(optarg, "center") == 0) {
                settings.method_for_rep = REP_CENTER_BOX;
            } else if (strcmp(optarg, "average") == 0) {
                settings.method_for_rep = REP_AVERAGE_COLORS;
            } else if ((strcmp(optarg, "histogram") == 0) ||
                       (strcmp(optarg, "histgram") == 0)) {
                settings.method_for_rep = REP_AVERAGE_PIXELS;
            } else {
                fprintf(stderr,
                        "Finding method '%s' is not supported.\n",
                        optarg);
                goto argerr;
            }
            break;
        case 'c':
            number = sscanf(optarg, "%dx%d+%d+%d",
                            &settings.clipwidth, &settings.clipheight,
                            &settings.clipx, &settings.clipy);
            if (number != 4) {
                goto argerr;
            }
            if (settings.clipwidth <= 0 || settings.clipheight <= 0) {
                goto argerr;
            }
            settings.clipfirst = 0;
            break;
        case 'w':
            parsed = sscanf(optarg, "%d%2s", &number, unit);
            if (parsed == 2 && strcmp(unit, "%") == 0) {
                settings.pixelwidth = (-1);
                settings.percentwidth = number;
            } else if (parsed == 1 || (parsed == 2 && strcmp(unit, "px") == 0)) {
                settings.pixelwidth = number;
                settings.percentwidth = (-1);
            } else if (strcmp(optarg, "auto") == 0) {
                settings.pixelwidth = (-1);
                settings.percentwidth = (-1);
            } else {
                fprintf(stderr,
                        "Cannot parse -w/--width option.\n");
                goto argerr;
            }
            if (settings.clipwidth) {
                settings.clipfirst = 1;
            }
            break;
        case 'h':
            parsed = sscanf(optarg, "%d%2s", &number, unit);
            if (parsed == 2 && strcmp(unit, "%") == 0) {
                settings.pixelheight = (-1);
                settings.percentheight = number;
            } else if (parsed == 1 || (parsed == 2 && strcmp(unit, "px") == 0)) {
                settings.pixelheight = number;
                settings.percentheight = (-1);
            } else if (strcmp(optarg, "auto") == 0) {
                settings.pixelheight = (-1);
                settings.percentheight = (-1);
            } else {
                fprintf(stderr,
                        "Cannot parse -h/--height option.\n");
                goto argerr;
            }
            if (settings.clipheight) {
                settings.clipfirst = 1;
            }
            break;
        case 'r':
            /* parse --resampling option */
            if (strcmp(optarg, "nearest") == 0) {
                settings.method_for_resampling = RES_NEAREST;
            } else if (strcmp(optarg, "gaussian") == 0) {
                settings.method_for_resampling = RES_GAUSSIAN;
            } else if (strcmp(optarg, "hanning") == 0) {
                settings.method_for_resampling = RES_HANNING;
            } else if (strcmp(optarg, "hamming") == 0) {
                settings.method_for_resampling = RES_HAMMING;
            } else if (strcmp(optarg, "bilinear") == 0) {
                settings.method_for_resampling = RES_BILINEAR;
            } else if (strcmp(optarg, "welsh") == 0) {
                settings.method_for_resampling = RES_WELSH;
            } else if (strcmp(optarg, "bicubic") == 0) {
                settings.method_for_resampling = RES_BICUBIC;
            } else if (strcmp(optarg, "lanczos2") == 0) {
                settings.method_for_resampling = RES_LANCZOS2;
            } else if (strcmp(optarg, "lanczos3") == 0) {
                settings.method_for_resampling = RES_LANCZOS3;
            } else if (strcmp(optarg, "lanczos4") == 0) {
                settings.method_for_resampling = RES_LANCZOS4;
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
                settings.quality_mode = QUALITY_AUTO;
            } else if (strcmp(optarg, "high") == 0) {
                settings.quality_mode = QUALITY_HIGH;
            } else if (strcmp(optarg, "low") == 0) {
                settings.quality_mode = QUALITY_LOW;
            } else if (strcmp(optarg, "full") == 0) {
                settings.quality_mode = QUALITY_FULL;
            } else {
                fprintf(stderr,
                        "Cannot parse quality option.\n");
                goto argerr;
            }
            break;
        case 'l':
            /* parse --loop-control option */
            if (strcmp(optarg, "auto") == 0) {
                settings.loop_mode = LOOP_AUTO;
            } else if (strcmp(optarg, "force") == 0) {
                settings.loop_mode = LOOP_FORCE;
            } else if (strcmp(optarg, "disable") == 0) {
                settings.loop_mode = LOOP_DISABLE;
            } else {
                fprintf(stderr,
                        "Cannot parse loop-control option.\n");
                goto argerr;
            }
            break;
        case 't':
            /* parse --palette-type option */
            if (strcmp(optarg, "auto") == 0) {
                settings.palette_type = PALETTETYPE_AUTO;
            } else if (strcmp(optarg, "hls") == 0) {
                settings.palette_type = PALETTETYPE_HLS;
            } else if (strcmp(optarg, "rgb") == 0) {
                settings.palette_type = PALETTETYPE_RGB;
            } else {
                fprintf(stderr,
                        "Cannot parse palette type option.\n");
                goto argerr;
            }
            break;
        case 'i':
            settings.finvert = 1;
            break;
        case 'u':
            settings.fuse_macro = 1;
            break;
        case 'n':
            settings.macro_number = atoi(optarg);
            if (settings.macro_number < 0) {
                goto argerr;
            }
            break;
        case 'g':
            settings.fignore_delay = 1;
            break;
        case 'v':
            settings.verbose = 1;
            break;
        case 'S':
            settings.fstatic = 1;
            break;
        case 'P':
            settings.penetrate_multiplexer = 1;
            break;
        case 'E':
            if (strcmp(optarg, "auto") == 0) {
                settings.encode_policy = ENCODEPOLICY_AUTO;
            } else if (strcmp(optarg, "fast") == 0) {
                settings.encode_policy = ENCODEPOLICY_FAST;
            } else if (strcmp(optarg, "size") == 0) {
                settings.encode_policy = ENCODEPOLICY_SIZE;
            } else {
                fprintf(stderr,
                        "Cannot parse encode policy option.\n");
                goto argerr;
            }
            break;
        case 'C':
            settings.complexion = atoi(optarg);
            if (settings.complexion < 1) {
                fprintf(stderr,
                        "complexion parameter must be 1 or more.\n");
                goto argerr;
            }
            break;
        case 'D':
            settings.pipe_mode = 1;
            break;
        case 'V':
            settings.show_version = 1;
            break;
        case 'H':
            settings.show_help = 1;
            break;
        case '?':  /* unknown option */
        default:
            unknown_opt = 1;
        }
    }

    /* detects arguments conflictions */
    if (settings.reqcolors != -1 && settings.mapfile) {
        fprintf(stderr, "option -p, --colors conflicts "
                        "with -m, --mapfile.\n");
        goto argerr;
    }
    if (settings.mapfile && settings.monochrome) {
        fprintf(stderr, "option -m, --mapfile conflicts "
                        "with -e, --monochrome.\n");
        goto argerr;
    }
    if (settings.monochrome && settings.reqcolors != (-1)) {
        fprintf(stderr, "option -e, --monochrome conflicts"
                        " with -p, --colors.\n");
        goto argerr;
    }
    if (settings.monochrome && settings.highcolor) {
        fprintf(stderr, "option -e, --monochrome conflicts"
                        " with -I, --high-color.\n");
        goto argerr;
    }
    if (settings.reqcolors != (-1) && settings.highcolor) {
        fprintf(stderr, "option -p, --colors conflicts"
                        " with -I, --high-color.\n");
        goto argerr;
    }
    if (settings.mapfile && settings.highcolor) {
        fprintf(stderr, "option -m, --mapfile conflicts"
                        " with -I, --high-color.\n");
        goto argerr;
    }
    if (settings.builtin_palette && settings.highcolor) {
        fprintf(stderr, "option -b, --builtin-palette conflicts"
                        " with -I, --high-color.\n");
        goto argerr;
    }
    if (settings.monochrome && settings.builtin_palette) {
        fprintf(stderr, "option -e, --monochrome conflicts"
                        " with -I, --builtin-palette.\n");
        goto argerr;
    }
    if (settings.mapfile && settings.builtin_palette) {
        fprintf(stderr, "option -m, --mapfile conflicts"
                        " with -b, --builtin-palette.\n");
        goto argerr;
    }
    if (settings.reqcolors != (-1) && settings.builtin_palette) {
        fprintf(stderr, "option -p, --colors conflicts"
                        " with -b, --builtin-palette.\n");
        goto argerr;
    }
    if (settings.pipe_mode && optind != argc) {
        fprintf(stderr, "option -D, --pipe_mode conflicts"
                        " with arguments [filename ...].\n");
        goto argerr;
    }

    /* evaluate the option -v,--version */
    if (settings.show_version) {
        show_version();
        exit_code = EXIT_SUCCESS;
        goto end;
    }

    /* evaluate the option -h,--help */
    if (settings.show_help) {
        show_help();
        exit_code = EXIT_SUCCESS;
        goto end;
    }

    /* exit if unknown options are specified */
    if (unknown_opt) {
        goto argerr;
    }

    if (settings.reqcolors == (-1)) {
        settings.reqcolors = SIXEL_PALETTE_MAX;
    }

    if (optind == argc) {
        ret = convert_to_sixel(NULL, &settings);
        if (ret != 0) {
            exit_code = EXIT_FAILURE;
            goto end;
        }
    } else {
        for (n = optind; n < argc; n++) {
            ret = convert_to_sixel(argv[n], &settings);
            if (ret != 0) {
                exit_code = EXIT_FAILURE;
                goto end;
            }
        }
    }

    /* mark as success */
    exit_code = EXIT_SUCCESS;
    goto end;

argerr:
    exit_code = EXIT_FAILURE;
    fprintf(stderr, "usage: img2sixel [-78eIiugvSPDVH] [-p colors] [-m file] [-d diffusiontype]\n"
                    "                 [-f findtype] [-s selecttype] [-c geometory] [-w width]\n"
                    "                 [-h height] [-r resamplingtype] [-q quality] [-l loopmode]\n"
                    "                 [-t palettetype] [-n macronumber] [-C score] [-b palette]\n"
                    "                 [-E encodepolicy] [filename ...]\n"
                    "for more details, type: 'img2sixel -H'.\n");

end:
    free(settings.mapfile);
    return exit_code;
}

/* emacs, -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set expandtab ts=4 : */
/* EOF */
