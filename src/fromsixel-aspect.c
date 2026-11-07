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

/* Look for terminators only in a gap that actually contains another raster
 * command. A normal single-DECGRA stream needs just one payload memchr. */
static int
sixel_aspect_has_st(unsigned char const *p, unsigned char const *end)
{
    unsigned char const *esc;

    if (memchr(p, 0x9c, (size_t)(end - p)) != NULL) {
        return 1;
    }
    while ((esc = memchr(p, 0x1b, (size_t)(end - p))) != NULL) {
        p = esc + 1;
        if (p < end && (*p == '\\' || *p == 0x9c)) {
            return 1;
        }
    }
    return 0;
}

/* Only the prefix before the first DECGRA needs a drawing-byte check.
 * After a head declaration, equal effective scales can always use the
 * ordinary decoder. The large paint payload is traversed by memchr only.
 * Invalid/overflowing parameters are left to the common parser's checks. */
SIXEL_INTERNAL_API int
sixel_aspect_scan(unsigned char const *p, int len, int body_only)
{
    unsigned char const *end;
    unsigned char const *raster;
    unsigned char const *prefix;
    int pan;
    int pad;
    int value;
    int field;
    int digit;
    int scale;
    int first_scale;
    int head;
    int state;

    if (p == NULL || len <= 0) {
        return 0;
    }
    end = p + len;
    raster = memchr(p, '"', (size_t)len);
    if (raster == NULL) {
        return 0;
    }
    state = body_only ? PS_DECSIXEL : PS_GROUND;
    while (p < end && state != PS_DECSIXEL) {
        if (state == PS_GROUND) {
            if (*p == 0x1b) {
                state = PS_ESC;
            } else if (*p == 0x90) {
                state = PS_DCS;
            } else if (*p == 0x9c) {
                return 0;
            }
        } else if (state == PS_ESC) {
            if (*p == 'P') {
                state = PS_DCS;
            } else if (*p == '\\' || *p == 0x9c) {
                return 0;
            }
        } else if (state == PS_DCS) {
            if (*p == 'q') {
                state = PS_DECSIXEL;
            } else if (*p == 0x1b) {
                state = PS_ESC;
            }
        }
        ++p;
    }
    if (raster < p) {
        raster = memchr(p, '"', (size_t)(end - p));
    }
    if (raster == NULL) {
        return 0;
    }
    head = 1;
    for (prefix = p; prefix < raster; ++prefix) {
        if ((*prefix >= '?' && *prefix <= '~') ||
                *prefix == '!' || *prefix == '$' || *prefix == '-') {
            head = 0;
            break;
        }
    }
    first_scale = 0;
    do {
        if (sixel_aspect_has_st(p, raster)) {
            break;
        }
        p = raster + 1;
        pan = pad = value = field = 0;
        while (p < end) {
            if (*p >= '0' && *p <= '9') {
                digit = *p - '0';
                if (value > (INT_MAX - digit) / 10) {
                    return 1;
                }
                value = value * 10 + digit;
            } else {
                if (field == 0) {
                    pan = value;
                } else if (field == 1) {
                    pad = value;
                }
                if (*p != ';') {
                    break;
                }
                ++field;
                value = 0;
            }
            ++p;
        }
        /* An unfinished or ESC-interrupted command has no effect. */
        if (p == end) {
            break;
        }
        if (*p != 0x1b) {
            scale = sixel_vertical_scale(pan, pad);
            if ((first_scale != 0 && scale != first_scale) ||
                    (first_scale == 0 && !head && scale != 1)) {
                return 1;
            }
            first_scale = scale;
        }
        raster = memchr(p, '"', (size_t)(end - p));
    } while (raster != NULL);
    return 0;
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
SIXELSTATUS
sixel_aspect_paint(image_buffer_t *image, parser_context_t *context,
    int bits, int trust_raster_size, sixel_allocator_t *allocator)
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
    size_t pos;

    scale = context->vertical_scale;
    if (context->pos_x < 0 || context->pos_y < 0 ||
            context->pos_x > INT_MAX - context->repeat_count ||
            context->pos_y > INT_MAX - 6 * scale) {
        return SIXEL_BAD_INPUT;
    }
    if (context->sequence && bits == 0) {
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
    for (bit = 0; bit < 6; ++bit) {
        if ((bits & (1 << bit)) == 0) {
            continue;
        }
        first = context->pos_y + bit * scale;
        last = first + scale;
        if (trust_raster_size && context->attributed_pv > 0 &&
                last > context->attributed_pv) {
            context->painted_outside_raster = 1;
            last = context->attributed_pv;
        }
        if (repeat <= 0 || first >= last) {
            continue;
        }
        for (y = first; y < last; ++y) {
            if (indices != NULL) {
                pos = (size_t)y * width + context->pos_x;
                for (x = 0; x < repeat; ++x) {
                    indices->pixels.in_shorts[pos + x] |=
                        (unsigned short)context->color_index;
                }
            } else {
                image_buffer_fill_span(image, y, context->pos_x,
                                        repeat, context->color_index);
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
