/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#if defined(HAVE_VULKAN) && !defined(HAVE_METAL)
#include <stdint.h>
#include <string.h>

#include "filter-binning.h"
#include "filter-factory.h"
#include "filter-sample.h"
#include "gpu-compute-vulkan.h"
#include "palette-common-cover.h"
#include "palette-common-merge.h"
#include "palette-common-snap.h"
#include "palette-kmeans-gpu.h"
#include "palette-resident-vulkan.h"
#include "pixelformat.h"

#define GRID_TILE 128u
#define HB_CELLS (64u * 64u * 64u)
#define SAMPLE_LIMIT 32768u
#define HB_MOMENTS (HB_CELLS * 4u)
#define HB_TILES (HB_MOMENTS + SAMPLE_LIMIT * 20u)
#define COVER_BINS (HB_TILES + 256u * 4u)
#define AUX_BYTES (COVER_BINS + 4096u * 20u)
#define RESULT_BYTES (8u + 3u * SIXEL_PALETTE_MAX)
#define INDIRECT_PARTIAL 12u
#define INDIRECT_LLOYD 24u
#define MERGE_CONTROL 36u

enum resident_slot {
    RES_RGB, RES_GEOMETRY, RES_POINTS, RES_WEIGHTS, RES_CENTERS, RES_PARAMS,
    RES_HB_PARAMS,
    RES_HB_ORDER, RES_HB_BOXES, RES_HB_RESULT, RES_AUX, RES_RESULT, RES_LUT,
    RES_INDICES, RES_INDIRECT, RES_STATE,
    RES_MEMBERSHIP, RES_DISTANCES, RES_PARTIALS, RES_SUMS, RES_MAXIMA,
    RES_HB_SCRATCH, WD_WORK, WD_MEANS, WD_ROWS, WD_PARAMS, WD_PAIRS,
    RES_BUFFER_COUNT
};

enum resident_stage {
    RES_HISTOGRAM, RES_COUNT, RES_PREFIX, RES_COMPACT, RES_MOMENTS, RES_AXIS,
    RES_TILES, RES_MERGE, RES_SEED,
    RES_ASSIGN, RES_PARTIAL, RES_REDUCE, RES_UPDATE, RES_SCORE, RES_OBJECTIVE,
    RES_PACK,
    RES_HB_FIRST, RES_HB_ACCUMULATE, RES_HB_PREFIX, RES_HB_COMPACT,
    RES_HB_SOLVE, RES_HB_REP,
    RES_SNAP, RES_COVER_HIST, RES_COVER_GAPS, RES_COVER_REPAIR, RES_BUILD_LUT,
    RES_APPLY, RES_STAGE_COUNT
};

enum ward_stage {
    WD_KM_POINTS, WD_KM_INIT, WD_KM_COLLECT, WD_KM_CLUSTERS,
    WD_HB_POINTS, WD_HB_CLUSTERS, WD_SEED, WD_SCAN, WD_MERGE,
    WD_REPLAY, WD_CLEAR, WD_ASSIGN, WD_UPDATE, WD_PACK, WD_STAGE_COUNT
};

typedef struct resident_geometry {
    uint32_t width, height, stride, samples, colors, min_iterations;
    float threshold;
    uint32_t cover, soft, grow;
    uint32_t snap[256], tones[256], anchors[SIXEL_PALETTE_COVER_ANCHOR_MAX];
    uint32_t target, heckbert, representative;
} resident_geometry_t;

typedef struct resident_binning {
    VkCommandBuffer command;
    uint32_t subgroup_width;
    int heckbert;
} resident_binning_t;

static char const *const resident_source[] = {
#include "generated-vulkan/palette-kmeans-resident-shader.inc.h"
#include "generated-vulkan/lookup-policy-6bit-gpu-shader.inc.h"
};
static char const *const resident_lloyd_source[] = {
#include "generated-vulkan/palette-kmeans-gpu-shader.inc.h"
};
static char const *const resident_heckbert_source[] = {
#include "generated-vulkan/palette-heckbert-gpu-shader.inc.h"
};

static char const *const ward_refine_source[] = {
#include "generated-vulkan/palette-ward-resident-shader.inc.h"
};
static char const *const ward_pairs_source[] = {
#include "generated-vulkan/palette-ward-shader.inc.h"
};

