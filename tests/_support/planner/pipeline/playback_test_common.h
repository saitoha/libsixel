/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_TESTS_PLAYBACK_TEST_COMMON_H
#define LIBSIXEL_TESTS_PLAYBACK_TEST_COMMON_H

#include <stddef.h>
#include <sixel.h>

typedef struct playback_test_buffer {
    unsigned char *bytes;
    size_t size;
    size_t capacity;
} playback_test_buffer_t;

/* sixel_write_function appending to a playback_test_buffer_t. */
int playback_test_write(char *data, int size, void *priv);

/* sixel_write_function that always fails. */
int playback_test_write_fail(char *data, int size, void *priv);

/* A moving bar over a gradient; frame INDEX shifts the bar. */
sixel_frame_t *playback_test_frame(int index);

/* The same moving features on a 16x16 fixture for scheduling observations. */
sixel_frame_t *playback_test_small_frame(int index);

/* FS diffusion with 16 colors; DELTA selects -Z delta. */
sixel_encoder_t *playback_test_encoder(int delta, char const *threads);

#endif
