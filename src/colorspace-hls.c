/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
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
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <sixel.h>
#include "colorspace.h"

/* Wire HLS arithmetic is shared by the encoder's history model and all
 * decoders, including the standalone Quick Look archive. Keep this unit
 * independent of frame conversion, SIMD, CMS and thread-pool machinery. */
/* Final-output conversion only: callers supply finite, bounded RGB.
 * Share field rounding with delta so its model cannot precede HLS rounding.
 * Keep the historical writer arithmetic for byte-stream compatibility. */
void
sixel_color_rgb_to_hls(int hls[3], double const rgb[3])
{
    double r;
    double g;
    double b;
    double maxc;
    double minc;
    double lightness;
    double saturation;
    double hue;
    double diff;
    int h;
    int l;
    int s;

    r = rgb[0];
    g = rgb[1];
    b = rgb[2];
    maxc = r > g ? (r > b ? r : b) : (g > b ? g : b);
    minc = r < g ? (r < b ? r : b) : (g < b ? g : b);
    lightness = (maxc + minc) * 0.5;
    saturation = 0.0;
    hue = 0.0;
    diff = maxc - minc;
    if (diff <= 0.0) {
        h = 0;
        s = 0;
    } else {
        if (lightness < 0.5) {
            saturation = diff / (maxc + minc);
        } else {
            saturation = diff / (2.0 - maxc - minc);
        }
        if (maxc == r) {
            hue = (g - b) / diff;
        } else if (maxc == g) {
            hue = 2.0 + (b - r) / diff;
        } else {
            hue = 4.0 + (r - g) / diff;
        }
        hue *= 60.0;
        if (hue < 0.0) {
            hue += 360.0;
        }
        if (hue >= 360.0) {
            hue -= 360.0;
        }
        /*
         * The DEC HLS color wheel used by DECGCI considers
         * hue==0 to be blue instead of red.  Rotate the hue by
         * +120 degrees so that RGB primaries continue to match
         * the historical img2sixel output and VT340 behavior.
         */
        hue += 120.0;
        while (hue >= 360.0) {
            hue -= 360.0;
        }
        while (hue < 0.0) {
            hue += 360.0;
        }
        s = (int)(saturation * 100.0 + 0.5);
        if (s < 0) {
            s = 0;
        } else if (s > 100) {
            s = 100;
        }
        h = (int)(hue + 0.5);
        if (h >= 360) {
            h -= 360;
        } else if (h < 0) {
            h = 0;
        }
    }
    l = (int)(lightness * 100.0 + 0.5);
    if (l < 0) {
        l = 0;
    } else if (l > 100) {
        l = 100;
    }
    hls[0] = h;
    hls[1] = l;
    hls[2] = s;
}

static int
sixel_clamp_hls_component(double value)
{
    int channel;

    channel = (int)(value + 0.5);
    if (channel < 0) {
        channel = 0;
    } else if (channel > 100) {
        channel = 100;
    }

    return channel;
}

/* Model HLS using the decoder's established percentage rounding, followed
 * by the caller's RGB888 projection. This is not receiver color readback.
 * Inputs are parsed DEC fields: H in 0..360 and L/S in 0..100. */
void
sixel_color_hls_to_rgb101(unsigned char rgb[3], int const hls[3])
{
    double min, max;
    int r, g, b;
    int hue, lum, sat;

    hue = hls[0];
    lum = hls[1];
    sat = hls[2];
    r = 0;
    g = 0;
    b = 0;

    if (sat == 0) {
        r = g = b = lum;
    }

    max = lum + sat * (1.0 - (lum > 50
        ? (2 * (lum / 100.0) - 1.0)
        : -(2 * (lum / 100.0) - 1.0))) / 2.0;

    min = lum - sat * (1.0 - (lum > 50
        ? (2 * (lum / 100.0) - 1.0)
        : -(2 * (lum / 100.0) - 1.0))) / 2.0;

    /* Rotate the DEC hue origin from blue to the conventional red. */
    hue = (hue + 240) % 360;

    switch (hue / 60) {
    case 0:  /* 0 <= hue < 60 */
        r = sixel_clamp_hls_component(max);
        g = sixel_clamp_hls_component(min + (max - min) * (hue / 60.0));
        b = sixel_clamp_hls_component(min);
        break;
    case 1:  /* 60 <= hue < 120 */
        r = sixel_clamp_hls_component(
                min + (max - min) * ((120 - hue) / 60.0));
        g = sixel_clamp_hls_component(max);
        b = sixel_clamp_hls_component(min);
        break;
    case 2:  /* 120 <= hue < 180 */
        r = sixel_clamp_hls_component(min);
        g = sixel_clamp_hls_component(max);
        b = sixel_clamp_hls_component(
                min + (max - min) * ((hue - 120) / 60.0));
        break;
    case 3:  /* 180 <= hue < 240 */
        r = sixel_clamp_hls_component(min);
        g = sixel_clamp_hls_component(
                min + (max - min) * ((240 - hue) / 60.0));
        b = sixel_clamp_hls_component(max);
        break;
    case 4:  /* 240 <= hue < 300 */
        r = sixel_clamp_hls_component(
                min + (max - min) * ((hue - 240) / 60.0));
        g = sixel_clamp_hls_component(min);
        b = sixel_clamp_hls_component(max);
        break;
    case 5:  /* 300 <= hue < 360 */
        r = sixel_clamp_hls_component(max);
        g = sixel_clamp_hls_component(min);
        b = sixel_clamp_hls_component(
                min + (max - min) * ((360 - hue) / 60.0));
        break;
    default:
#if HAVE___BUILTIN_UNREACHABLE
        __builtin_unreachable();
#endif
        break;
    }

    rgb[0] = (unsigned char)r;
    rgb[1] = (unsigned char)g;
    rgb[2] = (unsigned char)b;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
