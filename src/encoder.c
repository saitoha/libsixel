/* SPDX-License-Identifier: MIT AND BSD-3-Clause
 *
 * Copyright (c) 2014-2019 Hayaki Saito
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
 *
 * -------------------------------------------------------------------------------
 * Portions of this file(sixel_encoder_emit_drcsmmv2_chars) are derived from
 * mlterm's drcssixel.c.
 *
 * Copyright (c) Araki Ken(arakiken@users.sourceforge.net)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of any author may not be used to endorse or promote
 *    products derived from this software without their specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include "config.h"

/* STDC_HEADERS */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

# if HAVE_STRING_H
#include <string.h>
#endif  /* HAVE_STRING_H */
#if HAVE_UNISTD_H
# include <unistd.h>
#elif HAVE_SYS_UNISTD_H
# include <sys/unistd.h>
#endif  /* HAVE_SYS_UNISTD_H */
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif  /* HAVE_SYS_TYPES_H */
#if HAVE_INTTYPES_H
# include <inttypes.h>
#endif  /* HAVE_INTTYPES_H */
#if HAVE_ERRNO_H
# include <errno.h>
#endif  /* HAVE_ERRNO_H */
#if HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif  /* HAVE_SYS_STAT_H */
#if HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif  /* HAVE_SYS_IOCTL_H */
#if HAVE_FCNTL_H
# include <fcntl.h>
#endif  /* HAVE_FCNTL_H */
#if HAVE_ERRNO_H
# include <errno.h>
#endif  /* HAVE_ERRNO_H */
#if HAVE_CTYPE_H
# include <ctype.h>
#endif  /* HAVE_CTYPE_H */
#if HAVE_LIMITS_H
# include <limits.h>
#endif  /* HAVE_LIMITS_H */

#include <sixel.h>
#include "loader.h"
#include "tty.h"
#include "encoder.h"
#include "output.h"
#include "dither.h"
#include "frame.h"
#include "rgblookup.h"

#if defined(_WIN32)

# include <windows.h>
# if defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__)
#  include <io.h>
# endif
# if defined(_MSC_VER)
#   include <time.h>
# endif

/* for msvc */
# ifndef STDIN_FILENO
#  define STDIN_FILENO 0
# endif
# ifndef STDOUT_FILENO
#  define STDOUT_FILENO 1
# endif
# ifndef STDERR_FILENO
#  define STDERR_FILENO 2
# endif
# ifndef S_IRUSR
#  define S_IRUSR _S_IREAD
# endif
# ifndef S_IWUSR
#  define S_IWUSR _S_IWRITE
# endif

# if defined(CLOCKS_PER_SEC)
#  undef CLOCKS_PER_SEC
# endif
# define CLOCKS_PER_SEC 1000

# if !defined(HAVE_NANOSLEEP)
# define HAVE_NANOSLEEP_WIN 1
static int
nanosleep_win(
    struct timespec const *req,
    struct timespec *rem)
{
    LONGLONG nanoseconds;
    LARGE_INTEGER dueTime;
    HANDLE timer;

    if (req == NULL || req->tv_sec < 0 || req->tv_nsec < 0 ||
        req->tv_nsec >= 1000000000L) {
        errno = EINVAL;
        return (-1);
    }

    /* Convert to 100-nanosecond intervals (Windows FILETIME units) */
    nanoseconds = req->tv_sec * 1000000000LL + req->tv_nsec;
    dueTime.QuadPart = -(nanoseconds / 100); /* Negative for relative time */

    timer = CreateWaitableTimer(NULL, TRUE, NULL);
    if (timer == NULL) {
        errno = EFAULT;  /* Approximate error */
        return (-1);
    }

    if (! SetWaitableTimer(timer, &dueTime, 0, NULL, NULL, FALSE)) {
        (void) CloseHandle(timer);
        errno = EFAULT;
        return (-1);
    }

    (void) WaitForSingleObject(timer, INFINITE);
    (void) CloseHandle(timer);

    /* No interruption handling, so rem is unchanged */
    if (rem != NULL) {
        rem->tv_sec = 0;
        rem->tv_nsec = 0;
    }

    return (0);
}
# endif  /* HAVE_NANOSLEEP */

# if !defined(HAVE_CLOCK)
# define HAVE_CLOCK_WIN 1
static sixel_clock_t
clock_win(void)
{
    FILETIME ct, et, kt, ut;
    ULARGE_INTEGER u, k;

    if (! GetProcessTimes(GetCurrentProcess(), &ct, &et, &kt, &ut)) {
        return (sixel_clock_t)(-1);
    }
    u.LowPart = ut.dwLowDateTime; u.HighPart = ut.dwHighDateTime;
    k.LowPart = kt.dwLowDateTime; k.HighPart = kt.dwHighDateTime;
    /* 100ns -> ms */
    return (sixel_clock_t)((u.QuadPart + k.QuadPart) / 10000ULL);
}
# endif  /* HAVE_CLOCK */

#endif /* _WIN32 */


static char *
arg_strdup(
    char const          /* in */ *s,          /* source buffer */
    sixel_allocator_t   /* in */ *allocator)  /* allocator object for
                                                 destination buffer */
{
    char *p;
    size_t len;

    len = strlen(s);

    p = (char *)sixel_allocator_malloc(allocator, len + 1);
    if (p) {
#if HAVE_STRCPY_S
        (void) strcpy_s(p, (rsize_t)len, s);
#else
        (void) strcpy(p, s);
#endif  /* HAVE_STRCPY_S */
    }
    return p;
}