#define WARD_WORK_BYTES \
    (24u * SAMPLE_LIMIT + 128u * SIXEL_WARD_MAX_CLUSTERS + 32u)

static sixel_vk_compute_t resident_pipelines[RES_STAGE_COUNT];
static sixel_vk_compute_t ward_pipelines[WD_STAGE_COUNT];
static sixel_vk_compute_t ward_heckbert_pipeline;
static sixel_vk_compute_t ward_merge_pipelines[6];
static sixel_vk_buffer_t resident_buffers[RES_BUFFER_COUNT];
static sixel_vk_submission_t resident_submission =
    SIXEL_VK_SUBMISSION_INITIALIZER;

static uint32_t
resident_groups(uint32_t items, uint32_t width)
{
    return (items + width - 1u) / width;
}

/* Only these two kernel families use a different binding layout. Their
 * pipelines consume the resident buffers directly without a host adapter. */
static SIXELSTATUS
resident_prepare_pipelines(sixel_vk_runtime_t *runtime, size_t const *length,
    uint32_t solve_width, int heckbert, int snap, int cover, int soft,
    int ward)
{
    static char const *const entries[RES_STAGE_COUNT] = {
        "resident_histogram", "resident_count", "resident_prefix",
        "resident_compact", "resident_moments", "resident_axis",
        "resident_tiles", "resident_merge", "resident_seed",
        "km_assign", "km_partial", "km_reduce", "km_update",
        "resident_score", "resident_objective", "resident_pack",
        "resident_hb_first", "resident_hb_accumulate", "resident_hb_prefix",
        "resident_hb_compact", "heckbert_solve", "resident_hb_rep",
        "resident_snap", "resident_cover_hist", "resident_cover_gaps",
        "resident_cover", "lookup_6bit_build", "resident_apply"
    };
    static unsigned char const km_map[] = {
        RES_POINTS, RES_WEIGHTS, RES_CENTERS, RES_MEMBERSHIP, RES_DISTANCES,
        RES_PARTIALS, RES_SUMS,
        RES_STATE, RES_MAXIMA, RES_PARAMS
    };
    static unsigned char const hb_map[] = {
        RES_POINTS, RES_HB_ORDER, RES_HB_SCRATCH, RES_HB_BOXES, RES_HB_PARAMS,
        RES_HB_RESULT
    };
    sixel_vk_buffer_t const *bound[SIXEL_VK_COMPUTE_BINDINGS];
    char const *const *source;
    unsigned char const *map;
    sixel_vk_compute_t *pipeline;
    unsigned int stage, num_bound, i, slot;
    uint32_t width, capacity;
    size_t chunks;
    SIXELSTATUS status;

    for (stage = heckbert ? RES_HB_FIRST : RES_HISTOGRAM;
         stage < RES_STAGE_COUNT;
         ++stage) {
        if ((!heckbert && stage >= RES_HB_FIRST && stage <= RES_HB_REP) ||
                (ward && (stage == RES_HB_REP ||
                    (stage >= RES_SCORE && stage <= RES_PACK))) ||
                (stage == RES_SNAP && !snap) ||
                ((stage == RES_COVER_HIST ||
                  stage == RES_COVER_GAPS) && !soft) ||
                (stage == RES_COVER_REPAIR && !cover))
            continue;
        map = NULL;
        capacity = 0;
        pipeline = &resident_pipelines[stage];
        if (stage >= RES_ASSIGN && stage <= RES_UPDATE) {
            source = resident_lloyd_source;
            chunks = sizeof(resident_lloyd_source) /
                sizeof(resident_lloyd_source[0]);
            map = km_map;
            num_bound = sizeof(km_map);
            width = 0u;
        } else if (stage == RES_HB_SOLVE) {
            source = resident_heckbert_source;
            chunks = sizeof(resident_heckbert_source) /
                sizeof(resident_heckbert_source[0]);
            map = hb_map;
            num_bound = sizeof(hb_map);
            width = solve_width;
            if (ward) {
                capacity = SIXEL_WARD_MAX_CLUSTERS;
                pipeline = &ward_heckbert_pipeline;
            }
        } else {
            source = resident_source;
            chunks = sizeof(resident_source) / sizeof(resident_source[0]);
            num_bound = RES_STATE + 1u;
            width = GRID_TILE;
            if (stage == RES_PREFIX || stage == RES_AXIS || stage == RES_SEED ||
                    stage == RES_OBJECTIVE || stage == RES_HB_PREFIX)
                width = 1u;
            if (stage == RES_COVER_REPAIR)
                width = 256u;
        }
        status = sixel_vk_compute_create_specialized(runtime,
            pipeline, source, chunks, entries[stage], num_bound, width,
            capacity);
        if (status != SIXEL_OK)
            return status;
        for (i = 0; i < num_bound; ++i) {
            slot = map == NULL ? i : map[i];
            bound[i] = &resident_buffers[length[slot] ? slot : RES_AUX];
        }
        status = sixel_vk_compute_bind(runtime, pipeline, bound);
        if (status != SIXEL_OK)
            return status;
    }
    return SIXEL_OK;
}

