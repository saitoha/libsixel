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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if defined(HAVE_VULKAN)

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(HAVE_SHADERC)
#include <shaderc/shaderc.h>
#endif

#include "compat_stub.h"
#include "gpu-runtime-vulkan.h"
#include "gpu-statistics.h"

static pthread_mutex_t g_sixel_vk_lock = PTHREAD_MUTEX_INITIALIZER;
static sixel_vk_runtime_t g_sixel_vk_runtime;
static int g_sixel_vk_probed;
static int g_sixel_vk_available;

void
sixel_vk_lock(void)
{
    (void)pthread_mutex_lock(&g_sixel_vk_lock);
}

void
sixel_vk_unlock(void)
{
    (void)pthread_mutex_unlock(&g_sixel_vk_lock);
}

double
sixel_vk_now_ms(void)
{
    struct timespec ts;

    (void)clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e3 + (double)ts.tv_nsec / 1e6;
}

/* Literal names keep every environment read reviewable by staticcheck. */
static int
sixel_vk_debug_enabled(void)
{
    char const *value;

    value = sixel_compat_getenv("SIXEL_VULKAN_DEBUG");
    return value != NULL && value[0] != '\0' && strcmp(value, "0") != 0;
}

static int
sixel_vk_type_rank(VkPhysicalDeviceType type)
{
    switch (type) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        return 4;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
        return 3;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
        return 2;
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
        return 1;
    default:
        return 0;
    }
}

/* A device qualifies when it offers Vulkan 1.2, a compute queue and the
 * 8-bit storage/arithmetic features the RGB8 kernels rely on. */
static int
sixel_vk_device_usable(VkPhysicalDevice physical, uint32_t *queue_family)
{
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceVulkan12Features features12;
    VkPhysicalDeviceFeatures2 features;
    VkQueueFamilyProperties *families;
    uint32_t count;
    uint32_t index;
    int found;

    vkGetPhysicalDeviceProperties(physical, &properties);
    if (properties.apiVersion < VK_API_VERSION_1_2) {
        return 0;
    }
    memset(&features12, 0, sizeof(features12));
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    memset(&features, 0, sizeof(features));
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features.pNext = &features12;
    vkGetPhysicalDeviceFeatures2(physical, &features);
    if (!features12.storageBuffer8BitAccess || !features12.shaderInt8) {
        return 0;
    }
    count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, NULL);
    if (count == 0) {
        return 0;
    }
    families = (VkQueueFamilyProperties *)calloc(count, sizeof(*families));
    if (families == NULL) {
        return 0;
    }
    vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, families);
    found = 0;
    for (index = 0; index < count; ++index) {
        if (families[index].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            *queue_family = index;
            found = 1;
            break;
        }
    }
    free(families);
    return found;
}

static int
sixel_vk_select_device(sixel_vk_runtime_t *runtime)
{
    VkPhysicalDevice *devices;
    VkPhysicalDeviceProperties properties;
    char const *wanted;
    char *end;
    long wanted_index;
    uint32_t count;
    uint32_t index;
    uint32_t family;
    int best_rank;
    int rank;

    count = 0;
    if (vkEnumeratePhysicalDevices(runtime->instance, &count, NULL)
            != VK_SUCCESS || count == 0) {
        return 0;
    }
    devices = (VkPhysicalDevice *)calloc(count, sizeof(*devices));
    if (devices == NULL) {
        return 0;
    }
    if (vkEnumeratePhysicalDevices(runtime->instance, &count, devices)
            != VK_SUCCESS) {
        free(devices);
        return 0;
    }
    wanted = sixel_compat_getenv("SIXEL_VULKAN_DEVICE");
    if (wanted != NULL && wanted[0] == '\0') {
        wanted = NULL;
    }
    wanted_index = -1;
    if (wanted != NULL) {
        wanted_index = strtol(wanted, &end, 10);
        if (*end != '\0') {
            wanted_index = -1;
        }
    }
    best_rank = 0;
    for (index = 0; index < count; ++index) {
        vkGetPhysicalDeviceProperties(devices[index], &properties);
        if (runtime->debug) {
            fprintf(stderr, "libsixel: vulkan device %u: %s\n",
                (unsigned int)index, properties.deviceName);
        }
        if (!sixel_vk_device_usable(devices[index], &family)) {
            continue;
        }
        if (wanted != NULL) {
            if ((wanted_index >= 0 && (long)index == wanted_index) ||
                    (wanted_index < 0 &&
                     strstr(properties.deviceName, wanted) != NULL)) {
                rank = 100;
            } else {
                continue;
            }
        } else {
            rank = sixel_vk_type_rank(properties.deviceType) + 1;
        }
        if (rank > best_rank) {
            best_rank = rank;
            runtime->physical = devices[index];
            runtime->queue_family = family;
        }
    }
    free(devices);
    if (best_rank == 0) {
        return 0;
    }
    vkGetPhysicalDeviceProperties(runtime->physical, &runtime->properties);
    vkGetPhysicalDeviceMemoryProperties(runtime->physical, &runtime->memory);
    return 1;
}

