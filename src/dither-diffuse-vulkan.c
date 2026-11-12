/* SPDX-License-Identifier: MIT */
/*
 * Vulkan plan for the four-phase GPU error diffusion. The pass sequence,
 * state planes and history resolution follow dither-policy-fs-gpu-metal.m
 * and dither-policy-lso2-gpu-metal.m: initialize, eight iterate subpasses
 * (four spatial phases, two cycles), then finish for a monolithic run or
 * solve_tail for a deferred solve whose resolve compares against history.
 * The lookup is the NONE scan or the shared FHEDT or 5bit/6bit table
 * that gpu-palette-vulkan.c builds on the device, rebuilt in the same
 * submission whenever the palette or the table settings change.
 *
 * Every Vulkan call runs under sixel_vk_lock(). One descriptor set is
 * rewritten for each submission because every submission waits for its
 * fence before the lock is released.
 */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if defined(HAVE_VULKAN)

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "dither-diffuse-vulkan.h"
#include "gpu-runtime-vulkan.h"
#include "gpu-palette.h"
#include "gpu-palette-vulkan.h"
#include "encode-result.h"
#include "palette.h"
#include "sixel_atomic.h"
#include "delta-residual.h"
#include "bluenoise_64x64.h"
#include "lso2-gpu-coefficients.h"

#if defined(HAVE_VULKAN_PREBUILT_SPIRV)
static unsigned char const g_sixel_vk_diffuse_spirv[] = {
#include <generated-vulkan/dither-diffuse-vulkan-spirv.inc.h>
};
#endif
#if defined(HAVE_SHADERC)
static char const * const g_sixel_vk_diffuse_glsl[] = {
#include <generated-vulkan/dither-diffuse-vulkan-shader.inc.h>
    NULL
};
#endif

#define SIXEL_VK_DIFFUSE_LOCAL_SIZE 64U
#define SIXEL_VK_DIFFUSE_KERNELS 5
#define SIXEL_VK_DIFFUSE_METHODS 2
#define SIXEL_VK_DIFFUSE_TAPS 12

enum {
    SIXEL_VK_DIFFUSE_SOURCE = 0,
    SIXEL_VK_DIFFUSE_PALETTE = 1,
    SIXEL_VK_DIFFUSE_A = 2,
    SIXEL_VK_DIFFUSE_B = 3,
    SIXEL_VK_DIFFUSE_PARAMS = 4,
    SIXEL_VK_DIFFUSE_PHASE = 5,
    SIXEL_VK_DIFFUSE_POSITIONS = 6,
    SIXEL_VK_DIFFUSE_WEIGHTS = 7,
    SIXEL_VK_DIFFUSE_TRANSPARENT = 8,
    SIXEL_VK_DIFFUSE_RETAINED = 9,
    SIXEL_VK_DIFFUSE_VALID = 10,
    SIXEL_VK_DIFFUSE_RESULT = 11,
    SIXEL_VK_DIFFUSE_INVALID = 12,
    SIXEL_VK_DIFFUSE_SKIP = 13,
    /* Two views of the shared FHEDT table, or the 5bit/6bit table through
     * the byte view; never bundle buffers. */
    SIXEL_VK_DIFFUSE_FHEDT_TABLE8 = 14,
    SIXEL_VK_DIFFUSE_FHEDT_TABLE32 = 15,
    SIXEL_VK_DIFFUSE_BINDINGS = 16
};

enum {
    SIXEL_VK_DIFFUSE_INITIALIZE = 0,
    SIXEL_VK_DIFFUSE_ITERATE = 1,
    SIXEL_VK_DIFFUSE_FINISH = 2,
    SIXEL_VK_DIFFUSE_SOLVE_TAIL = 3,
    SIXEL_VK_DIFFUSE_RESOLVE = 4
};

/* Run modes: a monolithic run, a history-free solve, or its resolve. */
enum {
    SIXEL_VK_DIFFUSE_RUN = 0,
    SIXEL_VK_DIFFUSE_SOLVE = 1,
    SIXEL_VK_DIFFUSE_RESOLVE_ONLY = 2
};

/* Must match the Params block in dither-diffuse-vulkan.comp (std430). */
typedef struct sixel_vk_diffuse_params {
    uint32_t width, height, ncolors, tiles_x;
    uint32_t keycolor, delta, threshold, metric;
    uint32_t columns, rows, tiled, selected_threshold;
    uint32_t serpentine, clamp_hard, tap_count, weight_base;
    float deadzone, clamp_lower, clamp_upper, reserved;
    int32_t dx[SIXEL_VK_DIFFUSE_TAPS];
    int32_t dy[SIXEL_VK_DIFFUSE_TAPS];
    float tap_weights[SIXEL_VK_DIFFUSE_TAPS];
    uint32_t selection[128];
    uint32_t lookup, fhedt_resolution, fhedt_shift, fhedt_refine;
    uint32_t fhedt_dist2, bitlut_bits, unused[2];
} sixel_vk_diffuse_params_t;

typedef struct sixel_vk_diffuse_push {
    uint32_t step;
    uint32_t pitch;
    uint32_t count;
} sixel_vk_diffuse_push_t;

/* Fixed-tap kernels: dx columns behind along the sender row's scan
 * direction, dy rows above, and numerator over denominator. */
typedef struct sixel_vk_diffuse_taps {
    int method;
    int count;
    int denominator;
    int dx[SIXEL_VK_DIFFUSE_TAPS];
    int dy[SIXEL_VK_DIFFUSE_TAPS];
    int numerator[SIXEL_VK_DIFFUSE_TAPS];
} sixel_vk_diffuse_taps_t;

static sixel_vk_diffuse_taps_t const g_sixel_vk_diffuse_taps[] = {
    { SIXEL_DIFFUSE_FS, 4, 16,
      { 1, -1, 0, 1 }, { 0, 1, 1, 1 }, { 7, 3, 5, 1 } },
    { SIXEL_DIFFUSE_ATKINSON, 6, 8,
      { 1, 2, -1, 0, 1, 0 }, { 0, 0, 1, 1, 1, 2 }, { 1, 1, 1, 1, 1, 1 } },
    { SIXEL_DIFFUSE_JAJUNI, 12, 48,
      { 1, 2, -2, -1, 0, 1, 2, -2, -1, 0, 1, 2 },
      { 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2 },
      { 7, 5, 3, 5, 7, 5, 3, 1, 3, 5, 3, 1 } },
    { SIXEL_DIFFUSE_STUCKI, 12, 42,
      { 1, 2, -2, -1, 0, 1, 2, -2, -1, 0, 1, 2 },
      { 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2 },
      { 8, 4, 2, 4, 8, 4, 2, 1, 2, 4, 2, 1 } },
    { SIXEL_DIFFUSE_BURKES, 7, 32,
      { 1, 2, -2, -1, 0, 1, 2 }, { 0, 0, 1, 1, 1, 1, 1 },
      { 8, 4, 2, 4, 8, 4, 2 } },
    { SIXEL_DIFFUSE_SIERRA1, 3, 4,
      { 1, -1, 0 }, { 0, 1, 1 }, { 2, 1, 1 } },
    { SIXEL_DIFFUSE_SIERRA2, 7, 16,
      { 1, 2, -2, -1, 0, 1, 2 }, { 0, 0, 1, 1, 1, 1, 1 },
      { 4, 3, 1, 2, 3, 2, 1 } },
    { SIXEL_DIFFUSE_SIERRA3, 10, 32,
      { 1, 2, -2, -1, 0, 1, 2, -1, 0, 1 },
      { 0, 0, 1, 1, 1, 1, 1, 2, 2, 2 },
      { 5, 3, 2, 4, 5, 4, 2, 2, 3, 2 } }
};

