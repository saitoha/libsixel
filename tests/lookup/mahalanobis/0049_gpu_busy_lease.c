/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/palette-application/lookup-policy/mahalanobis.md
 */
#include "../../_support/processing/gpu-palette/component_common.h"
#define CHECK(x) do { if (!(x)) goto end; } while (0)

int
test_component_mahalanobis_busy_lease(int argc, char **argv)
{
    gpu_component_fixture_t f;

    int ok;

    (void)argc;
    (void)argv;
    ok = 0;
    CHECK(gpu_component_open(&f, 3));
    CHECK(f.plan->vtbl->begin(f.plan, &f.binding) == SIXEL_OK);
    ok = f.plan->vtbl->begin(f.plan, &f.second) == SIXEL_LOGIC_ERROR;
end:
    gpu_component_close(&f);
    return ok ? 0 : 1;
}