static void
sixel_vk_runtime_teardown(sixel_vk_runtime_t *runtime)
{
    int slot;

    if (runtime->timestamps != VK_NULL_HANDLE) {
        vkDestroyQueryPool(runtime->device, runtime->timestamps, NULL);
    }
    if (runtime->fence != VK_NULL_HANDLE) {
        vkDestroyFence(runtime->device, runtime->fence, NULL);
    }
    for (slot = 0; slot < SIXEL_VK_RING; ++slot) {
        if (runtime->ring_fences[slot] != VK_NULL_HANDLE) {
            vkDestroyFence(runtime->device, runtime->ring_fences[slot],
                NULL);
        }
    }
    if (runtime->command_pool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(runtime->device, runtime->command_pool, NULL);
    }
    if (runtime->device != VK_NULL_HANDLE) {
        vkDestroyDevice(runtime->device, NULL);
    }
    if (runtime->instance != VK_NULL_HANDLE) {
        vkDestroyInstance(runtime->instance, NULL);
    }
    memset(runtime, 0, sizeof(*runtime));
}

/* Timestamps are optional: a queue family without valid bits keeps the
 * runtime usable and statistics fall back to host wall time. */
static void
sixel_vk_timestamps_init(sixel_vk_runtime_t *runtime)
{
    VkQueueFamilyProperties *families;
    VkQueryPoolCreateInfo info;
    uint32_t count;
    uint32_t bits;

    if (runtime->properties.limits.timestampPeriod <= 0.0f) {
        return;
    }
    count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(runtime->physical, &count, NULL);
    if (count <= runtime->queue_family) {
        return;
    }
    families = (VkQueueFamilyProperties *)calloc(count, sizeof(*families));
    if (families == NULL) {
        return;
    }
    vkGetPhysicalDeviceQueueFamilyProperties(runtime->physical, &count,
        families);
    bits = families[runtime->queue_family].timestampValidBits;
    free(families);
    if (bits == 0) {
        return;
    }
    memset(&info, 0, sizeof(info));
    info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    info.queryType = VK_QUERY_TYPE_TIMESTAMP;
    info.queryCount = 2;
    if (vkCreateQueryPool(runtime->device, &info, NULL, &runtime->timestamps)
            != VK_SUCCESS) {
        runtime->timestamps = VK_NULL_HANDLE;
        return;
    }
    runtime->timestamp_mask = bits >= 64 ? ~(uint64_t)0 :
        (((uint64_t)1 << bits) - 1U);
    runtime->timestamp_period =
        (double)runtime->properties.limits.timestampPeriod;
}

