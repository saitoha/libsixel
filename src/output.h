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

#ifndef LIBSIXEL_OUTPUT_H
#define LIBSIXEL_OUTPUT_H

#include <6cells.h>

typedef struct sixel_output_options {
    int has_gri_arg_limit;
    int palette_type;
    int encode_policy;
    int ormode;
    int transparent_policy;
    int transparent_offset_left;
    int transparent_offset_top;
    int pixelformat;
    int source_colorspace;
    int colorspace;
} sixel_output_options_t;

typedef struct sixel_legacy_encode_request {
    unsigned char *pixels;
    int width;
    int height;
    int depth;
    sixel_dither_t *dither;
    sixel_output_t *output;
} sixel_legacy_encode_request_t;

/* Invocation-owned composition state, never part of encoder_core. A
 * successful session retains update only while optional paint is consumed. */
typedef struct sixel_encode_session {
    sixel_retained_plane_interface_t *plane;
    sixel_retained_update_t *update;
    sixel_retained_update_stats_t stats;
    int plane_width;
    int plane_height;
    int origin_x;
    int origin_y;
    int record_paint;
    int committed;
    int partial_output;
    sixel_cancel_function cancel;
    void *cancel_context;
} sixel_encode_session_t;

SIXEL_INTERNAL_API void
sixel_output_bind_session(sixel_output_t *output,
    sixel_encode_session_t *session);

typedef struct sixel_output_frame_delay {
    int elapsed_usec;
    unsigned int target_usec;
    unsigned int remaining_usec;
} sixel_output_frame_delay_t;

/*
 * This header is the legacy sixel_output_t boundary.  The concrete SIXEL
 * byte-stream storage lives in encoder-core-private.h and should only be
 * included by the encoder-core implementation family.
 */

SIXEL_INTERNAL_API SIXELSTATUS
sixel_output_init_writer(sixel_output_t *output,
                         sixel_write_function fn_write,
                         void *priv);
SIXEL_INTERNAL_API SIXELSTATUS
sixel_output_get_writer_controls(sixel_output_t *output,
                                 sixel_writer_controls_t *controls);
SIXEL_INTERNAL_API SIXELSTATUS
sixel_output_set_writer_controls(
    sixel_output_t *output,
    sixel_writer_controls_t const *controls);
SIXEL_INTERNAL_API SIXELSTATUS
sixel_output_get_encoder_options(sixel_output_t *output,
                                 sixel_output_options_t *options);
SIXEL_INTERNAL_API SIXELSTATUS
sixel_output_set_encoder_options(
    sixel_output_t *output,
    sixel_output_options_t const *options);
SIXEL_INTERNAL_API SIXELSTATUS
sixel_output_set_encoder_format(sixel_output_t *output,
                                int pixelformat,
                                int source_colorspace,
                                int colorspace);
SIXEL_INTERNAL_API SIXELSTATUS
sixel_output_compute_frame_delay(sixel_output_t *output,
                                 int delay_cs,
                                 sixel_output_frame_delay_t *delay);
SIXEL_INTERNAL_API SIXELSTATUS
sixel_output_begin_image(sixel_output_t *output,
                         int width,
                         int height,
                         int parameter0,
                         int parameter1,
                         int parameter2,
                         int parameter_count,
                         int use_raster_attributes);
SIXEL_INTERNAL_API SIXELSTATUS
sixel_output_write_bytes(sixel_output_t *output,
                         char const *data,
                         int size);
SIXEL_INTERNAL_API SIXELSTATUS
sixel_output_end_image(sixel_output_t *output);

#endif /* LIBSIXEL_OUTPUT_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
