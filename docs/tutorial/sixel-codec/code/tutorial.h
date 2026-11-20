/* SPDX-License-Identifier: MIT */
/*
 * Shared definitions for the step-by-step SIXEL codec tutorial.
 *
 * Every step implements the same two functions on the same indexed image,
 * so any encoder can be checked against any decoder. Pixels are palette
 * indexes, one byte each, row by row: exactly the pixel data of a P5 PGM
 * file. common.c supplies the command-line program around the two
 * functions and converts PNM files to and from indexed images. Everything
 * that reads or writes SIXEL lives in the chapter files, so each of them
 * shows all the code its codec needs.
 */
#ifndef SIXEL_TUTORIAL_H
#define SIXEL_TUTORIAL_H

#include <stddef.h>
#include <stdint.h>

#define TUTORIAL_MAX_SIDE 32768
/* A last band can extend past the limit, though paint there is an error. */
#define TUTORIAL_MAX_BANDS ((TUTORIAL_MAX_SIDE + 5) / 6)

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

/* Written once per step; this is your part. Both return 0 on success.
 * encode_sixel() appends a complete SIXEL stream to out. decode_sixel()
 * fills img, allocating its pixels with image_alloc(). */
int encode_sixel(image_t const *img, buffer_t *out);
int decode_sixel(unsigned char const *data, size_t size, image_t *img);

/* Image helpers. image_alloc() fills the pixels with register 0. */
int image_alloc(image_t *img, int width, int height);
void image_free(image_t *img);

/* Growable output buffer. */
void buf_reserve(buffer_t *b, size_t extra);
void buf_printf(buffer_t *b, char const *fmt, ...);
void buf_free(buffer_t *b);

static inline void buf_putc(buffer_t *b, int c)
{
    if (b->size == b->capacity) buf_reserve(b, 1);
    b->data[b->size++] = (unsigned char)c;
}

#endif /* SIXEL_TUTORIAL_H */
