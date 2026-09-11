/*
 * SPDX-License-Identifier: MIT
 *
 * Format-neutral in-memory loader test support.
 */

#ifndef LOADER_BUILTIN_MEMORY_TEST_COMMON_H
#define LOADER_BUILTIN_MEMORY_TEST_COMMON_H

#include <stddef.h>
#include <stdint.h>

#include <sixel.h>

#define EDGE_BUFFER_CAPACITY 65536u
#define EDGE_FRAME_CAPACITY 3
#define EDGE_RGB_CAPACITY 131072u
#define EDGE_PALETTE_CAPACITY 768u
#define EDGE_MASK_CAPACITY 16384u

typedef struct edge_writer {
    unsigned char *buffer;
    size_t capacity;
    size_t length;
    int failed;
} edge_writer_t;

typedef struct edge_loader_options {
    int require_static;
    int use_palette;
    int reqcolors;
    int loop_control;
    int cms_engine;
    int set_bgcolor;
    unsigned char bgcolor[3];
    int set_prefer_float32;
    int prefer_float32;
} edge_loader_options_t;

typedef struct edge_frame_probe {
    int callback_count;
    int width[EDGE_FRAME_CAPACITY];
    int height[EDGE_FRAME_CAPACITY];
    int pixelformat[EDGE_FRAME_CAPACITY];
    int colorspace[EDGE_FRAME_CAPACITY];
    int ncolors[EDGE_FRAME_CAPACITY];
    int delay[EDGE_FRAME_CAPACITY];
    int frame_no[EDGE_FRAME_CAPACITY];
    int loop_no[EDGE_FRAME_CAPACITY];
    int multiframe[EDGE_FRAME_CAPACITY];
    size_t rgb_size[EDGE_FRAME_CAPACITY];
    unsigned char rgb[EDGE_FRAME_CAPACITY][EDGE_RGB_CAPACITY];
    size_t palette_size[EDGE_FRAME_CAPACITY];
    unsigned char palette[EDGE_FRAME_CAPACITY][EDGE_PALETTE_CAPACITY];
    int transparent[EDGE_FRAME_CAPACITY];
    int alpha_zero_is_transparent[EDGE_FRAME_CAPACITY];
    size_t mask_size[EDGE_FRAME_CAPACITY];
    unsigned char mask[EDGE_FRAME_CAPACITY][EDGE_MASK_CAPACITY];
} edge_frame_probe_t;

void edge_put_u8(edge_writer_t *writer, unsigned int value);
void edge_put_u16le(edge_writer_t *writer, unsigned int value);
void edge_put_u16be(edge_writer_t *writer, unsigned int value);
void edge_put_bytes(edge_writer_t *writer,
                    unsigned char const *bytes,
                    size_t byte_count);
void edge_loader_options_init(edge_loader_options_t *options);

int edge_load_buffer(char const *label,
                     unsigned char const *buffer,
                     size_t buffer_size,
                     int require_static,
                     edge_frame_probe_t *probe,
                     SIXELSTATUS *load_status);
int edge_load_buffer_options(char const *label,
                             unsigned char const *buffer,
                             size_t buffer_size,
                             edge_loader_options_t const *options,
                             edge_frame_probe_t *probe,
                             SIXELSTATUS *load_status);
int edge_load_fixture(char const *label,
                      char const *relative_path,
                      int require_static,
                      edge_frame_probe_t *probe,
                      SIXELSTATUS *load_status);
int edge_load_fixture_options(char const *label,
                              char const *relative_path,
                              edge_loader_options_t const *options,
                              edge_frame_probe_t *probe,
                              SIXELSTATUS *load_status);
int edge_load_fixture_custom(char const *label,
                             char const *relative_path,
                             edge_loader_options_t const *options,
                             sixel_allocator_t *component_allocator,
                             sixel_load_image_function callback,
                             void *callback_context,
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
int edge_expect_fixture_rgb_digests(char const *label,
                                    char const *relative_path,
                                    int require_static,
                                    int expected_width,
                                    int expected_height,
                                    int expected_frames,
                                    uint64_t const *expected_digests);
int edge_expect_fixture_digests_options(
    char const *label,
    char const *relative_path,
    edge_loader_options_t const *options,
    int expected_width,
    int expected_height,
    int expected_frames,
    int expected_pixelformat,
    int expected_colorspace,
    uint64_t const *expected_digests);
int edge_expect_fixture_float_samples(
    char const *label,
    char const *relative_path,
    edge_loader_options_t const *options,
    int expected_width,
    int expected_height,
    int expected_pixelformat,
    int expected_colorspace,
    size_t const sample_pixels[3],
    float const expected_samples[9],
    float tolerance);
int edge_expect_buffer_float_samples(
    char const *label,
    unsigned char const *buffer,
    size_t buffer_size,
    edge_loader_options_t const *options,
    int expected_width,
    int expected_height,
    int expected_pixelformat,
    int expected_colorspace,
    size_t const sample_pixels[3],
    float const expected_samples[9],
    float tolerance);
int edge_expect_fixture_rgb_mask_digests(
    char const *label,
    char const *relative_path,
    edge_loader_options_t const *options,
    int expected_width,
    int expected_height,
    int expected_frames,
    uint64_t const *expected_rgb_digests,
    uint64_t const *expected_mask_digests);
uint64_t edge_digest_bytes(unsigned char const *bytes, size_t byte_count);

#endif

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