/* An clone function of XColorSpec() of xlib */
static SIXELSTATUS
sixel_parse_x_colorspec(
    unsigned char       /* out */ **bgcolor,     /* destination buffer */
    char const          /* in */  *s,            /* source buffer */
    sixel_allocator_t   /* in */  *allocator)    /* allocator object for
                                                    destination buffer */
{
    SIXELSTATUS status = SIXEL_FALSE;
    char *p;
    unsigned char components[3];
    int component_index = 0;
    unsigned long v;
    char *endptr;
    char *buf = NULL;
    struct color const *pcolor;

    /* from rgb_lookup.h generated by gpref */
    pcolor = lookup_rgb(s, strlen(s));
    if (pcolor) {
        *bgcolor = (unsigned char *)sixel_allocator_malloc(allocator, 3);
        if (*bgcolor == NULL) {
            sixel_helper_set_additional_message(
                "sixel_parse_x_colorspec: sixel_allocator_malloc() failed.");
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
                "sixel_parse_x_colorspec: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        while (*p) {
            v = 0;
            for (endptr = p; endptr - p <= 12; ++endptr) {
                if (*endptr >= '0' && *endptr <= '9') {
                    v = (v << 4) | (unsigned long)(*endptr - '0');
                } else if (*endptr >= 'a' && *endptr <= 'f') {
                    v = (v << 4) | (unsigned long)(*endptr - 'a' + 10);
                } else if (*endptr >= 'A' && *endptr <= 'F') {
                    v = (v << 4) | (unsigned long)(*endptr - 'A' + 10);
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
            components[component_index++] = (unsigned char)v;
            p = endptr;
            if (component_index == 3) {
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
        if (component_index != 3 || *p != '\0' || *p == '/') {
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        *bgcolor = (unsigned char *)sixel_allocator_malloc(allocator, 3);
        if (*bgcolor == NULL) {
            sixel_helper_set_additional_message(
                "sixel_parse_x_colorspec: sixel_allocator_malloc() failed.");
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
                "sixel_parse_x_colorspec: sixel_allocator_malloc() failed.");
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
                "sixel_parse_x_colorspec: sixel_allocator_malloc() failed.");
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
    int result;

#if HAVE__WRITE
    result = _write(*(int *)priv, data, (size_t)size);
#elif defined(__MINGW64__)
    result = write(*(int *)priv, data, (unsigned int)size);
#else
    result = write(*(int *)priv, data, (size_t)size);
#endif

    return result;
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
    int result;

    for (i = j = 0; i < size; ++i, ++j) {
        hex[j] = (data[i] >> 4) & 0xf;
        hex[j] += (hex[j] < 10 ? '0': ('a' - 10));
        hex[++j] = data[i] & 0xf;
        hex[j] += (hex[j] < 10 ? '0': ('a' - 10));
    }

#if HAVE__WRITE
    result = _write(*(int *)priv, hex, (unsigned int)(size * 2));
#elif defined(__MINGW64__)
    result = write(*(int *)priv, hex, (unsigned int)(size * 2));
#else
    result = write(*(int *)priv, hex, (size_t)(size * 2));
#endif

    return result;
}

static SIXELSTATUS
sixel_encoder_ensure_cell_size(sixel_encoder_t *encoder)
{
#if defined(TIOCGWINSZ)
    struct winsize ws;
    int result;
    int fd = 0;

    if (encoder->cell_width > 0 && encoder->cell_height > 0) {
        return SIXEL_OK;
    }

#if HAVE__OPEN
    fd = _open("/dev/tty", O_RDONLY);
#else
    fd = open("/dev/tty", O_RDONLY);
#endif  /* #if HAVE__OPEN */
    if (fd >= 0) {
        result = ioctl(fd, TIOCGWINSZ, &ws);
        close(fd);
    } else {
        sixel_helper_set_additional_message(
            "failed to open /dev/tty");
        return (SIXEL_LIBC_ERROR | (errno & 0xff));
    }
    if (result != 0) {
        sixel_helper_set_additional_message(
            "failed to query terminal geometry with ioctl().");
        return (SIXEL_LIBC_ERROR | (errno & 0xff));
    }

    if (ws.ws_col <= 0 || ws.ws_row <= 0 ||
        ws.ws_xpixel <= ws.ws_col || ws.ws_ypixel <= ws.ws_row) {
        sixel_helper_set_additional_message(
            "terminal does not report pixel cell size for drcs option.");
        return SIXEL_BAD_ARGUMENT;
    }

    encoder->cell_width = ws.ws_xpixel / ws.ws_col;
    encoder->cell_height = ws.ws_ypixel / ws.ws_row;
    if (encoder->cell_width <= 0 || encoder->cell_height <= 0) {
        sixel_helper_set_additional_message(
            "terminal cell size reported zero via ioctl().");
        return SIXEL_BAD_ARGUMENT;
    }

    return SIXEL_OK;
#else
    (void) encoder;
    sixel_helper_set_additional_message(
        "drcs option is not supported on this platform.");
    return SIXEL_NOT_IMPLEMENTED;
#endif
}


/* returns monochrome dithering context object */
static SIXELSTATUS
sixel_prepare_monochrome_palette(
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
            "sixel_prepare_monochrome_palette: sixel_dither_get() failed.");
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }

    status = SIXEL_OK;

end:
    return status;
}


/* returns dithering context object with specified builtin palette */
typedef struct palette_conversion {
    unsigned char *original;
    unsigned char *copy;
    size_t size;
    int convert_inplace;
    int converted;
    int frame_colorspace;
} palette_conversion_t;

static SIXELSTATUS
sixel_encoder_convert_palette(sixel_encoder_t *encoder,
                              sixel_output_t *output,
                              sixel_dither_t *dither,
                              int frame_colorspace,
                              int pixelformat,
                              palette_conversion_t *ctx)
{
    SIXELSTATUS status = SIXEL_OK;
    unsigned char *palette;
    int palette_colors;

    ctx->original = NULL;
    ctx->copy = NULL;
    ctx->size = 0;
    ctx->convert_inplace = 0;
    ctx->converted = 0;
    ctx->frame_colorspace = frame_colorspace;

    palette = sixel_dither_get_palette(dither);
    palette_colors = sixel_dither_get_num_of_palette_colors(dither);
    ctx->original = palette;

    if (palette == NULL || palette_colors <= 0 ||
            frame_colorspace == output->colorspace) {
        return SIXEL_OK;
    }

    ctx->size = (size_t)palette_colors * 3;

    output->pixelformat = SIXEL_PIXELFORMAT_RGB888;
    output->source_colorspace = frame_colorspace;

    if (palette != (unsigned char *)(dither + 1)) {
        ctx->copy = (unsigned char *)sixel_allocator_malloc(
            encoder->allocator, ctx->size);
        if (ctx->copy == NULL) {
            sixel_helper_set_additional_message(
                "sixel_encoder_convert_palette: "
                "sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        memcpy(ctx->copy, palette, ctx->size);
        dither->palette = ctx->copy;
    } else {
        ctx->convert_inplace = 1;
    }

    status = sixel_output_convert_colorspace(output,
                                             dither->palette,
                                             ctx->size);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    ctx->converted = 1;

end:
    output->pixelformat = pixelformat;
    output->source_colorspace = frame_colorspace;

    return status;
}

static void
sixel_encoder_restore_palette(sixel_encoder_t *encoder,
                              sixel_dither_t *dither,
                              palette_conversion_t *ctx)
{
    if (ctx->copy) {
        dither->palette = ctx->original;
        sixel_allocator_free(encoder->allocator, ctx->copy);
        ctx->copy = NULL;
    } else if (ctx->convert_inplace && ctx->converted &&
               ctx->original && ctx->size > 0) {
        (void)sixel_helper_convert_colorspace(ctx->original,
                                              ctx->size,
                                              SIXEL_PIXELFORMAT_RGB888,
                                              SIXEL_COLORSPACE_GAMMA,
                                              ctx->frame_colorspace);
    }
}

static SIXELSTATUS
sixel_prepare_builtin_palette(
    sixel_dither_t /* out */ **dither,
    int            /* in */  builtin_palette)
{
    SIXELSTATUS status = SIXEL_FALSE;

    *dither = sixel_dither_get(builtin_palette);
    if (*dither == NULL) {
        sixel_helper_set_additional_message(
            "sixel_prepare_builtin_palette: sixel_dither_get() failed.");
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }

    status = SIXEL_OK;

end:
    return status;
}

static int
sixel_encoder_thumbnail_hint(sixel_encoder_t *encoder)
{
    int width_hint;
    int height_hint;
    long base;
    long size;

    width_hint = 0;
    height_hint = 0;
    base = 0;
    size = 0;

    if (encoder == NULL) {
        return 0;
    }

    width_hint = encoder->pixelwidth;
    height_hint = encoder->pixelheight;

    /* Request extra resolution for downscaling to preserve detail. */
    if (width_hint > 0 && height_hint > 0) {
        /* Follow the CLI rule: double the larger axis before doubling
         * again for the final request size. */
        if (width_hint >= height_hint) {
            base = (long)width_hint;
        } else {
            base = (long)height_hint;
        }
        base *= 2L;
    } else if (width_hint > 0) {
        base = (long)width_hint;
    } else if (height_hint > 0) {
        base = (long)height_hint;
    } else {
        return 0;
    }

    size = base * 2L;
    if (size > (long)INT_MAX) {
        size = (long)INT_MAX;
    }
    if (size < 1L) {
        size = 1L;
    }

    return (int)size;
}


typedef struct sixel_callback_context_for_mapfile {
    int reqcolors;
    sixel_dither_t *dither;
    sixel_allocator_t *allocator;
    int working_colorspace;
} sixel_callback_context_for_mapfile_t;


/* callback function for sixel_helper_load_image_file() */
static SIXELSTATUS
load_image_callback_for_palette(
    sixel_frame_t   /* in */    *frame, /* frame object from image loader */
    void            /* in */    *data)  /* private data */
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_callback_context_for_mapfile_t *callback_context;

    /* get callback context object from the private data */
    callback_context = (sixel_callback_context_for_mapfile_t *)data;

    status = sixel_frame_ensure_colorspace(frame,
                                           callback_context->working_colorspace);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    switch (sixel_frame_get_pixelformat(frame)) {
    case SIXEL_PIXELFORMAT_PAL1:
    case SIXEL_PIXELFORMAT_PAL2:
    case SIXEL_PIXELFORMAT_PAL4:
    case SIXEL_PIXELFORMAT_PAL8:
        if (sixel_frame_get_palette(frame) == NULL) {
            status = SIXEL_LOGIC_ERROR;
            goto end;
        }
        /* create new dither object */
        status = sixel_dither_new(
            &callback_context->dither,
            sixel_frame_get_ncolors(frame),
            callback_context->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        /* use palette which is extracted from the image */
        sixel_dither_set_palette(callback_context->dither,
                                 sixel_frame_get_palette(frame));
        /* success */
        status = SIXEL_OK;
        break;
    case SIXEL_PIXELFORMAT_G1:
        /* use 1bpp grayscale builtin palette */
        callback_context->dither = sixel_dither_get(SIXEL_BUILTIN_G1);
        /* success */
        status = SIXEL_OK;
        break;
    case SIXEL_PIXELFORMAT_G2:
        /* use 2bpp grayscale builtin palette */
        callback_context->dither = sixel_dither_get(SIXEL_BUILTIN_G1);
        callback_context->dither = sixel_dither_get(SIXEL_BUILTIN_G2);
        /* success */
        status = SIXEL_OK;
        break;
    case SIXEL_PIXELFORMAT_G4:
        /* use 4bpp grayscale builtin palette */
        callback_context->dither = sixel_dither_get(SIXEL_BUILTIN_G4);
        /* success */
        status = SIXEL_OK;
        break;
    case SIXEL_PIXELFORMAT_G8:
        /* use 8bpp grayscale builtin palette */
        callback_context->dither = sixel_dither_get(SIXEL_BUILTIN_G8);
        /* success */
        status = SIXEL_OK;
        break;
    default:
        /* create new dither object */
        status = sixel_dither_new(
            &callback_context->dither,
            callback_context->reqcolors,
            callback_context->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        /* create adaptive palette from given frame object */
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

        /* success */
        status = SIXEL_OK;

        break;
    }

end:
    return status;
}


/* create palette from specified map file */
static SIXELSTATUS
sixel_prepare_specified_palette(
    sixel_dither_t  /* out */   **dither,
    sixel_encoder_t /* in */    *encoder)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_callback_context_for_mapfile_t callback_context;

    callback_context.reqcolors = encoder->reqcolors;
    callback_context.dither = NULL;
    callback_context.allocator = encoder->allocator;
    callback_context.working_colorspace = encoder->working_colorspace;

    sixel_helper_set_loader_trace(encoder->verbose);
    sixel_helper_set_thumbnail_size_hint(
        sixel_encoder_thumbnail_hint(encoder));
    status = sixel_helper_load_image_file(encoder->mapfile,
                                          1,   /* fstatic */
                                          1,   /* fuse_palette */
                                          SIXEL_PALETTE_MAX, /* reqcolors */
                                          encoder->bgcolor,
                                          SIXEL_LOOP_DISABLE,
                                          load_image_callback_for_palette,
                                          encoder->finsecure,
                                          encoder->cancel_flag,
                                          &callback_context,
                                          encoder->allocator);
    if (status != SIXEL_OK) {
        return status;
    }

    if (! callback_context.dither) {
        sixel_helper_set_additional_message(
            "sixel_prepare_specified_palette() failed.\n"
            "reason: mapfile is empty.");
        return SIXEL_BAD_INPUT;
    }

    *dither = callback_context.dither;

    return status;
}


/* create dither object from a frame */
static SIXELSTATUS
sixel_encoder_prepare_palette(
    sixel_encoder_t *encoder,  /* encoder object */
    sixel_frame_t   *frame,    /* input frame object */
    sixel_dither_t  **dither)  /* dither object to be created from the frame */
{
    SIXELSTATUS status = SIXEL_FALSE;
    int histogram_colors;

    switch (encoder->color_option) {
    case SIXEL_COLOR_OPTION_HIGHCOLOR:
        if (encoder->dither_cache) {
            *dither = encoder->dither_cache;
            status = SIXEL_OK;
        } else {
            status = sixel_dither_new(dither, (-1), encoder->allocator);
            sixel_dither_set_pixelformat(*dither, sixel_frame_get_pixelformat(frame));
        }
        goto end;
    case SIXEL_COLOR_OPTION_MONOCHROME:
        if (encoder->dither_cache) {
            *dither = encoder->dither_cache;
            status = SIXEL_OK;
        } else {
            status = sixel_prepare_monochrome_palette(dither, encoder->finvert);
        }
        goto end;
    case SIXEL_COLOR_OPTION_MAPFILE:
        if (encoder->dither_cache) {
            *dither = encoder->dither_cache;
            status = SIXEL_OK;
        } else {
            status = sixel_prepare_specified_palette(dither, encoder);
        }
        goto end;
    case SIXEL_COLOR_OPTION_BUILTIN:
        if (encoder->dither_cache) {
            *dither = encoder->dither_cache;
            status = SIXEL_OK;
        } else {
            status = sixel_prepare_builtin_palette(dither, encoder->builtin_palette);
        }
        goto end;
    case SIXEL_COLOR_OPTION_DEFAULT:
    default:
        break;
    }

    if (sixel_frame_get_pixelformat(frame) & SIXEL_FORMATTYPE_PALETTE) {
        if (!sixel_frame_get_palette(frame)) {
            status = SIXEL_LOGIC_ERROR;
            goto end;
        }
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
        if (*dither && encoder->dither_cache) {
            sixel_dither_unref(encoder->dither_cache);
        }
        goto end;
    }

    if (sixel_frame_get_pixelformat(frame) & SIXEL_FORMATTYPE_GRAYSCALE) {
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
            *dither = NULL;
            status = SIXEL_LOGIC_ERROR;
            goto end;
        }
        if (*dither && encoder->dither_cache) {
            sixel_dither_unref(encoder->dither_cache);
        }
        sixel_dither_set_pixelformat(*dither, sixel_frame_get_pixelformat(frame));
        status = SIXEL_OK;
        goto end;
    }

    if (encoder->dither_cache) {
        sixel_dither_unref(encoder->dither_cache);
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

    status = SIXEL_OK;

end:
    return status;
}


/* resize a frame with settings of specified encoder object */
static SIXELSTATUS
sixel_encoder_do_resize(
    sixel_encoder_t /* in */    *encoder,   /* encoder object */
    sixel_frame_t   /* in */    *frame)     /* frame object to be resized */
{
    SIXELSTATUS status = SIXEL_FALSE;
    int src_width;
    int src_height;
    int dst_width;
    int dst_height;

    /* get frame width and height */
    src_width = sixel_frame_get_width(frame);
    src_height = sixel_frame_get_height(frame);

    if (src_width < 1) {
         sixel_helper_set_additional_message(
             "sixel_encoder_do_resize: "
             "detected a frame with a non-positive width.");
        return SIXEL_BAD_ARGUMENT;
    }

    if (src_height < 1) {
         sixel_helper_set_additional_message(
             "sixel_encoder_do_resize: "
             "detected a frame with a non-positive height.");
        return SIXEL_BAD_ARGUMENT;
    }

    /* settings around scaling */
    dst_width = encoder->pixelwidth;    /* may be -1 (default) */
    dst_height = encoder->pixelheight;  /* may be -1 (default) */

    /* if the encoder has percentwidth or percentheight property,
       convert them to pixelwidth / pixelheight */
    if (encoder->percentwidth > 0) {
        dst_width = src_width * encoder->percentwidth / 100;
    }
    if (encoder->percentheight > 0) {
        dst_height = src_height * encoder->percentheight / 100;
    }

    /* if only either width or height is set, set also the other
       to retain frame aspect ratio */
    if (dst_width > 0 && dst_height <= 0) {
        dst_height = src_height * dst_width / src_width;
    }
    if (dst_height > 0 && dst_width <= 0) {
        dst_width = src_width * dst_height / src_height;
    }

    /* do resize */
    if (dst_width > 0 && dst_height > 0) {
        status = sixel_frame_resize(frame, dst_width, dst_height,
                                    encoder->method_for_resampling);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    /* success */
    status = SIXEL_OK;

end:
    return status;
}


/* clip a frame with settings of specified encoder object */
static SIXELSTATUS
sixel_encoder_do_clip(
    sixel_encoder_t /* in */    *encoder,   /* encoder object */
    sixel_frame_t   /* in */    *frame)     /* frame object to be resized */
{
    SIXELSTATUS status = SIXEL_FALSE;
    int src_width;
    int src_height;
    int clip_x;
    int clip_y;
    int clip_w;
    int clip_h;

    /* get frame width and height */
    src_width = sixel_frame_get_width(frame);
    src_height = sixel_frame_get_height(frame);

    /* settings around clipping */
    clip_x = encoder->clipx;
    clip_y = encoder->clipy;
    clip_w = encoder->clipwidth;
    clip_h = encoder->clipheight;

    /* adjust clipping width with comparing it to frame width */
    if (clip_w + clip_x > src_width) {
        if (clip_x > src_width) {
            clip_w = 0;
        } else {
            clip_w = src_width - clip_x;
        }
    }

    /* adjust clipping height with comparing it to frame height */
    if (clip_h + clip_y > src_height) {
        if (clip_y > src_height) {
            clip_h = 0;
        } else {
            clip_h = src_height - clip_y;
        }
    }

    /* do clipping */
    if (clip_w > 0 && clip_h > 0) {
        status = sixel_frame_clip(frame, clip_x, clip_y, clip_w, clip_h);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    /* success */
    status = SIXEL_OK;

end:
    return status;
}


static void
sixel_debug_print_palette(
    sixel_dither_t /* in */ *dither /* dithering object */
)
{
    unsigned char *palette;
    int i;

    palette = sixel_dither_get_palette(dither);
    fprintf(stderr, "palette:\n");
    for (i = 0; i < sixel_dither_get_num_of_palette_colors(dither); ++i) {
        fprintf(stderr, "%d: #%02x%02x%02x\n", i,
                palette[i * 3 + 0],
                palette[i * 3 + 1],
                palette[i * 3 + 2]);
    }
}


static SIXELSTATUS
sixel_encoder_output_without_macro(
    sixel_frame_t       /* in */ *frame,
    sixel_dither_t      /* in */ *dither,
    sixel_output_t      /* in */ *output,
    sixel_encoder_t     /* in */ *encoder)
{
    SIXELSTATUS status = SIXEL_OK;
    static unsigned char *p;
    int depth;
    enum { message_buffer_size = 256 };
    char message[message_buffer_size];
    int nwrite;
#if defined(HAVE_NANOSLEEP) || defined(HAVE_NANOSLEEP_WIN)
    int dulation;
    int delay;
    struct timespec tv;
#endif
    unsigned char *pixbuf;
    int width;
    int height;
    int pixelformat = 0;
    size_t size;
    int frame_colorspace = SIXEL_COLORSPACE_GAMMA;
    palette_conversion_t palette_ctx;

    memset(&palette_ctx, 0, sizeof(palette_ctx));
#if defined(HAVE_CLOCK) || defined(HAVE_CLOCK_WIN)
    sixel_clock_t last_clock;
#endif

    if (encoder == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encoder_output_without_macro: encoder object is null.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    if (encoder->color_option == SIXEL_COLOR_OPTION_DEFAULT) {
        sixel_dither_set_optimize_palette(dither, 1);
    }

    pixelformat = sixel_frame_get_pixelformat(frame);
    frame_colorspace = sixel_frame_get_colorspace(frame);
    output->pixelformat = pixelformat;
    output->source_colorspace = frame_colorspace;
    output->colorspace = encoder->output_colorspace;
    sixel_dither_set_pixelformat(dither, pixelformat);
    depth = sixel_helper_compute_depth(pixelformat);
    if (depth < 0) {
        status = SIXEL_LOGIC_ERROR;
        nwrite = sprintf(message,
                         "sixel_encoder_output_without_macro: "
                         "sixel_helper_compute_depth(%08x) failed.",
                         pixelformat);
        if (nwrite > 0) {
            sixel_helper_set_additional_message(message);
        }
        goto end;
    }

    width = sixel_frame_get_width(frame);
    height = sixel_frame_get_height(frame);
    size = (size_t)(width * height * depth);
    p = (unsigned char *)sixel_allocator_malloc(encoder->allocator, size);
    if (p == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encoder_output_without_macro: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
#if defined(HAVE_CLOCK)
    if (output->last_clock == 0) {
        output->last_clock = clock();
    }
#elif defined(HAVE_CLOCK_WIN)
    if (output->last_clock == 0) {
        output->last_clock = clock_win();
    }
#endif
#if defined(HAVE_NANOSLEEP) || defined(HAVE_NANOSLEEP_WIN)
    delay = sixel_frame_get_delay(frame);
    if (delay > 0 && !encoder->fignore_delay) {
# if defined(HAVE_CLOCK)
        last_clock = clock();
/* https://stackoverflow.com/questions/16697005/clock-and-clocks-per-sec-on-osx-10-7 */
#  if defined(__APPLE__)
        dulation = (int)((last_clock - output->last_clock) * 1000 * 1000
                          / 100000);
#  else
        dulation = (int)((last_clock - output->last_clock) * 1000 * 1000
                          / CLOCKS_PER_SEC);
#  endif
        output->last_clock = last_clock;
# elif defined(HAVE_CLOCK_WIN)
        last_clock = clock_win();
        dulation = (int)((last_clock - output->last_clock) * 1000 * 1000
                          / CLOCKS_PER_SEC);
        output->last_clock = last_clock;
# else
        dulation = 0;
# endif
        if (dulation < 1000 * 10 * delay) {
# if defined(HAVE_NANOSLEEP) || defined(HAVE_NANOSLEEP_WIN)
            tv.tv_sec = 0;
            tv.tv_nsec = (long)((1000 * 10 * delay - dulation) * 1000);
#  if defined(HAVE_NANOSLEEP)
            nanosleep(&tv, NULL);
#  else
            nanosleep_win(&tv, NULL);
#  endif
# endif
        }
    }
#endif

    pixbuf = sixel_frame_get_pixels(frame);
    memcpy(p, pixbuf, (size_t)(width * height * depth));

    status = sixel_output_convert_colorspace(output, p, size);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    if (encoder->cancel_flag && *encoder->cancel_flag) {
        goto end;
    }

    status = sixel_encoder_convert_palette(encoder,
                                           output,
                                           dither,
                                           frame_colorspace,
                                           pixelformat,
                                           &palette_ctx);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_encode(p, width, height, depth, dither, output);
    if (status != SIXEL_OK) {
        goto end;
    }

end:
    sixel_encoder_restore_palette(encoder, dither, &palette_ctx);
    output->pixelformat = pixelformat;
    output->source_colorspace = frame_colorspace;
    sixel_allocator_free(encoder->allocator, p);

    return status;
}


static SIXELSTATUS
sixel_encoder_output_with_macro(
    sixel_frame_t   /* in */ *frame,
    sixel_dither_t  /* in */ *dither,
    sixel_output_t  /* in */ *output,
    sixel_encoder_t /* in */ *encoder)
{
    SIXELSTATUS status = SIXEL_OK;
    enum { message_buffer_size = 256 };
    char buffer[message_buffer_size];
    int nwrite;
#if defined(HAVE_NANOSLEEP) || defined(HAVE_NANOSLEEP_WIN)
    int dulation;
    struct timespec tv;
#endif
    int width;
    int height;
    int pixelformat;
    int depth;
    size_t size = 0;
    int frame_colorspace = SIXEL_COLORSPACE_GAMMA;
    unsigned char *converted = NULL;
    palette_conversion_t palette_ctx;
#if defined(HAVE_NANOSLEEP) || defined(HAVE_NANOSLEEP_WIN)
    int delay;
#endif
#if defined(HAVE_CLOCK) || defined(HAVE_CLOCK_WIN)
    sixel_clock_t last_clock;
#endif

    memset(&palette_ctx, 0, sizeof(palette_ctx));

#if defined(HAVE_CLOCK)
    if (output->last_clock == 0) {
        output->last_clock = clock();
    }
#elif defined(HAVE_CLOCK_WIN)
    if (output->last_clock == 0) {
        output->last_clock = clock_win();
    }
#endif

    width = sixel_frame_get_width(frame);
    height = sixel_frame_get_height(frame);
    pixelformat = sixel_frame_get_pixelformat(frame);
    depth = sixel_helper_compute_depth(pixelformat);
    if (depth < 0) {
        status = SIXEL_LOGIC_ERROR;
        sixel_helper_set_additional_message(
            "sixel_encoder_output_with_macro: "
            "sixel_helper_compute_depth() failed.");
        goto end;
    }

    frame_colorspace = sixel_frame_get_colorspace(frame);
    size = (size_t)width * (size_t)height * (size_t)depth;
    converted = (unsigned char *)sixel_allocator_malloc(
        encoder->allocator, size);
    if (converted == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encoder_output_with_macro: "
            "sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    memcpy(converted, sixel_frame_get_pixels(frame), size);
    output->pixelformat = pixelformat;
    output->source_colorspace = frame_colorspace;
    output->colorspace = encoder->output_colorspace;
    status = sixel_output_convert_colorspace(output, converted, size);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_encoder_convert_palette(encoder,
                                           output,
                                           dither,
                                           frame_colorspace,
                                           pixelformat,
                                           &palette_ctx);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    if (sixel_frame_get_loop_no(frame) == 0) {
        if (encoder->macro_number >= 0) {
            nwrite = sprintf(buffer, "\033P%d;0;1!z",
                             encoder->macro_number);
        } else {
            nwrite = sprintf(buffer, "\033P%d;0;1!z",
                             sixel_frame_get_frame_no(frame));
        }
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "sixel_encoder_output_with_macro: sprintf() failed.");
            goto end;
        }
        nwrite = sixel_write_callback(buffer,
                                      (int)strlen(buffer),
                                      &encoder->outfd);
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "sixel_encoder_output_with_macro: "
                "sixel_write_callback() failed.");
            goto end;
        }

        status = sixel_encode(converted,
                              width,
                              height,
                              depth,
                              dither,
                              output);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        nwrite = sixel_write_callback("\033\\", 2, &encoder->outfd);
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "sixel_encoder_output_with_macro: "
                "sixel_write_callback() failed.");
            goto end;
        }
    }
    if (encoder->macro_number < 0) {
        nwrite = sprintf(buffer, "\033[%d*z",
                         sixel_frame_get_frame_no(frame));
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "sixel_encoder_output_with_macro: sprintf() failed.");
        }
        nwrite = sixel_write_callback(buffer,
                                      (int)strlen(buffer),
                                      &encoder->outfd);
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "sixel_encoder_output_with_macro: "
                "sixel_write_callback() failed.");
            goto end;
        }
#if defined(HAVE_NANOSLEEP) || defined(HAVE_NANOSLEEP_WIN)
        delay = sixel_frame_get_delay(frame);
        if (delay > 0 && !encoder->fignore_delay) {
# if defined(HAVE_CLOCK)
            last_clock = clock();
/* https://stackoverflow.com/questions/16697005/clock-and-clocks-per-sec-on-osx-10-7 */
#  if defined(__APPLE__)
            dulation = (int)((last_clock - output->last_clock) * 1000 * 1000
                             / 100000);
#  else
            dulation = (int)((last_clock - output->last_clock) * 1000 * 1000
                             / CLOCKS_PER_SEC);
#  endif
            output->last_clock = last_clock;
# elif defined(HAVE_CLOCK_WIN)
            last_clock = clock_win();
            dulation = (int)((last_clock - output->last_clock) * 1000 * 1000
                             / CLOCKS_PER_SEC);
            output->last_clock = last_clock;
# else
            dulation = 0;
# endif
            if (dulation < 1000 * 10 * delay) {
# if defined(HAVE_NANOSLEEP) || defined(HAVE_NANOSLEEP_WIN)
                tv.tv_sec = 0;
                tv.tv_nsec = (long)((1000 * 10 * delay - dulation) * 1000);
#  if defined(HAVE_NANOSLEEP)
                nanosleep(&tv, NULL);
#  else
                nanosleep_win(&tv, NULL);
#  endif
# endif
            }
        }
#endif
    }

end:
    sixel_encoder_restore_palette(encoder, dither, &palette_ctx);
    output->pixelformat = pixelformat;
    output->source_colorspace = frame_colorspace;
    sixel_allocator_free(encoder->allocator, converted);

    return status;
}


