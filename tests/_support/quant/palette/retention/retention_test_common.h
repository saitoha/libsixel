/* SPDX-License-Identifier: MIT */
#ifndef SIXEL_TEST_PALETTE_RETENTION_H
#define SIXEL_TEST_PALETTE_RETENTION_H
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <math.h>
#include <string.h>
#include <sixel.h>
#include "src/dither.h"
#include "src/palette.h"

typedef struct retention_capture {
    char bytes[2048];
    size_t length;
} retention_capture_t;

unsigned char *retention_bytes(sixel_dither_t *dither);
int retention_write(char *data, int size, void *context);
int retention_init(sixel_dither_t **dither, void *pixels, int format,
                   int model);
int retention_pair(sixel_dither_t *dither, float const expected[6]);
int retention_pick(sixel_dither_t *dither, float const pixel[3], int slot);
int retention_generated(int model);
#endif
