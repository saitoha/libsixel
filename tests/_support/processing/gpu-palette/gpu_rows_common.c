/* SPDX-License-Identifier: MIT */
#include "src/dither-policy-gpu.h"
#include "gpu_rows_common.h"
#include "src/lookup-policy-gpu.h"

static int
gpu_rows_cancel(void *context)
{
    gpu_rows_fixture_t *f;

    f = context;
    return f->cancel_after >= 0 && f->calls >= f->cancel_after;
}

SIXELSTATUS
gpu_rows_notify(void *context, int rows)
{
    gpu_rows_fixture_t *f;
    size_t bytes;

    f = context;
    bytes = (size_t)rows * GPU_ROWS_WIDTH;
    if (f->calls >= GPU_ROWS_HEIGHT || rows <= f->prefix ||
            rows > GPU_ROWS_HEIGHT) return SIXEL_LOGIC_ERROR;
    if (memcmp(f->output, f->saved, (size_t)f->prefix * GPU_ROWS_WIDTH)) {
        f->immutable = 0;
    }
    memcpy(f->saved, f->output, bytes);
    f->ends[f->calls++] = rows;
    f->prefix = rows;
    return f->callback_status;
}

int
gpu_rows_open(gpu_rows_fixture_t *f, int kind)
{
    sixel_gpu_lookup_program_view_t view;
    SIXELSTATUS status;
    int i;
    /* The policy gpu_component_open() creates for each fixture kind. */
    static int const policies[] = {
        SIXEL_LUT_POLICY_VPTREE, SIXEL_LUT_POLICY_ORDERED,
        SIXEL_LUT_POLICY_RBC, SIXEL_LUT_POLICY_MAHALANOBIS,
        SIXEL_LUT_POLICY_CERTLUT
    };

    memset(f, 0, sizeof(*f));
    f->cancel_after = -1;
    f->callback_status = SIXEL_OK;
    status = sixel_gpu_device_acquire(&f->device);
    if (status == SIXEL_FEATURE_ERROR) return 77;
    /* A device without a program for this policy refuses the plan. */
    if (status == SIXEL_OK && kind >= 0 && kind <= 4 &&
            !sixel_gpu_lookup_has_program(policies[kind],
                f->device->vtbl->program_format(f->device))) {
        return 77;
    }
    if (status != SIXEL_OK || !gpu_component_open(&f->component, kind)) {
        return 1;
    }
    f->component.request.allow_batch = 1;
    status = f->component.provider->vtbl->prepare(f->component.provider,
        &f->component.request, f->device, &f->component.other);
    if (status != SIXEL_OK) return 1;
    status = f->component.other->vtbl->describe(f->component.other, &view);
    if (status != SIXEL_OK) return 1;
    status = sixel_gpu_program_prepare(f->device, f->component.other,
        sixel_gpu_dither_byte_program(SIXEL_DIFFUSE_NONE), &f->program);
    if (status != SIXEL_OK) return 1;
    for (i = 0; i < GPU_ROWS_PIXELS; ++i) {
        /* Two exact palette choices give an independent index oracle. */
        f->pixels[i * 3] = (i % 3) != 0 ? 255 : 17;
        f->history[i * 3] = 17;
        f->valid[i] = 1;
        f->transparent[i] = (i % 7) == 0;
    }
    f->request.program = f->program;
    f->request.test_hooks = &f->hooks;
    f->request.policy = SIXEL_GPU_POLICY_FORCE;
    f->request.dest = f->output;
    f->request.pixels = f->pixels;
    f->request.pixel_count = GPU_ROWS_PIXELS;
    f->request.width = GPU_ROWS_WIDTH;
    f->request.height = GPU_ROWS_HEIGHT;
    f->request.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    f->request.palette = view.palette;
    f->request.palette_size = view.palette_size;
    f->request.palette_depth = 3;
    f->request.ncolors = 2;
    f->request.method_for_diffuse = SIXEL_DIFFUSE_NONE;
    f->request.method_for_scan = SIXEL_SCAN_RASTER;
    f->request.accumulation_result_mask = f->mask;
    f->request.accumulation_result_mask_size = sizeof(f->mask);
    f->request.rows_per_chunk = 6;
    f->request.rows_ready = gpu_rows_notify;
    f->request.rows_context = f;
    f->request.cancel = gpu_rows_cancel;
    f->request.cancel_context = f;
    return 0;
}

SIXELSTATUS
gpu_rows_run(gpu_rows_fixture_t *f)
{
    f->calls = f->prefix = 0;
    f->immutable = 1;
    memset(f->output, 90, sizeof(f->output));
    /* The result mask starts with source transparency, as in PaletteApply. */
    memset(f->mask, 0, sizeof(f->mask));
    if (f->request.transparent_mask != NULL) {
        memcpy(f->mask, f->request.transparent_mask, sizeof(f->mask));
    }
    f->status = sixel_gpu_palette_apply_prepared(&f->request);
    return f->status;
}

int
gpu_rows_worker(void *context)
{
    (void)gpu_rows_run(context);
    return 0;
}

void
gpu_rows_close(gpu_rows_fixture_t *f)
{
    sixel_gpu_program_destroy(f->program);
    if (f->device != NULL) f->device->vtbl->unref(f->device);
    gpu_component_close(&f->component);
}
