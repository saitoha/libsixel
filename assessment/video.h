/* SPDX-License-Identifier: MIT */
#ifndef SIXEL_ASSESSMENT_VIDEO_H
#define SIXEL_ASSESSMENT_VIDEO_H

#include "assessment.h"

typedef struct lsqa_video_options {
    char const *reference;
    char const *target;
    char const *timeline;
    char const *display_timeline;
    char const *report;
    char const *error_map;
    char const *loaders;
    char const *target_colorspace;
    unsigned char const *background;
    unsigned int metrics;
    double ppd;
    double threshold;
    double reference_fps;
    double persistence_time;
    int clock_alignment;
    int signal_space;
    int signal_bytes;
    int grayscale;
    int region_window;
    int print_summary;
} lsqa_video_options_t;

/* Frame- or clock-aligned evaluation of one source animation cycle and a
 * fixed-canvas SIXEL capture. No replay sleeps or image-count-sized storage
 * are used.
 * Caller receives duration-weighted means only after complete validation. */
SIXEL_INTERNAL_API SIXELSTATUS
lsqa_video_run(lsqa_video_options_t const *options,
    sixel_allocator_t *allocator, double *means, unsigned int *valid_mask);

#endif
