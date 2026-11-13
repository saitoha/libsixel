/* SPDX-License-Identifier: MIT */
#include "fixture.h"
#include "src/dither.h"
#include "src/bluenoise_64x64.h"
#include "src/gpu-palette.h"
#include "src/lookup-policy-gpu.h"
#include "src/dither-policy-fs-gpu.h"
#include "src/dither-policy-atkinson-gpu.h"
#include "src/dither-policy-jajuni-gpu.h"
#include "src/dither-policy-stucki-gpu.h"
#include "src/dither-policy-burkes-gpu.h"
#include "src/dither-policy-sierra1-gpu.h"
#include "src/dither-policy-sierra2-gpu.h"
#include "src/dither-policy-sierra3-gpu.h"

/* Nonzero when the acquired device carries a program for KIND. A device
 * whose ABI does not carry that policy yet skips the test rather than
 * running a different lookup. */
int
lso2_gpu_fixture_serves(lso2_gpu_fixture_t const *f, int kind)
{
    return f->device != NULL && sixel_gpu_lookup_has_program(kind,
        f->device->vtbl->program_format(f->device));
}

static int
lso2_gpu_fixture_open_device(lso2_gpu_fixture_t *f, int require_gpu)
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

int
lso2_gpu_fixture_open(lso2_gpu_fixture_t *f, int require_gpu)
{
    int outcome;

    outcome = lso2_gpu_fixture_open_device(f, require_gpu);
    if (outcome == 0 && require_gpu &&
            !lso2_gpu_fixture_serves(f, SIXEL_LUT_POLICY_ORDERED)) {
        return 77;
    }
    return outcome;
}

int
lso2_gpu_fixture_use_method(lso2_gpu_fixture_t *f, int method)
{
    static struct {
        int method;
        SIXELSTATUS (*create)(sixel_allocator_t *, void **);
    } const components[] = {
        { SIXEL_DIFFUSE_LSO2, sixel_dither_policy_lso2_gpu_new },
        { SIXEL_DIFFUSE_FS, sixel_dither_policy_fs_gpu_new },
        { SIXEL_DIFFUSE_ATKINSON, sixel_dither_policy_atkinson_gpu_new },
        { SIXEL_DIFFUSE_JAJUNI, sixel_dither_policy_jajuni_gpu_new },
        { SIXEL_DIFFUSE_STUCKI, sixel_dither_policy_stucki_gpu_new },
        { SIXEL_DIFFUSE_BURKES, sixel_dither_policy_burkes_gpu_new },
        { SIXEL_DIFFUSE_SIERRA1, sixel_dither_policy_sierra1_gpu_new },
        { SIXEL_DIFFUSE_SIERRA2, sixel_dither_policy_sierra2_gpu_new },
        { SIXEL_DIFFUSE_SIERRA3, sixel_dither_policy_sierra3_gpu_new }
    };
    size_t i;

    for (i = 0; i < sizeof(components) / sizeof(*components); ++i) {
        if (components[i].method != method) continue;
        f->dither->vtbl->unref(f->dither);
        f->dither = NULL;
        if (components[i].create(f->allocator, (void **)&f->dither)
                != SIXEL_OK) return 1;
    }
    f->prepare.gpu_dither = f->dither;
    f->configuration.options.method_for_diffuse = method;
    if (method != SIXEL_DIFFUSE_LSO2) {
        /* The fixed-tap components follow the CPU carry limiter. */
        f->configuration.options.clamp_policy = SIXEL_DITHER_CLAMP_SOFT;
        f->configuration.options.clamp_threshold = 1.01;
    }
    return 0;
}

int
lso2_gpu_fixture_open_lookup(lso2_gpu_fixture_t *f, int method,
    sixel_lookup_policy_configuration_t const *configuration)
{
    int outcome;

    outcome = lso2_gpu_fixture_open_device(f, 1);
    if (outcome != 0) return outcome;
    if (!lso2_gpu_fixture_serves(f, configuration->kind)) return 77;
    if (lso2_gpu_fixture_use_method(f, method) != 0) return 1;
    f->lookup->vtbl->unref(f->lookup);
    f->lookup = NULL;
    if (sixel_gpu_lookup_select(f->allocator, configuration, &f->lookup)
            != SIXEL_OK) return 1;
    f->prepare.gpu_lookup = f->lookup;
    return 0;
}

