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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <sixel.h>

#include "compat_stub.h"
#include "encoder.h"
#include "frame.h"
#include "planner.h"
#include "pixelformat.h"
#include "threading.h"

#define SIXEL_PLANNER_RESIZE_MODE_PRESERVE 1
#define SIXEL_PLANNER_RESIZE_MODE_LINEAR32 2
#define SIXEL_PLANNER_RESIZE_MODE_FLOAT_WORK 3

static void sixel_encoding_planner_plan_pipeline(
    sixel_encoding_planner_t *planner,
    sixel_encoder_t *encoder,
    sixel_frame_t *frame);
static void
sixel_encoding_planner_dag_clear(sixel_encoding_planner_t *planner);
static int
sixel_encoding_planner_dag_add_node(sixel_encoding_planner_t *planner,
                                    sixel_planner_node_kind_t kind,
                                    char const *label);
static void
sixel_encoding_planner_dag_add_edge(sixel_encoding_planner_t *planner,
                                    int from,
                                    int to,
                                    int pipeline);
static void
sixel_encoding_planner_build_dag(sixel_encoding_planner_t *planner,
                                 sixel_encoder_t *encoder,
                                 int palette_ready);
static float
sixel_encoding_planner_resolve_bluenoise_gradient_factor(
    sixel_encoder_t const *encoder);
static int
sixel_encoding_planner_replan_palette_branch(sixel_encoding_planner_t *planner,
                                             sixel_encoder_t *encoder,
                                             sixel_frame_t *frame,
                                             int palette_ready);
static char const *
sixel_encoding_planner_pixelformat_label(int pixelformat);
void
sixel_encoding_planner_set_loader_metadata(sixel_encoding_planner_t *planner,
                                           int multiframe_known,
                                           int multiframe);

static int
sixel_planner_pixelformat_for_colorspace(int colorspace,
                                          int prefer_float32)
{
    switch (colorspace) {
    case SIXEL_COLORSPACE_LINEAR:
        return SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
    case SIXEL_COLORSPACE_OKLAB:
        return SIXEL_PIXELFORMAT_OKLABFLOAT32;
    case SIXEL_COLORSPACE_CIELAB:
        return SIXEL_PIXELFORMAT_CIELABFLOAT32;
    case SIXEL_COLORSPACE_DIN99D:
        return SIXEL_PIXELFORMAT_DIN99DFLOAT32;
    default:
        if (prefer_float32) {
            return SIXEL_PIXELFORMAT_RGBFLOAT32;
        }
        return SIXEL_PIXELFORMAT_RGB888;
    }
}

static float
sixel_encoding_planner_resolve_bluenoise_gradient_factor(
    sixel_encoder_t const *encoder)
{
    char const *text;
    char *endptr;
    double parsed;
    float resolved;

    text = NULL;
    endptr = NULL;
    parsed = 0.0;
    resolved = 0.0f;

    if (encoder == NULL) {
        return 0.0f;
    }
    if (encoder->method_for_diffuse != SIXEL_DIFFUSE_BLUENOISE_DITHER) {
        return 0.0f;
    }

    if (encoder->bluenoise_gradient_factor_override != 0) {
        resolved = encoder->bluenoise_gradient_factor;
        if (resolved > 0.0f) {
            return resolved;
        }
        return 0.0f;
    }

    text = sixel_compat_getenv("SIXEL_DITHER_BLUENOISE_GRADIENT_FACTOR");
    if (text == NULL || text[0] == '\0') {
        return 0.0f;
    }

    errno = 0;
    parsed = strtod(text, &endptr);
    if (endptr == text
            || endptr == NULL
            || endptr[0] != '\0'
            || errno != 0
            || parsed <= 0.0) {
        return 0.0f;
    }

    resolved = (float)parsed;
    if (resolved <= 0.0f) {
        return 0.0f;
    }

    return resolved;
}

static void
sixel_encoding_planner_dag_clear(sixel_encoding_planner_t *planner)
{
    int i;

    if (planner == NULL) {
        return;
    }

    planner->dag_node_count = 0;
    planner->dag_edge_count = 0;
    for (i = 0; i < SIXEL_PLANNER_DAG_NODE_MAX; ++i) {
        planner->dag_nodes[i].kind = SIXEL_PLANNER_NODE_LOAD;
        planner->dag_nodes[i].label = NULL;
    }
    for (i = 0; i < SIXEL_PLANNER_DAG_EDGE_MAX; ++i) {
        planner->dag_edges[i].from = -1;
        planner->dag_edges[i].to = -1;
        planner->dag_edges[i].pipeline = 0;
    }
}

static int
sixel_encoding_planner_dag_add_node(sixel_encoding_planner_t *planner,
                                    sixel_planner_node_kind_t kind,
                                    char const *label)
{
    int index;

    if (planner == NULL || label == NULL) {
        return -1;
    }
    index = planner->dag_node_count;
    if (index < 0 || index >= SIXEL_PLANNER_DAG_NODE_MAX) {
        return -1;
    }

    planner->dag_nodes[index].kind = kind;
    planner->dag_nodes[index].label = label;
    planner->dag_node_count = index + 1;

    return index;
}

