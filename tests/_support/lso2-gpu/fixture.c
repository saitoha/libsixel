/* SPDX-License-Identifier: MIT */
#include "fixture.h"

int
lso2_gpu_fixture_open(lso2_gpu_fixture_t *f, int require_gpu)
{
    sixel_palette_entries_request_t entries;
    sixel_wire_palette_request_t wire;
    sixel_wire_color_definition_t definitions[5] = {
        {0,2,{0,0,0}}, {1,2,{5,5,5}}, {2,2,{10,10,10}},
        {3,2,{20,20,20}}, {4,2,{0,0,0}}
    };
    unsigned char colors[15] = {
        0,0,0, 13,13,13, 26,26,26, 51,51,51, 0,0,0
    };
    SIXELSTATUS status;
    int x, y;

    memset(f, 0, sizeof(*f));
    if (sixel_allocator_new(&f->allocator, NULL, NULL, NULL, NULL) != SIXEL_OK)
        return 1;
    if (require_gpu) {
        status = sixel_gpu_device_acquire(&f->device);
        if (status == SIXEL_FEATURE_ERROR) return 77;
        if (status != SIXEL_OK) return 1;
    }
    if (sixel_gpu_lookup_ordered_create(f->allocator, &f->lookup) != SIXEL_OK)
        return 1;
    if (sixel_dither_policy_lso2_gpu_new(f->allocator,
            (void **)&f->dither) != SIXEL_OK) return 1;
    if (sixel_palette_apply_factory_new(f->allocator, (void **)&f->apply)
            != SIXEL_OK) return 1;
    if (sixel_palette_factory_new(f->allocator, (void **)&f->palette)
            != SIXEL_OK) return 1;
    entries.entries = colors;
    entries.colors = 5;
    entries.depth = 3;
    if (f->palette->vtbl->init_entries(f->palette, &entries) != SIXEL_OK)
        return 1;
    memset(&wire, 0, sizeof(wire));
    wire.definitions = definitions;
    wire.count = 5;
    wire.colorspace = SIXEL_COLORSPACE_GAMMA;
    if (sixel_wire_palette_new(&f->wire, &wire, f->allocator) != SIXEL_OK)
        return 1;
    for (y = 0; y < LSO2_GPU_HEIGHT; ++y) {
        for (x = 0; x < LSO2_GPU_WIDTH; ++x) {
            f->pixels[(y*LSO2_GPU_WIDTH+x)*3] = (unsigned char)((2*x+y)%52);
            f->pixels[(y*LSO2_GPU_WIDTH+x)*3+1] = (unsigned char)((x+2*y)%52);
            f->pixels[(y*LSO2_GPU_WIDTH+x)*3+2] = (unsigned char)((x+y)%52);
        }
    }
    if (sixel_frame_new(&f->source, f->allocator) != SIXEL_OK) return 1;
    if (sixel_frame_init_borrowed(f->source, f->pixels, LSO2_GPU_WIDTH,
            LSO2_GPU_HEIGHT, SIXEL_PIXELFORMAT_RGB888, NULL, -1) != SIXEL_OK)
        return 1;
    sixel_frame_set_colorspace(f->source, SIXEL_COLORSPACE_GAMMA);
    f->configuration.options.method_for_diffuse = SIXEL_DIFFUSE_LSO2;
    f->configuration.options.ncolors = 5;
    f->configuration.options.keycolor_reserved = 1;
    f->configuration.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    f->configuration.method_for_scan = SIXEL_SCAN_SERPENTINE;
    f->prepare.working_palette = f->palette;
    f->prepare.gpu_dither = f->dither;
    f->prepare.device = f->device;
    f->prepare.gpu_lookup = f->lookup;
    f->prepare.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    f->prepare.colorspace = SIXEL_COLORSPACE_GAMMA;
    f->prepare.scan = SIXEL_SCAN_SERPENTINE;
    f->prepare.worker_threads = 1;
    f->prepare.gpu_policy = SIXEL_GPU_POLICY_FORCE;
    f->prepare.output.width = f->prepare.output.encoded_width =
        f->prepare.output.plane_width = LSO2_GPU_WIDTH;
    f->prepare.output.height = f->prepare.output.encoded_height =
        f->prepare.output.plane_height = LSO2_GPU_HEIGHT;
    f->prepare.output.palette = f->wire;
    f->prepare.output.keycolor = 4;
    f->prepare.output.keycolor_reserved = 1;
    f->prepare.output.transparent_background = 1;
    f->run.source = sixel_frame_as_interface(f->source);
    return 0;
}

SIXELSTATUS
lso2_gpu_fixture_prepare(lso2_gpu_fixture_t *f)
{
    SIXELSTATUS status;

    status = f->dither->vtbl->configure(f->dither, &f->configuration);
    if (status != SIXEL_OK) return status;
    return f->apply->vtbl->prepare(f->apply, &f->prepare);
}

