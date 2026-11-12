/* SPDX-License-Identifier: MIT */
/*
 * Vulkan execution of the Burkes GPU dither, the counterpart
 * of dither-policy-burkes-gpu-metal.m. The injected lookup plan supplies the
 * fractional query as "vulkan.lookup-rgb8.v1" chunks; this component
 * composes them between dither-policy-burkes-gpu-prefix.glsl and
 * dither-policy-burkes-gpu.glsl, runs the plan's builder passes when its
 * workspace lease asks for a build, and owns its private state planes. It
 * never interprets the lookup policy, its constants or its workspace.
 */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#if defined(HAVE_VULKAN)
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dither-policy-burkes-gpu.h"
#include "gpu-palette-vulkan.h"
#include "gpu-runtime-vulkan.h"
#include "encode-result.h"
#include "palette.h"
#include "sixel_atomic.h"
#include "delta-residual.h"
#include "bluenoise_64x64.h"

static char const * const burkes_gpu_vk_prefix[] = {
#include <generated-vulkan/dither-policy-burkes-gpu-prefix-shader.inc.h>
};
static char const * const burkes_gpu_vk_suffix[] = {
#include <generated-vulkan/dither-policy-burkes-gpu-shader.inc.h>
};

#define BURKES_GPU_VK_LOCAL_SIZE 64U
#define BURKES_GPU_VK_KERNELS 5
#define BURKES_GPU_VK_CODE_CACHE 8
#define BURKES_GPU_VK_IDLE 2
#define BURKES_GPU_VK_IDLE_BYTES (128U * 1024U * 1024U)

/* Buffer indexes follow dither-policy-burkes-gpu.metal; 7 is unused. The
 * builder color count is private to the builder set. */
enum {
    BURKES_GPU_VK_SOURCE = 0,
    BURKES_GPU_VK_PALETTE = 1,
    BURKES_GPU_VK_A = 2,
    BURKES_GPU_VK_B = 3,
    BURKES_GPU_VK_PARAMS = 4,
    BURKES_GPU_VK_PHASE = 5,
    BURKES_GPU_VK_POSITIONS = 6,
    BURKES_GPU_VK_TRANSPARENT = 8,
    BURKES_GPU_VK_RETAINED = 9,
    BURKES_GPU_VK_VALID = 10,
    BURKES_GPU_VK_RESULT = 11,
    BURKES_GPU_VK_CONSTANTS = 12,
    BURKES_GPU_VK_WORKSPACE = 13,
    BURKES_GPU_VK_INVALID = 14,
    BURKES_GPU_VK_SKIP = 15,
    BURKES_GPU_VK_COUNT = 16,
    BURKES_GPU_VK_BUFFERS = 17
};

#define BURKES_GPU_VK_BINDINGS 16

enum {
    BURKES_GPU_VK_INITIALIZE = 0,
    BURKES_GPU_VK_ITERATE = 1,
    BURKES_GPU_VK_FINISH = 2,
    BURKES_GPU_VK_SOLVE_TAIL = 3,
    BURKES_GPU_VK_RESOLVE = 4
};

/* Run modes: a monolithic run, a history-free solve, or its resolve. */
enum {
    BURKES_GPU_VK_RUN = 0,
    BURKES_GPU_VK_SOLVE = 1,
    BURKES_GPU_VK_RESOLVE_ONLY = 2
};

/* Must match the Params block in dither-policy-burkes-gpu.glsl (std430). */
typedef struct burkes_gpu_vk_params {
    uint32_t width, height, ncolors, tiles_x;
    uint32_t keycolor, delta, threshold, metric;
    uint32_t columns, rows, tiled, selected_threshold;
    uint32_t selection[128];
    uint32_t serpentine, clamp_hard;
    float deadzone, clamp_lower, clamp_upper;
} burkes_gpu_vk_params_t;

typedef struct burkes_gpu_vk_push {
    uint32_t step;
    uint32_t pitch;
    uint32_t count;
} burkes_gpu_vk_push_t;

/* Pipelines compiled from one composed text. Live plans keep an evicted
 * entry alive through its reference count. */
typedef struct burkes_gpu_vk_code {
    char *key;
    VkPipeline kernels[BURKES_GPU_VK_KERNELS];
    sixel_gpu_vulkan_builders_t builders;
    unsigned int refs;
    struct burkes_gpu_vk_code *next;
} burkes_gpu_vk_code_t;

/* Process-wide layouts, sets and the constant phase and position tables.
 * Objects stay allocated until process exit because the runtime is never
 * torn down. One set pair is enough: every record and submit holds the
 * runtime lock. */
typedef struct burkes_gpu_vk_state {
    int probed;
    int ready;
    VkDescriptorSetLayout set_layout;
    VkPipelineLayout pipeline_layout;
    VkDescriptorSetLayout builder_set_layout;
    VkPipelineLayout builder_layout;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSet descriptor_set;
    VkDescriptorSet builder_set;
    sixel_vk_buffer_t phase;
    sixel_vk_buffer_t positions;
} burkes_gpu_vk_state_t;

/* Image scratch for one plan. Idle storage is local to this dither, never
 * shared by live plans; reuse avoids the first-touch cost of two large
 * state planes on every video frame. Initialization rewrites every value a
 * pass reads, so no error crosses images. */
typedef struct burkes_gpu_vk_bundle {
    sixel_vk_buffer_t buffers[BURKES_GPU_VK_BUFFERS];
} burkes_gpu_vk_bundle_t;

typedef struct burkes_gpu_vk_plan {
    sixel_gpu_dither_plan_interface_t iface;
    sixel_gpu_dither_deferred_interface_t deferred;
    sixel_gpu_dither_solution_interface_t solution;
    sixel_atomic_u32_t solution_ref;
    sixel_atomic_u32_t consumed;
    sixel_atomic_u32_t ref;
    sixel_atomic_u32_t busy;
    sixel_allocator_t *allocator;
    sixel_gpu_device_interface_t *device;
    sixel_gpu_lookup_plan_interface_t *lookup;
    sixel_gpu_lookup_program_view_t view;
    burkes_gpu_vk_code_t *code;
    sixel_indexed_frame_description_t output;
    burkes_gpu_vk_bundle_t bundle;
    burkes_gpu_vk_params_t params;
} burkes_gpu_vk_plan_t;