static void
sixel_encoding_planner_dag_add_edge(sixel_encoding_planner_t *planner,
                                    int from,
                                    int to,
                                    int pipeline)
{
    int index;

    if (planner == NULL) {
        return;
    }
    index = planner->dag_edge_count;
    if (index < 0 || index >= SIXEL_PLANNER_DAG_EDGE_MAX) {
        return;
    }

    planner->dag_edges[index].from = from;
    planner->dag_edges[index].to = to;
    planner->dag_edges[index].pipeline = pipeline;
    planner->dag_edge_count = index + 1;
}

static void
sixel_encoding_planner_build_dag(sixel_encoding_planner_t *planner,
                                 sixel_encoder_t *encoder,
                                 int palette_ready)
{
    int load_node;
    int palette_node;
    int lut_node;
    int clip_node;
    int colorspace_pre_node;
    int scale_node;
    int colorspace_post_node;
    int join_node;
    int gradient_node;
    int dither_node;
    int encode_node;
    int work_tail;
    float gradient_factor;

    load_node = -1;
    palette_node = -1;
    lut_node = -1;
    clip_node = -1;
    colorspace_pre_node = -1;
    scale_node = -1;
    colorspace_post_node = -1;
    join_node = -1;
    gradient_node = -1;
    dither_node = -1;
    encode_node = -1;
    work_tail = -1;
    gradient_factor = 0.0f;

    if (planner == NULL || encoder == NULL) {
        return;
    }

    gradient_factor =
        sixel_encoding_planner_resolve_bluenoise_gradient_factor(encoder);

    sixel_encoding_planner_dag_clear(planner);

    load_node = sixel_encoding_planner_dag_add_node(
        planner, SIXEL_PLANNER_NODE_LOAD, "load");
    work_tail = load_node;

    if (encoder->clipfirst != 0 && planner->clip_active != 0) {
        clip_node = sixel_encoding_planner_dag_add_node(
            planner, SIXEL_PLANNER_NODE_CLIP, "clip");
        sixel_encoding_planner_dag_add_edge(planner, work_tail, clip_node, 0);
        work_tail = clip_node;
    }

    if (planner->colorspace_before_scale != 0) {
        colorspace_pre_node = sixel_encoding_planner_dag_add_node(
            planner, SIXEL_PLANNER_NODE_COLORSPACE_PRE, "colorspace(pre)");
        sixel_encoding_planner_dag_add_edge(planner,
                                            work_tail,
                                            colorspace_pre_node,
                                            0);
        work_tail = colorspace_pre_node;
    }

    if (planner->scale_active != 0) {
        scale_node = sixel_encoding_planner_dag_add_node(
            planner, SIXEL_PLANNER_NODE_SCALE, "scale");
        sixel_encoding_planner_dag_add_edge(planner, work_tail, scale_node, 0);
        work_tail = scale_node;
    }

    if (encoder->clipfirst == 0 && planner->clip_active != 0) {
        clip_node = sixel_encoding_planner_dag_add_node(
            planner, SIXEL_PLANNER_NODE_CLIP, "clip");
        sixel_encoding_planner_dag_add_edge(planner, work_tail, clip_node, 0);
        work_tail = clip_node;
    }

    if (planner->colorspace_after_scale != 0) {
        colorspace_post_node = sixel_encoding_planner_dag_add_node(
            planner,
            SIXEL_PLANNER_NODE_COLORSPACE_POST,
            "colorspace(post)");
        sixel_encoding_planner_dag_add_edge(planner,
                                            work_tail,
                                            colorspace_post_node,
                                            0);
        work_tail = colorspace_post_node;
    }

    if (palette_ready != 0) {
        palette_node = sixel_encoding_planner_dag_add_node(
            planner, SIXEL_PLANNER_NODE_PALETTE, "palette");
        sixel_encoding_planner_dag_add_edge(planner,
                                            load_node,
                                            palette_node,
                                            0);

        if (encoder->lut_policy == SIXEL_LUT_POLICY_FHEDT) {
            lut_node = sixel_encoding_planner_dag_add_node(
                planner, SIXEL_PLANNER_NODE_FHEDT, "fhedt");
        } else if (encoder->lut_policy == SIXEL_LUT_POLICY_VPTREE) {
            lut_node = sixel_encoding_planner_dag_add_node(
                planner, SIXEL_PLANNER_NODE_VPTREE, "vptree");
        } else if (encoder->lut_policy == SIXEL_LUT_POLICY_EYTZINGER) {
            lut_node = sixel_encoding_planner_dag_add_node(
                planner, SIXEL_PLANNER_NODE_EYTZINGER, "eytzinger");
        }
        if (lut_node >= 0) {
            sixel_encoding_planner_dag_add_edge(planner,
                                                palette_node,
                                                lut_node,
                                                0);
        }

        join_node = sixel_encoding_planner_dag_add_node(
            planner, SIXEL_PLANNER_NODE_JOIN, "join");
        if (lut_node >= 0) {
            sixel_encoding_planner_dag_add_edge(planner,
                                                lut_node,
                                                join_node,
                                                0);
        } else {
            sixel_encoding_planner_dag_add_edge(planner,
                                                palette_node,
                                                join_node,
                                                0);
        }
        sixel_encoding_planner_dag_add_edge(planner, work_tail, join_node, 0);
        work_tail = join_node;
    }

    if (gradient_factor > 0.0f) {
        gradient_node = sixel_encoding_planner_dag_add_node(
            planner,
            SIXEL_PLANNER_NODE_GRADIENT_MAP,
            "gradient-map");
        sixel_encoding_planner_dag_add_edge(planner,
                                            work_tail,
                                            gradient_node,
                                            0);
        work_tail = gradient_node;
    }

    dither_node = sixel_encoding_planner_dag_add_node(
        planner, SIXEL_PLANNER_NODE_DITHER, "dither");
    sixel_encoding_planner_dag_add_edge(planner, work_tail, dither_node, 0);

    encode_node = sixel_encoding_planner_dag_add_node(
        planner, SIXEL_PLANNER_NODE_ENCODE, "encode");
    sixel_encoding_planner_dag_add_edge(planner,
                                        dither_node,
                                        encode_node,
                                        planner->pipeline_active != 0);
}

