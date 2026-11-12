/* SPDX-License-Identifier: MIT */
/* Partial byte tiles must cover each streamed row exactly once. */
#include "tests/_support/processing/gpu-palette/gpu_rows_common.h"

int
test_gpu_blockcull_vulkan_streamed_tiles(int argc, char **argv)
{
    static int const chunks[] = {1, 6, 9};
    gpu_rows_fixture_t f;
    sixel_gpu_device_interface_t *device;
    SIXELSTATUS status;
    int outcome, i, chunk;

    (void)argc;
    (void)argv;
    device = NULL;
    status = sixel_gpu_device_acquire(&device);
    if (status == SIXEL_FEATURE_ERROR) return 77;
    if (status != SIXEL_OK) return 1;
    outcome = strcmp(device->vtbl->program_format(device),
        "vulkan.lookup-rgb8.v1") == 0 ? 0 : 77;
    device->vtbl->unref(device);
    if (outcome != 0) return outcome;
    outcome = gpu_rows_open(&f, 9);
    if (outcome != 0) {
        gpu_rows_close(&f);
        return outcome;
    }
    outcome = 1;
    for (chunk = 0; chunk < 3; ++chunk) {
        f.request.rows_per_chunk = chunks[chunk];
        memset(&f.hooks, 0, sizeof(f.hooks));
        ROWS_CHECK(gpu_rows_run(&f) == SIXEL_OK);
        ROWS_CHECK(f.immutable && f.prefix == GPU_ROWS_HEIGHT);
        ROWS_CHECK(f.calls == (GPU_ROWS_HEIGHT + chunks[chunk] - 1) /
            chunks[chunk]);
        for (i = 0; i < GPU_ROWS_PIXELS; ++i) {
            ROWS_CHECK(f.output[i] == (unsigned char)(i % 3 != 0));
        }
        for (i = 0; i < f.calls - 1; ++i) {
            ROWS_CHECK(f.ends[i] == (i + 1) * chunks[chunk]);
        }
    }
    outcome = 0;
end:
    gpu_rows_close(&f);
    return outcome;
}
