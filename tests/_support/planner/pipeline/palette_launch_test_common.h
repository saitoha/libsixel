/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_PALETTE_LAUNCH_TEST_COMMON_H
#define LIBSIXEL_PALETTE_LAUNCH_TEST_COMMON_H
#include "palette_store_test_common.h"
#include "src/encoder.h"
#include "src/dither.h"
int palette_launch_input(sixel_allocator_t *allocator, int floating,
    sixel_palette_sampling_policy_t sampling, sixel_encoder_t **encoder,
    sixel_frame_t **source, sixel_palette_build_request_t **request,
    sixel_frame_t **sample);
SIXELSTATUS palette_launch_clone(sixel_frame_t *frame,
    sixel_allocator_t *allocator, sixel_frame_t **out);
int palette_launch_equal(sixel_dither_t *a, sixel_dither_t *b, int floating);
#endif
