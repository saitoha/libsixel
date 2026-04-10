/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 * Copyright (c) 2014-2016 Hayaki Saito
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#if HAVE_STRING_H
# include <string.h>
#endif  /* HAVE_STRING_H */

#include <sixel.h>

#include "dither.h"
#include "logger.h"
#include "output.h"
#include "pixelformat.h"
#include "tosixel-highcolor.h"

enum {
    PALETTE_HIT    = 1,
    PALETTE_CHANGE = 2
};

/*
 * Internal encoder helpers implemented in tosixel.c.
 * Keep prototypes here to avoid implicit declarations in this module.
 */
SIXELSTATUS sixel_encode_header(
    int width,
    int height,
    int keycolor,
    sixel_output_t *output);
SIXELSTATUS sixel_encode_body(
    sixel_index_t *pixels,
    int width,
    int height,
    unsigned char *palette,
    float const *palette_float,
    int ncolors,
    int keycolor,
    int bodyonly,
    sixel_output_t *output,
    unsigned char *palstate,
    sixel_allocator_t *allocator,
    int pin_threads,
    sixel_logger_t *logger);
SIXELSTATUS sixel_encode_footer(sixel_output_t *output);

/*
 * Normalize a channel value into the range representable by an unsigned char
 * and make the narrowing explicit for MSVC. Negative errors are clamped to
 * zero and large positive values are capped at 0xff.
 */
static unsigned char
sixel_clamp_channel_to_byte(int value)
{
    if (value < 0) {
        return 0;
    }
    if (value > 0xff) {
        return 0xff;
    }

    return (unsigned char)value;
}

static void
dither_func_none(unsigned char *data, int width)
{
    (void) data;  /* unused */
    (void) width; /* unused */
}