static int
sixel_vk_runtime_init(sixel_vk_runtime_t *runtime)
{
    VkApplicationInfo application;
    VkInstanceCreateInfo instance_info;
    VkDeviceQueueCreateInfo queue_info;
    VkPhysicalDeviceVulkan12Features features12;
    VkPhysicalDeviceFeatures2 features;
    VkDeviceCreateInfo device_info;
    VkCommandPoolCreateInfo pool_info;
    VkCommandBufferAllocateInfo command_info;
    VkFenceCreateInfo fence_info;
    float priority;
    double started;
    int slot;

    memset(runtime, 0, sizeof(*runtime));
    runtime->debug = sixel_vk_debug_enabled();
    started = sixel_vk_now_ms();

    memset(&application, 0, sizeof(application));
    application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application.pApplicationName = "libsixel";
    application.pEngineName = "libsixel";
    application.apiVersion = VK_API_VERSION_1_2;
    memset(&instance_info, 0, sizeof(instance_info));
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo = &application;
    if (vkCreateInstance(&instance_info, NULL, &runtime->instance)
            != VK_SUCCESS) {
        runtime->instance = VK_NULL_HANDLE;
        goto fail;
    }
    if (!sixel_vk_select_device(runtime)) {
        goto fail;
    }

    priority = 1.0f;
    memset(&queue_info, 0, sizeof(queue_info));
    queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.queueFamilyIndex = runtime->queue_family;
    queue_info.queueCount = 1;
    queue_info.pQueuePriorities = &priority;
    memset(&features12, 0, sizeof(features12));
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.storageBuffer8BitAccess = VK_TRUE;
    features12.shaderInt8 = VK_TRUE;
    memset(&features, 0, sizeof(features));
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features.pNext = &features12;
    vkGetPhysicalDeviceFeatures(runtime->physical, &features.features);
    runtime->shader_int64 = features.features.shaderInt64 != VK_FALSE;
    memset(&features.features, 0, sizeof(features.features));
    features.features.shaderInt64 = runtime->shader_int64 ? VK_TRUE : VK_FALSE;
    memset(&device_info, 0, sizeof(device_info));
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.pNext = &features;
    device_info.queueCreateInfoCount = 1;
    device_info.pQueueCreateInfos = &queue_info;
    if (vkCreateDevice(runtime->physical, &device_info, NULL,
            &runtime->device) != VK_SUCCESS) {
        runtime->device = VK_NULL_HANDLE;
        goto fail;
    }
    vkGetDeviceQueue(runtime->device, runtime->queue_family, 0,
        &runtime->queue);

    memset(&pool_info, 0, sizeof(pool_info));
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = runtime->queue_family;
    if (vkCreateCommandPool(runtime->device, &pool_info, NULL,
            &runtime->command_pool) != VK_SUCCESS) {
        runtime->command_pool = VK_NULL_HANDLE;
        goto fail;
    }
    memset(&command_info, 0, sizeof(command_info));
    command_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_info.commandPool = runtime->command_pool;
    command_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command_info.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(runtime->device, &command_info,
            &runtime->command) != VK_SUCCESS) {
        runtime->command = VK_NULL_HANDLE;
        goto fail;
    }
    memset(&fence_info, 0, sizeof(fence_info));
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    if (vkCreateFence(runtime->device, &fence_info, NULL, &runtime->fence)
            != VK_SUCCESS) {
        runtime->fence = VK_NULL_HANDLE;
        goto fail;
    }
    command_info.commandBufferCount = SIXEL_VK_RING;
    if (vkAllocateCommandBuffers(runtime->device, &command_info,
            runtime->ring) != VK_SUCCESS) {
        memset(runtime->ring, 0, sizeof(runtime->ring));
        goto fail;
    }
    for (slot = 0; slot < SIXEL_VK_RING; ++slot) {
        if (vkCreateFence(runtime->device, &fence_info, NULL,
                &runtime->ring_fences[slot]) != VK_SUCCESS) {
            runtime->ring_fences[slot] = VK_NULL_HANDLE;
            goto fail;
        }
    }
    sixel_vk_timestamps_init(runtime);
    sixel_gpu_statistics_set_device("vulkan", runtime->properties.deviceName);
    if (runtime->debug) {
        fprintf(stderr, "libsixel: vulkan using %s (init %.2f ms, "
            "timestamps %s)\n", runtime->properties.deviceName,
            sixel_vk_now_ms() - started,
            runtime->timestamps != VK_NULL_HANDLE ? "on" : "off");
    }
    return 1;

fail:
    if (runtime->debug) {
        fprintf(stderr, "libsixel: vulkan unavailable (%.2f ms)\n",
            sixel_vk_now_ms() - started);
    }
    sixel_vk_runtime_teardown(runtime);
    return 0;
}

sixel_vk_runtime_t *
sixel_vk_runtime_get_locked(void)
{
    if (!g_sixel_vk_probed) {
        g_sixel_vk_probed = 1;
        g_sixel_vk_available = sixel_vk_runtime_init(&g_sixel_vk_runtime);
    }
    return g_sixel_vk_available ? &g_sixel_vk_runtime : NULL;
}

static int
sixel_vk_memory_type(sixel_vk_runtime_t *runtime, uint32_t type_bits,
    VkMemoryPropertyFlags required, VkMemoryPropertyFlags preferred)
{
    uint32_t index;
    VkMemoryPropertyFlags flags;

    for (index = 0; index < runtime->memory.memoryTypeCount; ++index) {
        flags = runtime->memory.memoryTypes[index].propertyFlags;
        if ((type_bits & (1U << index)) &&
                (flags & (required | preferred)) == (required | preferred)) {
            return (int)index;
        }
    }
    for (index = 0; index < runtime->memory.memoryTypeCount; ++index) {
        flags = runtime->memory.memoryTypes[index].propertyFlags;
        if ((type_bits & (1U << index)) && (flags & required) == required) {
            return (int)index;
        }
    }
    return -1;
}

