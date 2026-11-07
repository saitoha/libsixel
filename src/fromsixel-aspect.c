/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <limits.h>
#include <stdint.h>
#include <string.h>
#include "fromsixel-internal.h"

/* VT340 pixels are multiplied vertically. Use wide arithmetic even when
 * the parsed parameters are near INT_MAX; horizontal ratios remain 1:1. */
int
sixel_vertical_scale(int pan, int pad)
{
    int64_t scale;

    if (pan <= 0 || pad <= 0) {
        return 1;
    }
    scale = (2 * (int64_t)pan + pad) / (2 * (int64_t)pad);
    return scale < 1 ? 1 : (int)scale;
}

/* The common image allocator/resizer also owns the serial OR index plane.
 * Keeping all 16 register bits until final palette lookup matches ordinary
 * OR mode, including opaque index zero and late palette definitions. */
SIXELSTATUS
sixel_aspect_enable_ormode(image_buffer_t *image,
    parser_context_t *context, sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    image_buffer_t *indices;

    if (context->aspect_ormode != NULL) {
        return SIXEL_OK;
    }
    indices = sixel_allocator_calloc(allocator, 1U, sizeof(*indices));
    if (indices == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    status = image_buffer_init(indices, image->width, image->height,
                                0, 2, 0, allocator);
    if (SIXEL_FAILED(status)) {
        sixel_allocator_free(allocator, indices);
        return status;
    }
    context->aspect_ormode = indices;
    return SIXEL_OK;
}

/* Model the VT340's vertical pixel multiplication: a mid-image change only
 * affects subsequent pixels, and '-' advances by six times the new scale.
 * Mid-image changes have not been verified on VT340 hardware. */
/* Select the capture contract at the parser entry, not for each row. */
#if defined(_MSC_VER)
static __forceinline SIXELSTATUS
#elif defined(__GNUC__) || defined(__clang__)
static inline __attribute__((always_inline)) SIXELSTATUS
#else
static inline SIXELSTATUS
#endif
sixel_aspect_paint_body(image_buffer_t *image, parser_context_t *context,
    int bits, int trust_raster_size, sixel_allocator_t *allocator,
    int capture)
{
    SIXELSTATUS status;
    image_buffer_t *indices;
    int scale;
    int repeat;
    int bottom;
    int width;
    int height;
    int first;
    int last;
    int bit;
    int y;
    int x;
    int color;
    int end_bit;
    unsigned char rgba[4];
    size_t pos;

    scale = context->vertical_scale;
    if (context->pos_x < 0 || context->pos_y < 0 ||
            context->pos_x > INT_MAX - context->repeat_count ||
            context->pos_y > INT_MAX - 6 * scale) {
        return SIXEL_BAD_INPUT;
    }
    if ((context->sequence || capture) && bits == 0) {
        context->pos_x += context->repeat_count;
        context->repeat_count = 1;
        return SIXEL_OK;
    }
    repeat = context->repeat_count;
    if (trust_raster_size && context->attributed_ph > 0 &&
            repeat > context->attributed_ph - context->pos_x) {
        if (bits != 0) {
            context->painted_outside_raster = 1;
        }
        repeat = context->pos_x < context->attributed_ph
            ? context->attributed_ph - context->pos_x : 0;
    }
    bottom = context->pos_y + 6 * scale;
    if (trust_raster_size && context->attributed_pv > 0 &&
            bottom > context->attributed_pv) {
        bottom = context->attributed_pv;
    }
    if ((repeat > 0 && context->pos_x > SIXEL_WIDTH_LIMIT - repeat) ||
            bottom > SIXEL_HEIGHT_LIMIT) {
        return SIXEL_BAD_INPUT;
    }
    width = image->width;
    height = image->height;
    while (repeat > 0 && width < context->pos_x + repeat) {
        width = width > SIXEL_WIDTH_LIMIT / 2
            ? SIXEL_WIDTH_LIMIT : width * 2;
    }
    while (height < bottom) {
        height = height > SIXEL_HEIGHT_LIMIT / 2
            ? SIXEL_HEIGHT_LIMIT : height * 2;
    }
    if (width != image->width || height != image->height) {
        status = image_buffer_resize(image, width, height,
            context->ormode ? 0 : context->bgindex, allocator);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }
    indices = context->aspect_ormode;
    if (indices != NULL && (indices->width != width ||
            indices->height != height)) {
        status = image_buffer_resize(indices, width, height, 0, allocator);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }
    if (image->depth != 4 && context->color_index > image->ncolors) {
        image->ncolors = context->color_index;
    }
    color = image->palette[context->color_index];
    rgba[0] = (unsigned char)(color >> 16);
    rgba[1] = (unsigned char)(color >> 8);
    rgba[2] = (unsigned char)color;
    rgba[3] = 255;
    for (bit = 0; bit < 6; ++bit) {
        if ((bits & (1 << bit)) == 0) {
            continue;
        }
        first = context->pos_y + bit * scale;
        /* Consecutive set bits form one vertical run. Keep the final bit's
         * start for capture's partial-row exception below. */
        end_bit = bit + 1;
        while (end_bit < 6 && (bits & (1 << end_bit)) != 0) {
            ++end_bit;
        }
        last = context->pos_y + end_bit * scale;
        bit = end_bit - 1;
        if (trust_raster_size && context->attributed_pv > 0 &&
                last > context->attributed_pv) {
            /* Capture rasters allow a partial final multiplied row. */
            if (!capture || last - scale >= context->attributed_pv) {
                context->painted_outside_raster = 1;
            }
            last = context->attributed_pv;
        }
        if (repeat <= 0 || first >= last) {
            continue;
        }
        for (y = first; y < last; ++y) {
            pos = (size_t)y * width + context->pos_x;
            if (indices != NULL) {
                for (x = 0; x < repeat; ++x) {
                    indices->pixels.in_shorts[pos + x] |=
                        (unsigned short)context->color_index;
                }
            } else {
                if (image->depth == 1) {
                    memset(image->pixels.in_bytes + pos,
                        context->color_index, (size_t)repeat);
                } else if (image->depth == 2) {
                    for (x = 0; x < repeat; ++x) {
                        image->pixels.in_shorts[pos + x] =
                            (unsigned short)context->color_index;
                    }
                } else {
                    for (x = 0; x < repeat; ++x) {
                        memcpy(image->pixels.in_bytes + (pos + x) * 4,
                            rgba, sizeof(rgba));
                    }
                }
                if (image->paint_mask != NULL) {
                    memset(image->paint_mask + pos, 0xff, (size_t)repeat);
                }
            }
        }
        if (context->max_x < context->pos_x + repeat - 1) {
            context->max_x = context->pos_x + repeat - 1;
        }
        if (context->max_y < last - 1) {
            context->max_y = last - 1;
        }
    }
    context->pos_x += context->repeat_count;
    context->repeat_count = 1;
    return SIXEL_OK;
}

SIXELSTATUS
sixel_aspect_paint(image_buffer_t *image, parser_context_t *context,
    int bits, int trust_raster_size, sixel_allocator_t *allocator)
{
    return sixel_aspect_paint_body(image, context, bits, trust_raster_size,
        allocator, 0);
}

SIXELSTATUS
sixel_aspect_paint_capture(image_buffer_t *image, parser_context_t *context,
    int bits, int trust_raster_size, sixel_allocator_t *allocator)
{
    return sixel_aspect_paint_body(image, context, bits, trust_raster_size,
        allocator, 1);
}

/* Finalize after cropping, and release on both success and failure. */
SIXELSTATUS
sixel_aspect_finish(image_buffer_t *image, parser_context_t *context,
    SIXELSTATUS status, sixel_allocator_t *allocator)
{
    image_buffer_t *indices;
    size_t pos;
    size_t size;
    int color;

    indices = context->aspect_ormode;
    if (indices == NULL) {
        return status;
    }
    if (SIXEL_SUCCEEDED(status)) {
        if (indices->width != image->width ||
                indices->height != image->height) {
            status = image_buffer_resize(indices, image->width,
                                          image->height, 0, allocator);
        }
        if (SIXEL_SUCCEEDED(status)) {
            size = (size_t)image->width * image->height;
            for (pos = 0; pos < size; ++pos) {
                color = indices->pixels.in_shorts[pos];
                image_buffer_store_pixel(image, pos, color);
                if (color + 1 > image->ncolors) {
                    image->ncolors = color + 1;
                }
            }
            if (image->paint_mask != NULL) {
                memset(image->paint_mask, 0xff, size);
            }
        }
    }
    sixel_allocator_free(allocator, indices->pixels.p);
    sixel_allocator_free(allocator, indices);
    context->aspect_ormode = NULL;
    return status;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