static int
sixel_encoding_planner_replan_palette_branch(sixel_encoding_planner_t *planner,
                                             sixel_encoder_t *encoder,
                                             sixel_frame_t *frame,
                                             int palette_ready)
{
    int source_pixelformat;
    int source_ncolors;
    int effective_reqcolors;
    int has_palette;
    int palette_branch_active;

    source_pixelformat = 0;
    source_ncolors = 0;
    effective_reqcolors = 0;
    has_palette = 0;
    palette_branch_active = 0;

    if (planner == NULL || encoder == NULL || frame == NULL) {
        return 0;
    }

    source_pixelformat = sixel_frame_get_pixelformat(frame);
    source_ncolors = sixel_frame_get_ncolors(frame);
    effective_reqcolors = encoder->reqcolors;
    if (effective_reqcolors <= 0 || effective_reqcolors > SIXEL_PALETTE_MAX) {
        effective_reqcolors = SIXEL_PALETTE_MAX;
    }

    has_palette = (sixel_frame_get_palette(frame) != NULL && source_ncolors > 0)
        ? 1
        : 0;
    palette_branch_active = (palette_ready != 0) ? 1 : 0;

    /*
     * Replan rule for PAL-preserving inputs:
     *
     * - If the loader already produced a palette frame and the frame satisfies
     *   the requested palette size, skip palette/LUT generation in the DAG.
     * - This keeps the graph aligned with the no-requantization path used by
     *   the execution planner and avoids redundant palette branches.
     */
    if ((source_pixelformat & SIXEL_FORMATTYPE_PALETTE) != 0
        && encoder->color_option == SIXEL_COLOR_OPTION_DEFAULT
        && planner->scale_active == 0
        && has_palette != 0
        && source_ncolors <= effective_reqcolors) {
        palette_branch_active = 0;
    }

    return palette_branch_active;
}

void
sixel_encoding_planner_init(sixel_encoding_planner_t *planner)
{
    if (planner == NULL) {
        return;
    }

    planner->total_threads = 1;
    planner->main_threads = 1;
    planner->palette_threads = 0;
    planner->allow_palette_async = 0;
    planner->clip_active = 0;
    planner->scale_active = 0;
    planner->colorspace_active = 0;
    planner->heavy_ops = 0;
    planner->working_pixelformat = SIXEL_PIXELFORMAT_RGB888;
    planner->scale_pixelformat = SIXEL_PIXELFORMAT_RGB888;
    planner->scale_input_pixelformat = SIXEL_PIXELFORMAT_RGB888;
    planner->working_colorspace_effective = SIXEL_COLORSPACE_GAMMA;
    planner->colorspace_before_scale = 0;
    planner->colorspace_after_scale = 0;
    planner->resize_precision_mode = 1;
    planner->pipeline_active = 0;
    planner->pipeline_band_height = 0;
    planner->pipeline_overlap = 0;
    planner->pipeline_queue_depth = 0;
    planner->pipeline_dither_threads = 0;
    planner->pipeline_encode_threads = 0;
    planner->pipeline_bands = 0;
    planner->pipeline_pin_threads = 1;
    planner->loader_multiframe_known = 0;
    planner->loader_multiframe = 0;
    planner->loader_pipeline_active = 0;
    sixel_encoding_planner_dag_clear(planner);
}


