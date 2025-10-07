/*
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
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

#include <math.h>
#include <stddef.h>

#include <sixel.h>

#include "colorspace.h"

#define SIXEL_COLORSPACE_LUT_SIZE 256

static unsigned char gamma_to_linear_lut[SIXEL_COLORSPACE_LUT_SIZE];
static unsigned char linear_to_gamma_lut[SIXEL_COLORSPACE_LUT_SIZE];
static int tables_initialized = 0;

static unsigned char
sixel_colorspace_clamp(int value)
{
    if (value < 0) {
        return 0;
    }
    if (value > 255) {
        return 255;
    }
    return (unsigned char)value;
}

static void
sixel_colorspace_init_tables(void)
{
    int i;
    double gamma_value;
    double linear_value;
    double converted;

    if (tables_initialized) {
        return;
    }

    for (i = 0; i < SIXEL_COLORSPACE_LUT_SIZE; ++i) {
        gamma_value = (double)i / 255.0;
        if (gamma_value <= 0.04045) {
            converted = gamma_value / 12.92;
        } else {
            converted = pow((gamma_value + 0.055) / 1.055, 2.4);
        }
        gamma_to_linear_lut[i] =
            sixel_colorspace_clamp((int)(converted * 255.0 + 0.5));
    }

    for (i = 0; i < SIXEL_COLORSPACE_LUT_SIZE; ++i) {
        linear_value = (double)i / 255.0;
        if (linear_value <= 0.0031308) {
            converted = linear_value * 12.92;
        } else {
            converted = 1.055 * pow(linear_value, 1.0 / 2.4) - 0.055;
        }
        linear_to_gamma_lut[i] =
            sixel_colorspace_clamp((int)(converted * 255.0 + 0.5));
    }

    tables_initialized = 1;
}

static unsigned char
sixel_colorspace_convert_component(unsigned char value,
                                   int colorspace_src,
                                   int colorspace_dst)
{
    if (colorspace_src == colorspace_dst) {
        return value;
    }

    if (colorspace_src == SIXEL_COLORSPACE_GAMMA &&
            colorspace_dst == SIXEL_COLORSPACE_LINEAR) {
        return gamma_to_linear_lut[value];
    }

    if (colorspace_src == SIXEL_COLORSPACE_LINEAR &&
            colorspace_dst == SIXEL_COLORSPACE_GAMMA) {
        return linear_to_gamma_lut[value];
    }

    return value;
}

int
sixel_colorspace_supports_pixelformat(int pixelformat)
{
    switch (pixelformat) {
    case SIXEL_PIXELFORMAT_RGB888:
    case SIXEL_PIXELFORMAT_BGR888:
    case SIXEL_PIXELFORMAT_RGBA8888:
    case SIXEL_PIXELFORMAT_ARGB8888:
    case SIXEL_PIXELFORMAT_BGRA8888:
    case SIXEL_PIXELFORMAT_ABGR8888:
    case SIXEL_PIXELFORMAT_G8:
    case SIXEL_PIXELFORMAT_GA88:
    case SIXEL_PIXELFORMAT_AG88:
        return 1;
    default:
        break;
    }

    return 0;
}

SIXELAPI SIXELSTATUS
sixel_helper_convert_colorspace(unsigned char *pixels,
                                size_t size,
                                int pixelformat,
                                int colorspace_src,
                                int colorspace_dst)
{
    size_t i;

    if (pixels == NULL) {
        sixel_helper_set_additional_message(
            "sixel_helper_convert_colorspace: pixels is null.");
        return SIXEL_BAD_ARGUMENT;
    }

    if (colorspace_src == colorspace_dst) {
        return SIXEL_OK;
    }

    if (!sixel_colorspace_supports_pixelformat(pixelformat)) {
        sixel_helper_set_additional_message(
            "sixel_helper_convert_colorspace: unsupported pixelformat.");
        return SIXEL_BAD_INPUT;
    }

    sixel_colorspace_init_tables();

    switch (pixelformat) {
    case SIXEL_PIXELFORMAT_RGB888:
        if (size % 3 != 0) {
            sixel_helper_set_additional_message(
                "sixel_helper_convert_colorspace: invalid data size.");
            return SIXEL_BAD_INPUT;
        }
        for (i = 0; i + 2 < size; i += 3) {
            pixels[i + 0] = sixel_colorspace_convert_component(
                pixels[i + 0], colorspace_src, colorspace_dst);
            pixels[i + 1] = sixel_colorspace_convert_component(
                pixels[i + 1], colorspace_src, colorspace_dst);
            pixels[i + 2] = sixel_colorspace_convert_component(
                pixels[i + 2], colorspace_src, colorspace_dst);
        }
        break;
    case SIXEL_PIXELFORMAT_BGR888:
        if (size % 3 != 0) {
            sixel_helper_set_additional_message(
                "sixel_helper_convert_colorspace: invalid data size.");
            return SIXEL_BAD_INPUT;
        }
        for (i = 0; i + 2 < size; i += 3) {
            pixels[i + 0] = sixel_colorspace_convert_component(
                pixels[i + 0], colorspace_src, colorspace_dst);
            pixels[i + 1] = sixel_colorspace_convert_component(
                pixels[i + 1], colorspace_src, colorspace_dst);
            pixels[i + 2] = sixel_colorspace_convert_component(
                pixels[i + 2], colorspace_src, colorspace_dst);
        }
        break;
    case SIXEL_PIXELFORMAT_RGBA8888:
        if (size % 4 != 0) {
            sixel_helper_set_additional_message(
                "sixel_helper_convert_colorspace: invalid data size.");
            return SIXEL_BAD_INPUT;
        }
        for (i = 0; i + 3 < size; i += 4) {
            pixels[i + 0] = sixel_colorspace_convert_component(
                pixels[i + 0], colorspace_src, colorspace_dst);
            pixels[i + 1] = sixel_colorspace_convert_component(
                pixels[i + 1], colorspace_src, colorspace_dst);
            pixels[i + 2] = sixel_colorspace_convert_component(
                pixels[i + 2], colorspace_src, colorspace_dst);
        }
        break;
    case SIXEL_PIXELFORMAT_ARGB8888:
        if (size % 4 != 0) {
            sixel_helper_set_additional_message(
                "sixel_helper_convert_colorspace: invalid data size.");
            return SIXEL_BAD_INPUT;
        }
        for (i = 0; i + 3 < size; i += 4) {
            pixels[i + 1] = sixel_colorspace_convert_component(
                pixels[i + 1], colorspace_src, colorspace_dst);
            pixels[i + 2] = sixel_colorspace_convert_component(
                pixels[i + 2], colorspace_src, colorspace_dst);
            pixels[i + 3] = sixel_colorspace_convert_component(
                pixels[i + 3], colorspace_src, colorspace_dst);
        }
        break;
    case SIXEL_PIXELFORMAT_BGRA8888:
        if (size % 4 != 0) {
            sixel_helper_set_additional_message(
                "sixel_helper_convert_colorspace: invalid data size.");
            return SIXEL_BAD_INPUT;
        }
        for (i = 0; i + 3 < size; i += 4) {
            pixels[i + 0] = sixel_colorspace_convert_component(
                pixels[i + 0], colorspace_src, colorspace_dst);
            pixels[i + 1] = sixel_colorspace_convert_component(
                pixels[i + 1], colorspace_src, colorspace_dst);
            pixels[i + 2] = sixel_colorspace_convert_component(
                pixels[i + 2], colorspace_src, colorspace_dst);
        }
        break;
    case SIXEL_PIXELFORMAT_ABGR8888:
        if (size % 4 != 0) {
            sixel_helper_set_additional_message(
                "sixel_helper_convert_colorspace: invalid data size.");
            return SIXEL_BAD_INPUT;
        }
        for (i = 0; i + 3 < size; i += 4) {
            pixels[i + 1] = sixel_colorspace_convert_component(
                pixels[i + 1], colorspace_src, colorspace_dst);
            pixels[i + 2] = sixel_colorspace_convert_component(
                pixels[i + 2], colorspace_src, colorspace_dst);
            pixels[i + 3] = sixel_colorspace_convert_component(
                pixels[i + 3], colorspace_src, colorspace_dst);
        }
        break;
    case SIXEL_PIXELFORMAT_G8:
        for (i = 0; i < size; ++i) {
            pixels[i] = sixel_colorspace_convert_component(
                pixels[i], colorspace_src, colorspace_dst);
        }
        break;
    case SIXEL_PIXELFORMAT_GA88:
        if (size % 2 != 0) {
            sixel_helper_set_additional_message(
                "sixel_helper_convert_colorspace: invalid data size.");
            return SIXEL_BAD_INPUT;
        }
        for (i = 0; i + 1 < size; i += 2) {
            pixels[i + 0] = sixel_colorspace_convert_component(
                pixels[i + 0], colorspace_src, colorspace_dst);
        }
        break;
    case SIXEL_PIXELFORMAT_AG88:
        if (size % 2 != 0) {
            sixel_helper_set_additional_message(
                "sixel_helper_convert_colorspace: invalid data size.");
            return SIXEL_BAD_INPUT;
        }
        for (i = 0; i + 1 < size; i += 2) {
            pixels[i + 1] = sixel_colorspace_convert_component(
                pixels[i + 1], colorspace_src, colorspace_dst);
        }
        break;
    default:
        sixel_helper_set_additional_message(
            "sixel_helper_convert_colorspace: unsupported pixelformat.");
        return SIXEL_BAD_INPUT;
    }

    return SIXEL_OK;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
