/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/execution-map/README.md
 */
#include "../../_support/processing/gpu-palette/component_common.h"
#define CHECK(x) do { if (!(x)) goto end; } while (0)

int
test_component_ordered_completed_reuse(int argc, char **argv)
{
    gpu_component_fixture_t f;
    sixel_gpu_lookup_binding_view_t view;
    int ok;

    (void)argc;
    (void)argv;
    ok = 0;
    CHECK(gpu_component_open(&f, 1));
    CHECK(f.plan->vtbl->begin(f.plan, &f.binding) == SIXEL_OK);
    CHECK(f.binding->vtbl->finish(f.binding, SIXEL_OK) == SIXEL_OK);
    f.binding->vtbl->unref(f.binding);
    f.binding = NULL;
    CHECK(f.plan->vtbl->begin(f.plan, &f.binding) == SIXEL_OK);
    CHECK(f.binding->vtbl->get_view(f.binding, &view) == SIXEL_OK);
    ok = !view.needs_build;
end:
    gpu_component_close(&f);
    return ok ? 0 : 1;
}
