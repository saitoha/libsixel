/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#if defined(HAVE_VULKAN)
#include <stdint.h>
#include <string.h>
#include "gpu-compute-vulkan.h"
#include "gpu-dequant.h"

static char const *const dequant_source[] = {
#include "generated-vulkan/gpu-dequant-fast4-shader.inc.h"
};
static sixel_vk_compute_t dequant_stage;

int
sixel_gpu_dequant_vulkan_is_available(void)
{
    int available;

    sixel_vk_lock();
    available = sixel_vk_runtime_get_locked() != NULL;
    sixel_vk_unlock();
    return available;
}

SIXELSTATUS
sixel_gpu_dequant_vulkan_fast4_rgba(
    sixel_gpu_dequant_request_t const *request)
{
    sixel_vk_runtime_t *runtime;
    sixel_vk_buffer_t buffers[4];
    sixel_vk_buffer_t const *bound[4];
    VkCommandBuffer command;
    uint32_t params[5];
    size_t sizes[4];
    size_t i;
    SIXELSTATUS status;

    if (request->pixel_count > UINT32_MAX / 4u ||
            request->pixel_count != (size_t)request->width * request->height)
        return SIXEL_BAD_ARGUMENT;
    memset(buffers, 0, sizeof(buffers));
    sizes[0] = sizes[1] = request->pixel_count * 4;
    sizes[2] = (size_t)request->ncolors * 3;
    sizes[3] = sizeof(params);
    params[0] = (uint32_t)request->pixel_count;
    params[1] = (uint32_t)request->width;
    params[2] = (uint32_t)request->height;
    params[3] = (uint32_t)request->ncolors;
    params[4] = request->similarity_bias < 1 ? 1u
        : (uint32_t)request->similarity_bias;
    sixel_vk_lock();
    runtime = sixel_vk_runtime_get_locked();
    status = SIXEL_FEATURE_ERROR;
    if (runtime == NULL) goto end;
    status = sixel_vk_compute_create(runtime, &dequant_stage,
        dequant_source, sizeof(dequant_source) / sizeof(dequant_source[0]),
        "dequant_fast4", 4);
    if (status != SIXEL_OK) goto end;
    for (i = 0; i < 4; ++i) {
        status = sixel_vk_buffer_reserve(runtime, &buffers[i], sizes[i],
            i != 0);
        if (status != SIXEL_OK) goto end;
        bound[i] = &buffers[i];
    }
    memcpy(buffers[1].mapped, request->rgba, sizes[1]);
    memcpy(buffers[2].mapped, request->palette, sizes[2]);
    memcpy(buffers[3].mapped, params, sizes[3]);
    status = sixel_vk_compute_bind(runtime, &dequant_stage, bound);
    if (status != SIXEL_OK) goto end;
    status = sixel_vk_command_begin(runtime, &command);
    if (status != SIXEL_OK) goto end;
    for (i = 1; i < 4; ++i)
        sixel_vk_buffer_record_upload(command, &buffers[i], sizes[i]);
    sixel_vk_compute_barrier(command);
    sixel_vk_compute_dispatch(command, &dequant_stage,
        (params[0] + 255u) / 256u);
    sixel_vk_compute_download_barrier(command);
    sixel_vk_buffer_record_download(command, &buffers[0], sizes[0]);
    status = sixel_vk_submit_and_wait(runtime, command);
    if (status == SIXEL_OK) memcpy(request->dest, buffers[0].mapped, sizes[0]);
end:
    if (runtime != NULL) {
        for (i = 0; i < 4; ++i)
            sixel_vk_buffer_release(runtime, &buffers[i]);
    }
    sixel_vk_unlock();
    return status;
}
#endif