static void
dither_func_fs(unsigned char *data, int width)
{
    int r, g, b;
    int error_r = data[0] & 0x7;
    int error_g = data[1] & 0x7;
    int error_b = data[2] & 0x7;

    /* Floyd Steinberg Method
     *          curr    7/16
     *  3/16    5/48    1/16
     */
    r = (data[3 + 0] + (error_r * 5 >> 4));
    g = (data[3 + 1] + (error_g * 5 >> 4));
    b = (data[3 + 2] + (error_b * 5 >> 4));
    data[3 + 0] = sixel_clamp_channel_to_byte(r);
    data[3 + 1] = sixel_clamp_channel_to_byte(g);
    data[3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[width * 3 - 3 + 0] + (error_r * 3 >> 4);
    g = data[width * 3 - 3 + 1] + (error_g * 3 >> 4);
    b = data[width * 3 - 3 + 2] + (error_b * 3 >> 4);
    data[width * 3 - 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[width * 3 - 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[width * 3 - 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[width * 3 + 0] + (error_r * 5 >> 4);
    g = data[width * 3 + 1] + (error_g * 5 >> 4);
    b = data[width * 3 + 2] + (error_b * 5 >> 4);
    data[width * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[width * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[width * 3 + 2] = sixel_clamp_channel_to_byte(b);
}


static void
dither_func_atkinson(unsigned char *data, int width)
{
    int r, g, b;
    int error_r = data[0] & 0x7;
    int error_g = data[1] & 0x7;
    int error_b = data[2] & 0x7;

    error_r += 4;
    error_g += 4;
    error_b += 4;

    /* Atkinson's Method
     *          curr    1/8    1/8
     *   1/8     1/8    1/8
     *           1/8
     */
    r = data[(width * 0 + 1) * 3 + 0] + (error_r >> 3);
    g = data[(width * 0 + 1) * 3 + 1] + (error_g >> 3);
    b = data[(width * 0 + 1) * 3 + 2] + (error_b >> 3);
    data[(width * 0 + 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 0 + 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 0 + 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 0 + 2) * 3 + 0] + (error_r >> 3);
    g = data[(width * 0 + 2) * 3 + 1] + (error_g >> 3);
    b = data[(width * 0 + 2) * 3 + 2] + (error_b >> 3);
    data[(width * 0 + 2) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 0 + 2) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 0 + 2) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 - 1) * 3 + 0] + (error_r >> 3);
    g = data[(width * 1 - 1) * 3 + 1] + (error_g >> 3);
    b = data[(width * 1 - 1) * 3 + 2] + (error_b >> 3);
    data[(width * 1 - 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 - 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 - 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 + 0) * 3 + 0] + (error_r >> 3);
    g = data[(width * 1 + 0) * 3 + 1] + (error_g >> 3);
    b = data[(width * 1 + 0) * 3 + 2] + (error_b >> 3);
    data[(width * 1 + 0) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 + 0) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 + 0) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = (data[(width * 1 + 1) * 3 + 0] + (error_r >> 3));
    g = (data[(width * 1 + 1) * 3 + 1] + (error_g >> 3));
    b = (data[(width * 1 + 1) * 3 + 2] + (error_b >> 3));
    data[(width * 1 + 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 + 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 + 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = (data[(width * 2 + 0) * 3 + 0] + (error_r >> 3));
    g = (data[(width * 2 + 0) * 3 + 1] + (error_g >> 3));
    b = (data[(width * 2 + 0) * 3 + 2] + (error_b >> 3));
    data[(width * 2 + 0) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 2 + 0) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 2 + 0) * 3 + 2] = sixel_clamp_channel_to_byte(b);
}


static void
dither_func_jajuni(unsigned char *data, int width)
{
    int r, g, b;
    int error_r = data[0] & 0x7;
    int error_g = data[1] & 0x7;
    int error_b = data[2] & 0x7;

    error_r += 4;
    error_g += 4;
    error_b += 4;

    /* Jarvis, Judice & Ninke Method
     *                  curr    7/48    5/48
     *  3/48    5/48    7/48    5/48    3/48
     *  1/48    3/48    5/48    3/48    1/48
     */
    r = data[(width * 0 + 1) * 3 + 0] + (error_r * 7 / 48);
    g = data[(width * 0 + 1) * 3 + 1] + (error_g * 7 / 48);
    b = data[(width * 0 + 1) * 3 + 2] + (error_b * 7 / 48);
    data[(width * 0 + 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 0 + 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 0 + 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 0 + 2) * 3 + 0] + (error_r * 5 / 48);
    g = data[(width * 0 + 2) * 3 + 1] + (error_g * 5 / 48);
    b = data[(width * 0 + 2) * 3 + 2] + (error_b * 5 / 48);
    data[(width * 0 + 2) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 0 + 2) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 0 + 2) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 - 2) * 3 + 0] + (error_r * 3 / 48);
    g = data[(width * 1 - 2) * 3 + 1] + (error_g * 3 / 48);
    b = data[(width * 1 - 2) * 3 + 2] + (error_b * 3 / 48);
    data[(width * 1 - 2) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 - 2) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 - 2) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 - 1) * 3 + 0] + (error_r * 5 / 48);
    g = data[(width * 1 - 1) * 3 + 1] + (error_g * 5 / 48);
    b = data[(width * 1 - 1) * 3 + 2] + (error_b * 5 / 48);
    data[(width * 1 - 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 - 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 - 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 + 0) * 3 + 0] + (error_r * 7 / 48);
    g = data[(width * 1 + 0) * 3 + 1] + (error_g * 7 / 48);
    b = data[(width * 1 + 0) * 3 + 2] + (error_b * 7 / 48);
    data[(width * 1 + 0) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 + 0) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 + 0) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 + 1) * 3 + 0] + (error_r * 5 / 48);
    g = data[(width * 1 + 1) * 3 + 1] + (error_g * 5 / 48);
    b = data[(width * 1 + 1) * 3 + 2] + (error_b * 5 / 48);
    data[(width * 1 + 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 + 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 + 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 + 2) * 3 + 0] + (error_r * 3 / 48);
    g = data[(width * 1 + 2) * 3 + 1] + (error_g * 3 / 48);
    b = data[(width * 1 + 2) * 3 + 2] + (error_b * 3 / 48);
    data[(width * 1 + 2) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 + 2) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 + 2) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 2 - 2) * 3 + 0] + (error_r * 1 / 48);
    g = data[(width * 2 - 2) * 3 + 1] + (error_g * 1 / 48);
    b = data[(width * 2 - 2) * 3 + 2] + (error_b * 1 / 48);
    data[(width * 2 - 2) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 2 - 2) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 2 - 2) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 2 - 1) * 3 + 0] + (error_r * 3 / 48);
    g = data[(width * 2 - 1) * 3 + 1] + (error_g * 3 / 48);
    b = data[(width * 2 - 1) * 3 + 2] + (error_b * 3 / 48);
    data[(width * 2 - 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 2 - 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 2 - 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 2 + 0) * 3 + 0] + (error_r * 5 / 48);
    g = data[(width * 2 + 0) * 3 + 1] + (error_g * 5 / 48);
    b = data[(width * 2 + 0) * 3 + 2] + (error_b * 5 / 48);
    data[(width * 2 + 0) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 2 + 0) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 2 + 0) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 2 + 1) * 3 + 0] + (error_r * 3 / 48);
    g = data[(width * 2 + 1) * 3 + 1] + (error_g * 3 / 48);
    b = data[(width * 2 + 1) * 3 + 2] + (error_b * 3 / 48);
    data[(width * 2 + 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 2 + 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 2 + 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 2 + 2) * 3 + 0] + (error_r * 1 / 48);
    g = data[(width * 2 + 2) * 3 + 1] + (error_g * 1 / 48);
    b = data[(width * 2 + 2) * 3 + 2] + (error_b * 1 / 48);
    data[(width * 2 + 2) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 2 + 2) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 2 + 2) * 3 + 2] = sixel_clamp_channel_to_byte(b);
}


