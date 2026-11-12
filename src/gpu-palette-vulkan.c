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
 * submission. The scan also serves every exact lookup policy, which picks
 * the lowest index among equidistant entries just as the scan does. Other
 * lookup policies stay on the CPU until their GLSL programs exist; see
 * sixel_gpu_palette_vulkan_supports_lut().
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if defined(HAVE_VULKAN)

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "compat_stub.h"
#include "gpu-palette.h"
#include "gpu-palette-vulkan.h"
#include "gpu-runtime-vulkan.h"
#include "bluenoise_64x64.h"

#if defined(HAVE_VULKAN_PREBUILT_SPIRV)
static unsigned char const g_sixel_vk_palette_spirv[] = {
#include <generated-vulkan/gpu-palette-vulkan-spirv.inc.h>
};
#endif
#if defined(HAVE_SHADERC)
static char const * const g_sixel_vk_palette_glsl[] = {
#include <generated-vulkan/gpu-palette-vulkan-shader.inc.h>
    NULL
};
#endif
#if defined(HAVE_VULKAN_PREBUILT_SPIRV)
static unsigned char const g_sixel_vk_fhedt_spirv[] = {
#include <generated-vulkan/lookup-policy-fhedt-vulkan-spirv.inc.h>
};
#endif
#if defined(HAVE_SHADERC)
static char const * const g_sixel_vk_fhedt_glsl[] = {
#include <generated-vulkan/lookup-policy-fhedt-vulkan-shader.inc.h>
    NULL
};
#endif
#if defined(HAVE_VULKAN_PREBUILT_SPIRV)
static unsigned char const g_sixel_vk_bitlut_spirv[] = {
#include <generated-vulkan/lookup-policy-bitlut-vulkan-spirv.inc.h>
};
#endif
#if defined(HAVE_SHADERC)
static char const * const g_sixel_vk_bitlut_glsl[] = {
#include <generated-vulkan/lookup-policy-bitlut-vulkan-shader.inc.h>
    NULL
};
#endif

/* Must match the Params block in gpu-palette-vulkan.comp (std430). */
typedef struct sixel_gpu_vulkan_params {
    uint32_t pixel_count;
    uint32_t width;
    uint32_t height;
    uint32_t ncolors;
    uint32_t lookup_mode;
    uint32_t fhedt_resolution, fhedt_shift, fhedt_refine, fhedt_dist2;
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
} sixel_gpu_vulkan_range_t;

#define SIXEL_VK_LOCAL_SIZE 64U
#define SIXEL_VK_BINDINGS 12

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
    /* Two views of the FHEDT table, or the 5bit/6bit byte table in the
     * first; never host buffers of their own. */
    SIXEL_VK_FHEDT_TABLE8 = 10,
    SIXEL_VK_FHEDT_TABLE32 = 11
};

/* Byte lookup modes 6 and 7 are the 5bit and 6bit tables (bits + 1). */
#define SIXEL_VK_LOOKUP_BITLUT_BASE 1U

typedef struct sixel_gpu_vulkan_bitlut_push {
    uint32_t count;
    uint32_t pitch;
    uint32_t colors;
    uint32_t bits;
} sixel_gpu_vulkan_bitlut_push_t;

/* One build pipeline serves both widths through the push constant. The
 * table is cached by the palette and width it was built from and
 * published only after its submission completed. */
typedef struct sixel_gpu_vulkan_bitlut_state {
    int probed;
    int ready;
    VkDescriptorSetLayout set_layout;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSet set;
    sixel_vk_buffer_t table;
    int valid;
    int colors;
    int bits;
    unsigned char palette[SIXEL_PALETTE_MAX * 3];
} sixel_gpu_vulkan_bitlut_state_t;

/* Must match lookup-policy-fhedt-vulkan.comp. */
#define SIXEL_VK_FHEDT_KERNELS 5
#define SIXEL_VK_FHEDT_BINDINGS 5
#define SIXEL_VK_LOOKUP_FHEDT 5U

typedef struct sixel_gpu_vulkan_fhedt_push {
    uint32_t count;
    uint32_t pitch;
    uint32_t colors;
    uint32_t shift;
    uint32_t axis;
} sixel_gpu_vulkan_fhedt_push_t;

/* Build pipelines are specialized per resolution (64/128/256) and
 * refinement; the table is cached by the palette and settings it was built
 * from and published only after its submission completed. */
typedef struct sixel_gpu_vulkan_fhedt_state {
    int probed;
    int ready;
    VkShaderModule module;
    VkDescriptorSetLayout set_layout;
    VkPipelineLayout pipeline_layout;
    VkDescriptorPool descriptor_pool;
    /* [0] reads the first grid, [1] the second; see the pass table. */
    VkDescriptorSet sets[2];
    VkPipeline pipelines[3][2][SIXEL_VK_FHEDT_KERNELS];
    int attempted[3][2];
    sixel_vk_buffer_t first;
    sixel_vk_buffer_t second;
    sixel_vk_buffer_t table;
    int valid;
    int colors;
    int resolution;
    int refine;
    unsigned char palette[SIXEL_PALETTE_MAX * 3];
} sixel_gpu_vulkan_fhedt_state_t;

enum {
    SIXEL_VK_MODE_NONE = 0,
    SIXEL_VK_MODE_BLUENOISE = 1,
    SIXEL_VK_MODE_A_DITHER = 2,
    SIXEL_VK_MODE_X_DITHER = 3
};

