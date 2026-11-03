/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/execution-map/README.md
 */
#include "../../_support/processing/gpu-palette/component_common.h"
#define CHECK(x) do { if (!(x)) goto end; } while (0)

int
test_component_vptree_live_plan_isolation(int argc, char **argv)
{
    gpu_component_fixture_t f;
    sixel_gpu_lookup_program_view_t view;
    int ok;

    (void)argc;
    (void)argv;
    ok = 0;
    CHECK(gpu_component_open(&f, 0));
    CHECK(gpu_component_set_palette(&f, 91));
    CHECK(f.provider->vtbl->prepare(f.provider, &f.request,
        &f.device, &f.other) == SIXEL_OK);
    CHECK(f.plan->vtbl->describe(f.plan, &view) == SIXEL_OK);
    ok = view.palette[0] == 17;
end:
    gpu_component_close(&f);
    return ok ? 0 : 1;
}
