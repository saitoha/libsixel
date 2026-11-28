/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#if defined(HAVE_VULKAN)
#include <string.h>
#include "gpu-ycbcr.h"
#include "gpu-compute-vulkan.h"

static char const *const ycbcr_vk_source[] = {
#include "generated-vulkan/gpu-ycbcr-shader.inc.h"
};

enum { YCBCR_VK_INPUT, YCBCR_VK_OUTPUT, YCBCR_VK_TAPS, YCBCR_VK_PARAMS,
       YCBCR_VK_COUNT };

typedef struct sixel_ycbcr_vk_state {
    sixel_vk_runtime_t *runtime;
    sixel_vk_submission_t submission;
    sixel_vk_compute_t stage;
    sixel_vk_buffer_t buffers[YCBCR_VK_COUNT];
} sixel_ycbcr_vk_state_t;

static void
sixel_ycbcr_vk_destroy(void *state, sixel_allocator_t *allocator)
{
    sixel_ycbcr_vk_state_t *c = state;
    unsigned int i;

    sixel_vk_lock();
    if (c->runtime != NULL) {
        sixel_vk_submission_destroy(c->runtime, &c->submission);
        sixel_vk_compute_destroy(c->runtime, &c->stage);
        for (i = 0; i < YCBCR_VK_COUNT; ++i) {
            sixel_vk_buffer_release(c->runtime, &c->buffers[i]);
        }
    } else {
        (void)pthread_mutex_destroy(&c->submission.gate);
    }
    sixel_vk_unlock();
    sixel_allocator_free(allocator, c);
}

static SIXELSTATUS
sixel_ycbcr_vk_create(void **out, sixel_ycbcr_gpu_plan_t const *plan,
    sixel_ycbcr_gpu_taps_t const *taps, sixel_allocator_t *allocator)
{
    sixel_ycbcr_vk_state_t *c;
    sixel_vk_buffer_t const *binding[YCBCR_VK_COUNT];
    VkDeviceSize sizes[YCBCR_VK_COUNT];
    VkCommandBuffer command;
    SIXELSTATUS status = SIXEL_NOT_IMPLEMENTED;
    unsigned int i;

    c = sixel_allocator_calloc(allocator, 1, sizeof(*c));
    if (c == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    if (!sixel_vk_submission_init(&c->submission)) {
        sixel_allocator_free(allocator, c);
        return SIXEL_RUNTIME_ERROR;
    }
    sixel_vk_lock();
    c->runtime = sixel_vk_runtime_get_locked();
    if (c->runtime == NULL) {
        goto end;
    }
    if (c->runtime->properties.limits.maxComputeWorkGroupSize[0] < 256 ||
        c->runtime->properties.limits.maxComputeWorkGroupInvocations < 256 ||
        (plan->output_bytes / 3 + 255) / 256 > 65535u) {
        goto end;
    }
    status = sixel_vk_compute_create(c->runtime, &c->stage,
        ycbcr_vk_source, sizeof(ycbcr_vk_source) / sizeof(ycbcr_vk_source[0]),
        "ycbcr_rgb8", YCBCR_VK_COUNT);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    sizes[YCBCR_VK_INPUT] = (plan->input_bytes + 3u) & ~(VkDeviceSize)3u;
    sizes[YCBCR_VK_OUTPUT] = (plan->output_bytes + 3u) & ~(VkDeviceSize)3u;
    sizes[YCBCR_VK_TAPS] = plan->taps_bytes;
    sizes[YCBCR_VK_PARAMS] = sizeof(sixel_ycbcr_gpu_params_t);
    for (i = 0; i < YCBCR_VK_COUNT; ++i) {
        status = sixel_vk_buffer_reserve(c->runtime, &c->buffers[i], sizes[i],
                                        i != YCBCR_VK_OUTPUT);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        binding[i] = &c->buffers[i];
    }
    status = sixel_vk_compute_bind(c->runtime, &c->stage, binding);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    memcpy(c->buffers[YCBCR_VK_TAPS].mapped, taps, plan->taps_bytes);
    status = sixel_vk_submission_begin(c->runtime, &c->submission, &command);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    sixel_vk_buffer_record_upload(command, &c->buffers[YCBCR_VK_TAPS],
                                  plan->taps_bytes);
    status = sixel_vk_submission_submit_and_wait(c->runtime, &c->submission,
                                                 command);
end:
    sixel_vk_unlock();
    if (SIXEL_FAILED(status)) {
        sixel_ycbcr_vk_destroy(c, allocator);
        return status;
    }
    *out = c;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_ycbcr_vk_run(void *state, sixel_ycbcr_gpu_plan_t const *plan,
    sixel_ycbcr_gpu_params_t const *p, sixel_ycbcr_frame_view_t const *source,
    unsigned char *dst, size_t stride)
{
    sixel_ycbcr_vk_state_t *c = state;
    VkCommandBuffer command;
    SIXELSTATUS status;

    sixel_ycbcr_gpu_upload(c->buffers[YCBCR_VK_INPUT].mapped, p, source);
    memcpy(c->buffers[YCBCR_VK_PARAMS].mapped, p, sizeof(*p));
    sixel_vk_lock();
    status = sixel_vk_submission_begin(c->runtime, &c->submission, &command);
    if (status == SIXEL_OK) {
        sixel_vk_buffer_record_upload(command, &c->buffers[YCBCR_VK_INPUT],
            (plan->input_bytes + 3u) & ~(VkDeviceSize)3u);
        sixel_vk_buffer_record_upload(command, &c->buffers[YCBCR_VK_PARAMS],
                                      sizeof(*p));
        sixel_vk_compute_barrier(command);
        sixel_vk_compute_dispatch(command, &c->stage,
            (uint32_t)((plan->output_bytes / 3 + 255) / 256));
        sixel_vk_compute_download_barrier(command);
        sixel_vk_buffer_record_download(command, &c->buffers[YCBCR_VK_OUTPUT],
            (plan->output_bytes + 3u) & ~(VkDeviceSize)3u);
        status = sixel_vk_submission_submit_and_wait(c->runtime,
            &c->submission, command);
    }
    sixel_vk_unlock();
    if (status == SIXEL_OK) {
        sixel_ycbcr_gpu_readback(dst, stride,
            c->buffers[YCBCR_VK_OUTPUT].mapped, p);
    }
    return status;
}

sixel_ycbcr_gpu_ops_t const sixel_ycbcr_gpu_vulkan = {
    sixel_ycbcr_vk_create, sixel_ycbcr_vk_destroy, sixel_ycbcr_vk_run
};
#endif