typedef struct sixel_gpu_vulkan_palette_state {
    int probed;
    int ready;
    VkDescriptorSetLayout set_layout;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSet descriptor_set;
    sixel_vk_buffer_t buffers[SIXEL_VK_BINDINGS];
    sixel_vk_buffer_t dummy;
    /* Bytes each binding takes from its staging copy in this apply. */
    VkDeviceSize upload[SIXEL_VK_BINDINGS];
} sixel_gpu_vulkan_palette_state_t;

static sixel_gpu_vulkan_palette_state_t g_sixel_vk_palette;
static sixel_gpu_vulkan_fhedt_state_t g_sixel_vk_fhedt;
static sixel_gpu_vulkan_bitlut_state_t g_sixel_vk_bitlut;

static int
sixel_gpu_vulkan_palette_prepare_locked(sixel_vk_runtime_t *runtime)
{
    sixel_gpu_vulkan_palette_state_t *state;
    sixel_vk_shader_source_t source;
    VkDescriptorSetLayoutBinding bindings[SIXEL_VK_BINDINGS];
    VkDescriptorSetLayoutCreateInfo set_layout_info;
    VkPushConstantRange push_range;
    VkPipelineLayoutCreateInfo layout_info;
    VkComputePipelineCreateInfo pipeline_info;
    VkDescriptorPoolSize pool_size;
    VkDescriptorPoolCreateInfo pool_info;
    VkDescriptorSetAllocateInfo set_info;
    VkShaderModule module;
    double started;
    int index;

    state = &g_sixel_vk_palette;
    if (state->probed) {
        return state->ready;
    }
    state->probed = 1;
    module = VK_NULL_HANDLE;

    memset(&source, 0, sizeof(source));
    source.name = "gpu-palette-vulkan.comp";
#if defined(HAVE_VULKAN_PREBUILT_SPIRV)
    source.spirv = g_sixel_vk_palette_spirv;
    source.spirv_size = sizeof(g_sixel_vk_palette_spirv);
#endif
#if defined(HAVE_SHADERC)
    source.glsl_chunks = g_sixel_vk_palette_glsl;
#endif
    if (SIXEL_FAILED(sixel_vk_shader_module_create(runtime, &source,
            &module))) {
        goto fail;
    }

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
        goto fail;
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
            &state->pipeline_layout) != VK_SUCCESS) {
        goto fail;
    }

    started = sixel_vk_now_ms();
    memset(&pipeline_info, 0, sizeof(pipeline_info));
    pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeline_info.stage.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipeline_info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipeline_info.stage.module = module;
    pipeline_info.stage.pName = "main";
    pipeline_info.layout = state->pipeline_layout;
    if (vkCreateComputePipelines(runtime->device, VK_NULL_HANDLE, 1,
            &pipeline_info, NULL, &state->pipeline) != VK_SUCCESS) {
        state->pipeline = VK_NULL_HANDLE;
        goto fail;
    }
    if (runtime->debug) {
        fprintf(stderr, "libsixel: vulkan palette pipeline %.2f ms\n",
            sixel_vk_now_ms() - started);
    }
    vkDestroyShaderModule(runtime->device, module, NULL);
    module = VK_NULL_HANDLE;

    pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    pool_size.descriptorCount = SIXEL_VK_BINDINGS;
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

    /* The blue-noise table never changes; upload it once. */
    if (SIXEL_FAILED(sixel_vk_buffer_reserve(runtime,
            &state->buffers[SIXEL_VK_BLUE_NOISE], sizeof(sixel_bn64), 1)) ||
            SIXEL_FAILED(sixel_vk_buffer_reserve(runtime, &state->dummy, 4,
                0))) {
        goto fail;
    }
    memcpy(state->buffers[SIXEL_VK_BLUE_NOISE].mapped, sixel_bn64,
        sizeof(sixel_bn64));
    memset(state->dummy.mapped, 0, 4);

    state->ready = 1;
    return 1;

fail:
    if (module != VK_NULL_HANDLE) {
        vkDestroyShaderModule(runtime->device, module, NULL);
    }
    /* Objects stay allocated until process exit; the runtime is never torn
     * down while other components could still hold the device. */
    return 0;
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

/* AUTO resolves to the VP-tree on the CPU. These policies all return the
 * exact nearest entry with the lowest index on ties, so the direct scan
 * reproduces their indexes; approximate grids and trees are excluded. */
int
sixel_gpu_palette_vulkan_lut_is_exact(int lut_policy)
{
    return lut_policy == SIXEL_LUT_POLICY_AUTO ||
        lut_policy == SIXEL_LUT_POLICY_NONE ||
        lut_policy == SIXEL_LUT_POLICY_VPTREE ||
        lut_policy == SIXEL_LUT_POLICY_DELAUNAY ||
        lut_policy == SIXEL_LUT_POLICY_BLOCKCULL ||
        lut_policy == SIXEL_LUT_POLICY_ORDERED ||
        lut_policy == SIXEL_LUT_POLICY_GRIDLUT;
}

int
sixel_gpu_palette_vulkan_supports_lut(int lut_policy)
{
    return sixel_gpu_palette_vulkan_lut_is_exact(lut_policy) ||
        lut_policy == SIXEL_LUT_POLICY_FHEDT ||
        lut_policy == SIXEL_LUT_POLICY_5BIT ||
        lut_policy == SIXEL_LUT_POLICY_6BIT;
}

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

/* Byte queries at R256 are exact lattice points. Their stored label is
 * already nearest, and corner refinement visits that label first with a
 * strict distance comparison. Use the byte table without changing ties. */
static int
sixel_gpu_vulkan_fhedt_refines(sixel_gpu_palette_request_t const *request)
{
    return request->fhedt_refine && request->fhedt_resolution != 256;
}