static SIXELSTATUS
sixel_encoder_emit_iso2022_chars(
    sixel_encoder_t *encoder,
    sixel_frame_t *frame
)
{
    char *buf_p, *buf;
    int col, row;
    int charset = encoder->start_dscs;
    int is_96cs = 0;
    unsigned int code;
    int num_cols, num_rows;
    SIXELSTATUS status;
    size_t alloc_size;
    int nwrite;
    int target_fd;

    code = 0x100020 + (is_96cs ? 0x80 : 0) + charset * 0x100;
    num_cols = (sixel_frame_get_width(frame) + encoder->cell_width - 1)
             / encoder->cell_width;
    num_rows = (sixel_frame_get_height(frame) + encoder->cell_height - 1)
             / encoder->cell_height;

    /* cols x rows + designation(4 chars) + SI + SO + LFs */
    alloc_size = num_cols * num_rows + (num_cols * num_rows / 96 + 1) * 4 + 2 + num_rows;
    buf_p = buf = sixel_allocator_malloc(encoder->allocator, alloc_size);
    if (buf == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encoder_emit_iso2022_chars: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    code = 0x20;
    *(buf_p++) = '\016';  /* SI */
    *(buf_p++) = '\033';
    *(buf_p++) = ')';
    *(buf_p++) = ' ';
    *(buf_p++) = charset;
    for(row = 0; row < num_rows; row++) {
        for(col = 0; col < num_cols; col++) {
            if ((code & 0x7f) == 0x0) {
                if (charset == 0x7e) {
                    is_96cs = 1 - is_96cs;
                    charset = '0';
                } else {
                    charset++;
                }
                code = 0x20;
                *(buf_p++) = '\033';
                *(buf_p++) = is_96cs ? '-': ')';
                *(buf_p++) = ' ';
                *(buf_p++) = charset;
            }
            *(buf_p++) = code++;
        }
        *(buf_p++) = '\n';
    }
    *(buf_p++) = '\017';  /* SO */

    if (encoder->tile_outfd >= 0) {
        target_fd = encoder->tile_outfd;
    } else {
        target_fd = encoder->outfd;
    }

    nwrite = write(target_fd, buf, buf_p - buf);
    if (nwrite != buf_p - buf) {
        sixel_helper_set_additional_message(
            "sixel_encoder_emit_iso2022_chars: write() failed.");
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }

    sixel_allocator_free(encoder->allocator, buf);

    status = SIXEL_OK;

end:
    return status;
}


/*
 * This routine is derived from mlterm's drcssixel.c
 * (https://raw.githubusercontent.com/arakiken/mlterm/master/drcssixel/drcssixel.c).
 * The original implementation is credited to Araki Ken.
 * Adjusted here to integrate with libsixel's encoder pipeline.
 */
static SIXELSTATUS
sixel_encoder_emit_drcsmmv2_chars(
    sixel_encoder_t *encoder,
    sixel_frame_t *frame
)
{
    char *buf_p, *buf;
    int col, row;
    int charset = encoder->start_dscs;
    int is_96cs = 0;
    unsigned int code;
    int num_cols, num_rows;
    SIXELSTATUS status;
    size_t alloc_size;
    int nwrite;
    int target_fd;

    code = 0x100020 + (is_96cs ? 0x80 : 0) + charset * 0x100;
    num_cols = (sixel_frame_get_width(frame) + encoder->cell_width - 1)
             / encoder->cell_width;
    num_rows = (sixel_frame_get_height(frame) + encoder->cell_height - 1)
             / encoder->cell_height;

    /* cols x rows x 4(out of BMP) + rows(LFs) */
    alloc_size = num_cols * num_rows * 4 + num_rows;
    buf_p = buf = sixel_allocator_malloc(encoder->allocator, alloc_size);
    if (buf == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encoder_emit_drcsmmv2_chars: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    for(row = 0; row < num_rows; row++) {
        for(col = 0; col < num_cols; col++) {
            *(buf_p++) = ((code >> 18) & 0x07) | 0xf0;
            *(buf_p++) = ((code >> 12) & 0x3f) | 0x80;
            *(buf_p++) = ((code >> 6) & 0x3f) | 0x80;
            *(buf_p++) = (code & 0x3f) | 0x80;
            code++;
            if ((code & 0x7f) == 0x0) {
                if (charset == 0x7e) {
                    is_96cs = 1 - is_96cs;
                    charset = '0';
                } else {
                    charset++;
                }
                code = 0x100020 + (is_96cs ? 0x80 : 0) + charset * 0x100;
            }
        }
        *(buf_p++) = '\n';
    }

    if (charset == 0x7e) {
        is_96cs = 1 - is_96cs;
    } else {
        charset = '0';
        charset++;
    }
    if (encoder->tile_outfd >= 0) {
        target_fd = encoder->tile_outfd;
    } else {
        target_fd = encoder->outfd;
    }

    nwrite = write(target_fd, buf, buf_p - buf);
    if (nwrite != buf_p - buf) {
        sixel_helper_set_additional_message(
            "sixel_encoder_emit_drcsmmv2_chars: write() failed.");
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }

    sixel_allocator_free(encoder->allocator, buf);

    status = SIXEL_OK;

end:
    return status;
}

static SIXELSTATUS
sixel_encoder_encode_frame(
    sixel_encoder_t *encoder,
    sixel_frame_t   *frame,
    sixel_output_t  *output)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_dither_t *dither = NULL;
    int height;
    int is_animation = 0;
    int nwrite;
    char buf[256];
    sixel_write_function fn_write = NULL;

    /* evaluate -w, -h, and -c option: crop/scale input source */
    if (encoder->clipfirst) {
        /* clipping */
        status = sixel_encoder_do_clip(encoder, frame);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        /* scaling */
        status = sixel_encoder_do_resize(encoder, frame);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    } else {
        /* scaling */
        status = sixel_encoder_do_resize(encoder, frame);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        /* clipping */
        status = sixel_encoder_do_clip(encoder, frame);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    status = sixel_frame_ensure_colorspace(frame,
                                           encoder->working_colorspace);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    /* prepare dither context */
    status = sixel_encoder_prepare_palette(encoder, frame, &dither);
    if (status != SIXEL_OK) {
        dither = NULL;
        goto end;
    }

    if (encoder->dither_cache != NULL) {
        encoder->dither_cache = dither;
        sixel_dither_ref(dither);
    }

    if (encoder->fdrcs) {
        status = sixel_encoder_ensure_cell_size(encoder);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        if (encoder->fuse_macro || encoder->macro_number >= 0) {
            sixel_helper_set_additional_message(
                "drcs option cannot be used together with macro output.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
    }

    /* evaluate -v option: print palette */
    if (encoder->verbose) {
        if ((sixel_frame_get_pixelformat(frame) & SIXEL_FORMATTYPE_PALETTE)) {
            sixel_debug_print_palette(dither);
        }
    }

    /* evaluate -d option: set method for diffusion */
    sixel_dither_set_diffusion_type(dither, encoder->method_for_diffuse);
    sixel_dither_set_diffusion_scan(dither, encoder->method_for_scan);
    sixel_dither_set_diffusion_carry(dither, encoder->method_for_carry);

    /* evaluate -C option: set complexion score */
    if (encoder->complexion > 1) {
        sixel_dither_set_complexion_score(dither, encoder->complexion);
    }

    if (output) {
        sixel_output_ref(output);
    } else {
        /* create output context */
        if (encoder->fuse_macro || encoder->macro_number >= 0) {
            /* -u or -n option */
            fn_write = sixel_hex_write_callback;
        } else {
            fn_write = sixel_write_callback;
        }
        status = sixel_output_new(&output,
                                  fn_write,
                                  &encoder->outfd,
                                  encoder->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    if (encoder->fdrcs) {
        sixel_output_set_skip_dcs_envelope(output, 1);
        sixel_output_set_skip_header(output, 1);
    }

    sixel_output_set_8bit_availability(output, encoder->f8bit);
    sixel_output_set_gri_arg_limit(output, encoder->has_gri_arg_limit);
    sixel_output_set_palette_type(output, encoder->palette_type);
    sixel_output_set_penetrate_multiplexer(
        output, encoder->penetrate_multiplexer);
    sixel_output_set_encode_policy(output, encoder->encode_policy);
    sixel_output_set_ormode(output, encoder->ormode);

    if (sixel_frame_get_multiframe(frame) && !encoder->fstatic) {
        if (sixel_frame_get_loop_no(frame) != 0 || sixel_frame_get_frame_no(frame) != 0) {
            is_animation = 1;
        }
        height = sixel_frame_get_height(frame);
        (void) sixel_tty_scroll(sixel_write_callback, encoder->outfd, height, is_animation);
    }

    if (encoder->cancel_flag && *encoder->cancel_flag) {
        status = SIXEL_INTERRUPTED;
        goto end;
    }

    if (encoder->fdrcs) {  /* -@ option */
        nwrite = sprintf(buf,
                         "%s%s1;0;0;%d;1;3;%d;0{ %c",
                         (encoder->drcs_mmv > 0) ? (
                             encoder->f8bit ? "\233?8800h": "\033[?8800h"
                         ): "",
                         encoder->f8bit ? "\220": "\033P",
                         encoder->cell_width,
                         encoder->cell_height,
                         encoder->start_dscs);
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "sixel_encoder_encode_frame: sprintf() failed.");
            goto end;
        }
        nwrite = fn_write(buf, nwrite, &encoder->outfd);
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "sixel_encoder_encode_frame: write() failed.");
            goto end;
        }
    }

    /* output sixel: junction of multi-frame processing strategy */
    if (encoder->fuse_macro) {  /* -u option */
        /* use macro */
        status = sixel_encoder_output_with_macro(frame, dither, output, encoder);
    } else if (encoder->macro_number >= 0) { /* -n option */
        /* use macro */
        status = sixel_encoder_output_with_macro(frame, dither, output, encoder);
    } else {
        /* do not use macro */
        status = sixel_encoder_output_without_macro(frame, dither, output, encoder);
    }
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    if (encoder->cancel_flag && *encoder->cancel_flag) {
        nwrite = sixel_write_callback("\x18\033\\", 3, &encoder->outfd);
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "sixel_encoder_encode_frame: sixel_write_callback() failed.");
            goto end;
        }
        status = SIXEL_INTERRUPTED;
    }
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    if (encoder->fdrcs) {  /* -@ option */
        if (encoder->f8bit) {
            nwrite = fn_write("\234", 1, &encoder->outfd);
        } else {
            nwrite = fn_write("\033\\", 2, &encoder->outfd);
        }
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "sixel_encoder_encode_frame: fn_write() failed.");
            goto end;
        }

        if (encoder->tile_outfd >= 0) {
            if (encoder->drcs_mmv == 0) {
                status = sixel_encoder_emit_iso2022_chars(encoder, frame);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
            } else {
                status = sixel_encoder_emit_drcsmmv2_chars(encoder, frame);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
            }
        }
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
    char const *env_default_bgcolor = NULL;
    char const *env_default_ncolors = NULL;
    int ncolors;
