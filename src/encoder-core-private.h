/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef LIBSIXEL_ENCODER_CORE_PRIVATE_H
#define LIBSIXEL_ENCODER_CORE_PRIVATE_H

#include "encoder-core.h"
#include "sixel-writer.h"
#include "sixel_atomic.h"

/*
 * Private run node used by the SIXEL encoder core.  It intentionally stays out
 * of the public output wrapper so only the encoder-core implementation family
 * can mutate the hot-path run list.
 */
typedef struct sixel_node {
    struct sixel_node *next;
    int pal;
    int sx;
    int mx;
    char *map;
} sixel_node_t;

/*
 * Private storage backing the legacy sixel_output_t handle.  The first field
 * is the 6cells encoder_core dispatch header, so a public sixel_output_t can be
 * cast to sixel_encoder_core_t without changing the public ABI.
 */
struct sixel_output {
    sixel_encoder_core_t encoder_core_interface;

    sixel_atomic_u32_t ref;
    sixel_allocator_t *allocator;
    sixel_writer_t *writer;
    sixel_writer_controls_t writer_controls;

    unsigned char has_8bit_control;
    unsigned char has_sixel_scrolling;
    unsigned char has_gri_arg_limit;
    unsigned char has_sdm_glitch;
    unsigned char skip_dcs_envelope;
    unsigned char skip_header;
    unsigned char palette_type;

    int colorspace;
    int source_colorspace;
    int pixelformat;

    int save_pixel;
    int save_count;
    int active_palette;
    int omit_two_color_keycolor_palette;

    sixel_node_t *node_top;
    sixel_node_t *node_free;

    int penetrate_multiplexer;
    int encode_policy;
    int ormode;
    long long last_frame_time_usec;

    int pos;
    unsigned char buffer[1];
};

SIXEL_INTERNAL_API SIXELSTATUS
sixel_encoder_core_encode_dispatch(
    sixel_encoder_core_encode_request_t const *request);

#endif /* LIBSIXEL_ENCODER_CORE_PRIVATE_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