static int
sixel_gpu_vulkan_fhedt_prepare_locked(sixel_vk_runtime_t *runtime)
{
    sixel_gpu_vulkan_fhedt_state_t *state;
    sixel_vk_shader_source_t source;
    VkDescriptorSetLayoutBinding bindings[SIXEL_VK_FHEDT_BINDINGS];
    VkDescriptorSetLayoutCreateInfo set_layout_info;
    VkPushConstantRange push_range;
    VkPipelineLayoutCreateInfo layout_info;
    VkDescriptorPoolSize pool_size;
    VkDescriptorPoolCreateInfo pool_info;
    VkDescriptorSetLayout layouts[2];
    VkDescriptorSetAllocateInfo set_info;
    int index;

    state = &g_sixel_vk_fhedt;
    if (state->probed) {
        return state->ready;
    }
    state->probed = 1;
    memset(&source, 0, sizeof(source));
    source.name = "lookup-policy-fhedt-vulkan.comp";
#if defined(HAVE_VULKAN_PREBUILT_SPIRV)
    source.spirv = g_sixel_vk_fhedt_spirv;
    source.spirv_size = sizeof(g_sixel_vk_fhedt_spirv);
#endif
#if defined(HAVE_SHADERC)
    source.glsl_chunks = g_sixel_vk_fhedt_glsl;
#endif
    if (SIXEL_FAILED(sixel_vk_shader_module_create(runtime, &source,
            &state->module))) {
        state->module = VK_NULL_HANDLE;
        return 0;
    }
    memset(bindings, 0, sizeof(bindings));
    for (index = 0; index < SIXEL_VK_FHEDT_BINDINGS; ++index) {
        bindings[index].binding = (uint32_t)index;
        bindings[index].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[index].descriptorCount = 1;
        bindings[index].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }
    memset(&set_layout_info, 0, sizeof(set_layout_info));
    set_layout_info.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    set_layout_info.bindingCount = SIXEL_VK_FHEDT_BINDINGS;
    set_layout_info.pBindings = bindings;
    if (vkCreateDescriptorSetLayout(runtime->device, &set_layout_info, NULL,
            &state->set_layout) != VK_SUCCESS) {
        return 0;
    }
    push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    push_range.offset = 0;
    push_range.size = sizeof(sixel_gpu_vulkan_fhedt_push_t);
    memset(&layout_info, 0, sizeof(layout_info));
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount = 1;
    layout_info.pSetLayouts = &state->set_layout;
    layout_info.pushConstantRangeCount = 1;
    layout_info.pPushConstantRanges = &push_range;
    if (vkCreatePipelineLayout(runtime->device, &layout_info, NULL,
            &state->pipeline_layout) != VK_SUCCESS) {
        return 0;
    }
    pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    pool_size.descriptorCount = 2 * SIXEL_VK_FHEDT_BINDINGS;
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
    layouts[1] = state->set_layout;
    memset(&set_info, 0, sizeof(set_info));
    set_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    set_info.descriptorPool = state->descriptor_pool;
    set_info.descriptorSetCount = 2;
    set_info.pSetLayouts = layouts;
    if (vkAllocateDescriptorSets(runtime->device, &set_info, state->sets)
            != VK_SUCCESS) {
        return 0;
    }
    state->ready = 1;
    return 1;
}

/* Specialize the five build kernels for one resolution and refinement. */
static int
sixel_gpu_vulkan_fhedt_pipelines_locked(sixel_vk_runtime_t *runtime,
    int resolution, int refine)
{
    sixel_gpu_vulkan_fhedt_state_t *state;
    VkSpecializationMapEntry entries[3];
    VkSpecializationInfo specialization;
    VkComputePipelineCreateInfo pipeline_info;
    int32_t constants[3];
    int slot, kernel, ready;
    double started;

    state = &g_sixel_vk_fhedt;
    slot = resolution == 64 ? 0 : resolution == 128 ? 1 : 2;
    if (state->attempted[slot][refine]) {
        return state->pipelines[slot][refine][0] != VK_NULL_HANDLE;
    }
    state->attempted[slot][refine] = 1;
    for (kernel = 0; kernel < 3; ++kernel) {
        entries[kernel].constantID = (uint32_t)kernel;
        entries[kernel].offset = (uint32_t)(kernel * sizeof(int32_t));
        entries[kernel].size = sizeof(int32_t);
    }
    memset(&specialization, 0, sizeof(specialization));
    specialization.mapEntryCount = 3;
    specialization.pMapEntries = entries;
    specialization.dataSize = sizeof(constants);
    specialization.pData = constants;
    memset(&pipeline_info, 0, sizeof(pipeline_info));
    pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeline_info.stage.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipeline_info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipeline_info.stage.module = state->module;
    pipeline_info.stage.pName = "main";
    pipeline_info.stage.pSpecializationInfo = &specialization;
    pipeline_info.layout = state->pipeline_layout;
    constants[1] = resolution;
    constants[2] = refine;
    started = sixel_vk_now_ms();
    ready = 1;
    for (kernel = 0; kernel < SIXEL_VK_FHEDT_KERNELS; ++kernel) {
        constants[0] = kernel;
        if (vkCreateComputePipelines(runtime->device, VK_NULL_HANDLE, 1,
                &pipeline_info, NULL, &state->pipelines[slot][refine][kernel])
                != VK_SUCCESS) {
            state->pipelines[slot][refine][kernel] = VK_NULL_HANDLE;
            ready = 0;
        }
    }
    if (!ready) {
        for (kernel = 0; kernel < SIXEL_VK_FHEDT_KERNELS; ++kernel) {
            if (state->pipelines[slot][refine][kernel] != VK_NULL_HANDLE) {
                vkDestroyPipeline(runtime->device,
                    state->pipelines[slot][refine][kernel], NULL);
                state->pipelines[slot][refine][kernel] = VK_NULL_HANDLE;
            }
        }
    }
    if (runtime->debug) {
        fprintf(stderr, "libsixel: vulkan FHEDT R%d%s pipelines %s "
            "(%.2f ms)\n", resolution, refine ? " refined" : "",
            ready ? "ready" : "failed", sixel_vk_now_ms() - started);
    }
    return ready;
}

