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
#include "gpu-palette.h"
#include "loader-common.h"
#include "options.h"
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
sixel_encoding_planner_resolve_band_overwrap(
    sixel_encoder_t const *encoder,
    unsigned int *value);
static int
sixel_encoding_planner_resolve_band_width(
    sixel_encoder_t const *encoder,
    unsigned int *value);
static int
sixel_encoding_planner_resolve_threads_max(
    sixel_encoder_t const *encoder,
    unsigned int *value);
static int
sixel_encoding_planner_resolve_pin_threads(
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
    float resolved;

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

    if (!sixel_option_resolve_registered_float_binding(
            SIXEL_OPTION_SCHEMA_DIFFUSION,
            "bluenoise",
            SIXEL_SUBOPTION_BINDING_ID_2(
                bluenoise_gradient_factor,
                bluenoise_gradient_factor_override),
            &resolved)) {
        return 0.0f;
    }
    if (resolved <= 0.0f) {
        return 0.0f;
    }

    return resolved;
}

/*
 * Prefer the request-bound value and preserve the registered environment for
 * callers that build an encoder without passing through CLI option parsing.
 */
static int
sixel_encoding_planner_resolve_band_overwrap(
    sixel_encoder_t const *encoder,
    unsigned int *value)
{
    if (encoder == NULL || value == NULL) {
        return 0;
    }
    if (encoder->dither_parallel_band_overwrap_override != 0) {
        *value = encoder->dither_parallel_band_overwrap;
        return 1;
    }

    return sixel_option_resolve_registered_uint_binding(
        SIXEL_OPTION_SCHEMA_DIFFUSION,
        NULL,
        SIXEL_SUBOPTION_BINDING_ID_2(
            dither_parallel_band_overwrap,
            dither_parallel_band_overwrap_override),
        value);
}

/* Resolve the request-local band height before the environment fallback. */
static int
sixel_encoding_planner_resolve_band_width(
    sixel_encoder_t const *encoder,
    unsigned int *value)
{
    if (encoder == NULL || value == NULL) {
        return 0;
    }
    if (encoder->dither_parallel_band_width_override != 0) {
        *value = encoder->dither_parallel_band_width;
        return 1;
    }

    return sixel_option_resolve_registered_uint_binding(
        SIXEL_OPTION_SCHEMA_DIFFUSION,
        NULL,
        SIXEL_SUBOPTION_BINDING_ID_2(
            dither_parallel_band_width,
            dither_parallel_band_width_override),
        value);
}

/* prepare_workers caps scale, colorspace, sampling and palette workers.
 * It never reduces the dither/encode reservation planned later. */
static int
sixel_encoding_planner_prepare_threads(int total)
{
    unsigned int limit;

    limit = 0u;
    if (total < 1) {
        total = 1;
    }
    if (sixel_threading_policy_stage_threads(SIXEL_THREADING_STAGE_PREPARE,
                                             &limit) &&
            limit >= 1u && limit < (unsigned int)total) {
        return (int)limit;
    }
    return total;
}

/* Threads left to the caller beside the early palette worker. This is the
 * uncapped main reservation, so prepare_workers never disables loading
 * ahead. */
static int
sixel_encoding_planner_loader_threads(
    sixel_encoding_planner_t const *planner)
{
    int total;

    total = planner->total_threads > 0 ? planner->total_threads : 1;
    if (planner->allow_palette_async != 0) {
        return total - total / 2;
    }
    return total;
}

/* The active object snapshot supplies the dither worker cap. */
static int
sixel_encoding_planner_resolve_threads_max(
    sixel_encoder_t const *encoder,
    unsigned int *value)
{
    if (encoder == NULL || value == NULL) {
        return 0;
    }

    return sixel_threading_policy_stage_threads(
        SIXEL_THREADING_STAGE_DITHER, value);
}

