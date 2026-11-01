/* SPDX-License-Identifier: MIT */
#ifndef PALETTE_MAP_TEST_COMMON_H
#define PALETTE_MAP_TEST_COMMON_H
#include <sixel.h>

/* One specimen describes an observable row, repeated over the canvas. */
struct palette_map_case {
    char const *body;
    int params[2];
    unsigned char const *row;
    int width;
    int height;
    unsigned int flags;
    int mapped;
    int reconstruct;
    int rgb_output;
    int parallel;
    int gpu;
};
int test_palette_map_case(struct palette_map_case const *specimen);
#endif