/* Process-wide programs and the constant phase, position and LSO2 weight
 * tables. Objects stay allocated until process exit, like the palette
 * pipeline, because the runtime is never torn down. */
typedef struct sixel_vk_diffuse_state {
    int probed;
    int ready;
    VkDescriptorSetLayout set_layout;
    VkPipelineLayout pipeline_layout;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSet descriptor_set;
    VkPipeline pipelines[SIXEL_VK_DIFFUSE_METHODS][SIXEL_VK_DIFFUSE_KERNELS];
    sixel_vk_buffer_t phase;
    sixel_vk_buffer_t positions;
    sixel_vk_buffer_t weights;
} sixel_vk_diffuse_state_t;

/* Image scratch for one plan. Idle bundles are kept for the next plan so a
 * video does not pay first-touch allocation for two large state planes on
 * every frame; initialization rewrites every value a pass reads. */
typedef struct sixel_vk_diffuse_bundle {
    sixel_vk_buffer_t buffers[SIXEL_VK_DIFFUSE_BINDINGS];
} sixel_vk_diffuse_bundle_t;

#define SIXEL_VK_DIFFUSE_IDLE 2
#define SIXEL_VK_DIFFUSE_IDLE_BYTES (256U * 1024U * 1024U)

static sixel_vk_diffuse_state_t g_sixel_vk_diffuse;
static sixel_vk_diffuse_bundle_t g_sixel_vk_diffuse_idle[
    SIXEL_VK_DIFFUSE_IDLE];
static unsigned int g_sixel_vk_diffuse_idle_next;

typedef struct sixel_vk_diffuse_plan {
    sixel_gpu_dither_plan_interface_t iface;
    sixel_gpu_dither_deferred_interface_t deferred;
    sixel_gpu_dither_solution_interface_t solution;
    sixel_atomic_u32_t solution_ref;
    sixel_atomic_u32_t consumed;
    sixel_atomic_u32_t ref;
    sixel_atomic_u32_t busy;
    sixel_allocator_t *allocator;
    sixel_indexed_frame_description_t output;
    int method;
    unsigned char palette[768];
    size_t palette_size;
    sixel_vk_diffuse_bundle_t bundle;
    sixel_vk_diffuse_params_t params;
} sixel_vk_diffuse_plan_t;

static int
sixel_vk_diffuse_prepare_locked(sixel_vk_runtime_t *runtime)
{
    sixel_vk_diffuse_state_t *state;
    sixel_vk_shader_source_t source;
    VkShaderModule module;
    VkDescriptorSetLayoutBinding bindings[SIXEL_VK_DIFFUSE_BINDINGS];
    VkDescriptorSetLayoutCreateInfo set_layout_info;
    VkPushConstantRange push_range;
    VkPipelineLayoutCreateInfo layout_info;
    VkDescriptorPoolSize pool_size;
    VkDescriptorPoolCreateInfo pool_info;
    VkDescriptorSetAllocateInfo set_info;
    VkSpecializationMapEntry entries[2];
    VkSpecializationInfo specialization;
    VkComputePipelineCreateInfo pipeline_info;
    int32_t constants[2];
    unsigned char *phase;
    uint32_t *positions, counts[4];
    float *weights, ratio;
    double gain, started;
    size_t i, j, k, x, y, bucket, knots;
    int method, kernel;

    state = &g_sixel_vk_diffuse;
    if (state->probed) {
        return state->ready;
    }
    state->probed = 1;
    module = VK_NULL_HANDLE;
    memset(&source, 0, sizeof(source));
    source.name = "dither-diffuse-vulkan.comp";
#if defined(HAVE_VULKAN_PREBUILT_SPIRV)
    source.spirv = g_sixel_vk_diffuse_spirv;
    source.spirv_size = sizeof(g_sixel_vk_diffuse_spirv);
#endif
#if defined(HAVE_SHADERC)
    source.glsl_chunks = g_sixel_vk_diffuse_glsl;
#endif
    if (SIXEL_FAILED(sixel_vk_shader_module_create(runtime, &source,
            &module))) {
        return 0;
    }
    memset(bindings, 0, sizeof(bindings));
    for (i = 0; i < SIXEL_VK_DIFFUSE_BINDINGS; ++i) {
        bindings[i].binding = (uint32_t)i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }
    memset(&set_layout_info, 0, sizeof(set_layout_info));
    set_layout_info.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    set_layout_info.bindingCount = SIXEL_VK_DIFFUSE_BINDINGS;
    set_layout_info.pBindings = bindings;
    if (vkCreateDescriptorSetLayout(runtime->device, &set_layout_info, NULL,
            &state->set_layout) != VK_SUCCESS) {
        goto fail;
    }
    push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    push_range.offset = 0;
    push_range.size = sizeof(sixel_vk_diffuse_push_t);
    memset(&layout_info, 0, sizeof(layout_info));
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount = 1;
    layout_info.pSetLayouts = &state->set_layout;
    layout_info.pushConstantRangeCount = 1;
    layout_info.pPushConstantRanges = &push_range;
    if (vkCreatePipelineLayout(runtime->device, &layout_info, NULL,
            &state->pipeline_layout) != VK_SUCCESS) {
        goto fail;
    }
    pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    pool_size.descriptorCount = SIXEL_VK_DIFFUSE_BINDINGS;
    memset(&pool_info, 0, sizeof(pool_info));
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.maxSets = 1;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = &pool_size;
    if (vkCreateDescriptorPool(runtime->device, &pool_info, NULL,
            &state->descriptor_pool) != VK_SUCCESS) {
        goto fail;
    }
    memset(&set_info, 0, sizeof(set_info));
    set_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    set_info.descriptorPool = state->descriptor_pool;
    set_info.descriptorSetCount = 1;
    set_info.pSetLayouts = &state->set_layout;
    if (vkAllocateDescriptorSets(runtime->device, &set_info,
            &state->descriptor_set) != VK_SUCCESS) {
        goto fail;
    }

    /* KERNEL and METHOD are specialization constants 0 and 1. */
    for (i = 0; i < 2; ++i) {
        entries[i].constantID = (uint32_t)i;
        entries[i].offset = (uint32_t)(i * sizeof(int32_t));
        entries[i].size = sizeof(int32_t);
    }
    memset(&specialization, 0, sizeof(specialization));
    specialization.mapEntryCount = 2;
    specialization.pMapEntries = entries;
    specialization.dataSize = sizeof(constants);
    specialization.pData = constants;
    memset(&pipeline_info, 0, sizeof(pipeline_info));
    pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeline_info.stage.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipeline_info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipeline_info.stage.module = module;
    pipeline_info.stage.pName = "main";
    pipeline_info.stage.pSpecializationInfo = &specialization;
    pipeline_info.layout = state->pipeline_layout;
    started = sixel_vk_now_ms();
    for (method = 0; method < SIXEL_VK_DIFFUSE_METHODS; ++method) {
        for (kernel = 0; kernel < SIXEL_VK_DIFFUSE_KERNELS; ++kernel) {
            constants[0] = kernel;
            constants[1] = method;
            if (vkCreateComputePipelines(runtime->device, VK_NULL_HANDLE, 1,
                    &pipeline_info, NULL,
                    &state->pipelines[method][kernel]) != VK_SUCCESS) {
                state->pipelines[method][kernel] = VK_NULL_HANDLE;
                goto fail;
            }
        }
    }
    if (runtime->debug) {
        fprintf(stderr, "libsixel: vulkan diffusion pipelines %.2f ms\n",
            sixel_vk_now_ms() - started);
    }
    vkDestroyShaderModule(runtime->device, module, NULL);
    module = VK_NULL_HANDLE;

    /* Constant tables: the 64x64 phase map, the per-phase position lists
     * and both LSO2 gain variants of the per-magnitude weights. */
    if (SIXEL_FAILED(sixel_vk_buffer_reserve(runtime, &state->phase, 4096,
            1)) ||
            SIXEL_FAILED(sixel_vk_buffer_reserve(runtime, &state->positions,
                4096 * sizeof(uint32_t), 1)) ||
            SIXEL_FAILED(sixel_vk_buffer_reserve(runtime, &state->weights,
                2U * 256U * 8U * sizeof(float), 1))) {
        goto fail;
    }
    phase = (unsigned char *)state->phase.mapped;
    positions = (uint32_t *)state->positions.mapped;
    weights = (float *)state->weights.mapped;
    memset(counts, 0, sizeof(counts));
    for (y = 0; y < 64; ++y) {
        for (x = 0; x < 64; ++x) {
            bucket = sixel_bn64[((y + 7) & 63) * 64 + ((x + 43) & 63)] / 64;
            phase[y * 64 + x] = (unsigned char)bucket;
            if (counts[bucket] >= 1024) {
                goto fail;
            }
            positions[bucket * 1024 + counts[bucket]++] =
                (uint32_t)(y * 64 + x);
        }
    }
    memset(weights, 0, 2U * 256U * 8U * sizeof(float));
    knots = sizeof(lso2_gpu_gain_knots) / sizeof(*lso2_gpu_gain_knots);
    for (method = 0; method < 2; ++method) {
        k = 0;
        for (i = 0; i < 256; ++i) {
            gain = 1.0;
            if (method == 1) {
                /* SIXEL_LSO2_GAIN_SMALL_ERROR */
                while (k + 2 < knots &&
                        (double)i > lso2_gpu_gain_knots[k + 1][0]) {
                    ++k;
                }
                gain = lso2_gpu_gain_knots[k][1] +
                    (lso2_gpu_gain_knots[k + 1][1] -
                     lso2_gpu_gain_knots[k][1]) *
                    ((double)i - lso2_gpu_gain_knots[k][0]) /
                    (lso2_gpu_gain_knots[k + 1][0] -
                     lso2_gpu_gain_knots[k][0]);
            }
            for (j = 0; j < 6; ++j) {
                /* Preserve the measured float ratio, then one double gain
                 * and one float rounding, as the Metal plan does. */
                ratio = (float)lso2_gpu_ratios[i][j] /
                    (float)lso2_gpu_ratios[i][6];
                weights[(size_t)method * 2048U + 8U * i + j] =
                    (float)((double)ratio * gain);
            }
        }
    }
    state->ready = 1;
    return 1;

fail:
    if (module != VK_NULL_HANDLE) {
        vkDestroyShaderModule(runtime->device, module, NULL);
    }
    return 0;
}