/* Reserve build storage and point both descriptor sets at it. Set 0 reads
 * the first grid and writes the second; set 1 is the reverse. */
static SIXELSTATUS
sixel_gpu_vulkan_fhedt_bind_locked(sixel_vk_runtime_t *runtime,
    sixel_vk_buffer_t const *palette, int resolution, int refine,
    int rebuild)
{
    sixel_gpu_vulkan_fhedt_state_t *state;
    VkDescriptorBufferInfo infos[2][SIXEL_VK_FHEDT_BINDINGS];
    VkWriteDescriptorSet writes[2 * SIXEL_VK_FHEDT_BINDINGS];
    VkDeviceSize cells;
    SIXELSTATUS status;
    int set, index;

    state = &g_sixel_vk_fhedt;
    cells = (VkDeviceSize)resolution * resolution * resolution;
    status = sixel_vk_buffer_reserve_device(runtime, &state->table,
        cells * (refine ? 4U : 1U));
    if (SIXEL_SUCCEEDED(status) && rebuild) {
        /* Grid cells plus R*R column heads and one link per seed. */
        status = sixel_vk_buffer_reserve_device(runtime, &state->first,
            cells * 4U + SIXEL_PALETTE_MAX * 4U);
    }
    if (SIXEL_SUCCEEDED(status) && rebuild) {
        status = sixel_vk_buffer_reserve_device(runtime, &state->second,
            cells * 4U);
    }
    if (SIXEL_FAILED(status) || !rebuild) {
        return status;
    }
    memset(writes, 0, sizeof(writes));
    for (set = 0; set < 2; ++set) {
        infos[set][0].buffer = set == 0 ? state->first.buffer
            : state->second.buffer;
        infos[set][1].buffer = set == 0 ? state->second.buffer
            : state->first.buffer;
        infos[set][2].buffer = palette->buffer;
        infos[set][3].buffer = state->table.buffer;
        infos[set][4].buffer = state->table.buffer;
        for (index = 0; index < SIXEL_VK_FHEDT_BINDINGS; ++index) {
            infos[set][index].offset = 0;
            infos[set][index].range = VK_WHOLE_SIZE;
            writes[set * SIXEL_VK_FHEDT_BINDINGS + index].sType =
                VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[set * SIXEL_VK_FHEDT_BINDINGS + index].dstSet =
                state->sets[set];
            writes[set * SIXEL_VK_FHEDT_BINDINGS + index].dstBinding =
                (uint32_t)index;
            writes[set * SIXEL_VK_FHEDT_BINDINGS + index].descriptorCount = 1;
            writes[set * SIXEL_VK_FHEDT_BINDINGS + index].descriptorType =
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writes[set * SIXEL_VK_FHEDT_BINDINGS + index].pBufferInfo =
                &infos[set][index];
        }
    }
    vkUpdateDescriptorSets(runtime->device, 2 * SIXEL_VK_FHEDT_BINDINGS,
        writes, 0, NULL);
    return SIXEL_OK;
}

/* Record the Metal pass sequence: clear heads, seed column lists, X scan,
 * Y and Z distance transforms, then boundary packing when refining. Each
 * pass reads what the previous one wrote. */
static SIXELSTATUS
sixel_gpu_vulkan_fhedt_record(sixel_vk_runtime_t *runtime,
    VkCommandBuffer command, int resolution, int refine, int colors)
{
    sixel_gpu_vulkan_fhedt_state_t *state;
    sixel_gpu_vulkan_fhedt_push_t push;
    uint64_t size, count;
    uint32_t groups_x, groups_y;
    int pass, slot, kernel;
    SIXELSTATUS status;

    state = &g_sixel_vk_fhedt;
    size = (uint64_t)resolution;
    slot = resolution == 64 ? 0 : resolution == 128 ? 1 : 2;
    memset(&push, 0, sizeof(push));
    push.colors = (uint32_t)colors;
    push.shift = resolution == 64 ? 2U : resolution == 128 ? 1U : 0U;
    for (pass = 0; pass < (refine ? 6 : 5); ++pass) {
        kernel = pass < 3 ? pass : pass < 5 ? 3 : 4;
        push.axis = pass >= 2 && pass < 5 ? (uint32_t)(pass - 2) : 0U;
        count = pass == 1 ? (uint64_t)colors :
            pass == 0 || pass == 3 || pass == 4 ? size * size :
            size * size * size;
        status = sixel_gpu_vulkan_groups(runtime, count, &groups_x,
            &groups_y);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        push.count = (uint32_t)count;
        push.pitch = groups_x * SIXEL_VK_LOCAL_SIZE;
        vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_COMPUTE,
            state->pipelines[slot][refine][kernel]);
        vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_COMPUTE,
            state->pipeline_layout, 0, 1,
            &state->sets[pass == 3 || pass == 5 ? 1 : 0], 0, NULL);
        vkCmdPushConstants(command, state->pipeline_layout,
            VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), &push);
        vkCmdDispatch(command, groups_x, groups_y, 1);
        sixel_vk_record_barrier(command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
    }
    return SIXEL_OK;
}

