/* SPDX-License-Identifier: MIT */
/*
 * Your own dither. Replace the body of dither() lesson by lesson, and check
 * it with `make mydither && ./mydither selftest`.
 */
#include "tutorial.h"

/* Set this to 1 once your dither keeps the average tone of an area. */
int const dither_keeps_tone = 0;

int
dither(image_t *out, image_t const *in, palette_t const *pal,
       char const *option)
{
    int n = in->width * in->height;
    int i;

    (void)option;
    (void)pal;
    /* Lesson 1: for each pixel, find the nearest palette color with
     * nearest_color() and store it with put_color(). As it stands, every
     * pixel gets palette color 0. */
    for (i = 0; i < n; i++) {
        put_color(out, i, pal, 0);
    }
    return 0;
}