void
sixel_encoding_planner_reset_for_frame(sixel_encoding_planner_t *planner)
{
    int loader_multiframe_known;
    int loader_multiframe;

    loader_multiframe_known = 0;
    loader_multiframe = 0;

    if (planner == NULL) {
        return;
    }

    loader_multiframe_known = planner->loader_multiframe_known;
    loader_multiframe = planner->loader_multiframe;

    sixel_encoding_planner_init(planner);
    sixel_encoding_planner_set_loader_metadata(planner,
                                               loader_multiframe_known,
                                               loader_multiframe);
}

int
sixel_encoding_palette_job_ready(sixel_encoder_t *encoder,
                                 sixel_encoding_planner_t *planner,
                                 sixel_frame_t *frame)
{
    int pixelformat;

    if (encoder == NULL || planner == NULL || frame == NULL) {
        return 0;
    }

    if (encoder->palette_job_enabled == 0) {
        return 0;
    }
    if (planner->allow_palette_async == 0) {
        return 0;
    }
    if (encoder->color_option != SIXEL_COLOR_OPTION_DEFAULT) {
        return 0;
    }
    if (encoder->dither_cache != NULL) {
        return 0;
    }

    pixelformat = sixel_frame_get_pixelformat(frame);
    if ((pixelformat & SIXEL_FORMATTYPE_PALETTE) != 0) {
        return 0;
    }

    return 1;
}