void
sixel_vk_buffer_release(sixel_vk_runtime_t *runtime, sixel_vk_buffer_t *buffer)
{
    VkDeviceMemory host_memory;

    if (runtime == NULL || buffer == NULL) {
        return;
    }
    host_memory = buffer->staging != VK_NULL_HANDLE
        ? buffer->staging_memory : buffer->memory;
    if (buffer->mapped != NULL) {
        vkUnmapMemory(runtime->device, host_memory);
    }
    if (buffer->staging != VK_NULL_HANDLE) {
        vkDestroyBuffer(runtime->device, buffer->staging, NULL);
    }
    if (buffer->staging_memory != VK_NULL_HANDLE) {
        vkFreeMemory(runtime->device, buffer->staging_memory, NULL);
    }
    if (buffer->buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(runtime->device, buffer->buffer, NULL);
    }
    if (buffer->memory != VK_NULL_HANDLE) {
        vkFreeMemory(runtime->device, buffer->memory, NULL);
    }
    memset(buffer, 0, sizeof(*buffer));
}

/* Create one buffer with its own allocation. flags receives the property
 * flags of the chosen memory type. */
static SIXELSTATUS
sixel_vk_allocate(sixel_vk_runtime_t *runtime, VkDeviceSize size,
    VkBufferUsageFlags usage, VkMemoryPropertyFlags required,
    VkMemoryPropertyFlags preferred, VkBuffer *buffer, VkDeviceMemory *memory,
    VkMemoryPropertyFlags *flags)
{
    VkBufferCreateInfo buffer_info;
    VkMemoryRequirements requirements;
    VkMemoryAllocateInfo allocate_info;
    int type;

    *buffer = VK_NULL_HANDLE;
    *memory = VK_NULL_HANDLE;
    memset(&buffer_info, 0, sizeof(buffer_info));
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(runtime->device, &buffer_info, NULL, buffer)
            != VK_SUCCESS) {
        *buffer = VK_NULL_HANDLE;
        return SIXEL_BAD_ALLOCATION;
    }
    vkGetBufferMemoryRequirements(runtime->device, *buffer, &requirements);
    type = sixel_vk_memory_type(runtime, requirements.memoryTypeBits,
        required, preferred);
    memset(&allocate_info, 0, sizeof(allocate_info));
    allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate_info.allocationSize = requirements.size;
    allocate_info.memoryTypeIndex = (uint32_t)(type < 0 ? 0 : type);
    if (type < 0 ||
            vkAllocateMemory(runtime->device, &allocate_info, NULL, memory)
                != VK_SUCCESS ||
            vkBindBufferMemory(runtime->device, *buffer, *memory, 0)
                != VK_SUCCESS) {
        if (type >= 0 && *memory != VK_NULL_HANDLE) {
            vkFreeMemory(runtime->device, *memory, NULL);
        }
        vkDestroyBuffer(runtime->device, *buffer, NULL);
        *buffer = VK_NULL_HANDLE;
        *memory = VK_NULL_HANDLE;
        return SIXEL_BAD_ALLOCATION;
    }
    *flags = runtime->memory.memoryTypes[type].propertyFlags;
    return SIXEL_OK;
}