int
sixel_dither_diffuse_vulkan_serves(int method_for_diffuse, int lut_policy)
{
    sixel_vk_runtime_t *runtime;
    size_t i;
    int known, available;

    known = method_for_diffuse == SIXEL_DIFFUSE_LSO2;
    for (i = 0; i < sizeof(g_sixel_vk_diffuse_taps) /
            sizeof(*g_sixel_vk_diffuse_taps); ++i) {
        if (g_sixel_vk_diffuse_taps[i].method == method_for_diffuse) {
            known = 1;
        }
    }
    /* NONE is the exact scan; FHEDT and 5bit/6bit query the device table
     * their own Vulkan builds produce. FHEDT settings are checked against
     * the resolved configuration when a plan is prepared. Other policies,
     * AUTO included, have no Vulkan implementation yet. */
    if (!known || !sixel_gpu_palette_vulkan_supports_lut(lut_policy)) {
        return 0;
    }
    sixel_vk_lock();
    runtime = sixel_vk_runtime_get_locked();
    available = runtime != NULL && sixel_vk_diffuse_prepare_locked(runtime);
    sixel_vk_unlock();
    return available;
}

/* As above, and for FHEDT also the resolved table settings the device
 * build reproduces. */
int
sixel_dither_diffuse_vulkan_lookup_serves(int method_for_diffuse,
    sixel_lookup_policy_configuration_t const *configuration)
{
    sixel_gpu_palette_request_t admission;

    if (configuration == NULL || !sixel_dither_diffuse_vulkan_serves(
            method_for_diffuse, configuration->kind)) {
        return 0;
    }
    if (configuration->kind == SIXEL_LUT_POLICY_5BIT ||
            configuration->kind == SIXEL_LUT_POLICY_6BIT) {
        return configuration->pixelformat == SIXEL_PIXELFORMAT_RGB888;
    }
    if (configuration->kind != SIXEL_LUT_POLICY_FHEDT) {
        return 1;
    }
    memset(&admission, 0, sizeof(admission));
    sixel_gpu_palette_set_lookup_configuration(&admission, configuration);
    return configuration->pixelformat == SIXEL_PIXELFORMAT_RGB888 &&
        sixel_gpu_palette_fhedt_is_supported(&admission);
}

static void
sixel_vk_diffuse_bundle_release(sixel_vk_runtime_t *runtime,
    sixel_vk_diffuse_bundle_t *bundle)
{
    size_t i;

    for (i = 0; i < SIXEL_VK_DIFFUSE_BINDINGS; ++i) {
        sixel_vk_buffer_release(runtime, &bundle->buffers[i]);
    }
    memset(bundle, 0, sizeof(*bundle));
}

static size_t
sixel_vk_diffuse_bundle_bytes(sixel_vk_diffuse_bundle_t const *bundle)
{
    size_t i, bytes;

    bytes = 0;
    for (i = 0; i < SIXEL_VK_DIFFUSE_BINDINGS; ++i) {
        bytes += (size_t)bundle->buffers[i].capacity;
    }
    return bytes;
}

/* Take the largest idle bundle; a smaller one would be regrown anyway. */
static void
sixel_vk_diffuse_borrow_locked(sixel_vk_diffuse_plan_t *plan)
{
    size_t slot, best, bytes, largest;

    best = SIXEL_VK_DIFFUSE_IDLE;
    largest = 0;
    for (slot = 0; slot < SIXEL_VK_DIFFUSE_IDLE; ++slot) {
        bytes = sixel_vk_diffuse_bundle_bytes(&g_sixel_vk_diffuse_idle[slot]);
        if (bytes > largest) {
            largest = bytes;
            best = slot;
        }
    }
    if (best < SIXEL_VK_DIFFUSE_IDLE) {
        plan->bundle = g_sixel_vk_diffuse_idle[best];
        memset(&g_sixel_vk_diffuse_idle[best], 0,
            sizeof(g_sixel_vk_diffuse_idle[best]));
    }
}