SIXELSTATUS
lso2_gpu_fixture_run(lso2_gpu_fixture_t *f)
{
    SIXELSTATUS status;
    sixel_retained_read_view_t view;

    sixel_indexed_frame_unref(f->image);
    f->image = NULL;
    status = sixel_indexed_frame_new(&f->image, &f->prepare.output,
        f->allocator);
    if (status != SIXEL_OK) return status;
    if (f->read != NULL) {
        status = sixel_retained_read_get_view(f->read, &view);
        if (status != SIXEL_OK) return status;
        f->image->identity.stream = view.stream;
        f->image->identity.epoch = view.epoch;
        f->image->identity.base_sequence = view.sequence;
    }
    f->run.result = f->image;
    return f->apply->vtbl->run(f->apply, &f->run);
}

void
lso2_gpu_fixture_close(lso2_gpu_fixture_t *f)
{
    if (f->apply != NULL) f->apply->vtbl->unref(f->apply);
    if (f->dither != NULL) f->dither->vtbl->unref(f->dither);
    if (f->lookup != NULL) f->lookup->vtbl->unref(f->lookup);
    if (f->palette != NULL) f->palette->vtbl->unref(f->palette);
    if (f->device != NULL) f->device->vtbl->unref(f->device);
    sixel_indexed_frame_unref(f->image);
    sixel_retained_read_unref(f->read);
    if (f->plane != NULL) f->plane->vtbl->unref(f->plane);
    sixel_frame_unref(f->source);
    sixel_wire_palette_unref(f->wire);
    sixel_allocator_unref(f->allocator);
}

SIXELSTATUS
lso2_gpu_fixture_baseline(lso2_gpu_fixture_t *f, int offset)
{
    sixel_retained_plane_description_t description;
    unsigned char *rgb, *valid;
    size_t cells;
    SIXELSTATUS status;

    status = sixel_retained_plane_factory_new(f->allocator,
        (void **)&f->plane);
    if (status != SIXEL_OK) return status;
    memset(&description, 0, sizeof(description));
    description.width = LSO2_GPU_WIDTH + (offset ? 4 : 0);
    description.height = LSO2_GPU_HEIGHT + (offset ? 5 : 0);
    description.colorspace = SIXEL_COLORSPACE_GAMMA;
    status = f->plane->vtbl->configure(f->plane, &description);
    if (status != SIXEL_OK) return status;
    status = sixel_retained_plane_legacy_map(f->plane, &rgb, &valid);
    if (status != SIXEL_OK) return status;
    cells = (size_t)description.width*description.height;
    memset(rgb, 31, cells*3);
    memset(valid, 1, cells);
    sixel_retained_plane_legacy_unmap(f->plane);
    status = f->plane->vtbl->acquire_read(f->plane, &f->read);
    if (status != SIXEL_OK) return status;
    f->prepare.output.plane_width = description.width;
    f->prepare.output.plane_height = description.height;
    f->prepare.output.origin_x = offset ? 2 : 0;
    f->prepare.output.origin_y = offset ? 3 : 0;
    f->run.baseline = f->read;
    f->run.delta.enabled = 1;
    f->run.delta.metric = SIXEL_DELTA_METRIC_MAX;
    return SIXEL_OK;
}

SIXELSTATUS
lso2_gpu_fixture_deferred(lso2_gpu_fixture_t *f)
{
    sixel_palette_apply_deferred_interface_t *deferred;
    sixel_palette_apply_solution_interface_t *solution;
    sixel_palette_apply_solve_request_t solve;
    sixel_palette_apply_resolve_request_t resolve;
    sixel_retained_read_view_t view;
    SIXELSTATUS status;

    deferred = NULL;
    solution = NULL;
    status = f->apply->vtbl->get_deferred(f->apply, &deferred);
    if (status != SIXEL_OK) goto end;
    memset(&solve, 0, sizeof(solve));
    solve.source = f->run.source;
    status = deferred->vtbl->solve(deferred, &solve, &solution);
    if (status != SIXEL_OK) goto end;
    sixel_indexed_frame_unref(f->image);
    f->image = NULL;
    status = sixel_indexed_frame_new(&f->image, &f->prepare.output,
        f->allocator);
    if (status != SIXEL_OK) goto end;
    if (f->read != NULL) {
        status = sixel_retained_read_get_view(f->read, &view);
        if (status != SIXEL_OK) goto end;
        f->image->identity.stream = view.stream;
        f->image->identity.epoch = view.epoch;
        f->image->identity.base_sequence = view.sequence;
    }
    memset(&resolve, 0, sizeof(resolve));
    resolve.baseline = f->run.baseline;
    resolve.result = f->image;
    resolve.delta = f->run.delta;
    status = solution->vtbl->resolve(solution, &resolve);
end:
    if (solution != NULL) solution->vtbl->unref(solution);
    if (deferred != NULL) deferred->vtbl->unref(deferred);
    return status;
}
