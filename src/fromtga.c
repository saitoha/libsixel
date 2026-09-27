/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF, OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdint.h>
#include <string.h>
#include "fromtga.h"

static unsigned int
tga_color_word(unsigned char const *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
}

static uint32_t
tga_color_offset(unsigned char const *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
        ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Validate only the encoded raster span. This prevents a short raw/RLE
 * image from consuming extension/table bytes as pixels. Packet bodies are
 * skipped in constant time; the existing raster decoder owns their values. */
static int
tga_color_raster_fits(unsigned char const *data, size_t limit)
{
    size_t offset;
    size_t remaining;
    size_t bytes;
    size_t count;
    size_t sample_bytes;
    unsigned int command;

    offset = 18u + data[0];
    remaining = (size_t)tga_color_word(data + 12u) *
        (size_t)tga_color_word(data + 14u);
    sample_bytes = ((size_t)data[16] + 7u) / 8u;
    if (data[1] != 0u) {
        offset += (size_t)tga_color_word(data + 5u) *
            (((size_t)data[7] + 7u) / 8u);
    }
    if (offset > limit || sample_bytes == 0u || remaining == 0u) {
        return 0;
    }
    if (data[2] < 8u) {
        return remaining <= (limit - offset) / sample_bytes;
    }
    while (remaining != 0u) {
        if (offset == limit) {
            return 0;
        }
        command = data[offset++];
        count = (command & 127u) + 1u;
        if (count > remaining) {
            return 0;
        }
        bytes = (command & 128u) ? sample_bytes : count * sample_bytes;
        if (bytes > limit - offset) {
            return 0;
        }
        offset += bytes;
        remaining -= count;
    }
    return 1;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_tga_color_read(unsigned char const *data, size_t size,
                     sixel_tga_color_t *color)
{
    size_t footer;
    size_t extension;
    size_t length;
    size_t table;

    if (data == NULL || color == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    memset(color, 0, sizeof(*color));
    if (size < 44u ||
        memcmp(data + size - 18u, "TRUEVISION-XFILE.\0", 18u) != 0) {
        return SIXEL_FALSE;
    }
    footer = size - 26u;
    extension = tga_color_offset(data + footer);
    if (extension == 0u) {
        return SIXEL_FALSE;
    }
    if (extension < 18u || extension > footer ||
        footer - extension < 495u) {
        return SIXEL_BAD_INPUT;
    }
    length = tga_color_word(data + extension);
    if (length < 495u || length > footer - extension) {
        return SIXEL_BAD_INPUT;
    }
    color->gamma_num = tga_color_word(data + extension + 478u);
    color->gamma_den = tga_color_word(data + extension + 480u);
    color->alpha_type = data[extension + 494u];
    color->raster_limit = extension;
    table = tga_color_offset(data + extension + 482u);
    if (table != 0u) {
        if (table < 18u || table > footer || footer - table < 2048u ||
            (table < extension + length && extension < table + 2048u)) {
            return SIXEL_BAD_INPUT;
        }
        color->table = data + table;
        if (table < color->raster_limit) {
            color->raster_limit = table;
        }
    } else if (color->gamma_den == 0u) {
        /* An absent denominator explicitly means no gamma declaration. */
        return SIXEL_FALSE;
    } else if (color->gamma_num == 0u ||
               color->gamma_num > 10u * color->gamma_den) {
        return SIXEL_BAD_INPUT;
    }
    if (!tga_color_raster_fits(data, color->raster_limit)) {
        return SIXEL_BAD_INPUT;
    }
    return SIXEL_OK;
}

SIXEL_INTERNAL_API float
sixel_tga_color_sample(sixel_tga_color_t const *color,
                       unsigned int channel, unsigned int sample)
{
    if (color->table == NULL) {
        return (float)((double)sample / 255.0);
    }
    return (float)((double)tga_color_word(
        color->table + (size_t)sample * 8u + channel * 2u) / 65535.0);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 : */
/* EOF */
