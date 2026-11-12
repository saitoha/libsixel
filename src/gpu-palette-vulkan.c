/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * Vulkan palette apply: the legacy whole-frame kernel with the direct
 * (lookup NONE) scan or a byte FHEDT table built on the device in the same
 * submission. Other lookup policies stay on the CPU until their GLSL
 * programs exist; see sixel_gpu_palette_vulkan_supports_lut().
 */
/*
 * Vulkan program runtime and palette-apply consumer. This is the Vulkan
 * counterpart of gpu-palette-metal.m with gpu-palette-program.inc.h: the
 * runtime exposes a gpu_device whose program format is
 * "vulkan.lookup-rgb8.v1", turns a lookup plan's frozen program view into
 * pipelines and runs the fused per-pixel kernel around the policy's own
 * lookup_rgb8(). It never interprets a lookup policy, its constants or its
 * workspace; each policy owns those in its lookup-policy-<name>-gpu files.
 */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if defined(HAVE_VULKAN)

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compat_stub.h"
#include "gpu-palette.h"
#include "gpu-palette-vulkan.h"
#include "gpu-program.h"
#include "gpu-runtime-vulkan.h"
#include "sixel_atomic.h"
#include "bluenoise_64x64.h"

/* Readable sources: gpu-palette-fused-prefix.glsl,
 * gpu-palette-fused-suffix.glsl and gpu-lookup-builder-prefix.glsl. */
static char const * const g_sixel_vk_fused_prefix[] = {
#include <generated-vulkan/gpu-palette-fused-prefix-shader.inc.h>
};
static char const * const g_sixel_vk_fused_suffix[] = {
#include <generated-vulkan/gpu-palette-fused-suffix-shader.inc.h>
};
static char const * const g_sixel_vk_builder_prefix[] = {
#include <generated-vulkan/gpu-lookup-builder-prefix-shader.inc.h>
};

/* Must match the Params block in gpu-palette-fused-prefix.glsl (std430). */
typedef struct sixel_gpu_vulkan_params {
    uint32_t pixel_count;
    uint32_t width;
    uint32_t height;
    uint32_t ncolors;
    uint32_t mode;
    uint32_t has_transparent;
    uint32_t transparent_size;
    uint32_t transparent_keycolor;
    uint32_t has_6delta;
    uint32_t accumulation_pixel_count;
    uint32_t accumulation_valid_mask_size;
    uint32_t accumulation_keycolor;
    uint32_t accumulation_stride, accumulation_origin;
    uint32_t sixdelta_threshold;
    uint32_t sixdelta_metric;
    uint32_t sixdelta_compare;
    uint32_t residual_threshold;
    uint32_t residual_columns, residual_rows;
    uint32_t residual_tiled;
    uint32_t residual_cells[128];
    uint32_t accumulation_result_mask_size;
    float positional_strength;
    float bluenoise_strength;
    float bluenoise_gradient_factor;
    int32_t bluenoise_phase_x;
    int32_t bluenoise_phase_y;
    uint32_t bluenoise_channel_rgb;
    uint32_t gradient_size;
    uint32_t gradient_width;
    uint32_t gradient_height;
    unsigned char sixdelta_palette[SIXEL_PALETTE_MAX * 3];
} sixel_gpu_vulkan_params_t;

typedef struct sixel_gpu_vulkan_range {
    uint32_t first;
    uint32_t count;
    uint32_t pitch;
    uint32_t flag;
} sixel_gpu_vulkan_range_t;

/* A row range submitted through one runtime ring slot. */
typedef struct sixel_gpu_vulkan_chunk {
    int first;
    int rows;
    int inflight;
    unsigned int sequence;
} sixel_gpu_vulkan_chunk_t;

#define SIXEL_VK_LOCAL_SIZE 64U
#define SIXEL_VK_BINDINGS 13
#define SIXEL_VK_PROGRAM_CACHE 8

enum {
    SIXEL_VK_RESULT = 0,
    SIXEL_VK_PIXELS = 1,
    SIXEL_VK_PALETTE = 2,
    SIXEL_VK_TRANSPARENT = 3,
    SIXEL_VK_BLUE_NOISE = 4,
    SIXEL_VK_GRADIENT = 5,
    SIXEL_VK_PARAMS = 6,
    SIXEL_VK_ACCUMULATION = 7,
    SIXEL_VK_ACCUMULATION_VALID = 8,
    SIXEL_VK_ACCUMULATION_RESULT = 9,
    SIXEL_VK_CONSTANTS = 10,
    /* The lookup plan's leased workspace; never a host buffer here. */
    SIXEL_VK_WORKSPACE = 11,
    SIXEL_VK_INVALID = 12
};

enum {
    SIXEL_VK_MODE_NONE = 0,
    SIXEL_VK_MODE_BLUENOISE = 1,
    SIXEL_VK_MODE_A_DITHER = 2,
    SIXEL_VK_MODE_X_DITHER = 3
};

/* Only the runtime knows the native allocation behind a gpu_buffer. */
typedef struct sixel_gpu_vulkan_native_buffer {
    sixel_gpu_buffer_interface_t iface;
    sixel_atomic_u32_t refs;
    sixel_vk_runtime_t *runtime;
    sixel_vk_buffer_t storage;
    size_t size;
} sixel_gpu_vulkan_native_buffer_t;

/* Pipelines compiled from one program text. Live programs keep an evicted
 * entry alive through its reference count. */
typedef struct sixel_gpu_vulkan_compiled {
    char *key;
    VkPipeline fused;
    sixel_gpu_vulkan_builders_t builders;
    unsigned int refs;
    struct sixel_gpu_vulkan_compiled *next;
} sixel_gpu_vulkan_compiled_t;

struct sixel_gpu_program {
    sixel_gpu_lookup_plan_interface_t *plan;
    sixel_gpu_lookup_program_view_t view;
    sixel_gpu_vulkan_compiled_t *compiled;
};

typedef struct sixel_gpu_vulkan_palette_state {
    int probed;
    int ready;
    VkDescriptorSetLayout set_layout;
    VkPipelineLayout pipeline_layout;
    VkDescriptorSetLayout builder_set_layout;
    VkPipelineLayout builder_layout;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSet descriptor_set;
    VkDescriptorSet builder_set;
    sixel_vk_buffer_t buffers[SIXEL_VK_BINDINGS];
    sixel_vk_buffer_t count;
    sixel_vk_buffer_t dummy;
    /* Bytes each binding takes from its staging copy in this apply. */
    VkDeviceSize upload[SIXEL_VK_BINDINGS];
} sixel_gpu_vulkan_palette_state_t;

static sixel_gpu_vulkan_palette_state_t g_sixel_vk_palette;
static sixel_gpu_vulkan_compiled_t *g_sixel_vk_compiled;
static size_t g_sixel_vk_compiled_count;

/* --- gpu_device ------------------------------------------------------- */

