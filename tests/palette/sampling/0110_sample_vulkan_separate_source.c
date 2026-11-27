/* SPDX-License-Identifier: MIT */
/* A separate RGB palette source scans on Vulkan without a dither owner. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <string.h>
#include <sixel.h>
#include "src/filter-factory.h"
#include "src/filter-sample.h"
#include "src/gpu-statistics.h"
#include "tests/_support/processing/filter/filter_test_common.h"

int
test_sample_vulkan_separate_source(int argc, char **argv)
{
#if defined(HAVE_VULKAN)
    enum { WIDTH = 1024, HEIGHT = 512 };
    sixel_allocator_t *allocator;
    sixel_frame_t *source;
    sixel_filter_t *filter;
    sixel_sample_stream_t cpu;
    sixel_sample_stream_t gpu;
    sixel_sample_stream_t *result;
    sixel_filter_sample_config_t config = {0};
    sixel_gpu_statistics_t before;
    sixel_gpu_statistics_t after;
    SIXELSTATUS status;
    size_t bytes;
    int run;
    int answer;

    (void)argc;
    (void)argv;
    if (!sixel_filter_sample_vulkan_choose(SIXEL_GPU_POLICY_AUTO))
        return 77;
    allocator = NULL;
    source = NULL;
    filter = NULL;
    answer = 1;
    sixel_sample_stream_init(&cpu);
    sixel_sample_stream_init(&gpu);
    config.policy = SIXEL_PALETTE_SAMPLING_ADAPTIVE_GRID;
    config.source = SIXEL_PALETTE_SAMPLING_SOURCE_LOADED_FRAME;
    config.reqcolors = 256;
    config.quality_mode = SIXEL_QUALITY_AUTO;
    config.share_gpu_input = 0;
    status = make_allocator(&allocator);
    if (status != SIXEL_OK) goto end;
    status = make_rgb_frame(allocator, WIDTH, HEIGHT, &source);
    if (status != SIXEL_OK) goto end;
    for (run = 0; run < 2; ++run) {
        result = run == 0 ? &cpu : &gpu;
        config.gpu_policy = run == 0 ? SIXEL_GPU_POLICY_OFF
            : SIXEL_GPU_POLICY_AUTO;
        status = sixel_filter_factory_create_by_kind(
            SIXEL_FILTER_KIND_SAMPLE, &config, &filter);
        if (status != SIXEL_OK) goto end;
        sixel_filter_bind_input(filter, &source,
            SIXEL_PIXELFORMAT_RGB888, SIXEL_COLORSPACE_SRGB);
        sixel_filter_bind_sample_output(filter, result,
            SIXEL_PIXELFORMAT_RGB888, SIXEL_COLORSPACE_SRGB);
        if (run == 1 &&
                sixel_gpu_get_statistics(&before) != SIXEL_OK)
            goto end;
        status = sixel_filter_run(filter, allocator, NULL);
        if (status != SIXEL_OK) goto end;
        if (run == 1 &&
                sixel_gpu_get_statistics(&after) != SIXEL_OK)
            goto end;
        sixel_filter_free(filter);
        filter = NULL;
    }
    if (after.dispatches != before.dispatches + 1 ||
            cpu.frame == NULL || gpu.frame == NULL ||
            sixel_frame_get_width(cpu.frame) !=
                sixel_frame_get_width(gpu.frame) ||
            sixel_frame_get_height(cpu.frame) !=
                sixel_frame_get_height(gpu.frame))
        goto end;
    bytes = (size_t)sixel_frame_get_width(cpu.frame) *
        (size_t)sixel_frame_get_height(cpu.frame) * 3u;
    if (memcmp(sixel_frame_get_pixels(cpu.frame),
            sixel_frame_get_pixels(gpu.frame), bytes) != 0)
        goto end;
    answer = 0;
end:
    sixel_filter_free(filter);
    sixel_sample_stream_dispose(&cpu);
    sixel_sample_stream_dispose(&gpu);
    sixel_frame_unref(source);
    sixel_allocator_unref(allocator);
    return answer;
#else
    (void)argc;
    (void)argv;
    return 77;
#endif
}