/* Keep the reusable pair shader's four bindings and workgroup geometry. */
static SIXELSTATUS
resident_prepare_ward(sixel_vk_runtime_t *runtime, size_t const *length,
    uint32_t subgroup, uint32_t colors, int heckbert)
{
    static char const *const names[WD_STAGE_COUNT] = {
        "ward_km_points", "ward_km_init", "ward_km_collect",
        "ward_km_clusters", "ward_hb_points", "ward_hb_clusters",
        "ward_seed", "ward_rows", "ward_merge", "ward_replay",
        "ward_lloyd_clear", "ward_lloyd_assign", "ward_lloyd_update",
        "ward_pack"
    };
    static unsigned char const refine[] = {
        RES_GEOMETRY, RES_POINTS, RES_PARAMS, RES_HB_PARAMS, RES_HB_ORDER,
        RES_HB_BOXES, RES_HB_RESULT, RES_AUX, RES_RESULT, RES_STATE,
        RES_MEMBERSHIP, RES_CENTERS, WD_WORK, WD_MEANS, WD_PARAMS, WD_PAIRS
    };
    static unsigned char const pairs[] = { WD_MEANS, WD_ROWS, WD_PARAMS,
        WD_PAIRS };
    sixel_vk_buffer_t const *bound[SIXEL_VK_COMPUTE_BINDINGS];
    sixel_vk_compute_t *pipeline;
    unsigned char const *slots;
    char const *const *source;
    unsigned int stage, i, bindings, lane_groups, slot;
    uint32_t width;
    size_t chunks;
    SIXELSTATUS status;

    lane_groups = resident_groups(colors, 128u);
    for (stage = 0; stage < WD_STAGE_COUNT; ++stage) {
        if ((heckbert && stage <= WD_KM_CLUSTERS) ||
                (!heckbert && stage >= WD_HB_POINTS &&
                    stage <= WD_HB_CLUSTERS))
            continue;
        pipeline = &ward_pipelines[stage];
        if (stage == WD_SCAN || stage == WD_MERGE) {
            source = ward_pairs_source;
            chunks = sizeof(ward_pairs_source) / sizeof(ward_pairs_source[0]);
            slots = pairs;
            bindings = sizeof(pairs);
            width = subgroup;
            if (stage == WD_MERGE) {
                width = 128u * lane_groups;
                pipeline = &ward_merge_pipelines[lane_groups - 1u];
            }
        } else {
            source = ward_refine_source;
            chunks = sizeof(ward_refine_source) /
                sizeof(ward_refine_source[0]);
            slots = refine;
            bindings = sizeof(refine);
            width = stage == WD_REPLAY || stage == WD_UPDATE ? 1u : GRID_TILE;
        }
        status = sixel_vk_compute_create_local(runtime, pipeline, source,
            chunks, names[stage], bindings, width);
        if (status != SIXEL_OK)
            return status;
        for (i = 0; i < bindings; ++i) {
            slot = slots[i];
            bound[i] = &resident_buffers[length[slot] ? slot : RES_AUX];
        }
        status = sixel_vk_compute_bind(runtime, pipeline, bound);
        if (status != SIXEL_OK)
            return status;
    }
    return SIXEL_OK;
}

static void
resident_ward_dispatch(VkCommandBuffer command, enum ward_stage stage,
                       uint32_t groups)
{
    sixel_vk_compute_barrier(command);
    sixel_vk_compute_dispatch(command, &ward_pipelines[stage], groups);
}

static void
resident_dispatch(VkCommandBuffer command, enum resident_stage stage,
    uint32_t groups)
{
    sixel_vk_compute_barrier(command);
    sixel_vk_compute_dispatch(command, &resident_pipelines[stage], groups);
}