/* Shared FHEDT table for callers that own their own submission (palette
 * apply and the diffusion plan). Under sixel_vk_lock(): when the cached
 * table does not match this palette and these settings, invalidate it,
 * point the build at the device palette buffer and report a rebuild. The
 * caller records the build after the palette upload and publishes the
 * table only after the submission completed. dist2 only changes queries,
 * so it is not part of the table key. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_gpu_palette_vulkan_fhedt_acquire_locked(sixel_vk_runtime_t *runtime,
    sixel_vk_buffer_t const *palette_buffer, unsigned char const *palette,
    int ncolors, int resolution, int refine,
    sixel_vk_buffer_t const **table, int *rebuild)
{
    if (runtime == NULL || palette_buffer == NULL || palette == NULL ||
            table == NULL || rebuild == NULL || ncolors < 1 ||
            ncolors > SIXEL_PALETTE_MAX ||
            (resolution != 64 && resolution != 128 && resolution != 256)) {
        return SIXEL_BAD_ARGUMENT;
    }
    *table = NULL;
    *rebuild = 0;
    refine = refine != 0;
    if (!sixel_gpu_vulkan_fhedt_prepare_locked(runtime) ||
            !sixel_gpu_vulkan_fhedt_pipelines_locked(runtime, resolution,
                refine)) {
        return SIXEL_FEATURE_ERROR;
    }
    *rebuild = !g_sixel_vk_fhedt.valid ||
        g_sixel_vk_fhedt.colors != ncolors ||
        g_sixel_vk_fhedt.resolution != resolution ||
        g_sixel_vk_fhedt.refine != refine ||
        memcmp(g_sixel_vk_fhedt.palette, palette, (size_t)ncolors * 3U) != 0;
    /* A failed rebuild leaves a partially written table behind. */
    if (*rebuild) {
        g_sixel_vk_fhedt.valid = 0;
    }
    *table = &g_sixel_vk_fhedt.table;
    return sixel_gpu_vulkan_fhedt_bind_locked(runtime, palette_buffer,
        resolution, refine, *rebuild);
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_gpu_palette_vulkan_fhedt_record_locked(sixel_vk_runtime_t *runtime,
    VkCommandBuffer command, int resolution, int refine, int ncolors)
{
    return sixel_gpu_vulkan_fhedt_record(runtime, command, resolution,
        refine != 0, ncolors);
}

SIXEL_INTERNAL_API void
sixel_gpu_palette_vulkan_fhedt_publish_locked(unsigned char const *palette,
    int ncolors, int resolution, int refine)
{
    memcpy(g_sixel_vk_fhedt.palette, palette, (size_t)ncolors * 3U);
    g_sixel_vk_fhedt.colors = ncolors;
    g_sixel_vk_fhedt.resolution = resolution;
    g_sixel_vk_fhedt.refine = refine != 0;
    g_sixel_vk_fhedt.valid = 1;
}

static int
sixel_gpu_vulkan_bitlut_prepare_locked(sixel_vk_runtime_t *runtime)
{
    sixel_gpu_vulkan_bitlut_state_t *state;
    sixel_vk_shader_source_t source;
    VkShaderModule module;
    VkDescriptorSetLayoutBinding bindings[2];
    VkDescriptorSetLayoutCreateInfo set_layout_info;
    VkPushConstantRange push_range;
    VkPipelineLayoutCreateInfo layout_info;
    VkDescriptorPoolSize pool_size;
    VkDescriptorPoolCreateInfo pool_info;
    VkDescriptorSetAllocateInfo set_info;
    VkComputePipelineCreateInfo pipeline_info;
    int index, ready;

    state = &g_sixel_vk_bitlut;
    if (state->probed) {
        return state->ready;
    }
    state->probed = 1;
    module = VK_NULL_HANDLE;
    ready = 0;
    memset(&source, 0, sizeof(source));
    source.name = "lookup-policy-bitlut-vulkan.comp";
#if defined(HAVE_VULKAN_PREBUILT_SPIRV)
    source.spirv = g_sixel_vk_bitlut_spirv;
    source.spirv_size = sizeof(g_sixel_vk_bitlut_spirv);
#endif
#if defined(HAVE_SHADERC)
    source.glsl_chunks = g_sixel_vk_bitlut_glsl;
#endif
    if (SIXEL_FAILED(sixel_vk_shader_module_create(runtime, &source,
            &module))) {
        return 0;
    }
    memset(bindings, 0, sizeof(bindings));
    for (index = 0; index < 2; ++index) {
        bindings[index].binding = (uint32_t)index;
        bindings[index].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[index].descriptorCount = 1;
        bindings[index].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }
    memset(&set_layout_info, 0, sizeof(set_layout_info));
    set_layout_info.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    set_layout_info.bindingCount = 2;
    set_layout_info.pBindings = bindings;
    if (vkCreateDescriptorSetLayout(runtime->device, &set_layout_info, NULL,
            &state->set_layout) != VK_SUCCESS) {
        goto end;
    }
    push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    push_range.offset = 0;
    push_range.size = sizeof(sixel_gpu_vulkan_bitlut_push_t);
    memset(&layout_info, 0, sizeof(layout_info));
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount = 1;
    layout_info.pSetLayouts = &state->set_layout;
    layout_info.pushConstantRangeCount = 1;
    layout_info.pPushConstantRanges = &push_range;
    if (vkCreatePipelineLayout(runtime->device, &layout_info, NULL,
            &state->pipeline_layout) != VK_SUCCESS) {
        goto end;
    }
    pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    pool_size.descriptorCount = 2;
    memset(&pool_info, 0, sizeof(pool_info));
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.maxSets = 1;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = &pool_size;
    if (vkCreateDescriptorPool(runtime->device, &pool_info, NULL,
            &state->descriptor_pool) != VK_SUCCESS) {
        goto end;
    }
    memset(&set_info, 0, sizeof(set_info));
    set_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    set_info.descriptorPool = state->descriptor_pool;
    set_info.descriptorSetCount = 1;
    set_info.pSetLayouts = &state->set_layout;
    if (vkAllocateDescriptorSets(runtime->device, &set_info, &state->set)
            != VK_SUCCESS) {
        goto end;
    }
    memset(&pipeline_info, 0, sizeof(pipeline_info));
    pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeline_info.stage.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipeline_info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipeline_info.stage.module = module;
    pipeline_info.stage.pName = "main";
    pipeline_info.layout = state->pipeline_layout;
    if (vkCreateComputePipelines(runtime->device, VK_NULL_HANDLE, 1,
            &pipeline_info, NULL, &state->pipeline) != VK_SUCCESS) {
        state->pipeline = VK_NULL_HANDLE;
        goto end;
    }
    ready = 1;
    state->ready = 1;
end:
    vkDestroyShaderModule(runtime->device, module, NULL);
    return ready;
}