static burkes_gpu_vk_state_t burkes_gpu_vk_state;
static burkes_gpu_vk_code_t *burkes_gpu_vk_codes;
static size_t burkes_gpu_vk_code_count;
static burkes_gpu_vk_bundle_t burkes_gpu_vk_idle[BURKES_GPU_VK_IDLE];
static unsigned int burkes_gpu_vk_idle_next;

static int
burkes_gpu_vk_state_locked(sixel_vk_runtime_t *runtime)
{
    burkes_gpu_vk_state_t *state;
    VkDescriptorSetLayoutBinding bindings[BURKES_GPU_VK_BINDINGS];
    VkDescriptorSetLayoutCreateInfo set_layout_info;
    VkPushConstantRange push_range;
    VkPipelineLayoutCreateInfo layout_info;
    VkDescriptorPoolSize pool_size;
    VkDescriptorPoolCreateInfo pool_info;
    VkDescriptorSetAllocateInfo set_info;
    VkDescriptorSetLayout layouts[2];
    VkDescriptorSet sets[2];
    unsigned char *phase;
    uint32_t *positions, counts[4];
    size_t i, x, y, bucket;

    state = &burkes_gpu_vk_state;
    if (state->probed) {
        return state->ready;
    }
    state->probed = 1;
    memset(bindings, 0, sizeof(bindings));
    for (i = 0; i < BURKES_GPU_VK_BINDINGS; ++i) {
        bindings[i].binding = (uint32_t)i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }
    memset(&set_layout_info, 0, sizeof(set_layout_info));
    set_layout_info.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    set_layout_info.bindingCount = BURKES_GPU_VK_BINDINGS;
    set_layout_info.pBindings = bindings;
    if (vkCreateDescriptorSetLayout(runtime->device, &set_layout_info, NULL,
            &state->set_layout) != VK_SUCCESS) {
        return 0;
    }
    push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    push_range.offset = 0;
    push_range.size = sizeof(burkes_gpu_vk_push_t);
    memset(&layout_info, 0, sizeof(layout_info));
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount = 1;
    layout_info.pSetLayouts = &state->set_layout;
    layout_info.pushConstantRangeCount = 1;
    layout_info.pPushConstantRanges = &push_range;
    if (vkCreatePipelineLayout(runtime->device, &layout_info, NULL,
            &state->pipeline_layout) != VK_SUCCESS ||
            SIXEL_FAILED(sixel_gpu_vulkan_builder_layout_create(runtime,
                &state->builder_set_layout, &state->builder_layout))) {
        return 0;
    }
    pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    pool_size.descriptorCount = BURKES_GPU_VK_BINDINGS + 4;
    memset(&pool_info, 0, sizeof(pool_info));
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.maxSets = 2;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = &pool_size;
    if (vkCreateDescriptorPool(runtime->device, &pool_info, NULL,
            &state->descriptor_pool) != VK_SUCCESS) {
        return 0;
    }
    layouts[0] = state->set_layout;
    layouts[1] = state->builder_set_layout;
    memset(&set_info, 0, sizeof(set_info));
    set_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    set_info.descriptorPool = state->descriptor_pool;
    set_info.descriptorSetCount = 2;
    set_info.pSetLayouts = layouts;
    if (vkAllocateDescriptorSets(runtime->device, &set_info, sets)
            != VK_SUCCESS) {
        return 0;
    }
    state->descriptor_set = sets[0];
    state->builder_set = sets[1];

    /* The 64x64 phase map and the per-phase position lists. */
    if (SIXEL_FAILED(sixel_vk_buffer_reserve(runtime, &state->phase, 4096,
            1)) ||
            SIXEL_FAILED(sixel_vk_buffer_reserve(runtime, &state->positions,
                4096 * sizeof(uint32_t), 1))) {
        return 0;
    }
    phase = (unsigned char *)state->phase.mapped;
    positions = (uint32_t *)state->positions.mapped;
    memset(counts, 0, sizeof(counts));
    for (y = 0; y < 64; ++y) {
        for (x = 0; x < 64; ++x) {
            bucket = sixel_bn64[((y + 7) & 63) * 64 + ((x + 43) & 63)] / 64;
            phase[y * 64 + x] = (unsigned char)bucket;
            if (counts[bucket] >= 1024) {
                return 0;
            }
            positions[bucket * 1024 + counts[bucket]++] =
                (uint32_t)(y * 64 + x);
        }
    }
    state->ready = 1;
    return 1;
}

/* --- code --------------------------------------------------------------- */

static void
burkes_gpu_vk_code_unref_locked(sixel_vk_runtime_t *runtime,
    burkes_gpu_vk_code_t *code)
{
    size_t i;

    if (--code->refs != 0) {
        return;
    }
    for (i = 0; i < BURKES_GPU_VK_KERNELS; ++i) {
        if (code->kernels[i] != VK_NULL_HANDLE) {
            vkDestroyPipeline(runtime->device, code->kernels[i], NULL);
        }
    }
    sixel_gpu_vulkan_builders_destroy(runtime, &code->builders);
    free(code->key);
    free(code);
}

/* The injected chunks plus the builder entries and dimensions name the
 * compiled code, never a policy name. */
static char *
burkes_gpu_vk_key(sixel_gpu_lookup_program_view_t const *view)
{
    sixel_gpu_lookup_build_pass_t const *pass;
    char *key;
    size_t length, i;

    length = 1;
    for (i = 0; i < view->chunk_count; ++i) {
        length += strlen(view->chunks[i]);
    }
    for (i = 0; i < view->build_pass_count; ++i) {
        if (view->build_passes[i].entry_point == NULL) {
            return NULL;
        }
        length += strlen(view->build_passes[i].entry_point) + 80U;
    }
    key = (char *)malloc(length);
    if (key == NULL) {
        return NULL;
    }
    length = 0;
    for (i = 0; i < view->chunk_count; ++i) {
        strcpy(key + length, view->chunks[i]);
        length += strlen(view->chunks[i]);
    }
    for (i = 0; i < view->build_pass_count; ++i) {
        pass = &view->build_passes[i];
        length += (size_t)sprintf(key + length, "\n// entry:%s %lu %lu %lu",
            pass->entry_point, (unsigned long)pass->threads[0],
            (unsigned long)pass->threads[1],
            (unsigned long)pass->threads[2]);
    }
    key[length] = '\0';
    return key;
}

/* Compose prefix, the policy's chunks and the dither body into one module;
 * the KERNEL specialization constant selects each of the five passes. */