static void
sixel_vk_diffuse_recycle(sixel_vk_diffuse_plan_t *plan)
{
    sixel_vk_runtime_t *runtime;
    unsigned int slot;

    sixel_vk_lock();
    runtime = sixel_vk_runtime_get_locked();
    if (runtime != NULL) {
        if (sixel_vk_diffuse_bundle_bytes(&plan->bundle) >
                SIXEL_VK_DIFFUSE_IDLE_BYTES) {
            sixel_vk_diffuse_bundle_release(runtime, &plan->bundle);
        } else {
            slot = g_sixel_vk_diffuse_idle_next++ % SIXEL_VK_DIFFUSE_IDLE;
            sixel_vk_diffuse_bundle_release(runtime,
                &g_sixel_vk_diffuse_idle[slot]);
            g_sixel_vk_diffuse_idle[slot] = plan->bundle;
            memset(&plan->bundle, 0, sizeof(plan->bundle));
        }
    }
    sixel_vk_unlock();
}

static void
sixel_vk_diffuse_plan_ref(sixel_gpu_dither_plan_interface_t *self)
{
    sixel_vk_diffuse_plan_t *plan;

    plan = (sixel_vk_diffuse_plan_t *)self;
    (void)sixel_atomic_fetch_add_u32(&plan->ref, 1);
}

static void
sixel_vk_diffuse_plan_unref(sixel_gpu_dither_plan_interface_t *self)
{
    sixel_vk_diffuse_plan_t *plan;
    sixel_allocator_t *allocator;

    plan = (sixel_vk_diffuse_plan_t *)self;
    if (sixel_atomic_fetch_sub_u32(&plan->ref, 1) != 1) {
        return;
    }
    allocator = plan->allocator;
    if (plan->output.palette != NULL) {
        sixel_wire_palette_unref(plan->output.palette);
    }
    sixel_vk_diffuse_recycle(plan);
    sixel_allocator_free(allocator, plan);
    sixel_allocator_unref(allocator);
}

/* Validate without touching scratch so bad arguments do not consume a
 * ready solution. The caller must keep the borrowed request stable. */
static SIXELSTATUS
sixel_vk_diffuse_validate(sixel_vk_diffuse_plan_t *plan,
    sixel_gpu_dither_run_request_t const *request,
    sixel_vk_diffuse_params_t *resolved)
{
    sixel_vk_diffuse_params_t params;
    sixel_retained_read_view_t baseline;
    sixel_indexed_frame_description_t const *description;
    size_t cells;
    SIXELSTATUS status;

    if (request == NULL || request->result == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    description = &request->result->description;
    if (sixel_indexed_frame_get_state(request->result) !=
            SIXEL_INDEXED_BUILDING ||
            description->width != plan->output.width ||
            description->height != plan->output.height ||
            description->palette != plan->output.palette ||
            description->keycolor != plan->output.keycolor ||
            description->keycolor_reserved !=
                plan->output.keycolor_reserved ||
            description->transparent_background !=
                plan->output.transparent_background ||
            description->encoded_x != plan->output.encoded_x ||
            description->encoded_y != plan->output.encoded_y ||
            description->encoded_width != plan->output.encoded_width ||
            description->encoded_height != plan->output.encoded_height ||
            description->origin_x != plan->output.origin_x ||
            description->origin_y != plan->output.origin_y ||
            description->plane_width != plan->output.plane_width ||
            description->plane_height != plan->output.plane_height) {
        return SIXEL_BAD_ARGUMENT;
    }
    params = plan->params;
    params.delta = request->delta.enabled && request->baseline != NULL;
    params.threshold = request->delta.threshold;
    params.metric = request->delta.metric;
    if (params.delta && (params.threshold > 255 ||
            (params.metric != SIXEL_DELTA_METRIC_MAX &&
             params.metric != SIXEL_DELTA_METRIC_RGB241))) {
        return SIXEL_BAD_ARGUMENT;
    }
    cells = (size_t)request->delta.selection.columns *
        request->delta.selection.rows;
    if (request->delta.selection.columns > 64 ||
            request->delta.selection.rows > 64 ||
            ((request->delta.selection.columns == 0) !=
             (request->delta.selection.rows == 0)) ||
            (cells != 0 && (request->delta.selection.words == NULL ||
                request->delta.selection.word_count < (cells + 31) / 32))) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (cells != 0) {
        params.columns = request->delta.selection.columns;
        params.rows = request->delta.selection.rows;
        params.tiled = request->delta.selection.tiled;
        params.selected_threshold =
            request->delta.selection.selected_threshold;
        memcpy(params.selection, request->delta.selection.words,
            ((cells + 31) / 32) * sizeof(uint32_t));
    }
    if ((params.columns == 0) != (params.rows == 0) ||
            (params.tiled && (params.columns != 64 || params.rows != 64))) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (params.delta) {
        status = sixel_retained_read_get_view(request->baseline, &baseline);
        if (status != SIXEL_OK) {
            return status;
        }
        if (baseline.rgb == NULL || baseline.valid == NULL ||
                baseline.rgb_stride < (size_t)baseline.description.width * 3 ||
                baseline.valid_stride < (size_t)baseline.description.width ||
                request->result->identity.stream != baseline.stream ||
                request->result->identity.epoch != baseline.epoch ||
                request->result->identity.base_sequence !=
                    baseline.sequence ||
                plan->output.plane_width != baseline.description.width ||
                plan->output.plane_height != baseline.description.height ||
                plan->output.origin_x < 0 || plan->output.origin_y < 0 ||
                plan->output.origin_x > baseline.description.width ||
                plan->output.origin_y > baseline.description.height ||
                plan->output.width >
                    baseline.description.width - plan->output.origin_x ||
                plan->output.height >
                    baseline.description.height - plan->output.origin_y ||
                plan->output.keycolor < 0) {
            return SIXEL_BAD_ARGUMENT;
        }
    }
    *resolved = params;
    return SIXEL_OK;
}

/* Fold a 1-D invocation count into a 2-D grid within device limits. */
static SIXELSTATUS
sixel_vk_diffuse_groups(sixel_vk_runtime_t const *runtime, uint64_t count,
    uint32_t *groups_x, uint32_t *groups_y)
{
    uint64_t groups;
    uint32_t limit;

    groups = (count + SIXEL_VK_DIFFUSE_LOCAL_SIZE - 1U) /
        SIXEL_VK_DIFFUSE_LOCAL_SIZE;
    if (groups == 0) {
        groups = 1;
    }
    limit = runtime->properties.limits.maxComputeWorkGroupCount[0];
    if (limit > 65535U) {
        limit = 65535U;
    }
    *groups_x = (uint64_t)limit > groups ? (uint32_t)groups : limit;
    groups = (groups + *groups_x - 1U) / *groups_x;
    if (groups > runtime->properties.limits.maxComputeWorkGroupCount[1]) {
        return SIXEL_BAD_ARGUMENT;
    }
    *groups_y = (uint32_t)groups;
    return SIXEL_OK;
}

static void
sixel_vk_diffuse_bind_locked(sixel_vk_runtime_t *runtime,
    sixel_vk_diffuse_plan_t *plan, sixel_vk_buffer_t const *table)
{
    sixel_vk_diffuse_state_t *state;
    VkDescriptorBufferInfo infos[SIXEL_VK_DIFFUSE_BINDINGS];
    VkWriteDescriptorSet writes[SIXEL_VK_DIFFUSE_BINDINGS];
    sixel_vk_buffer_t const *buffer;
    size_t i;

    state = &g_sixel_vk_diffuse;
    memset(infos, 0, sizeof(infos));
    memset(writes, 0, sizeof(writes));
    for (i = 0; i < SIXEL_VK_DIFFUSE_BINDINGS; ++i) {
        buffer = i == SIXEL_VK_DIFFUSE_PHASE ? &state->phase
            : i == SIXEL_VK_DIFFUSE_POSITIONS ? &state->positions
            : i == SIXEL_VK_DIFFUSE_WEIGHTS ? &state->weights
            : i >= SIXEL_VK_DIFFUSE_FHEDT_TABLE8
                ? (table != NULL ? table : &state->phase)
            : &plan->bundle.buffers[i];
        infos[i].buffer = buffer->buffer;
        infos[i].offset = 0;
        infos[i].range = VK_WHOLE_SIZE;
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = state->descriptor_set;
        writes[i].dstBinding = (uint32_t)i;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[i].pBufferInfo = &infos[i];
    }
    vkUpdateDescriptorSets(runtime->device, SIXEL_VK_DIFFUSE_BINDINGS,
        writes, 0, NULL);
}

static SIXELSTATUS
sixel_vk_diffuse_dispatch(sixel_vk_runtime_t *runtime,
    VkCommandBuffer command, int method, int kernel, uint32_t step,
    uint64_t count)
{
    sixel_vk_diffuse_state_t *state;
    sixel_vk_diffuse_push_t push;
    uint32_t groups_x, groups_y;
    SIXELSTATUS status;

    state = &g_sixel_vk_diffuse;
    status = sixel_vk_diffuse_groups(runtime, count, &groups_x, &groups_y);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    push.step = step;
    push.pitch = groups_x * SIXEL_VK_DIFFUSE_LOCAL_SIZE;
    push.count = (uint32_t)count;
    vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_COMPUTE,
        state->pipelines[method][kernel]);
    vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_COMPUTE,
        state->pipeline_layout, 0, 1, &state->descriptor_set, 0, NULL);
    vkCmdPushConstants(command, state->pipeline_layout,
        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), &push);
    vkCmdDispatch(command, groups_x, groups_y, 1);
    sixel_vk_record_barrier(command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
    return SIXEL_OK;
}