void
sixel_encoding_planner_plan(sixel_encoding_planner_t *planner,
                            sixel_encoder_t *encoder,
                            sixel_frame_t *frame)
{
    int total;
    int budget;
    int source_colorspace;
    int source_pixelformat;
    int source_is_float32;
    int source_ncolors;
    int target_pixelformat;
    int scale_pixelformat;
    int scale_input_pixelformat;
    int prefer_float32 = 0;
    int prefer_float32_effective;
    int float_resize_required;
    int source_width;
    int source_height;
    int scale_depth;
    int colorspace_before_scale;
    int colorspace_after_scale;
    int working_colorspace_effective;
    int resize_mode;
    int effective_reqcolors;
    size_t scale_pixels;
    size_t scale_bytes;
    size_t scale_limit;
    char const *resize_mode_env;
    char *resize_endptr;

    if (planner == NULL || encoder == NULL || frame == NULL) {
        return;
    }

    resize_mode = SIXEL_PLANNER_RESIZE_MODE_PRESERVE;
    resize_mode_env = NULL;
    resize_endptr = NULL;
    colorspace_after_scale = 0;
    colorspace_before_scale = 0;
    prefer_float32_effective = prefer_float32;
    working_colorspace_effective = encoder->working_colorspace;
    source_pixelformat = sixel_frame_get_pixelformat(frame);
    source_is_float32 = SIXEL_PIXELFORMAT_IS_FLOAT32(source_pixelformat);
    source_ncolors = sixel_frame_get_ncolors(frame);
    effective_reqcolors = encoder->reqcolors;
    if (effective_reqcolors <= 0 || effective_reqcolors > SIXEL_PALETTE_MAX) {
        effective_reqcolors = SIXEL_PALETTE_MAX;
    }

    /*
     * Keep palette sampling from spawning extra threads when resize/clip or
     * full-frame colorspace conversion already occupy the available workers.
     * Heavy steps consume the budget; the palette worker runs only when at
     * least one spare thread remains after accounting for them.
     */
    total = sixel_threads_resolve();
    planner->total_threads = total;
    planner->palette_threads = 0;
    planner->allow_palette_async = 0;
    planner->main_threads = (total > 0) ? total : 1;
    planner->clip_active = (encoder->clipwidth > 0 && encoder->clipheight > 0)
        ? 1
        : 0;
    planner->scale_active = (encoder->pixelwidth >= 0
                            || encoder->pixelheight >= 0
                            || encoder->percentwidth >= 0
                            || encoder->percentheight >= 0)
        ? 1
        : 0;
    source_colorspace = sixel_frame_get_colorspace(frame);
    prefer_float32 = encoder->prefer_float32;
    if (source_is_float32) {
        /*
         * Loader-originated float buffers must stay float through the planner
         * path so pre-plan does not down-convert them to RGB888 before dither.
         */
        prefer_float32 = 1;
    }
    scale_pixelformat = SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
    scale_input_pixelformat = sixel_frame_get_pixelformat(frame);
    float_resize_required = (planner->scale_active != 0) ? 1 : 0;
    source_width = sixel_frame_get_width(frame);
    source_height = sixel_frame_get_height(frame);
    scale_pixels = 0U;
    scale_bytes = 0U;
    scale_depth = 0;
    scale_limit = SIXEL_ALLOCATE_BYTES_MAX / 2U;

    resize_mode_env = sixel_compat_getenv(
        "SIXEL_PLANNER_RESIZE_PRECISION_MODE");
    if (resize_mode_env != NULL && resize_mode_env[0] != '\0') {
        errno = 0;
        resize_mode = (int)strtol(resize_mode_env,
                                  &resize_endptr,
                                  10);
        if (errno != ERANGE && resize_endptr != resize_mode_env) {
            if (resize_mode < SIXEL_PLANNER_RESIZE_MODE_PRESERVE
                || resize_mode > SIXEL_PLANNER_RESIZE_MODE_FLOAT_WORK) {
                resize_mode = SIXEL_PLANNER_RESIZE_MODE_PRESERVE;
            }
        } else {
            resize_mode = SIXEL_PLANNER_RESIZE_MODE_PRESERVE;
        }
    } else if (prefer_float32 != 0) {
        resize_mode = SIXEL_PLANNER_RESIZE_MODE_FLOAT_WORK;
    } else if (encoder->working_colorspace != source_colorspace) {
        resize_mode = SIXEL_PLANNER_RESIZE_MODE_LINEAR32;
    }

    if (resize_mode == SIXEL_PLANNER_RESIZE_MODE_FLOAT_WORK) {
        prefer_float32_effective = 1;
    }

    target_pixelformat = sixel_planner_pixelformat_for_colorspace(
        working_colorspace_effective,
        prefer_float32_effective);
    if (source_is_float32
        && !SIXEL_PIXELFORMAT_IS_FLOAT32(target_pixelformat)) {
        target_pixelformat = sixel_planner_pixelformat_for_colorspace(
            working_colorspace_effective,
            1);
        prefer_float32_effective = 1;
    }

    /*
     * PAL sources that already satisfy the requested palette size do not need
     * to round-trip through RGB and a new palette solve. Keep the existing
     * palette so the DAG skips redundant work and preserves image fidelity.
     */
    if ((source_pixelformat & SIXEL_FORMATTYPE_PALETTE) != 0
        && encoder->color_option == SIXEL_COLOR_OPTION_DEFAULT
        && planner->scale_active == 0
        && sixel_frame_get_palette(frame) != NULL
        && source_ncolors > 0
        && source_ncolors <= effective_reqcolors) {
        target_pixelformat = source_pixelformat;
        working_colorspace_effective = sixel_frame_get_colorspace(frame);
        prefer_float32_effective = 0;
    }

    planner->working_pixelformat = target_pixelformat;
    planner->working_colorspace_effective = working_colorspace_effective;
    planner->resize_precision_mode = resize_mode;

    if (planner->scale_active != 0 && float_resize_required != 0) {
        switch (resize_mode) {
        case SIXEL_PLANNER_RESIZE_MODE_LINEAR32:
            scale_input_pixelformat = SIXEL_PIXELFORMAT_RGB888;
            break;
        case SIXEL_PLANNER_RESIZE_MODE_FLOAT_WORK:
            /*
             * Non-RGB workspaces cannot be resampled directly. Force a
             * linear RGB buffer for scaling and convert back afterward.
             */
            scale_input_pixelformat = SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
            break;
        case SIXEL_PLANNER_RESIZE_MODE_PRESERVE:
        default:
            /*
             * Preserve bit depth but still scale in linear RGB to avoid
             * gamma artifacts. A colorspace(pre) node is inserted when the
             * source is not already linear.
             */
            scale_input_pixelformat = SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
            break;
        }
        scale_pixelformat = scale_input_pixelformat;

        colorspace_before_scale = (sixel_frame_get_pixelformat(frame)
                                   != scale_input_pixelformat);
        colorspace_after_scale = (target_pixelformat
                                  != scale_pixelformat);
    } else {
        colorspace_before_scale = 0;
        colorspace_after_scale = (target_pixelformat
                                  != sixel_frame_get_pixelformat(frame));
        scale_pixelformat = sixel_frame_get_pixelformat(frame);
        scale_input_pixelformat = sixel_frame_get_pixelformat(frame);
    }

    if (encoder->verbose != 0) {
        fprintf(stderr,
                "libsixel: scale plan active=%d width=%d height=%d fmt=%08x\n",
                planner->scale_active,
                source_width,
                source_height,
                scale_input_pixelformat);
    }

    /*
     * Avoid promoting extremely tall frames to float32 when the intermediate
     * buffer would exceed the allocation guard. Crafted regressions feed
     * thousands of rows into tiny outputs; scaling in byte RGB keeps the
     * temporary working set below half of SIXEL_ALLOCATE_BYTES_MAX while still
     * downsampling before the palette stage.
     */
    if (planner->scale_active != 0
        && float_resize_required != 0
        && source_width > 0
        && source_height > 0
        && SIXEL_PIXELFORMAT_IS_FLOAT32(scale_input_pixelformat)) {
        scale_depth =
            sixel_helper_compute_depth(scale_input_pixelformat);
        if (scale_depth > 0
            && (size_t)source_width <= SIZE_MAX / (size_t)source_height) {
            scale_pixels = (size_t)source_width * (size_t)source_height;
            if (scale_pixels <= SIZE_MAX / (size_t)scale_depth) {
                scale_bytes = scale_pixels * (size_t)scale_depth;
                if (encoder->verbose != 0) {
                    fprintf(stderr,
                            "libsixel: scale budget float=%zu limit=%zu\n",
                            scale_bytes,
                            scale_limit);
                }
                if (!source_is_float32
                        && (scale_limit == 0U || scale_bytes > scale_limit)) {
                    scale_input_pixelformat = SIXEL_PIXELFORMAT_RGB888;
                    scale_pixelformat = scale_input_pixelformat;
                    colorspace_before_scale = 1;
                    colorspace_after_scale = (target_pixelformat
                                              != scale_pixelformat);
                }
            }
        }
    }

    planner->scale_pixelformat = scale_pixelformat;
    planner->scale_input_pixelformat = scale_input_pixelformat;
    planner->colorspace_before_scale = colorspace_before_scale;
    planner->colorspace_after_scale = colorspace_after_scale;
    planner->colorspace_active = (colorspace_before_scale != 0
                                  || colorspace_after_scale != 0
                                  || working_colorspace_effective
                                         != source_colorspace
                                  || prefer_float32_effective != 0)
        ? 1
        : 0;

    planner->heavy_ops = planner->clip_active
        + planner->scale_active
        + planner->colorspace_active;

    if (total <= 1) {
        return;
    }

    budget = total - planner->heavy_ops;
    if (budget > 1) {
        planner->palette_threads = 1;
        planner->allow_palette_async = 1;
        planner->main_threads = total - planner->palette_threads;
    } else {
        planner->main_threads = total;
    }

    sixel_encoding_planner_set_loader_metadata(
        planner,
        planner->loader_multiframe_known,
        planner->loader_multiframe);

    sixel_encoding_planner_plan_pipeline(planner, encoder, frame);

    sixel_encoding_planner_replan(planner,
                                 encoder,
                                 frame,
                                 planner->allow_palette_async != 0);
}