static void
dither_func_stucki(unsigned char *data, int width)
{
    int r, g, b;
    int error_r = data[0] & 0x7;
    int error_g = data[1] & 0x7;
    int error_b = data[2] & 0x7;

    error_r += 4;
    error_g += 4;
    error_b += 4;

    /* Stucki's Method
     *                  curr    8/48    4/48
     *  2/48    4/48    8/48    4/48    2/48
     *  1/48    2/48    4/48    2/48    1/48
     */
    r = data[(width * 0 + 1) * 3 + 0] + (error_r * 8 / 48);
    g = data[(width * 0 + 1) * 3 + 1] + (error_g * 8 / 48);
    b = data[(width * 0 + 1) * 3 + 2] + (error_b * 8 / 48);
    data[(width * 0 + 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 0 + 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 0 + 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 0 + 2) * 3 + 0] + (error_r * 4 / 48);
    g = data[(width * 0 + 2) * 3 + 1] + (error_g * 4 / 48);
    b = data[(width * 0 + 2) * 3 + 2] + (error_b * 4 / 48);
    data[(width * 0 + 2) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 0 + 2) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 0 + 2) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 - 2) * 3 + 0] + (error_r * 2 / 48);
    g = data[(width * 1 - 2) * 3 + 1] + (error_g * 2 / 48);
    b = data[(width * 1 - 2) * 3 + 2] + (error_b * 2 / 48);
    data[(width * 1 - 2) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 - 2) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 - 2) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 - 1) * 3 + 0] + (error_r * 4 / 48);
    g = data[(width * 1 - 1) * 3 + 1] + (error_g * 4 / 48);
    b = data[(width * 1 - 1) * 3 + 2] + (error_b * 4 / 48);
    data[(width * 1 - 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 - 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 - 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 + 0) * 3 + 0] + (error_r * 8 / 48);
    g = data[(width * 1 + 0) * 3 + 1] + (error_g * 8 / 48);
    b = data[(width * 1 + 0) * 3 + 2] + (error_b * 8 / 48);
    data[(width * 1 + 0) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 + 0) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 + 0) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 + 1) * 3 + 0] + (error_r * 4 / 48);
    g = data[(width * 1 + 1) * 3 + 1] + (error_g * 4 / 48);
    b = data[(width * 1 + 1) * 3 + 2] + (error_b * 4 / 48);
    data[(width * 1 + 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 + 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 + 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 + 2) * 3 + 0] + (error_r * 2 / 48);
    g = data[(width * 1 + 2) * 3 + 1] + (error_g * 2 / 48);
    b = data[(width * 1 + 2) * 3 + 2] + (error_b * 2 / 48);
    data[(width * 1 + 2) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 + 2) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 + 2) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 2 - 2) * 3 + 0] + (error_r * 1 / 48);
    g = data[(width * 2 - 2) * 3 + 1] + (error_g * 1 / 48);
    b = data[(width * 2 - 2) * 3 + 2] + (error_b * 1 / 48);
    data[(width * 2 - 2) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 2 - 2) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 2 - 2) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 2 - 1) * 3 + 0] + (error_r * 2 / 48);
    g = data[(width * 2 - 1) * 3 + 1] + (error_g * 2 / 48);
    b = data[(width * 2 - 1) * 3 + 2] + (error_b * 2 / 48);
    data[(width * 2 - 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 2 - 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 2 - 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 2 + 0) * 3 + 0] + (error_r * 4 / 48);
    g = data[(width * 2 + 0) * 3 + 1] + (error_g * 4 / 48);
    b = data[(width * 2 + 0) * 3 + 2] + (error_b * 4 / 48);
    data[(width * 2 + 0) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 2 + 0) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 2 + 0) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 2 + 1) * 3 + 0] + (error_r * 2 / 48);
    g = data[(width * 2 + 1) * 3 + 1] + (error_g * 2 / 48);
    b = data[(width * 2 + 1) * 3 + 2] + (error_b * 2 / 48);
    data[(width * 2 + 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 2 + 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 2 + 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 2 + 2) * 3 + 0] + (error_r * 1 / 48);
    g = data[(width * 2 + 2) * 3 + 1] + (error_g * 1 / 48);
    b = data[(width * 2 + 2) * 3 + 2] + (error_b * 1 / 48);
    data[(width * 2 + 2) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 2 + 2) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 2 + 2) * 3 + 2] = sixel_clamp_channel_to_byte(b);
}


