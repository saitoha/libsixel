/* SPDX-License-Identifier: MIT */
/*
 * Shared definitions for the step-by-step SIXEL codec tutorial.
 *
 * Every step implements the same two functions on the same indexed image,
 * so any encoder can be checked against any decoder. Pixels are palette
 * indexes; the harness in common.c converts PPM files to and from them.
 */
#ifndef SIXEL_TUTORIAL_H
#define SIXEL_TUTORIAL_H

#include <stddef.h>
#include <stdint.h>

#define TUTORIAL_MAX_SIDE 32768

typedef struct image {
    int width;
    int height;
    int ncolors;                    /* registers in use, 1 to 256 */
    unsigned char *pixels;          /* width * height palette indexes */
    unsigned char palette[256 * 3]; /* 8-bit RGB triplets */
} image_t;

typedef struct buffer {
    unsigned char *data;
    size_t size;
    size_t capacity;
} buffer_t;

/* Implemented once per step. Both return 0 on success. */
int tutorial_encode(image_t const *img, buffer_t *out);
int tutorial_decode(unsigned char const *data, size_t size, image_t *img);

/* Image helpers. image_alloc() fills the pixels with register 0. */
int image_alloc(image_t *img, int width, int height);
void image_free(image_t *img);

/* SIXEL color coordinates are percentages (0-100). */
#define PCT(v) (((v) * 100 + 127) / 255)
void set_rgb_pct(image_t *img, int color, int r, int g, int b);
void set_hls(image_t *img, int color, int h, int l, int s);

/* Read up to max numeric parameters separated by ';'. Missing values are
 * zero. Returns the number of parameters present, at most max. */
int parse_params(unsigned char const **p, unsigned char const *end,
                 int *params, int max);

/* Growable output buffer. */
void buf_reserve(buffer_t *b, size_t extra);
void buf_printf(buffer_t *b, char const *fmt, ...);
void buf_free(buffer_t *b);

static inline void
buf_putc(buffer_t *b, int c)
{
    if (b->size == b->capacity) {
        buf_reserve(b, 1);
    }
    b->data[b->size++] = (unsigned char)c;
}

static inline void
buf_putnum(buffer_t *b, int n)
{
    char digits[12];
    int i = 0;

    do {
        digits[i++] = (char)('0' + n % 10);
        n /= 10;
    } while (n > 0);
    buf_reserve(b, (size_t)i);
    while (i > 0) {
        b->data[b->size++] = (unsigned char)digits[--i];
    }
}

/* Emit `count` copies of a sixel character, using DECGRI ('!') when the
 * repeat form is shorter. Shared by steps 2 to 5 so their output matches. */
static inline void
buf_putrun(buffer_t *b, int ch, int count)
{
    if (count > 3) {
        buf_putc(b, '!');
        buf_putnum(b, count);
        buf_putc(b, ch);
    } else {
        while (count-- > 0) {
            buf_putc(b, ch);
        }
    }
}

/* Header and palette shared by every encoder. */
void put_header_and_palette(image_t const *img, buffer_t *out);

#endif /* SIXEL_TUTORIAL_H */