static SIXELSTATUS
burkes_gpu_vk_compile_locked(sixel_vk_runtime_t *runtime,
    sixel_gpu_lookup_program_view_t const *view,
    burkes_gpu_vk_code_t **result)
{
    burkes_gpu_vk_state_t *state;
    burkes_gpu_vk_code_t *code, **link;
    sixel_vk_source_part_t parts[3];
    VkSpecializationMapEntry entry;
    VkSpecializationInfo specialization;
    VkComputePipelineCreateInfo pipeline_info;
    VkShaderModule module;
    int32_t kernel;
    char *key;
    SIXELSTATUS status;

    *result = NULL;
    state = &burkes_gpu_vk_state;
    key = burkes_gpu_vk_key(view);
    if (key == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    for (code = burkes_gpu_vk_codes; code != NULL; code = code->next) {
        if (strcmp(code->key, key) == 0) {
            free(key);
            ++code->refs;
            *result = code;
            return SIXEL_OK;
        }
    }
    code = (burkes_gpu_vk_code_t *)calloc(1, sizeof(*code));
    if (code == NULL) {
        free(key);
        return SIXEL_BAD_ALLOCATION;
    }
    code->key = key;
    code->refs = 1;
    parts[0].chunks = burkes_gpu_vk_prefix;
    parts[0].count = sizeof(burkes_gpu_vk_prefix) /
        sizeof(burkes_gpu_vk_prefix[0]);
    parts[1].chunks = view->chunks;
    parts[1].count = view->chunk_count;
    parts[2].chunks = burkes_gpu_vk_suffix;
    parts[2].count = sizeof(burkes_gpu_vk_suffix) /
        sizeof(burkes_gpu_vk_suffix[0]);
    status = sixel_vk_shader_module_compose(runtime, "burkes gpu", parts, 3,
        &module);
    if (SIXEL_SUCCEEDED(status)) {
        entry.constantID = 0;
        entry.offset = 0;
        entry.size = sizeof(kernel);
        memset(&specialization, 0, sizeof(specialization));
        specialization.mapEntryCount = 1;
        specialization.pMapEntries = &entry;
        specialization.dataSize = sizeof(kernel);
        specialization.pData = &kernel;
        memset(&pipeline_info, 0, sizeof(pipeline_info));
        pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipeline_info.stage.sType =
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipeline_info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        pipeline_info.stage.module = module;
        pipeline_info.stage.pName = "main";
        pipeline_info.stage.pSpecializationInfo = &specialization;
        pipeline_info.layout = state->pipeline_layout;
        for (kernel = 0; kernel < BURKES_GPU_VK_KERNELS; ++kernel) {
            if (vkCreateComputePipelines(runtime->device, VK_NULL_HANDLE, 1,
                    &pipeline_info, NULL, &code->kernels[kernel])
                    != VK_SUCCESS) {
                code->kernels[kernel] = VK_NULL_HANDLE;
                status = SIXEL_RUNTIME_ERROR;
                break;
            }
        }
        vkDestroyShaderModule(runtime->device, module, NULL);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_gpu_vulkan_builders_create(runtime,
            state->builder_layout, view, &code->builders);
    }
    if (SIXEL_FAILED(status)) {
        burkes_gpu_vk_code_unref_locked(runtime, code);
        return status;
    }
    /* Bounded code retention avoids recompiling the same injected lookup on
     * every video frame; live plans keep evicted code alive. */
    if (burkes_gpu_vk_code_count == BURKES_GPU_VK_CODE_CACHE) {
        link = &burkes_gpu_vk_codes;
        while ((*link)->next != NULL) {
            link = &(*link)->next;
        }
        burkes_gpu_vk_code_unref_locked(runtime, *link);
        *link = NULL;
        --burkes_gpu_vk_code_count;
    }
    code->next = burkes_gpu_vk_codes;
    burkes_gpu_vk_codes = code;
    ++burkes_gpu_vk_code_count;
    ++code->refs;
    *result = code;
    return SIXEL_OK;
}

/* --- scratch ------------------------------------------------------------- */

static void
burkes_gpu_vk_bundle_release(sixel_vk_runtime_t *runtime,
    burkes_gpu_vk_bundle_t *bundle)
{
    size_t i;

    for (i = 0; i < BURKES_GPU_VK_BUFFERS; ++i) {
        sixel_vk_buffer_release(runtime, &bundle->buffers[i]);
    }
    memset(bundle, 0, sizeof(*bundle));
}

static size_t
burkes_gpu_vk_bundle_bytes(burkes_gpu_vk_bundle_t const *bundle)
{
    size_t i, bytes;

    bytes = 0;
    for (i = 0; i < BURKES_GPU_VK_BUFFERS; ++i) {
        bytes += (size_t)bundle->buffers[i].capacity;
    }
    return bytes;
}

/* Take the largest idle bundle; a smaller one would be regrown anyway. */
static void
burkes_gpu_vk_borrow_locked(burkes_gpu_vk_plan_t *plan)
{
    size_t slot, best, bytes, largest;

    best = BURKES_GPU_VK_IDLE;
    largest = 0;
    for (slot = 0; slot < BURKES_GPU_VK_IDLE; ++slot) {
        bytes = burkes_gpu_vk_bundle_bytes(&burkes_gpu_vk_idle[slot]);
        if (bytes > largest) {
            largest = bytes;
            best = slot;
        }
    }
    if (best < BURKES_GPU_VK_IDLE) {
        plan->bundle = burkes_gpu_vk_idle[best];
        memset(&burkes_gpu_vk_idle[best], 0,
            sizeof(burkes_gpu_vk_idle[best]));
    }
}

/* Two bundles, each at most the idle byte bound, bound retained
 * memory. */
static void
burkes_gpu_vk_recycle_locked(sixel_vk_runtime_t *runtime,
    burkes_gpu_vk_plan_t *plan)
{
    unsigned int slot;

    if (burkes_gpu_vk_bundle_bytes(&plan->bundle) >
            BURKES_GPU_VK_IDLE_BYTES) {
        burkes_gpu_vk_bundle_release(runtime, &plan->bundle);
        return;
    }
    slot = burkes_gpu_vk_idle_next++ % BURKES_GPU_VK_IDLE;
    burkes_gpu_vk_bundle_release(runtime, &burkes_gpu_vk_idle[slot]);
    burkes_gpu_vk_idle[slot] = plan->bundle;
    memset(&plan->bundle, 0, sizeof(plan->bundle));
}

/* Reserve image scratch during preparation so a run allocates nothing. */
static SIXELSTATUS
burkes_gpu_vk_reserve_locked(sixel_vk_runtime_t *runtime,
    burkes_gpu_vk_plan_t *plan, uint64_t pixels)
{
    sixel_vk_buffer_t *buffers;
    size_t constants;
    SIXELSTATUS status;

    buffers = plan->bundle.buffers;
    constants = plan->view.constants_size != 0 ? plan->view.constants_size
        : 4U;
    status = sixel_vk_buffer_reserve(runtime, &buffers[BURKES_GPU_VK_SOURCE],
        pixels * 3U, 1);
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_vk_buffer_reserve(runtime,
            &buffers[BURKES_GPU_VK_PALETTE], plan->view.palette_size, 1);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_vk_buffer_reserve_device(runtime,
            &buffers[BURKES_GPU_VK_A], pixels * 16U);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_vk_buffer_reserve_device(runtime,
            &buffers[BURKES_GPU_VK_B], pixels * 16U);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_vk_buffer_reserve(runtime,
            &buffers[BURKES_GPU_VK_PARAMS],
            sizeof(burkes_gpu_vk_params_t), 1);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_vk_buffer_reserve(runtime,
            &buffers[BURKES_GPU_VK_TRANSPARENT], pixels, 1);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_vk_buffer_reserve(runtime,
            &buffers[BURKES_GPU_VK_RETAINED], pixels * 3U, 1);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_vk_buffer_reserve(runtime,
            &buffers[BURKES_GPU_VK_VALID], pixels, 1);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_vk_buffer_reserve(runtime,
            &buffers[BURKES_GPU_VK_CONSTANTS], constants, 1);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_vk_buffer_reserve(runtime,
            &buffers[BURKES_GPU_VK_COUNT], sizeof(uint32_t), 1);
    }
    /* Results and the invalid flag are read back by the host. */
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_vk_buffer_reserve(runtime,
            &buffers[BURKES_GPU_VK_RESULT], pixels, 0);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_vk_buffer_reserve(runtime,
            &buffers[BURKES_GPU_VK_INVALID], 4U, 0);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_vk_buffer_reserve(runtime,
            &buffers[BURKES_GPU_VK_SKIP], pixels, 0);
    }
    return status;
}