/* Mode RUN executes the monolithic pass sequence. SOLVE stops after the
 * history-independent solve. RESOLVE_ONLY uses the completed private
 * planes and the current history. */
static SIXELSTATUS
sixel_vk_diffuse_run_impl(sixel_vk_diffuse_plan_t *plan,
    sixel_gpu_dither_run_request_t const *request, int mode)
{
    sixel_frame_pixels_view_t source;
    sixel_frame_transparency_t transparency;
    sixel_retained_read_view_t baseline;
    sixel_vk_runtime_t *runtime;
    sixel_vk_buffer_t *buffers;
    sixel_vk_diffuse_params_t params;
    sixel_vk_buffer_t const *table;
    VkCommandBuffer command;
    uint64_t pixels, lanes;
    size_t y, offset;
    double started, staged, waited;
    uint32_t invalid;
    int pass, kernel, rebuild;
    SIXELSTATUS status;

    if (request == NULL || (mode != SIXEL_VK_DIFFUSE_RESOLVE_ONLY &&
            request->source == NULL)) {
        return SIXEL_BAD_ARGUMENT;
    }
    params = plan->params;
    if (mode != SIXEL_VK_DIFFUSE_SOLVE) {
        status = sixel_vk_diffuse_validate(plan, request, &params);
        if (status != SIXEL_OK) {
            return status;
        }
    }
    if (request->cancel != NULL && request->cancel(request->cancel_context)) {
        return SIXEL_INTERRUPTED;
    }
    pixels = (uint64_t)params.width * params.height;
    lanes = (uint64_t)params.tiles_x * ((params.height + 63U) / 64U) * 1024U;
    buffers = plan->bundle.buffers;
    memset(&source, 0, sizeof(source));
    memset(&transparency, 0, sizeof(transparency));
    if (mode != SIXEL_VK_DIFFUSE_RESOLVE_ONLY) {
        status = request->source->vtbl->get_pixels(request->source, &source);
        if (status != SIXEL_OK) {
            return status;
        }
        if (source.pixels == NULL || source.width != (int)params.width ||
                source.height != (int)params.height ||
                source.pixelformat != SIXEL_PIXELFORMAT_RGB888 ||
                source.colorspace != SIXEL_COLORSPACE_GAMMA) {
            return SIXEL_BAD_ARGUMENT;
        }
        status = request->source->vtbl->get_transparency(request->source,
            &transparency);
        if (status != SIXEL_OK) {
            return status;
        }
        if (transparency.transparent_mask != NULL &&
                (transparency.transparent_mask_size < pixels ||
                 plan->output.keycolor < 0)) {
            return SIXEL_BAD_ARGUMENT;
        }
    }
    if (params.delta) {
        status = sixel_retained_read_get_view(request->baseline, &baseline);
        if (status != SIXEL_OK) {
            return status;
        }
    }

    command = VK_NULL_HANDLE;
    started = sixel_vk_now_ms();
    sixel_vk_lock();
    runtime = sixel_vk_runtime_get_locked();
    if (runtime == NULL || !sixel_vk_diffuse_prepare_locked(runtime)) {
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }
    /* Host-written inputs land in mapped memory (host-visible VRAM when
     * available); state planes never leave the device. */
    if (mode != SIXEL_VK_DIFFUSE_RESOLVE_ONLY) {
        memcpy(buffers[SIXEL_VK_DIFFUSE_SOURCE].mapped, source.pixels,
            (size_t)pixels * 3U);
        if (transparency.transparent_mask != NULL) {
            memcpy(buffers[SIXEL_VK_DIFFUSE_TRANSPARENT].mapped,
                transparency.transparent_mask, (size_t)pixels);
        } else {
            memset(buffers[SIXEL_VK_DIFFUSE_TRANSPARENT].mapped, 0,
                (size_t)pixels);
        }
        memcpy(buffers[SIXEL_VK_DIFFUSE_PALETTE].mapped, plan->palette,
            plan->palette_size);
    }
    if (params.delta) {
        for (y = 0; y < params.height; ++y) {
            offset = y + (size_t)plan->output.origin_y;
            memcpy((unsigned char *)buffers[SIXEL_VK_DIFFUSE_RETAINED].mapped
                    + y * params.width * 3U,
                baseline.rgb + offset * baseline.rgb_stride +
                    (size_t)plan->output.origin_x * 3U,
                params.width * 3U);
            memcpy((unsigned char *)buffers[SIXEL_VK_DIFFUSE_VALID].mapped
                    + y * params.width,
                baseline.valid + offset * baseline.valid_stride +
                    (size_t)plan->output.origin_x,
                params.width);
        }
    }
    memcpy(buffers[SIXEL_VK_DIFFUSE_PARAMS].mapped, &params, sizeof(params));
    invalid = 0;
    memcpy(buffers[SIXEL_VK_DIFFUSE_INVALID].mapped, &invalid,
        sizeof(invalid));
    /* A resolve may follow another plan's rebuild; the device palette
     * still holds this plan's colors from its solve upload. */
    table = NULL;
    rebuild = 0;
    if (params.lookup == 2) {
        status = sixel_gpu_palette_vulkan_bitlut_acquire_locked(runtime,
            &buffers[SIXEL_VK_DIFFUSE_PALETTE], plan->palette,
            (int)params.ncolors, (int)params.bitlut_bits, &table, &rebuild);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    } else if (params.lookup != 0) {
        status = sixel_gpu_palette_vulkan_fhedt_acquire_locked(runtime,
            &buffers[SIXEL_VK_DIFFUSE_PALETTE], plan->palette,
            (int)params.ncolors, (int)params.fhedt_resolution,
            (int)params.fhedt_refine, &table, &rebuild);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }
    sixel_vk_diffuse_bind_locked(runtime, plan, table);

    staged = sixel_vk_now_ms();
    status = sixel_vk_command_begin(runtime, &command);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (mode != SIXEL_VK_DIFFUSE_RESOLVE_ONLY) {
        sixel_vk_buffer_record_upload(command,
            &buffers[SIXEL_VK_DIFFUSE_SOURCE], pixels * 3U);
        sixel_vk_buffer_record_upload(command,
            &buffers[SIXEL_VK_DIFFUSE_TRANSPARENT], pixels);
        sixel_vk_buffer_record_upload(command,
            &buffers[SIXEL_VK_DIFFUSE_PALETTE], plan->palette_size);
    }
    if (params.delta) {
        sixel_vk_buffer_record_upload(command,
            &buffers[SIXEL_VK_DIFFUSE_RETAINED], pixels * 3U);
        sixel_vk_buffer_record_upload(command,
            &buffers[SIXEL_VK_DIFFUSE_VALID], pixels);
    }
    sixel_vk_buffer_record_upload(command, &buffers[SIXEL_VK_DIFFUSE_PARAMS],
        sizeof(params));
    sixel_vk_buffer_record_upload(command,
        &buffers[SIXEL_VK_DIFFUSE_INVALID], sizeof(invalid));
    sixel_vk_record_barrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
    if (rebuild) {
        /* The build reads the uploaded palette; its last barrier orders
         * the table writes before the first lookup. */
        status = params.lookup == 2
            ? sixel_gpu_palette_vulkan_bitlut_record_locked(runtime,
                command, (int)params.bitlut_bits, (int)params.ncolors)
            : sixel_gpu_palette_vulkan_fhedt_record_locked(runtime,
                command, (int)params.fhedt_resolution,
                (int)params.fhedt_refine, (int)params.ncolors);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }
    /* Pass 0 initializes, 1..8 iterate subpasses 0..7, 9 completes. */
    for (pass = mode == SIXEL_VK_DIFFUSE_RESOLVE_ONLY ? 9 : 0; pass < 10;
            ++pass) {
        kernel = pass == 0 ? SIXEL_VK_DIFFUSE_INITIALIZE
            : pass < 9 ? SIXEL_VK_DIFFUSE_ITERATE
            : mode == SIXEL_VK_DIFFUSE_RUN ? SIXEL_VK_DIFFUSE_FINISH
            : mode == SIXEL_VK_DIFFUSE_SOLVE ? SIXEL_VK_DIFFUSE_SOLVE_TAIL
            : SIXEL_VK_DIFFUSE_RESOLVE;
        status = sixel_vk_diffuse_dispatch(runtime, command, plan->method,
            kernel, pass == 0 ? 0U : (uint32_t)(pass - 1),
            kernel == SIXEL_VK_DIFFUSE_ITERATE ? lanes : pixels);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }
    sixel_vk_record_barrier(command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_READ_BIT);
    sixel_vk_buffer_record_download(command,
        &buffers[SIXEL_VK_DIFFUSE_INVALID], sizeof(invalid));
    if (mode != SIXEL_VK_DIFFUSE_SOLVE) {
        sixel_vk_buffer_record_download(command,
            &buffers[SIXEL_VK_DIFFUSE_RESULT], pixels);
        sixel_vk_buffer_record_download(command,
            &buffers[SIXEL_VK_DIFFUSE_SKIP], pixels);
    }
    /* Make shader and copy writes visible to the mapped host memory. */
    sixel_vk_record_barrier(command,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_HOST_READ_BIT);
    status = sixel_vk_submit_and_wait(runtime, command);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    waited = sixel_vk_now_ms();
    if (rebuild && params.lookup == 2) {
        sixel_gpu_palette_vulkan_bitlut_publish_locked(plan->palette,
            (int)params.ncolors, (int)params.bitlut_bits);
    } else if (rebuild) {
        sixel_gpu_palette_vulkan_fhedt_publish_locked(plan->palette,
            (int)params.ncolors, (int)params.fhedt_resolution,
            (int)params.fhedt_refine);
    }
    memcpy(&invalid, buffers[SIXEL_VK_DIFFUSE_INVALID].mapped,
        sizeof(invalid));
    if (invalid != 0) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    if (request->cancel != NULL && request->cancel(request->cancel_context)) {
        status = SIXEL_INTERRUPTED;
        goto end;
    }
    if (mode != SIXEL_VK_DIFFUSE_SOLVE) {
        memcpy(request->result->indexes,
            buffers[SIXEL_VK_DIFFUSE_RESULT].mapped, (size_t)pixels);
        memcpy(request->result->skip, buffers[SIXEL_VK_DIFFUSE_SKIP].mapped,
            (size_t)pixels);
    }
    if (runtime->debug) {
        fprintf(stderr, "libsixel: vulkan diffusion %ux%u, %u colors, %s%s: "
            "%.2f ms (stage %.2f, submit %.2f, gpu %.2f, read %.2f)\n",
            (unsigned int)params.width, (unsigned int)params.height,
            (unsigned int)params.ncolors,
            mode == SIXEL_VK_DIFFUSE_RUN ? "run"
                : mode == SIXEL_VK_DIFFUSE_SOLVE ? "solve" : "resolve",
            params.lookup == 0 ? ""
                : params.lookup == 2 ? (rebuild ? ", bitlut build"
                    : ", bitlut reuse")
                : rebuild ? ", fhedt build" : ", fhedt reuse",
            sixel_vk_now_ms() - started, staged - started, waited - staged,
            runtime->last_busy_ms, sixel_vk_now_ms() - waited);
    }
    status = SIXEL_OK;

end:
    if (command != VK_NULL_HANDLE) {
        sixel_vk_command_free(runtime, command);
    }
    sixel_vk_unlock();
    return status;
}

