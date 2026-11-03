/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/execution-map/README.md
 */
#include "../../_support/processing/gpu-palette/component_common.h"
#define CHECK(x) do { if (!(x)) goto end; } while (0)

int
test_component_ordered_float_decline(int argc, char **argv)
{
    gpu_component_fixture_t f;

    int ok;

    (void)argc;
    (void)argv;
    ok = 0;
    CHECK(gpu_component_open(&f, 1));
    f.request.pixelformat = SIXEL_PIXELFORMAT_RGBFLOAT32;
    ok = f.provider->vtbl->prepare(f.provider, &f.request,
        &f.device, &f.other) == SIXEL_FEATURE_ERROR;
end:
    gpu_component_close(&f);
    return ok ? 0 : 1;
}
