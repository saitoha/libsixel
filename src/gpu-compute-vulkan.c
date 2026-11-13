/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#if defined(HAVE_VULKAN)
#include <stdio.h>
#include <string.h>
#include "gpu-compute-vulkan.h"

void
sixel_vk_compute_destroy(sixel_vk_runtime_t *runtime,
    sixel_vk_compute_t *stage)
{
    vkDestroyPipeline(runtime->device, stage->pipeline, NULL);
    vkDestroyDescriptorPool(runtime->device, stage->pool, NULL);
    vkDestroyPipelineLayout(runtime->device, stage->layout, NULL);
    vkDestroyDescriptorSetLayout(runtime->device, stage->set_layout, NULL);
    memset(stage, 0, sizeof(*stage));
}

SIXELSTATUS
sixel_vk_compute_create(sixel_vk_runtime_t *runtime,
    sixel_vk_compute_t *stage, char const *const *chunks, size_t count,
    char const *entry, unsigned int bindings)
{
    VkPhysicalDeviceLimits const *limits;
    VkDescriptorSetLayoutBinding slots[SIXEL_VK_COMPUTE_BINDINGS];
    VkDescriptorSetLayoutCreateInfo set_info;
    VkPipelineLayoutCreateInfo layout_info;
    VkDescriptorPoolSize size;
    VkDescriptorPoolCreateInfo pool_info;
    VkDescriptorSetAllocateInfo allocate;
    VkComputePipelineCreateInfo pipeline;
    VkShaderModule module;
    sixel_vk_source_part_t parts[2];
    char suffix[128];
    char const *tail;
    unsigned int i;
    int length;
    SIXELSTATUS status;

    if (stage->pipeline != VK_NULL_HANDLE) return SIXEL_OK;
    if (bindings == 0 || bindings > SIXEL_VK_COMPUTE_BINDINGS)
        return SIXEL_BAD_ARGUMENT;
    limits = &runtime->properties.limits;
    /* Refuse unsupported descriptor layouts before creating Vulkan objects. */
    if (bindings > limits->maxPerStageDescriptorStorageBuffers
            || bindings > limits->maxDescriptorSetStorageBuffers)
        return SIXEL_FEATURE_ERROR;
    length = snprintf(suffix, sizeof(suffix),
        "\nvoid main()\n{\n    %s();\n}\n", entry);
    if (length < 0 || (size_t)length >= sizeof(suffix))
        return SIXEL_BAD_ARGUMENT;
    tail = suffix;
    parts[0].chunks = chunks;
    parts[0].count = count;
    parts[1].chunks = &tail;
    parts[1].count = 1;
    module = VK_NULL_HANDLE;
    status = sixel_vk_shader_module_compose(runtime, entry, parts, 2,
        &module);
    if (status != SIXEL_OK) return status;
    status = SIXEL_RUNTIME_ERROR;
    memset(slots, 0, sizeof(slots));
    for (i = 0; i < bindings; ++i) {
        slots[i].binding = i;
        slots[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        slots[i].descriptorCount = 1;
        slots[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }
    memset(&set_info, 0, sizeof(set_info));
    set_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    set_info.bindingCount = bindings;
    set_info.pBindings = slots;
    if (vkCreateDescriptorSetLayout(runtime->device, &set_info, NULL,
            &stage->set_layout) != VK_SUCCESS) goto end;
    memset(&layout_info, 0, sizeof(layout_info));
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount = 1;
    layout_info.pSetLayouts = &stage->set_layout;
    if (vkCreatePipelineLayout(runtime->device, &layout_info, NULL,
            &stage->layout) != VK_SUCCESS) goto end;
    size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    size.descriptorCount = bindings;
    memset(&pool_info, 0, sizeof(pool_info));
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.maxSets = 1;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = &size;
    if (vkCreateDescriptorPool(runtime->device, &pool_info, NULL,
            &stage->pool) != VK_SUCCESS) goto end;
    memset(&allocate, 0, sizeof(allocate));
    allocate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocate.descriptorPool = stage->pool;
    allocate.descriptorSetCount = 1;
    allocate.pSetLayouts = &stage->set_layout;
    if (vkAllocateDescriptorSets(runtime->device, &allocate, &stage->set)
            != VK_SUCCESS) goto end;
    memset(&pipeline, 0, sizeof(pipeline));
    pipeline.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeline.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipeline.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipeline.stage.module = module;
    pipeline.stage.pName = "main";
    pipeline.layout = stage->layout;
    if (vkCreateComputePipelines(runtime->device, VK_NULL_HANDLE, 1,
            &pipeline, NULL, &stage->pipeline) != VK_SUCCESS) goto end;
    stage->bindings = bindings;
    status = SIXEL_OK;
end:
    vkDestroyShaderModule(runtime->device, module, NULL);
    if (status != SIXEL_OK) sixel_vk_compute_destroy(runtime, stage);
    return status;
}

SIXELSTATUS
sixel_vk_compute_bind(sixel_vk_runtime_t *runtime, sixel_vk_compute_t *stage,
    sixel_vk_buffer_t const *const *buffers)
{
    VkDescriptorBufferInfo infos[SIXEL_VK_COMPUTE_BINDINGS];
    VkWriteDescriptorSet writes[SIXEL_VK_COMPUTE_BINDINGS];
    unsigned int i;

    memset(infos, 0, sizeof(infos));
    memset(writes, 0, sizeof(writes));
    for (i = 0; i < stage->bindings; ++i) {
        if (buffers[i]->capacity >
                runtime->properties.limits.maxStorageBufferRange)
            return SIXEL_FEATURE_ERROR;
        infos[i].buffer = buffers[i]->buffer;
        infos[i].range = VK_WHOLE_SIZE;
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = stage->set;
        writes[i].dstBinding = i;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[i].pBufferInfo = &infos[i];
    }
    vkUpdateDescriptorSets(runtime->device, stage->bindings, writes, 0, NULL);
    return SIXEL_OK;
}

void
sixel_vk_compute_barrier(VkCommandBuffer command)
{
    sixel_vk_record_barrier(command,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_ACCESS_MEMORY_WRITE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
}

void
sixel_vk_compute_download_barrier(VkCommandBuffer command)
{
    sixel_vk_record_barrier(command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT |
        VK_PIPELINE_STAGE_HOST_BIT,
        VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_HOST_READ_BIT);
}

void
sixel_vk_compute_dispatch(VkCommandBuffer command,
    sixel_vk_compute_t const *stage, uint32_t groups)
{
    vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_COMPUTE,
        stage->pipeline);
    vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_COMPUTE,
        stage->layout, 0, 1, &stage->set, 0, NULL);
    vkCmdDispatch(command, groups > 65535u ? 65535u : groups,
        (groups + 65534u) / 65535u, 1);
}
#endif