SIXELSTATUS
sixel_vk_buffer_reserve(sixel_vk_runtime_t *runtime, sixel_vk_buffer_t *buffer,
    VkDeviceSize size, int host_written)
{
    VkMemoryPropertyFlags flags;
    VkDeviceMemory host_memory;
    SIXELSTATUS status;

    /* Storage-buffer ranges are bound in 4-byte units. */
    size = size == 0 ? 4 : (size + 3U) & ~(VkDeviceSize)3U;
    if (buffer->buffer != VK_NULL_HANDLE && buffer->capacity >= size) {
        return SIXEL_OK;
    }
    sixel_vk_buffer_release(runtime, buffer);

    status = SIXEL_FEATURE_ERROR;
    if (host_written) {
        /* Host-visible VRAM takes sequential host writes at bus speed; one
         * write replaces the copy into staging plus the device copy out of
         * it. Without such a type, or once that window is exhausted, the
         * staged path below still works. */
        status = sixel_vk_allocate(runtime, size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 0, &buffer->buffer,
            &buffer->memory, &flags);
    }
    if (SIXEL_FAILED(status)) {
        /* Cached host memory keeps CPU readback of results fast; coherence
         * avoids explicit flush/invalidate around the synchronous submit. */
        status = sixel_vk_allocate(runtime, size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VK_MEMORY_PROPERTY_HOST_CACHED_BIT, &buffer->buffer,
            &buffer->memory, &flags);
    }
    if (SIXEL_FAILED(status)) {
        return status;
    }
    host_memory = buffer->memory;
    if (!(flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
        /* On a discrete GPU this memory is system RAM: a shader reading it
         * crosses PCIe on every access, which made the direct scan an order
         * of magnitude slower than the arithmetic. Keep the host allocation
         * as a staging copy and give the shader device-local storage. A
         * device without separate VRAM (integrated, CPU) keeps one buffer. */
        buffer->staging = buffer->buffer;
        buffer->staging_memory = buffer->memory;
        buffer->buffer = VK_NULL_HANDLE;
        buffer->memory = VK_NULL_HANDLE;
        status = sixel_vk_allocate(runtime, size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, &buffer->buffer,
            &buffer->memory, &flags);
        if (SIXEL_FAILED(status)) {
            /* Out of VRAM: binding the host buffer is slow but correct. */
            buffer->buffer = buffer->staging;
            buffer->memory = buffer->staging_memory;
            buffer->staging = VK_NULL_HANDLE;
            buffer->staging_memory = VK_NULL_HANDLE;
        }
        host_memory = buffer->staging != VK_NULL_HANDLE
            ? buffer->staging_memory : buffer->memory;
    }
    if (vkMapMemory(runtime->device, host_memory, 0, VK_WHOLE_SIZE, 0,
            &buffer->mapped) != VK_SUCCESS) {
        buffer->mapped = NULL;
        sixel_vk_buffer_release(runtime, buffer);
        return SIXEL_BAD_ALLOCATION;
    }
    buffer->capacity = size;
    return SIXEL_OK;
}

SIXELSTATUS
sixel_vk_buffer_reserve_device(sixel_vk_runtime_t *runtime,
    sixel_vk_buffer_t *buffer, VkDeviceSize size)
{
    VkMemoryPropertyFlags flags;
    SIXELSTATUS status;

    size = size == 0 ? 4 : (size + 3U) & ~(VkDeviceSize)3U;
    if (buffer->buffer != VK_NULL_HANDLE && buffer->capacity >= size) {
        return SIXEL_OK;
    }
    sixel_vk_buffer_release(runtime, buffer);
    status = sixel_vk_allocate(runtime, size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, &buffer->buffer,
        &buffer->memory, &flags);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    buffer->capacity = size;
    return SIXEL_OK;
}

SIXELSTATUS
sixel_vk_buffer_make_shared(sixel_vk_runtime_t *runtime,
    sixel_vk_buffer_t *buffer)
{
    sixel_vk_buffer_t shared;
    VkMemoryPropertyFlags flags;
    VkBufferCopy copy;
    VkCommandBuffer command;
    SIXELSTATUS status;

    if (buffer->mapped != NULL && buffer->staging == VK_NULL_HANDLE)
        return SIXEL_OK;
    memset(&shared, 0, sizeof(shared));
    status = sixel_vk_allocate(runtime, buffer->capacity,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &shared.buffer,
        &shared.memory, &flags);
    if (status != SIXEL_OK) return status;
    if (vkMapMemory(runtime->device, shared.memory, 0, VK_WHOLE_SIZE, 0,
            &shared.mapped) != VK_SUCCESS) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    shared.capacity = buffer->capacity;
    status = sixel_vk_command_begin(runtime, &command);
    if (status != SIXEL_OK) goto end;
    sixel_vk_record_barrier(command, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_ACCESS_MEMORY_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_READ_BIT);
    copy.srcOffset = copy.dstOffset = 0;
    copy.size = buffer->capacity;
    vkCmdCopyBuffer(command, buffer->buffer, shared.buffer, 1, &copy);
    sixel_vk_record_barrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_HOST_BIT,
        VK_ACCESS_HOST_READ_BIT);
    status = sixel_vk_submit_and_wait(runtime, command);
    if (status == SIXEL_OK) {
        sixel_vk_buffer_release(runtime, buffer);
        *buffer = shared;
        return status;
    }
end:
    sixel_vk_buffer_release(runtime, &shared);
    return status;
}

void
sixel_vk_buffer_record_upload(VkCommandBuffer command,
    sixel_vk_buffer_t const *buffer, VkDeviceSize size)
{
    VkBufferCopy region;

    if (buffer->staging == VK_NULL_HANDLE || size == 0) {
        return;
    }
    region.srcOffset = 0;
    region.dstOffset = 0;
    region.size = size < buffer->capacity ? size : buffer->capacity;
    vkCmdCopyBuffer(command, buffer->staging, buffer->buffer, 1, &region);
}

void
sixel_vk_buffer_record_download(VkCommandBuffer command,
    sixel_vk_buffer_t const *buffer, VkDeviceSize size)
{
    VkBufferCopy region;

    if (buffer->staging == VK_NULL_HANDLE || size == 0) {
        return;
    }
    region.srcOffset = 0;
    region.dstOffset = 0;
    region.size = size < buffer->capacity ? size : buffer->capacity;
    vkCmdCopyBuffer(command, buffer->buffer, buffer->staging, 1, &region);
}

