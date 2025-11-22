/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 * Copyright (c) 2014-2020 Hayaki Saito
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
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#if HAVE_MATH_H
# include <math.h>
#endif  /* HAVE_MATH_H */
#if HAVE_LIMITS_H
# include <limits.h>
#endif  /* HAVE_LIMITS_H */
#if HAVE_INTTYPES_H
# include <inttypes.h>
#endif  /* HAVE_INTTYPES_H */

#include "frame.h"
#include "pixelformat.h"
#include "compat_stub.h"

#if !defined(HAVE_MEMMOVE)
# define memmove(d, s, n) (bcopy ((s), (d), (n)))
#endif

static SIXELSTATUS
sixel_frame_convert_to_rgb888(sixel_frame_t /*in */ *frame);
static SIXELSTATUS
sixel_frame_promote_to_rgbfloat32(sixel_frame_t *frame);
static int
sixel_frame_colorspace_from_pixelformat(int pixelformat);
static void
sixel_frame_apply_pixelformat(sixel_frame_t *frame, int pixelformat);

/* constructor of frame object */
SIXELAPI SIXELSTATUS
sixel_frame_new(
    sixel_frame_t       /* out */ **ppframe,    /* frame object to be created */
    sixel_allocator_t   /* in */  *allocator)   /* allocator, null if you use
                                                   default allocator */
{
    SIXELSTATUS status = SIXEL_FALSE;

    if (allocator == NULL) {
        status = sixel_allocator_new(&allocator, malloc, calloc, realloc, free);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    *ppframe = (sixel_frame_t *)sixel_allocator_malloc(allocator, sizeof(sixel_frame_t));
    if (*ppframe == NULL) {
        sixel_helper_set_additional_message(
            "sixel_frame_resize: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    (*ppframe)->ref = 1;
    (*ppframe)->pixels = NULL;
    (*ppframe)->palette = NULL;
    (*ppframe)->width = 0;
    (*ppframe)->height = 0;
    (*ppframe)->ncolors = (-1);
    (*ppframe)->pixelformat = SIXEL_PIXELFORMAT_RGB888;
    (*ppframe)->delay = 0;
    (*ppframe)->frame_no = 0;
    (*ppframe)->loop_count = 0;
    (*ppframe)->multiframe = 0;
    (*ppframe)->transparent = (-1);
    (*ppframe)->allocator = allocator;

    sixel_allocator_ref(allocator);

    /* Normalize between byte and float pipelines when buffers are present. */
    status = SIXEL_OK;

end:
    return status;
}


SIXELAPI /* deprecated */ sixel_frame_t *
sixel_frame_create(void)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_frame_t *frame = NULL;

    status = sixel_frame_new(&frame, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

end:
    return frame;
}


static void
sixel_frame_destroy(sixel_frame_t /* in */ *frame)
{
    sixel_allocator_t *allocator = NULL;

    if (frame) {
        allocator = frame->allocator;
        sixel_allocator_free(allocator, frame->pixels);
        sixel_allocator_free(allocator, frame->palette);
        sixel_allocator_free(allocator, frame);
        sixel_allocator_unref(allocator);
    }
}


/* increase reference count of frame object (thread-unsafe) */
SIXELAPI void
sixel_frame_ref(sixel_frame_t *frame)
{
    /* TODO: be thread safe */
    ++frame->ref;
}


/* decrease reference count of frame object (thread-unsafe) */
SIXELAPI void
sixel_frame_unref(sixel_frame_t *frame)
{
    /* TODO: be thread safe */
    if (frame != NULL && --frame->ref == 0) {
        sixel_frame_destroy(frame);
    }
}


/* initialize frame object with a pixel buffer */
SIXELAPI SIXELSTATUS
sixel_frame_init(
    sixel_frame_t   /* in */ *frame,        /* frame object to be initialize */
    unsigned char   /* in */ *pixels,       /* pixel buffer */
    int             /* in */ width,         /* pixel width of buffer */
    int             /* in */ height,        /* pixel height of buffer */
    int             /* in */ pixelformat,   /* pixelformat of buffer */
    unsigned char   /* in */ *palette,      /* palette for buffer or NULL */
    int             /* in */ ncolors        /* number of palette colors or (-1) */
)
{
    SIXELSTATUS status = SIXEL_FALSE;

    sixel_frame_ref(frame);

    /* check parameters */
    if (width <= 0) {
        sixel_helper_set_additional_message(
            "sixel_frame_init: an invalid width parameter detected.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (height <= 0) {
        sixel_helper_set_additional_message(
            "sixel_frame_init: an invalid width parameter detected.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (width > SIXEL_WIDTH_LIMIT) {
        sixel_helper_set_additional_message(
            "sixel_frame_init: given width parameter is too huge.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (height > SIXEL_HEIGHT_LIMIT) {
        sixel_helper_set_additional_message(
            "sixel_frame_init: given height parameter is too huge.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    frame->pixels = pixels;
    frame->width = width;
    frame->height = height;
    sixel_frame_apply_pixelformat(frame, pixelformat);
    frame->palette = palette;
    frame->ncolors = ncolors;
    status = SIXEL_OK;

end:
    sixel_frame_unref(frame);

    return status;
}


/* get pixels */
SIXELAPI unsigned char *
sixel_frame_get_pixels(sixel_frame_t /* in */ *frame)  /* frame object */
{
    return frame->pixels;
}


/* set pixels */
SIXELAPI void
sixel_frame_set_pixels(
    sixel_frame_t  /* in */ *frame,
    unsigned char  /* in */ *pixels)
{
    frame->pixels = pixels;
}


/* get palette */
SIXELAPI unsigned char *
sixel_frame_get_palette(sixel_frame_t /* in */ *frame)  /* frame object */
{
    return frame->palette;
}


/* set palette */
SIXELAPI void
sixel_frame_set_palette(
    sixel_frame_t  /* in */ *frame,
    unsigned char  /* in */ *palette)
{
    frame->palette = palette;
}


/* get width */
SIXELAPI int
sixel_frame_get_width(sixel_frame_t /* in */ *frame)  /* frame object */
{
    return frame->width;
}


/* set width */
SIXELAPI void
sixel_frame_set_width(sixel_frame_t /* in */ *frame, int /* in */ width)
{
    frame->width = width;
}


/* get height */
SIXELAPI int
sixel_frame_get_height(sixel_frame_t /* in */ *frame)  /* frame object */
{
    return frame->height;
}


/* set height */
SIXELAPI void
sixel_frame_set_height(sixel_frame_t /* in */ *frame, int /* in */ height)
{
    frame->height = height;
}


/* get ncolors */
SIXELAPI int
sixel_frame_get_ncolors(sixel_frame_t /* in */ *frame)  /* frame object */
{
    return frame->ncolors;
}


/* set ncolors */
SIXELAPI void
sixel_frame_set_ncolors(
    sixel_frame_t  /* in */ *frame,
    int            /* in */ ncolors)
{
    frame->ncolors = ncolors;
}


/* get pixelformat */
SIXELAPI int
sixel_frame_get_pixelformat(sixel_frame_t /* in */ *frame)  /* frame object */
{
    return frame->pixelformat;
}


/* set pixelformat */
SIXELAPI SIXELSTATUS
sixel_frame_set_pixelformat(
    sixel_frame_t  /* in */ *frame,
    int            /* in */ pixelformat)
{
    SIXELSTATUS status;
    int source_colorspace;
    int target_colorspace;
    int working_pixelformat;
    int depth;
    size_t pixel_total;
    size_t pixel_size;

    if (frame == NULL) {
        sixel_helper_set_additional_message(
            "sixel_frame_set_pixelformat: frame is null.");
        return SIXEL_BAD_ARGUMENT;
    }
    if (pixelformat == frame->pixelformat) {
        return SIXEL_OK;
    }
    if (frame->pixels == NULL) {
        sixel_frame_apply_pixelformat(frame, pixelformat);
        return SIXEL_OK;
    }

    status = SIXEL_OK;
    working_pixelformat = frame->pixelformat;
    source_colorspace = frame->colorspace;

    /*
     * Palette and byte-form buffers need to be normalized before any
     * colorspace adjustments so that channel ordering matches the
     * converter's expectations.
     */
    if (pixelformat == SIXEL_PIXELFORMAT_RGBFLOAT32
            || pixelformat == SIXEL_PIXELFORMAT_LINEARRGBFLOAT32
            || pixelformat == SIXEL_PIXELFORMAT_OKLABFLOAT32) {
        if (working_pixelformat & SIXEL_FORMATTYPE_PALETTE) {
            status = sixel_frame_convert_to_rgb888(frame);
        }
        if (SIXEL_SUCCEEDED(status)
                && !SIXEL_PIXELFORMAT_IS_FLOAT32(frame->pixelformat)) {
            status = sixel_frame_promote_to_rgbfloat32(frame);
        }
    } else if (pixelformat == SIXEL_PIXELFORMAT_RGB888
            && working_pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        status = sixel_frame_convert_to_rgb888(frame);
    } else if (!SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat)
            && !SIXEL_PIXELFORMAT_IS_FLOAT32(working_pixelformat)
            && (working_pixelformat & SIXEL_FORMATTYPE_PALETTE)) {
        status = sixel_frame_convert_to_rgb888(frame);
    }

    if (SIXEL_FAILED(status)) {
        return status;
    }

    working_pixelformat = frame->pixelformat;
    source_colorspace = frame->colorspace;
    target_colorspace = sixel_frame_colorspace_from_pixelformat(pixelformat);

    if (target_colorspace != source_colorspace) {
        /*
         * Convert in-place so callers can request alternate transfer
         * curves or OKLab buffers without mutating the frame twice.
         */
        if (frame->width <= 0 || frame->height <= 0) {
            sixel_helper_set_additional_message(
                "sixel_frame_set_pixelformat: invalid frame size.");
            return SIXEL_BAD_INPUT;
        }

        pixel_total = (size_t)frame->width * (size_t)frame->height;
        if (pixel_total / (size_t)frame->width != (size_t)frame->height) {
            sixel_helper_set_additional_message(
                "sixel_frame_set_pixelformat: buffer overflow risk.");
            return SIXEL_BAD_INPUT;
        }

        depth = sixel_helper_compute_depth(working_pixelformat);
        if (depth <= 0) {
            sixel_helper_set_additional_message(
                "sixel_frame_set_pixelformat: invalid pixelformat depth.");
            return SIXEL_BAD_INPUT;
        }
        if (pixel_total > SIZE_MAX / (size_t)depth) {
            sixel_helper_set_additional_message(
                "sixel_frame_set_pixelformat: buffer size overflow.");
            return SIXEL_BAD_INPUT;
        }
        pixel_size = pixel_total * (size_t)depth;

        status = sixel_helper_convert_colorspace(frame->pixels,
                                                 pixel_size,
                                                 working_pixelformat,
                                                 source_colorspace,
                                                 target_colorspace);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }

    sixel_frame_apply_pixelformat(frame, pixelformat);
    return SIXEL_OK;
}


SIXELAPI int
sixel_frame_get_colorspace(sixel_frame_t /* in */ *frame)  /* frame object */
{
    return frame->colorspace;
}


/* set colorspace */
SIXELAPI void
sixel_frame_set_colorspace(
    sixel_frame_t  /* in */ *frame,
    int            /* in */ colorspace)
{
    frame->colorspace = colorspace;
}


/* get transparent */
SIXELAPI int
sixel_frame_get_transparent(sixel_frame_t /* in */ *frame)  /* frame object */
{
    return frame->transparent;
}


/* set transparent */
SIXELAPI void
sixel_frame_set_transparent(
    sixel_frame_t  /* in */ *frame,
    int            /* in */ transparent)
{
    frame->transparent = transparent;
}


/* get transparent */
SIXELAPI int
sixel_frame_get_multiframe(sixel_frame_t /* in */ *frame)  /* frame object */
{
    return frame->multiframe;
}


/* set multiframe */
SIXELAPI void
sixel_frame_set_multiframe(
    sixel_frame_t  /* in */ *frame,
    int            /* in */ multiframe)
{
    frame->multiframe = multiframe;
}


/* get delay */
SIXELAPI int
sixel_frame_get_delay(sixel_frame_t /* in */ *frame)  /* frame object */
{
    return frame->delay;
}


/* set delay */
SIXELAPI void
sixel_frame_set_delay(sixel_frame_t /* in */ *frame, int /* in */ delay)
{
    frame->delay = delay;
}


/* get frame no */
SIXELAPI int
sixel_frame_get_frame_no(sixel_frame_t /* in */ *frame)  /* frame object */
{
    return frame->frame_no;
}


/* set frame index */
SIXELAPI void
sixel_frame_set_frame_no(
    sixel_frame_t  /* in */ *frame,
    int            /* in */ frame_no)
{
    frame->frame_no = frame_no;
}


/* increment frame index */
SIXELAPI void
sixel_frame_increment_frame_no(sixel_frame_t /* in */ *frame)
{
    ++frame->frame_no;
}


/* reset frame index */
SIXELAPI void
sixel_frame_reset_frame_no(sixel_frame_t /* in */ *frame)
{
    frame->frame_no = 0;
}


/* get loop no */
SIXELAPI int
sixel_frame_get_loop_no(sixel_frame_t /* in */ *frame)  /* frame object */
{
    return frame->loop_count;
}


/* set loop count */
SIXELAPI void
sixel_frame_set_loop_count(
    sixel_frame_t  /* in */ *frame,
    int            /* in */ loop_count)
{
    frame->loop_count = loop_count;
}


/* increment loop count */
SIXELAPI void
sixel_frame_increment_loop_count(sixel_frame_t /* in */ *frame)
{
    ++frame->loop_count;
}


/* get allocator */
SIXELAPI sixel_allocator_t *
sixel_frame_get_allocator(sixel_frame_t /* in */ *frame)
{
    return frame->allocator;
}

/* strip alpha from RGBA/ARGB/BGRA/ABGR formatted pixbuf */
SIXELAPI SIXELSTATUS
sixel_frame_strip_alpha(
    sixel_frame_t  /* in */ *frame,
    unsigned char  /* in */ *bgcolor
)
{
    SIXELSTATUS status = SIXEL_FALSE;
    int i;
    unsigned char *src;
    unsigned char *dst;
    unsigned char alpha;

    sixel_frame_ref(frame);

    src = dst = frame->pixels;

    if (bgcolor) {
        switch (frame->pixelformat) {
        case SIXEL_PIXELFORMAT_ARGB8888:
            for (i = 0; i < frame->height * frame->width; i++) {
                alpha = src[0];
                *dst++ = (*src++ * alpha + bgcolor[0] * (0xff - alpha)) >> 8;
                *dst++ = (*src++ * alpha + bgcolor[1] * (0xff - alpha)) >> 8;
                *dst++ = (*src++ * alpha + bgcolor[2] * (0xff - alpha)) >> 8;
                src++;
            }
            sixel_frame_apply_pixelformat(
                frame,
                SIXEL_PIXELFORMAT_RGB888);
            break;
        case SIXEL_PIXELFORMAT_RGBA8888:
            for (i = 0; i < frame->height * frame->width; i++) {
                alpha = src[3];
                *dst++ = (*src++ * alpha + bgcolor[0] * (0xff - alpha)) >> 8;
                *dst++ = (*src++ * alpha + bgcolor[1] * (0xff - alpha)) >> 8;
                *dst++ = (*src++ * alpha + bgcolor[2] * (0xff - alpha)) >> 8;
                src++;
            }
            sixel_frame_apply_pixelformat(
                frame,
                SIXEL_PIXELFORMAT_RGB888);
            break;
        case SIXEL_PIXELFORMAT_ABGR8888:
            for (i = 0; i < frame->height * frame->width; i++) {
                alpha = src[0];
                *dst++ = (src[3] * alpha + bgcolor[0] * (0xff - alpha)) >> 8;
                *dst++ = (src[2] * alpha + bgcolor[1] * (0xff - alpha)) >> 8;
                *dst++ = (src[1] * alpha + bgcolor[2] * (0xff - alpha)) >> 8;
                src += 4;
            }
            sixel_frame_apply_pixelformat(
                frame,
                SIXEL_PIXELFORMAT_RGB888);
            break;
        case SIXEL_PIXELFORMAT_BGRA8888:
            for (i = 0; i < frame->height * frame->width; i++) {
                alpha = src[3];
                *dst++ = (src[2] * alpha + bgcolor[0] * (0xff - alpha)) >> 8;
                *dst++ = (src[1] * alpha + bgcolor[1] * (0xff - alpha)) >> 8;
                *dst++ = (src[0] * alpha + bgcolor[2] * (0xff - alpha)) >> 8;
                src += 4;
            }
            sixel_frame_apply_pixelformat(
                frame,
                SIXEL_PIXELFORMAT_RGB888);
            break;
        default:
            break;
        }
    } else {
        switch (frame->pixelformat) {
        case SIXEL_PIXELFORMAT_ARGB8888:
            for (i = 0; i < frame->height * frame->width; i++) {
                src++;            /* A */
                *dst++ = *src++;  /* R */
                *dst++ = *src++;  /* G */
                *dst++ = *src++;  /* B */
            }
            sixel_frame_apply_pixelformat(
                frame,
                SIXEL_PIXELFORMAT_RGB888);
            break;
        case SIXEL_PIXELFORMAT_RGBA8888:
            for (i = 0; i < frame->height * frame->width; i++) {
                *dst++ = *src++;  /* R */
                *dst++ = *src++;  /* G */
                *dst++ = *src++;  /* B */
                src++;            /* A */
            }
            sixel_frame_apply_pixelformat(
                frame,
                SIXEL_PIXELFORMAT_RGB888);
            break;
        case SIXEL_PIXELFORMAT_ABGR8888:
            for (i = 0; i < frame->height * frame->width; i++) {
                *dst++ = src[3];  /* R */
                *dst++ = src[2];  /* G */
                *dst++ = src[1];  /* B */
                src += 4;
            }
            sixel_frame_apply_pixelformat(
                frame,
                SIXEL_PIXELFORMAT_RGB888);
            break;
        case SIXEL_PIXELFORMAT_BGRA8888:
            for (i = 0; i < frame->height * frame->width; i++) {
                *dst++ = src[2];  /* R */
                *dst++ = src[1];  /* G */
                *dst++ = src[0];  /* B */
                src += 4;
            }
            sixel_frame_apply_pixelformat(
                frame,
                SIXEL_PIXELFORMAT_RGB888);
            break;
        default:
            break;
        }
    }

    status = SIXEL_OK;

    sixel_frame_unref(frame);

    return status;
}


static SIXELSTATUS
sixel_frame_convert_to_rgb888(sixel_frame_t /*in */ *frame)
{
    SIXELSTATUS status = SIXEL_FALSE;
    unsigned char *normalized_pixels = NULL;
    size_t size;
    unsigned char *dst;
    unsigned char *src;
    unsigned char *p;

    sixel_frame_ref(frame);

    switch (frame->pixelformat) {
    case SIXEL_PIXELFORMAT_PAL1:
    case SIXEL_PIXELFORMAT_PAL2:
    case SIXEL_PIXELFORMAT_PAL4:
        size = (size_t)(frame->width * frame->height * 4);
        normalized_pixels = (unsigned char *)sixel_allocator_malloc(frame->allocator, size);
        if (normalized_pixels == NULL) {
            sixel_helper_set_additional_message(
                "sixel_frame_convert_to_rgb888: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        src = normalized_pixels + frame->width * frame->height * 3;
        dst = normalized_pixels;
        status = sixel_helper_normalize_pixelformat(src,
                                                    &frame->pixelformat,
                                                    frame->pixels,
                                                    frame->pixelformat,
                                                    frame->width,
                                                    frame->height);
        if (SIXEL_FAILED(status)) {
            sixel_allocator_free(frame->allocator, normalized_pixels);
            goto end;
        }
        for (p = src; dst < src; ++p) {
            *dst++ = *(frame->palette + *p * 3 + 0);
            *dst++ = *(frame->palette + *p * 3 + 1);
            *dst++ = *(frame->palette + *p * 3 + 2);
        }
        sixel_allocator_free(frame->allocator, frame->pixels);
        frame->pixels = normalized_pixels;
        sixel_frame_apply_pixelformat(
            frame,
            SIXEL_PIXELFORMAT_RGB888);
        break;
    case SIXEL_PIXELFORMAT_PAL8:
        size = (size_t)(frame->width * frame->height * 3);
        normalized_pixels = (unsigned char *)sixel_allocator_malloc(frame->allocator, size);
        if (normalized_pixels == NULL) {
            sixel_helper_set_additional_message(
                "sixel_frame_convert_to_rgb888: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        src = frame->pixels;
        dst = normalized_pixels;
        for (; dst != normalized_pixels + size; ++src) {
            *dst++ = frame->palette[*src * 3 + 0];
            *dst++ = frame->palette[*src * 3 + 1];
            *dst++ = frame->palette[*src * 3 + 2];
        }
        sixel_allocator_free(frame->allocator, frame->pixels);
        frame->pixels = normalized_pixels;
        sixel_frame_apply_pixelformat(
            frame,
            SIXEL_PIXELFORMAT_RGB888);
        break;
    case SIXEL_PIXELFORMAT_RGB888:
        break;
    case SIXEL_PIXELFORMAT_G8:
    case SIXEL_PIXELFORMAT_GA88:
    case SIXEL_PIXELFORMAT_AG88:
    case SIXEL_PIXELFORMAT_RGB555:
    case SIXEL_PIXELFORMAT_RGB565:
    case SIXEL_PIXELFORMAT_BGR555:
    case SIXEL_PIXELFORMAT_BGR565:
    case SIXEL_PIXELFORMAT_RGBA8888:
    case SIXEL_PIXELFORMAT_ARGB8888:
    case SIXEL_PIXELFORMAT_RGBFLOAT32:
    case SIXEL_PIXELFORMAT_LINEARRGBFLOAT32:
    case SIXEL_PIXELFORMAT_OKLABFLOAT32:
        /* normalize pixelformat */
        size = (size_t)(frame->width * frame->height * 3);
        normalized_pixels = (unsigned char *)sixel_allocator_malloc(frame->allocator, size);
        if (normalized_pixels == NULL) {
            sixel_helper_set_additional_message(
                "sixel_frame_convert_to_rgb888: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        status = sixel_helper_normalize_pixelformat(normalized_pixels,
                                                    &frame->pixelformat,
                                                    frame->pixels,
                                                    frame->pixelformat,
                                                    frame->width,
                                                    frame->height);
        if (SIXEL_FAILED(status)) {
            sixel_allocator_free(frame->allocator, normalized_pixels);
            goto end;
        }
        sixel_allocator_free(frame->allocator, frame->pixels);
        frame->pixels = normalized_pixels;
        break;
    default:
        status = SIXEL_LOGIC_ERROR;
        sixel_helper_set_additional_message(
            "sixel_frame_convert_to_rgb888: invalid pixelformat.");
        goto end;
    }

    status = SIXEL_OK;

end:
    sixel_frame_unref(frame);

    return status;
}

/*
 * Infer colorspace metadata from the pixelformat.  Float formats encode
 * their transfer characteristics directly, while byte-oriented formats
 * default to gamma encoded RGB.
 */
static int
sixel_frame_colorspace_from_pixelformat(int pixelformat)
{
    switch (pixelformat) {
    case SIXEL_PIXELFORMAT_LINEARRGBFLOAT32:
        return SIXEL_COLORSPACE_LINEAR;
    case SIXEL_PIXELFORMAT_OKLABFLOAT32:
        return SIXEL_COLORSPACE_OKLAB;
    default:
        return SIXEL_COLORSPACE_GAMMA;
    }
}

static void
sixel_frame_apply_pixelformat(sixel_frame_t *frame, int pixelformat)
{
    frame->pixelformat = pixelformat;
    frame->colorspace = sixel_frame_colorspace_from_pixelformat(pixelformat);
}

#if HAVE_DIAGNOSTIC_UNUSED_FUNCTION
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wunused-function"
#endif

/*
 * Select the float pixelformat that matches the frame's current colorspace
 * so downstream conversions interpret each channel correctly.  OKLab uses
 * a [-0.5, 0.5] range for a/b, while gamma/linear share the 0-1 interval.
 */
static int
sixel_frame_float_pixelformat_for_colorspace(int colorspace)
{
    switch (colorspace) {
    case SIXEL_COLORSPACE_LINEAR:
        return SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
    case SIXEL_COLORSPACE_OKLAB:
        return SIXEL_PIXELFORMAT_OKLABFLOAT32;
    default:
        return SIXEL_PIXELFORMAT_RGBFLOAT32;
    }
}

static SIXELSTATUS
sixel_frame_promote_to_rgbfloat32(sixel_frame_t *frame)
{
    float *float_pixels;
    unsigned char *byte_pixels;
    unsigned char const *pixel;
    size_t pixel_total;
    size_t bytes;
    size_t index;
    size_t base;
    int float_pixelformat;
    int step;
    int index_r;
    int index_g;
    int index_b;

    step = 0;
    index_r = 0;
    index_g = 0;
    index_b = 0;

    /*
     * Derive the byte stride and per-channel offsets instead of coercing the
     * frame to RGB888 first.  OKLab buffers keep their signed A/B buckets in
     * the order dictated by pixelformat, so preserving that layout avoids the
     * blue casts reported when we reinterpreted them as gamma RGB.
     */
    switch (frame->pixelformat) {
    case SIXEL_PIXELFORMAT_RGB888:
        step = 3;
        index_r = 0;
        index_g = 1;
        index_b = 2;
        break;
    case SIXEL_PIXELFORMAT_BGR888:
        step = 3;
        index_r = 2;
        index_g = 1;
        index_b = 0;
        break;
    case SIXEL_PIXELFORMAT_RGBA8888:
        step = 4;
        index_r = 0;
        index_g = 1;
        index_b = 2;
        break;
    case SIXEL_PIXELFORMAT_ARGB8888:
        step = 4;
        index_r = 1;
        index_g = 2;
        index_b = 3;
        break;
    case SIXEL_PIXELFORMAT_BGRA8888:
        step = 4;
        index_r = 2;
        index_g = 1;
        index_b = 0;
        break;
    case SIXEL_PIXELFORMAT_ABGR8888:
        step = 4;
        index_r = 3;
        index_g = 2;
        index_b = 1;
        break;
    case SIXEL_PIXELFORMAT_G8:
        step = 1;
        index_r = 0;
        index_g = 0;
        index_b = 0;
        break;
    case SIXEL_PIXELFORMAT_GA88:
        step = 2;
        index_r = 0;
        index_g = 0;
        index_b = 0;
        break;
    case SIXEL_PIXELFORMAT_AG88:
        step = 2;
        index_r = 1;
        index_g = 1;
        index_b = 1;
        break;
    default:
        sixel_helper_set_additional_message(
            "sixel_frame_promote_to_rgbfloat32: unsupported pixelformat.");
        return SIXEL_BAD_INPUT;
    }

    if ((size_t)frame->width > SIZE_MAX / (size_t)frame->height) {
        sixel_helper_set_additional_message(
            "sixel_frame_promote_to_rgbfloat32: overflow.");
        return SIXEL_BAD_INPUT;
    }

    pixel_total = (size_t)frame->width * (size_t)frame->height;
    if (pixel_total > SIZE_MAX / (3U * sizeof(float))) {
        sixel_helper_set_additional_message(
            "sixel_frame_promote_to_rgbfloat32: buffer too large.");
        return SIXEL_BAD_INPUT;
    }
    bytes = pixel_total * 3U * sizeof(float);
    float_pixels = (float *)sixel_allocator_malloc(frame->allocator, bytes);
    if (float_pixels == NULL) {
        sixel_helper_set_additional_message(
            "sixel_frame_promote_to_rgbfloat32: "
            "sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    byte_pixels = frame->pixels;
    float_pixelformat =
        sixel_frame_float_pixelformat_for_colorspace(frame->colorspace);

    for (index = 0U; index < pixel_total; ++index) {
        unsigned char r8;
        unsigned char g8;
        unsigned char b8;

        pixel = byte_pixels + index * (size_t)step;
        r8 = *(pixel + (size_t)index_r);
        g8 = *(pixel + (size_t)index_g);
        b8 = *(pixel + (size_t)index_b);

        base = index * 3U;
        float_pixels[base + 0U] =
            sixel_pixelformat_byte_to_float(float_pixelformat, 0, r8);
        float_pixels[base + 1U] =
            sixel_pixelformat_byte_to_float(float_pixelformat, 1, g8);
        float_pixels[base + 2U] =
            sixel_pixelformat_byte_to_float(float_pixelformat, 2, b8);
    }

    sixel_allocator_free(frame->allocator, frame->pixels);
    frame->pixels = (unsigned char *)float_pixels;
    sixel_frame_apply_pixelformat(frame, float_pixelformat);
    return SIXEL_OK;
}

#if HAVE_DIAGNOSTIC_UNUSED_FUNCTION
# pragma GCC diagnostic pop
#endif

/* resize a frame to given size with specified resampling filter */
SIXELAPI SIXELSTATUS
sixel_frame_resize(
    sixel_frame_t *frame,
    int width,
    int height,
    int method_for_resampling
)
{
    SIXELSTATUS status = SIXEL_FALSE;
    size_t size;
    unsigned char *scaled_frame = NULL;

    sixel_frame_ref(frame);

    /* check parameters */
    if (width <= 0) {
        sixel_helper_set_additional_message(
            "sixel_frame_resize: an invalid width parameter detected.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (height <= 0) {
        sixel_helper_set_additional_message(
            "sixel_frame_resize: an invalid width parameter detected.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (width > SIXEL_WIDTH_LIMIT) {
        sixel_helper_set_additional_message(
            "sixel_frame_resize: given width parameter is too huge.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (height > SIXEL_HEIGHT_LIMIT) {
        sixel_helper_set_additional_message(
            "sixel_frame_resize: given height parameter is too huge.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    if (width == frame->width && height == frame->height) {
        /* nothing to do */
        goto out;
    }

    status = sixel_frame_convert_to_rgb888(frame);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    size = (size_t)width * (size_t)height * 3UL;
    scaled_frame = (unsigned char *)sixel_allocator_malloc(frame->allocator, size);
    if (scaled_frame == NULL) {
        sixel_helper_set_additional_message(
            "sixel_frame_resize: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    status = sixel_helper_scale_image(
        scaled_frame,
        frame->pixels,
        frame->width,
        frame->height,
        3,
        width,
        height,
        method_for_resampling,
        frame->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    sixel_allocator_free(frame->allocator, frame->pixels);
    frame->pixels = scaled_frame;
    frame->width = width;
    frame->height = height;

out:
    status = SIXEL_OK;

end:
    sixel_frame_unref(frame);

    return status;
}


static SIXELSTATUS
clip(unsigned char *pixels,
     int sx,
     int sy,
     int pixelformat,
     int cx,
     int cy,
     int cw,
     int ch)
{
    SIXELSTATUS status = SIXEL_FALSE;
    int y;
    unsigned char *src;
    unsigned char *dst;
    int depth;
    char message[256];
    int nwrite;

    /* unused */ (void) sx;
    /* unused */ (void) sy;
    /* unused */ (void) cx;

    switch (pixelformat) {
    case SIXEL_PIXELFORMAT_PAL8:
    case SIXEL_PIXELFORMAT_G8:
    case SIXEL_PIXELFORMAT_RGB888:
    case SIXEL_PIXELFORMAT_RGBFLOAT32:
        depth = sixel_helper_compute_depth(pixelformat);
        if (depth < 0) {
            status = SIXEL_LOGIC_ERROR;
            /*
             * We funnel formatting through the compat helper so that MSVC
             * receives explicit bounds information.
             */
            nwrite = sixel_compat_snprintf(
                message,
                sizeof(message),
                "clip: sixel_helper_compute_depth(%08x) failed.",
                pixelformat);
            if (nwrite > 0) {
                sixel_helper_set_additional_message(message);
            }
            goto end;
        }

        dst = pixels;
        src = pixels + cy * sx * depth + cx * depth;
        for (y = 0; y < ch; y++) {
            memmove(dst, src, (size_t)(cw * depth));
            dst += (cw * depth);
            src += (sx * depth);
        }

        status = SIXEL_OK;

        break;
    default:
        status = SIXEL_BAD_ARGUMENT;
        nwrite = sixel_compat_snprintf(
            message,
            sizeof(message),
            "clip: invalid pixelformat(%08x) is specified.",
            pixelformat);
        if (nwrite > 0) {
            sixel_helper_set_additional_message(message);
        }
        break;
    }

end:
    return status;
}


/* clip frame */
SIXELAPI SIXELSTATUS
sixel_frame_clip(
    sixel_frame_t *frame,
    int x,
    int y,
    int width,
    int height
)
{
    SIXELSTATUS status = SIXEL_FALSE;
    unsigned char *normalized_pixels;

    sixel_frame_ref(frame);

    /* check parameters */
    if (width <= 0) {
        sixel_helper_set_additional_message(
            "sixel_frame_clip: an invalid width parameter detected.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (height <= 0) {
        sixel_helper_set_additional_message(
            "sixel_frame_clip: an invalid width parameter detected.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (width > SIXEL_WIDTH_LIMIT) {
        sixel_helper_set_additional_message(
            "sixel_frame_clip: given width parameter is too huge.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (height > SIXEL_HEIGHT_LIMIT) {
        sixel_helper_set_additional_message(
            "sixel_frame_clip: given height parameter is too huge.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    switch (frame->pixelformat) {
    case SIXEL_PIXELFORMAT_PAL1:
    case SIXEL_PIXELFORMAT_PAL2:
    case SIXEL_PIXELFORMAT_PAL4:
    case SIXEL_PIXELFORMAT_G1:
    case SIXEL_PIXELFORMAT_G2:
    case SIXEL_PIXELFORMAT_G4:
        normalized_pixels = (unsigned char *)sixel_allocator_malloc(frame->allocator,
                                                                    (size_t)(frame->width * frame->height));
        status = sixel_helper_normalize_pixelformat(normalized_pixels,
                                                    &frame->pixelformat,
                                                    frame->pixels,
                                                    frame->pixelformat,
                                                    frame->width,
                                                    frame->height);
        if (SIXEL_FAILED(status)) {
            sixel_allocator_free(frame->allocator, normalized_pixels);
            goto end;
        }
        sixel_allocator_free(frame->allocator, frame->pixels);
        frame->pixels = normalized_pixels;
        break;
    default:
        break;
    }

    status = clip(frame->pixels,
                  frame->width,
                  frame->height,
                  frame->pixelformat,
                  x,
                  y,
                  width,
                  height);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    frame->width = width;
    frame->height = height;

    status = SIXEL_OK;

end:
    sixel_frame_unref(frame);

    return status;
}


#if HAVE_TESTS
static int
test1(void)
{
    sixel_frame_t *frame = NULL;
    int nret = EXIT_FAILURE;

#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    frame = sixel_frame_create();
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
#  pragma GCC diagnostic pop
#endif
    if (frame == NULL) {
        goto error;
    }
    sixel_frame_ref(frame);
    sixel_frame_unref(frame);
    nret = EXIT_SUCCESS;

error:
    sixel_frame_unref(frame);
    return nret;
}


static int
test2(void)
{
    sixel_frame_t *frame = NULL;
    int nret = EXIT_FAILURE;
    unsigned char *pixels = malloc(4);
    unsigned char *bgcolor = malloc(3);
    SIXELSTATUS status;

    pixels[0] = 0x43;
    pixels[1] = 0x89;
    pixels[2] = 0x97;
    pixels[3] = 0x32;

    memset(bgcolor, 0x10, 3);

#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    frame = sixel_frame_create();
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
#  pragma GCC diagnostic pop
#endif

    if (frame == NULL) {
        goto error;
    }

    status = sixel_frame_init(frame,
                              pixels,
                              1,
                              1,
                              SIXEL_PIXELFORMAT_RGBA8888,
                              NULL,
                              0);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_frame_strip_alpha(frame, bgcolor);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    if (frame->pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        goto error;
    }

    if (frame->pixels[0] != (0x43 * 0x32 + 0x10 * (0xff - 0x32)) >> 8) {
        goto error;
    }

    if (frame->pixels[1] != (0x89 * 0x32 + 0x10 * (0xff - 0x32)) >> 8) {
        goto error;
    }

    if (frame->pixels[2] != (0x97 * 0x32 + 0x10 * (0xff - 0x32)) >> 8) {
        goto error;
    }

    nret = EXIT_SUCCESS;

error:
    sixel_frame_unref(frame);
    return nret;
}


static int
test3(void)
{
    sixel_frame_t *frame = NULL;
    int nret = EXIT_FAILURE;
    unsigned char *pixels = malloc(4);
    SIXELSTATUS status;

    pixels[0] = 0x43;
    pixels[1] = 0x89;
    pixels[2] = 0x97;
    pixels[3] = 0x32;

#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    frame = sixel_frame_create();
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
#  pragma GCC diagnostic pop
#endif
    if (frame == NULL) {
        goto error;
    }

    status = sixel_frame_init(frame,
                              pixels,
                              1,
                              1,
                              SIXEL_PIXELFORMAT_RGBA8888,
                              NULL,
                              0);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_frame_strip_alpha(frame, NULL);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    if (frame->pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        goto error;
    }

    if (frame->pixels[0] != 0x43) {
        goto error;
    }

    if (frame->pixels[1] != 0x89) {
        goto error;
    }

    if (frame->pixels[2] != 0x97) {
        goto error;
    }

    nret = EXIT_SUCCESS;

error:
    sixel_frame_unref(frame);
    return nret;
}


static int
test4(void)
{
    sixel_frame_t *frame = NULL;
    int nret = EXIT_FAILURE;
    unsigned char *pixels = malloc(4);
    SIXELSTATUS status;

    pixels[0] = 0x43;
    pixels[1] = 0x89;
    pixels[2] = 0x97;
    pixels[3] = 0x32;

#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    frame = sixel_frame_create();
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
#  pragma GCC diagnostic pop
#endif
    if (frame == NULL) {
        goto error;
    }

    status = sixel_frame_init(frame,
                              pixels,
                              1,
                              1,
                              SIXEL_PIXELFORMAT_ARGB8888,
                              NULL,
                              0);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_frame_strip_alpha(frame, NULL);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    if (frame->pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        goto error;
    }

    if (frame->pixels[0] != 0x89) {
        goto error;
    }

    if (frame->pixels[1] != 0x97) {
        goto error;
    }

    if (frame->pixels[2] != 0x32) {
        goto error;
    }

    nret = EXIT_SUCCESS;

error:
    sixel_frame_unref(frame);
    return nret;
}


static int
test5(void)
{
    sixel_frame_t *frame = NULL;
    int nret = EXIT_FAILURE;
    unsigned char *pixels = malloc(1);
    unsigned char *palette = malloc(3);
    SIXELSTATUS status;

    palette[0] = 0x43;
    palette[1] = 0x89;
    palette[2] = 0x97;

    pixels[0] = 0;

#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    frame = sixel_frame_create();
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
#  pragma GCC diagnostic pop
#endif
    if (frame == NULL) {
        goto error;
    }

    status = sixel_frame_init(frame,
                              pixels,
                              1,
                              1,
                              SIXEL_PIXELFORMAT_PAL8,
                              palette,
                              1);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_frame_convert_to_rgb888(frame);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    if (frame->pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        goto error;
    }

    if (frame->pixels[0] != 0x43) {
        goto error;
    }

    if (frame->pixels[1] != 0x89) {
        goto error;
    }

    if (frame->pixels[2] != 0x97) {
        goto error;
    }

    nret = EXIT_SUCCESS;

error:
    sixel_frame_unref(frame);
    return nret;
}


static int
test6(void)
{
    sixel_frame_t *frame = NULL;
    int nret = EXIT_FAILURE;
    unsigned char *pixels = malloc(6);
    unsigned char *palette = malloc(3);
    SIXELSTATUS status;

    palette[0] = 0x43;
    palette[1] = 0x89;
    palette[2] = 0x97;

    pixels[0] = 0;

#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    frame = sixel_frame_create();
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
#  pragma GCC diagnostic pop
#endif
    if (frame == NULL) {
        goto error;
    }

    status = sixel_frame_init(frame,
                              pixels,
                              1,
                              1,
                              SIXEL_PIXELFORMAT_PAL1,
                              palette,
                              1);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_frame_convert_to_rgb888(frame);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    if (frame->pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        goto error;
    }

    if (frame->pixels[0] != 0x43) {
        goto error;
    }

    if (frame->pixels[1] != 0x89) {
        goto error;
    }

    if (frame->pixels[2] != 0x97) {
        goto error;
    }

    nret = EXIT_SUCCESS;

error:
    sixel_frame_unref(frame);
    return nret;
}


SIXELAPI int
sixel_frame_tests_main(void)
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
