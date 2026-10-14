/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_PROGRESSIVE_APPLY_H
#define LIBSIXEL_PROGRESSIVE_APPLY_H

#include "progressive-palette.h"
#include "palette.h"
#include "progressive-gpu.h"

/* Palette entries are already projected through the wire representation.
 * Selection and paint are independent masks: rejection always keeps history,
 * including when the nearest paintable palette entry is index zero. */
typedef struct sixel_repair_application {
    float const *source;
    double const *errors;
    unsigned char const *selected;
    sixel_palette_emitted_color_t const *colors;
    unsigned int const *palette;
    unsigned int color_count;
    size_t count;
    int width;
    int origin_x;
    int origin_y;
    size_t protected_position;
    double strength;
    unsigned int phase_x;
    unsigned int phase_y;
    int channel_rgb;
    /* Borrowed context cache. AUTO has already passed population admission;
     * a backend failure disables GPU retries for this application only. */
    sixel_repair_gpu_t *gpu;
    int gpu_policy;
    int gpu_used;
    unsigned char *indices;
    unsigned char *paint;
    double *gains;
    size_t painted;
    size_t position;
    unsigned int candidate;
    unsigned int winner;
    float query[3];
    double nearest;
    int started;
} sixel_repair_application_t;

/* The caller supplies output arrays of COUNT elements. This initializer
 * allocates nothing; a failed lookup cannot modify authoritative history. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_repair_application_begin(sixel_repair_application_t *application);
SIXEL_INTERNAL_API SIXELSTATUS
sixel_repair_application_advance(sixel_repair_application_t *application,
    sixel_repair_work_t *work, int *done);

#endif