void
sixel_encoding_planner_replan(sixel_encoding_planner_t *planner,
                              sixel_encoder_t *encoder,
                              sixel_frame_t *frame,
                              int palette_ready)
{
    int palette_branch_active;

    if (planner == NULL || encoder == NULL || frame == NULL) {
        return;
    }

    palette_branch_active = sixel_encoding_planner_replan_palette_branch(
        planner,
        encoder,
        frame,
        palette_ready);
    sixel_encoding_planner_build_dag(planner, encoder, palette_branch_active);
}

void
sixel_encoding_planner_plan_pipeline(sixel_encoding_planner_t *planner,
                                     sixel_encoder_t *encoder,
                                     sixel_frame_t *frame)
{
    char const *text;
    char *endptr;
    long parsed;
    int height;
    int nbands;
    int threads;
    int dither_threads;
    int encode_threads;
    int band_height;
    int overlap;
    int queue_depth;
    int dither_env_override;
    int pin_threads;
    int pin_env_override;
    int ncolors;

    text = NULL;
    endptr = NULL;
    parsed = 0;
    height = 0;
    nbands = 0;
    threads = 0;
    dither_threads = 0;
    encode_threads = 0;
    band_height = 0;
    overlap = 0;
    queue_depth = 0;
    dither_env_override = 0;
    ncolors = SIXEL_PALETTE_MAX;

    if (planner == NULL || encoder == NULL || frame == NULL) {
        return;
    }

    planner->pipeline_active = 0;
    planner->pipeline_band_height = 0;
    planner->pipeline_overlap = 0;
    planner->pipeline_queue_depth = 0;
    planner->pipeline_dither_threads = 0;
    planner->pipeline_encode_threads = 0;
    planner->pipeline_bands = 0;
    planner->pipeline_pin_threads = 1;
    planner->loader_pipeline_active = 0;
    if (!planner->loader_multiframe_known) {
        planner->loader_multiframe = 0;
    }

    pin_threads = 1;
    pin_env_override = 0;

    text = sixel_compat_getenv("SIXEL_DITHER_PIN_THREADS");
    if (text != NULL && text[0] != '\0') {
        errno = 0;
        parsed = strtol(text, &endptr, 10);
        if (endptr != text && errno != ERANGE) {
            pin_env_override = 1;
            pin_threads = (parsed != 0) ? 1 : 0;
        }
    }
    planner->pipeline_pin_threads = pin_threads;
    (void)pin_env_override;

    height = sixel_frame_get_height(frame);
    threads = planner->main_threads;
    if (height <= 0 || threads <= 1) {
        return;
    }

    nbands = (height + 5) / 6;
    planner->pipeline_bands = nbands;
    if (nbands <= 1) {
        return;
    }

    dither_threads = (threads * 7 + 9) / 10;
    text = sixel_compat_getenv("SIXEL_DITHER_PARALLEL_THREADS_MAX");
    if (text != NULL && text[0] != '\0') {
        errno = 0;
        parsed = strtol(text, &endptr, 10);
        if (endptr != text && errno != ERANGE && parsed > 0) {
            if (parsed > INT_MAX) {
                parsed = INT_MAX;
            }
            dither_threads = (int)parsed;
            dither_env_override = 1;
        }
    }
    if (dither_threads < 1) {
        dither_threads = 1;
    }
    if (dither_threads > threads) {
        dither_threads = threads;
    }

    if (dither_env_override == 0 && threads >= 4 && dither_threads < 2) {
        dither_threads = threads - 2;
    }

    encode_threads = threads - dither_threads;
    if (encode_threads < 2 && threads > 2) {
        encode_threads = 2;
        dither_threads = threads - encode_threads;
    }
    if (encode_threads < 1) {
        encode_threads = 1;
        dither_threads = threads - encode_threads;
    }
    if (dither_threads < 1) {
        return;
    }

    text = sixel_compat_getenv("SIXEL_DITHER_PARALLEL_BAND_WIDTH");
    if (text != NULL && text[0] != '\0') {
        errno = 0;
        parsed = strtol(text, &endptr, 10);
        if (endptr != text && errno != ERANGE && parsed > 0) {
            if (parsed > INT_MAX) {
                parsed = INT_MAX;
            }
            band_height = (int)parsed;
        }
    }
    if (band_height <= 0) {
        band_height = (height + dither_threads - 1) / dither_threads;
    }
    if (band_height < 6) {
        band_height = 6;
    }
    if ((band_height % 6) != 0) {
        band_height = ((band_height + 5) / 6) * 6;
    }

    ncolors = encoder->reqcolors;
    if (ncolors <= 0 || ncolors > SIXEL_PALETTE_MAX) {
        ncolors = SIXEL_PALETTE_MAX;
    }
    text = sixel_compat_getenv("SIXEL_DITHER_PARALLEL_BAND_OVERWRAP");
    if (ncolors <= 32) {
        overlap = 6;
    } else {
        overlap = 0;
    }
    if (text != NULL && text[0] != '\0') {
        errno = 0;
        parsed = strtol(text, &endptr, 10);
        if (endptr != text && errno != ERANGE && parsed >= 0) {
            if (parsed > INT_MAX) {
                parsed = INT_MAX;
            }
            overlap = (int)parsed;
        }
    }
    if (overlap < 0) {
        overlap = 0;
    }
    if (overlap > band_height / 2) {
        overlap = band_height / 2;
    }

    queue_depth = threads * 3;
    if (queue_depth > nbands) {
        queue_depth = nbands;
    }
    if (queue_depth < 1) {
        queue_depth = 1;
    }

    planner->pipeline_active = 1;
    planner->pipeline_band_height = band_height;
    planner->pipeline_overlap = overlap;
    planner->pipeline_queue_depth = queue_depth;
    planner->pipeline_dither_threads = dither_threads;
    planner->pipeline_encode_threads = encode_threads;
    planner->pipeline_pin_threads = pin_threads;
}