static void
dither_func_burkes(unsigned char *data, int width)
{
    int r, g, b;
    int error_r = data[0] & 0x7;
    int error_g = data[1] & 0x7;
    int error_b = data[2] & 0x7;

    error_r += 2;
    error_g += 2;
    error_b += 2;

    /* Burkes' Method
     *                  curr    4/16    2/16
     *  1/16    2/16    4/16    2/16    1/16
     */
    r = data[(width * 0 + 1) * 3 + 0] + (error_r * 4 / 16);
    g = data[(width * 0 + 1) * 3 + 1] + (error_g * 4 / 16);
    b = data[(width * 0 + 1) * 3 + 2] + (error_b * 4 / 16);
    data[(width * 0 + 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 0 + 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 0 + 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 0 + 2) * 3 + 0] + (error_r * 2 / 16);
    g = data[(width * 0 + 2) * 3 + 1] + (error_g * 2 / 16);
    b = data[(width * 0 + 2) * 3 + 2] + (error_b * 2 / 16);
    data[(width * 0 + 2) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 0 + 2) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 0 + 2) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 - 2) * 3 + 0] + (error_r * 1 / 16);
    g = data[(width * 1 - 2) * 3 + 1] + (error_g * 1 / 16);
    b = data[(width * 1 - 2) * 3 + 2] + (error_b * 1 / 16);
    data[(width * 1 - 2) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 - 2) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 - 2) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 - 1) * 3 + 0] + (error_r * 2 / 16);
    g = data[(width * 1 - 1) * 3 + 1] + (error_g * 2 / 16);
    b = data[(width * 1 - 1) * 3 + 2] + (error_b * 2 / 16);
    data[(width * 1 - 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 - 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 - 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 + 0) * 3 + 0] + (error_r * 4 / 16);
    g = data[(width * 1 + 0) * 3 + 1] + (error_g * 4 / 16);
    b = data[(width * 1 + 0) * 3 + 2] + (error_b * 4 / 16);
    data[(width * 1 + 0) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 + 0) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 + 0) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 + 1) * 3 + 0] + (error_r * 2 / 16);
    g = data[(width * 1 + 1) * 3 + 1] + (error_g * 2 / 16);
    b = data[(width * 1 + 1) * 3 + 2] + (error_b * 2 / 16);
    data[(width * 1 + 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 + 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 + 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 + 2) * 3 + 0] + (error_r * 1 / 16);
    g = data[(width * 1 + 2) * 3 + 1] + (error_g * 1 / 16);
    b = data[(width * 1 + 2) * 3 + 2] + (error_b * 1 / 16);
    data[(width * 1 + 2) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 + 2) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 + 2) * 3 + 2] = sixel_clamp_channel_to_byte(b);
}


static void
dither_func_sierra1(unsigned char *data, int width)
{
    int r, g, b;
    int error_r = data[0] & 0x7;
    int error_g = data[1] & 0x7;
    int error_b = data[2] & 0x7;

    error_r += 2;
    error_g += 2;
    error_b += 2;

    /* Sierra Lite Method
     *          curr    2/4
     *  1/4     1/4
     */
    r = data[(width * 0 + 1) * 3 + 0] + (error_r * 2 / 4);
    g = data[(width * 0 + 1) * 3 + 1] + (error_g * 2 / 4);
    b = data[(width * 0 + 1) * 3 + 2] + (error_b * 2 / 4);
    data[(width * 0 + 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 0 + 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 0 + 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 - 1) * 3 + 0] + (error_r * 1 / 4);
    g = data[(width * 1 - 1) * 3 + 1] + (error_g * 1 / 4);
    b = data[(width * 1 - 1) * 3 + 2] + (error_b * 1 / 4);
    data[(width * 1 - 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 - 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 - 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 + 0) * 3 + 0] + (error_r * 1 / 4);
    g = data[(width * 1 + 0) * 3 + 1] + (error_g * 1 / 4);
    b = data[(width * 1 + 0) * 3 + 2] + (error_b * 1 / 4);
    data[(width * 1 + 0) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 + 0) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 + 0) * 3 + 2] = sixel_clamp_channel_to_byte(b);
}


