/* SPDX-License-Identifier: MIT */
/*
 * Step 8: the error as whole numbers, with limits you can choose.
 *
 *   step8 IN PAL [KERNEL][,serpentine][,clamp=soft|hard|none][,deadzone=N]
 *
 * An error row holds numerators over the kernel's divisor, so no share is
 * ever divided or rounded on its way. The sum at a pixel is therefore the
 * same on every computer and in every order of addition. The default is
 * fs,clamp=soft,deadzone=0.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "tutorial.h"

int const dither_keeps_tone = DITHER_KEEPS_NUMBERS;

enum { CLAMP_SOFT, CLAMP_HARD, CLAMP_NONE };

typedef struct tap {
    int dx;
    int dy;
    int weight;
} tap_t;

typedef struct kernel {
    char const *name;
    int divisor;
    int ntaps;
    tap_t taps[12];
} kernel_t;

typedef struct settings {
    kernel_t const *kernel;
    int serpentine;
    int clamp;
    int deadzone;           /* in levels; 0 turns it off */
} settings_t;

static kernel_t const kernels[] = {
    {"fs", 16, 4, {{1, 0, 7}, {-1, 1, 3}, {0, 1, 5}, {1, 1, 1}}},
    {"atkinson", 8, 6, {{1, 0, 1}, {2, 0, 1}, {-1, 1, 1}, {0, 1, 1},
                        {1, 1, 1}, {0, 2, 1}}},
    {"jajuni", 48, 12, {{1, 0, 7}, {2, 0, 5}, {-2, 1, 3}, {-1, 1, 5},
                        {0, 1, 7}, {1, 1, 5}, {2, 1, 3}, {-2, 2, 1},
                        {-1, 2, 3}, {0, 2, 5}, {1, 2, 3}, {2, 2, 1}}},
};

/* Read one comma-separated word of the option. */
static int
parse_word(char const *word, size_t length, settings_t *s)
{
    size_t i;
    char *end;

    if (length == 10 && strncmp(word, "serpentine", length) == 0) {
        s->serpentine = 1;
        return 0;
    }
    if (length == 10 && strncmp(word, "clamp=soft", length) == 0) {
        s->clamp = CLAMP_SOFT;
        return 0;
    }
    if (length == 10 && strncmp(word, "clamp=hard", length) == 0) {
        s->clamp = CLAMP_HARD;
        return 0;
    }
    if (length == 10 && strncmp(word, "clamp=none", length) == 0) {
        s->clamp = CLAMP_NONE;
        return 0;
    }
    if (length > 9 && strncmp(word, "deadzone=", 9) == 0) {
        s->deadzone = (int)strtol(word + 9, &end, 10);
        return end != word + length || s->deadzone < 0 || s->deadzone > 255;
    }
    for (i = 0; i < sizeof(kernels) / sizeof(kernels[0]); i++) {
        if (strlen(kernels[i].name) == length &&
                strncmp(word, kernels[i].name, length) == 0) {
            s->kernel = &kernels[i];
            return 0;
        }
    }
    return 1;
}

static int
parse_option(char const *option, settings_t *s)
{
    size_t length;

    s->kernel = &kernels[0];
    s->serpentine = 0;
    s->clamp = CLAMP_SOFT;
    s->deadzone = 0;
    while (option != NULL && *option != '\0') {
        length = strcspn(option, ",");
        if (parse_word(option, length, s) != 0) {
            return 1;
        }
        option += length;
        option += *option == ',';
    }
    return 0;
}

/* The nearest whole number to n / d, for d > 0. A tie goes to the even
 * neighbor, so that rounding leans neither up nor down on average. */
static int64_t
round_div(int64_t n, int64_t d)
{
    int64_t q = n / d;
    int64_t r = n % d;

    if (r < 0) {            /* C division truncates: make it a floor */
        r += d;
        q--;
    }
    if (2 * r > d || (2 * r == d && (q & 1))) {
        q++;
    }
    return q;
}

/* How much of the error to add, as the fraction *b / *c. The fraction is the
 * largest that keeps every channel of source * d + carry inside [low, high].
 * One fraction for all channels keeps the direction of the error, and so its
 * hue. Fractions are compared by cross multiplication, without rounding. */