void
sixel_encoding_planner_set_loader_metadata(sixel_encoding_planner_t *planner,
                                           int multiframe_known,
                                           int multiframe)
{
    int allow_pipeline;

    allow_pipeline = 0;

    if (planner == NULL) {
        return;
    }

    planner->loader_multiframe_known = multiframe_known != 0 ? 1 : 0;
    planner->loader_multiframe = multiframe != 0 ? 1 : 0;

    /*
     * Loader/encoder handoff starts in serial mode and is enabled only after
     * the loader reports multiframe metadata. Restrict the handoff pipeline to
     * runs where the main path can spare at least one extra thread.
     */
    allow_pipeline = planner->main_threads > 1
        && planner->loader_multiframe_known != 0
        && planner->loader_multiframe != 0;
    planner->loader_pipeline_active = allow_pipeline ? 1 : 0;
}

int
sixel_encoding_planner_update_loader_handoff(
    sixel_encoding_planner_t *planner,
    sixel_encoder_t *encoder,
    sixel_frame_t *frame)
{
    int multiframe;

    multiframe = 0;

    if (planner == NULL || frame == NULL) {
        return 0;
    }

    /*
     * The loader callback only reports frame metadata and asks the planner
     * for the current handoff decision. Keep this path free from DAG
     * replanning so callback-side policy remains minimal and deterministic.
     */
    multiframe = sixel_frame_get_multiframe(frame);
    if (encoder != NULL && encoder->fstatic != 0) {
        multiframe = 0;
    }
    sixel_encoding_planner_set_loader_metadata(planner, 1, multiframe);

    return planner->loader_pipeline_active;
}





