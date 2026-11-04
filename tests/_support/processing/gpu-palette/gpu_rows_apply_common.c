/* SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include "gpu_rows_common.h"
#include "src/encode-result.h"
#include "src/dither-policy-none.h"

typedef struct gpu_rows_apply_probe {
    int prefix;
    int calls;
    int reject;
} gpu_rows_apply_probe_t;

static SIXELSTATUS
gpu_rows_apply_ready(sixel_indexed_frame_t *image, int rows, void *context)
{
    gpu_rows_apply_probe_t *probe;
    sixel_indexed_rows_view_t view;
    size_t i;

    probe = context;
    if (rows <= probe->prefix || rows > 512 ||
            sixel_indexed_frame_get_rows(image, probe->prefix,
                rows - probe->prefix, &view) != SIXEL_OK) {
        return SIXEL_LOGIC_ERROR;
    }
    /* Inspect only the newly published interval. Its constant source has an
     * exact palette choice independent of the selected lookup algorithm. */
    for (i = 0; i < (size_t)(rows - probe->prefix) * 512; ++i) {
        if (view.indexes[i] != 0 || view.skip[i] != 0) {
            return SIXEL_BAD_INPUT;
        }
    }
    probe->prefix = rows;
    ++probe->calls;
    if (rows < 512 && sixel_indexed_frame_get_rows(image, rows, 1, &view)
            != SIXEL_FALSE) return SIXEL_LOGIC_ERROR;
    return probe->reject ? SIXEL_BAD_INPUT : SIXEL_OK;
}

int
gpu_rows_apply_test(int reject)
{
    gpu_rows_fixture_t f;
    gpu_rows_apply_probe_t probe;
    sixel_palette_apply_interface_t *apply;
    sixel_dither_policy_interface_t *dither;
    sixel_dither_policy_prepare_request_t dither_request;
    sixel_palette_apply_prepare_request_t request;
    sixel_palette_apply_run_request_t run;
    sixel_wire_palette_t *wire;
    sixel_wire_palette_request_t wire_request;
    sixel_wire_color_definition_t definitions[2] = {
        {0, 2, {7, 0, 0}}, {1, 2, {100, 0, 0}}
    };
    sixel_frame_t *frame;
    sixel_indexed_frame_t *image;
    unsigned char *pixels;
    void *object;
    SIXELSTATUS status;
    int outcome;
    int i;

    apply = NULL;
    dither = NULL;
    wire = NULL;
    frame = NULL;
    image = NULL;
    pixels = NULL;
    memset(&probe, 0, sizeof(probe));
    memset(&request, 0, sizeof(request));
    memset(&run, 0, sizeof(run));
    memset(&wire_request, 0, sizeof(wire_request));
    memset(&dither_request, 0, sizeof(dither_request));
    outcome = gpu_rows_open(&f, 1);
    if (outcome != 0) goto end;
    outcome = 1;
    pixels = calloc(512 * 512, 3);
    ROWS_CHECK(pixels != NULL);
    for (i = 0; i < 512 * 512; ++i) pixels[i * 3] = 17;
    ROWS_CHECK(sixel_frame_new(&frame, f.component.allocator) == SIXEL_OK);
    ROWS_CHECK(sixel_frame_init_borrowed(frame, pixels, 512, 512,
        SIXEL_PIXELFORMAT_RGB888, NULL, -1) == SIXEL_OK);
    sixel_frame_set_colorspace(frame, SIXEL_COLORSPACE_GAMMA);
    object = NULL;
    ROWS_CHECK(sixel_dither_policy_none_8bit_new(f.component.allocator,
        &object) == SIXEL_OK);
    dither = object;
    dither_request.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    dither_request.method_for_scan = SIXEL_SCAN_RASTER;
    dither_request.options.ncolors = 2;
    dither_request.options.method_for_diffuse = SIXEL_DIFFUSE_NONE;
    ROWS_CHECK(dither->vtbl->prepare(dither, &dither_request) == SIXEL_OK);
    wire_request.definitions = definitions;
    wire_request.count = 2;
    wire_request.color_definition = SIXEL_PALETTE_OUTPUT_LEGACY;
    wire_request.colorspace = SIXEL_COLORSPACE_GAMMA;
    ROWS_CHECK(sixel_wire_palette_new(&wire, &wire_request,
        f.component.allocator) == SIXEL_OK);
    request.working_palette = f.component.palette;
    request.dither = dither;
    request.gpu_lookup = f.component.provider;
    request.device = f.device;
    request.output.palette = wire;
    request.output.width = request.output.height = 512;
    request.output.encoded_width = request.output.encoded_height = 512;
    request.output.plane_width = request.output.plane_height = 512;
    request.output.keycolor = -1;
    request.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    request.colorspace = SIXEL_COLORSPACE_GAMMA;
    request.scan = SIXEL_SCAN_RASTER;
    request.worker_threads = 1;
    request.gpu_policy = SIXEL_GPU_POLICY_FORCE;
    object = NULL;
    ROWS_CHECK(sixel_palette_apply_factory_new(f.component.allocator,
        &object) == SIXEL_OK);
    apply = object;
    ROWS_CHECK(apply->vtbl->prepare(apply, &request) == SIXEL_OK);
    ROWS_CHECK(sixel_indexed_frame_new(&image, &request.output,
        f.component.allocator) == SIXEL_OK);
    probe.reject = reject;
    run.source = sixel_frame_as_interface(frame);
    run.result = image;
    run.rows_ready = gpu_rows_apply_ready;
    run.rows_context = &probe;
    status = apply->vtbl->run(apply, &run);
    if (reject) {
        ROWS_CHECK(status == SIXEL_BAD_INPUT && probe.calls == 1 &&
            probe.prefix < 512 &&
            sixel_indexed_frame_get_state(image) == SIXEL_INDEXED_FAILED);
    } else {
        ROWS_CHECK(status == SIXEL_OK && probe.calls == 6 &&
            probe.prefix == 512 &&
            sixel_indexed_frame_get_state(image) == SIXEL_INDEXED_COMPLETE);
    }
    outcome = 0;
end:
    if (apply != NULL) apply->vtbl->unref(apply);
    if (dither != NULL) dither->vtbl->unref(dither);
    sixel_indexed_frame_unref(image);
    sixel_frame_unref(frame);
    sixel_wire_palette_unref(wire);
    free(pixels);
    gpu_rows_close(&f);
    return outcome;
}
