/* SPDX-License-Identifier: MIT */
/*
 * Step 3: Floyd-Steinberg error diffusion.
 *
 * What a pixel could not show is handed on to the pixels not yet visited:
 *
 *           *   7
 *       3   5   1      (sixteenths)
 *
 * The error of every pixel survives, so the tone of an area stays right.
 */
#include <stdlib.h>
#include <string.h>

#include "tutorial.h"

int const dither_keeps_tone = 1;

static float
clamp(float value)
{
    return value < 0.0f ? 0.0f : value > 255.0f ? 255.0f : value;
}

int
dither(image_t *out, image_t const *in, palette_t const *pal,
       char const *option)
{
    int w = in->width;
    size_t row_size = ((size_t)w + 2) * 3 * sizeof(float);
    float *this_row;        /* error waiting for the current row */
    float *next_row;        /* error waiting for the row below */
    float *swap;
    unsigned char const *p = in->rgb;
    float want;
    float error;
    int index;
    int rgb[3];
    int x;
    int y;
    int c;
    int k;

    if (option != NULL) {
        return 1;
    }
    this_row = calloc(1, row_size);
    next_row = calloc(1, row_size);
    if (this_row == NULL || next_row == NULL) {
        free(this_row);
        free(next_row);
        return 1;
    }
    for (y = 0; y < in->height; y++) {
        for (x = 0; x < w; x++, p += 3) {
            /* column x is element x + 1, so x - 1 and x + 1 always exist */
            k = (x + 1) * 3;
            for (c = 0; c < 3; c++) {
                rgb[c] = (int)(clamp(p[c] + this_row[k + c]) + 0.5f);
            }
            index = nearest_color(pal, rgb[0], rgb[1], rgb[2]);
            put_color(out, y * w + x, pal, index);
            for (c = 0; c < 3; c++) {
                want = clamp(p[c] + this_row[k + c]);
                error = want - (float)pal->rgb[index][c];
                this_row[k + 3 + c] += error * 7.0f / 16.0f;
                next_row[k - 3 + c] += error * 3.0f / 16.0f;
                next_row[k + c] += error * 5.0f / 16.0f;
                next_row[k + 3 + c] += error * 1.0f / 16.0f;
            }
        }
        swap = this_row;
        this_row = next_row;
        next_row = swap;
        memset(next_row, 0, row_size);
    }
    free(this_row);
    free(next_row);
    return 0;
}