/* Dither workers stay pinned unless the threading policy says otherwise. */
static int
sixel_encoding_planner_resolve_pin_threads(
    sixel_encoder_t const *encoder)
{
    (void)encoder;
    return sixel_threading_policy_pin(1);
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
    planner->palette_latest = 0;
    planner->prepare_threads = 1;
    planner->main_threads = 1;
    planner->palette_threads = 0;
    planner->emit_threads = 0;
    planner->next_prepare_threads = 0;
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
    planner->pipeline_staged = 0;
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
sixel_encoding_palette_job_eligible(sixel_encoder_t *encoder,
                                    sixel_frame_t *frame)
{
    int pixelformat;

    if (encoder == NULL || frame == NULL) {
        return 0;
    }

    if (encoder->palette_job_enabled == 0) {
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

int
sixel_encoding_palette_job_ready(sixel_encoder_t *encoder,
                                 sixel_encoding_planner_t *planner,
                                 sixel_frame_t *frame)
{
    if (planner == NULL || planner->allow_palette_async == 0) {
        return 0;
    }
    return sixel_encoding_palette_job_eligible(encoder, frame);
}


void
sixel_encoding_planner_analyze(sixel_encoding_planner_t *planner,
                               sixel_encoder_t *encoder,
                               sixel_frame_t *frame)
{
    int total;
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
    int colorspace_before_scale;
    int colorspace_after_scale;
    int working_colorspace_effective;
    int resize_mode;
    int effective_reqcolors;

    if (planner == NULL || encoder == NULL || frame == NULL) {
        return;
    }

    resize_mode = SIXEL_PLANNER_RESIZE_MODE_PRESERVE;
    colorspace_after_scale = 0;
    colorspace_before_scale = 0;
    working_colorspace_effective = encoder->working_colorspace;
    source_pixelformat = sixel_frame_get_pixelformat(frame);
    source_is_float32 = SIXEL_PIXELFORMAT_IS_FLOAT32(source_pixelformat);
    source_ncolors = sixel_frame_get_ncolors(frame);
    effective_reqcolors = encoder->reqcolors;
    if (effective_reqcolors <= 0 || effective_reqcolors > SIXEL_PALETTE_MAX) {
        effective_reqcolors = SIXEL_PALETTE_MAX;
    }

    /*
     * Describe work that consumes thread capacity. The scheduler uses this
     * profile for worker admission, never to select the sampling population.
     */
    total = sixel_threads_resolve();
    planner->total_threads = total;
    planner->prepare_threads = sixel_encoding_planner_prepare_threads(total);
    planner->palette_threads = 0;
    planner->allow_palette_async = 0;
    planner->main_threads = planner->prepare_threads;
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
    if (source_is_float32 ||
        encoder->working_colorspace != source_colorspace) {
        /* A byte preference never discards precision supplied by a loader
         * or required by a coordinate transform. Explicit float additionally
         * promotes an otherwise sufficient byte path. */
        prefer_float32 = 1;
    }
    /* Preserve the requested work precision even when resize itself uses
     * preserve or a temporary linear buffer instead of float-work mode. */
    prefer_float32_effective = prefer_float32;
    scale_pixelformat = SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
    scale_input_pixelformat = sixel_frame_get_pixelformat(frame);
    float_resize_required = (planner->scale_active != 0) ? 1 : 0;
    source_width = sixel_frame_get_width(frame);
    source_height = sixel_frame_get_height(frame);
    if (prefer_float32 != 0) {
        resize_mode = SIXEL_PLANNER_RESIZE_MODE_FLOAT_WORK;
    } else if (planner->scale_active != 0
               || encoder->working_colorspace != source_colorspace) {
        resize_mode = SIXEL_PLANNER_RESIZE_MODE_LINEAR32;
    }
    resize_mode = sixel_runtime_policy_resize_precision(resize_mode);
    sixel_trace_topic_message(
        "runtime_contract",
        "LSXRT1|resize_precision=%d",
        resize_mode);

    if (resize_mode == SIXEL_PLANNER_RESIZE_MODE_FLOAT_WORK) {
        prefer_float32_effective = 1;
    }

    target_pixelformat = sixel_planner_pixelformat_for_colorspace(
        working_colorspace_effective,
        prefer_float32_effective);

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
        case SIXEL_PLANNER_RESIZE_MODE_PRESERVE:
            /*
             * Keep integer inputs in byte RGB when the caller explicitly
             * accepts the accuracy trade-off to reduce peak memory. RGB
             * float inputs can already be resized without promotion.
             */
            if (source_pixelformat == SIXEL_PIXELFORMAT_RGBFLOAT32
                || source_pixelformat ==
                       SIXEL_PIXELFORMAT_LINEARRGBFLOAT32) {
                scale_input_pixelformat = source_pixelformat;
            } else {
                scale_input_pixelformat = SIXEL_PIXELFORMAT_RGB888;
            }
            break;
        case SIXEL_PLANNER_RESIZE_MODE_LINEAR32:
        case SIXEL_PLANNER_RESIZE_MODE_FLOAT_WORK:
            /*
             * Non-RGB workspaces cannot be resampled directly. Force a
             * linear RGB buffer for scaling and convert back afterward.
             */
            scale_input_pixelformat = SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
            break;
        default:
            /* The runtime option parser must reject unknown modes. */
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
}

SIXELSTATUS
sixel_encoding_planner_schedule(
    sixel_encoding_planner_t *planner,
    sixel_encoder_t *encoder,
    sixel_frame_t *frame,
    sixel_palette_sampling_policy_t sampling_policy)
{
    int budget;
    int total;
    int async_eligible;

    if (planner == NULL || encoder == NULL || frame == NULL ||
            (sampling_policy != SIXEL_PALETTE_SAMPLING_FULL_FRAME &&
             sampling_policy != SIXEL_PALETTE_SAMPLING_ADAPTIVE_GRID)) {
        return SIXEL_BAD_ARGUMENT;
    }
    /* The early palette overlap splits the prepare reservation only. */
    planner->prepare_threads =
        sixel_encoding_planner_prepare_threads(planner->total_threads);
    total = planner->prepare_threads;
    async_eligible = sixel_encoding_palette_job_eligible(encoder, frame);
    planner->palette_threads = 0;
    planner->allow_palette_async = 0;
    planner->main_threads = total > 0 ? total : 1;

    if (total <= 1) {
        return SIXEL_OK;
    }

    budget = total - planner->heavy_ops;
    if (sampling_policy == SIXEL_PALETTE_SAMPLING_ADAPTIVE_GRID &&
            budget > 1 && async_eligible != 0) {
        /* Split the early overlap evenly. Each side includes its caller;
         * float converters use N-1 children plus that caller. In particular,
         * the palette worker may parallelize -X without duplicating N. */
        planner->palette_threads = total / 2;
        planner->allow_palette_async = 1;
        planner->main_threads = total - planner->palette_threads;
        if (sixel_threading_policy_mode() == SIXEL_THREADING_POLICY_LAZY) {
            /* Independent branches may overlap without data fan-out. */
            planner->palette_threads = 1;
            planner->main_threads = 1;
        }
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

    return SIXEL_OK;
}

void
sixel_encoding_planner_plan(sixel_encoding_planner_t *planner,
                            sixel_encoder_t *encoder,
                            sixel_frame_t *frame)
{
    SIXELSTATUS status;
    sixel_palette_sampling_resolver_input_t input;
    sixel_palette_sampling_selection_t selection;
    sixel_palette_sampling_policy_t sampling_policy;

    status = SIXEL_FALSE;
    input.requested = SIXEL_PALETTE_SAMPLING_AUTO;
    input.adaptive_eligible = 0;
    selection.effective = SIXEL_PALETTE_SAMPLING_FULL_FRAME;
    selection.source =
        SIXEL_PALETTE_SAMPLING_SOURCE_PREPROCESSED_FRAME;
    selection.reason = SIXEL_PALETTE_RESOLUTION_NONE;
    sampling_policy = SIXEL_PALETTE_SAMPLING_FULL_FRAME;
    if (planner == NULL || encoder == NULL || frame == NULL) {
        return;
    }

    sixel_encoding_planner_analyze(planner, encoder, frame);
    /*
     * Loader handoff uses this compatibility wrapper before the encode-frame
     * policy state exists.  It still avoids reserving a palette worker when
     * the frame cannot use one.  The encode DAG records the same resolution
     * through analyze, resolve, and schedule explicitly.
     */
    input.requested = (sixel_palette_sampling_policy_t)
        encoder->palette_sampling_policy;
    input.adaptive_eligible =
        sixel_encoding_palette_job_eligible(encoder, frame);
    status = sixel_palette_sampling_select(&input, &selection);
    if (SIXEL_FAILED(status)) {
        return;
    }
    sampling_policy = selection.effective;
    (void)sixel_encoding_planner_schedule(planner,
                                          encoder,
                                          frame,
                                          sampling_policy);
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

/* Small budgets benefit from giving each phase the whole reservation.
 * Beyond four slots, retain row overlap instead of extrapolating that gain.
 * Use the effective method: exact-fit palettes can turn diffusion into NONE,
 * whose bulk lookup path does not justify the same allocation change. */
int
sixel_dither_encode_default_staged(int total_threads, int method_for_diffuse)
{
    if (total_threads < 2 || total_threads > 4) {
        return 0;
    }
    switch (method_for_diffuse) {
    case SIXEL_DIFFUSE_AUTO:
    case SIXEL_DIFFUSE_FS:
    case SIXEL_DIFFUSE_ATKINSON:
    case SIXEL_DIFFUSE_JAJUNI:
    case SIXEL_DIFFUSE_STUCKI:
    case SIXEL_DIFFUSE_BURKES:
    case SIXEL_DIFFUSE_SIERRA1:
    case SIXEL_DIFFUSE_SIERRA2:
    case SIXEL_DIFFUSE_SIERRA3:
    case SIXEL_DIFFUSE_LSO2:
    case SIXEL_DIFFUSE_A_DITHER:
    case SIXEL_DIFFUSE_X_DITHER:
    case SIXEL_DIFFUSE_BLUENOISE_DITHER:
        return 1;
    default:
        return 0;
    }
}

void
sixel_dither_encode_allocate(sixel_dither_encode_request_t const *request,
                              sixel_dither_encode_allocation_t *allocation)
{
    int threads;
    int dither_threads;
    int encode_threads;
    int band_height;
    int overlap;
    int staged;
    long long requested_total;

    if (allocation == NULL) {
        return;
    }
    allocation->mode = SIXEL_DITHER_ENCODE_SERIAL;
    allocation->dither_threads = 1;
    allocation->encode_threads = 1;
    allocation->band_height = 0;
    allocation->overlap = 0;
    if (request == NULL) {
        return;
    }
    threads = request->total_threads;
    if (threads <= 1 || request->height <= 0) {
        return;
    }
    allocation->encode_threads = threads;
    if (request->gpu_encode_only) {
        /* GPU application has no CPU producer reservation. Preserve the
         * full encode budget, independently of CPU geometry overrides. */
        if (request->encode_threads_max_set &&
                request->encode_threads_max >= 1u &&
                request->encode_threads_max < (unsigned int)threads) {
            allocation->encode_threads = (int)request->encode_threads_max;
        }
        allocation->mode = SIXEL_DITHER_ENCODE_PIPELINE;
        allocation->dither_threads = 0;
        allocation->band_height = 6;
        return;
    }

    staged = !request->lazy && request->prefer_staged
        && !request->threads_max_set
        && !request->encode_threads_max_set;
    /* Only the spatial policies support independent work bands. A staged
     * unknown policy would lose overlap without gaining parallel
     * application. Keep its existing reservation instead. */
    switch (request->method_for_diffuse) {
    case SIXEL_DIFFUSE_AUTO:
    case SIXEL_DIFFUSE_NONE:
    case SIXEL_DIFFUSE_FS:
    case SIXEL_DIFFUSE_ATKINSON:
    case SIXEL_DIFFUSE_JAJUNI:
    case SIXEL_DIFFUSE_STUCKI:
    case SIXEL_DIFFUSE_BURKES:
    case SIXEL_DIFFUSE_SIERRA1:
    case SIXEL_DIFFUSE_SIERRA2:
    case SIXEL_DIFFUSE_SIERRA3:
    case SIXEL_DIFFUSE_LSO2:
    case SIXEL_DIFFUSE_A_DITHER:
    case SIXEL_DIFFUSE_X_DITHER:
    case SIXEL_DIFFUSE_BLUENOISE_DITHER:
        break;
    default:
        staged = 0;
        break;
    }
#if !SIXEL_ENABLE_THREADS || defined(__PCC__) || defined(__TINYC__)
    /* Match the application path's platform-specific serial restriction. */
    staged = 0;
#endif
    if (staged) {
        dither_threads = threads;
        encode_threads = threads;
    } else {
        /* ceil(0.7 * threads), without overflowing the multiplication. */
        dither_threads = (threads / 10) * 7
            + ((threads % 10) * 7 + 9) / 10;
        if (request->threads_max_set) {
            dither_threads = request->threads_max > (unsigned int)INT_MAX
                ? INT_MAX : (int)request->threads_max;
        } else if (request->lazy) {
            dither_threads = 1;
        } else if (request->encode_threads_max_set &&
                   request->encode_threads_max < (unsigned int)threads) {
            dither_threads = threads - (int)request->encode_threads_max;
        }
        if (dither_threads < 1) {
            dither_threads = 1;
        }
        if (dither_threads > threads) {
            dither_threads = threads;
        }
        if (request->encode_threads_max_set) {
            encode_threads = request->encode_threads_max > (unsigned int)INT_MAX
                ? INT_MAX : (int)request->encode_threads_max;
            if (encode_threads < 1) {
                encode_threads = 1;
            }
            requested_total = (long long)dither_threads + encode_threads;
            if (requested_total > threads) {
                /* Stage limits never fail the plan. Shrink both in
                 * proportion so their sum fits the total budget. */
                dither_threads = (int)((long long)dither_threads * threads
                                       / requested_total);
                if (dither_threads < 1) {
                    dither_threads = 1;
                }
                if (dither_threads > threads - 1) {
                    dither_threads = threads - 1;
                }
                encode_threads = threads - dither_threads;
            }
        } else {
            encode_threads = threads - dither_threads;
            /* Explicit caps retain the existing minimum encode
             * reservation. */
            if (encode_threads < 2 && threads > 2) {
                encode_threads = 2;
                dither_threads = threads - encode_threads;
            }
            if (encode_threads < 1) {
                encode_threads = 1;
                dither_threads = threads - encode_threads;
            }
        }
        if (encode_threads <= 1) {
            /* The legacy fallback applies serially, then lets the encoder
             * use the full budget. Its E=1 split never starts a pipeline. */
            return;
        }
    }

    band_height = request->band_height_set
        ? (int)request->band_height : 0;
    if (band_height <= 0) {
        band_height = request->height / dither_threads
            + (request->height % dither_threads != 0);
    }
    if (band_height < 6) {
        band_height = 6;
    }
    if (band_height > INT_MAX - 5) {
        band_height = INT_MAX - (INT_MAX % 6);
    } else if (band_height % 6 != 0) {
        band_height = ((band_height + 5) / 6) * 6;
    }
    overlap = request->ncolors <= 32 ? 6 : 0;
    /* These policies depend only on the current pixel and its absolute
     * position. Replaying preceding rows cannot warm any residual state. */
    switch (request->method_for_diffuse) {
    case SIXEL_DIFFUSE_NONE:
    case SIXEL_DIFFUSE_A_DITHER:
    case SIXEL_DIFFUSE_X_DITHER:
    case SIXEL_DIFFUSE_BLUENOISE_DITHER:
        overlap = 0;
        break;
    default:
        break;
    }
    if (request->overlap_set) {
        overlap = (int)request->overlap;
    }
    if (overlap < 0) {
        overlap = 0;
    }
    if (overlap > band_height / 2) {
        overlap = band_height / 2;
    }

    allocation->mode = staged ? SIXEL_DITHER_ENCODE_STAGED
        : SIXEL_DITHER_ENCODE_PIPELINE;
    allocation->dither_threads = dither_threads;
    allocation->encode_threads = encode_threads;
    allocation->band_height = band_height;
    allocation->overlap = overlap;
}

void
sixel_encoding_planner_plan_pipeline(sixel_encoding_planner_t *planner,
                                     sixel_encoder_t *encoder,
                                     sixel_frame_t *frame)
{
    size_t pixel_count;
    int width;
    int height;
    int nbands;
    int threads;
    int queue_depth;
    int pin_threads;
    sixel_dither_encode_request_t request = {0};
    sixel_dither_encode_allocation_t allocation;

    if (planner == NULL || encoder == NULL || frame == NULL) {
        return;
    }

    planner->pipeline_active = 0;
    planner->pipeline_staged = 0;
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

    pin_threads = sixel_encoding_planner_resolve_pin_threads(encoder);
    planner->pipeline_pin_threads = pin_threads;
    height = sixel_frame_get_height(frame);
    width = sixel_frame_get_width(frame);
    /* Latest fixes the same full emission budget as frame mode. Live CPU
     * loans never alter allocator geometry or its stage limits. */
    threads = planner->emit_threads > 0
        ? planner->emit_threads : planner->total_threads;
    if (width <= 0 || height <= 0 || threads <= 1) {
        return;
    }
    if ((size_t)width > SIZE_MAX / (size_t)height) {
        return;
    }
    pixel_count = (size_t)width * (size_t)height;
    nbands = height / 6 + (height % 6 != 0);
    planner->pipeline_bands = nbands;
    if (nbands <= 1) {
        return;
    }

    request.total_threads = threads;
    request.height = height;
    request.ncolors = encoder->reqcolors;
    if (request.ncolors <= 0 || request.ncolors > SIXEL_PALETTE_MAX) {
        request.ncolors = SIXEL_PALETTE_MAX;
    }
    /* This pre-palette estimate uses the request. Runtime invokes the same
     * allocator with the completed palette and effective diffusion method. */
    request.method_for_diffuse = encoder->method_for_diffuse;
    request.lazy = sixel_threading_policy_mode() ==
        SIXEL_THREADING_POLICY_LAZY;
    request.prefer_staged = sixel_dither_encode_default_staged(
        threads, request.method_for_diffuse);
    request.gpu_encode_only = sixel_gpu_palette_policy_claims_apply_stage(
        encoder->gpu_policy, encoder->gpu_palette_threshold,
        encoder->lut_policy, encoder->method_for_diffuse,
        encoder->method_for_scan, pixel_count);
    request.threads_max_set = sixel_encoding_planner_resolve_threads_max(
        encoder, &request.threads_max);
    request.encode_threads_max_set = sixel_threading_policy_stage_threads(
        SIXEL_THREADING_STAGE_ENCODE, &request.encode_threads_max);
    request.band_height_set = sixel_encoding_planner_resolve_band_width(
        encoder, &request.band_height);
    request.overlap_set = sixel_encoding_planner_resolve_band_overwrap(
        encoder, &request.overlap);
    sixel_dither_encode_allocate(&request, &allocation);

    /* The queue belongs to the initial encode reservation. In staged mode
     * it is created only after the application workers have all joined. */
    queue_depth = nbands;
    if (allocation.encode_threads <= nbands / 3) {
        queue_depth = allocation.encode_threads * 3;
    }
    planner->pipeline_active =
        allocation.mode == SIXEL_DITHER_ENCODE_PIPELINE;
    planner->pipeline_staged =
        allocation.mode == SIXEL_DITHER_ENCODE_STAGED;
    planner->pipeline_band_height = allocation.band_height;
    planner->pipeline_overlap = allocation.overlap;
    planner->pipeline_queue_depth = queue_depth;
    planner->pipeline_dither_threads = allocation.dither_threads;
    planner->pipeline_encode_threads = allocation.encode_threads;
}

void
sixel_encoding_planner_plan_emit(sixel_encoding_planner_t *planner,
    sixel_encoder_t *encoder, sixel_frame_t *frame)
{
    /* Preparation may have run with Q=1. Never reuse that estimate for
     * application: both this plan and runtime allocate from the live M. */
    planner->emit_threads = sixel_threads_resolve();
    planner->main_threads = planner->emit_threads;
    sixel_encoding_planner_plan_pipeline(planner, encoder, frame);
    sixel_encoding_planner_replan(planner, encoder, frame, 0);
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
    allow_pipeline = sixel_encoding_planner_loader_threads(planner) > 1
        && planner->loader_multiframe_known != 0
        && planner->loader_multiframe != 0
        && sixel_threading_policy_load_mode() == SIXEL_THREADING_FRAME_AHEAD;
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
            "  threading: mode=%s budget=%d "
            "load=%s load_depth=%d prepare=%s "
            "prepare_workers=%d pin=%d palette=%s palette_effective=%s\n",
            sixel_threading_policy_mode_name(),
            planner->total_threads,
            sixel_threading_policy_load_mode() == SIXEL_THREADING_FRAME_AHEAD
                ? "ahead" : "serial",
            sixel_threading_policy_load_depth(),
            sixel_threading_policy_prepare_mode() ==
                    SIXEL_THREADING_FRAME_AHEAD
                ? "ahead" : "serial",
            planner->prepare_threads,
            planner->pipeline_pin_threads,
            sixel_threading_policy_palette_mode() ==
                    SIXEL_THREADING_PALETTE_LATEST
                ? "latest" : "frame",
            planner->palette_latest ? "latest" : "frame");
    if (planner->emit_threads > 0) {
        fprintf(stream, "  latest leases: palette=%d prepare=%d emit=%d\n",
            planner->palette_threads, planner->next_prepare_threads,
            planner->emit_threads);
    }
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
    fprintf(stream, "    resolution=pre-palette-estimate\n");
    fprintf(stream, "    bands=%d queue=%d mode=%s\n",
            planner->pipeline_bands,
            planner->pipeline_queue_depth,
            planner->pipeline_active != 0 ? "pipeline"
                : planner->pipeline_staged != 0 ? "staged" : "serial");
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
