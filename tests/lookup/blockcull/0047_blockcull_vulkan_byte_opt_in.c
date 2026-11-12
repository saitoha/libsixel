/* SPDX-License-Identifier: MIT */
/* Byte batch preparation must reject a caller that only accepts points. */
#include <string.h>
#include "tests/_support/processing/gpu-palette/component_common.h"
#include "src/gpu-program.h"

int
test_gpu_blockcull_vulkan_byte_opt_in(int argc, char **argv)
{
    gpu_component_fixture_t f;
    sixel_gpu_device_interface_t *device;
    SIXELSTATUS status;
    int outcome;

    (void)argc;
    (void)argv;
    device = NULL;
    memset(&f, 0, sizeof(f));
    status = sixel_gpu_device_acquire(&device);
    outcome = status == SIXEL_FEATURE_ERROR ? 77 : 1;
    if (status != SIXEL_OK) goto end;
    if (strcmp(device->vtbl->program_format(device),
            "vulkan.lookup-rgb8.v1") != 0) {
        outcome = 77;
        goto end;
    }
    if (!gpu_component_open(&f, 9)) goto end;
    if (f.provider->vtbl->prepare(f.provider, &f.request, device, &f.other)
            != SIXEL_FEATURE_ERROR || f.other != NULL) goto end;
    f.request.allow_batch = 1;
    if (f.provider->vtbl->prepare(f.provider, &f.request, device, &f.other)
            != SIXEL_OK || f.other == NULL) goto end;
    outcome = 0;
end:
    gpu_component_close(&f);
    if (device != NULL) device->vtbl->unref(device);
    return outcome;
}
