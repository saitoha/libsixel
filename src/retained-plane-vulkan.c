/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#if defined(HAVE_VULKAN) && !defined(HAVE_METAL)
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "gpu-compute-vulkan.h"
#include "gpu-palette-vulkan.h"
#include "retained-plane-gpu.h"

static char const *const retained_source[] = {
#include "generated-vulkan/retained-plane-gpu-shader.inc.h"
};
static sixel_vk_compute_t retained_stage;
typedef struct retained_vulkan_params {
    uint32_t width, height, x, y, stride, count, record_paint;
    unsigned char palette[768];
} retained_vulkan_params_t;


struct sixel_retained_gpu {
    sixel_gpu_device_interface_t *device;
    sixel_retained_plane_description_t description;
    sixel_gpu_buffer_interface_t *rgb[2];
    sixel_gpu_buffer_interface_t *valid[2];
    unsigned char *rgb_map[2];
    unsigned char *valid_map[2];
    /* Indexes, skip, paint, reductions and immutable dispatch parameters. */
    sixel_vk_buffer_t scratch[5];
    retained_vulkan_params_t params;
    uint32_t groups;
    int committed;
    uint32_t dirty_x, dirty_y, dirty_width, dirty_height;
};

SIXEL_INTERNAL_API void
sixel_retained_gpu_free(sixel_retained_gpu_t *gpu)
{
    sixel_vk_runtime_t *runtime;
    int i;

    if (gpu == NULL) return;
    for (i = 0; i < 2; ++i) {
        if (gpu->rgb[i] != NULL) gpu->rgb[i]->vtbl->unref(gpu->rgb[i]);
        if (gpu->valid[i] != NULL) gpu->valid[i]->vtbl->unref(gpu->valid[i]);
    }
    sixel_vk_lock();
    runtime = sixel_vk_runtime_get_locked();
    for (i = 0; i < 5; ++i)
        sixel_vk_buffer_release(runtime, &gpu->scratch[i]);
    sixel_vk_unlock();
    gpu->device->vtbl->unref(gpu->device);
    free(gpu);
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_retained_gpu_new(sixel_gpu_device_interface_t *device,
    sixel_retained_plane_description_t const *description,
    unsigned char const *rgb, unsigned char const *valid,
    sixel_retained_gpu_t **result)
{
    sixel_retained_gpu_t *gpu;
    sixel_vk_runtime_t *runtime;
    void *memory;
    size_t pixels;
    int i;
    SIXELSTATUS status;

    *result = NULL;
    if (!sixel_gpu_vulkan_is_device(device)) return SIXEL_FEATURE_ERROR;
    pixels = (size_t)description->width * description->height;
    if (pixels > UINT32_MAX / 3) return SIXEL_FEATURE_ERROR;
    gpu = calloc(1, sizeof(*gpu));
    if (gpu == NULL) return SIXEL_BAD_ALLOCATION;
    gpu->device = device;
    gpu->description = *description;
    device->vtbl->ref(device);
    sixel_vk_lock();
    runtime = sixel_vk_runtime_get_locked();
    status = runtime == NULL ? SIXEL_FEATURE_ERROR :
        sixel_vk_compute_create(runtime, &retained_stage, retained_source,
            sizeof(retained_source) / sizeof(retained_source[0]),
            "retained_expand", 9);
    sixel_vk_unlock();
    if (status != SIXEL_OK) goto fail;
    for (i = 0; i < 2; ++i) {
        status = device->vtbl->create_buffer(device,
            (pixels * 3 + 3) & ~(size_t)3, &gpu->rgb[i]);
        if (status != SIXEL_OK) goto fail;
        status = gpu->rgb[i]->vtbl->shared_memory(gpu->rgb[i], &memory);
        if (status != SIXEL_OK) goto fail;
        gpu->rgb_map[i] = memory;
        status = device->vtbl->create_buffer(device,
            (pixels + 3) & ~(size_t)3, &gpu->valid[i]);
        if (status != SIXEL_OK) goto fail;
        status = gpu->valid[i]->vtbl->shared_memory(gpu->valid[i], &memory);
        if (status != SIXEL_OK) goto fail;
        gpu->valid_map[i] = memory;
        memset(gpu->rgb_map[i], 0, (pixels * 3 + 3) & ~(size_t)3);
        memset(gpu->valid_map[i], 0, (pixels + 3) & ~(size_t)3);
        memcpy(gpu->rgb_map[i], rgb, pixels * 3);
        memcpy(gpu->valid_map[i], valid, pixels);
    }
    *result = gpu;
    return SIXEL_OK;
fail:
    sixel_retained_gpu_free(gpu);
    return status;
}
SIXEL_INTERNAL_API SIXELSTATUS
sixel_retained_gpu_read(sixel_retained_gpu_t *gpu,
    sixel_gpu_device_interface_t *device, sixel_retained_gpu_read_view_t *view)
{
    if (gpu == NULL || gpu->device != device) return SIXEL_FEATURE_ERROR;
    view->rgb = gpu->rgb[gpu->committed];
    view->valid = gpu->valid[gpu->committed];
    view->rgb_stride = (size_t)gpu->description.width * 3;
    view->valid_stride = (size_t)gpu->description.width;
    view->description = gpu->description;
    return SIXEL_OK;
}

SIXEL_INTERNAL_API void
sixel_retained_gpu_map(sixel_retained_gpu_t *gpu,
    unsigned char **rgb, unsigned char **valid)
{
    *rgb = gpu->rgb_map[gpu->committed];
    *valid = gpu->valid_map[gpu->committed];
}


SIXEL_INTERNAL_API SIXELSTATUS
sixel_retained_gpu_prepare(sixel_retained_gpu_t *gpu,
    sixel_indexed_frame_description_t const *image,
    sixel_wire_palette_view_t const *palette, int record_paint)
{
    sixel_vk_runtime_t *runtime;
    size_t sizes[5];
    size_t pixels;
    size_t i;
    SIXELSTATUS status;

    pixels = (size_t)image->width * image->height;
    memset(&gpu->params, 0, sizeof(gpu->params));
    gpu->params.width = (uint32_t)image->width;
    gpu->params.height = (uint32_t)image->height;
    gpu->params.x = (uint32_t)image->origin_x;
    gpu->params.y = (uint32_t)image->origin_y;
    gpu->params.stride = (uint32_t)gpu->description.width;
    gpu->params.count = (uint32_t)pixels;
    gpu->groups = ((uint32_t)pixels + 255u) / 256u;
    gpu->params.record_paint = record_paint != 0;
    for (i = 0; i < palette->count; ++i)
        memcpy(gpu->params.palette + 3 * i, palette->colors[i].emitted_rgb, 3);
    sizes[0] = sizes[1] = pixels;
    sizes[2] = record_paint ? pixels : 4;
    sizes[3] = (size_t)gpu->groups * 8;
    sizes[4] = sizeof(gpu->params);
    sixel_vk_lock();
    runtime = sixel_vk_runtime_get_locked();
    status = SIXEL_OK;
    for (i = 0; i < 5 && status == SIXEL_OK; ++i)
        status = sixel_vk_buffer_reserve(runtime, &gpu->scratch[i], sizes[i],
            i < 2 || i == 4);
    sixel_vk_unlock();
    return status;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_retained_gpu_stage(sixel_retained_gpu_t *gpu,
    unsigned char const *indexes, unsigned char const *skip,
    unsigned char *paint, sixel_retained_update_stats_t *stats, int fault)
{
    sixel_vk_runtime_t *runtime;
    sixel_vk_buffer_t const *bound[9];
    VkCommandBuffer command;
    VkBufferCopy copy;
    size_t offset, length, aligned, bytes;
    size_t pixels;
    size_t i;
    uint32_t right, bottom;
    uint32_t const *counts;
    int covered;
    int current;
    SIXELSTATUS status;

    if (fault == 1) return SIXEL_RUNTIME_ERROR;
    current = gpu->committed;
    pixels = gpu->params.count;
    bound[0] = sixel_gpu_vulkan_buffer(gpu->device, gpu->rgb[current]);
    bound[1] = sixel_gpu_vulkan_buffer(gpu->device, gpu->valid[current]);
    bound[2] = sixel_gpu_vulkan_buffer(gpu->device, gpu->rgb[1-current]);
    bound[3] = sixel_gpu_vulkan_buffer(gpu->device, gpu->valid[1-current]);
    for (i = 0; i < 5; ++i) bound[4 + i] = &gpu->scratch[i];
    memcpy(gpu->scratch[0].mapped, indexes, pixels);
    if (skip != NULL) memcpy(gpu->scratch[1].mapped, skip, pixels);
    else memset(gpu->scratch[1].mapped, 0, pixels);
    memcpy(gpu->scratch[4].mapped, &gpu->params, sizeof(gpu->params));
    sixel_vk_lock();
    runtime = sixel_vk_runtime_get_locked();
    status = sixel_vk_compute_bind(runtime, &retained_stage, bound);
    if (status != SIXEL_OK) goto end;
    status = sixel_vk_command_begin(runtime, &command);
    if (status != SIXEL_OK) goto end;
    covered = gpu->params.x <= gpu->dirty_x &&
        gpu->params.y <= gpu->dirty_y &&
        gpu->params.x + gpu->params.width >= gpu->dirty_x + gpu->dirty_width &&
        gpu->params.y + gpu->params.height >= gpu->dirty_y + gpu->dirty_height;
    if (gpu->dirty_height != 0 && !covered) {
        /* Copy only the stale span. Four-byte rounding is contained by
         * the padded canvas allocations, including the final RGB pixel. */
        offset = (size_t)gpu->dirty_y * gpu->description.width + gpu->dirty_x;
        length = (size_t)(gpu->dirty_height - 1) * gpu->description.width +
            gpu->dirty_width;
        sixel_vk_record_barrier(command, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VK_ACCESS_MEMORY_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
        for (i = 0; i < 2; ++i) {
            aligned = (offset * (i == 0 ? 3 : 1)) & ~(size_t)3;
            bytes = ((offset + length) * (i == 0 ? 3 : 1) + 3) & ~(size_t)3;
            copy.srcOffset = copy.dstOffset = aligned;
            copy.size = bytes - aligned;
            vkCmdCopyBuffer(command, bound[i]->buffer, bound[2+i]->buffer,
                1, &copy);
        }
    }
    sixel_vk_buffer_record_upload(command, bound[4], pixels);
    sixel_vk_buffer_record_upload(command, bound[5], pixels);
    sixel_vk_buffer_record_upload(command, bound[8], sizeof(gpu->params));
    sixel_vk_compute_barrier(command);
    sixel_vk_compute_dispatch(command, &retained_stage, gpu->groups);
    sixel_vk_compute_download_barrier(command);
    sixel_vk_buffer_record_download(command, bound[7], (size_t)gpu->groups * 8);
    if (paint != NULL)
        sixel_vk_buffer_record_download(command, bound[6], pixels);
    /* A failed command may have touched any encoded range. Keep the union
     * dirty until successful completion narrows it to this rectangle. */
    right = gpu->params.x + gpu->params.width;
    bottom = gpu->params.y + gpu->params.height;
    if (gpu->dirty_height == 0) {
        gpu->dirty_x = gpu->params.x;
        gpu->dirty_y = gpu->params.y;
    } else {
        if (right < gpu->dirty_x + gpu->dirty_width)
            right = gpu->dirty_x + gpu->dirty_width;
        if (bottom < gpu->dirty_y + gpu->dirty_height)
            bottom = gpu->dirty_y + gpu->dirty_height;
        if (gpu->dirty_x > gpu->params.x) gpu->dirty_x = gpu->params.x;
        if (gpu->dirty_y > gpu->params.y) gpu->dirty_y = gpu->params.y;
    }
    gpu->dirty_width = right - gpu->dirty_x;
    gpu->dirty_height = bottom - gpu->dirty_y;
    status = sixel_vk_submit_and_wait(runtime, command);
    if (status != SIXEL_OK) goto end;
    if (fault == 2) {
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }
    gpu->dirty_x = gpu->params.x;
    gpu->dirty_y = gpu->params.y;
    gpu->dirty_width = gpu->params.width;
    gpu->dirty_height = gpu->params.height;
    memset(stats, 0, sizeof(*stats));
    counts = gpu->scratch[3].mapped;
    for (i = 0; i < gpu->groups; ++i) {
        stats->painted_pixels += counts[i * 2];
        stats->refinement_needed |= counts[i * 2 + 1] != 0;
    }
    if (paint != NULL) memcpy(paint, gpu->scratch[2].mapped, pixels);
end:
    sixel_vk_unlock();
    return status;
}
SIXEL_INTERNAL_API void
sixel_retained_gpu_cpu_write(sixel_retained_gpu_t *gpu)
{
    if (gpu == NULL) return;
    gpu->dirty_x = gpu->dirty_y = 0;
    gpu->dirty_width = (uint32_t)gpu->description.width;
    gpu->dirty_height = (uint32_t)gpu->description.height;
}

SIXEL_INTERNAL_API void
sixel_retained_gpu_commit(sixel_retained_gpu_t *gpu)
{
    gpu->committed = 1 - gpu->committed;
}
#endif
