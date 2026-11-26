/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#if defined(HAVE_VULKAN) && !defined(HAVE_METAL)
#include <string.h>

#include "gpu-compute-vulkan.h"
#include "palette-common-merge.h"
#include "sixel_atomic.h"

enum {
    WARD_VK_INPUT, WARD_VK_ROWS, WARD_VK_PARAMS, WARD_VK_PAIRS,
    WARD_VK_BUFFERS
};

static char const *const sixel_ward_vulkan_source[] = {
#include "generated-vulkan/palette-ward-shader.inc.h"
};

static uint32_t const sixel_ward_vulkan_widths[] = {
    128u, 256u, 512u, SIXEL_WARD_MAX_CLUSTERS
};
static sixel_vk_compute_t sixel_ward_vulkan_rows;
static sixel_vk_compute_t sixel_ward_vulkan_merges[4];
static sixel_vk_buffer_t sixel_ward_vulkan_buffers[WARD_VK_BUFFERS];
static sixel_vk_submission_t sixel_ward_vulkan_submission =
    SIXEL_VK_SUBMISSION_INITIALIZER;
static sixel_atomic_i32_t sixel_ward_vulkan_ready;
static uint32_t sixel_ward_vulkan_subgroup;

static unsigned int
sixel_ward_vulkan_shape(unsigned int count)
{
    unsigned int i;

    for (i = 0; i < 3u; ++i) {
        if (count <= sixel_ward_vulkan_widths[i])
            return i;
    }
    return i;
}

int
sixel_ward_vulkan_is_ready(unsigned int count)
{
    return (sixel_atomic_load_relaxed_i32(&sixel_ward_vulkan_ready) &
        (1 << sixel_ward_vulkan_shape(count))) != 0;
}

static int
sixel_ward_vulkan_prepare(sixel_vk_runtime_t *runtime, unsigned int shape)
{
    static VkDeviceSize const sizes[WARD_VK_BUFFERS] = {
        SIXEL_WARD_MAX_CLUSTERS * 4u * sizeof(float),
        SIXEL_WARD_MAX_CLUSTERS * 2u * sizeof(uint32_t),
        2u * sizeof(uint32_t),
        SIXEL_WARD_MAX_CLUSTERS * sizeof(uint32_t)
    };
    VkPhysicalDeviceSubgroupProperties subgroup;
    VkPhysicalDeviceProperties2 properties;
    VkPhysicalDeviceLimits const *limits;
    VkSubgroupFeatureFlags needed;
    sixel_vk_buffer_t *buffer;
    sixel_vk_buffer_t const *bound[WARD_VK_BUFFERS];
    unsigned int i;
    uint32_t lanes;
    size_t sources;
    SIXELSTATUS status;

    if (sixel_atomic_load_relaxed_i32(&sixel_ward_vulkan_ready) &
            (1 << shape))
        return 1;
    lanes = sixel_ward_vulkan_widths[shape];
    limits = &runtime->properties.limits;
    if (lanes > limits->maxComputeWorkGroupInvocations ||
            lanes > limits->maxComputeWorkGroupSize[0] ||
            limits->maxComputeSharedMemorySize < 32768u)
        return 0;
    if (!sixel_ward_vulkan_subgroup) {
        memset(&subgroup, 0, sizeof(subgroup));
        subgroup.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
        memset(&properties, 0, sizeof(properties));
        properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        properties.pNext = &subgroup;
        vkGetPhysicalDeviceProperties2(runtime->physical, &properties);
        needed = VK_SUBGROUP_FEATURE_BASIC_BIT |
            VK_SUBGROUP_FEATURE_ARITHMETIC_BIT;
        if (!(subgroup.supportedStages & VK_SHADER_STAGE_COMPUTE_BIT) ||
                (subgroup.supportedOperations & needed) != needed ||
                subgroup.subgroupSize < 8u || subgroup.subgroupSize > 128u)
            return 0;
        sixel_ward_vulkan_subgroup = subgroup.subgroupSize;
    }
    for (i = 0; i < WARD_VK_BUFFERS; ++i) {
        buffer = &sixel_ward_vulkan_buffers[i];
        if (i == WARD_VK_ROWS)
            status = sixel_vk_buffer_reserve_device(runtime, buffer, sizes[i]);
        else if (i == WARD_VK_PAIRS)
            status = sixel_vk_buffer_reserve_readback(runtime, buffer,
                sizes[i]);
        else
            status = sixel_vk_buffer_reserve(runtime, buffer, sizes[i], 1);
        if (status != SIXEL_OK)
            return 0;
        bound[i] = buffer;
    }
    sources = sizeof(sixel_ward_vulkan_source) /
        sizeof(sixel_ward_vulkan_source[0]);
    if (sixel_vk_compute_create_local(runtime, &sixel_ward_vulkan_rows,
            sixel_ward_vulkan_source, sources, "ward_rows",
            WARD_VK_BUFFERS, sixel_ward_vulkan_subgroup) != SIXEL_OK ||
            sixel_vk_compute_create_local(runtime,
                &sixel_ward_vulkan_merges[shape],
                sixel_ward_vulkan_source, sources, "ward_merge",
                WARD_VK_BUFFERS, lanes) != SIXEL_OK ||
            sixel_vk_compute_bind(runtime, &sixel_ward_vulkan_rows,
                bound) != SIXEL_OK ||
            sixel_vk_compute_bind(runtime, &sixel_ward_vulkan_merges[shape],
                bound) != SIXEL_OK)
        return 0;
    sixel_atomic_store_relaxed_i32(&sixel_ward_vulkan_ready,
        sixel_atomic_load_relaxed_i32(&sixel_ward_vulkan_ready) | (1 << shape));
    return 1;
}

