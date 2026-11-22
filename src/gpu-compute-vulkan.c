/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#if defined(HAVE_VULKAN)
#include <stdio.h>
#include <string.h>
#include "compat_stub.h"
#include "gpu-compute-vulkan.h"

int
sixel_vk_submission_init(sixel_vk_submission_t *stage)
{
    return pthread_mutex_init(&stage->gate, NULL) == 0;
}

void
sixel_vk_submission_enter(sixel_vk_submission_t *stage)
{
    (void)pthread_mutex_lock(&stage->gate);
}

void
sixel_vk_submission_leave(sixel_vk_submission_t *stage)
{
    (void)pthread_mutex_unlock(&stage->gate);
}

void
sixel_vk_submission_destroy(sixel_vk_runtime_t *runtime,
    sixel_vk_submission_t *stage)
{
    vkDestroyQueryPool(runtime->device, stage->timestamps, NULL);
    vkDestroyFence(runtime->device, stage->fence, NULL);
    vkDestroyCommandPool(runtime->device, stage->pool, NULL);
    stage->timestamps = VK_NULL_HANDLE;
    stage->fence = VK_NULL_HANDLE;
    stage->command = VK_NULL_HANDLE;
    stage->pool = VK_NULL_HANDLE;
    (void)pthread_mutex_destroy(&stage->gate);
}

SIXELSTATUS
sixel_vk_submission_begin(sixel_vk_runtime_t *runtime,
    sixel_vk_submission_t *stage, VkCommandBuffer *command)
{
    VkCommandPoolCreateInfo pool_info;
    VkCommandBufferAllocateInfo allocate;
    VkFenceCreateInfo fence_info;
    VkQueryPoolCreateInfo query_info;
    VkCommandBufferBeginInfo begin_info;

    *command = VK_NULL_HANDLE;
    if (stage->pool == VK_NULL_HANDLE) {
        memset(&pool_info, 0, sizeof(pool_info));
        pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool_info.queueFamilyIndex = runtime->queue_family;
        if (vkCreateCommandPool(runtime->device, &pool_info, NULL,
                &stage->pool) != VK_SUCCESS)
            return SIXEL_RUNTIME_ERROR;
        memset(&allocate, 0, sizeof(allocate));
        allocate.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocate.commandPool = stage->pool;
        allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate.commandBufferCount = 1;
        if (vkAllocateCommandBuffers(runtime->device, &allocate,
                &stage->command) != VK_SUCCESS)
            return SIXEL_RUNTIME_ERROR;
        memset(&fence_info, 0, sizeof(fence_info));
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        if (vkCreateFence(runtime->device, &fence_info, NULL,
                &stage->fence) != VK_SUCCESS)
            return SIXEL_RUNTIME_ERROR;
        if (runtime->timestamps_supported) {
            memset(&query_info, 0, sizeof(query_info));
            query_info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
            query_info.queryType = VK_QUERY_TYPE_TIMESTAMP;
            query_info.queryCount = 2;
            /* Timing is optional; failure leaves wall time as the metric. */
            (void)vkCreateQueryPool(runtime->device, &query_info, NULL,
                &stage->timestamps);
        }
    }
    if (stage->command == VK_NULL_HANDLE ||
            stage->fence == VK_NULL_HANDLE ||
            vkResetCommandBuffer(stage->command, 0) != VK_SUCCESS)
        return SIXEL_RUNTIME_ERROR;
    memset(&begin_info, 0, sizeof(begin_info));
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(stage->command, &begin_info) != VK_SUCCESS)
        return SIXEL_RUNTIME_ERROR;
    *command = stage->command;
    if (stage->timestamps != VK_NULL_HANDLE) {
        vkCmdResetQueryPool(*command, stage->timestamps, 0, 2);
        vkCmdWriteTimestamp(*command, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            stage->timestamps, 0);
    }
    return SIXEL_OK;
}