static void
dither_func_sierra2(unsigned char *data, int width)
{
    int r, g, b;
    int error_r = data[0] & 0x7;
    int error_g = data[1] & 0x7;
    int error_b = data[2] & 0x7;

    error_r += 4;
    error_g += 4;
    error_b += 4;

    /* Sierra Two-row Method
     *                  curr    4/32    3/32
     *  1/32    2/32    3/32    2/32    1/32
     *                  2/32    3/32    2/32
     */
    r = data[(width * 0 + 1) * 3 + 0] + (error_r * 4 / 32);
    g = data[(width * 0 + 1) * 3 + 1] + (error_g * 4 / 32);
    b = data[(width * 0 + 1) * 3 + 2] + (error_b * 4 / 32);
    data[(width * 0 + 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 0 + 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 0 + 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 0 + 2) * 3 + 0] + (error_r * 3 / 32);
    g = data[(width * 0 + 2) * 3 + 1] + (error_g * 3 / 32);
    b = data[(width * 0 + 2) * 3 + 2] + (error_b * 3 / 32);
    data[(width * 0 + 2) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 0 + 2) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 0 + 2) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 - 2) * 3 + 0] + (error_r * 1 / 32);
    g = data[(width * 1 - 2) * 3 + 1] + (error_g * 1 / 32);
    b = data[(width * 1 - 2) * 3 + 2] + (error_b * 1 / 32);
    data[(width * 1 - 2) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 - 2) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 - 2) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 - 1) * 3 + 0] + (error_r * 2 / 32);
    g = data[(width * 1 - 1) * 3 + 1] + (error_g * 2 / 32);
    b = data[(width * 1 - 1) * 3 + 2] + (error_b * 2 / 32);
    data[(width * 1 - 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 - 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 - 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 + 0) * 3 + 0] + (error_r * 3 / 32);
    g = data[(width * 1 + 0) * 3 + 1] + (error_g * 3 / 32);
    b = data[(width * 1 + 0) * 3 + 2] + (error_b * 3 / 32);
    data[(width * 1 + 0) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 + 0) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 + 0) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 + 1) * 3 + 0] + (error_r * 2 / 32);
    g = data[(width * 1 + 1) * 3 + 1] + (error_g * 2 / 32);
    b = data[(width * 1 + 1) * 3 + 2] + (error_b * 2 / 32);
    data[(width * 1 + 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 + 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 + 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 + 2) * 3 + 0] + (error_r * 1 / 32);
    g = data[(width * 1 + 2) * 3 + 1] + (error_g * 1 / 32);
    b = data[(width * 1 + 2) * 3 + 2] + (error_b * 1 / 32);
    data[(width * 1 + 2) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 + 2) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 + 2) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 2 - 1) * 3 + 0] + (error_r * 2 / 32);
    g = data[(width * 2 - 1) * 3 + 1] + (error_g * 2 / 32);
    b = data[(width * 2 - 1) * 3 + 2] + (error_b * 2 / 32);
    data[(width * 2 - 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 2 - 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 2 - 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 2 + 0) * 3 + 0] + (error_r * 3 / 32);
    g = data[(width * 2 + 0) * 3 + 1] + (error_g * 3 / 32);
    b = data[(width * 2 + 0) * 3 + 2] + (error_b * 3 / 32);
    data[(width * 2 + 0) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 2 + 0) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 2 + 0) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 2 + 1) * 3 + 0] + (error_r * 2 / 32);
    g = data[(width * 2 + 1) * 3 + 1] + (error_g * 2 / 32);
    b = data[(width * 2 + 1) * 3 + 2] + (error_b * 2 / 32);
    data[(width * 2 + 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 2 + 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 2 + 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 2 + 2) * 3 + 0] + (error_r * 1 / 32);
    g = data[(width * 2 + 2) * 3 + 1] + (error_g * 1 / 32);
    b = data[(width * 2 + 2) * 3 + 2] + (error_b * 1 / 32);
    data[(width * 2 + 2) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 2 + 2) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 2 + 2) * 3 + 2] = sixel_clamp_channel_to_byte(b);
}