/* --- plan ---------------------------------------------------------------- */

static void
burkes_gpu_vk_plan_ref(sixel_gpu_dither_plan_interface_t *self)
{
    burkes_gpu_vk_plan_t *plan;

    plan = (burkes_gpu_vk_plan_t *)self;
    (void)sixel_atomic_fetch_add_u32(&plan->ref, 1);
}

static void
burkes_gpu_vk_plan_unref(sixel_gpu_dither_plan_interface_t *self)
{
    burkes_gpu_vk_plan_t *plan;
    sixel_allocator_t *allocator;
    sixel_vk_runtime_t *runtime;

    plan = (burkes_gpu_vk_plan_t *)self;
    if (sixel_atomic_fetch_sub_u32(&plan->ref, 1) != 1) {
        return;
    }
    allocator = plan->allocator;
    sixel_vk_lock();
    runtime = sixel_vk_runtime_get_locked();
    if (runtime != NULL) {
        burkes_gpu_vk_recycle_locked(runtime, plan);
        if (plan->code != NULL) {
            burkes_gpu_vk_code_unref_locked(runtime, plan->code);
        }
    }
    sixel_vk_unlock();
    /* The lookup plan may release device buffers, which take the lock. */
    if (plan->lookup != NULL) {
        plan->lookup->vtbl->unref(plan->lookup);
    }
    if (plan->device != NULL) {
        plan->device->vtbl->unref(plan->device);
    }
    if (plan->output.palette != NULL) {
        sixel_wire_palette_unref(plan->output.palette);
    }
    sixel_allocator_free(allocator, plan);
    sixel_allocator_unref(allocator);
}

/* Validate without touching scratch so bad arguments do not consume a
 * ready solution. The caller must keep the borrowed request stable. */