/* The shared 5bit/6bit table, like the FHEDT one: under sixel_vk_lock(),
 * invalidate it and point the build at the device palette when the cached
 * table does not match, and report a rebuild. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_gpu_palette_vulkan_bitlut_acquire_locked(sixel_vk_runtime_t *runtime,
    sixel_vk_buffer_t const *palette_buffer, unsigned char const *palette,
    int ncolors, int bits, sixel_vk_buffer_t const **table, int *rebuild)
{
    sixel_gpu_vulkan_bitlut_state_t *state;
    VkDescriptorBufferInfo infos[2];
    VkWriteDescriptorSet writes[2];
    SIXELSTATUS status;
    int index;

    if (runtime == NULL || palette_buffer == NULL || palette == NULL ||
            table == NULL || rebuild == NULL || ncolors < 1 ||
            ncolors > SIXEL_PALETTE_MAX || (bits != 5 && bits != 6)) {
        return SIXEL_BAD_ARGUMENT;
    }
    state = &g_sixel_vk_bitlut;
    *table = NULL;
    *rebuild = 0;
    if (!sixel_gpu_vulkan_bitlut_prepare_locked(runtime)) {
        return SIXEL_FEATURE_ERROR;
    }
    *rebuild = !state->valid || state->colors != ncolors ||
        state->bits != bits ||
        memcmp(state->palette, palette, (size_t)ncolors * 3U) != 0;
    if (!*rebuild) {
        *table = &state->table;
        return SIXEL_OK;
    }
    /* A failed rebuild leaves a partially written table behind. */
    state->valid = 0;
    status = sixel_vk_buffer_reserve_device(runtime, &state->table,
        (VkDeviceSize)1U << (bits * 3));
    if (SIXEL_FAILED(status)) {
        return status;
    }
    memset(infos, 0, sizeof(infos));
    memset(writes, 0, sizeof(writes));
    for (index = 0; index < 2; ++index) {
        infos[index].buffer = index == 0 ? palette_buffer->buffer
            : state->table.buffer;
        infos[index].offset = 0;
        infos[index].range = VK_WHOLE_SIZE;
        writes[index].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[index].dstSet = state->set;
        writes[index].dstBinding = (uint32_t)index;
        writes[index].descriptorCount = 1;
        writes[index].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[index].pBufferInfo = &infos[index];
    }
    vkUpdateDescriptorSets(runtime->device, 2, writes, 0, NULL);
    *table = &state->table;
    return SIXEL_OK;
}