static void
resident_dispatch_indirect(VkCommandBuffer command, enum resident_stage stage,
    VkDeviceSize offset)
{
    sixel_vk_compute_dispatch_indirect(command, &resident_pipelines[stage],
        resident_buffers[RES_INDIRECT].buffer, offset);
}

static SIXELSTATUS
resident_record_binning(void *opaque, sixel_filter_sample_grid_t const *grid)
{
    resident_binning_t const *record;
    VkCommandBuffer command;
    uint32_t tiles;

    record = opaque;
    command = record->command;
    tiles = resident_groups((uint32_t)grid->count, GRID_TILE);
    if (record->heckbert) {
        resident_dispatch(command, RES_HB_FIRST, tiles);
        resident_dispatch(command, RES_HB_ACCUMULATE, tiles);
        resident_dispatch(command, RES_HB_PREFIX, 1u);
        resident_dispatch(command, RES_HB_COMPACT,
            resident_groups(tiles, GRID_TILE / record->subgroup_width));
    } else {
        resident_dispatch(command, RES_HISTOGRAM, tiles);
        resident_dispatch(command, RES_COUNT, 2u);
        resident_dispatch(command, RES_PREFIX, 1u);
        sixel_vk_record_barrier(command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
            VK_ACCESS_INDIRECT_COMMAND_READ_BIT);
        resident_dispatch(command, RES_COMPACT, 2u);
    }
    return SIXEL_OK;
}

