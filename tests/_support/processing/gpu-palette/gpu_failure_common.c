/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/dither.h"
#include "ordered_state_common.h"

#define FAULT_CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "Metal fault %d policy %d line %d: %s\n", \
        fault, lookup, __LINE__, #x); goto end; } } while (0)

/* Rebuild failures must invalidate the identity, including failures after
 * the real command completed. Admission rejection cannot exercise this. */
int
gpu_failure_test(int lookup, int fault)
{
    ordered_state_fixture_t fixture;
    sixel_gpu_palette_test_hooks_t hooks;
    sixel_dither_t *cpu, *gpu;
    sixel_index_t *a, *b;
    unsigned char history[ORDERED_STATE_PIXELS * 3];
    unsigned char valid[ORDERED_STATE_PIXELS];
    unsigned char saved[sizeof(history)];
    unsigned char const *mask_a, *mask_b;
    size_t size_a, size_b;
    SIXELSTATUS status;
    int i, ok;

    cpu = gpu = NULL;
    a = b = NULL;
    ok = 0;
    memset(&hooks, 0, sizeof(hooks));
    ordered_state_init(&fixture, 33, 1);
    fixture.request.lut_policy = lookup;
    fixture.request.test_hooks = &hooks;
    FAULT_CHECK(ordered_state_check(&fixture));
    ordered_state_init(&fixture, 33, 2);
    fixture.request.lut_policy = lookup;
    fixture.request.test_hooks = &hooks;
    hooks.fault = fault;
    status = sixel_gpu_palette_apply(&fixture.request);
    FAULT_CHECK(status == (fault == 1 ? SIXEL_BAD_ALLOCATION
        : SIXEL_RUNTIME_ERROR));
    FAULT_CHECK(hooks.entered && hooks.rebuild && !hooks.published);
    fixture.request.policy = SIXEL_GPU_POLICY_AUTO;
    FAULT_CHECK(sixel_gpu_palette_apply(&fixture.request) == SIXEL_FALSE);
    FAULT_CHECK(hooks.rebuild && !hooks.published);
    hooks.fault = 0;
    fixture.request.policy = SIXEL_GPU_POLICY_FORCE;
    FAULT_CHECK(ordered_state_check(&fixture));
    FAULT_CHECK(hooks.rebuild && hooks.published);
    FAULT_CHECK(ordered_state_check(&fixture));
    FAULT_CHECK(!hooks.rebuild && hooks.published);

    /* Exercise the actual dither caller's AUTO fallback, not a manual CPU
     * retry. Retained inputs are immutable and indexes/masks must match an
     * independently replayed CPU application after an execution failure. */
    FAULT_CHECK(sixel_dither_new(&cpu, 34, NULL) == SIXEL_OK);
    FAULT_CHECK(sixel_dither_new(&gpu, 34, NULL) == SIXEL_OK);
    for (i = 0; i < (int)sizeof(history); ++i) history[i] = fixture.pixels[i];
    for (i = 0; i < ORDERED_STATE_PIXELS; ++i) valid[i] = i % 3 != 0;
    memcpy(saved, history, sizeof(saved));
    for (i = 0; i < 2; ++i) {
        sixel_dither_set_palette(i ? gpu : cpu, fixture.palette);
        sixel_dither_set_pixelformat(i ? gpu : cpu, SIXEL_PIXELFORMAT_RGB888);
        sixel_dither_set_lut_policy(i ? gpu : cpu,
            i ? lookup : SIXEL_LUT_POLICY_NONE);
        sixel_dither_set_diffusion_type(i ? gpu : cpu, SIXEL_DIFFUSE_NONE);
        sixel_dither_set_optimize_palette(i ? gpu : cpu, 0);
        sixel_dither_set_transparent(i ? gpu : cpu, 33);
        (i ? gpu : cpu)->keycolor_reserved = 1;
        sixel_dither_set_pipeline_accumulation_result_enabled(i ? gpu : cpu,
            1);
        sixel_dither_set_pipeline_accumulation_buffer_hint(i ? gpu : cpu,
            history, sizeof(history), valid, sizeof(valid),
            ORDERED_STATE_WIDTH, ORDERED_STATE_HEIGHT, 0, 0,
            ORDERED_STATE_WIDTH, ORDERED_STATE_HEIGHT, 33, 1, 8,
            SIXEL_6DELTA_ERROR_DIFFUSE);
    }
    cpu->gpu_policy = SIXEL_GPU_POLICY_OFF;
    gpu->gpu_policy = SIXEL_GPU_POLICY_AUTO;
    gpu->gpu_test_hooks = &hooks;
    /* AUTO admission is size based. This test deliberately disables only
     * the cutoff, retaining normal failure/fallback processing. */
    gpu->gpu_palette_threshold = 0;
    hooks.fault = fault;
    hooks.entered = 0;
    a = sixel_dither_apply_palette(cpu, fixture.pixels,
        ORDERED_STATE_WIDTH, ORDERED_STATE_HEIGHT);
    b = sixel_dither_apply_palette(gpu, fixture.pixels,
        ORDERED_STATE_WIDTH, ORDERED_STATE_HEIGHT);
    FAULT_CHECK(a != NULL && b != NULL && hooks.entered && !hooks.published);
    mask_a = sixel_dither_get_pipeline_accumulation_result_mask(cpu, &size_a);
    mask_b = sixel_dither_get_pipeline_accumulation_result_mask(gpu, &size_b);
    FAULT_CHECK(mask_a != NULL && mask_b != NULL);
    FAULT_CHECK(size_a == sizeof(valid) && size_a == size_b &&
        !memcmp(a, b, sizeof(fixture.output)) &&
        !memcmp(mask_a, mask_b, size_a) &&
        !memcmp(history, saved, sizeof(history)));
    sixel_allocator_free(gpu->allocator, b);
    b = NULL;
    hooks.fault = 0;
    gpu->gpu_policy = SIXEL_GPU_POLICY_FORCE;
    /* Accumulation hints are per application, including a recovered frame. */
    sixel_dither_set_pipeline_accumulation_buffer_hint(gpu,
        history, sizeof(history), valid, sizeof(valid),
        ORDERED_STATE_WIDTH, ORDERED_STATE_HEIGHT, 0, 0,
        ORDERED_STATE_WIDTH, ORDERED_STATE_HEIGHT, 33, 1, 8,
        SIXEL_6DELTA_ERROR_DIFFUSE);
    b = sixel_dither_apply_palette(gpu, fixture.pixels,
        ORDERED_STATE_WIDTH, ORDERED_STATE_HEIGHT);
    FAULT_CHECK(b != NULL && hooks.published);
    mask_b = sixel_dither_get_pipeline_accumulation_result_mask(gpu, &size_b);
    FAULT_CHECK(mask_b != NULL && size_b == size_a);
    FAULT_CHECK(!memcmp(a, b, sizeof(fixture.output)));
    FAULT_CHECK(!memcmp(mask_a, mask_b, size_a));
    FAULT_CHECK(!memcmp(history, saved, sizeof(history)));
    ok = 1;
end:
    if (cpu != NULL) sixel_allocator_free(cpu->allocator, a);
    if (gpu != NULL) sixel_allocator_free(gpu->allocator, b);
    sixel_dither_unref(cpu);
    sixel_dither_unref(gpu);
    return ok ? 0 : 1;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