void
sixel_vk_buffer_record_download_range(VkCommandBuffer command,
    sixel_vk_buffer_t const *buffer, VkDeviceSize offset, VkDeviceSize size)
{
    VkBufferCopy region;

    if (buffer->staging == VK_NULL_HANDLE || size == 0 ||
            offset >= buffer->capacity) {
        return;
    }
    region.srcOffset = offset;
    region.dstOffset = offset;
    region.size = size < buffer->capacity - offset
        ? size : buffer->capacity - offset;
    vkCmdCopyBuffer(command, buffer->buffer, buffer->staging, 1, &region);
}

void
sixel_vk_record_barrier(VkCommandBuffer command,
    VkPipelineStageFlags src_stage, VkAccessFlags src_access,
    VkPipelineStageFlags dst_stage, VkAccessFlags dst_access)
{
    VkMemoryBarrier barrier;

    memset(&barrier, 0, sizeof(barrier));
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = src_access;
    barrier.dstAccessMask = dst_access;
    vkCmdPipelineBarrier(command, src_stage, dst_stage, 0, 1, &barrier, 0,
        NULL, 0, NULL);
}

#if defined(HAVE_VULKAN_PREBUILT_SPIRV)
/* Every composed lookup program the build matrix produced, keyed by the
 * POSIX cksum CRC and length of its exact GLSL text. */
typedef struct sixel_vk_composed_spirv {
    uint32_t crc;
    size_t length;
    unsigned char const *spirv;
    size_t spirv_size;
} sixel_vk_composed_spirv_t;
#include <generated-vulkan/lookup-programs-spirv.inc.h>
#endif

