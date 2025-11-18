/*
 * Internal dithering helpers shared between the 8bit and float32
 * implementations.  The header exposes the context descriptor passed to the
 * per-algorithm workers so each backend can access the relevant buffers
 * without expanding their public signatures.
 */
#ifndef LIBSIXEL_DITHER_INTERNAL_H
#define LIBSIXEL_DITHER_INTERNAL_H

#include "dither.h"

typedef struct sixel_dither_context {
    sixel_index_t *result;
    unsigned char *pixels;
    float *pixels_float;
    int width;
    int height;
    int depth;
    unsigned char *palette;
    float *palette_float;
    int reqcolor;
    int method_for_diffuse;
    int method_for_scan;
    int method_for_carry;
    int optimize_palette;
    int complexion;
    int (*lookup)(const unsigned char *pixel,
                  int depth,
                  const unsigned char *palette,
                  int reqcolor,
                  unsigned short *cachetable,
                  int complexion);
    unsigned short *indextable;
    unsigned char *scratch;
    unsigned char *new_palette;
    float *new_palette_float;
    unsigned short *migration_map;
    int *ncolors;
    int pixelformat;
    int float_depth;
    int lookup_source_is_float;
    int prefer_palette_float_lookup;
} sixel_dither_context_t;

#endif /* LIBSIXEL_DITHER_INTERNAL_H */