static void
sixel_gpu_vulkan_buffer_ref(sixel_gpu_buffer_interface_t *self)
{
    sixel_gpu_vulkan_native_buffer_t *buffer;

    buffer = (sixel_gpu_vulkan_native_buffer_t *)self;
    (void)sixel_atomic_fetch_add_u32(&buffer->refs, 1U);
}

static void
sixel_gpu_vulkan_buffer_unref(sixel_gpu_buffer_interface_t *self)
{
    sixel_gpu_vulkan_native_buffer_t *buffer;

    buffer = (sixel_gpu_vulkan_native_buffer_t *)self;
    if (sixel_atomic_fetch_sub_u32(&buffer->refs, 1U) != 1U) {
        return;
    }
    /* Leases end only after their submissions completed, so no command
     * still uses the storage. Destroying it needs no queue lock. */
    sixel_vk_buffer_release(buffer->runtime, &buffer->storage);
    free(buffer);
}

static size_t
sixel_gpu_vulkan_buffer_size(sixel_gpu_buffer_interface_t const *self)
{
    return ((sixel_gpu_vulkan_native_buffer_t const *)self)->size;
}

/* Workspaces live in device-local memory the host never maps. */
static SIXELSTATUS
sixel_gpu_vulkan_buffer_shared_memory(
    sixel_gpu_buffer_interface_t const *self, void **memory)
{
    (void)self;
    if (memory == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *memory = NULL;
    return SIXEL_FEATURE_ERROR;
}

static sixel_gpu_buffer_vtbl_t const g_sixel_vk_buffer_vtbl = {
    sixel_gpu_vulkan_buffer_ref, sixel_gpu_vulkan_buffer_unref,
    sixel_gpu_vulkan_buffer_size, sixel_gpu_vulkan_buffer_shared_memory
};

static void
sixel_gpu_vulkan_device_ref(sixel_gpu_device_interface_t *self)
{
    /* The selected runtime device has process lifetime. */
    (void)self;
}

static char const *
sixel_gpu_vulkan_device_format(sixel_gpu_device_interface_t const *self)
{
    (void)self;
    return "vulkan.lookup-rgb8.v1";
}

static SIXELSTATUS
sixel_gpu_vulkan_device_buffer(sixel_gpu_device_interface_t *self,
    size_t bytes, sixel_gpu_buffer_interface_t **result)
{
    sixel_gpu_vulkan_native_buffer_t *buffer;
    sixel_vk_runtime_t *runtime;
    SIXELSTATUS status;

    (void)self;
    if (result == NULL || bytes == 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    *result = NULL;
    buffer = (sixel_gpu_vulkan_native_buffer_t *)calloc(1, sizeof(*buffer));
    if (buffer == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    sixel_vk_lock();
    runtime = sixel_vk_runtime_get_locked();
    status = runtime == NULL ? SIXEL_FEATURE_ERROR
        : sixel_vk_buffer_reserve_device(runtime, &buffer->storage,
            (VkDeviceSize)bytes);
    sixel_vk_unlock();
    if (SIXEL_FAILED(status)) {
        free(buffer);
        return status;
    }
    buffer->iface.vtbl = &g_sixel_vk_buffer_vtbl;
    buffer->refs = 1U;
    buffer->runtime = runtime;
    buffer->size = bytes;
    *result = &buffer->iface;
    return SIXEL_OK;
}

static sixel_gpu_device_vtbl_t const g_sixel_vk_device_vtbl = {
    sixel_gpu_vulkan_device_ref, sixel_gpu_vulkan_device_ref,
    sixel_gpu_vulkan_device_format, sixel_gpu_vulkan_device_buffer
};
static sixel_gpu_device_interface_t g_sixel_vk_device = {
    &g_sixel_vk_device_vtbl
};

sixel_vk_buffer_t const *
sixel_gpu_vulkan_buffer(sixel_gpu_device_interface_t *device,
    sixel_gpu_buffer_interface_t *buffer)
{
    if (device != &g_sixel_vk_device || buffer == NULL ||
            buffer->vtbl != &g_sixel_vk_buffer_vtbl) {
        return NULL;
    }
    return &((sixel_gpu_vulkan_native_buffer_t *)buffer)->storage;
}

int
sixel_gpu_vulkan_is_device(sixel_gpu_device_interface_t const *device)
{
    return device == &g_sixel_vk_device;
}

/* --- builders ---------------------------------------------------------- */

/* The builder layout of the program ABI: storage buffers 0..3 and no push
 * constants. Every consumer binds builders with it. */
SIXELSTATUS
sixel_gpu_vulkan_builder_layout_create(sixel_vk_runtime_t *runtime,
    VkDescriptorSetLayout *set_layout, VkPipelineLayout *layout)
{
    VkDescriptorSetLayoutBinding bindings[4];
    VkDescriptorSetLayoutCreateInfo set_layout_info;
    VkPipelineLayoutCreateInfo layout_info;
    int index;

    *set_layout = VK_NULL_HANDLE;
    *layout = VK_NULL_HANDLE;
    memset(bindings, 0, sizeof(bindings));
    for (index = 0; index < 4; ++index) {
        bindings[index].binding = (uint32_t)index;
        bindings[index].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[index].descriptorCount = 1;
        bindings[index].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }
    memset(&set_layout_info, 0, sizeof(set_layout_info));
    set_layout_info.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    set_layout_info.bindingCount = 4;
    set_layout_info.pBindings = bindings;
    if (vkCreateDescriptorSetLayout(runtime->device, &set_layout_info, NULL,
            set_layout) != VK_SUCCESS) {
        *set_layout = VK_NULL_HANDLE;
        return SIXEL_RUNTIME_ERROR;
    }
    memset(&layout_info, 0, sizeof(layout_info));
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount = 1;
    layout_info.pSetLayouts = set_layout;
    if (vkCreatePipelineLayout(runtime->device, &layout_info, NULL, layout)
            != VK_SUCCESS) {
        *layout = VK_NULL_HANDLE;
        return SIXEL_RUNTIME_ERROR;
    }
    return SIXEL_OK;
}

/* Validate the view's builder passes against the device limits, then
 * compile one pipeline per pass: the builder prelude, the policy's chunks
 * and a main() calling the pass's entry. Specialization constants 0..2
 * carry the pass's local dimensions. */
SIXELSTATUS
sixel_gpu_vulkan_builders_create(sixel_vk_runtime_t *runtime,
    VkPipelineLayout layout, sixel_gpu_lookup_program_view_t const *view,
    sixel_gpu_vulkan_builders_t *builders)
{
    VkSpecializationMapEntry entries[3];
    VkSpecializationInfo specialization;
    VkComputePipelineCreateInfo pipeline_info;
    VkPhysicalDeviceLimits const *limits;
    VkShaderModule module;
    sixel_vk_source_part_t parts[3];
    sixel_gpu_lookup_build_pass_t const *pass;
    char const *wrapper[1];
    char *text;
    uint32_t threads[3];
    size_t index;
    size_t axis;
    size_t total;
    SIXELSTATUS status;
    VkResult result;

    memset(builders, 0, sizeof(*builders));
    if (view->build_pass_count > SIXEL_GPU_VULKAN_MAX_BUILD_PASSES) {
        return SIXEL_FEATURE_ERROR;
    }
    limits = &runtime->properties.limits;
    for (index = 0; index < view->build_pass_count; ++index) {
        pass = &view->build_passes[index];
        if (pass->entry_point == NULL) {
            return SIXEL_BAD_ARGUMENT;
        }
        total = 1;
        for (axis = 0; axis < 3; ++axis) {
            if (pass->groups[axis] == 0 || pass->threads[axis] == 0 ||
                    pass->groups[axis] >
                        limits->maxComputeWorkGroupCount[axis] ||
                    pass->threads[axis] >
                        limits->maxComputeWorkGroupSize[axis] ||
                    pass->threads[axis] > SIZE_MAX / total) {
                return SIXEL_FEATURE_ERROR;
            }
            total *= pass->threads[axis];
        }
        if (total > limits->maxComputeWorkGroupInvocations) {
            return SIXEL_FEATURE_ERROR;
        }
    }
    for (axis = 0; axis < 3; ++axis) {
        entries[axis].constantID = (uint32_t)axis;
        entries[axis].offset = (uint32_t)(axis * sizeof(uint32_t));
        entries[axis].size = sizeof(uint32_t);
    }
    memset(&specialization, 0, sizeof(specialization));
    specialization.mapEntryCount = 3;
    specialization.pMapEntries = entries;
    specialization.dataSize = sizeof(threads);
    specialization.pData = threads;
    parts[0].chunks = g_sixel_vk_builder_prefix;
    parts[0].count = sizeof(g_sixel_vk_builder_prefix) /
        sizeof(g_sixel_vk_builder_prefix[0]);
    parts[1].chunks = view->chunks;
    parts[1].count = view->chunk_count;
    parts[2].chunks = wrapper;
    parts[2].count = 1;
    status = SIXEL_OK;
    for (index = 0; index < view->build_pass_count; ++index) {
        pass = &view->build_passes[index];
        /* tools/gen-vulkan-programs.sh appends the identical text. */
        text = (char *)malloc(strlen(pass->entry_point) + 32U);
        if (text == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            break;
        }
        sprintf(text, "\nvoid main()\n{\n    %s();\n}\n", pass->entry_point);
        wrapper[0] = text;
        status = sixel_vk_shader_module_compose(runtime, pass->entry_point,
            parts, 3, &module);
        free(text);
        if (SIXEL_FAILED(status)) {
            break;
        }
        for (axis = 0; axis < 3; ++axis) {
            threads[axis] = (uint32_t)pass->threads[axis];
        }
        memset(&pipeline_info, 0, sizeof(pipeline_info));
        pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipeline_info.stage.sType =
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipeline_info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        pipeline_info.stage.module = module;
        pipeline_info.stage.pName = "main";
        pipeline_info.stage.pSpecializationInfo = &specialization;
        pipeline_info.layout = layout;
        result = vkCreateComputePipelines(runtime->device, VK_NULL_HANDLE, 1,
            &pipeline_info, NULL, &builders->pipelines[index]);
        vkDestroyShaderModule(runtime->device, module, NULL);
        if (result != VK_SUCCESS) {
            builders->pipelines[index] = VK_NULL_HANDLE;
            status = SIXEL_RUNTIME_ERROR;
            break;
        }
        builders->count = index + 1U;
    }
    if (SIXEL_FAILED(status)) {
        sixel_gpu_vulkan_builders_destroy(runtime, builders);
    }
    return status;
}

void
sixel_gpu_vulkan_builders_destroy(sixel_vk_runtime_t *runtime,
    sixel_gpu_vulkan_builders_t *builders)
{
    size_t index;

    for (index = 0; index < builders->count; ++index) {
        if (builders->pipelines[index] != VK_NULL_HANDLE) {
            vkDestroyPipeline(runtime->device, builders->pipelines[index],
                NULL);
        }
    }
    memset(builders, 0, sizeof(*builders));
}

/* Record every builder pass with the given set bound; each pass's writes
 * are visible to the next pass and to the consumer that follows. */
void
sixel_gpu_vulkan_builders_record(VkCommandBuffer command,
    VkPipelineLayout layout, VkDescriptorSet set,
    sixel_gpu_vulkan_builders_t const *builders,
    sixel_gpu_lookup_program_view_t const *view)
{
    size_t index;

    for (index = 0; index < builders->count; ++index) {
        vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_COMPUTE,
            builders->pipelines[index]);
        vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_COMPUTE,
            layout, 0, 1, &set, 0, NULL);
        vkCmdDispatch(command, (uint32_t)view->build_passes[index].groups[0],
            (uint32_t)view->build_passes[index].groups[1],
            (uint32_t)view->build_passes[index].groups[2]);
        sixel_vk_record_barrier(command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
    }
}

/* Check the shape every consumer of the program ABI relies on. */
SIXELSTATUS
sixel_gpu_vulkan_view_check(sixel_gpu_lookup_program_view_t const *view,
    sixel_gpu_lookup_query_kind_t query_kind)
{
    if (view->program_format == NULL ||
            strcmp(view->program_format, "vulkan.lookup-rgb8.v1") != 0 ||
            view->query_kind != query_kind || view->batch != NULL) {
        return SIXEL_FEATURE_ERROR;
    }
    if (view->palette == NULL || view->ncolors < 1 ||
            view->ncolors > SIXEL_PALETTE_MAX ||
            view->palette_size < view->ncolors * 3U ||
            view->workspace_size == 0 || view->chunks == NULL ||
            view->chunk_count == 0 || view->constants_size % 4U != 0 ||
            (view->constants_size != 0 && view->constants == NULL) ||
            (view->build_pass_count != 0 && view->build_passes == NULL)) {
        return SIXEL_BAD_ARGUMENT;
    }
    return SIXEL_OK;
}

/* --- palette-apply state and programs ---------------------------------- */

static int
sixel_gpu_vulkan_palette_prepare_locked(sixel_vk_runtime_t *runtime)
{
    sixel_gpu_vulkan_palette_state_t *state;
    VkDescriptorSetLayoutBinding bindings[SIXEL_VK_BINDINGS];
    VkDescriptorSetLayoutCreateInfo set_layout_info;
    VkPushConstantRange push_range;
    VkPipelineLayoutCreateInfo layout_info;
    VkDescriptorPoolSize pool_size;
    VkDescriptorPoolCreateInfo pool_info;
    VkDescriptorSetAllocateInfo set_info;
    VkDescriptorSetLayout layouts[2];
    VkDescriptorSet sets[2];
    int index;

    state = &g_sixel_vk_palette;
    if (state->probed) {
        return state->ready;
    }
    state->probed = 1;

    memset(bindings, 0, sizeof(bindings));
    for (index = 0; index < SIXEL_VK_BINDINGS; ++index) {
        bindings[index].binding = (uint32_t)index;
        bindings[index].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[index].descriptorCount = 1;
        bindings[index].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }
    memset(&set_layout_info, 0, sizeof(set_layout_info));
    set_layout_info.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    set_layout_info.bindingCount = SIXEL_VK_BINDINGS;
    set_layout_info.pBindings = bindings;
    if (vkCreateDescriptorSetLayout(runtime->device, &set_layout_info, NULL,
            &state->set_layout) != VK_SUCCESS) {
        return 0;
    }
    push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    push_range.offset = 0;
    push_range.size = sizeof(sixel_gpu_vulkan_range_t);
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
    pool_size.descriptorCount = SIXEL_VK_BINDINGS + 4;
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

    /* The blue-noise table never changes; upload it once. */
    if (SIXEL_FAILED(sixel_vk_buffer_reserve(runtime,
            &state->buffers[SIXEL_VK_BLUE_NOISE], sizeof(sixel_bn64), 1)) ||
            SIXEL_FAILED(sixel_vk_buffer_reserve(runtime, &state->dummy, 4,
                0))) {
        return 0;
    }
    memcpy(state->buffers[SIXEL_VK_BLUE_NOISE].mapped, sixel_bn64,
        sizeof(sixel_bn64));
    memset(state->dummy.mapped, 0, 4);
    /* Objects stay allocated until process exit; the runtime is never torn
     * down while other components could still hold the device. */
    state->ready = 1;
    return 1;
}

int
sixel_gpu_palette_vulkan_is_available(void)
{
    sixel_vk_runtime_t *runtime;
    int available;

    sixel_vk_lock();
    runtime = sixel_vk_runtime_get_locked();
    available = runtime != NULL &&
        sixel_gpu_vulkan_palette_prepare_locked(runtime);
    sixel_vk_unlock();
    return available;
}

SIXELSTATUS
sixel_gpu_device_acquire(sixel_gpu_device_interface_t **result)
{
    if (result == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *result = NULL;
    if (!sixel_gpu_palette_vulkan_is_available()) {
        return SIXEL_FEATURE_ERROR;
    }
    *result = &g_sixel_vk_device;
    return SIXEL_OK;
}

static void
sixel_gpu_vulkan_compiled_unref(sixel_vk_runtime_t *runtime,
    sixel_gpu_vulkan_compiled_t *compiled)
{
    if (--compiled->refs != 0) {
        return;
    }
    if (compiled->fused != VK_NULL_HANDLE) {
        vkDestroyPipeline(runtime->device, compiled->fused, NULL);
    }
    sixel_gpu_vulkan_builders_destroy(runtime, &compiled->builders);
    free(compiled->key);
    free(compiled);
}

/* The full composed text plus the builder entries and dimensions name the
 * compiled code, never a policy name. */
static char *
sixel_gpu_vulkan_program_key(sixel_gpu_lookup_program_view_t const *view)
{
    sixel_gpu_lookup_build_pass_t const *pass;
    char *key;
    size_t length;
    size_t index;

    length = 1;
    for (index = 0; index < view->chunk_count; ++index) {
        length += strlen(view->chunks[index]);
    }
    for (index = 0; index < view->build_pass_count; ++index) {
        length += strlen(view->build_passes[index].entry_point) + 80U;
    }
    key = (char *)malloc(length);
    if (key == NULL) {
        return NULL;
    }
    length = 0;
    for (index = 0; index < view->chunk_count; ++index) {
        strcpy(key + length, view->chunks[index]);
        length += strlen(view->chunks[index]);
    }
    for (index = 0; index < view->build_pass_count; ++index) {
        pass = &view->build_passes[index];
        length += (size_t)sprintf(key + length, "\n// entry:%s %lu %lu %lu",
            pass->entry_point, (unsigned long)pass->threads[0],
            (unsigned long)pass->threads[1],
            (unsigned long)pass->threads[2]);
    }
    key[length] = '\0';
    return key;
}

static SIXELSTATUS
sixel_gpu_vulkan_compile_locked(sixel_vk_runtime_t *runtime,
    sixel_gpu_lookup_program_view_t const *view,
    sixel_gpu_vulkan_compiled_t **result)
{
    sixel_gpu_vulkan_palette_state_t *state;
    sixel_gpu_vulkan_compiled_t *compiled;
    sixel_gpu_vulkan_compiled_t **link;
    sixel_vk_source_part_t parts[3];
    VkComputePipelineCreateInfo pipeline_info;
    VkShaderModule module;
    VkResult vk_result;
    char *key;
    SIXELSTATUS status;

    *result = NULL;
    state = &g_sixel_vk_palette;
    key = sixel_gpu_vulkan_program_key(view);
    if (key == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    for (compiled = g_sixel_vk_compiled; compiled != NULL;
            compiled = compiled->next) {
        if (strcmp(compiled->key, key) == 0) {
            free(key);
            ++compiled->refs;
            *result = compiled;
            return SIXEL_OK;
        }
    }
    compiled = (sixel_gpu_vulkan_compiled_t *)calloc(1, sizeof(*compiled));
    if (compiled == NULL) {
        free(key);
        return SIXEL_BAD_ALLOCATION;
    }
    compiled->key = key;
    compiled->refs = 1;
    parts[0].chunks = g_sixel_vk_fused_prefix;
    parts[0].count = sizeof(g_sixel_vk_fused_prefix) /
        sizeof(g_sixel_vk_fused_prefix[0]);
    parts[1].chunks = view->chunks;
    parts[1].count = view->chunk_count;
    parts[2].chunks = g_sixel_vk_fused_suffix;
    parts[2].count = sizeof(g_sixel_vk_fused_suffix) /
        sizeof(g_sixel_vk_fused_suffix[0]);
    status = sixel_vk_shader_module_compose(runtime,
        view->logical_name != NULL ? view->logical_name : "lookup",
        parts, 3, &module);
    if (SIXEL_SUCCEEDED(status)) {
        memset(&pipeline_info, 0, sizeof(pipeline_info));
        pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipeline_info.stage.sType =
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipeline_info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        pipeline_info.stage.module = module;
        pipeline_info.stage.pName = "main";
        pipeline_info.layout = state->pipeline_layout;
        vk_result = vkCreateComputePipelines(runtime->device,
            VK_NULL_HANDLE, 1, &pipeline_info, NULL, &compiled->fused);
        vkDestroyShaderModule(runtime->device, module, NULL);
        if (vk_result != VK_SUCCESS) {
            compiled->fused = VK_NULL_HANDLE;
            status = SIXEL_RUNTIME_ERROR;
        }
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_gpu_vulkan_builders_create(runtime,
            state->builder_layout, view, &compiled->builders);
    }
    if (SIXEL_FAILED(status)) {
        sixel_gpu_vulkan_compiled_unref(runtime, compiled);
        return status;
    }
    /* Bounded code retention; live programs keep evicted code alive. */
    if (g_sixel_vk_compiled_count == SIXEL_VK_PROGRAM_CACHE) {
        link = &g_sixel_vk_compiled;
        while ((*link)->next != NULL) {
            link = &(*link)->next;
        }
        sixel_gpu_vulkan_compiled_unref(runtime, *link);
        *link = NULL;
        --g_sixel_vk_compiled_count;
    }
    compiled->next = g_sixel_vk_compiled;
    g_sixel_vk_compiled = compiled;
    ++g_sixel_vk_compiled_count;
    ++compiled->refs;
    *result = compiled;
    return SIXEL_OK;
}

SIXELSTATUS
sixel_gpu_program_prepare(sixel_gpu_device_interface_t *device,
    sixel_gpu_lookup_plan_interface_t *plan, sixel_gpu_program_t **result)
{
    sixel_gpu_program_t *program;
    sixel_vk_runtime_t *runtime;
    SIXELSTATUS status;

    if (device == NULL || plan == NULL || result == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *result = NULL;
    if (device != &g_sixel_vk_device) {
        return SIXEL_FEATURE_ERROR;
    }
    program = (sixel_gpu_program_t *)calloc(1, sizeof(*program));
    if (program == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    status = plan->vtbl->describe(plan, &program->view);
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_gpu_vulkan_view_check(&program->view,
            SIXEL_GPU_LOOKUP_QUERY_BYTE);
    }
    if (SIXEL_SUCCEEDED(status)) {
        sixel_vk_lock();
        runtime = sixel_vk_runtime_get_locked();
        status = runtime == NULL ||
            !sixel_gpu_vulkan_palette_prepare_locked(runtime)
            ? SIXEL_FEATURE_ERROR
            : sixel_gpu_vulkan_compile_locked(runtime, &program->view,
                &program->compiled);
        sixel_vk_unlock();
    }
    if (SIXEL_FAILED(status)) {
        free(program);
        return status;
    }
    program->plan = plan;
    plan->vtbl->ref(plan);
    *result = program;
    return SIXEL_OK;
}

void
sixel_gpu_program_destroy(sixel_gpu_program_t *program)
{
    sixel_vk_runtime_t *runtime;

    if (program == NULL) {
        return;
    }
    if (program->compiled != NULL) {
        sixel_vk_lock();
        runtime = sixel_vk_runtime_get_locked();
        if (runtime != NULL) {
            sixel_gpu_vulkan_compiled_unref(runtime, program->compiled);
        }
        sixel_vk_unlock();
    }
    program->plan->vtbl->unref(program->plan);
    free(program);
}

/* --- palette apply ----------------------------------------------------- */

/* Fold a 1-D invocation count into a 2-D grid within device limits. */
static SIXELSTATUS
sixel_gpu_vulkan_groups(sixel_vk_runtime_t const *runtime, uint64_t count,
    uint32_t *groups_x, uint32_t *groups_y)
{
    uint64_t groups;
    uint32_t limit;

    groups = (count + SIXEL_VK_LOCAL_SIZE - 1U) / SIXEL_VK_LOCAL_SIZE;
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
sixel_gpu_vulkan_fill_params(sixel_gpu_vulkan_params_t *params,
    sixel_gpu_palette_request_t const *request)
{
    memset(params, 0, sizeof(*params));
    /* Without native retained storage the frame rectangle is packed, so
     * the retained plane is addressed with frame stride and no origin. */
    params->accumulation_stride = (uint32_t)request->width;
    params->accumulation_origin = 0;
    params->pixel_count = (uint32_t)request->pixel_count;
    params->width = (uint32_t)request->width;
    params->height = (uint32_t)request->height;
    params->ncolors = (uint32_t)request->ncolors;
    params->mode = SIXEL_VK_MODE_NONE;
    if (request->method_for_diffuse == SIXEL_DIFFUSE_BLUENOISE_DITHER) {
        params->mode = SIXEL_VK_MODE_BLUENOISE;
    } else if (request->method_for_diffuse == SIXEL_DIFFUSE_A_DITHER) {
        params->mode = SIXEL_VK_MODE_A_DITHER;
    } else if (request->method_for_diffuse == SIXEL_DIFFUSE_X_DITHER) {
        params->mode = SIXEL_VK_MODE_X_DITHER;
    }
    params->positional_strength = request->positional_strength;
    if (request->transparent_mask != NULL &&
            request->transparent_keycolor >= 0 &&
            request->transparent_keycolor < SIXEL_PALETTE_MAX) {
        params->has_transparent = 1U;
        params->transparent_size = (uint32_t)request->transparent_mask_size;
        params->transparent_keycolor =
            (uint32_t)request->transparent_keycolor;
    }
    if (request->has_6delta_accumulation != 0 &&
            request->accumulation_pixels != NULL &&
            request->accumulation_keycolor >= 0 &&
            request->accumulation_keycolor < SIXEL_PALETTE_MAX) {
        params->has_6delta = 1U;
        params->accumulation_pixel_count = (uint32_t)request->pixel_count;
        params->accumulation_valid_mask_size =
            request->accumulation_valid_mask != NULL
                ? (uint32_t)request->pixel_count : 0U;
        params->accumulation_keycolor =
            (uint32_t)request->accumulation_keycolor;
        params->sixdelta_threshold = (uint32_t)request->sixdelta_threshold;
        params->sixdelta_metric = (uint32_t)request->sixdelta_metric;
        params->sixdelta_compare = (uint32_t)request->sixdelta_compare;
        if (request->residual_mask != NULL) {
            params->residual_columns = request->residual_mask->columns;
            params->residual_rows = request->residual_mask->rows;
            params->residual_tiled = request->residual_mask->tiled;
            params->residual_threshold = request->residual_mask->threshold;
            memcpy(params->residual_cells, request->residual_mask->cells,
                sizeof(params->residual_cells));
        }
        params->accumulation_result_mask_size =
            (uint32_t)request->accumulation_result_mask_size;
        memcpy(params->sixdelta_palette,
            request->sixdelta_comparison_palette != NULL
                ? request->sixdelta_comparison_palette : request->palette,
            (size_t)request->ncolors * 3U);
    }
    params->bluenoise_strength = request->bluenoise_strength;
    params->bluenoise_gradient_factor = request->bluenoise_gradient_factor;
    params->bluenoise_phase_x = (int32_t)request->bluenoise_phase_x;
    params->bluenoise_phase_y = (int32_t)request->bluenoise_phase_y;
    params->bluenoise_channel_rgb =
        request->bluenoise_channel_rgb != 0 ? 1U : 0U;
    params->gradient_size = (uint32_t)request->bluenoise_gradient_map_size;
    params->gradient_width = (uint32_t)request->bluenoise_gradient_width;
    params->gradient_height = (uint32_t)request->bluenoise_gradient_height;
}

/* Copy one retained plane's frame rectangle into a packed buffer. */
static void
sixel_gpu_vulkan_pack_retained(unsigned char *dest,
    unsigned char const *bytes, sixel_gpu_palette_request_t const *request,
    size_t depth)
{
    size_t stride;
    size_t row_bytes;
    int y;

    stride = (size_t)(request->accumulation_width != 0
        ? request->accumulation_width : request->width) * depth;
    row_bytes = (size_t)request->width * depth;
    bytes += (size_t)request->accumulation_origin_y * stride +
        (size_t)request->accumulation_origin_x * depth;
    if (stride == row_bytes) {
        memcpy(dest, bytes, request->pixel_count * depth);
        return;
    }
    for (y = 0; y < request->height; ++y) {
        memcpy(dest + (size_t)y * row_bytes, bytes + (size_t)y * stride,
            row_bytes);
    }
}

static SIXELSTATUS
sixel_gpu_vulkan_upload(sixel_vk_runtime_t *runtime, int slot,
    void const *bytes, size_t size)
{
    sixel_vk_buffer_t *buffer;
    SIXELSTATUS status;

    buffer = &g_sixel_vk_palette.buffers[slot];
    /* The accumulation result mask is also read back after the dispatch. */
    status = sixel_vk_buffer_reserve(runtime, buffer, size,
        slot != SIXEL_VK_ACCUMULATION_RESULT);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (bytes != NULL && size != 0) {
        memcpy(buffer->mapped, bytes, size);
        g_sixel_vk_palette.upload[slot] = size;
    }
    return SIXEL_OK;
}

/* Run the fused kernel of a prepared program. The plan's binding leases
 * the policy's workspace; its builders run first in the same submission
 * when the lease asks for a build. begin and finish run outside the
 * runtime lock because a policy may allocate or recycle device storage. */
SIXELSTATUS
sixel_gpu_palette_vulkan_apply(sixel_gpu_palette_request_t const *request)
{
    sixel_gpu_vulkan_palette_state_t *state;
    sixel_vk_runtime_t *runtime;
    sixel_gpu_vulkan_params_t params;
    sixel_gpu_vulkan_range_t range;
    sixel_gpu_vulkan_chunk_t chunks[SIXEL_VK_RING];
    sixel_gpu_vulkan_chunk_t *chunk;
    sixel_gpu_palette_test_hooks_t *hooks;
    sixel_gpu_program_t *program;
    sixel_gpu_lookup_program_view_t const *view;
    sixel_gpu_lookup_binding_interface_t *binding;
    sixel_gpu_lookup_binding_view_t binding_view;
    sixel_vk_buffer_t const *workspace;
    sixel_vk_buffer_t const *bound[SIXEL_VK_BINDINGS];
    sixel_vk_buffer_t const *builder_bound[4];
    VkDescriptorBufferInfo infos[SIXEL_VK_BINDINGS + 4];
    VkWriteDescriptorSet writes[SIXEL_VK_BINDINGS + 4];
    VkCommandBuffer command;
    uint32_t groups_x;
    uint32_t groups_y;
    uint32_t count;
    uint32_t zeros[SIXEL_VK_RING];
    uint32_t flag;
    size_t pixel_count;
    size_t offset;
    size_t length;
    double started;
    double staged;
    double waited;
    int has_accumulation;
    int has_valid;
    int index;
    int locked;
    int streamed;
    int rows;
    int next;
    int head;
    int pending;
    int slot;
    unsigned int submitted;
    SIXELSTATUS status;

    program = request == NULL ? NULL : request->program;
    if (program == NULL || request->retained_rgb != NULL) {
        return SIXEL_FEATURE_ERROR;
    }
    view = &program->view;
    if ((size_t)request->ncolors != view->ncolors ||
            request->palette == NULL ||
            memcmp(request->palette, view->palette,
                (size_t)view->ncolors * 3U) != 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    pixel_count = request->pixel_count;
    hooks = request->test_hooks;
    memset(chunks, 0, sizeof(chunks));
    command = VK_NULL_HANDLE;
    runtime = NULL;
    locked = 0;
    state = &g_sixel_vk_palette;
    binding = NULL;
    memset(&binding_view, 0, sizeof(binding_view));
    status = program->plan->vtbl->begin(program->plan, &binding);
    if (SIXEL_SUCCEEDED(status) && binding == NULL) {
        status = SIXEL_LOGIC_ERROR;
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = binding->vtbl->get_view(binding, &binding_view);
    }
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    workspace = sixel_gpu_vulkan_buffer(&g_sixel_vk_device,
        binding_view.workspace);
    if (workspace == NULL || binding_view.workspace->vtbl->size(
            binding_view.workspace) < view->workspace_size) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    sixel_vk_lock();
    locked = 1;
    runtime = sixel_vk_runtime_get_locked();
    if (runtime == NULL || !sixel_gpu_vulkan_palette_prepare_locked(runtime)) {
        sixel_helper_set_additional_message(
            "gpu palette apply: Vulkan is unavailable.");
        status = SIXEL_FEATURE_ERROR;
        goto end;
    }
    started = sixel_vk_now_ms();
    if (request->test_hooks != NULL) {
        /* Kernel numbers follow sixel_gpu_palette_test_hooks_t. */
        request->test_hooks->entered = 1;
        request->test_hooks->kernel = 9;
        request->test_hooks->rebuild = binding_view.needs_build;
        request->test_hooks->published = 0;
    }

    sixel_gpu_vulkan_fill_params(&params, request);
    memset(state->upload, 0, sizeof(state->upload));
    /* The blue-noise staging copy never changes. Copying the small table
     * every apply is cheaper than tracking when device storage was filled. */
    state->upload[SIXEL_VK_BLUE_NOISE] = sizeof(sixel_bn64);
    has_accumulation = params.has_6delta != 0;
    has_valid = has_accumulation && request->accumulation_valid_mask != NULL;

    status = sixel_vk_buffer_reserve(runtime,
        &state->buffers[SIXEL_VK_RESULT], pixel_count, 0);
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_gpu_vulkan_upload(runtime, SIXEL_VK_PIXELS,
            request->pixels, pixel_count * 3U);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_gpu_vulkan_upload(runtime, SIXEL_VK_PALETTE,
            view->palette, (size_t)view->ncolors * 3U);
    }
    if (SIXEL_SUCCEEDED(status)) {
        /* A zero constant size binds a harmless zero word. */
        status = sixel_gpu_vulkan_upload(runtime, SIXEL_VK_CONSTANTS,
            view->constants_size != 0 ? view->constants
                : (void const *)state->dummy.mapped,
            view->constants_size != 0 ? view->constants_size : 4U);
    }
    if (SIXEL_SUCCEEDED(status)) {
        /* One cleared validation word per ring slot; a reused slot clears
         * its word again in its own command. */
        memset(zeros, 0, sizeof(zeros));
        status = sixel_gpu_vulkan_upload(runtime, SIXEL_VK_INVALID, zeros,
            sizeof(zeros));
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_vk_buffer_reserve(runtime, &state->count,
            sizeof(count), 1);
        if (SIXEL_SUCCEEDED(status)) {
            count = view->ncolors;
            memcpy(state->count.mapped, &count, sizeof(count));
        }
    }
    if (SIXEL_SUCCEEDED(status) && request->transparent_mask != NULL) {
        status = sixel_gpu_vulkan_upload(runtime, SIXEL_VK_TRANSPARENT,
            request->transparent_mask, request->transparent_mask_size);
    }
    if (SIXEL_SUCCEEDED(status) && request->bluenoise_gradient_map != NULL) {
        status = sixel_gpu_vulkan_upload(runtime, SIXEL_VK_GRADIENT,
            request->bluenoise_gradient_map,
            request->bluenoise_gradient_map_size);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_gpu_vulkan_upload(runtime, SIXEL_VK_PARAMS,
            &params, sizeof(params));
    }
    if (SIXEL_SUCCEEDED(status) && has_accumulation) {
        status = sixel_vk_buffer_reserve(runtime,
            &state->buffers[SIXEL_VK_ACCUMULATION], pixel_count * 3U, 1);
        if (SIXEL_SUCCEEDED(status)) {
            sixel_gpu_vulkan_pack_retained(
                state->buffers[SIXEL_VK_ACCUMULATION].mapped,
                request->accumulation_pixels, request, 3U);
            state->upload[SIXEL_VK_ACCUMULATION] = pixel_count * 3U;
        }
    }
    if (SIXEL_SUCCEEDED(status) && has_valid) {
        status = sixel_vk_buffer_reserve(runtime,
            &state->buffers[SIXEL_VK_ACCUMULATION_VALID], pixel_count, 1);
        if (SIXEL_SUCCEEDED(status)) {
            sixel_gpu_vulkan_pack_retained(
                state->buffers[SIXEL_VK_ACCUMULATION_VALID].mapped,
                request->accumulation_valid_mask, request, 1U);
            state->upload[SIXEL_VK_ACCUMULATION_VALID] = pixel_count;
        }
    }
    if (SIXEL_SUCCEEDED(status) && request->accumulation_result_mask != NULL) {
        status = sixel_gpu_vulkan_upload(runtime,
            SIXEL_VK_ACCUMULATION_RESULT, request->accumulation_result_mask,
            request->accumulation_result_mask_size);
    }
    if (SIXEL_SUCCEEDED(status) && request->test_hooks != NULL &&
            request->test_hooks->fault == 1) {
        status = SIXEL_BAD_ALLOCATION;
    }
    if (SIXEL_FAILED(status)) {
        sixel_helper_set_additional_message(
            "gpu palette apply: Vulkan buffer allocation failed.");
        goto end;
    }

    for (index = 0; index < SIXEL_VK_BINDINGS; ++index) {
        bound[index] = &state->buffers[index];
    }
    if (request->transparent_mask == NULL) {
        bound[SIXEL_VK_TRANSPARENT] = &state->dummy;
    }
    if (request->bluenoise_gradient_map == NULL) {
        bound[SIXEL_VK_GRADIENT] = &state->dummy;
    }
    if (!has_accumulation) {
        bound[SIXEL_VK_ACCUMULATION] = &state->dummy;
    }
    if (!has_valid) {
        bound[SIXEL_VK_ACCUMULATION_VALID] = &state->dummy;
    }
    if (request->accumulation_result_mask == NULL) {
        bound[SIXEL_VK_ACCUMULATION_RESULT] = &state->dummy;
    }
    bound[SIXEL_VK_WORKSPACE] = workspace;
    builder_bound[0] = bound[SIXEL_VK_PALETTE];
    builder_bound[1] = &state->count;
    builder_bound[2] = bound[SIXEL_VK_CONSTANTS];
    builder_bound[3] = workspace;
    memset(writes, 0, sizeof(writes));
    for (index = 0; index < SIXEL_VK_BINDINGS + 4; ++index) {
        infos[index].buffer = index < SIXEL_VK_BINDINGS
            ? bound[index]->buffer
            : builder_bound[index - SIXEL_VK_BINDINGS]->buffer;
        infos[index].offset = 0;
        infos[index].range = VK_WHOLE_SIZE;
        writes[index].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[index].dstSet = index < SIXEL_VK_BINDINGS
            ? state->descriptor_set : state->builder_set;
        writes[index].dstBinding = (uint32_t)(index < SIXEL_VK_BINDINGS
            ? index : index - SIXEL_VK_BINDINGS);
        writes[index].descriptorCount = 1;
        writes[index].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[index].pBufferInfo = &infos[index];
    }
    vkUpdateDescriptorSets(runtime->device, SIXEL_VK_BINDINGS + 4, writes, 0,
        NULL);

    /* Rows stream to the caller in ranges when it consumes them, as the
     * Metal palette application does: up to SIXEL_VK_RING commands stay in
     * flight, each with its own validation word, and completed ranges are
     * copied out and published in submission order. Otherwise one range
     * covers the frame. */
    streamed = request->rows_ready != NULL && request->rows_per_chunk > 0 &&
        request->rows_per_chunk < request->height;
    rows = streamed ? request->rows_per_chunk : request->height;
    next = head = pending = 0;
    submitted = 0;
    staged = sixel_vk_now_ms();
    while (next < request->height || pending != 0) {
        while (next < request->height && pending < SIXEL_VK_RING) {
            if (request->cancel != NULL &&
                    request->cancel(request->cancel_context)) {
                status = SIXEL_INTERRUPTED;
                goto end;
            }
            /* Test fault 2 models a failed submission of one command. */
            if (hooks != NULL && hooks->fault == 2 &&
                    hooks->fault_after_chunks == submitted) {
                status = SIXEL_RUNTIME_ERROR;
                goto end;
            }
            slot = (head + pending) % SIXEL_VK_RING;
            chunk = &chunks[slot];
            chunk->first = next;
            chunk->rows = request->height - next < rows
                ? request->height - next : rows;
            chunk->sequence = submitted;
            status = sixel_vk_ring_begin(runtime, slot, &command);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
            if (submitted == 0) {
                for (index = 0; index < SIXEL_VK_BINDINGS; ++index) {
                    if (bound[index] == &state->dummy) {
                        sixel_vk_buffer_record_upload(command, &state->dummy,
                            4);
                    } else if (index != SIXEL_VK_WORKSPACE) {
                        sixel_vk_buffer_record_upload(command, bound[index],
                            state->upload[index]);
                    }
                }
                sixel_vk_buffer_record_upload(command, &state->count,
                    sizeof(count));
                sixel_vk_record_barrier(command,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_ACCESS_TRANSFER_WRITE_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
                if (binding_view.needs_build) {
                    /* The builders read the palette just uploaded; their
                     * barriers order the workspace writes before the
                     * fused kernel reads them. */
                    sixel_gpu_vulkan_builders_record(command,
                        state->builder_layout, state->builder_set,
                        &program->compiled->builders, view);
                }
            } else {
                /* Order this range after every earlier submission's
                 * uploads, builds, dispatches and copies. */
                sixel_vk_record_barrier(command,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT |
                        VK_ACCESS_TRANSFER_READ_BIT |
                        VK_ACCESS_TRANSFER_WRITE_BIT);
                if (submitted >= SIXEL_VK_RING) {
                    /* The slot's previous range has completed and been
                     * read; clear its validation word for this one. */
                    vkCmdFillBuffer(command,
                        state->buffers[SIXEL_VK_INVALID].buffer,
                        (VkDeviceSize)slot * sizeof(uint32_t),
                        sizeof(uint32_t), 0);
                    sixel_vk_record_barrier(command,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_ACCESS_TRANSFER_WRITE_BIT,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        VK_ACCESS_SHADER_READ_BIT |
                            VK_ACCESS_SHADER_WRITE_BIT);
                }
            }
            offset = (size_t)chunk->first * request->width;
            length = (size_t)chunk->rows * request->width;
            status = sixel_gpu_vulkan_groups(runtime, (uint64_t)length,
                &groups_x, &groups_y);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
            range.first = (uint32_t)offset;
            range.count = (uint32_t)length;
            range.pitch = groups_x * SIXEL_VK_LOCAL_SIZE;
            range.flag = (uint32_t)slot;
            vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_COMPUTE,
                program->compiled->fused);
            vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_COMPUTE,
                state->pipeline_layout, 0, 1, &state->descriptor_set, 0,
                NULL);
            vkCmdPushConstants(command, state->pipeline_layout,
                VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(range), &range);
            vkCmdDispatch(command, groups_x, groups_y, 1);
            sixel_vk_record_barrier(command,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_ACCESS_TRANSFER_READ_BIT);
            sixel_vk_buffer_record_download_range(command,
                &state->buffers[SIXEL_VK_RESULT], offset, length);
            sixel_vk_buffer_record_download_range(command,
                &state->buffers[SIXEL_VK_INVALID],
                (VkDeviceSize)slot * sizeof(uint32_t), sizeof(uint32_t));
            if (request->accumulation_result_mask != NULL) {
                sixel_vk_buffer_record_download_range(command,
                    &state->buffers[SIXEL_VK_ACCUMULATION_RESULT], offset,
                    length);
            }
            /* Make shader and copy writes visible to mapped host memory. */
            sixel_vk_record_barrier(command,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_HOST_READ_BIT);
            status = sixel_vk_ring_submit(runtime, slot);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
            chunk->inflight = 1;
            ++submitted;
            ++pending;
            next += chunk->rows;
            if (hooks != NULL) {
                hooks->submitted_chunks = submitted;
                if ((unsigned int)pending > hooks->peak_inflight) {
                    hooks->peak_inflight = (unsigned int)pending;
                }
            }
        }
        chunk = &chunks[head];
        status = sixel_vk_ring_wait(runtime, head);
        chunk->inflight = 0;
        if (hooks != NULL) {
            ++hooks->completed_chunks;
        }
        /* Test fault 3 models a failed completion of one command; neither
         * fault lets the lease publish a build. */
        if (SIXEL_SUCCEEDED(status) && hooks != NULL && hooks->fault == 3 &&
                hooks->fault_after_chunks == chunk->sequence) {
            status = SIXEL_RUNTIME_ERROR;
        }
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        memcpy(&flag, (unsigned char *)state->buffers[SIXEL_VK_INVALID]
            .mapped + (size_t)head * sizeof(uint32_t), sizeof(flag));
        if (flag != 0) {
            sixel_helper_set_additional_message(
                "gpu palette apply: the lookup returned an invalid index.");
            status = SIXEL_LOGIC_ERROR;
            goto end;
        }
        if (request->cancel != NULL &&
                request->cancel(request->cancel_context)) {
            status = SIXEL_INTERRUPTED;
            goto end;
        }
        offset = (size_t)chunk->first * request->width;
        length = (size_t)chunk->rows * request->width;
        memcpy(request->dest + offset,
            (unsigned char *)state->buffers[SIXEL_VK_RESULT].mapped + offset,
            length);
        if (request->accumulation_result_mask != NULL) {
            memcpy(request->accumulation_result_mask + offset,
                (unsigned char *)
                    state->buffers[SIXEL_VK_ACCUMULATION_RESULT].mapped +
                    offset, length);
        }
        if (streamed) {
            /* The callback runs on the caller under the runtime lock and
             * must not reenter GPU execution. Later ranges continue on the
             * device while it consumes this completed prefix. */
            status = request->rows_ready(request->rows_context,
                chunk->first + chunk->rows);
            if (status != SIXEL_OK) {
                goto end;
            }
            if (hooks != NULL) {
                hooks->notified_rows =
                    (unsigned int)(chunk->first + chunk->rows);
            }
        }
        head = (head + 1) % SIXEL_VK_RING;
        --pending;
    }
    waited = sixel_vk_now_ms();
    if (hooks != NULL) {
        hooks->published = 1;
    }
    if (runtime->debug) {
        /* stage: host copies into mapped memory; submit: record, submit
         * and fence wait (gpu: device time inside it); read: result copy
         * out of mapped memory. */
        fprintf(stderr, "libsixel: vulkan palette apply %ux%u, %d colors, "
            "%s %s: %.2f ms (stage %.2f, submit %.2f, gpu %.2f, "
            "read %.2f)\n",
            (unsigned int)request->width, (unsigned int)request->height,
            request->ncolors, view->logical_name != NULL
                ? view->logical_name : "lookup",
            binding_view.needs_build ? "build" : "reuse",
            sixel_vk_now_ms() - started,
            staged - started, waited - staged, runtime->last_busy_ms,
            sixel_vk_now_ms() - waited);
    }
    status = SIXEL_OK;

end:
    /* Never finish the lookup lease or release the shared buffers while a
     * submitted range can still write. */
    for (slot = 0; slot < SIXEL_VK_RING; ++slot) {
        if (chunks[slot].inflight) {
            (void)sixel_vk_ring_wait(runtime, slot);
            chunks[slot].inflight = 0;
            if (hooks != NULL) {
                ++hooks->completed_chunks;
            }
        }
    }
    if (locked) {
        sixel_vk_unlock();
    }
    if (binding != NULL) {
        (void)binding->vtbl->finish(binding, status);
        binding->vtbl->unref(binding);
    }
    return status;
}

#endif /* HAVE_VULKAN */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
