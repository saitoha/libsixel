/* SPDX-License-Identifier: MIT */
#include <string.h>
#include "component_common.h"
#include "src/gpu-palette.h"

typedef struct third_program {
    sixel_gpu_lookup_plan_interface_t iface;
    sixel_gpu_lookup_plan_interface_t *lease_owner;
    uint32_t invalid;
} third_program_t;

/* This test-only program is absent from every production registration.
 * A private builder token makes an omitted build observable in the result. */
static char const * const third_chunks[] = {
    "kernel void third_build(device uint *p [[buffer(3)]]) {"
    "p[0] = 0x12345678u;}\n"
    "static uint lookup_rgb8(uchar3 q, constant uchar *p, uint n,"
    "constant uchar *c, device const uchar *w) {"
    "uint best = 0u, distance = 0xffffffffu;"
    "if (*((constant uint *)c) == 1u ||"
    "(*((constant uint *)c) == 2u && q.r > 128u)) return n;"
    "if (*((device const uint *)w) != 0x12345678u) return 255u;"
    "for (uint i = 0u; i < n; ++i) {"
    "int3 d = int3(q) - int3(p[3u*i], p[3u*i+1u], p[3u*i+2u]);"
    "uint next = uint(d.x*d.x + d.y*d.y + d.z*d.z);"
    "if (next < distance) { distance = next; best = i; }}"
    "return best;}\n"
};
static sixel_gpu_lookup_build_pass_t const third_pass = {
    "third_build", {1, 1, 1}, {1, 1, 1}
};

static void
third_ref(sixel_gpu_lookup_plan_interface_t *self)
{
    third_program_t *program;

    program = (third_program_t *)self;
    program->lease_owner->vtbl->ref(program->lease_owner);
}

static void
third_unref(sixel_gpu_lookup_plan_interface_t *self)
{
    third_program_t *program;

    program = (third_program_t *)self;
    program->lease_owner->vtbl->unref(program->lease_owner);
}

static SIXELSTATUS
third_describe(sixel_gpu_lookup_plan_interface_t const *self,
    sixel_gpu_lookup_program_view_t *view)
{
    third_program_t const *program;
    SIXELSTATUS status;

    program = (third_program_t const *)self;
    status = program->lease_owner->vtbl->describe(program->lease_owner, view);
    if (status != SIXEL_OK) return status;
    view->logical_name = "third-test-program";
    view->chunks = third_chunks;
    view->chunk_count = 1;
    view->workspace_size = sizeof(uint32_t);
    view->constants = &program->invalid;
    view->constants_size = sizeof(program->invalid);
    view->build_passes = &third_pass;
    view->build_pass_count = 1;
    return SIXEL_OK;
}

static SIXELSTATUS
third_begin(sixel_gpu_lookup_plan_interface_t *self,
    sixel_gpu_lookup_binding_interface_t **binding)
{
    third_program_t *program;

    program = (third_program_t *)self;
    return program->lease_owner->vtbl->begin(program->lease_owner, binding);
}

static sixel_gpu_lookup_plan_vtbl_t const third_vtbl = {
    third_ref, third_unref, third_describe, third_begin
};

static SIXELSTATUS
third_rows(void *context, int rows)
{
    int *prefix;

    prefix = context;
    *prefix = rows;
    return SIXEL_OK;
}

int
injected_gpu_program_test(int invalid)
{
    gpu_component_fixture_t fixture;
    third_program_t third;
    sixel_gpu_device_interface_t *device;
    sixel_gpu_program_t *program;
    sixel_gpu_palette_request_t request;
    sixel_gpu_lookup_program_view_t view;
    unsigned char pixels[72] = {15, 0, 0, 240, 0, 0};
    sixel_index_t result[24];
    SIXELSTATUS status;
    int outcome;
    int prefix;
    int i;

    device = NULL;
    program = NULL;
    outcome = 1;
    memset(&fixture, 0, sizeof(fixture));
    memset(&request, 0, sizeof(request));
    status = sixel_gpu_device_acquire(&device);
    if (status == SIXEL_FEATURE_ERROR) return 77;
    if (status != SIXEL_OK || !gpu_component_open(&fixture, 0)) goto end;
    if (fixture.provider->vtbl->prepare(fixture.provider, &fixture.request,
            device, &fixture.other) != SIXEL_OK) goto end;
    third.iface.vtbl = &third_vtbl;
    third.invalid = (uint32_t)invalid;
    third.lease_owner = fixture.other;
    if (third_describe(&third.iface, &view) != SIXEL_OK ||
            sixel_gpu_program_prepare(device, &third.iface, &program)
                != SIXEL_OK) goto end;
    request.program = program;
    request.policy = SIXEL_GPU_POLICY_FORCE;
    request.pixels = pixels;
    request.dest = result;
    request.pixel_count = 2;
    request.width = 2;
    request.height = 1;
    request.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    request.palette = view.palette;
    request.palette_size = view.palette_size;
    request.palette_depth = 3;
    request.ncolors = 2;
    request.method_for_diffuse = SIXEL_DIFFUSE_NONE;
    request.method_for_scan = SIXEL_SCAN_RASTER;
    prefix = 0;
    if (invalid == 2) {
        /* The first range is valid even if the second already failed on GPU. */
        for (i = 0; i < 24; ++i) pixels[i * 3] = i < 12 ? 15 : 240;
        request.pixel_count = 24;
        request.height = 12;
        request.rows_per_chunk = 6;
        request.rows_ready = third_rows;
        request.rows_context = &prefix;
    }
    status = sixel_gpu_palette_apply_prepared(&request);
    outcome = invalid ? (status == SIXEL_LOGIC_ERROR ? 0 : 1)
        : (status == SIXEL_OK && result[0] == 0 && result[1] == 1 ? 0 : 1);
    if (invalid == 2 && prefix != 6) outcome = 1;
end:
    sixel_gpu_program_destroy(program);
    gpu_component_close(&fixture);
    if (device != NULL) device->vtbl->unref(device);
    return outcome;
}