static void
dither_func_sierra3(unsigned char *data, int width)
{
    int r, g, b;
    int error_r = data[0] & 0x7;
    int error_g = data[1] & 0x7;
    int error_b = data[2] & 0x7;

    error_r += 4;
    error_g += 4;
    error_b += 4;

    /* Sierra-3 Method
     *                  curr    5/32    3/32
     *  2/32    4/32    5/32    4/32    2/32
     *                  2/32    3/32    2/32
     */
    r = data[(width * 0 + 1) * 3 + 0] + (error_r * 5 / 32);
    g = data[(width * 0 + 1) * 3 + 1] + (error_g * 5 / 32);
    b = data[(width * 0 + 1) * 3 + 2] + (error_b * 5 / 32);
    data[(width * 0 + 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 0 + 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 0 + 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 0 + 2) * 3 + 0] + (error_r * 3 / 32);
    g = data[(width * 0 + 2) * 3 + 1] + (error_g * 3 / 32);
    b = data[(width * 0 + 2) * 3 + 2] + (error_b * 3 / 32);
    data[(width * 0 + 2) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 0 + 2) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 0 + 2) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 - 2) * 3 + 0] + (error_r * 2 / 32);
    g = data[(width * 1 - 2) * 3 + 1] + (error_g * 2 / 32);
    b = data[(width * 1 - 2) * 3 + 2] + (error_b * 2 / 32);
    data[(width * 1 - 2) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 - 2) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 - 2) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 - 1) * 3 + 0] + (error_r * 4 / 32);
    g = data[(width * 1 - 1) * 3 + 1] + (error_g * 4 / 32);
    b = data[(width * 1 - 1) * 3 + 2] + (error_b * 4 / 32);
    data[(width * 1 - 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 - 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 - 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 + 0) * 3 + 0] + (error_r * 5 / 32);
    g = data[(width * 1 + 0) * 3 + 1] + (error_g * 5 / 32);
    b = data[(width * 1 + 0) * 3 + 2] + (error_b * 5 / 32);
    data[(width * 1 + 0) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 + 0) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 + 0) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 + 1) * 3 + 0] + (error_r * 4 / 32);
    g = data[(width * 1 + 1) * 3 + 1] + (error_g * 4 / 32);
    b = data[(width * 1 + 1) * 3 + 2] + (error_b * 4 / 32);
    data[(width * 1 + 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 + 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 + 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 1 + 2) * 3 + 0] + (error_r * 2 / 32);
    g = data[(width * 1 + 2) * 3 + 1] + (error_g * 2 / 32);
    b = data[(width * 1 + 2) * 3 + 2] + (error_b * 2 / 32);
    data[(width * 1 + 2) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 1 + 2) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 1 + 2) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 2 - 1) * 3 + 0] + (error_r * 2 / 32);
    g = data[(width * 2 - 1) * 3 + 1] + (error_g * 2 / 32);
    b = data[(width * 2 - 1) * 3 + 2] + (error_b * 2 / 32);
    data[(width * 2 - 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 2 - 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 2 - 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 2 + 0) * 3 + 0] + (error_r * 3 / 32);
    g = data[(width * 2 + 0) * 3 + 1] + (error_g * 3 / 32);
    b = data[(width * 2 + 0) * 3 + 2] + (error_b * 3 / 32);
    data[(width * 2 + 0) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 2 + 0) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 2 + 0) * 3 + 2] = sixel_clamp_channel_to_byte(b);
    r = data[(width * 2 + 1) * 3 + 0] + (error_r * 2 / 32);
    g = data[(width * 2 + 1) * 3 + 1] + (error_g * 2 / 32);
    b = data[(width * 2 + 1) * 3 + 2] + (error_b * 2 / 32);
    data[(width * 2 + 1) * 3 + 0] = sixel_clamp_channel_to_byte(r);
    data[(width * 2 + 1) * 3 + 1] = sixel_clamp_channel_to_byte(g);
    data[(width * 2 + 1) * 3 + 2] = sixel_clamp_channel_to_byte(b);
}


static void
dither_func_a_dither(unsigned char *data, int width, int x, int y)
{
    int c;
    float value;
    float mask;

    (void) width; /* unused */

    for (c = 0; c < 3; c ++) {
        mask = (float)((((x + c * 17) + y * 236) * 119) & 255);
        mask = ((mask - 128) / 256.0f);
        value = data[c] + mask;
        if (value < 0) {
            value = 0;
        }
        value = value > 255 ? 255 : value;
        data[c] = sixel_clamp_channel_to_byte((int)value);
    }
}


static void
dither_func_x_dither(unsigned char *data, int width, int x, int y)
{
    int c;
    float value;
    float mask;

    (void) width;  /* unused */

    for (c = 0; c < 3; c ++) {
        mask = (float)((((x + c * 17) ^ (y * 236)) * 1234) & 511);
        mask = ((mask - 128) / 512.0f);
        value = data[c] + mask;
        if (value < 0) {
            value = 0;
        }
        value = value > 255 ? 255 : value;
        data[c] = sixel_clamp_channel_to_byte((int)value);
    }
}


