/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/execution-map/README.md
 */
#include <string.h>
#include "../../_support/processing/gpu-palette/component_common.h"
#include "src/dither.h"

int
test_cpu_palette_snapshot(int argc, char **argv)
{
    gpu_component_fixture_t fixture;
    sixel_dither_t *dither;
    sixel_lookup_policy_interface_t *lookup;
    sixel_palette_apply_prepare_request_t preparers;
    sixel_palette_entries_request_t entries;
    unsigned char swapped[6] = {255, 0, 0, 17, 0, 0};
    unsigned char query[3] = {17, 0, 0};
    int ok;

    (void)argc;
    (void)argv;
    dither = NULL;
    lookup = NULL;
    ok = 0;
    memset(&preparers, 0, sizeof(preparers));
    if (!gpu_component_open(&fixture, 0) ||
            sixel_dither_new(&dither, 2, fixture.allocator) != SIXEL_OK) {
        goto end;
    }
    sixel_dither_set_lut_policy(dither, SIXEL_LUT_POLICY_NONE);
    preparers.gpu_policy = SIXEL_GPU_POLICY_OFF;
    if (sixel_dither_lookup_preparers(dither, &preparers) != SIXEL_OK) goto end;
    if (preparers.cpu_lookup->vtbl->prepare(preparers.cpu_lookup,
            &fixture.request, &lookup) != SIXEL_OK) goto end;
    preparers.cpu_lookup->vtbl->unref(preparers.cpu_lookup);
    preparers.cpu_lookup = NULL;
    sixel_dither_unref(dither);
    dither = NULL;
    entries.entries = swapped;
    entries.colors = 2;
    entries.depth = 3;
    if (fixture.palette->vtbl->init_entries(fixture.palette, &entries)
            != SIXEL_OK) goto end;
    ok = lookup->vtbl->map_pixel(lookup, query) == 0;
end:
    if (preparers.cpu_lookup != NULL) {
        preparers.cpu_lookup->vtbl->unref(preparers.cpu_lookup);
    }
    if (lookup != NULL) lookup->vtbl->unref(lookup);
    sixel_dither_unref(dither);
    gpu_component_close(&fixture);
    return ok ? 0 : 1;
}
