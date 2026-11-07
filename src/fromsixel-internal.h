/* SPDX-License-Identifier: MIT */
#ifndef SIXEL_FROMSIXEL_INTERNAL_H
#define SIXEL_FROMSIXEL_INTERNAL_H

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
    int palette_redefined_after_paint;
    /* Sequence composition keeps one canvas; parallel workers own a packet. */
    int sequence;
    int param;
    int nparams;
    int params[DECSIXEL_PARAMS_MAX];
} parser_context_t;


/* Shared control parsing keeps palette, framing and error semantics equal.
 * The changing-aspect painter owns only device-row geometry and stores. */
int sixel_vertical_scale(int pan, int pad);
SIXEL_INTERNAL_API int
sixel_aspect_scan(unsigned char const *p, int len, int body_only);
SIXELSTATUS sixel_aspect_paint(image_buffer_t *image,
    parser_context_t *context, int bits, int trust_raster_size,
    sixel_allocator_t *allocator);
SIXELSTATUS sixel_aspect_enable_ormode(image_buffer_t *image,
    parser_context_t *context, sixel_allocator_t *allocator);
SIXELSTATUS sixel_aspect_finish(image_buffer_t *image,
    parser_context_t *context, SIXELSTATUS status,
    sixel_allocator_t *allocator);
void image_buffer_store_pixel(image_buffer_t *image, size_t pos,
    int color_index);
void image_buffer_fill_span(image_buffer_t *image, int y, int x,
    int repeat, int color_index);

#endif /* SIXEL_FROMSIXEL_INTERNAL_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