static SIXELSTATUS
sixel_vk_diffuse_run(sixel_gpu_dither_plan_interface_t *self,
    sixel_gpu_dither_run_request_t const *request)
{
    sixel_vk_diffuse_plan_t *plan;
    SIXELSTATUS status;

    plan = (sixel_vk_diffuse_plan_t *)self;
    if (sixel_atomic_fetch_add_u32(&plan->busy, 1) != 0) {
        (void)sixel_atomic_fetch_sub_u32(&plan->busy, 1);
        return SIXEL_LOGIC_ERROR;
    }
    status = sixel_vk_diffuse_run_impl(plan, request, SIXEL_VK_DIFFUSE_RUN);
    (void)sixel_atomic_fetch_sub_u32(&plan->busy, 1);
    return status;
}

/* Facets share the frozen plan but have independent reference ownership.
 * A live solution holds busy until its final release, even after resolve. */
static sixel_vk_diffuse_plan_t *
sixel_vk_diffuse_from_deferred(
    sixel_gpu_dither_deferred_interface_t const *self)
{
    return (sixel_vk_diffuse_plan_t *)((char *)self -
        offsetof(sixel_vk_diffuse_plan_t, deferred));
}

static sixel_vk_diffuse_plan_t *
sixel_vk_diffuse_from_solution(
    sixel_gpu_dither_solution_interface_t const *self)
{
    return (sixel_vk_diffuse_plan_t *)((char *)self -
        offsetof(sixel_vk_diffuse_plan_t, solution));
}

