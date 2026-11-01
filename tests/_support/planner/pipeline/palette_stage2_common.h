/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_TEST_PALETTE_STAGE2_COMMON_H
#define LIBSIXEL_TEST_PALETTE_STAGE2_COMMON_H
#include <sixel.h>
#include "src/encoder-palette.h"
#include "src/dither.h"

/* Build a two-color request, then destroy its encoder before execution. */
int palette_stage2_request(sixel_allocator_t *allocator, int floating,
    int lookup, int shared, int endpoint,
    sixel_palette_build_request_t **request, sixel_frame_t **frame);
int palette_stage2_snapshot(sixel_allocator_t *allocator, int floating,
    int lookup, int shared, sixel_palette_snapshot_t **snapshot);
int palette_stage2_isolation(int floating);
int palette_stage2_fhedt(int floating);
#endif
