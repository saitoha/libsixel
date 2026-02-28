/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef LIBSIXEL_PLANNER_H
#define LIBSIXEL_PLANNER_H

struct sixel_encoder;
struct sixel_frame;

typedef enum sixel_planner_node_kind {
    SIXEL_PLANNER_NODE_LOAD = 0,
    SIXEL_PLANNER_NODE_PALETTE,
    SIXEL_PLANNER_NODE_FHEDT,
    SIXEL_PLANNER_NODE_VPTREE,
    SIXEL_PLANNER_NODE_EYTZINGER,
    SIXEL_PLANNER_NODE_CLIP,
    SIXEL_PLANNER_NODE_COLORSPACE_PRE,
    SIXEL_PLANNER_NODE_SCALE,
    SIXEL_PLANNER_NODE_COLORSPACE_POST,
    SIXEL_PLANNER_NODE_JOIN,
    SIXEL_PLANNER_NODE_DITHER,
    SIXEL_PLANNER_NODE_ENCODE,
} sixel_planner_node_kind_t;

typedef struct sixel_planner_node {
    sixel_planner_node_kind_t kind;
    char const *label;
} sixel_planner_node_t;

typedef struct sixel_planner_edge {
    int from;
    int to;
    int pipeline;
} sixel_planner_edge_t;

#define SIXEL_PLANNER_DAG_NODE_MAX 16
#define SIXEL_PLANNER_DAG_EDGE_MAX 32

typedef struct sixel_encoding_planner {
    int total_threads;
    int main_threads;
    int palette_threads;
    int allow_palette_async;
    int clip_active;
    int scale_active;
    int colorspace_active;
    int heavy_ops;
    int working_pixelformat;
    int scale_pixelformat;
    int scale_input_pixelformat;
    int working_colorspace_effective;
    int colorspace_before_scale;
    int colorspace_after_scale;
    int resize_precision_mode;
    int pipeline_active;
    int pipeline_band_height;
    int pipeline_overlap;
    int pipeline_queue_depth;
    int pipeline_dither_threads;
    int pipeline_encode_threads;
    int pipeline_bands;
    int pipeline_pin_threads;

    int dag_node_count;
    int dag_edge_count;
    sixel_planner_node_t dag_nodes[SIXEL_PLANNER_DAG_NODE_MAX];
    sixel_planner_edge_t dag_edges[SIXEL_PLANNER_DAG_EDGE_MAX];
} sixel_encoding_planner_t;

void sixel_encoding_planner_init(sixel_encoding_planner_t *planner);
void sixel_encoding_planner_plan(sixel_encoding_planner_t *planner,
                                 struct sixel_encoder *encoder,
                                 struct sixel_frame *frame);
void sixel_encoding_planner_dump(sixel_encoding_planner_t *planner,
                                 struct sixel_encoder *encoder,
                                 struct sixel_frame *frame,
                                 int palette_ready);
void sixel_encoding_planner_replan(sixel_encoding_planner_t *planner,
                                   struct sixel_encoder *encoder,
                                   struct sixel_frame *frame,
                                   int palette_ready);
int sixel_encoding_palette_job_ready(struct sixel_encoder *encoder,
                                     sixel_encoding_planner_t *planner,
                                     struct sixel_frame *frame);

#endif /* LIBSIXEL_PLANNER_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
