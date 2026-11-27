/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers
 * Copyright (c) 2014-2016 Hayaki Saito
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

#ifndef LIBSIXEL_PIXELFORMAT_H
#define LIBSIXEL_PIXELFORMAT_H

#include <sixel.h>
#include "colorspace.h"

static inline int
sixel_pixelformat_is_ycbcr422(int layout)
{
    return layout == SIXEL_PIXELFORMAT_YUYV ||
        layout == SIXEL_PIXELFORMAT_UYVY;
}

/* Internal sample representations for working buffers. The public frame
 * layout is RGBFLOAT32 for each of these coordinate systems. */
#define SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 (SIXEL_FORMATTYPE_COLOR | 0x21)
#define SIXEL_PIXELFORMAT_OKLABFLOAT32 (SIXEL_FORMATTYPE_COLOR | 0x22)
#define SIXEL_PIXELFORMAT_CIELABFLOAT32 (SIXEL_FORMATTYPE_COLOR | 0x23)
#define SIXEL_PIXELFORMAT_DIN99DFLOAT32 (SIXEL_FORMATTYPE_COLOR | 0x24)

#define SIXEL_WORKFORMAT_IS_FLOAT32(fmt) \
    ((fmt) == SIXEL_PIXELFORMAT_RGBFLOAT32 || \
     (fmt) == SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 || \
     (fmt) == SIXEL_PIXELFORMAT_OKLABFLOAT32 || \
     (fmt) == SIXEL_PIXELFORMAT_CIELABFLOAT32 || \
     (fmt) == SIXEL_PIXELFORMAT_DIN99DFLOAT32)

/*
 * Shared upper bound for channel-oriented working buffers in dithering and
 * palette lookup internals.
 */
#define SIXEL_MAX_CHANNELS 4

#ifdef __cplusplus
extern "C" {
#endif


float
sixel_pixelformat_float_channel_min(int pixelformat,
                                    int channel);

float
sixel_pixelformat_float_channel_max(int pixelformat,
                                    int channel);

SIXEL_INTERNAL_API float
sixel_pixelformat_float_channel_range(int pixelformat,
                                      int channel);

float
sixel_pixelformat_float_channel_clamp(int pixelformat,
                                      int channel,
                                      float value);

SIXEL_INTERNAL_API unsigned char
sixel_pixelformat_float_channel_to_byte(int pixelformat,
                                        int channel,
                                        float value);

SIXEL_INTERNAL_API float
sixel_pixelformat_byte_to_float(int pixelformat,
                                int channel,
                                unsigned char value);

typedef void (*sixel_rgb_reader_t)(unsigned char const *data,
                                    unsigned char *r,
                                    unsigned char *g,
                                    unsigned char *b);

sixel_rgb_reader_t
sixel_pixelformat_rgb_reader(int pixelformat);

/* Return the coordinate colorspace encoded by a pixel format. */
SIXEL_INTERNAL_API int
sixel_pixelformat_get_colorspace(int pixelformat);

/* Encode resolved coordinates for a legacy palette, lookup or dither API. */
static inline int
sixel_pixelformat_for_colorspace(int colorspace, int prefer_float32)
{
    sixel_color_geometry_t const *geometry;

    geometry = sixel_color_geometry(colorspace);
    if (geometry != NULL) {
        return sixel_geometry_format(geometry, prefer_float32);
    }
    return prefer_float32 ? SIXEL_PIXELFORMAT_RGBFLOAT32
                          : SIXEL_PIXELFORMAT_RGB888;
}

int
sixel_helper_compute_depth(int pixelformat);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_pixelformat_buffer_bytes(int width, int height, int pixelformat,
                               size_t *bytes);

/* Number of bits below the high five-bit field, or zero for other layouts. */
int
sixel_pixelformat_packed_rgb_shift(int format);

/* Swap the five-bit red and blue fields in packed 555 or 565 pixels. */
void
sixel_pixelformat_swap_packed_rb(unsigned char *dst, unsigned char const *src,
                                size_t bytes, int shift);

/* Pack RGB888/BGR888 or change the packed field widths. Buffers may alias. */
void
sixel_pixelformat_pack_rgb(unsigned char *dst, unsigned char const *src,
                          size_t count, int source, int target,
                          int src_shift, int dst_shift);

/* Expand packed 1-, 2-, or 4-bit gray rows to full-range G8 samples.
 * The public normalization helper preserves the unpacked code values.
 */
void
sixel_pixelformat_expand_gray8(unsigned char *dst, unsigned char const *src,
                               int width, int height, int pixelformat);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_PIXELFORMAT_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