#if defined(HAVE_SHADERC)
static SIXELSTATUS
sixel_vk_compile_text(sixel_vk_runtime_t *runtime, char const *name,
    char const *text, size_t length, uint32_t **words, size_t *size)
{
    shaderc_compiler_t compiler;
    shaderc_compile_options_t options;
    shaderc_compilation_result_t result;
    SIXELSTATUS status;

    status = SIXEL_RUNTIME_ERROR;
    compiler = shaderc_compiler_initialize();
    options = shaderc_compile_options_initialize();
    result = NULL;
    if (compiler == NULL || options == NULL) {
        goto end;
    }
    shaderc_compile_options_set_target_env(options,
        shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
    shaderc_compile_options_set_optimization_level(options,
        shaderc_optimization_level_performance);
    result = shaderc_compile_into_spv(compiler, text, length,
        shaderc_compute_shader, name, "main", options);
    if (result == NULL ||
            shaderc_result_get_compilation_status(result) !=
                shaderc_compilation_status_success) {
        sixel_helper_set_additional_message(
            "vulkan: runtime GLSL compilation failed.");
        if (runtime->debug && result != NULL) {
            fprintf(stderr, "libsixel: %s: %s\n", name,
                shaderc_result_get_error_message(result));
        }
        goto end;
    }
    *size = shaderc_result_get_length(result);
    *words = (uint32_t *)malloc(*size);
    if (*words == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    memcpy(*words, shaderc_result_get_bytes(result), *size);
    status = SIXEL_OK;
end:
    if (result != NULL) {
        shaderc_result_release(result);
    }
    if (options != NULL) {
        shaderc_compile_options_release(options);
    }
    if (compiler != NULL) {
        shaderc_compiler_release(compiler);
    }
    return status;
}
#endif

static SIXELSTATUS
sixel_vk_module_from_words(sixel_vk_runtime_t *runtime, char const *name,
    char const *origin, uint32_t *words, size_t size, double started,
    VkShaderModule *module)
{
    VkShaderModuleCreateInfo info;
    VkResult result;

    if (size == 0 || size % 4U != 0) {
        free(words);
        return SIXEL_RUNTIME_ERROR;
    }
    memset(&info, 0, sizeof(info));
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = size;
    info.pCode = words;
    result = vkCreateShaderModule(runtime->device, &info, NULL, module);
    free(words);
    if (result != VK_SUCCESS) {
        *module = VK_NULL_HANDLE;
        return SIXEL_RUNTIME_ERROR;
    }
    if (runtime->debug) {
        fprintf(stderr, "libsixel: vulkan shader %s from %s SPIR-V "
            "(%.2f ms)\n", name, origin, sixel_vk_now_ms() - started);
    }
    return SIXEL_OK;
}

/* Copy embedded bytes, which carry no alignment guarantee, into words. */
static SIXELSTATUS
sixel_vk_copy_words(unsigned char const *spirv, size_t size,
    uint32_t **words)
{
    *words = (uint32_t *)malloc(size == 0 ? 1U : size);
    if (*words == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    memcpy(*words, spirv, size);
    return SIXEL_OK;
}

/* POSIX cksum: CRC-32 (polynomial 0x04c11db7, most significant bit first)
 * over the bytes and then the length, complemented. The build matrix keys
 * its SPIR-V with cksum(1), so both sides name one text identically. */
static uint32_t
sixel_vk_cksum_byte(uint32_t crc, unsigned int byte)
{
    int bit;

    crc ^= (uint32_t)(byte & 0xffU) << 24;
    for (bit = 0; bit < 8; ++bit) {
        crc = (crc & 0x80000000U) != 0
            ? (crc << 1) ^ 0x04c11db7U : crc << 1;
    }
    return crc;
}

SIXELSTATUS
sixel_vk_shader_module_compose(sixel_vk_runtime_t *runtime, char const *name,
    sixel_vk_source_part_t const *parts, size_t part_count,
    VkShaderModule *module)
{
    char const *mode;
    char const *chunk;
    uint32_t *words;
    uint32_t crc;
    size_t length;
    size_t size;
    size_t part;
    size_t index;
    size_t remaining;
    double started;
    SIXELSTATUS status;
#if defined(HAVE_VULKAN_PREBUILT_SPIRV)
    size_t entry;
#endif
#if defined(HAVE_SHADERC)
    char *text;
#endif

    *module = VK_NULL_HANDLE;
    words = NULL;
    size = 0;
    started = sixel_vk_now_ms();
    mode = sixel_compat_getenv("SIXEL_VULKAN_SHADER");
    crc = 0;
    length = 0;
    for (part = 0; part < part_count; ++part) {
        for (index = 0; index < parts[part].count; ++index) {
            if (parts[part].chunks[index] == NULL) {
                return SIXEL_BAD_ARGUMENT;
            }
            for (chunk = parts[part].chunks[index]; *chunk != '\0';
                    ++chunk) {
                crc = sixel_vk_cksum_byte(crc, (unsigned char)*chunk);
                ++length;
            }
        }
    }
    for (remaining = length; remaining != 0; remaining >>= 8) {
        crc = sixel_vk_cksum_byte(crc, (unsigned int)(remaining & 0xffU));
    }
    crc = ~crc;
#if defined(HAVE_VULKAN_PREBUILT_SPIRV)
    if (mode == NULL || strcmp(mode, "runtime") != 0) {
        for (entry = 0; entry < sizeof(sixel_vk_composed_spirv) /
                sizeof(sixel_vk_composed_spirv[0]); ++entry) {
            if (sixel_vk_composed_spirv[entry].crc == crc &&
                    sixel_vk_composed_spirv[entry].length == length) {
                size = sixel_vk_composed_spirv[entry].spirv_size;
                status = sixel_vk_copy_words(
                    sixel_vk_composed_spirv[entry].spirv, size, &words);
                if (SIXEL_FAILED(status)) {
                    return status;
                }
                return sixel_vk_module_from_words(runtime, name,
                    "prebuilt", words, size, started, module);
            }
        }
    }
#endif
    if (mode != NULL && strcmp(mode, "prebuilt") == 0) {
        sixel_helper_set_additional_message(
            "vulkan: prebuilt SPIR-V is not available.");
        return SIXEL_FEATURE_ERROR;
    }
#if defined(HAVE_SHADERC)
    text = (char *)malloc(length + 1U);
    if (text == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    length = 0;
    for (part = 0; part < part_count; ++part) {
        for (index = 0; index < parts[part].count; ++index) {
            size = strlen(parts[part].chunks[index]);
            memcpy(text + length, parts[part].chunks[index], size);
            length += size;
        }
    }
    text[length] = '\0';
    status = sixel_vk_compile_text(runtime, name, text, length, &words,
        &size);
    free(text);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    return sixel_vk_module_from_words(runtime, name, "runtime-compiled",
        words, size, started, module);
#else
    sixel_helper_set_additional_message(
        "vulkan: this lookup program was not built and no runtime GLSL "
        "compiler is available.");
    return SIXEL_FEATURE_ERROR;
#endif
}

SIXELSTATUS
sixel_vk_command_begin(sixel_vk_runtime_t *runtime, VkCommandBuffer *command)
{
    VkCommandBufferBeginInfo begin_info;

    *command = VK_NULL_HANDLE;
    if (vkResetCommandBuffer(runtime->command, 0) != VK_SUCCESS) {
        return SIXEL_RUNTIME_ERROR;
    }
    memset(&begin_info, 0, sizeof(begin_info));
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(runtime->command, &begin_info) != VK_SUCCESS) {
        return SIXEL_RUNTIME_ERROR;
    }
    *command = runtime->command;
    if (runtime->timestamps != VK_NULL_HANDLE) {
        vkCmdResetQueryPool(*command, runtime->timestamps, 0, 2);
        vkCmdWriteTimestamp(*command, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            runtime->timestamps, 0);
    }
    return SIXEL_OK;
}

void
sixel_vk_command_free(sixel_vk_runtime_t *runtime, VkCommandBuffer command)
{
    /* The runtime owns the buffer; the next command_begin resets it. */
    (void)runtime;
    (void)command;
}

SIXELSTATUS
sixel_vk_submit_and_wait(sixel_vk_runtime_t *runtime, VkCommandBuffer command)
{
    VkSubmitInfo submit;
    VkFence fence;
    VkResult result;
    uint64_t stamps[2];
    unsigned long long busy;
    double started;

    if (runtime->timestamps != VK_NULL_HANDLE) {
        vkCmdWriteTimestamp(command, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            runtime->timestamps, 1);
    }
    if (vkEndCommandBuffer(command) != VK_SUCCESS) {
        return SIXEL_RUNTIME_ERROR;
    }
    fence = runtime->fence;
    if (vkResetFences(runtime->device, 1, &fence) != VK_SUCCESS) {
        return SIXEL_RUNTIME_ERROR;
    }
    memset(&submit, 0, sizeof(submit));
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &command;
    started = sixel_vk_now_ms();
    result = vkQueueSubmit(runtime->queue, 1, &submit, fence);
    if (result == VK_SUCCESS) {
        result = vkWaitForFences(runtime->device, 1, &fence, VK_TRUE,
            UINT64_MAX);
    }
    if (result != VK_SUCCESS) {
        sixel_helper_set_additional_message(
            "vulkan: command submission failed.");
        return SIXEL_RUNTIME_ERROR;
    }
    busy = (unsigned long long)((sixel_vk_now_ms() - started) * 1e6);
    if (runtime->timestamps != VK_NULL_HANDLE &&
            vkGetQueryPoolResults(runtime->device, runtime->timestamps, 0, 2,
                sizeof(stamps), stamps, sizeof(stamps[0]),
                VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT)
                == VK_SUCCESS) {
        busy = (unsigned long long)((double)((stamps[1] - stamps[0]) &
            runtime->timestamp_mask) * runtime->timestamp_period);
    }
    runtime->last_busy_ms = (double)busy / 1e6;
    sixel_gpu_statistics_record(busy);
    return SIXEL_OK;
}

SIXELSTATUS
sixel_vk_ring_begin(sixel_vk_runtime_t *runtime, int slot,
    VkCommandBuffer *command)
{
    VkCommandBufferBeginInfo begin_info;

    *command = VK_NULL_HANDLE;
    if (slot < 0 || slot >= SIXEL_VK_RING) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (vkResetCommandBuffer(runtime->ring[slot], 0) != VK_SUCCESS) {
        return SIXEL_RUNTIME_ERROR;
    }
    memset(&begin_info, 0, sizeof(begin_info));
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(runtime->ring[slot], &begin_info)
            != VK_SUCCESS) {
        return SIXEL_RUNTIME_ERROR;
    }
    *command = runtime->ring[slot];
    return SIXEL_OK;
}

SIXELSTATUS
sixel_vk_ring_submit(sixel_vk_runtime_t *runtime, int slot)
{
    VkSubmitInfo submit;

    if (slot < 0 || slot >= SIXEL_VK_RING) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (vkEndCommandBuffer(runtime->ring[slot]) != VK_SUCCESS ||
            vkResetFences(runtime->device, 1, &runtime->ring_fences[slot])
                != VK_SUCCESS) {
        return SIXEL_RUNTIME_ERROR;
    }
    memset(&submit, 0, sizeof(submit));
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &runtime->ring[slot];
    runtime->ring_submitted_ms[slot] = sixel_vk_now_ms();
    if (vkQueueSubmit(runtime->queue, 1, &submit, runtime->ring_fences[slot])
            != VK_SUCCESS) {
        sixel_helper_set_additional_message(
            "vulkan: command submission failed.");
        return SIXEL_RUNTIME_ERROR;
    }
    return SIXEL_OK;
}

SIXELSTATUS
sixel_vk_ring_wait(sixel_vk_runtime_t *runtime, int slot)
{
    double busy;

    if (slot < 0 || slot >= SIXEL_VK_RING) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (vkWaitForFences(runtime->device, 1, &runtime->ring_fences[slot],
            VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
        sixel_helper_set_additional_message(
            "vulkan: command completion failed.");
        return SIXEL_RUNTIME_ERROR;
    }
    busy = sixel_vk_now_ms() - runtime->ring_submitted_ms[slot];
    runtime->last_busy_ms = busy;
    sixel_gpu_statistics_record((unsigned long long)(busy * 1e6));
    return SIXEL_OK;
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