int
lso2_gpu_fixture_open_direct(lso2_gpu_fixture_t *f, int method)
{
    sixel_lookup_policy_configuration_t configuration;

    memset(&configuration, 0, sizeof(configuration));
    configuration.kind = SIXEL_LUT_POLICY_NONE;
    configuration.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    return lso2_gpu_fixture_open_lookup(f, method, &configuration);
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

/* Independent scalar byte oracle: arithmetic phases are integer fractions,
 * and triangular blue noise is the sum of two table entries minus 255. */
static int
positional_gpu_oracle(lso2_gpu_fixture_t const *f, int method)
{
    static int const colors[] = {0, 13, 26, 51};
    int x, y, c, q[3], phase, value, u, v, best, index, distance, minimum;

    for (y = 0; y < LSO2_GPU_HEIGHT; ++y) {
        for (x = 0; x < LSO2_GPU_WIDTH; ++x) {
            for (c = 0; c < 3; ++c) {
                value = f->pixels[(y * LSO2_GPU_WIDTH + x) * 3 + c];
                if (method == SIXEL_DIFFUSE_A_DITHER) {
                    phase = ((x + c * 67 + y * 236) * 119) & 255;
                    value += (int)((phase - 128) * 16.0 / 128.0);
                } else if (method == SIXEL_DIFFUSE_X_DITHER) {
                    phase = (((x + c * 29) ^ (y * 149)) * 1234) & 511;
                    value += (int)((phase - 256) * 16.0 / 256.0);
                } else if (method == SIXEL_DIFFUSE_BLUENOISE_DITHER) {
                    u = sixel_bn64[(y & 63) * 64 + (x & 63)];
                    v = sixel_bn64[((y + 29) & 63) * 64 + ((x + 13) & 63)];
                    value += (int)((u + v - 255) * 16.0 / 255.0);
                }
                q[c] = value < 0 ? 0 : value > 255 ? 255 : value;
            }
            minimum = 3 * 256 * 256;
            best = 0;
            for (index = 0; index < 4; ++index) {
                distance = 0;
                for (c = 0; c < 3; ++c)
                    distance += (q[c] - colors[index]) *
                        (q[c] - colors[index]);
                if (distance < minimum) {
                    minimum = distance;
                    best = index;
                }
            }
            if (f->image->indexes[y * LSO2_GPU_WIDTH + x] != best) return 0;
        }
    }
    return 1;
}

static int
positional_gpu_open(lso2_gpu_fixture_t *f, int method, int direct)
{
    static char const *const classes[] = {
        "dither/none.gpu", "dither/a_dither.gpu",
        "dither/x_dither.gpu", "dither/bluenoise.gpu"
    };
    static int const methods[] = {SIXEL_DIFFUSE_NONE, SIXEL_DIFFUSE_A_DITHER,
        SIXEL_DIFFUSE_X_DITHER, SIXEL_DIFFUSE_BLUENOISE_DITHER};
    sixel_factory_t *factory;
    sixel_lookup_policy_configuration_t lookup;
    void *service;
    int outcome, i;
    SIXELSTATUS status;

    outcome = lso2_gpu_fixture_open(f, 1);
    if (outcome != 0) return outcome;
    for (i = 0; i < 4 && methods[i] != method; ++i) {}
    if (i == 4) return 1;
    if (sixel_components_getservice("services/factory", &service) != SIXEL_OK)
        return 1;
    factory = service;
    f->dither->vtbl->unref(f->dither);
    f->dither = NULL;
    status = factory->vtbl->create(factory, classes[i], f->allocator,
        (void **)&f->dither);
    factory->vtbl->unref(factory);
    if (status != SIXEL_OK) return 1;
    f->prepare.gpu_dither = f->dither;
    f->configuration.options.method_for_diffuse = method;
    f->configuration.options.a_dither_strength = 0.5;
    f->configuration.options.x_dither_strength = 0.5;
    f->configuration.options.bluenoise_strength = 0.5;
    f->configuration.options.bluenoise_size = 64;
    if (direct) {
        memset(&lookup, 0, sizeof(lookup));
        lookup.kind = SIXEL_LUT_POLICY_NONE;
        lookup.pixelformat = SIXEL_PIXELFORMAT_RGB888;
        f->lookup->vtbl->unref(f->lookup);
        f->lookup = NULL;
        if (sixel_gpu_lookup_select(f->allocator, &lookup, &f->lookup)
                != SIXEL_OK) return 1;
        f->prepare.gpu_lookup = f->lookup;
    }
    return 0;
}

/* Each wrapper reports one observation for one independently owned policy.
 * Live plans must survive preparer mutation and intervening policy dispatch. */
int
positional_gpu_component_check(int method, int observation)
{
    lso2_gpu_fixture_t f, other;
    sixel_dither_policy_prepare_request_t frozen;
    int outcome;

    memset(&other, 0, sizeof(other));
    outcome = positional_gpu_open(&f, method, observation == 2);
    if (outcome != 0) goto end;
    outcome = 1;
    LSO2_GPU_CHECK(lso2_gpu_fixture_prepare(&f) == SIXEL_OK);
    frozen = f.configuration;
    if (observation == 0) {
        f.configuration.options.a_dither_strength = 0;
        f.configuration.options.x_dither_strength = 0;
        f.configuration.options.bluenoise_strength = 0;
        f.configuration.options.ncolors = 4;
        LSO2_GPU_CHECK(f.dither->vtbl->configure(f.dither, &f.configuration)
            == SIXEL_OK);
    } else if (observation == 1) {
        f.configuration.options.method_for_diffuse = SIXEL_DIFFUSE_FS;
        LSO2_GPU_CHECK(f.dither->vtbl->configure(f.dither, &f.configuration)
            == SIXEL_BAD_ARGUMENT);
        LSO2_GPU_CHECK(f.dither->vtbl->get_configuration(f.dither,
            &f.configuration) == SIXEL_OK);
        LSO2_GPU_CHECK(!memcmp(&frozen, &f.configuration, sizeof(frozen)));
    } else {
        LSO2_GPU_CHECK(lso2_gpu_fixture_run(&f) == SIXEL_OK);
        LSO2_GPU_CHECK(positional_gpu_oracle(&f, method));
        LSO2_GPU_CHECK(positional_gpu_open(&other,
            method == SIXEL_DIFFUSE_A_DITHER ? SIXEL_DIFFUSE_X_DITHER
                : SIXEL_DIFFUSE_A_DITHER, 1) == 0);
        LSO2_GPU_CHECK(lso2_gpu_fixture_prepare(&other) == SIXEL_OK);
        LSO2_GPU_CHECK(lso2_gpu_fixture_run(&other) == SIXEL_OK);
    }
    LSO2_GPU_CHECK(lso2_gpu_fixture_run(&f) == SIXEL_OK);
    LSO2_GPU_CHECK(positional_gpu_oracle(&f, method));
    outcome = 0;
end:
    lso2_gpu_fixture_close(&other);
    lso2_gpu_fixture_close(&f);
    return outcome;
}

/* The context can return to the same diffusion kind after a positional
 * component. Stale kind metadata must not configure the wrong preparer. */
int
positional_gpu_reconfigure_check(int method)
{
    sixel_dither_t *dither;
    sixel_dither_policy_prepare_request_t configuration;
    unsigned char palette[6] = {0, 0, 0, 255, 255, 255};
    int outcome;

    dither = NULL;
    outcome = 1;
    LSO2_GPU_CHECK(sixel_dither_new(&dither, 2, NULL) == SIXEL_OK);
    sixel_dither_set_palette(dither, palette);
    sixel_dither_set_pixelformat(dither, SIXEL_PIXELFORMAT_RGB888);
    dither->gpu_policy = SIXEL_GPU_POLICY_FORCE;
    dither->diffuse_on_gpu = 1;
    sixel_dither_set_diffusion_type(dither, SIXEL_DIFFUSE_FS);
    LSO2_GPU_CHECK(sixel_dither_prepare_indexed_components(dither, 0, 0)
        == SIXEL_OK);
    dither->diffuse_on_gpu = 0;
    sixel_dither_set_diffusion_type(dither, method);
    LSO2_GPU_CHECK(sixel_dither_prepare_indexed_components(dither, 0, 0)
        == SIXEL_OK);
    LSO2_GPU_CHECK(dither->gpu_dither != NULL);
    LSO2_GPU_CHECK(dither->gpu_dither->vtbl->get_configuration(
        dither->gpu_dither, &configuration) == SIXEL_OK);
    LSO2_GPU_CHECK(configuration.options.method_for_diffuse == method);
    dither->diffuse_on_gpu = 1;
    sixel_dither_set_diffusion_type(dither, SIXEL_DIFFUSE_FS);
    LSO2_GPU_CHECK(sixel_dither_prepare_indexed_components(dither, 0, 0)
        == SIXEL_OK);
    LSO2_GPU_CHECK(dither->gpu_dither->vtbl->get_configuration(
        dither->gpu_dither, &configuration) == SIXEL_OK);
    LSO2_GPU_CHECK(configuration.options.method_for_diffuse ==
        SIXEL_DIFFUSE_FS);
    outcome = 0;
end:
    sixel_dither_unref(dither);
    return outcome;
}