#if HAVE__DUPENV_S
    errno_t e;
    size_t len;
#endif  /* HAVE__DUPENV_S */

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
    (*ppencoder)->color_option          = SIXEL_COLOR_OPTION_DEFAULT;
    (*ppencoder)->builtin_palette       = 0;
    (*ppencoder)->method_for_diffuse    = SIXEL_DIFFUSE_AUTO;
    (*ppencoder)->method_for_scan       = SIXEL_SCAN_AUTO;
    (*ppencoder)->method_for_carry      = SIXEL_CARRY_AUTO;
    (*ppencoder)->method_for_largest    = SIXEL_LARGE_AUTO;
    (*ppencoder)->method_for_rep        = SIXEL_REP_AUTO;
    (*ppencoder)->quality_mode          = SIXEL_QUALITY_AUTO;
    (*ppencoder)->method_for_resampling = SIXEL_RES_BILINEAR;
    (*ppencoder)->loop_mode             = SIXEL_LOOP_AUTO;
    (*ppencoder)->palette_type          = SIXEL_PALETTETYPE_AUTO;
    (*ppencoder)->f8bit                 = 0;
    (*ppencoder)->has_gri_arg_limit     = 0;
    (*ppencoder)->finvert               = 0;
    (*ppencoder)->fuse_macro            = 0;
    (*ppencoder)->fdrcs                 = 0;
    (*ppencoder)->fignore_delay         = 0;
    (*ppencoder)->complexion            = 1;
    (*ppencoder)->fstatic               = 0;
    (*ppencoder)->cell_width            = 0;
    (*ppencoder)->cell_height           = 0;
    (*ppencoder)->pixelwidth            = (-1);
    (*ppencoder)->pixelheight           = (-1);
    (*ppencoder)->percentwidth          = (-1);
    (*ppencoder)->percentheight         = (-1);
    (*ppencoder)->clipx                 = 0;
    (*ppencoder)->clipy                 = 0;
    (*ppencoder)->clipwidth             = 0;
    (*ppencoder)->clipheight            = 0;
    (*ppencoder)->clipfirst             = 0;
    (*ppencoder)->macro_number          = (-1);
    (*ppencoder)->verbose               = 0;
    (*ppencoder)->penetrate_multiplexer = 0;
    (*ppencoder)->encode_policy         = SIXEL_ENCODEPOLICY_AUTO;
    (*ppencoder)->working_colorspace    = SIXEL_COLORSPACE_GAMMA;
    (*ppencoder)->output_colorspace     = SIXEL_COLORSPACE_GAMMA;
    (*ppencoder)->ormode                = 0;
    (*ppencoder)->pipe_mode             = 0;
    (*ppencoder)->bgcolor               = NULL;
    (*ppencoder)->outfd                 = STDOUT_FILENO;
    (*ppencoder)->tile_outfd            = (-1);
    (*ppencoder)->finsecure             = 0;
    (*ppencoder)->cancel_flag           = NULL;
    (*ppencoder)->dither_cache          = NULL;
    (*ppencoder)->start_dscs            = '0';
    (*ppencoder)->drcs_mmv              = 2;
    (*ppencoder)->allocator             = allocator;

    /* evaluate environment variable ${SIXEL_BGCOLOR} */