SIXELSTATUS
sixel_vk_submission_submit_and_wait(sixel_vk_runtime_t *runtime,
    sixel_vk_submission_t *stage, VkCommandBuffer command)
{
    VkSubmitInfo submit;
    VkResult result;
    uint64_t stamps[2];
    unsigned long long busy;
    double started;

    if (stage->timestamps != VK_NULL_HANDLE)
        vkCmdWriteTimestamp(command, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            stage->timestamps, 1);
    if (vkEndCommandBuffer(command) != VK_SUCCESS ||
            vkResetFences(runtime->device, 1, &stage->fence) != VK_SUCCESS)
        return SIXEL_RUNTIME_ERROR;
    memset(&submit, 0, sizeof(submit));
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &command;
    started = sixel_vk_now_ms();
    result = vkQueueSubmit(runtime->queue, 1, &submit, stage->fence);
    if (result != VK_SUCCESS) {
        sixel_helper_set_additional_message(
            "vulkan: command submission failed.");
        return SIXEL_RUNTIME_ERROR;
    }
    /* stage->gate protects the in-flight command, descriptors and staging.
     * The shared queue needs serialization only through vkQueueSubmit. */
    sixel_vk_unlock();
    result = vkWaitForFences(runtime->device, 1, &stage->fence, VK_TRUE,
        UINT64_MAX);
    sixel_vk_lock();
    if (result != VK_SUCCESS) {
        sixel_helper_set_additional_message(
            "vulkan: command completion failed.");
        return SIXEL_RUNTIME_ERROR;
    }
    busy = (unsigned long long)((sixel_vk_now_ms() - started) * 1e6);
    if (stage->timestamps != VK_NULL_HANDLE &&
            vkGetQueryPoolResults(runtime->device, stage->timestamps, 0, 2,
                sizeof(stamps), stamps, sizeof(stamps[0]),
                VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT)
                == VK_SUCCESS)
        busy = (unsigned long long)((double)((stamps[1] - stamps[0]) &
            runtime->timestamp_mask) * runtime->timestamp_period);
    stage->last_busy_ms = (double)busy / 1e6;
    runtime->last_busy_ms = stage->last_busy_ms;
    sixel_gpu_statistics_record(busy);
    return SIXEL_OK;
}

void
sixel_vk_ring_submission_enter(sixel_vk_ring_submission_t *stage)
{
    (void)pthread_mutex_lock(&stage->gate);
}

void
sixel_vk_ring_submission_leave(sixel_vk_ring_submission_t *stage)
{
    (void)pthread_mutex_unlock(&stage->gate);
}

SIXELSTATUS
sixel_vk_ring_submission_begin(sixel_vk_runtime_t *runtime,
    sixel_vk_ring_submission_t *stage, int slot,
    VkCommandBuffer *command)
{
    VkCommandPoolCreateInfo pool_info;
    VkCommandBufferAllocateInfo allocate;
    VkFenceCreateInfo fence_info;
    VkCommandBufferBeginInfo begin_info;
    int i;

    *command = VK_NULL_HANDLE;
    if (slot < 0 || slot >= SIXEL_VK_RING) return SIXEL_BAD_ARGUMENT;
    if (stage->pool == VK_NULL_HANDLE) {
        memset(&pool_info, 0, sizeof(pool_info));
        pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool_info.queueFamilyIndex = runtime->queue_family;
        if (vkCreateCommandPool(runtime->device, &pool_info, NULL,
                &stage->pool) != VK_SUCCESS)
            return SIXEL_RUNTIME_ERROR;
        memset(&allocate, 0, sizeof(allocate));
        allocate.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocate.commandPool = stage->pool;
        allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate.commandBufferCount = SIXEL_VK_RING;
        if (vkAllocateCommandBuffers(runtime->device, &allocate,
                stage->commands) != VK_SUCCESS)
            return SIXEL_RUNTIME_ERROR;
        memset(&fence_info, 0, sizeof(fence_info));
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        for (i = 0; i < SIXEL_VK_RING; ++i) {
            if (vkCreateFence(runtime->device, &fence_info, NULL,
                    &stage->fences[i]) != VK_SUCCESS)
                return SIXEL_RUNTIME_ERROR;
        }
    }
    if (vkResetCommandBuffer(stage->commands[slot], 0) != VK_SUCCESS)
        return SIXEL_RUNTIME_ERROR;
    memset(&begin_info, 0, sizeof(begin_info));
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(stage->commands[slot], &begin_info)
            != VK_SUCCESS)
        return SIXEL_RUNTIME_ERROR;
    *command = stage->commands[slot];
    return SIXEL_OK;
}

