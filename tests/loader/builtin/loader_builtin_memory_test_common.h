/*
 * SPDX-License-Identifier: MIT
 *
 * Format-neutral in-memory loader test support.
 */

#ifndef LOADER_BUILTIN_MEMORY_TEST_COMMON_H
#define LOADER_BUILTIN_MEMORY_TEST_COMMON_H

#include <stddef.h>

#include <sixel.h>

#define EDGE_BUFFER_CAPACITY 65536u
#define EDGE_FRAME_CAPACITY 2
#define EDGE_RGB_CAPACITY 12288u

typedef struct edge_writer {
    unsigned char *buffer;
    size_t capacity;
    size_t length;
    int failed;
} edge_writer_t;

typedef struct edge_frame_probe {
    int callback_count;
    int width[EDGE_FRAME_CAPACITY];
    int height[EDGE_FRAME_CAPACITY];
    int pixelformat[EDGE_FRAME_CAPACITY];
    size_t rgb_size[EDGE_FRAME_CAPACITY];
    unsigned char rgb[EDGE_FRAME_CAPACITY][EDGE_RGB_CAPACITY];
} edge_frame_probe_t;

void edge_put_u8(edge_writer_t *writer, unsigned int value);
void edge_put_u16le(edge_writer_t *writer, unsigned int value);
void edge_put_u16be(edge_writer_t *writer, unsigned int value);
void edge_put_bytes(edge_writer_t *writer,
                    unsigned char const *bytes,
                    size_t byte_count);

int edge_load_buffer(char const *label,
                     unsigned char const *buffer,
                     size_t buffer_size,
                     int require_static,
                     edge_frame_probe_t *probe,
                     SIXELSTATUS *load_status);
int edge_expect_rgb(char const *label,
                    unsigned char const *buffer,
                    size_t buffer_size,
                    int require_static,
                    int expected_width,
                    int expected_height,
                    int expected_frames,
                    unsigned char const *expected_rgb,
                    size_t expected_frame_size);
int edge_expect_failure(char const *label,
                        unsigned char const *buffer,
                        size_t buffer_size);

#endif

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
