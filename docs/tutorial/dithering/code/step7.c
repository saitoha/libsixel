/* SPDX-License-Identifier: MIT */
/*
 * Step 7: Floyd-Steinberg in linear light.
 *
 * The numbers in a picture file are not amounts of light. This step turns
 * them into light first, does all of step 3's arithmetic there, and so keeps
 * the amount of light of an area instead of the sum of its stored numbers.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "tutorial.h"

int const dither_keeps_tone = DITHER_KEEPS_LIGHT;

/* The sRGB curve: a stored number from 0 to 255 as light from 0 to 1. */
static float
stored_to_light(int stored)
{
    double v = stored / 255.0;

    return (float)(v <= 0.04045 ? v / 12.92 : pow((v + 0.055) / 1.055, 2.4));
}

static float
clamp(float value)
{
    return value < 0.0f ? 0.0f : value > 1.0f ? 1.0f : value;
}

int
dither(image_t *out, image_t const *in, palette_t const *pal,
       char const *option)
{
    int w = in->width;
    size_t row_size = ((size_t)w + 2) * 3 * sizeof(float);
    float light[256];
    float pal_light[256][3];
    float *this_row;
    float *next_row;
    float *swap;
    unsigned char const *p = in->rgb;
    float want[3];
    float error;
    float d;
    float distance;
    float best_distance;
    int index;
    int i;
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
    for (i = 0; i < 256; i++) {
        light[i] = stored_to_light(i);
    }
    for (i = 0; i < pal->ncolors; i++) {
        for (c = 0; c < 3; c++) {
            pal_light[i][c] = light[pal->rgb[i][c]];
        }
    }
    for (y = 0; y < in->height; y++) {
        for (x = 0; x < w; x++, p += 3) {
            k = (x + 1) * 3;
            for (c = 0; c < 3; c++) {
                want[c] = clamp(light[p[c]] + this_row[k + c]);
            }
            /* The nearest color, measured in light as well. */
            index = 0;
            best_distance = 1e30f;
            for (i = 0; i < pal->ncolors; i++) {
                distance = 0.0f;
                for (c = 0; c < 3; c++) {
                    d = want[c] - pal_light[i][c];
                    distance += d * d;
                }
                if (distance < best_distance) {
                    best_distance = distance;
                    index = i;
                }
            }
            put_color(out, y * w + x, pal, index);
            for (c = 0; c < 3; c++) {
                error = want[c] - pal_light[index][c];
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