static SIXELSTATUS
burkes_gpu_vk_validate(burkes_gpu_vk_plan_t *plan,
    sixel_gpu_dither_run_request_t const *request,
    burkes_gpu_vk_params_t *resolved)
{
    burkes_gpu_vk_params_t params;
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
burkes_gpu_vk_groups(sixel_vk_runtime_t const *runtime, uint64_t count,
    uint32_t *groups_x, uint32_t *groups_y)
{
    uint64_t groups;
    uint32_t limit;

    groups = (count + BURKES_GPU_VK_LOCAL_SIZE - 1U) /
        BURKES_GPU_VK_LOCAL_SIZE;
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

/* Bind the plan's scratch, the constant tables and the leased workspace.
 * Binding 7 is unused by this dither and repeats the phase table. A
 * resolve performs no lookup and holds no lease; the phase table stands in
 * for the workspace the module declares. */
static void
burkes_gpu_vk_bind_locked(sixel_vk_runtime_t *runtime,
    burkes_gpu_vk_plan_t *plan, sixel_vk_buffer_t const *workspace)
{
    burkes_gpu_vk_state_t *state;
    sixel_vk_buffer_t const *bound[BURKES_GPU_VK_BINDINGS + 4];
    VkDescriptorBufferInfo infos[BURKES_GPU_VK_BINDINGS + 4];
    VkWriteDescriptorSet writes[BURKES_GPU_VK_BINDINGS + 4];
    size_t i;

    state = &burkes_gpu_vk_state;
    for (i = 0; i < BURKES_GPU_VK_BINDINGS; ++i) {
        bound[i] = &plan->bundle.buffers[i];
    }
    bound[BURKES_GPU_VK_PHASE] = &state->phase;
    bound[BURKES_GPU_VK_POSITIONS] = &state->positions;
    bound[7] = &state->phase;
    bound[BURKES_GPU_VK_WORKSPACE] = workspace != NULL ? workspace
        : &state->phase;
    bound[BURKES_GPU_VK_BINDINGS] =
        &plan->bundle.buffers[BURKES_GPU_VK_PALETTE];
    bound[BURKES_GPU_VK_BINDINGS + 1] =
        &plan->bundle.buffers[BURKES_GPU_VK_COUNT];
    bound[BURKES_GPU_VK_BINDINGS + 2] =
        &plan->bundle.buffers[BURKES_GPU_VK_CONSTANTS];
    bound[BURKES_GPU_VK_BINDINGS + 3] = bound[BURKES_GPU_VK_WORKSPACE];
    memset(infos, 0, sizeof(infos));
    memset(writes, 0, sizeof(writes));
    for (i = 0; i < BURKES_GPU_VK_BINDINGS + 4; ++i) {
        infos[i].buffer = bound[i]->buffer;
        infos[i].offset = 0;
        infos[i].range = VK_WHOLE_SIZE;
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = i < BURKES_GPU_VK_BINDINGS ? state->descriptor_set
            : state->builder_set;
        writes[i].dstBinding = (uint32_t)(i < BURKES_GPU_VK_BINDINGS ? i
            : i - BURKES_GPU_VK_BINDINGS);
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[i].pBufferInfo = &infos[i];
    }
    vkUpdateDescriptorSets(runtime->device, BURKES_GPU_VK_BINDINGS + 4,
        writes, 0, NULL);
}

static SIXELSTATUS
burkes_gpu_vk_dispatch(sixel_vk_runtime_t *runtime, VkCommandBuffer command,
    VkPipeline pipeline, uint32_t step, uint64_t count)
{
    burkes_gpu_vk_state_t *state;
    burkes_gpu_vk_push_t push;
    uint32_t groups_x, groups_y;
    SIXELSTATUS status;

    state = &burkes_gpu_vk_state;
    status = burkes_gpu_vk_groups(runtime, count, &groups_x, &groups_y);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    push.step = step;
    push.pitch = groups_x * BURKES_GPU_VK_LOCAL_SIZE;
    push.count = (uint32_t)count;
    vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
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
 * planes and the current history. Only RUN and SOLVE take a lookup lease;
 * the lease is begun and finished outside the runtime lock because the
 * lookup plan creates device buffers through the device, which locks. */
static SIXELSTATUS
burkes_gpu_vk_run_impl(burkes_gpu_vk_plan_t *plan,
    sixel_gpu_dither_run_request_t const *request, int mode)
{
    sixel_frame_pixels_view_t source;
    sixel_frame_transparency_t transparency;
    sixel_retained_read_view_t baseline;
    sixel_gpu_lookup_binding_interface_t *binding;
    sixel_gpu_lookup_binding_view_t binding_view;
    sixel_vk_runtime_t *runtime;
    sixel_vk_buffer_t *buffers;
    sixel_vk_buffer_t const *workspace;
    burkes_gpu_vk_params_t params;
    VkCommandBuffer command;
    uint64_t pixels, lanes;
    size_t y, offset, constants;
    double started, staged, waited;
    uint32_t invalid, count;
    int pass, kernel, locked;
    SIXELSTATUS status, finish;

    if (request == NULL || (mode != BURKES_GPU_VK_RESOLVE_ONLY &&
            request->source == NULL)) {
        return SIXEL_BAD_ARGUMENT;
    }
    params = plan->params;
    if (mode != BURKES_GPU_VK_SOLVE) {
        status = burkes_gpu_vk_validate(plan, request, &params);
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
    constants = plan->view.constants_size != 0 ? plan->view.constants_size
        : 4U;
    memset(&source, 0, sizeof(source));
    memset(&transparency, 0, sizeof(transparency));
    if (mode != BURKES_GPU_VK_RESOLVE_ONLY) {
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

    binding = NULL;
    workspace = NULL;
    command = VK_NULL_HANDLE;
    runtime = NULL;
    locked = 0;
    memset(&binding_view, 0, sizeof(binding_view));
    if (mode != BURKES_GPU_VK_RESOLVE_ONLY) {
        status = plan->lookup->vtbl->begin(plan->lookup, &binding);
        if (status != SIXEL_OK) {
            return status;
        }
        if (binding == NULL) {
            return SIXEL_LOGIC_ERROR;
        }
        status = binding->vtbl->get_view(binding, &binding_view);
        if (status != SIXEL_OK) {
            goto end;
        }
        workspace = sixel_gpu_vulkan_buffer(plan->device,
            binding_view.workspace);
        if (workspace == NULL || binding_view.workspace->vtbl->size(
                binding_view.workspace) < plan->view.workspace_size) {
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
    }

    started = sixel_vk_now_ms();
    sixel_vk_lock();
    locked = 1;
    runtime = sixel_vk_runtime_get_locked();
    if (runtime == NULL || !burkes_gpu_vk_state_locked(runtime)) {
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }
    /* Host-written inputs land in mapped memory (host-visible VRAM when
     * available); state planes never leave the device. */
    if (mode != BURKES_GPU_VK_RESOLVE_ONLY) {
        memcpy(buffers[BURKES_GPU_VK_SOURCE].mapped, source.pixels,
            (size_t)pixels * 3U);
        if (transparency.transparent_mask != NULL) {
            memcpy(buffers[BURKES_GPU_VK_TRANSPARENT].mapped,
                transparency.transparent_mask, (size_t)pixels);
        } else {
            memset(buffers[BURKES_GPU_VK_TRANSPARENT].mapped, 0,
                (size_t)pixels);
        }
    }
    if (params.delta) {
        for (y = 0; y < params.height; ++y) {
            offset = y + (size_t)plan->output.origin_y;
            memcpy((unsigned char *)buffers[BURKES_GPU_VK_RETAINED].mapped
                    + y * params.width * 3U,
                baseline.rgb + offset * baseline.rgb_stride +
                    (size_t)plan->output.origin_x * 3U,
                params.width * 3U);
            memcpy((unsigned char *)buffers[BURKES_GPU_VK_VALID].mapped
                    + y * params.width,
                baseline.valid + offset * baseline.valid_stride +
                    (size_t)plan->output.origin_x,
                params.width);
        }
    }
    memcpy(buffers[BURKES_GPU_VK_PARAMS].mapped, &params, sizeof(params));
    invalid = 0;
    memcpy(buffers[BURKES_GPU_VK_INVALID].mapped, &invalid, sizeof(invalid));
    burkes_gpu_vk_bind_locked(runtime, plan, workspace);

    staged = sixel_vk_now_ms();
    status = sixel_vk_command_begin(runtime, &command);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (mode != BURKES_GPU_VK_RESOLVE_ONLY) {
        sixel_vk_buffer_record_upload(command, &buffers[BURKES_GPU_VK_SOURCE],
            pixels * 3U);
        sixel_vk_buffer_record_upload(command,
            &buffers[BURKES_GPU_VK_TRANSPARENT], pixels);
        sixel_vk_buffer_record_upload(command,
            &buffers[BURKES_GPU_VK_PALETTE], plan->view.palette_size);
        sixel_vk_buffer_record_upload(command,
            &buffers[BURKES_GPU_VK_CONSTANTS], constants);
        sixel_vk_buffer_record_upload(command, &buffers[BURKES_GPU_VK_COUNT],
            sizeof(count));
    }
    if (params.delta) {
        sixel_vk_buffer_record_upload(command,
            &buffers[BURKES_GPU_VK_RETAINED], pixels * 3U);
        sixel_vk_buffer_record_upload(command, &buffers[BURKES_GPU_VK_VALID],
            pixels);
    }
    sixel_vk_buffer_record_upload(command, &buffers[BURKES_GPU_VK_PARAMS],
        sizeof(params));
    sixel_vk_buffer_record_upload(command, &buffers[BURKES_GPU_VK_INVALID],
        sizeof(invalid));
    sixel_vk_record_barrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
    if (binding_view.needs_build) {
        /* The builders read the palette just uploaded; their barriers
         * order the workspace writes before the first lookup. */
        sixel_gpu_vulkan_builders_record(command,
            burkes_gpu_vk_state.builder_layout,
            burkes_gpu_vk_state.builder_set, &plan->code->builders,
            &plan->view);
    }
    /* Pass 0 initializes, 1..8 iterate subpasses 0..7, 9 completes. */
    for (pass = mode == BURKES_GPU_VK_RESOLVE_ONLY ? 9 : 0; pass < 10;
            ++pass) {
        kernel = pass == 0 ? BURKES_GPU_VK_INITIALIZE
            : pass < 9 ? BURKES_GPU_VK_ITERATE
            : mode == BURKES_GPU_VK_RUN ? BURKES_GPU_VK_FINISH
            : mode == BURKES_GPU_VK_SOLVE ? BURKES_GPU_VK_SOLVE_TAIL
            : BURKES_GPU_VK_RESOLVE;
        status = burkes_gpu_vk_dispatch(runtime, command,
            plan->code->kernels[kernel],
            pass == 0 ? 0U : (uint32_t)(pass - 1),
            kernel == BURKES_GPU_VK_ITERATE ? lanes : pixels);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }
    sixel_vk_record_barrier(command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_READ_BIT);
    sixel_vk_buffer_record_download(command, &buffers[BURKES_GPU_VK_INVALID],
        sizeof(invalid));
    if (mode != BURKES_GPU_VK_SOLVE) {
        sixel_vk_buffer_record_download(command,
            &buffers[BURKES_GPU_VK_RESULT], pixels);
        sixel_vk_buffer_record_download(command, &buffers[BURKES_GPU_VK_SKIP],
            pixels);
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
    memcpy(&invalid, buffers[BURKES_GPU_VK_INVALID].mapped, sizeof(invalid));
    if (invalid != 0) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    if (request->cancel != NULL && request->cancel(request->cancel_context)) {
        status = SIXEL_INTERRUPTED;
        goto end;
    }
    if (mode != BURKES_GPU_VK_SOLVE) {
        memcpy(request->result->indexes, buffers[BURKES_GPU_VK_RESULT].mapped,
            (size_t)pixels);
        memcpy(request->result->skip, buffers[BURKES_GPU_VK_SKIP].mapped,
            (size_t)pixels);
    }
    if (runtime->debug) {
        fprintf(stderr, "libsixel: vulkan burkes %ux%u, %u colors, %s, %s%s: "
            "%.2f ms (stage %.2f, submit %.2f, gpu %.2f, read %.2f)\n",
            (unsigned int)params.width, (unsigned int)params.height,
            (unsigned int)params.ncolors,
            mode == BURKES_GPU_VK_RUN ? "run"
                : mode == BURKES_GPU_VK_SOLVE ? "solve" : "resolve",
            plan->view.logical_name != NULL ? plan->view.logical_name
                : "lookup",
            binding_view.needs_build ? " build" : "",
            sixel_vk_now_ms() - started, staged - started, waited - staged,
            runtime->last_busy_ms, sixel_vk_now_ms() - waited);
    }
    status = SIXEL_OK;

end:
    if (command != VK_NULL_HANDLE) {
        sixel_vk_command_free(runtime, command);
    }
    if (locked) {
        sixel_vk_unlock();
    }
    /* Submitted work is always drained above before the lease ends; only a
     * completed build is published. */
    finish = SIXEL_OK;
    if (binding != NULL) {
        finish = binding->vtbl->finish(binding, status);
        binding->vtbl->unref(binding);
    }
    return status != SIXEL_OK ? status : finish;
}

static SIXELSTATUS
burkes_gpu_vk_run(sixel_gpu_dither_plan_interface_t *self,
    sixel_gpu_dither_run_request_t const *request)
{
    burkes_gpu_vk_plan_t *plan;
    SIXELSTATUS status;

    plan = (burkes_gpu_vk_plan_t *)self;
    if (sixel_atomic_fetch_add_u32(&plan->busy, 1) != 0) {
        (void)sixel_atomic_fetch_sub_u32(&plan->busy, 1);
        return SIXEL_LOGIC_ERROR;
    }
    status = burkes_gpu_vk_run_impl(plan, request, BURKES_GPU_VK_RUN);
    (void)sixel_atomic_fetch_sub_u32(&plan->busy, 1);
    return status;
}

/* Facets share the frozen plan but have independent reference ownership.
 * A live solution holds busy until its final release, even after resolve. */
static burkes_gpu_vk_plan_t *
burkes_gpu_vk_from_deferred(sixel_gpu_dither_deferred_interface_t const *self)
{
    return (burkes_gpu_vk_plan_t *)((char *)self -
        offsetof(burkes_gpu_vk_plan_t, deferred));
}

static burkes_gpu_vk_plan_t *
burkes_gpu_vk_from_solution(sixel_gpu_dither_solution_interface_t const *self)
{
    return (burkes_gpu_vk_plan_t *)((char *)self -
        offsetof(burkes_gpu_vk_plan_t, solution));
}

static void
burkes_gpu_vk_deferred_ref(sixel_gpu_dither_deferred_interface_t *self)
{
    burkes_gpu_vk_plan_ref(&burkes_gpu_vk_from_deferred(self)->iface);
}

static void
burkes_gpu_vk_deferred_unref(sixel_gpu_dither_deferred_interface_t *self)
{
    burkes_gpu_vk_plan_unref(&burkes_gpu_vk_from_deferred(self)->iface);
}

static void
burkes_gpu_vk_solution_ref(sixel_gpu_dither_solution_interface_t *self)
{
    (void)sixel_atomic_fetch_add_u32(
        &burkes_gpu_vk_from_solution(self)->solution_ref, 1);
}

static void
burkes_gpu_vk_solution_unref(sixel_gpu_dither_solution_interface_t *self)
{
    burkes_gpu_vk_plan_t *plan;

    plan = burkes_gpu_vk_from_solution(self);
    if (sixel_atomic_fetch_sub_u32(&plan->solution_ref, 1) != 1) {
        return;
    }
    (void)sixel_atomic_fetch_sub_u32(&plan->busy, 1);
    burkes_gpu_vk_plan_unref(&plan->iface);
}

static SIXELSTATUS
burkes_gpu_vk_solution_description(
    sixel_gpu_dither_solution_interface_t const *self,
    sixel_indexed_frame_description_t *description)
{
    if (self == NULL || description == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *description = burkes_gpu_vk_from_solution(self)->output;
    return SIXEL_OK;
}

static SIXELSTATUS
burkes_gpu_vk_solve(sixel_gpu_dither_deferred_interface_t *self,
    sixel_gpu_dither_solve_request_t const *request,
    sixel_gpu_dither_solution_interface_t **solution)
{
    burkes_gpu_vk_plan_t *plan;
    sixel_gpu_dither_run_request_t run;
    SIXELSTATUS status;

    if (self == NULL || request == NULL || solution == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *solution = NULL;
    plan = burkes_gpu_vk_from_deferred(self);
    if (sixel_atomic_fetch_add_u32(&plan->busy, 1) != 0) {
        (void)sixel_atomic_fetch_sub_u32(&plan->busy, 1);
        return SIXEL_LOGIC_ERROR;
    }
    memset(&run, 0, sizeof(run));
    run.source = request->source;
    run.logger = request->logger;
    run.cancel = request->cancel;
    run.cancel_context = request->cancel_context;
    status = burkes_gpu_vk_run_impl(plan, &run, BURKES_GPU_VK_SOLVE);
    if (status == SIXEL_OK) {
        plan->solution_ref = 1;
        plan->consumed = 0;
        burkes_gpu_vk_plan_ref(&plan->iface);
        *solution = &plan->solution;
    } else {
        (void)sixel_atomic_fetch_sub_u32(&plan->busy, 1);
    }
    return status;
}

static SIXELSTATUS
burkes_gpu_vk_resolve(sixel_gpu_dither_solution_interface_t *self,
    sixel_gpu_dither_resolve_request_t const *request)
{
    burkes_gpu_vk_plan_t *plan;
    sixel_gpu_dither_run_request_t run;
    burkes_gpu_vk_params_t params;
    SIXELSTATUS status;

    if (self == NULL || request == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    plan = burkes_gpu_vk_from_solution(self);
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
    status = burkes_gpu_vk_validate(plan, &run, &params);
    if (status != SIXEL_OK) {
        return status;
    }
    if (sixel_atomic_fetch_add_u32(&plan->consumed, 1) != 0) {
        (void)sixel_atomic_fetch_sub_u32(&plan->consumed, 1);
        return SIXEL_LOGIC_ERROR;
    }
    return burkes_gpu_vk_run_impl(plan, &run, BURKES_GPU_VK_RESOLVE_ONLY);
}

static SIXELSTATUS
burkes_gpu_vk_get_deferred(sixel_gpu_dither_plan_interface_t const *self,
    sixel_gpu_dither_deferred_interface_t **deferred)
{
    burkes_gpu_vk_plan_t *plan;

    if (self == NULL || deferred == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    plan = (burkes_gpu_vk_plan_t *)self;
    burkes_gpu_vk_plan_ref(&plan->iface);
    *deferred = &plan->deferred;
    return SIXEL_OK;
}

static sixel_gpu_dither_deferred_vtbl_t const burkes_gpu_vk_deferred_vtbl = {
    burkes_gpu_vk_deferred_ref, burkes_gpu_vk_deferred_unref,
    burkes_gpu_vk_solve
};

static sixel_gpu_dither_solution_vtbl_t const burkes_gpu_vk_solution_vtbl = {
    burkes_gpu_vk_solution_ref, burkes_gpu_vk_solution_unref,
    burkes_gpu_vk_solution_description, burkes_gpu_vk_resolve
};

static sixel_gpu_dither_plan_vtbl_t const burkes_gpu_vk_plan_vtbl = {
    burkes_gpu_vk_plan_ref, burkes_gpu_vk_plan_unref, burkes_gpu_vk_run,
    burkes_gpu_vk_get_deferred
};

/* Prepare the injected lookup for fractional queries on the Vulkan device,
 * compose and compile its program, reserve its workspace with a lease that
 * is never published, and reserve this plan's scratch. */
SIXELSTATUS
sixel_dither_burkes_gpu_vulkan_prepare(sixel_allocator_t *allocator,
    sixel_dither_policy_prepare_request_t const *configuration,
    sixel_gpu_dither_prepare_request_t const *request,
    sixel_gpu_dither_plan_interface_t **result)
{
    burkes_gpu_vk_plan_t *plan;
    sixel_palette_t *palette;
    sixel_palette_entries_request_t entries;
    sixel_wire_palette_view_t wire;
    sixel_lookup_execution_request_t lookup_request;
    sixel_gpu_lookup_binding_interface_t *warm_binding;
    sixel_vk_runtime_t *runtime;
    unsigned char colors[768];
    uint32_t count;
    double margin;
    uint64_t pixels, limit;
    size_t i;
    SIXELSTATUS status;

    if (allocator == NULL || configuration == NULL || request == NULL ||
            result == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *result = NULL;
    if (request->device == NULL || request->lookup == NULL ||
            request->output.palette == NULL || request->output.width < 1 ||
            request->output.height < 1 || request->working_palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (request->pixelformat != SIXEL_PIXELFORMAT_RGB888 ||
            request->colorspace != SIXEL_COLORSPACE_GAMMA ||
            (request->scan != SIXEL_SCAN_SERPENTINE &&
             request->scan != SIXEL_SCAN_RASTER) ||
            !sixel_gpu_vulkan_is_device(request->device)) {
        return SIXEL_FEATURE_ERROR;
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
    for (i = 0; i < wire.count; ++i) {
        memcpy(colors + 3 * i, wire.colors[i].emitted_rgb, 3);
    }
    plan = sixel_allocator_calloc(allocator, 1, sizeof(*plan));
    if (plan == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    plan->iface.vtbl = &burkes_gpu_vk_plan_vtbl;
    plan->deferred.vtbl = &burkes_gpu_vk_deferred_vtbl;
    plan->solution.vtbl = &burkes_gpu_vk_solution_vtbl;
    plan->ref = 1;
    plan->allocator = allocator;
    sixel_allocator_ref(allocator);
    plan->device = request->device;
    plan->device->vtbl->ref(plan->device);
    plan->output = request->output;
    sixel_wire_palette_ref(plan->output.palette);
    palette = NULL;
    status = sixel_palette_factory_new(allocator, (void **)&palette);
    if (status != SIXEL_OK) {
        goto end;
    }
    entries.entries = colors;
    entries.colors = (unsigned int)wire.count;
    entries.depth = 3;
    status = palette->vtbl->init_entries(palette, &entries);
    if (status != SIXEL_OK) {
        goto end;
    }
    /* The reserved transparency entry never answers a lookup. */
    memset(&lookup_request, 0, sizeof(lookup_request));
    lookup_request.working_palette = palette;
    lookup_request.ncolors =
        (unsigned int)wire.count - (request->output.keycolor_reserved != 0);
    lookup_request.pixelformat = request->pixelformat;
    lookup_request.colorspace = request->colorspace;
    lookup_request.query_kind = SIXEL_GPU_LOOKUP_QUERY_FRACTIONAL;
    lookup_request.allow_batch = 0;
    status = request->lookup->vtbl->prepare(request->lookup, &lookup_request,
        request->device, &plan->lookup);
    if (status != SIXEL_OK) {
        goto end;
    }
    if (plan->lookup == NULL) {
        status = SIXEL_LOGIC_ERROR;
        goto end;
    }
    status = plan->lookup->vtbl->describe(plan->lookup, &plan->view);
    if (status != SIXEL_OK) {
        goto end;
    }
    status = sixel_gpu_vulkan_view_check(&plan->view,
        SIXEL_GPU_LOOKUP_QUERY_FRACTIONAL);
    if (status != SIXEL_OK) {
        goto end;
    }
    if (plan->view.ncolors != lookup_request.ncolors ||
            memcmp(plan->view.palette, colors,
                lookup_request.ncolors * 3U) != 0) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    sixel_vk_lock();
    runtime = sixel_vk_runtime_get_locked();
    status = SIXEL_FEATURE_ERROR;
    if (runtime != NULL && burkes_gpu_vk_state_locked(runtime)) {
        limit = runtime->properties.limits.maxStorageBufferRange;
        status = pixels * 16U > limit ||
            plan->view.workspace_size > limit ? SIXEL_FEATURE_ERROR
            : burkes_gpu_vk_compile_locked(runtime, &plan->view, &plan->code);
    }
    sixel_vk_unlock();
    if (status != SIXEL_OK) {
        goto end;
    }
    /* Reserve lookup capacity during preparation. No builder is submitted,
     * so an unbuilt cache must not be marked valid by this temporary
     * lease. */
    warm_binding = NULL;
    status = plan->lookup->vtbl->begin(plan->lookup, &warm_binding);
    if (status != SIXEL_OK) {
        goto end;
    }
    if (warm_binding == NULL) {
        status = SIXEL_LOGIC_ERROR;
        goto end;
    }
    status = warm_binding->vtbl->finish(warm_binding, SIXEL_INTERRUPTED);
    warm_binding->vtbl->unref(warm_binding);
    if (status != SIXEL_OK) {
        goto end;
    }

    plan->params.width = (uint32_t)request->output.width;
    plan->params.height = (uint32_t)request->output.height;
    plan->params.ncolors = lookup_request.ncolors;
    plan->params.tiles_x = (plan->params.width + 63U) / 64U;
    plan->params.keycolor = (uint32_t)request->output.keycolor;
    plan->params.serpentine = request->scan == SIXEL_SCAN_SERPENTINE;
    /* The CPU limiter keeps (threshold - 1) * 255 of headroom on both
     * sides of the byte range. The deadzone discards small corrections
     * before limiting. */
    margin = (configuration->options.clamp_threshold - 1.0) * 255.0;
    plan->params.clamp_hard =
        configuration->options.clamp_policy == SIXEL_DITHER_CLAMP_HARD;
    plan->params.deadzone = (float)configuration->options.diffusion_deadzone;
    plan->params.clamp_lower = (float)-margin;
    plan->params.clamp_upper = (float)(255.0 + margin);

    sixel_vk_lock();
    runtime = sixel_vk_runtime_get_locked();
    status = SIXEL_FEATURE_ERROR;
    if (runtime != NULL) {
        burkes_gpu_vk_borrow_locked(plan);
        status = burkes_gpu_vk_reserve_locked(runtime, plan, pixels);
    }
    if (status == SIXEL_OK) {
        memcpy(plan->bundle.buffers[BURKES_GPU_VK_PALETTE].mapped,
            plan->view.palette, plan->view.palette_size);
        if (plan->view.constants_size != 0) {
            memcpy(plan->bundle.buffers[BURKES_GPU_VK_CONSTANTS].mapped,
                plan->view.constants, plan->view.constants_size);
        } else {
            memset(plan->bundle.buffers[BURKES_GPU_VK_CONSTANTS].mapped, 0,
                4U);
        }
        count = lookup_request.ncolors;
        memcpy(plan->bundle.buffers[BURKES_GPU_VK_COUNT].mapped, &count,
            sizeof(count));
    }
    sixel_vk_unlock();
    if (status != SIXEL_OK) {
        goto end;
    }
    *result = &plan->iface;

end:
    if (palette != NULL) {
        palette->vtbl->unref(palette);
    }
    if (status != SIXEL_OK) {
        burkes_gpu_vk_plan_unref(&plan->iface);
    }
    return status;
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