/* One cell per invocation; the trailing barrier orders the table writes
 * before any lookup that follows in the same command buffer. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_gpu_palette_vulkan_bitlut_record_locked(sixel_vk_runtime_t *runtime,
    VkCommandBuffer command, int bits, int ncolors)
{
    sixel_gpu_vulkan_bitlut_state_t *state;
    sixel_gpu_vulkan_bitlut_push_t push;
    uint32_t groups_x, groups_y;
    SIXELSTATUS status;

    state = &g_sixel_vk_bitlut;
    push.count = 1U << (bits * 3);
    status = sixel_gpu_vulkan_groups(runtime, push.count, &groups_x,
        &groups_y);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    push.pitch = groups_x * SIXEL_VK_LOCAL_SIZE;
    push.colors = (uint32_t)ncolors;
    push.bits = (uint32_t)bits;
    vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_COMPUTE,
        state->pipeline);
    vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_COMPUTE,
        state->pipeline_layout, 0, 1, &state->set, 0, NULL);
    vkCmdPushConstants(command, state->pipeline_layout,
        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), &push);
    vkCmdDispatch(command, groups_x, groups_y, 1);
    sixel_vk_record_barrier(command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
    return SIXEL_OK;
}

SIXEL_INTERNAL_API void
sixel_gpu_palette_vulkan_bitlut_publish_locked(unsigned char const *palette,
    int ncolors, int bits)
{
    memcpy(g_sixel_vk_bitlut.palette, palette, (size_t)ncolors * 3U);
    g_sixel_vk_bitlut.colors = ncolors;
    g_sixel_vk_bitlut.bits = bits;
    g_sixel_vk_bitlut.valid = 1;
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
    params->lookup_mode = 0;
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

SIXELSTATUS
sixel_gpu_palette_vulkan_apply(sixel_gpu_palette_request_t const *request)
{
    sixel_gpu_vulkan_palette_state_t *state;
    sixel_vk_runtime_t *runtime;
    sixel_gpu_vulkan_params_t params;
    sixel_gpu_vulkan_range_t range;
    sixel_vk_buffer_t const *bound[SIXEL_VK_BINDINGS];
    VkDescriptorBufferInfo infos[SIXEL_VK_BINDINGS];
    VkWriteDescriptorSet writes[SIXEL_VK_BINDINGS];
    VkCommandBuffer command;
    uint32_t groups_x;
    uint32_t groups_y;
    size_t pixel_count;
    double started;
    double staged;
    double waited;
    int has_accumulation;
    int has_valid;
    int fhedt;
    int fhedt_refine;
    int bitlut_bits;
    int rebuild;
    int index;
    sixel_vk_buffer_t const *bitlut_table;
    SIXELSTATUS status;

    if (request == NULL || request->program != NULL ||
            !sixel_gpu_palette_vulkan_supports_lut(request->lut_policy) ||
            request->retained_rgb != NULL) {
        return SIXEL_FEATURE_ERROR;
    }
    fhedt = request->lut_policy == SIXEL_LUT_POLICY_FHEDT;
    if (fhedt && !sixel_gpu_palette_fhedt_is_supported(request)) {
        return SIXEL_FEATURE_ERROR;
    }
    fhedt_refine = fhedt && sixel_gpu_vulkan_fhedt_refines(request);
    bitlut_bits = request->lut_policy == SIXEL_LUT_POLICY_5BIT ? 5
        : request->lut_policy == SIXEL_LUT_POLICY_6BIT ? 6 : 0;
    bitlut_table = NULL;
    rebuild = 0;
    pixel_count = request->pixel_count;
    command = VK_NULL_HANDLE;
    state = &g_sixel_vk_palette;

    sixel_vk_lock();
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
        request->test_hooks->kernel = bitlut_bits != 0 ? 8 : 1;
        request->test_hooks->rebuild = 0;
        request->test_hooks->published = 0;
    }

    sixel_gpu_vulkan_fill_params(&params, request);
    if (fhedt) {
        if (!sixel_gpu_vulkan_fhedt_prepare_locked(runtime) ||
                !sixel_gpu_vulkan_fhedt_pipelines_locked(runtime,
                    request->fhedt_resolution, fhedt_refine)) {
            sixel_helper_set_additional_message(
                "gpu palette apply: Vulkan FHEDT pipelines unavailable.");
            status = SIXEL_FEATURE_ERROR;
            goto end;
        }
        params.lookup_mode = SIXEL_VK_LOOKUP_FHEDT;
        params.fhedt_resolution = (uint32_t)request->fhedt_resolution;
        params.fhedt_shift = request->fhedt_resolution == 64 ? 2U :
            request->fhedt_resolution == 128 ? 1U : 0U;
        params.fhedt_refine = (uint32_t)fhedt_refine;
        params.fhedt_dist2 = (uint32_t)request->fhedt_use_dist2;
        if (request->test_hooks != NULL) {
            request->test_hooks->fhedt_table_entry_bytes =
                fhedt_refine ? 4U : 1U;
        }
        /* dist2 only changes queries, so it is not part of the table key. */
        rebuild = !g_sixel_vk_fhedt.valid ||
            g_sixel_vk_fhedt.colors != request->ncolors ||
            g_sixel_vk_fhedt.resolution != request->fhedt_resolution ||
            g_sixel_vk_fhedt.refine != fhedt_refine ||
            memcmp(g_sixel_vk_fhedt.palette, request->palette,
                (size_t)request->ncolors * 3U) != 0;
        /* A failed rebuild leaves a partially written table behind. */
        if (rebuild) {
            g_sixel_vk_fhedt.valid = 0;
        }
    }
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
            request->palette, (size_t)request->ncolors * 3U);
    }
    if (SIXEL_SUCCEEDED(status) && fhedt) {
        status = sixel_gpu_vulkan_fhedt_bind_locked(runtime,
            &state->buffers[SIXEL_VK_PALETTE], request->fhedt_resolution,
            fhedt_refine, rebuild);
    }
    if (SIXEL_SUCCEEDED(status) && bitlut_bits != 0) {
        status = sixel_gpu_palette_vulkan_bitlut_acquire_locked(runtime,
            &state->buffers[SIXEL_VK_PALETTE], request->palette,
            request->ncolors, bitlut_bits, &bitlut_table, &rebuild);
        if (SIXEL_SUCCEEDED(status)) {
            params.lookup_mode = (uint32_t)bitlut_bits +
                SIXEL_VK_LOOKUP_BITLUT_BASE;
        }
        if (request->test_hooks != NULL) {
            request->test_hooks->bitlut_table_bytes =
                (size_t)1U << (bitlut_bits * 3);
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
    if (request->test_hooks != NULL) {
        request->test_hooks->rebuild = rebuild;
        if (SIXEL_SUCCEEDED(status) && request->test_hooks->fault == 1) {
            status = SIXEL_BAD_ALLOCATION;
        }
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
    bound[SIXEL_VK_FHEDT_TABLE8] = fhedt ? &g_sixel_vk_fhedt.table
        : bitlut_table != NULL ? bitlut_table : &state->dummy;
    bound[SIXEL_VK_FHEDT_TABLE32] = bound[SIXEL_VK_FHEDT_TABLE8];
    memset(writes, 0, sizeof(writes));
    for (index = 0; index < SIXEL_VK_BINDINGS; ++index) {
        infos[index].buffer = bound[index]->buffer;
        infos[index].offset = 0;
        infos[index].range = VK_WHOLE_SIZE;
        writes[index].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[index].dstSet = state->descriptor_set;
        writes[index].dstBinding = (uint32_t)index;
        writes[index].descriptorCount = 1;
        writes[index].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[index].pBufferInfo = &infos[index];
    }
    vkUpdateDescriptorSets(runtime->device, SIXEL_VK_BINDINGS, writes, 0,
        NULL);

    status = sixel_gpu_vulkan_groups(runtime, (uint64_t)pixel_count,
        &groups_x, &groups_y);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    range.first = 0;
    range.count = (uint32_t)pixel_count;
    range.pitch = groups_x * SIXEL_VK_LOCAL_SIZE;

    staged = sixel_vk_now_ms();
    status = sixel_vk_command_begin(runtime, &command);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    for (index = 0; index < SIXEL_VK_BINDINGS; ++index) {
        if (bound[index] == &state->dummy) {
            sixel_vk_buffer_record_upload(command, &state->dummy, 4);
        } else {
            sixel_vk_buffer_record_upload(command, bound[index],
                state->upload[index]);
        }
    }
    sixel_vk_record_barrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
    if (rebuild) {
        /* The build reads the palette just uploaded; its last barrier
         * orders the table writes before the apply dispatch reads them. */
        status = bitlut_bits != 0
            ? sixel_gpu_palette_vulkan_bitlut_record_locked(runtime, command,
                bitlut_bits, request->ncolors)
            : sixel_gpu_vulkan_fhedt_record(runtime, command,
                request->fhedt_resolution, fhedt_refine, request->ncolors);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }
    vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_COMPUTE,
        state->pipeline);
    vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_COMPUTE,
        state->pipeline_layout, 0, 1, &state->descriptor_set, 0, NULL);
    vkCmdPushConstants(command, state->pipeline_layout,
        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(range), &range);
    vkCmdDispatch(command, groups_x, groups_y, 1);
    sixel_vk_record_barrier(command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_READ_BIT);
    sixel_vk_buffer_record_download(command,
        &state->buffers[SIXEL_VK_RESULT], pixel_count);
    if (request->accumulation_result_mask != NULL) {
        sixel_vk_buffer_record_download(command,
            &state->buffers[SIXEL_VK_ACCUMULATION_RESULT],
            request->accumulation_result_mask_size);
    }
    /* Make shader and copy writes visible to the mapped host memory. */
    sixel_vk_record_barrier(command,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_HOST_READ_BIT);
    /* Test faults 2 and 3 model a failed submission and a failed
     * completion; neither publishes a rebuilt table. */
    if (request->test_hooks != NULL && request->test_hooks->fault == 2) {
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }
    status = sixel_vk_submit_and_wait(runtime, command);
    if (SIXEL_SUCCEEDED(status) && request->test_hooks != NULL &&
            request->test_hooks->fault == 3) {
        status = SIXEL_RUNTIME_ERROR;
    }
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    waited = sixel_vk_now_ms();
    if (rebuild && bitlut_bits != 0) {
        sixel_gpu_palette_vulkan_bitlut_publish_locked(request->palette,
            request->ncolors, bitlut_bits);
    } else if (rebuild) {
        memcpy(g_sixel_vk_fhedt.palette, request->palette,
            (size_t)request->ncolors * 3U);
        g_sixel_vk_fhedt.colors = request->ncolors;
        g_sixel_vk_fhedt.resolution = request->fhedt_resolution;
        g_sixel_vk_fhedt.refine = fhedt_refine;
        g_sixel_vk_fhedt.valid = 1;
    }

    if (request->cancel != NULL && request->cancel(request->cancel_context)) {
        status = SIXEL_INTERRUPTED;
        goto end;
    }
    memcpy(request->dest, state->buffers[SIXEL_VK_RESULT].mapped,
        pixel_count);
    if (request->accumulation_result_mask != NULL) {
        memcpy(request->accumulation_result_mask,
            state->buffers[SIXEL_VK_ACCUMULATION_RESULT].mapped,
            request->accumulation_result_mask_size);
    }
    if (request->test_hooks != NULL) {
        request->test_hooks->published = 1;
        request->test_hooks->submitted_chunks = 1;
        request->test_hooks->completed_chunks = 1;
        request->test_hooks->peak_inflight = 1;
    }
    if (runtime->debug) {
        /* stage: host copies into mapped memory; submit: record, submit
         * and fence wait (gpu: device time inside it); read: result copy
         * out of mapped memory. */
        fprintf(stderr, "libsixel: vulkan palette apply %ux%u, %d colors, "
            "%s: %.2f ms (stage %.2f, submit %.2f, gpu %.2f, read %.2f)\n",
            (unsigned int)request->width, (unsigned int)request->height,
            request->ncolors, bitlut_bits != 0
                ? (rebuild ? "bitlut build" : "bitlut reuse")
                : !fhedt ? "direct" :
                rebuild ? "fhedt build" : "fhedt reuse",
            sixel_vk_now_ms() - started,
            staged - started, waited - staged, runtime->last_busy_ms,
            sixel_vk_now_ms() - waited);
    }
    status = SIXEL_OK;

end:
    if (command != VK_NULL_HANDLE) {
        sixel_vk_command_free(runtime, command);
    }
    sixel_vk_unlock();
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