static void
soft_fraction(unsigned char const *source, int64_t const *carry, int d,
              int64_t low, int64_t high, int64_t *b, int64_t *c)
{
    int64_t room;
    int64_t size;
    int64_t value;
    int n;

    *b = *c = 1;
    for (n = 0; n < 3; n++) {
        value = source[n] * (int64_t)d + carry[n];
        if (value < low) {
            room = source[n] * (int64_t)d - low;
            size = -carry[n];
        } else if (value > high) {
            room = high - source[n] * (int64_t)d;
            size = carry[n];
        } else {
            continue;
        }
        if (room * *c < *b * size) {
            *b = room;
            *c = size;
        }
    }
}

/* Turn a source pixel and its waiting error into the color to look up. */
static void
candidate(unsigned char const *source, int64_t const *carry, int d,
          settings_t const *s, int *want)
{
    int64_t radius = (int64_t)s->deadzone * d;
    int64_t norm = 0;
    int64_t margin = round_div(255 * (int64_t)d, 100);  /* 1% headroom */
    int64_t value;
    int64_t b;
    int64_t c;
    int small = 1;
    int n;

    /* Testing each channel first keeps the squares from overflowing. */
    for (n = 0; n < 3; n++) {
        small = small && llabs(carry[n]) < radius;
    }
    if (small) {
        for (n = 0; n < 3; n++) {
            norm += carry[n] * carry[n];
        }
        if (norm < radius * radius) {
            for (n = 0; n < 3; n++) {
                want[n] = source[n];    /* too small to act on: drop it */
            }
            return;
        }
    }
    if (s->clamp == CLAMP_SOFT) {
        soft_fraction(source, carry, d, -margin, 255 * d + margin, &b, &c);
    } else {
        b = c = 1;
    }
    for (n = 0; n < 3; n++) {
        value = source[n] * (int64_t)d * c + carry[n] * b;
        if (s->clamp == CLAMP_HARD) {
            value = value < 0 ? 0 : value > 255 * (int64_t)d ? 255 * d : value;
        }
        want[n] = (int)round_div(value, d * c);
    }
}

static int
in_range(int value)
{
    return value < 0 ? 0 : value > 255 ? 255 : value;
}

int
dither(image_t *out, image_t const *in, palette_t const *pal,
       char const *option)
{
    enum { MARGIN = 2, ROWS = 3 };
    int w = in->width;
    settings_t s;
    size_t row_size = ((size_t)w + 2 * MARGIN) * 3 * sizeof(int64_t);
    int64_t *rows[ROWS];
    int64_t *swap;
    unsigned char const *p;
    tap_t const *tap;
    int want[3];
    int reverse;
    int index;
    int error;
    int target;
    int x;
    int y;
    int i;
    int c;
    int t;

    if (parse_option(option, &s) != 0) {
        return 1;
    }
    for (i = 0; i < ROWS; i++) {
        rows[i] = calloc(1, row_size);
    }
    if (rows[0] == NULL || rows[1] == NULL || rows[2] == NULL) {
        for (i = 0; i < ROWS; i++) {
            free(rows[i]);
        }
        return 1;
    }
    for (y = 0; y < in->height; y++) {
        reverse = s.serpentine && (y & 1);
        for (i = 0; i < w; i++) {
            x = reverse ? w - 1 - i : i;
            p = in->rgb + ((size_t)y * (size_t)w + (size_t)x) * 3;
            candidate(p, rows[0] + (x + MARGIN) * 3, s.kernel->divisor, &s,
                      want);
            index = nearest_color(pal, in_range(want[0]), in_range(want[1]),
                                  in_range(want[2]));
            put_color(out, y * w + x, pal, index);
            for (c = 0; c < 3; c++) {
                error = want[c] - pal->rgb[index][c];
                for (t = 0; t < s.kernel->ntaps; t++) {
                    tap = &s.kernel->taps[t];
                    target = (x + (reverse ? -tap->dx : tap->dx) + MARGIN) * 3;
                    rows[tap->dy][target + c] += error * tap->weight;
                }
            }
        }
        swap = rows[0];
        rows[0] = rows[1];
        rows[1] = rows[2];
        rows[2] = swap;
        memset(rows[2], 0, row_size);
    }
    for (i = 0; i < ROWS; i++) {
        free(rows[i]);
    }
    return 0;
}
