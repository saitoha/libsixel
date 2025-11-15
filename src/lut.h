/*
 * Copyright (c) 2024 libsixel developers
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
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef LIBSIXEL_LUT_H
#define LIBSIXEL_LUT_H

#include <sixel.h>

#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

static unsigned char const sixel_safe_tones[256] = {
    0,   0,   3,   3,   5,   5,   5,   8,   8,   10,  10,  10,  13,  13,  13,
    15,  15,  18,  18,  18,  20,  20,  23,  23,  23,  26,  26,  28,  28,  28,
    31,  31,  33,  33,  33,  36,  36,  38,  38,  38,  41,  41,  41,  43,  43,
    46,  46,  46,  48,  48,  51,  51,  51,  54,  54,  56,  56,  56,  59,  59,
    61,  61,  61,  64,  64,  64,  66,  66,  69,  69,  69,  71,  71,  74,  74,
    74,  77,  77,  79,  79,  79,  82,  82,  84,  84,  84,  87,  87,  89,  89,
    89,  92,  92,  92,  94,  94,  97,  97,  97,  99,  99,  102, 102, 102, 105,
    105, 107, 107, 107, 110, 110, 112, 112, 112, 115, 115, 115, 117, 117, 120,
    120, 120, 122, 122, 125, 125, 125, 128, 128, 130, 130, 130, 133, 133, 135,
    135, 135, 138, 138, 140, 140, 140, 143, 143, 143, 145, 145, 148, 148, 148,
    150, 150, 153, 153, 153, 156, 156, 158, 158, 158, 161, 161, 163, 163, 163,
    166, 166, 166, 168, 168, 171, 171, 171, 173, 173, 176, 176, 176, 179, 179,
    181, 181, 181, 184, 184, 186, 186, 186, 189, 189, 191, 191, 191, 194, 194,
    194, 196, 196, 199, 199, 199, 201, 201, 204, 204, 204, 207, 207, 209, 209,
    209, 212, 212, 214, 214, 214, 217, 217, 217, 219, 219, 222, 222, 222, 224,
    224, 227, 227, 227, 230, 230, 232, 232, 232, 235, 235, 237, 237, 237, 240,
    240, 242, 242, 242, 245, 245, 245, 247, 247, 250, 250, 250, 252, 252, 255,
    255
};

#define sixel_palette_reversible_value(sample) \
    sixel_safe_tones[sample >= 255 ? 255: sample];

typedef struct sixel_lut sixel_lut_t;

SIXELSTATUS
sixel_lut_new(sixel_lut_t **out,
              int policy,
              sixel_allocator_t *allocator);

void
sixel_lut_unref(sixel_lut_t *lut);

SIXELSTATUS
sixel_lut_configure(sixel_lut_t *lut,
                    unsigned char const *palette,
                    int depth,
                    int ncolors,
                    int complexion,
                    int wR,
                    int wG,
                    int wB,
                    int policy);

/* lookup */
int
sixel_lut_map_pixel(sixel_lut_t *lut, unsigned char const *pixel);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_LUT_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
