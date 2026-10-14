/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_ENCODER_CORE_PACKET_H
#define LIBSIXEL_ENCODER_CORE_PACKET_H

#include "palette.h"

typedef struct sixel_indexed_packet_request {
    int width;
    int height;
    unsigned char const *indices;
    unsigned char const *paint;
    sixel_palette_emitted_color_t const *colors;
    unsigned int const *palette;
    unsigned int color_count;
    size_t byte_limit;
    int has_8bit_control;
    int has_gri_arg_limit;
} sixel_indexed_packet_request_t;

/* Bounded private serialization. The byte count includes the entire DCS;
 * only the prefix up to byte_limit is stored when the packet does not fit.
 * All image/palette pointers are borrowed until completion or destruction. */
typedef struct sixel_indexed_packet {
    sixel_indexed_packet_request_t request;
    sixel_allocator_t *allocator;
    sixel_output_t *output;
    unsigned char *data;
    unsigned char *map;
    size_t bytes;
    size_t painted;
    size_t count;
    size_t cursor;
    int byte_overflow;
    int stage;
    int band;
    int column;
    int first_color;
    int maximum[255];
    unsigned char used[255];
    unsigned char remap[255];
    unsigned int colors;
    unsigned int color;
} sixel_indexed_packet_t;

SIXEL_INTERNAL_API SIXELSTATUS
sixel_indexed_packet_new(sixel_indexed_packet_t **packet,
    sixel_indexed_packet_request_t const *request,
    sixel_allocator_t *allocator);
SIXEL_INTERNAL_API void
sixel_indexed_packet_free(sixel_indexed_packet_t *packet);
/* Advance at most 256 units within the remaining visit budget. No external
 * output is invoked; the same RLE helpers serve ordinary and staged output. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_indexed_packet_advance(sixel_indexed_packet_t *packet,
    size_t visit_limit, size_t *visits, int *done);

#endif
