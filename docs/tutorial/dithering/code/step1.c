/* SPDX-License-Identifier: MIT */
/*
 * Step 1: the nearest color, and nothing else.
 *
 * Each pixel independently takes the closest palette color. Smooth shading
 * falls apart into flat steps: banding.
 */
#include "tutorial.h"

int const dither_keeps_tone = 0;

int
dither(image_t *out, image_t const *in, palette_t const *pal,
       char const *option)
{
    unsigned char const *p = in->rgb;
    int n = in->width * in->height;
    int i;

    if (option != NULL) {
        return 1;
    }
    for (i = 0; i < n; i++, p += 3) {
        put_color(out, i, pal, nearest_color(pal, p[0], p[1], p[2]));
    }
    return 0;
}