static void
sixel_apply_15bpp_dither(
    unsigned char *pixels,
    int x,
    int y,
    int width,
    int height,
    int method_for_diffuse)
{
    /* apply floyd steinberg dithering */
    switch (method_for_diffuse) {
    case SIXEL_DIFFUSE_FS:
        if (x < width - 1 && y < height - 1) {
            dither_func_fs(pixels, width);
        }
        break;
    case SIXEL_DIFFUSE_ATKINSON:
        if (x < width - 2 && y < height - 2) {
            dither_func_atkinson(pixels, width);
        }
        break;
    case SIXEL_DIFFUSE_JAJUNI:
        if (x < width - 2 && y < height - 2) {
            dither_func_jajuni(pixels, width);
        }
        break;
    case SIXEL_DIFFUSE_STUCKI:
        if (x < width - 2 && y < height - 2) {
            dither_func_stucki(pixels, width);
        }
        break;
    case SIXEL_DIFFUSE_BURKES:
        if (x < width - 2 && y < height - 1) {
            dither_func_burkes(pixels, width);
        }
        break;
    case SIXEL_DIFFUSE_SIERRA1:
        if (x < width - 1 && y < height - 1) {
            dither_func_sierra1(pixels, width);
        }
        break;
    case SIXEL_DIFFUSE_SIERRA2:
        if (x < width - 2 && y < height - 2) {
            dither_func_sierra2(pixels, width);
        }
        break;
    case SIXEL_DIFFUSE_SIERRA3:
        if (x < width - 2 && y < height - 2) {
            dither_func_sierra3(pixels, width);
        }
        break;
    case SIXEL_DIFFUSE_A_DITHER:
        dither_func_a_dither(pixels, width, x, y);
        break;
    case SIXEL_DIFFUSE_X_DITHER:
        dither_func_x_dither(pixels, width, x, y);
        break;
    case SIXEL_DIFFUSE_NONE:
    default:
        dither_func_none(pixels, width);
        break;
    }
}


/*
 * Encode high-color data (15bpp dithering).
 *
 * Flow overview:
 * - Normalize input pixels to RGB888 when needed.
 * - Track palette usage per pass to keep the palette within 255 entries.
 * - Emit sixel bands while updating palette hits and palette-change flags.
 */
SIXELSTATUS
sixel_encode_highcolor(
    unsigned char *pixels,
    int width,
    int height,
    sixel_dither_t *dither,
    sixel_output_t *output)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_index_t *paletted_pixels = NULL;
    unsigned char *normalized_pixels = NULL;
    /* Mark sixel line pixels which have been already drawn. */
    unsigned char *marks;
    unsigned char *rgbhit;
    unsigned char *rgb2pal;
    unsigned char palhitcount[SIXEL_PALETTE_MAX];
    unsigned char palstate[SIXEL_PALETTE_MAX];
    int output_count;
    int const maxcolors = 1 << 15;
    int whole_size = width * height  /* for paletted_pixels */
                   + maxcolors       /* for rgbhit */
                   + maxcolors       /* for rgb2pal */
                   + width * 6;      /* for marks */
    int x, y;
    unsigned char *dst;
    unsigned char *mptr;
    int dirty;
    int mod_y;
    int nextpal;
    int threshold;
    int pix;
    unsigned char mapped_index;
    int orig_height;
    unsigned char *pal;
    unsigned char *palette_entries = NULL;
    sixel_palette_t *palette_obj = NULL;
    size_t palette_count = 0U;

    if (pixels == NULL || dither == NULL || output == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encode_highcolor: invalid argument "
            "(pixels == NULL || dither == NULL || output == NULL)");
        return SIXEL_BAD_ARGUMENT;
    }

    if (dither->method_for_diffuse == SIXEL_DIFFUSE_INTERFRAME) {
        sixel_helper_set_additional_message(
            "sixel_encode_highcolor: interframe is not supported "
            "with high-color mode.");
        status = SIXEL_BAD_ARGUMENT;
        goto error;
    }

    if (dither->pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        /* normalize pixelformat */
        normalized_pixels = (unsigned char *)sixel_allocator_malloc(
            dither->allocator,
            (size_t)(width * height * 3));
        if (normalized_pixels == NULL) {
            goto error;
        }
        status = sixel_helper_normalize_pixelformat(
            normalized_pixels,
            &dither->pixelformat,
            pixels,
            dither->pixelformat,
            width,
            height);
        if (SIXEL_FAILED(status)) {
            goto error;
        }
        pixels = normalized_pixels;
    }

    palette_entries = NULL;
    palette_obj = NULL;
    palette_count = 0U;
    status = sixel_dither_get_quantized_palette(dither, &palette_obj);
    if (SIXEL_FAILED(status)) {
        goto error;
    }
    status = sixel_palette_copy_entries_8bit(
        palette_obj,
        &palette_entries,
        &palette_count,
        SIXEL_PIXELFORMAT_RGB888,
        dither->allocator);
    sixel_palette_unref(palette_obj);
    palette_obj = NULL;
    if (SIXEL_FAILED(status) || palette_entries == NULL) {
        goto error;
    }

    paletted_pixels = (sixel_index_t *)sixel_allocator_malloc(
        dither->allocator,
        (size_t)whole_size);
    if (paletted_pixels == NULL) {
        goto error;
    }
    rgbhit = paletted_pixels + width * height;
    memset(rgbhit, 0, (size_t)(maxcolors * 2 + width * 6));
    rgb2pal = rgbhit + maxcolors;
    marks = rgb2pal + maxcolors;
    output_count = 0;

