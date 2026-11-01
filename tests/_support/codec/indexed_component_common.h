/* SPDX-License-Identifier: MIT */
#ifndef TEST_INDEXED_COMPONENT_COMMON_H
#define TEST_INDEXED_COMPONENT_COMMON_H
#include <string.h>
#include <sixel.h>
#include <6cells.h>
#include "src/encode-result.h"
#include "src/palette.h"
#include "src/encoder-core.h"
#include "src/sixel-writer.h"

typedef struct indexed_fixture {
    sixel_allocator_t *allocator;
    sixel_wire_palette_t *wire;
    sixel_indexed_frame_t *image;
    sixel_retained_plane_interface_t *plane;
    sixel_retained_read_t *read;
    sixel_retained_update_t *update;
    sixel_writer_t *writer;
    sixel_stream_receipt_t *receipt;
    sixel_retained_update_stats_t stats;
    sixel_retained_read_view_t view;
    int reject;
    int calls;
} indexed_fixture_t;
int indexed_fixture_init(indexed_fixture_t *f);
SIXELSTATUS indexed_fixture_begin(indexed_fixture_t *f);
SIXELSTATUS indexed_fixture_seal(indexed_fixture_t *f);
SIXELSTATUS indexed_fixture_accept(indexed_fixture_t *f);
SIXELSTATUS indexed_fixture_commit(indexed_fixture_t *f);
void indexed_fixture_destroy(indexed_fixture_t *f);
#endif
