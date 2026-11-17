/* SPDX-License-Identifier: MIT */
#include "fixture.h"

/* Compare the same diffusion result with CPU and device history. Vary both
 * retained RGB and validity across rows so a partial frame cannot silently
 * read a packed rectangle at the wrong canvas origin or stride. This checks
 * indexes and keep reasons; discrete-GPU transfer cost is measured separately.
 */
int
test_diffusion_gpu_retained(int method, int partial, int deferred)
{
    lso2_gpu_fixture_t f;
    sixel_retained_gpu_read_view_t gpu;
    unsigned char indexes[LSO2_GPU_PIXELS], skip[LSO2_GPU_PIXELS];
    unsigned char *rgb, *valid;
    size_t i, cells;
    int outcome, paints, keeps;
    SIXELSTATUS status;

    outcome = lso2_gpu_fixture_open_direct(&f, method);
    if (outcome != 0) goto end;
    outcome = 1;
    LSO2_GPU_CHECK(lso2_gpu_fixture_baseline(&f, partial) == SIXEL_OK);
    sixel_retained_read_unref(f.read);
    f.read = NULL;
    LSO2_GPU_CHECK(sixel_retained_plane_legacy_map(f.plane, &rgb, &valid)
        == SIXEL_OK);
    cells = (size_t)f.prepare.output.plane_width *
        f.prepare.output.plane_height;
    for (i = 0; i < cells; ++i) {
        rgb[3 * i] = (unsigned char)((i * 7) % 52);
        rgb[3 * i + 1] = (unsigned char)((i * 11) % 52);
        rgb[3 * i + 2] = (unsigned char)((i * 13) % 52);
        valid[i] = i % 3 != 0;
    }
    sixel_retained_plane_legacy_unmap(f.plane);
    LSO2_GPU_CHECK(f.plane->vtbl->acquire_read(f.plane, &f.read) == SIXEL_OK);
    f.run.baseline = f.read;
    f.run.delta.threshold = 24;
    LSO2_GPU_CHECK(lso2_gpu_fixture_prepare(&f) == SIXEL_OK);
    LSO2_GPU_CHECK((deferred ? lso2_gpu_fixture_deferred(&f) :
        lso2_gpu_fixture_run(&f)) == SIXEL_OK);
    memcpy(indexes, f.image->indexes, sizeof(indexes));
    memcpy(skip, f.image->skip, sizeof(skip));
    paints = keeps = 0;
    for (i = 0; i < LSO2_GPU_PIXELS; ++i) {
        paints += skip[i] == SIXEL_INDEXED_PAINT;
        keeps += skip[i] != SIXEL_INDEXED_PAINT;
    }
    LSO2_GPU_CHECK(paints != 0 && keeps != 0);
    sixel_retained_read_unref(f.read);
    f.read = NULL;
    status = f.plane->vtbl->set_device(f.plane, f.device);
    if (status == SIXEL_FEATURE_ERROR) {
        outcome = 77;
        goto end;
    }
    LSO2_GPU_CHECK(status == SIXEL_OK);
    LSO2_GPU_CHECK(f.plane->vtbl->acquire_read(f.plane, &f.read) == SIXEL_OK);
    LSO2_GPU_CHECK(sixel_retained_read_get_gpu_view(f.read, f.device, &gpu)
        == SIXEL_OK);
    f.run.baseline = f.read;
    LSO2_GPU_CHECK((deferred ? lso2_gpu_fixture_deferred(&f) :
        lso2_gpu_fixture_run(&f)) == SIXEL_OK);
    LSO2_GPU_CHECK(memcmp(indexes, f.image->indexes, sizeof(indexes)) == 0);
    LSO2_GPU_CHECK(memcmp(skip, f.image->skip, sizeof(skip)) == 0);
    outcome = 0;
end:
    lso2_gpu_fixture_close(&f);
    return outcome;
}
