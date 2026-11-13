/* SPDX-License-Identifier: MIT */
/* Verify gpu r256 refinement uses a byte table. */
#include <string.h>
#include "tests/_support/processing/gpu-palette/component_common.h"
#include "src/lookup-policy-gpu.h"
#include "../../_support/processing/gpu-palette/ordered_state_common.h"

static int
fhedt_vulkan_compact_contract(void)
{
    gpu_component_fixture_t fixture;
    sixel_gpu_device_interface_t *device;
    sixel_gpu_lookup_program_view_t view;
    sixel_lookup_policy_configuration_t configuration;
    uint32_t const *constants;
    int ok;

    device = NULL;
    ok = 0;
    memset(&fixture, 0, sizeof(fixture));
    memset(&configuration, 0, sizeof(configuration));
    if (sixel_gpu_device_acquire(&device) != SIXEL_OK ||
            !gpu_component_open(&fixture, 12)) goto end;
    fixture.provider->vtbl->unref(fixture.provider);
    fixture.provider = NULL;
    configuration.kind = SIXEL_LUT_POLICY_FHEDT;
    configuration.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    configuration.fhedt_resolution = 256;
    configuration.fhedt_refine = configuration.fhedt_use_dist2 = 1;
    configuration.fhedt_weights[0] = configuration.fhedt_weights[1] =
        configuration.fhedt_weights[2] = 1;
    if (sixel_gpu_lookup_select(fixture.allocator, &configuration,
            &fixture.provider) != SIXEL_OK ||
            fixture.provider->vtbl->prepare(fixture.provider,
                &fixture.request, device, &fixture.other) != SIXEL_OK ||
            fixture.other->vtbl->describe(fixture.other, &view) != SIXEL_OK)
        goto end;
    /* The final builder and query descriptor must agree on byte labels;
     * word scratch for the three EDT passes remains private to this plan. */
    constants = view.constants;
    ok = view.constants_size == 6 * sizeof(uint32_t) && constants[5] == 1 &&
        view.build_pass_count == 6 &&
        strcmp(view.build_passes[5].entry_point, "fhedt_compact") == 0;
end:
    gpu_component_close(&fixture);
    if (device != NULL) device->vtbl->unref(device);
    return ok ? 0 : 1;
}

int
test_fh_gpu_r256_compact_table(int argc, char **argv)
{
    ordered_state_fixture_t fixture;
    sixel_gpu_palette_test_hooks_t hooks;

    (void)argc;
    (void)argv;
    ordered_state_init(&fixture, 2, 1);
    memset(&hooks, 0, sizeof(hooks));
    fixture.request.test_hooks = &hooks;
    fixture.request.lut_policy = SIXEL_LUT_POLICY_FHEDT;
    fixture.request.fhedt_configured = 1;
    fixture.request.fhedt_resolution = 256;
    fixture.request.fhedt_refine = 1;
    fixture.request.fhedt_use_dist2 = 1;
    fixture.request.fhedt_weights[0] = 1;
    fixture.request.fhedt_weights[1] = 1;
    fixture.request.fhedt_weights[2] = 1;
    if (sixel_gpu_palette_apply(&fixture.request) != SIXEL_OK ||
            !hooks.published) return 1;
    if (hooks.kernel == 9) return fhedt_vulkan_compact_contract();
    if (hooks.kernel != 7) return 1;
    return hooks.fhedt_table_entry_bytes == 1U ? 0 : 1;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