#if HAVE__DUPENV_S
    e = _dupenv_s(&env_default_bgcolor, &len, "SIXEL_BGCOLOR");
    if (e != (0)) {
        sixel_helper_set_additional_message(
            "failed to get environment variable $SIXEL_BGCOLOR.");
        return (SIXEL_LIBC_ERROR | (e & 0xff));
    }
#else
    env_default_bgcolor = getenv("SIXEL_BGCOLOR");
#endif  /* HAVE__DUPENV_S */
    if (env_default_bgcolor != NULL) {
        status = sixel_parse_x_colorspec(&(*ppencoder)->bgcolor,
                                         env_default_bgcolor,
                                         allocator);
        if (SIXEL_FAILED(status)) {
            goto error;
        }
    }

    /* evaluate environment variable ${SIXEL_COLORS} */
#if HAVE__DUPENV_S
    e = _dupenv_s(&env_default_bgcolor, &len, "SIXEL_COLORS");
    if (e != (0)) {
        sixel_helper_set_additional_message(
            "failed to get environment variable $SIXEL_COLORS.");
        return (SIXEL_LIBC_ERROR | (e & 0xff));
    }
#else
    env_default_ncolors = getenv("SIXEL_COLORS");
#endif  /* HAVE__DUPENV_S */
    if (env_default_ncolors) {
        ncolors = atoi(env_default_ncolors); /* may overflow */
        if (ncolors > 1 && ncolors <= SIXEL_PALETTE_MAX) {
            (*ppencoder)->reqcolors = ncolors;
        }
    }

    /* success */
    status = SIXEL_OK;

    goto end;

