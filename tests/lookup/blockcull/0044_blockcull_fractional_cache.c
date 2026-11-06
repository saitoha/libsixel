/* SPDX-License-Identifier: MIT */
#include "tests/_support/processing/gpu-palette/component_common.h"

/* Precision belongs to the immutable plan's cache key. A byte plan held
 * across fractional preparation must remain usable with its byte ABI. */
int
test_gpu_blockcull_fractional_cache(int argc, char **argv)
{
    gpu_component_fixture_t f;
    sixel_gpu_lookup_program_view_t byte_view, fractional_view;
    int result;

    (void)argc;
    (void)argv;
    result = 1;
    if (!gpu_component_open(&f, 9)) goto end;
    f.request.allow_batch = 1;
    f.request.query_kind = SIXEL_GPU_LOOKUP_QUERY_FRACTIONAL;
    if (f.provider->vtbl->prepare(f.provider, &f.request, &f.device,
            &f.other) != SIXEL_OK) goto end;
    if (f.plan == f.other) goto end;
    if (f.plan->vtbl->describe(f.plan, &byte_view) != SIXEL_OK) goto end;
    if (f.other->vtbl->describe(f.other, &fractional_view) != SIXEL_OK)
        goto end;
    if (byte_view.query_kind != SIXEL_GPU_LOOKUP_QUERY_BYTE ||
            fractional_view.query_kind != SIXEL_GPU_LOOKUP_QUERY_FRACTIONAL)
        goto end;
    result = 0;
end:
    gpu_component_close(&f);
    return result;
}
