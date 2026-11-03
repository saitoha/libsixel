/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/execution-map/README.md
 */
#include "../../_support/processing/gpu-palette/component_common.h"
#define CHECK(x) do { if (!(x)) goto end; } while (0)

int
test_component_ordered_binding_lifetime(int argc, char **argv)
{
    gpu_component_fixture_t f;
    sixel_gpu_lookup_binding_view_t view;
    int ok;

    (void)argc;
    (void)argv;
    ok = 0;
    CHECK(gpu_component_open(&f, 1));
    CHECK(f.plan->vtbl->begin(f.plan, &f.binding) == SIXEL_OK);
    f.provider->vtbl->unref(f.provider);
    f.provider = NULL;
    f.plan->vtbl->unref(f.plan);
    f.plan = NULL;
    ok = f.binding->vtbl->get_view(f.binding, &view) == SIXEL_OK;
end:
    gpu_component_close(&f);
    return ok ? 0 : 1;
}
