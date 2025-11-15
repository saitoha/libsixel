/*
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "config.h"

#include <string.h>

#include "dither-pattern.h"
#include "dither-common-pipeline.h"

/*
 * Helper configuring serpentine row traversal.  Keeping the helper local to
 * this translation unit avoids coupling pattern dithering to other
 * implementations while still documenting the walk order.
 */
static void
sixel_dither_scanline_params(int serpentine,
                             int index,
                             int limit,
                             int *start,
                             int *end,
                             int *step,
                             int *direction)
{
    if (serpentine && (index & 1)) {
        *start = limit - 1;
        *end = -1;
        *step = -1;
        *direction = -1;
    } else {
        *start = 0;
        *end = limit;
        *step = 1;
        *direction = 1;
    }
}

static float mask_a(int x, int y, int c);
static float mask_x(int x, int y, int c);

/*
 * Procedural masks used by ordered/pattern dithering.  Each channel uses a
 * unique scramble to avoid repeating patterns when walking serpentine rows.
 */
static float
mask_a(int x, int y, int c)
{
    return ((((x + c * 67) + y * 236) * 119) & 255) / 128.0f - 1.0f;
}

static float
mask_x(int x, int y, int c)
{
    return ((((x + c * 29) ^ (y * 149)) * 1234) & 511) / 256.0f - 1.0f;
}

SIXELSTATUS
sixel_dither_apply_positional(sixel_index_t *result,
                              unsigned char *data,
                              int width,
                              int height,
                              int depth,
                              unsigned char *palette,
                              int reqcolor,
                              int method_for_diffuse,
                              int method_for_scan,
                              int optimize_palette,
                              int (*f_lookup)(const unsigned char *pixel,
                                              int depth,
                                              const unsigned char *palette,
                                              int reqcolor,
                                              unsigned short *cachetable,
                                              int complexion),
                              unsigned short *indextable,
                              int complexion,
                              unsigned char copy[],
                              unsigned char new_palette[],
                              unsigned short migration_map[],
                              int *ncolors,
                              sixel_dither_t *dither)
{
    int serpentine;
    int y;
    float (*f_mask)(int x, int y, int c);

    switch (method_for_diffuse) {
    case SIXEL_DIFFUSE_A_DITHER:
        f_mask = mask_a;
        break;
    case SIXEL_DIFFUSE_X_DITHER:
    default:
        f_mask = mask_x;
        break;
    }

    serpentine = (method_for_scan == SIXEL_SCAN_SERPENTINE);

    if (optimize_palette) {
        int x;

        *ncolors = 0;
        memset(new_palette, 0x00,
               (size_t)SIXEL_PALETTE_MAX * (size_t)depth);
        memset(migration_map, 0x00,
               sizeof(unsigned short) * (size_t)SIXEL_PALETTE_MAX);
        for (y = 0; y < height; ++y) {
            int start;
            int end;
            int step;
            int direction;

            sixel_dither_scanline_params(serpentine, y, width,
                                         &start, &end, &step, &direction);
            (void)direction;
            for (x = start; x != end; x += step) {
                int pos;
                int d;
                int color_index;

                pos = y * width + x;
                for (d = 0; d < depth; ++d) {
                    int val;

                    val = data[pos * depth + d]
                        + (int)(f_mask(x, y, d) * 32.0f);
                    copy[d] = val < 0 ? 0
                               : val > 255 ? 255 : val;
                }
                color_index = f_lookup(copy, depth, palette,
                                       reqcolor, indextable,
                                       complexion);
                if (migration_map[color_index] == 0) {
                    result[pos] = *ncolors;
                    for (d = 0; d < depth; ++d) {
                        new_palette[*ncolors * depth + d]
                            = palette[color_index * depth + d];
                    }
                    ++*ncolors;
                    migration_map[color_index] = *ncolors;
                } else {
                    result[pos] = migration_map[color_index] - 1;
                }
            }
            sixel_dither_pipeline_row_notify(dither, y);
        }
        memcpy(palette, new_palette, (size_t)(*ncolors * depth));
    } else {
        int x;

        for (y = 0; y < height; ++y) {
            int start;
            int end;
            int step;
            int direction;

            sixel_dither_scanline_params(serpentine, y, width,
                                         &start, &end, &step, &direction);
            (void)direction;
            for (x = start; x != end; x += step) {
                int pos;
                int d;

                pos = y * width + x;
                for (d = 0; d < depth; ++d) {
                    int val;

                    val = data[pos * depth + d]
                        + (int)(f_mask(x, y, d) * 32.0f);
                    copy[d] = val < 0 ? 0
                               : val > 255 ? 255 : val;
                }
                result[pos] = f_lookup(copy, depth, palette,
                                       reqcolor, indextable,
                                       complexion);
            }
            sixel_dither_pipeline_row_notify(dither, y);
        }
        *ncolors = reqcolor;
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