error:
    sixel_allocator_free(allocator, *ppencoder);
    sixel_allocator_unref(allocator);
    *ppencoder = NULL;

end:
#if HAVE__DUPENV_S
    free(env_default_bgcolor);
    free(env_default_ncolors);
#endif  /* HAVE__DUPENV_S */
    return status;
}


/* create encoder object (deprecated version) */
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


/* destroy encoder object */
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
#if HAVE__CLOSE
            (void) _close(encoder->outfd);
#else
            (void) close(encoder->outfd);
#endif  /* HAVE__CLOSE */
        }
        if (encoder->tile_outfd >= 0
            && encoder->tile_outfd != encoder->outfd
            && encoder->tile_outfd != STDOUT_FILENO
            && encoder->tile_outfd != STDERR_FILENO) {
#if HAVE__CLOSE
            (void) _close(encoder->tile_outfd);
#else
            (void) close(encoder->tile_outfd);
#endif  /* HAVE__CLOSE */
        }
        sixel_allocator_free(allocator, encoder);
        sixel_allocator_unref(allocator);
    }
}


/* increase reference count of encoder object (thread-unsafe) */
SIXELAPI void
sixel_encoder_ref(sixel_encoder_t *encoder)
{
    /* TODO: be thread safe */
    ++encoder->ref;
}


/* decrease reference count of encoder object (thread-unsafe) */
SIXELAPI void
sixel_encoder_unref(sixel_encoder_t *encoder)
{
    /* TODO: be thread safe */
    if (encoder != NULL && --encoder->ref == 0) {
        sixel_encoder_destroy(encoder);
    }
}


/* set cancel state flag to encoder object */
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