next:
    dst = paletted_pixels;
    nextpal = 0;
    threshold = 1;
    dirty = 0;
    mptr = marks;
    memset(palstate, 0, sizeof(palstate));
    /* Clear palette hit counters for each pass to avoid stale data. */
    memset(palhitcount, 0, sizeof(palhitcount));
    y = mod_y = 0;

    while (1) {
        for (x = 0; x < width; x++, mptr++, dst++, pixels += 3) {
            if (*mptr) {
                *dst = 255;
            } else {
                sixel_apply_15bpp_dither(
                    pixels,
                    x,
                    y,
                    width,
                    height,
                    dither->method_for_diffuse);
                pix = ((pixels[0] & 0xf8) << 7) |
                      ((pixels[1] & 0xf8) << 2) |
                      ((pixels[2] >> 3) & 0x1f);

                if (!rgbhit[pix]) {
                    while (1) {
                        if (nextpal >= 255) {
                            if (threshold >= 255) {
                                break;
                            } else {
                                threshold = (threshold == 1) ? 9: 255;
                                nextpal = 0;
                            }
                        } else if (palstate[nextpal]
                                 || palhitcount[nextpal] > threshold) {
                            nextpal++;
                        } else {
                            break;
                        }
                    }

                    if (nextpal >= 255) {
                        dirty = 1;
                        *dst = 255;
                    } else {
                        pal = palette_entries + (nextpal * 3);

                        rgbhit[pix] = 1;
                        if (output_count > 0) {
                            rgbhit[((pal[0] & 0xf8) << 7) |
                                   ((pal[1] & 0xf8) << 2) |
                                   ((pal[2] >> 3) & 0x1f)] = 0;
                        }
                        /*
                         * Palette indices never exceed 255 in this loop, so
                         * the explicit cast keeps MSVC quiet while preserving
                         * the existing behavior.
                         */
                        mapped_index = (unsigned char)nextpal;
                        *dst = mapped_index;
                        rgb2pal[pix] = mapped_index;
                        nextpal++;
                        *mptr = 1;
                        palstate[*dst] = PALETTE_CHANGE;
                        palhitcount[*dst] = 1;
                        *(pal++) = pixels[0];
                        *(pal++) = pixels[1];
                        *(pal++) = pixels[2];
                    }
                } else {
                    *dst = rgb2pal[pix];
                    *mptr = 1;
                    if (!palstate[*dst]) {
                        palstate[*dst] = PALETTE_HIT;
                    }
                    if (palhitcount[*dst] < 255) {
                        palhitcount[*dst]++;
                    }
                }
            }
        }

        if (++y >= height) {
            if (dirty) {
                mod_y = 5;
            } else {
                goto end;
            }
        }
        if (dirty && (mod_y == 5 || y >= height)) {
            orig_height = height;

            if (output_count++ == 0) {
                status = sixel_encode_header(width, height, dither->keycolor, output);
                if (SIXEL_FAILED(status)) {
                    goto error;
                }
            }
            height = y;
            status = sixel_encode_body(
                paletted_pixels,
                width,
                height,
                palette_entries,
                NULL,
                255,
                255,
                dither->bodyonly,
                output,
                palstate,
                dither->allocator,
                dither->pipeline_pin_threads,
                NULL);
            if (SIXEL_FAILED(status)) {
                goto error;
            }
            if (y >= orig_height) {
              goto end;
            }
            pixels -= (6 * width * 3);
            height = orig_height - height + 6;
            goto next;
        }

        if (++mod_y == 6) {
            mptr = (unsigned char *)memset(marks, 0, (size_t)(width * 6));
            mod_y = 0;
        }
    }

end:
    if (output_count == 0) {
        status = sixel_encode_header(width, height, dither->keycolor, output);
        if (SIXEL_FAILED(status)) {
            goto error;
        }
    }
    status = sixel_encode_body(
        paletted_pixels,
        width,
        height,
        palette_entries,
        NULL,
        255,
        255,
        dither->bodyonly,
        output,
        palstate,
        dither->allocator,
        dither->pipeline_pin_threads,
        NULL);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_encode_footer(output);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

error:
    if (palette_entries != NULL) {
        sixel_allocator_free(dither->allocator, palette_entries);
    }
    sixel_allocator_free(dither->allocator, paletted_pixels);
    sixel_allocator_free(dither->allocator, normalized_pixels);

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