static char const *
sixel_encoding_planner_pixelformat_label(int pixelformat)
{
    switch (pixelformat) {
    case SIXEL_PIXELFORMAT_RGB555:
        return "rgb555";
    case SIXEL_PIXELFORMAT_RGB565:
        return "rgb565";
    case SIXEL_PIXELFORMAT_RGB888:
        return "rgb888";
    case SIXEL_PIXELFORMAT_BGR555:
        return "bgr555";
    case SIXEL_PIXELFORMAT_BGR565:
        return "bgr565";
    case SIXEL_PIXELFORMAT_BGR888:
        return "bgr888";
    case SIXEL_PIXELFORMAT_ARGB8888:
        return "argb8888";
    case SIXEL_PIXELFORMAT_RGBA8888:
        return "rgba8888";
    case SIXEL_PIXELFORMAT_ABGR8888:
        return "abgr8888";
    case SIXEL_PIXELFORMAT_BGRA8888:
        return "bgra8888";
    case SIXEL_PIXELFORMAT_PAL1:
        return "pal1";
    case SIXEL_PIXELFORMAT_PAL2:
        return "pal2";
    case SIXEL_PIXELFORMAT_PAL4:
        return "pal4";
    case SIXEL_PIXELFORMAT_PAL8:
        return "pal8";
    case SIXEL_PIXELFORMAT_G1:
        return "g1";
    case SIXEL_PIXELFORMAT_G2:
        return "g2";
    case SIXEL_PIXELFORMAT_G4:
        return "g4";
    case SIXEL_PIXELFORMAT_G8:
        return "g8";
    case SIXEL_PIXELFORMAT_AG88:
        return "ag88";
    case SIXEL_PIXELFORMAT_GA88:
        return "ga88";
    case SIXEL_PIXELFORMAT_RGBFLOAT32:
        return "rgb-f32";
    case SIXEL_PIXELFORMAT_LINEARRGBFLOAT32:
        return "linear-f32";
    case SIXEL_PIXELFORMAT_OKLABFLOAT32:
        return "oklab-f32";
    case SIXEL_PIXELFORMAT_CIELABFLOAT32:
        return "cielab-f32";
    case SIXEL_PIXELFORMAT_DIN99DFLOAT32:
        return "din99d-f32";
    default:
        return "unknown";
    }
}


void
sixel_encoding_planner_dump(sixel_encoding_planner_t *planner,
                            sixel_encoder_t *encoder,
                            sixel_frame_t *frame,
                            int palette_ready)
{
    FILE *stream;
    int i;
    sixel_planner_edge_t const *edge;
    sixel_planner_node_t const *from;
    sixel_planner_node_t const *to;

    if (planner == NULL || encoder == NULL || frame == NULL) {
        return;
    }

    stream = stderr;
    edge = NULL;
    from = NULL;
    to = NULL;
    sixel_encoding_planner_replan(planner, encoder, frame,
                                  palette_ready);

    fprintf(stream, "[planner] DAG (Directed Acyclic Graph)\n");
    fprintf(stream,
            "  workers: total=%d main=%d palette=%d heavy_ops=%d\n",
            planner->total_threads,
            planner->main_threads,
            planner->palette_threads,
            planner->heavy_ops);
    fprintf(stream,
            "  formats: source=%s work=%s scale_out=%s\n",
            sixel_encoding_planner_pixelformat_label(
                sixel_frame_get_pixelformat(frame)),
            sixel_encoding_planner_pixelformat_label(
                planner->working_pixelformat),
            sixel_encoding_planner_pixelformat_label(
                planner->scale_pixelformat));
    fprintf(stream,
            "  resize: mode=%d input=%s\n",
            planner->resize_precision_mode,
            sixel_encoding_planner_pixelformat_label(
                planner->scale_input_pixelformat));
    fprintf(stream,
            "  loader: multiframe=%s handoff=%s\n",
            planner->loader_multiframe_known != 0
                ? (planner->loader_multiframe != 0 ? "yes" : "no")
                : "unknown",
            planner->loader_pipeline_active != 0 ? "pipeline" : "serial");

    fprintf(stream, "  nodes:\n");
    for (i = 0; i < planner->dag_node_count; ++i) {
        fprintf(stream, "    %s\n", planner->dag_nodes[i].label);
    }

    fprintf(stream, "  edges:\n");
    for (i = 0; i < planner->dag_edge_count; ++i) {
        edge = &planner->dag_edges[i];
        if (edge->from < 0 || edge->from >= planner->dag_node_count
            || edge->to < 0 || edge->to >= planner->dag_node_count) {
            continue;
        }
        from = &planner->dag_nodes[edge->from];
        to = &planner->dag_nodes[edge->to];
        if (edge->pipeline != 0) {
            fprintf(stream,
                    "    %s -> %s (pipeline)\n",
                    from->label,
                    to->label);
        } else {
            fprintf(stream, "    %s -> %s\n", from->label, to->label);
        }
    }

    fprintf(stream, "  pipeline:\n");
    fprintf(stream, "    bands=%d queue=%d mode=%s\n",
            planner->pipeline_bands,
            planner->pipeline_queue_depth,
            planner->pipeline_active != 0 ? "pipeline" : "serial");
    fprintf(stream,
            "    band_height=%d overlap=%d threads: dither=%d encode=%d\n",
            planner->pipeline_band_height,
            planner->pipeline_overlap,
            planner->pipeline_dither_threads,
            planner->pipeline_encode_threads);
}


/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