SIXELSTATUS
sixel_vk_ring_submission_submit(sixel_vk_runtime_t *runtime,
    sixel_vk_ring_submission_t *stage, int slot)
{
    VkSubmitInfo submit;

    if (slot < 0 || slot >= SIXEL_VK_RING) return SIXEL_BAD_ARGUMENT;
    if (vkEndCommandBuffer(stage->commands[slot]) != VK_SUCCESS ||
            vkResetFences(runtime->device, 1, &stage->fences[slot])
                != VK_SUCCESS)
        return SIXEL_RUNTIME_ERROR;
    memset(&submit, 0, sizeof(submit));
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &stage->commands[slot];
    stage->submitted_ms[slot] = sixel_vk_now_ms();
    if (vkQueueSubmit(runtime->queue, 1, &submit, stage->fences[slot])
            != VK_SUCCESS) {
        sixel_helper_set_additional_message(
            "vulkan: command submission failed.");
        return SIXEL_RUNTIME_ERROR;
    }
    return SIXEL_OK;
}

SIXELSTATUS
sixel_vk_ring_submission_wait(sixel_vk_runtime_t *runtime,
    sixel_vk_ring_submission_t *stage, int slot)
{
    VkResult result;
    double busy;

    if (slot < 0 || slot >= SIXEL_VK_RING) return SIXEL_BAD_ARGUMENT;
    sixel_vk_unlock();
    result = vkWaitForFences(runtime->device, 1, &stage->fences[slot],
        VK_TRUE, UINT64_MAX);
    sixel_vk_lock();
    if (result != VK_SUCCESS) {
        sixel_helper_set_additional_message(
            "vulkan: command completion failed.");
        return SIXEL_RUNTIME_ERROR;
    }
    busy = sixel_vk_now_ms() - stage->submitted_ms[slot];
    runtime->last_busy_ms = busy;
    sixel_gpu_statistics_record((unsigned long long)(busy * 1e6));
    return SIXEL_OK;
}

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
sixel_vk_compute_create_local(sixel_vk_runtime_t *runtime,
    sixel_vk_compute_t *stage, char const *const *chunks, size_t count,
    char const *entry, unsigned int bindings, uint32_t local_size_x)
{
    VkPhysicalDeviceLimits const *limits;
    VkDescriptorSetLayoutBinding slots[SIXEL_VK_COMPUTE_BINDINGS];
    VkDescriptorSetLayoutCreateInfo set_info;
    VkPipelineLayoutCreateInfo layout_info;
    VkDescriptorPoolSize size;
    VkDescriptorPoolCreateInfo pool_info;
    VkDescriptorSetAllocateInfo allocate;
    VkComputePipelineCreateInfo pipeline;
    VkSpecializationMapEntry map;
    VkSpecializationInfo specialization;
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
    if (local_size_x != 0) {
        memset(&map, 0, sizeof(map));
        map.constantID = 0;
        map.size = sizeof(local_size_x);
        memset(&specialization, 0, sizeof(specialization));
        specialization.mapEntryCount = 1;
        specialization.pMapEntries = &map;
        specialization.dataSize = sizeof(local_size_x);
        specialization.pData = &local_size_x;
        pipeline.stage.pSpecializationInfo = &specialization;
    }
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
sixel_vk_compute_create(sixel_vk_runtime_t *runtime,
    sixel_vk_compute_t *stage, char const *const *chunks, size_t count,
    char const *entry, unsigned int bindings)
{
    return sixel_vk_compute_create_local(runtime, stage, chunks, count,
        entry, bindings, 0);
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