int
sixel_ward_vulkan_pairs(float const *input, unsigned int count,
    unsigned int merges, sixel_ward_pair_t *pairs)
{
    sixel_vk_runtime_t *runtime;
    sixel_vk_submission_t *submit;
    sixel_vk_buffer_t *buffers;
    VkCommandBuffer command;
    uint32_t params[2];
    uint32_t const *output;
    size_t input_bytes, output_bytes;
    unsigned int i, shape;
    int ok;

    if (!count || count > SIXEL_WARD_MAX_CLUSTERS ||
            !merges || merges >= count)
        return 0;
    shape = sixel_ward_vulkan_shape(count);
    input_bytes = (size_t)count * 4u * sizeof(*input);
    output_bytes = (size_t)merges * sizeof(*output);
    params[0] = count;
    params[1] = merges;
    ok = 0;
    submit = &sixel_ward_vulkan_submission;
    buffers = sixel_ward_vulkan_buffers;
    sixel_vk_submission_enter(submit);
    sixel_vk_lock();
    runtime = sixel_vk_runtime_get_locked();
    if (runtime == NULL || !sixel_ward_vulkan_prepare(runtime, shape))
        goto done;
    memcpy(buffers[WARD_VK_INPUT].mapped, input, input_bytes);
    memcpy(buffers[WARD_VK_PARAMS].mapped, params, sizeof(params));

    submit->scoped_timestamps = 1;
    if (sixel_vk_submission_begin(runtime, submit, &command) != SIXEL_OK)
        goto done;
    sixel_vk_buffer_record_upload(command, &buffers[WARD_VK_INPUT],
        input_bytes);
    sixel_vk_buffer_record_upload(command, &buffers[WARD_VK_PARAMS],
        sizeof(params));
    /* Missing pairs must be rejected even if the same slot held valid
     * output on the previous frame. */
    vkCmdFillBuffer(command, buffers[WARD_VK_PAIRS].buffer,
        0, output_bytes, UINT32_MAX);
    sixel_vk_compute_barrier(command);
    if (submit->timestamps != VK_NULL_HANDLE)
        vkCmdWriteTimestamp(command, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            submit->timestamps, 0);
    sixel_vk_compute_dispatch(command, &sixel_ward_vulkan_rows, count);
    sixel_vk_compute_barrier(command);
    sixel_vk_compute_dispatch(command, &sixel_ward_vulkan_merges[shape], 1u);
    if (submit->timestamps != VK_NULL_HANDLE)
        vkCmdWriteTimestamp(command, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            submit->timestamps, 1);
    sixel_vk_compute_download_barrier(command);
    sixel_vk_buffer_record_download(command, &buffers[WARD_VK_PAIRS],
        output_bytes);
    sixel_vk_record_barrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_HOST_BIT,
        VK_ACCESS_HOST_READ_BIT);
    if (sixel_vk_submission_submit_and_wait(runtime, submit, command)
            != SIXEL_OK ||
            sixel_vk_buffer_invalidate(runtime, &buffers[WARD_VK_PAIRS],
                output_bytes) != SIXEL_OK)
        goto done;
    output = buffers[WARD_VK_PAIRS].mapped;
    for (i = 0; i < merges; ++i) {
        pairs[i].kept = (uint16_t)(output[i] >> 16u);
        pairs[i].removed = (uint16_t)output[i];
    }
    ok = 1;
done:
    sixel_vk_unlock();
    sixel_vk_submission_leave(submit);
    return ok;
}
#else
typedef int palette_ward_vulkan_disabled;
#endif
