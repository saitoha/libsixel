/* SPDX-License-Identifier: MIT */
#include <stddef.h>
#include <string.h>
#include "component_common.h"
#include "src/encode-result.h"
#include "src/dither-policy-none.h"

typedef struct admission_probe {
    sixel_lookup_preparer_interface_t cpu;
    sixel_gpu_lookup_preparer_interface_t gpu;
    int cpu_calls;
    int gpu_calls;
    SIXELSTATUS gpu_status;
} admission_probe_t;

static void
admission_cpu_ref(sixel_lookup_preparer_interface_t *self)
{
    (void)self;
}

static SIXELSTATUS
admission_cpu_prepare(sixel_lookup_preparer_interface_t const *self,
    sixel_lookup_execution_request_t const *request,
    sixel_lookup_policy_interface_t **result)
{
    admission_probe_t *probe;

    (void)request;
    probe = (admission_probe_t *)self;
    ++probe->cpu_calls;
    *result = NULL;
    /* A distinctive failure proves that this capability was invoked. */
    return SIXEL_BAD_INPUT;
}

static void
admission_gpu_ref(sixel_gpu_lookup_preparer_interface_t *self)
{
    (void)self;
}

static SIXELSTATUS
admission_gpu_prepare(sixel_gpu_lookup_preparer_interface_t const *self,
    sixel_lookup_execution_request_t const *request,
    sixel_gpu_device_interface_t *device,
    sixel_gpu_lookup_plan_interface_t **result)
{
    admission_probe_t *probe;

    (void)request;
    (void)device;
    probe = (admission_probe_t *)((unsigned char *)self
        - offsetof(admission_probe_t, gpu));
    ++probe->gpu_calls;
    *result = NULL;
    return probe->gpu_status;
}

static sixel_lookup_preparer_vtbl_t const admission_cpu_vtbl = {
    admission_cpu_ref, admission_cpu_ref, admission_cpu_prepare
};
static sixel_gpu_lookup_preparer_vtbl_t const admission_gpu_vtbl = {
    admission_gpu_ref, admission_gpu_ref, admission_gpu_prepare
};

int
lookup_admission_test(int policy, size_t threshold, SIXELSTATUS gpu_status,
    SIXELSTATUS expected_status, int expected_cpu, int expected_gpu)
{
    gpu_component_fixture_t fixture;
    admission_probe_t probe;
    sixel_palette_apply_interface_t *apply;
    sixel_dither_policy_interface_t *dither;
    sixel_dither_policy_prepare_request_t dither_request;
    sixel_palette_apply_prepare_request_t request;
    sixel_wire_palette_t *wire;
    sixel_wire_palette_request_t wire_request;
    sixel_wire_color_definition_t definitions[2] = {
        {0, 2, {0, 0, 0}}, {1, 2, {100, 0, 0}}
    };
    void *object;
    SIXELSTATUS status;
    int ok;

    apply = NULL;
    dither = NULL;
    wire = NULL;
    ok = 0;
    memset(&probe, 0, sizeof(probe));
    memset(&request, 0, sizeof(request));
    memset(&wire_request, 0, sizeof(wire_request));
    memset(&dither_request, 0, sizeof(dither_request));
    if (!gpu_component_open(&fixture, 0)) goto end;
    probe.cpu.vtbl = &admission_cpu_vtbl;
    probe.gpu.vtbl = &admission_gpu_vtbl;
    probe.gpu_status = gpu_status;
    object = NULL;
    if (sixel_dither_policy_none_8bit_new(fixture.allocator, &object)
            != SIXEL_OK) goto end;
    dither = object;
    dither_request.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    dither_request.method_for_scan = SIXEL_SCAN_RASTER;
    dither_request.options.ncolors = 2;
    dither_request.options.method_for_diffuse = SIXEL_DIFFUSE_NONE;
    if (dither->vtbl->prepare(dither, &dither_request) != SIXEL_OK) goto end;
    wire_request.definitions = definitions;
    wire_request.count = 2;
    wire_request.color_definition = SIXEL_PALETTE_OUTPUT_LEGACY;
    wire_request.colorspace = SIXEL_COLORSPACE_GAMMA;
    if (sixel_wire_palette_new(&wire, &wire_request, fixture.allocator)
            != SIXEL_OK) goto end;
    request.working_palette = fixture.palette;
    request.dither = dither;
    request.cpu_lookup = &probe.cpu;
    request.gpu_lookup = &probe.gpu;
    request.device = &fixture.device;
    request.output.palette = wire;
    request.output.width = request.output.height = 1;
    request.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    request.colorspace = SIXEL_COLORSPACE_GAMMA;
    request.scan = SIXEL_SCAN_RASTER;
    request.worker_threads = 1;
    request.gpu_policy = policy;
    request.gpu_auto_threshold = threshold;
    object = NULL;
    if (sixel_palette_apply_factory_new(fixture.allocator, &object)
            != SIXEL_OK) goto end;
    apply = object;
    status = apply->vtbl->prepare(apply, &request);
    ok = status == expected_status && probe.cpu_calls == expected_cpu
        && probe.gpu_calls == expected_gpu;
end:
    if (apply != NULL) apply->vtbl->unref(apply);
    if (dither != NULL) dither->vtbl->unref(dither);
    sixel_wire_palette_unref(wire);
    gpu_component_close(&fixture);
    return ok ? 0 : 1;
}
