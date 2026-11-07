/* SPDX-License-Identifier: MIT */
#ifndef SIXEL_FROMSIXEL_INTERNAL_H
#define SIXEL_FROMSIXEL_INTERNAL_H

#include <string.h>
#include "decoder-image.h"
#include "sixel_decode_pixels.h"

#define DECSIXEL_PARAMS_MAX 16

typedef enum parse_state {
    PS_GROUND     = 0,
    PS_ESC        = 1,  /* ESC */
    PS_DCS        = 2,  /* DCS header: ESC P parameters q */
    PS_DECSIXEL   = 3,  /* DECSIXEL body part ", $, -, ? ... ~ */
    PS_DECGRA     = 4,  /* DECGRA Set Raster Attributes " Pan; Pad; Ph; Pv */
    PS_DECGRI     = 5,  /* DECGRI Graphics Repeat Introducer ! Pn Ch */
    PS_DECGCI     = 6   /* Color: # Pc; Pu; Px; Py; Pz */
} parse_state_t;

typedef struct parser_context {
    parse_state_t state;
    sixel_palette_transform_t const *palette_transform;
    int pos_x;
    int pos_y;
    int max_x;
    int max_y;
    int attributed_pan;
    int attributed_pad;
    int attributed_ph;
    /* Only the constant-aspect path converts Pv to source rows. */
    int output_pv;
    int attributed_pv;
    int vertical_scale;
    int changing_aspect;
    image_buffer_t *aspect_ormode;
    int repeat_count;
    int color_index;
    int bgindex;
    int ormode;
    uint64_t ormode_spread;
    uint64_t ormode_high_spread;
    uint64_t normal_color;
    int normal_disabled;
    int simd_neon;
    int painted_outside_raster;
    int painted;
    /* Non-pixel drawing bytes also lock the current scale. */
    int drawing_control;
    int palette_redefined_after_paint;
    /* Sequence composition keeps one canvas; parallel workers own a packet. */
    int sequence;
    int param;
    int nparams;
    int params[DECSIXEL_PARAMS_MAX];
    /* Strict packet syntax without disabling ordinary pixel/worker lanes. */
    int capture;
    unsigned char *capture_defined;
    int capture_definitions;
    uint64_t **capture_columns;
    int capture_columns_height;
} parser_context_t;


/* Shared control parsing keeps palette, framing and error semantics equal.
 * The changing-aspect painter owns only device-row geometry and stores. */
int sixel_vertical_scale(int pan, int pad);
SIXELSTATUS sixel_aspect_paint(image_buffer_t *image,
    parser_context_t *context, int bits, int trust_raster_size,
    sixel_allocator_t *allocator);
SIXELSTATUS sixel_aspect_paint_capture(image_buffer_t *image,
    parser_context_t *context, int bits, int trust_raster_size,
    sixel_allocator_t *allocator);
SIXELSTATUS sixel_aspect_enable_ormode(image_buffer_t *image,
    parser_context_t *context, sixel_allocator_t *allocator);
SIXELSTATUS sixel_aspect_finish(image_buffer_t *image,
    parser_context_t *context, SIXELSTATUS status,
    sixel_allocator_t *allocator);
/* Both painters need these tiny stores visible to the optimizer. External
 * linkage can prevent specialization of the ordinary decoder's hot paths. */
/*
 * Store a single pixel in the image buffer. When the decoder is in direct
 * color mode the palette index is translated into an RGBA quadruplet with an
 * opaque alpha channel. Indexed mode keeps the original palette entry so the
 * caller can compose a palette later.
 */
static inline void
image_buffer_store_pixel(image_buffer_t *image, size_t pos, int color_index)
{
    unsigned char *bytes;
    int color;
    int depth;

    depth = image->depth;
    if (color_index < 0 || color_index >= SIXEL_PALETTE_MAX_DECODER) {
        return;
    }

    if (depth == 1U) {
        image->pixels.in_bytes[pos] = (unsigned char)color_index;
    } else if (depth == 2U) {
        image->pixels.in_shorts[pos] = (unsigned short)color_index;
    } else {  /* rgba */
        color = image->palette[color_index];
        bytes = image->pixels.in_bytes + pos * 4U;
        bytes[0] = (unsigned char)((color >> 16) & 0xff);
        bytes[1] = (unsigned char)((color >> 8) & 0xff);
        bytes[2] = (unsigned char)(color & 0xff);
        bytes[3] = 255u;
    }
}

static inline void
image_buffer_mark_painted_span(image_buffer_t *image,
                               size_t pos,
                               int repeat)
{
    if (image->paint_mask == NULL || repeat <= 0) {
        return;
    }

    memset(image->paint_mask + pos, 0xff, (size_t)repeat);
}

/*
 * Fill a horizontal run starting at (x, y) with the requested palette index.
 * Direct color output expands the span into RGBA bytes to keep the alpha
 * channel and the color components consistent with single pixel writes.
 */
static inline void
image_buffer_fill_span(image_buffer_t *image,
                       int y,
                       int x,
                       int repeat,
                       int color_index)
{
    size_t pos;
    int n;

    pos = (size_t)image->width * (size_t)y + (size_t)x;

    if (image->depth == 1U) {
        memset(image->pixels.in_bytes + pos, color_index, (size_t)repeat);
    } else {
        for (n = 0; n < repeat; ++n) {
            image_buffer_store_pixel(image, pos + (size_t)n, color_index);
        }
    }
    image_buffer_mark_painted_span(image, pos, repeat);
}


#endif /* SIXEL_FROMSIXEL_INTERNAL_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
