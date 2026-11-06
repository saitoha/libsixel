/* SPDX-License-Identifier: MIT */
#include "tests/_support/processing/gpu-palette/component_common.h"

int
test_gpu_blockcull_batch_opt_in(int argc, char **argv)
{
    gpu_component_fixture_t f;
    int result;

    (void)argc; (void)argv;
    result = 1;
    if (!gpu_component_open(&f,9)) goto end;
    f.request.query_kind = SIXEL_GPU_LOOKUP_QUERY_FRACTIONAL;
    if (f.provider->vtbl->prepare(f.provider,&f.request,&f.device,&f.other)
            != SIXEL_FEATURE_ERROR) goto end;
    result = 0;
end:
    gpu_component_close(&f);
    return result;
}
