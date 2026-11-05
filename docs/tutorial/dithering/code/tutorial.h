/* SPDX-License-Identifier: MIT */
/*
 * Shared definitions of the dithering tutorial.
 *
 * Every step is one function, dither(). Reading and writing pictures and
 * palettes live in common.c, the command line and the self test in main.c.
 * None of this uses libsixel.
 */
#ifndef DITHER_TUTORIAL_H
#define DITHER_TUTORIAL_H

#include <stddef.h>

typedef struct image {
    int width;
    int height;
    unsigned char *rgb;     /* width * height * 3 bytes, row after row */
} image_t;

typedef struct palette {
    int ncolors;            /* 1 to 256 */
    unsigned char rgb[256][3];
} palette_t;

/* Written by each step. Fill out, which already has the size of in, with
 * colors of pal. The option is the optional last word of the command line,
 * NULL if there is none. Return 0, or nonzero for an unknown option. */
int dither(image_t *out, image_t const *in, palette_t const *pal,
           char const *option);

/* Written by each step: does this step keep the average tone of a flat area
 * with its default option? The self test holds it to the answer; a step that
 * answers no must be the plain nearest color. */
extern int const dither_keeps_tone;

/* The index of the palette color closest to (r, g, b). */
int nearest_color(palette_t const *pal, int r, int g, int b);

/* The typical distance between neighboring colors of a palette, in levels. */
double palette_gap(palette_t const *pal);

/* Copy palette color index to pixel p of out. */
void put_color(image_t *out, int p, palette_t const *pal, int index);

/* Allocate and free pictures. image_new returns 0 on success. */
int image_new(image_t *img, int width, int height);
void image_free(image_t *img);

/* Read a PNM picture (P2, P3, P5 or P6); NULL path means standard input. */
int image_read(image_t *img, char const *path);
/* Write a binary P6 picture to standard output. */
int image_write(image_t const *img);
/* Every pixel of a picture is a palette entry, in reading order. */
int palette_read(palette_t *pal, char const *path);
int palette_from_image(palette_t *pal, image_t const *img);

/* The usual gradient of the lessons, and a flat area. */
void image_gradient(image_t *img, int r0, int g0, int b0,
                    int r1, int g1, int b1);
void image_flat(image_t *img, int r, int g, int b);

/* A palette of a cube with this many levels per channel, or of grays. */
void palette_cube(palette_t *pal, int levels);
void palette_gray(palette_t *pal, int levels);

#endif /* DITHER_TUTORIAL_H */