static void
resident_record_sort_control(VkCommandBuffer command, uint32_t run,
    uint32_t reverse)
{
    uint32_t pair[2];

    pair[0] = run;
    pair[1] = reverse;
    /* Each pass reads the same control words that the next pass replaces.
     * Include transfer writes so consecutive updates also have a WAW edge. */
    sixel_vk_record_barrier(command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
    vkCmdUpdateBuffer(command, resident_buffers[RES_INDIRECT].buffer,
        MERGE_CONTROL, sizeof(pair), pair);
}

static void
resident_record_lloyd(VkCommandBuffer command, uint32_t samples,
    uint32_t colors, uint32_t iterations, int ward)
{
    uint32_t i, run, reverse;

    resident_dispatch(command, RES_MOMENTS,
        resident_groups(resident_groups(samples, 64u), GRID_TILE));
    resident_dispatch(command, RES_AXIS, 1u);
    resident_dispatch_indirect(command, RES_TILES, 0);
    reverse = 0u;
    for (run = GRID_TILE; run < samples; run *= 2u) {
        resident_record_sort_control(command, run, reverse);
        resident_dispatch_indirect(command, RES_MERGE, 0);
        reverse ^= 1u;
    }
    resident_record_sort_control(command, 0u, reverse);
    resident_dispatch(command, RES_SEED, 1u);
    for (i = 0; i < iterations; ++i) {
        resident_dispatch_indirect(command, RES_ASSIGN, INDIRECT_LLOYD);
        resident_dispatch_indirect(command, RES_PARTIAL, INDIRECT_PARTIAL);
        resident_dispatch(command, RES_REDUCE, resident_groups(colors, 256u));
        resident_dispatch(command, RES_UPDATE, 1u);
    }
    if (ward)
        return;
    resident_dispatch_indirect(command, RES_SCORE, 0);
    resident_dispatch(command, RES_OBJECTIVE, 1u);
    resident_dispatch(command, RES_PACK, resident_groups(colors, GRID_TILE));
}

static void
resident_record_ward(VkCommandBuffer command,
    sixel_palette_resident_request_t const *request, int heckbert)
{
    uint32_t batches, samples, centers, i;

    centers = request->ward_colors;
    batches = resident_groups(centers, GRID_TILE);
    samples = resident_groups((uint32_t)request->grid->count, GRID_TILE);
    if (heckbert) {
        resident_ward_dispatch(command, WD_HB_POINTS, samples);
        resident_ward_dispatch(command, WD_HB_CLUSTERS, batches);
    } else {
        resident_ward_dispatch(command, WD_KM_INIT, batches);
        resident_dispatch_indirect(command, RES_ASSIGN, INDIRECT_LLOYD);
        resident_ward_dispatch(command, WD_KM_COLLECT, samples);
        resident_ward_dispatch(command, WD_KM_CLUSTERS, batches);
    }
    resident_ward_dispatch(command, WD_SEED, batches);
    resident_ward_dispatch(command, WD_SCAN, centers);
    sixel_vk_compute_barrier(command);
    sixel_vk_compute_dispatch(command,
        &ward_merge_pipelines[batches - 1u], 1u);
    resident_ward_dispatch(command, WD_REPLAY, 1u);
    batches = resident_groups(request->colors, GRID_TILE);
    for (i = 0; i < request->ward_iterations; ++i) {
        resident_ward_dispatch(command, WD_CLEAR, batches);
        resident_ward_dispatch(command, WD_ASSIGN, samples);
        resident_ward_dispatch(command, WD_UPDATE, 1u);
    }
    resident_ward_dispatch(command, WD_PACK, batches);
}

static SIXELSTATUS
resident_reserve_buffers(sixel_vk_runtime_t *runtime, size_t *length,
    int heckbert, uint32_t samples, size_t pixels, uint32_t colors, int ward,
    int spell)
{
    static size_t const fixed[RES_BUFFER_COUNT] = {
        [RES_GEOMETRY] = sizeof(resident_geometry_t),
        [RES_PARAMS] = 6u * sizeof(uint32_t),
        [RES_AUX] = AUX_BYTES, [RES_RESULT] = RESULT_BYTES, [RES_LUT] = HB_CELLS
    };
    size_t tiles;
    unsigned int slot;
    VkBufferUsageFlags usage;
    SIXELSTATUS status;

    memcpy(length, fixed, sizeof(fixed));
    length[RES_RGB] = (3u * pixels + 3u) & ~(size_t)3u;
    length[RES_INDICES] = (pixels + 3u) & ~(size_t)3u;
    if (heckbert) {
        length[RES_POINTS] = 16u * samples;
        length[RES_HB_PARAMS] = 6u * sizeof(uint32_t);
        length[RES_HB_ORDER] = sizeof(uint32_t) * samples;
        length[RES_HB_SCRATCH] = length[RES_HB_ORDER];
        length[RES_HB_BOXES] = 16u * colors;
        length[RES_HB_RESULT] = sizeof(uint32_t);
    } else {
        tiles = resident_groups(samples, 512u);
        length[RES_POINTS] = 12u * samples;
        length[RES_WEIGHTS] = 4u * samples;
        length[RES_CENTERS] = 12u * colors;
        length[RES_MEMBERSHIP] = length[RES_DISTANCES] = 4u * samples;
        length[RES_PARTIALS] = 16u * tiles * colors;
        length[RES_SUMS] = 16u * colors;
        length[RES_MAXIMA] = 4u * tiles * SIXEL_KMEANS_GPU_MAXIMA_PER_TILE;
        length[RES_STATE] = 5u * sizeof(uint32_t);
        length[RES_INDIRECT] = MERGE_CONTROL + 2u * sizeof(uint32_t);
    }
    if (ward) {
        length[WD_WORK] = WARD_WORK_BYTES;
        length[WD_MEANS] = 16u * colors;
        length[WD_ROWS] = 8u * colors;
        length[WD_PARAMS] = 8u;
        length[WD_PAIRS] = 4u * colors;
    }
    for (slot = 0; slot < RES_BUFFER_COUNT; ++slot) {
        if (length[slot] > runtime->properties.limits.maxStorageBufferRange)
            return SIXEL_FALSE;
    }
    for (slot = 0; slot < RES_BUFFER_COUNT; ++slot) {
        if (!length[slot])
            continue;
        if (slot == RES_RGB || slot == RES_GEOMETRY) {
            status = sixel_vk_buffer_reserve(runtime, &resident_buffers[slot],
                length[slot], 1);
        } else if (slot == RES_RESULT || slot == RES_INDICES) {
            if (resident_buffers[slot].buffer != VK_NULL_HANDLE &&
                    (resident_buffers[slot].mapped != NULL) == spell)
                sixel_vk_buffer_release(runtime, &resident_buffers[slot]);
            if (spell) {
                status = sixel_vk_buffer_reserve_device_usage(runtime,
                    &resident_buffers[slot], length[slot],
                    slot == RES_RESULT ? VK_BUFFER_USAGE_TRANSFER_DST_BIT : 0);
            } else {
                status = sixel_vk_buffer_reserve_readback(runtime,
                   &resident_buffers[slot], length[slot]);
            }
        } else {
            usage = 0;
            if (slot == RES_AUX || slot == RES_STATE)
                usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            if (slot == RES_INDIRECT)
                usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                    VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
            status = sixel_vk_buffer_reserve_device_usage(runtime,
                &resident_buffers[slot], length[slot], usage);
        }
        if (status != SIXEL_OK)
            return status;
    }
    return SIXEL_OK;
}

static void
resident_stage_geometry(resident_geometry_t *dst,
    sixel_palette_resident_request_t const *request)
{
    sixel_filter_sample_grid_t const *grid;
    unsigned char const *anchor;
    uint32_t i;
    float value, target;

    grid = request->grid;
    memset(dst, 0, sizeof(*dst));
    dst->width = grid->width;
    dst->height = grid->height;
    dst->stride = grid->stride;
    dst->samples = (uint32_t)grid->count;
    dst->colors = request->ward_colors ? request->ward_colors : request->colors;
    dst->target = request->colors;
    dst->heckbert = request->quantizer == SIXEL_QUANTIZE_MODEL_MEDIANCUT;
    dst->representative = request->representative;
    dst->min_iterations = request->min_iterations;
    dst->threshold = request->threshold;
    dst->cover = request->cover;
    dst->soft = request->soft;
    dst->grow = request->grow;
    if (request->snap) {
        for (i = 0; i < 256u; ++i) {
            dst->tones[i] = sixel_palette_reversible_value(i);
            value = sixel_pixelformat_byte_to_float(
                SIXEL_PIXELFORMAT_RGB888, 0, (unsigned char)i);
            target = sixel_pixelformat_byte_to_float(
                SIXEL_PIXELFORMAT_RGB888, 0, (unsigned char)dst->tones[i]);
            value += (target - value) * (float)request->snap_rate;
            dst->snap[i] = sixel_pixelformat_float_channel_to_byte(
                SIXEL_PIXELFORMAT_RGB888, 0, value);
        }
    }
    if (request->cover && !request->soft) {
        for (i = 0; i < SIXEL_PALETTE_COVER_ANCHOR_MAX; ++i) {
            anchor = sixel_palette_cover_fixed_anchor(i);
            dst->anchors[i] = ((uint32_t)anchor[0] << 16u) |
                ((uint32_t)anchor[1] << 8u) | (uint32_t)anchor[2];
        }
    }
}

static void
resident_clear_histograms(VkCommandBuffer command, uint32_t samples,
    int heckbert, int soft)
{
    if (heckbert) {
        vkCmdFillBuffer(command, resident_buffers[RES_AUX].buffer, 0,
            HB_MOMENTS, UINT32_MAX);
        vkCmdFillBuffer(command, resident_buffers[RES_AUX].buffer, HB_MOMENTS,
            16u * samples, 0);
        vkCmdFillBuffer(command, resident_buffers[RES_AUX].buffer, HB_TILES,
            4u * resident_groups(samples, GRID_TILE), 0);
    } else {
        vkCmdFillBuffer(command, resident_buffers[RES_AUX].buffer, 0,
            16u * SAMPLE_LIMIT, 0);
        vkCmdFillBuffer(command, resident_buffers[RES_STATE].buffer, 0, 20u, 0);
    }
    if (soft)
        vkCmdFillBuffer(command, resident_buffers[RES_AUX].buffer, COVER_BINS,
            16u * 4096u, 0);
    vkCmdFillBuffer(command, resident_buffers[RES_RESULT].buffer, 0, 8u, 0);
}

SIXELSTATUS
sixel_palette_vulkan_resident(sixel_palette_resident_request_t const *request,
    unsigned char const *rgb, unsigned char *indices,
    unsigned char *palette, unsigned int *colors,
    sixel_palette_resident_body_t *body)
{
    sixel_vk_runtime_t *runtime;
    VkPhysicalDeviceSubgroupProperties subgroup;
    VkPhysicalDeviceProperties2 properties;
    VkPhysicalDeviceLimits const *limits;
    sixel_filter_sample_grid_t const *grid;
    sixel_filter_binning_config_t binning;
    sixel_filter_t filter;
    resident_binning_t record;
    VkCommandBuffer command;
    size_t length[RES_BUFFER_COUNT];
    uint32_t samples, pixels, solve_width, colors_work, ward_lanes;
    uint32_t const *result;
    unsigned char *input;
    int heckbert, soft, ward;
    SIXELSTATUS status;

    if (!request || !rgb ||
            (!body && (!indices || !palette || !colors)) ||
            !request->colors || request->colors > SIXEL_PALETTE_MAX)
        return SIXEL_BAD_ARGUMENT;
    grid = request->grid;
    if (!grid || !grid->stride || !grid->width || !grid->height ||
            !grid->count || grid->count > SAMPLE_LIMIT ||
            grid->width > (UINT32_MAX - 3u) / 3u / grid->height)
        return SIXEL_FALSE;
    heckbert = request->quantizer == SIXEL_QUANTIZE_MODEL_MEDIANCUT;
    if (!heckbert &&
            (!request->max_iterations || request->max_iterations > 116u))
        return SIXEL_BAD_ARGUMENT;
    ward = request->ward_colors != 0;
    colors_work = ward ? request->ward_colors : request->colors;
    if (ward && (colors_work <= request->colors ||
            colors_work > SIXEL_WARD_MAX_CLUSTERS ||
            request->ward_iterations > 30u))
        return SIXEL_BAD_ARGUMENT;
    ward_lanes = resident_groups(colors_work, 128u) * 128u;
    soft = request->cover && request->soft;
    samples = (uint32_t)grid->count;
    pixels = grid->width * grid->height;

    sixel_vk_submission_enter(&resident_submission);
    sixel_vk_lock();
    runtime = sixel_vk_runtime_get_locked();
    status = SIXEL_FALSE;
    if (runtime == NULL)
        goto end;
    limits = &runtime->properties.limits;
    if (limits->maxComputeWorkGroupInvocations < 256u ||
            limits->maxComputeWorkGroupSize[0] < 256u ||
            limits->maxPerStageDescriptorStorageBuffers < RES_STATE + 1u ||
            limits->maxDescriptorSetStorageBuffers < RES_STATE + 1u)
        goto end;
    memset(&subgroup, 0, sizeof(subgroup));
    subgroup.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
    memset(&properties, 0, sizeof(properties));
    properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    properties.pNext = &subgroup;
    vkGetPhysicalDeviceProperties2(runtime->physical, &properties);
    if (!(subgroup.supportedStages & VK_SHADER_STAGE_COMPUTE_BIT) ||
            !(subgroup.supportedOperations &
              VK_SUBGROUP_FEATURE_ARITHMETIC_BIT) ||
            !subgroup.subgroupSize || GRID_TILE % subgroup.subgroupSize)
        goto end;
    if (ward && (!runtime->shader_int64 || subgroup.subgroupSize < 8u ||
            limits->maxComputeSharedMemorySize <
                (heckbert ? 49152u : 32768u) ||
            limits->maxComputeWorkGroupSize[0] < ward_lanes ||
            limits->maxComputeWorkGroupInvocations < ward_lanes))
        goto end;
    solve_width = heckbert ? sixel_vk_heckbert_threads(runtime) : 0u;
    if (heckbert && !solve_width)
        goto end;
    status = resident_reserve_buffers(runtime, length, heckbert, samples,
        pixels, colors_work, ward, body != NULL);
    if (status != SIXEL_OK)
        goto end;
    status = resident_prepare_pipelines(runtime, length, solve_width, heckbert,
        request->snap, request->cover != 0, soft, ward);
    if (status != SIXEL_OK)
        goto end;
    if (ward) {
        status = resident_prepare_ward(runtime, length, subgroup.subgroupSize,
            colors_work, heckbert);
        if (status != SIXEL_OK)
            goto end;
    }
    input = resident_buffers[RES_RGB].mapped;
    memcpy(input, rgb, 3u * (size_t)pixels);
    memset(input + 3u * (size_t)pixels, 0,
        length[RES_RGB] - 3u * (size_t)pixels);
    resident_stage_geometry(resident_buffers[RES_GEOMETRY].mapped, request);
    resident_submission.scoped_timestamps = 1;
    status = sixel_vk_submission_begin(runtime, &resident_submission, &command);
    if (status != SIXEL_OK)
        goto end;
    sixel_vk_buffer_record_upload(command, &resident_buffers[RES_RGB],
        length[RES_RGB]);
    sixel_vk_buffer_record_upload(command, &resident_buffers[RES_GEOMETRY],
        length[RES_GEOMETRY]);
    resident_clear_histograms(command, samples, heckbert, soft);
    sixel_vk_compute_barrier(command);
    if (resident_submission.timestamps != VK_NULL_HANDLE)
        vkCmdWriteTimestamp(command, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            resident_submission.timestamps, 0u);

    memset(&binning, 0, sizeof(binning));
    record.command = command;
    record.subgroup_width = subgroup.subgroupSize;
    record.heckbert = heckbert;
    binning.grid_input = grid;
    binning.record = resident_record_binning;
    binning.record_context = &record;
    status = sixel_filter_factory_init_by_kind(SIXEL_FILTER_KIND_BINNING,
        &binning, &filter);
    if (status != SIXEL_OK)
        goto end;
    status = sixel_filter_run(&filter, NULL, NULL);
    sixel_filter_teardown(&filter);
    if (status != SIXEL_OK)
        goto end;
    if (heckbert && ward) {
        sixel_vk_compute_barrier(command);
        sixel_vk_compute_dispatch(command, &ward_heckbert_pipeline, 1u);
    } else if (heckbert) {
        resident_dispatch(command, RES_HB_SOLVE, 1u);
        resident_dispatch(command, RES_HB_REP,
            resident_groups(colors_work, GRID_TILE));
    } else {
        if (ward)
            resident_ward_dispatch(command, WD_KM_POINTS, 2u);
        resident_record_lloyd(command, samples, colors_work,
            request->max_iterations, ward);
    }
    if (ward)
        resident_record_ward(command, request, heckbert);
    if (request->snap)
        resident_dispatch(command, RES_SNAP,
            resident_groups(request->colors, GRID_TILE));
    if (soft) {
        resident_dispatch(command, RES_COVER_HIST,
            resident_groups(samples, GRID_TILE));
        resident_dispatch(command, RES_COVER_GAPS, 4096u / GRID_TILE);
    }
    if (request->cover)
        resident_dispatch(command, RES_COVER_REPAIR, 1u);
    resident_dispatch(command, RES_BUILD_LUT, HB_CELLS / GRID_TILE);
    resident_dispatch(command, RES_APPLY, resident_groups(pixels, GRID_TILE));

    if (body != NULL) {
        status = sixel_vk_body_record_resident(runtime, command,
            &resident_buffers[RES_INDICES], &resident_buffers[RES_RESULT],
            grid->width, grid->height, request->body_gri_limit,
            resident_submission.timestamps);
        if (status != SIXEL_OK)
            goto end;
        status = sixel_vk_submission_submit_and_wait(runtime,
            &resident_submission, command);
        if (status == SIXEL_OK)
            status = sixel_vk_body_read_resident(runtime, &body->bytes,
                &body->size);
        goto end;
    }

    /* The indexed consumer borrows only the final palette and indices. */
    if (resident_submission.timestamps != VK_NULL_HANDLE)
        vkCmdWriteTimestamp(command, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            resident_submission.timestamps, 1u);
    sixel_vk_compute_download_barrier(command);
    sixel_vk_buffer_record_download(command, &resident_buffers[RES_RESULT],
        length[RES_RESULT]);
    sixel_vk_buffer_record_download(command, &resident_buffers[RES_INDICES],
        length[RES_INDICES]);
    sixel_vk_record_barrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_HOST_BIT,
        VK_ACCESS_HOST_READ_BIT);
    status = sixel_vk_submission_submit_and_wait(runtime, &resident_submission,
        command);
    if (status != SIXEL_OK)
        goto end;
    status = sixel_vk_buffer_invalidate(runtime, &resident_buffers[RES_RESULT],
        length[RES_RESULT]);
    if (status != SIXEL_OK)
        goto end;
    result = resident_buffers[RES_RESULT].mapped;
    status = SIXEL_RUNTIME_ERROR;
    if (result[1] || !result[0] || result[0] > SIXEL_PALETTE_MAX) {
        if (ward)
            status = SIXEL_FALSE;
        goto end;
    }
    status = sixel_vk_buffer_invalidate(runtime, &resident_buffers[RES_INDICES],
        length[RES_INDICES]);
    if (status != SIXEL_OK)
        goto end;
    memcpy(indices, resident_buffers[RES_INDICES].mapped, pixels);
    memcpy(palette, result + 2, 3u * result[0]);
    *colors = result[0];
end:
    sixel_vk_unlock();
    sixel_vk_submission_leave(&resident_submission);
    return status;
}
#else
typedef int palette_resident_vulkan_disabled;
#endif