static void
sixel_vk_diffuse_deferred_ref(sixel_gpu_dither_deferred_interface_t *self)
{
    sixel_vk_diffuse_plan_ref(&sixel_vk_diffuse_from_deferred(self)->iface);
}

static void
sixel_vk_diffuse_deferred_unref(sixel_gpu_dither_deferred_interface_t *self)
{
    sixel_vk_diffuse_plan_unref(
        &sixel_vk_diffuse_from_deferred(self)->iface);
}

static void
sixel_vk_diffuse_solution_ref(sixel_gpu_dither_solution_interface_t *self)
{
    (void)sixel_atomic_fetch_add_u32(
        &sixel_vk_diffuse_from_solution(self)->solution_ref, 1);
}

static void
sixel_vk_diffuse_solution_unref(sixel_gpu_dither_solution_interface_t *self)
{
    sixel_vk_diffuse_plan_t *plan;

    plan = sixel_vk_diffuse_from_solution(self);
    if (sixel_atomic_fetch_sub_u32(&plan->solution_ref, 1) != 1) {
        return;
    }
    (void)sixel_atomic_fetch_sub_u32(&plan->busy, 1);
    sixel_vk_diffuse_plan_unref(&plan->iface);
}

static SIXELSTATUS
sixel_vk_diffuse_solution_description(
    sixel_gpu_dither_solution_interface_t const *self,
    sixel_indexed_frame_description_t *description)
{
    if (self == NULL || description == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *description = sixel_vk_diffuse_from_solution(self)->output;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_vk_diffuse_solve(sixel_gpu_dither_deferred_interface_t *self,
    sixel_gpu_dither_solve_request_t const *request,
    sixel_gpu_dither_solution_interface_t **solution)
{
    sixel_vk_diffuse_plan_t *plan;
    sixel_gpu_dither_run_request_t run;
    SIXELSTATUS status;

    if (self == NULL || request == NULL || solution == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *solution = NULL;
    plan = sixel_vk_diffuse_from_deferred(self);
    if (sixel_atomic_fetch_add_u32(&plan->busy, 1) != 0) {
        (void)sixel_atomic_fetch_sub_u32(&plan->busy, 1);
        return SIXEL_LOGIC_ERROR;
    }
    memset(&run, 0, sizeof(run));
    run.source = request->source;
    run.logger = request->logger;
    run.cancel = request->cancel;
    run.cancel_context = request->cancel_context;
    status = sixel_vk_diffuse_run_impl(plan, &run, SIXEL_VK_DIFFUSE_SOLVE);
    if (status == SIXEL_OK) {
        plan->solution_ref = 1;
        plan->consumed = 0;
        sixel_vk_diffuse_plan_ref(&plan->iface);
        *solution = &plan->solution;
    } else {
        (void)sixel_atomic_fetch_sub_u32(&plan->busy, 1);
    }
    return status;
}

static SIXELSTATUS
sixel_vk_diffuse_resolve(sixel_gpu_dither_solution_interface_t *self,
    sixel_gpu_dither_resolve_request_t const *request)
{
    sixel_vk_diffuse_plan_t *plan;
    sixel_gpu_dither_run_request_t run;
    sixel_vk_diffuse_params_t params;
    SIXELSTATUS status;

    if (self == NULL || request == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    plan = sixel_vk_diffuse_from_solution(self);
    if (sixel_atomic_fetch_add_u32(&plan->consumed, 0) != 0) {
        return SIXEL_LOGIC_ERROR;
    }
    memset(&run, 0, sizeof(run));
    run.result = request->result;
    run.baseline = request->baseline;
    run.delta = request->delta;
    run.logger = request->logger;
    run.cancel = request->cancel;
    run.cancel_context = request->cancel_context;
    status = sixel_vk_diffuse_validate(plan, &run, &params);
    if (status != SIXEL_OK) {
        return status;
    }
    if (sixel_atomic_fetch_add_u32(&plan->consumed, 1) != 0) {
        (void)sixel_atomic_fetch_sub_u32(&plan->consumed, 1);
        return SIXEL_LOGIC_ERROR;
    }
    return sixel_vk_diffuse_run_impl(plan, &run,
        SIXEL_VK_DIFFUSE_RESOLVE_ONLY);
}

static SIXELSTATUS
sixel_vk_diffuse_get_deferred(sixel_gpu_dither_plan_interface_t const *self,
    sixel_gpu_dither_deferred_interface_t **deferred)
{
    sixel_vk_diffuse_plan_t *plan;

    if (self == NULL || deferred == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    plan = (sixel_vk_diffuse_plan_t *)self;
    sixel_vk_diffuse_plan_ref(&plan->iface);
    *deferred = &plan->deferred;
    return SIXEL_OK;
}

static sixel_gpu_dither_deferred_vtbl_t const sixel_vk_diffuse_deferred_vtbl =
{
    sixel_vk_diffuse_deferred_ref, sixel_vk_diffuse_deferred_unref,
    sixel_vk_diffuse_solve
};

static sixel_gpu_dither_solution_vtbl_t const sixel_vk_diffuse_solution_vtbl =
{
    sixel_vk_diffuse_solution_ref, sixel_vk_diffuse_solution_unref,
    sixel_vk_diffuse_solution_description, sixel_vk_diffuse_resolve
};

static sixel_gpu_dither_plan_vtbl_t const sixel_vk_diffuse_plan_vtbl = {
    sixel_vk_diffuse_plan_ref, sixel_vk_diffuse_plan_unref,
    sixel_vk_diffuse_run, sixel_vk_diffuse_get_deferred
};

/* Reserve image scratch during preparation so a run allocates nothing. */
static SIXELSTATUS
sixel_vk_diffuse_reserve_locked(sixel_vk_runtime_t *runtime,
    sixel_vk_diffuse_plan_t *plan, uint64_t pixels)
{
    sixel_vk_buffer_t *buffers;
    SIXELSTATUS status;

    buffers = plan->bundle.buffers;
    status = sixel_vk_buffer_reserve(runtime,
        &buffers[SIXEL_VK_DIFFUSE_SOURCE], pixels * 3U, 1);
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_vk_buffer_reserve(runtime,
            &buffers[SIXEL_VK_DIFFUSE_PALETTE], 768U, 1);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_vk_buffer_reserve_device(runtime,
            &buffers[SIXEL_VK_DIFFUSE_A], pixels * 16U);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_vk_buffer_reserve_device(runtime,
            &buffers[SIXEL_VK_DIFFUSE_B], pixels * 16U);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_vk_buffer_reserve(runtime,
            &buffers[SIXEL_VK_DIFFUSE_PARAMS],
            sizeof(sixel_vk_diffuse_params_t), 1);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_vk_buffer_reserve(runtime,
            &buffers[SIXEL_VK_DIFFUSE_TRANSPARENT], pixels, 1);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_vk_buffer_reserve(runtime,
            &buffers[SIXEL_VK_DIFFUSE_RETAINED], pixels * 3U, 1);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_vk_buffer_reserve(runtime,
            &buffers[SIXEL_VK_DIFFUSE_VALID], pixels, 1);
    }
    /* Results and the invalid flag are read back by the host. */
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_vk_buffer_reserve(runtime,
            &buffers[SIXEL_VK_DIFFUSE_RESULT], pixels, 0);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_vk_buffer_reserve(runtime,
            &buffers[SIXEL_VK_DIFFUSE_INVALID], 4U, 0);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_vk_buffer_reserve(runtime,
            &buffers[SIXEL_VK_DIFFUSE_SKIP], pixels, 0);
    }
    return status;
}

SIXELSTATUS
sixel_dither_diffuse_vulkan_prepare(sixel_allocator_t *allocator,
    sixel_dither_policy_prepare_request_t const *configuration,
    sixel_gpu_dither_prepare_request_t const *request,
    sixel_gpu_dither_plan_interface_t **result)
{
    sixel_vk_diffuse_plan_t *plan;
    sixel_vk_diffuse_taps_t const *taps;
    sixel_wire_palette_view_t wire;
    sixel_gpu_palette_request_t admission;
    sixel_vk_runtime_t *runtime;
    double margin;
    uint64_t pixels, limit;
    size_t i;
    int method;
    SIXELSTATUS status;

    if (allocator == NULL || configuration == NULL || request == NULL ||
            result == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *result = NULL;
    if (request->output.palette == NULL || request->output.width < 1 ||
            request->output.height < 1) {
        return SIXEL_BAD_ARGUMENT;
    }
    method = configuration->options.method_for_diffuse;
    if (!sixel_dither_diffuse_vulkan_lookup_serves(method,
            &request->direct_lookup)) {
        return SIXEL_FEATURE_ERROR;
    }
    if (request->pixelformat != SIXEL_PIXELFORMAT_RGB888 ||
            request->colorspace != SIXEL_COLORSPACE_GAMMA ||
            (request->scan != SIXEL_SCAN_SERPENTINE &&
             request->scan != SIXEL_SCAN_RASTER) ||
            (method == SIXEL_DIFFUSE_LSO2 &&
             request->scan != SIXEL_SCAN_SERPENTINE)) {
        return SIXEL_FEATURE_ERROR;
    }
    taps = NULL;
    for (i = 0; i < sizeof(g_sixel_vk_diffuse_taps) /
            sizeof(*g_sixel_vk_diffuse_taps); ++i) {
        if (g_sixel_vk_diffuse_taps[i].method == method) {
            taps = &g_sixel_vk_diffuse_taps[i];
        }
    }
    pixels = (uint64_t)request->output.width * request->output.height;
    if (pixels > UINT32_MAX / 16U) {
        return SIXEL_BAD_ARGUMENT;
    }
    status = sixel_wire_palette_get_view(request->output.palette, &wire);
    if (status != SIXEL_OK) {
        return status;
    }
    if (wire.count != (size_t)configuration->options.ncolors ||
            wire.count > 256 ||
            wire.count <= (size_t)request->output.keycolor_reserved ||
            request->output.keycolor_reserved !=
                configuration->options.keycolor_reserved ||
            (request->output.keycolor_reserved &&
             request->output.keycolor != (int)wire.count - 1)) {
        return SIXEL_BAD_ARGUMENT;
    }
    plan = sixel_allocator_calloc(allocator, 1, sizeof(*plan));
    if (plan == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    plan->iface.vtbl = &sixel_vk_diffuse_plan_vtbl;
    plan->deferred.vtbl = &sixel_vk_diffuse_deferred_vtbl;
    plan->solution.vtbl = &sixel_vk_diffuse_solution_vtbl;
    plan->ref = 1;
    plan->allocator = allocator;
    sixel_allocator_ref(allocator);
    plan->output = request->output;
    sixel_wire_palette_ref(plan->output.palette);
    plan->method = taps == NULL ? 1 : 0;
    for (i = 0; i < wire.count; ++i) {
        memcpy(plan->palette + 3 * i, wire.colors[i].emitted_rgb, 3);
    }
    plan->palette_size = wire.count * 3U;
    /* The reserved transparency entry never answers a lookup. */
    plan->params.width = (uint32_t)request->output.width;
    plan->params.height = (uint32_t)request->output.height;
    plan->params.ncolors = (uint32_t)wire.count -
        (request->output.keycolor_reserved != 0);
    plan->params.tiles_x = (plan->params.width + 63U) / 64U;
    plan->params.keycolor = (uint32_t)request->output.keycolor;
    plan->params.serpentine = request->scan == SIXEL_SCAN_SERPENTINE;
    if (taps != NULL) {
        /* The CPU limiter keeps (threshold - 1) * 255 of headroom on both
         * sides of the byte range. The deadzone discards small corrections
         * before limiting. */
        margin = (configuration->options.clamp_threshold - 1.0) * 255.0;
        plan->params.clamp_hard =
            configuration->options.clamp_policy == SIXEL_DITHER_CLAMP_HARD;
        plan->params.deadzone =
            (float)configuration->options.diffusion_deadzone;
        plan->params.clamp_lower = (float)-margin;
        plan->params.clamp_upper = (float)(255.0 + margin);
        plan->params.tap_count = (uint32_t)taps->count;
        for (i = 0; i < (size_t)taps->count; ++i) {
            plan->params.dx[i] = taps->dx[i];
            plan->params.dy[i] = taps->dy[i];
            plan->params.tap_weights[i] = (float)taps->numerator[i] /
                (float)taps->denominator;
        }
    } else {
        plan->params.weight_base =
            configuration->options.lso2_gain == SIXEL_LSO2_GAIN_SMALL_ERROR
                ? 2048U : 0U;
    }
    if (request->direct_lookup.kind == SIXEL_LUT_POLICY_FHEDT) {
        /* Fractional queries keep refinement at R256, unlike byte apply,
         * because corrected colors fall between lattice points. */
        memset(&admission, 0, sizeof(admission));
        sixel_gpu_palette_set_lookup_configuration(&admission,
            &request->direct_lookup);
        plan->params.lookup = 1U;
        plan->params.fhedt_resolution = (uint32_t)admission.fhedt_resolution;
        plan->params.fhedt_shift = admission.fhedt_resolution == 64 ? 2U :
            admission.fhedt_resolution == 128 ? 1U : 0U;
        plan->params.fhedt_refine = (uint32_t)admission.fhedt_refine;
        plan->params.fhedt_dist2 = (uint32_t)admission.fhedt_use_dist2;
    } else if (request->direct_lookup.kind == SIXEL_LUT_POLICY_5BIT ||
            request->direct_lookup.kind == SIXEL_LUT_POLICY_6BIT) {
        plan->params.lookup = 2U;
        plan->params.bitlut_bits =
            request->direct_lookup.kind == SIXEL_LUT_POLICY_5BIT ? 5U : 6U;
    }

    sixel_vk_lock();
    runtime = sixel_vk_runtime_get_locked();
    status = SIXEL_FEATURE_ERROR;
    if (runtime != NULL && sixel_vk_diffuse_prepare_locked(runtime)) {
        limit = runtime->properties.limits.maxStorageBufferRange;
        status = pixels * 16U > limit ? SIXEL_FEATURE_ERROR
            : SIXEL_OK;
        if (status == SIXEL_OK) {
            sixel_vk_diffuse_borrow_locked(plan);
            status = sixel_vk_diffuse_reserve_locked(runtime, plan, pixels);
        }
    }
    sixel_vk_unlock();
    if (status != SIXEL_OK) {
        sixel_vk_diffuse_plan_unref(&plan->iface);
        return status;
    }
    *result = &plan->iface;
    return SIXEL_OK;
}

#endif /* defined(HAVE_VULKAN) */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