/* set an option flag to encoder object */
SIXELAPI SIXELSTATUS
sixel_encoder_setopt(
    sixel_encoder_t /* in */ *encoder,
    int             /* in */ arg,
    char const      /* in */ *value)
{
    SIXELSTATUS status = SIXEL_FALSE;
    int number;
    int parsed;
    char unit[32];
    char lowered[16];
    size_t len;
    size_t i;

    sixel_encoder_ref(encoder);

    switch(arg) {
    case SIXEL_OPTFLAG_OUTFILE:  /* o */
        if (*value == '\0') {
            sixel_helper_set_additional_message(
                "no file name specified.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        if (strcmp(value, "-") != 0) {
            if (encoder->outfd && encoder->outfd != STDOUT_FILENO) {
#if HAVE__CLOSE
                (void) _close(encoder->outfd);
#else
                (void) close(encoder->outfd);
#endif  /* HAVE__CLOSE */
            }
#if HAVE__OPEN
            encoder->outfd = _open(value,
                                   O_RDWR|O_CREAT|O_TRUNC,
                                   S_IRUSR|S_IWUSR);
#else
            encoder->outfd = open(value,
                                  O_RDWR|O_CREAT|O_TRUNC,
                                  S_IRUSR|S_IWUSR);
#endif  /* HAVE__OPEN */
        }
        break;
    case SIXEL_OPTFLAG_ALT_CHARSET_PATH:  /* T */
        if (*value == '\0') {
            sixel_helper_set_additional_message(
                "no file name specified.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        if (encoder->tile_outfd >= 0
            && encoder->tile_outfd != encoder->outfd
            && encoder->tile_outfd != STDOUT_FILENO
            && encoder->tile_outfd != STDERR_FILENO) {
#if HAVE__CLOSE
            (void) _close(encoder->tile_outfd);
#else
            (void) close(encoder->tile_outfd);
#endif  /* HAVE__CLOSE */
        }
        encoder->tile_outfd = (-1);
        if (strcmp(value, "-") == 0) {
            encoder->tile_outfd = STDOUT_FILENO;
        } else {
#if HAVE__OPEN
            encoder->tile_outfd = _open(value,
                                           O_RDWR|O_CREAT|O_TRUNC,
                                           S_IRUSR|S_IWUSR);
#else
            encoder->tile_outfd = open(value,
                                          O_RDWR|O_CREAT|O_TRUNC,
                                          S_IRUSR|S_IWUSR);
#endif  /* HAVE__OPEN */
            if (encoder->tile_outfd < 0) {
                sixel_helper_set_additional_message(
                    "sixel_encoder_setopt: failed to open tile"
                    " output path.");
                status = SIXEL_RUNTIME_ERROR;
                goto end;
            }
        }
        break;
    case SIXEL_OPTFLAG_7BIT_MODE:  /* 7 */
        encoder->f8bit = 0;
        break;
    case SIXEL_OPTFLAG_8BIT_MODE:  /* 8 */
        encoder->f8bit = 1;
        break;
    case SIXEL_OPTFLAG_HAS_GRI_ARG_LIMIT:  /* R */
        encoder->has_gri_arg_limit = 1;
        break;
    case SIXEL_OPTFLAG_COLORS:  /* p */
        encoder->reqcolors = atoi(value);
        if (encoder->complexion < 1) {
            sixel_helper_set_additional_message(
                "-p/--colors parameter must be 1 or more.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        } else if (encoder->complexion > 256) {
            sixel_helper_set_additional_message(
                "-p/--colors parameter must be less then or equal to 256.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_MAPFILE:  /* m */
        if (encoder->mapfile) {
            sixel_allocator_free(encoder->allocator, encoder->mapfile);
        }
        encoder->mapfile = arg_strdup(value, encoder->allocator);
        if (encoder->mapfile == NULL) {
            sixel_helper_set_additional_message(
                "sixel_encoder_setopt: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        encoder->color_option = SIXEL_COLOR_OPTION_MAPFILE;
        break;
    case SIXEL_OPTFLAG_MONOCHROME:  /* e */
        encoder->color_option = SIXEL_COLOR_OPTION_MONOCHROME;
        break;
    case SIXEL_OPTFLAG_HIGH_COLOR:  /* I */
        encoder->color_option = SIXEL_COLOR_OPTION_HIGHCOLOR;
        break;
    case SIXEL_OPTFLAG_BUILTIN_PALETTE:  /* b */
        if (strcmp(value, "xterm16") == 0) {
            encoder->builtin_palette = SIXEL_BUILTIN_XTERM16;
        } else if (strcmp(value, "xterm256") == 0) {
            encoder->builtin_palette = SIXEL_BUILTIN_XTERM256;
        } else if (strcmp(value, "vt340mono") == 0) {
            encoder->builtin_palette = SIXEL_BUILTIN_VT340_MONO;
        } else if (strcmp(value, "vt340color") == 0) {
            encoder->builtin_palette = SIXEL_BUILTIN_VT340_COLOR;
        } else if (strcmp(value, "gray1") == 0) {
            encoder->builtin_palette = SIXEL_BUILTIN_G1;
        } else if (strcmp(value, "gray2") == 0) {
            encoder->builtin_palette = SIXEL_BUILTIN_G2;
        } else if (strcmp(value, "gray4") == 0) {
            encoder->builtin_palette = SIXEL_BUILTIN_G4;
        } else if (strcmp(value, "gray8") == 0) {
            encoder->builtin_palette = SIXEL_BUILTIN_G8;
        } else {
            sixel_helper_set_additional_message(
                    "cannot parse builtin palette option.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        encoder->color_option = SIXEL_COLOR_OPTION_BUILTIN;
        break;
    case SIXEL_OPTFLAG_DIFFUSION:  /* d */
        /* parse --diffusion option */
        if (strcmp(value, "auto") == 0) {
            encoder->method_for_diffuse = SIXEL_DIFFUSE_AUTO;
        } else if (strcmp(value, "none") == 0) {
            encoder->method_for_diffuse = SIXEL_DIFFUSE_NONE;
        } else if (strcmp(value, "fs") == 0) {
            encoder->method_for_diffuse = SIXEL_DIFFUSE_FS;
        } else if (strcmp(value, "atkinson") == 0) {
            encoder->method_for_diffuse = SIXEL_DIFFUSE_ATKINSON;
        } else if (strcmp(value, "jajuni") == 0) {
            encoder->method_for_diffuse = SIXEL_DIFFUSE_JAJUNI;
        } else if (strcmp(value, "stucki") == 0) {
            encoder->method_for_diffuse = SIXEL_DIFFUSE_STUCKI;
        } else if (strcmp(value, "burkes") == 0) {
            encoder->method_for_diffuse = SIXEL_DIFFUSE_BURKES;
        } else if (strcmp(value, "a_dither") == 0) {
            encoder->method_for_diffuse = SIXEL_DIFFUSE_A_DITHER;
        } else if (strcmp(value, "x_dither") == 0) {
            encoder->method_for_diffuse = SIXEL_DIFFUSE_X_DITHER;
        } else if (strcmp(value, "lso1") == 0) {
            encoder->method_for_diffuse = SIXEL_DIFFUSE_LSO1;
        } else if (strcmp(value, "lso2") == 0) {
            encoder->method_for_diffuse = SIXEL_DIFFUSE_LSO2;
        } else if (strcmp(value, "lso3") == 0) {
            encoder->method_for_diffuse = SIXEL_DIFFUSE_LSO3;
        } else {
            sixel_helper_set_additional_message(
                "specified diffusion method is not supported.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_DIFFUSION_SCAN:  /* y */
        if (strcmp(value, "auto") == 0) {
            encoder->method_for_scan = SIXEL_SCAN_AUTO;
        } else if (strcmp(value, "serpentine") == 0) {
            encoder->method_for_scan = SIXEL_SCAN_SERPENTINE;
        } else if (strcmp(value, "raster") == 0) {
            encoder->method_for_scan = SIXEL_SCAN_RASTER;
        } else {
            sixel_helper_set_additional_message(
                "specified diffusion scan is not supported.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_DIFFUSION_CARRY:  /* Y */
        if (strcmp(value, "auto") == 0) {
            encoder->method_for_carry = SIXEL_CARRY_AUTO;
        } else if (strcmp(value, "direct") == 0) {
            encoder->method_for_carry = SIXEL_CARRY_DISABLE;
        } else if (strcmp(value, "carry") == 0) {
            encoder->method_for_carry = SIXEL_CARRY_ENABLE;
        } else {
            sixel_helper_set_additional_message(
                "specified diffusion carry mode is not supported.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_FIND_LARGEST:  /* f */
        /* parse --find-largest option */
        if (value) {
            if (strcmp(value, "auto") == 0) {
                encoder->method_for_largest = SIXEL_LARGE_AUTO;
            } else if (strcmp(value, "norm") == 0) {
                encoder->method_for_largest = SIXEL_LARGE_NORM;
            } else if (strcmp(value, "lum") == 0) {
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
        if (strcmp(value, "auto") == 0) {
            encoder->method_for_rep = SIXEL_REP_AUTO;
        } else if (strcmp(value, "center") == 0) {
            encoder->method_for_rep = SIXEL_REP_CENTER_BOX;
        } else if (strcmp(value, "average") == 0) {
            encoder->method_for_rep = SIXEL_REP_AVERAGE_COLORS;
        } else if ((strcmp(value, "histogram") == 0) ||
                   (strcmp(value, "histgram") == 0)) {
            encoder->method_for_rep = SIXEL_REP_AVERAGE_PIXELS;
        } else {
            sixel_helper_set_additional_message(
                "specified finding method is not supported.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_CROP:  /* c */
#if HAVE_SSCANF_S
        number = sscanf_s(value, "%dx%d+%d+%d",
                          &encoder->clipwidth, &encoder->clipheight,
                          &encoder->clipx, &encoder->clipy);
#else
        number = sscanf(value, "%dx%d+%d+%d",
                        &encoder->clipwidth, &encoder->clipheight,
                        &encoder->clipx, &encoder->clipy);
#endif  /* HAVE_SSCANF_S */
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
#if HAVE_SSCANF_S
        parsed = sscanf_s(value, "%d%2s", &number, unit, sizeof(unit) - 1);
#else
        parsed = sscanf(value, "%d%2s", &number, unit);
#endif  /* HAVE_SSCANF_S */
        if (parsed == 2 && strcmp(unit, "%") == 0) {
            encoder->pixelwidth = (-1);
            encoder->percentwidth = number;
        } else if (parsed == 1 || (parsed == 2 && strcmp(unit, "px") == 0)) {
            encoder->pixelwidth = number;
            encoder->percentwidth = (-1);
        } else if (strcmp(value, "auto") == 0) {
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
#if HAVE_SSCANF_S
        parsed = sscanf_s(value, "%d%2s", &number, unit, sizeof(unit) - 1);
#else
        parsed = sscanf(value, "%d%2s", &number, unit);
#endif  /* HAVE_SSCANF_S */
        if (parsed == 2 && strcmp(unit, "%") == 0) {
            encoder->pixelheight = (-1);
            encoder->percentheight = number;
        } else if (parsed == 1 || (parsed == 2 && strcmp(unit, "px") == 0)) {
            encoder->pixelheight = number;
            encoder->percentheight = (-1);
        } else if (strcmp(value, "auto") == 0) {
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
        if (strcmp(value, "nearest") == 0) {
            encoder->method_for_resampling = SIXEL_RES_NEAREST;
        } else if (strcmp(value, "gaussian") == 0) {
            encoder->method_for_resampling = SIXEL_RES_GAUSSIAN;
        } else if (strcmp(value, "hanning") == 0) {
            encoder->method_for_resampling = SIXEL_RES_HANNING;
        } else if (strcmp(value, "hamming") == 0) {
            encoder->method_for_resampling = SIXEL_RES_HAMMING;
        } else if (strcmp(value, "bilinear") == 0) {
            encoder->method_for_resampling = SIXEL_RES_BILINEAR;
        } else if (strcmp(value, "welsh") == 0) {
            encoder->method_for_resampling = SIXEL_RES_WELSH;
        } else if (strcmp(value, "bicubic") == 0) {
            encoder->method_for_resampling = SIXEL_RES_BICUBIC;
        } else if (strcmp(value, "lanczos2") == 0) {
            encoder->method_for_resampling = SIXEL_RES_LANCZOS2;
        } else if (strcmp(value, "lanczos3") == 0) {
            encoder->method_for_resampling = SIXEL_RES_LANCZOS3;
        } else if (strcmp(value, "lanczos4") == 0) {
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
        if (strcmp(value, "auto") == 0) {
            encoder->quality_mode = SIXEL_QUALITY_AUTO;
        } else if (strcmp(value, "high") == 0) {
            encoder->quality_mode = SIXEL_QUALITY_HIGH;
        } else if (strcmp(value, "low") == 0) {
            encoder->quality_mode = SIXEL_QUALITY_LOW;
        } else if (strcmp(value, "full") == 0) {
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
        if (strcmp(value, "auto") == 0) {
            encoder->loop_mode = SIXEL_LOOP_AUTO;
        } else if (strcmp(value, "force") == 0) {
            encoder->loop_mode = SIXEL_LOOP_FORCE;
        } else if (strcmp(value, "disable") == 0) {
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
        if (strcmp(value, "auto") == 0) {
            encoder->palette_type = SIXEL_PALETTETYPE_AUTO;
        } else if (strcmp(value, "hls") == 0) {
            encoder->palette_type = SIXEL_PALETTETYPE_HLS;
        } else if (strcmp(value, "rgb") == 0) {
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
            encoder->bgcolor = NULL;
        }
        status = sixel_parse_x_colorspec(&encoder->bgcolor,
                                         value,
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
        encoder->macro_number = atoi(value);
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
        sixel_helper_set_loader_trace(1);
        break;
    case SIXEL_OPTFLAG_STATIC:  /* S */
        encoder->fstatic = 1;
        break;
    case SIXEL_OPTFLAG_DRCS:  /* @ */
        encoder->fdrcs = 1;
        if (strlen(value) == 1 && value[0] >= 32) {
            encoder->start_dscs = value[0];
        } else {
            sixel_helper_set_additional_message(
                "cannot parse DSCS option.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_MAPPING_VERSION:  /* M */
        encoder->drcs_mmv = atoi(value);
        if (encoder->drcs_mmv < 0 || encoder->drcs_mmv >= 3) {
            sixel_helper_set_additional_message(
                "unknown DRCS unicode mapping version.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_PENETRATE:  /* P */
        encoder->penetrate_multiplexer = 1;
        break;
    case SIXEL_OPTFLAG_ENCODE_POLICY:  /* E */
        if (strcmp(value, "auto") == 0) {
            encoder->encode_policy = SIXEL_ENCODEPOLICY_AUTO;
        } else if (strcmp(value, "fast") == 0) {
            encoder->encode_policy = SIXEL_ENCODEPOLICY_FAST;
        } else if (strcmp(value, "size") == 0) {
            encoder->encode_policy = SIXEL_ENCODEPOLICY_SIZE;
        } else {
            sixel_helper_set_additional_message(
                "cannot parse encode policy option.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_WORKING_COLORSPACE:  /* W */
        if (value == NULL) {
            sixel_helper_set_additional_message(
                "working-colorspace requires an argument.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        } else {
            len = strlen(value);

            if (len >= sizeof(lowered)) {
                sixel_helper_set_additional_message(
                    "specified working colorspace name is too long.");
                status = SIXEL_BAD_ARGUMENT;
                goto end;
            }
            for (i = 0; i < len; ++i) {
                lowered[i] = (char)tolower((unsigned char)value[i]);
            }
            lowered[len] = '\0';

            if (strcmp(lowered, "gamma") == 0) {
                encoder->working_colorspace = SIXEL_COLORSPACE_GAMMA;
            } else if (strcmp(lowered, "linear") == 0) {
                encoder->working_colorspace = SIXEL_COLORSPACE_LINEAR;
            } else if (strcmp(lowered, "oklab") == 0) {
                encoder->working_colorspace = SIXEL_COLORSPACE_OKLAB;
            } else {
                sixel_helper_set_additional_message(
                    "unsupported working colorspace specified.");
                status = SIXEL_BAD_ARGUMENT;
                goto end;
            }
        }
        break;
    case SIXEL_OPTFLAG_OUTPUT_COLORSPACE:  /* U */
        if (value == NULL) {
            sixel_helper_set_additional_message(
                "output-colorspace requires an argument.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        } else {
            len = strlen(value);

            if (len >= sizeof(lowered)) {
                sixel_helper_set_additional_message(
                    "specified output colorspace name is too long.");
                status = SIXEL_BAD_ARGUMENT;
                goto end;
            }
            for (i = 0; i < len; ++i) {
                lowered[i] = (char)tolower((unsigned char)value[i]);
            }
            lowered[len] = '\0';

            if (strcmp(lowered, "gamma") == 0) {
                encoder->output_colorspace = SIXEL_COLORSPACE_GAMMA;
            } else if (strcmp(lowered, "linear") == 0) {
                encoder->output_colorspace = SIXEL_COLORSPACE_LINEAR;
            } else if (strcmp(lowered, "smpte-c") == 0 ||
                       strcmp(lowered, "smptec") == 0) {
                encoder->output_colorspace = SIXEL_COLORSPACE_SMPTEC;
            } else {
                sixel_helper_set_additional_message(
                    "unsupported output colorspace specified.");
                status = SIXEL_BAD_ARGUMENT;
                goto end;
            }
        }
        break;
    case SIXEL_OPTFLAG_ORMODE:  /* O */
        encoder->ormode = 1;
        break;
    case SIXEL_OPTFLAG_COMPLEXION_SCORE:  /* C */
        encoder->complexion = atoi(value);
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
            "unknown option is specified.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    /* detects arguments conflictions */
    if (encoder->reqcolors != (-1)) {
        switch (encoder->color_option) {
        case SIXEL_COLOR_OPTION_MAPFILE:
            sixel_helper_set_additional_message(
                "option -p, --colors conflicts with -m, --mapfile.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        case SIXEL_COLOR_OPTION_MONOCHROME:
            sixel_helper_set_additional_message(
                "option -e, --monochrome conflicts with -p, --colors.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        case SIXEL_COLOR_OPTION_HIGHCOLOR:
            sixel_helper_set_additional_message(
                "option -p, --colors conflicts with -I, --high-color.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        case SIXEL_COLOR_OPTION_BUILTIN:
            sixel_helper_set_additional_message(
                "option -p, --colors conflicts with -b, --builtin-palette.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        default:
            break;
        }
    }

    /* 8bit output option(-8) conflicts width GNU Screen integration(-P) */
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


/* called when image loader component load a image frame */
static SIXELSTATUS
load_image_callback(sixel_frame_t *frame, void *data)
{
    return sixel_encoder_encode_frame((sixel_encoder_t *)data, frame, NULL);
}


/* load source data from specified file and encode it to SIXEL format
 * output to encoder->outfd */
SIXELAPI SIXELSTATUS
sixel_encoder_encode(
    sixel_encoder_t *encoder,   /* encoder object */
    char const      *filename)  /* input filename */
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

    /* if required color is not set, set the max value */
    if (encoder->reqcolors == (-1)) {
        encoder->reqcolors = SIXEL_PALETTE_MAX;
    }

    /* if required color is less then 2, set the min value */
    if (encoder->reqcolors < 2) {
        encoder->reqcolors = SIXEL_PALETTE_MIN;
    }

    /* if color space option is not set, choose RGB color space */
    if (encoder->palette_type == SIXEL_PALETTETYPE_AUTO) {
        encoder->palette_type = SIXEL_PALETTETYPE_RGB;
    }

    /* if color option is not default value, prohibit to read
       the file as a paletted image */
    if (encoder->color_option != SIXEL_COLOR_OPTION_DEFAULT) {
        fuse_palette = 0;
    }

    /* if scaling options are set, prohibit to read the file as
       a paletted image */
    if (encoder->percentwidth > 0 ||
        encoder->percentheight > 0 ||
        encoder->pixelwidth > 0 ||
        encoder->pixelheight > 0) {
        fuse_palette = 0;
    }

reload:
    sixel_helper_set_loader_trace(encoder->verbose);
    sixel_helper_set_thumbnail_size_hint(
        sixel_encoder_thumbnail_hint(encoder));
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
            status = sixel_tty_wait_stdin(1000000);
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

    /* the status may not be SIXEL_OK */

end:
    sixel_encoder_unref(encoder);

    return status;
}


/* encode specified pixel data to SIXEL format
 * output to encoder->outfd */
SIXELAPI SIXELSTATUS
sixel_encoder_encode_bytes(
    sixel_encoder_t     /* in */    *encoder,
    unsigned char       /* in */    *bytes,
    int                 /* in */    width,
    int                 /* in */    height,
    int                 /* in */    pixelformat,
    unsigned char       /* in */    *palette,
    int                 /* in */    ncolors)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_frame_t *frame = NULL;

    if (encoder == NULL || bytes == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    status = sixel_frame_new(&frame, encoder->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_frame_init(frame, bytes, width, height,
                              pixelformat, palette, ncolors);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_encoder_encode_frame(encoder, frame, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = SIXEL_OK;

end:
    /* we need to free the frame before exiting, but we can't use the
       sixel_frame_destroy function, because that will also attempt to
       free the pixels and palette, which we don't own */
    if (frame != NULL && encoder->allocator != NULL) {
        sixel_allocator_free(encoder->allocator, frame);
        sixel_allocator_unref(encoder->allocator);
    }
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
    int height = 0;
    int is_animation = 0;

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
    status = sixel_frame_init(frame, buffer, 1, 1,
                              SIXEL_PIXELFORMAT_RGB888,
                              NULL, 0);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    if (sixel_frame_get_loop_no(frame) != 0 || sixel_frame_get_frame_no(frame) != 0) {
        is_animation = 1;
    }

    height = sixel_frame_get_height(frame);

    status = sixel_tty_scroll(sixel_write_callback, encoder->outfd, height, is_animation);
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

    result = sixel_tty_wait_stdin(1000);
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
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    encoder = sixel_encoder_create();
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
# pragma GCC diagnostic pop
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


SIXELAPI int
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


/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 : */
/* EOF */
