/* SPDX-License-Identifier: MIT */
/*
 * Step 2: nudge each pixel by something that depends only on its position.
 *
 *   step2 IN PAL           the 8x8 Bayer pattern
 *   step2 IN PAL random    a random nudge per pixel, for comparison
 *
 * Averaged over an area, the nudges let a flat color land on two palette
 * colors in the right proportion, so the steps of step 1 dissolve. No pixel
 * looks at any other, so this could run on any number of threads.
 */
#include <stdint.h>
#include <string.h>

#include "tutorial.h"

int const dither_keeps_tone = 1;

/* A number from 0 to 63 that visits neighboring cells as late as possible.
 * Each pair of coordinate bits adds one level of the recursive 2x2 pattern
 * 0 2
 * 3 1 */
static int
bayer(int x, int y)
{
    int value = 0;
    int i;

    for (i = 0; i < 3; i++) {
        value = value << 2 | ((x ^ y) >> i & 1) << 1 | (y >> i & 1);
    }
    return value;
}

/* A small repeatable random generator (xorshift32), from 0 to 63.99. */
static double
random_cell(uint32_t *state)
{
    uint32_t s = *state;

    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    *state = s;
    return (double)(s >> 8) / (double)(1u << 24) * 64.0;
}

static int
clamp(double value)
{
    return value < 0.0 ? 0 : value > 255.0 ? 255 : (int)(value + 0.5);
}

int
dither(image_t *out, image_t const *in, palette_t const *pal,
       char const *option)
{
    unsigned char const *p = in->rgb;
    double gap = palette_gap(pal);
    uint32_t state = 2463534242u;
    int use_random = 0;
    double cell;
    double nudge;
    int x;
    int y;

    if (option != NULL) {
        if (strcmp(option, "random") != 0) {
            return 1;
        }
        use_random = 1;
    }
    for (y = 0; y < in->height; y++) {
        for (x = 0; x < in->width; x++, p += 3) {
            cell = use_random ? random_cell(&state) : bayer(x, y);
            nudge = ((cell + 0.5) / 64.0 - 0.5) * gap;
            put_color(out, y * in->width + x, pal,
                      nearest_color(pal, clamp(p[0] + nudge),
                                    clamp(p[1] + nudge),
                                    clamp(p[2] + nudge)));
        }
    }
    return 0;
}
