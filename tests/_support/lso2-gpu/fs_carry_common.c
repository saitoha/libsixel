/* SPDX-License-Identifier: MIT */
#include "tests/_support/lso2-gpu/fixture.h"
#include "src/dither-policy-fs-gpu.h"

/* Solve the shared fixture image with the fs GPU component. A deadzone
 * above every possible correction discards all carry, so the result is the
 * plain lookup of the source in either scan; live carry must change it. */
static int
fs_gpu_carry_solve(int scan, double deadzone, unsigned char *indexes)
{
    lso2_gpu_fixture_t f;
    SIXELSTATUS status;
    int outcome;

    outcome = lso2_gpu_fixture_open(&f, 1);
    if (outcome == 0) {
        f.dither->vtbl->unref(f.dither);
        f.dither = NULL;
        if (sixel_dither_policy_fs_gpu_new(f.allocator,
                (void **)&f.dither) != SIXEL_OK) outcome = 1;
    }
    if (outcome != 0) {
        lso2_gpu_fixture_close(&f);
        return outcome;
    }
    f.prepare.gpu_dither = f.dither;
    f.configuration.options.method_for_diffuse = SIXEL_DIFFUSE_FS;
    f.configuration.options.clamp_policy = SIXEL_DITHER_CLAMP_SOFT;
    f.configuration.options.clamp_threshold = 1.01;
    f.configuration.options.diffusion_deadzone = deadzone;
    f.configuration.method_for_scan = scan;
    f.prepare.scan = scan;
    status = lso2_gpu_fixture_prepare(&f);
    if (status == SIXEL_OK) status = lso2_gpu_fixture_run(&f);
    if (status == SIXEL_OK)
        memcpy(indexes, f.image->indexes, LSO2_GPU_PIXELS);
    lso2_gpu_fixture_close(&f);
    return status == SIXEL_OK ? 0 : 1;
}

SIXELSTATUS
test_fs_gpu_carry_configure(int method, int scan, int policy,
    double threshold, double deadzone, float perturb)
{
    sixel_gpu_dither_preparer_interface_t *dither;
    sixel_dither_policy_prepare_request_t request;
    sixel_allocator_t *allocator;
    SIXELSTATUS status;

    allocator = NULL;
    dither = NULL;
    if (sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL) != SIXEL_OK)
        return SIXEL_RUNTIME_ERROR;
    status = sixel_dither_policy_fs_gpu_new(allocator, (void **)&dither);
    if (status == SIXEL_OK) {
        memset(&request, 0, sizeof(request));
        request.options.method_for_diffuse = method;
        request.options.ncolors = 5;
        request.options.keycolor_reserved = 1;
        request.options.clamp_policy = policy;
        request.options.clamp_threshold = threshold;
        request.options.diffusion_deadzone = deadzone;
        request.options.diffusion_perturb = perturb;
        request.pixelformat = SIXEL_PIXELFORMAT_RGB888;
        request.method_for_scan = scan;
        status = dither->vtbl->configure(dither, &request);
        dither->vtbl->unref(dither);
    } else {
        /* Constructor failure must not satisfy a configure rejection. */
        status = SIXEL_RUNTIME_ERROR;
    }
    sixel_allocator_unref(allocator);
    return status;
}

/* Scan independence with carry suppressed and a live-carry effect are
 * separate observations. Each comparison starts from the same fixture. */
int
test_fs_gpu_carry_observation(int live)
{
    unsigned char baseline[LSO2_GPU_PIXELS], actual[LSO2_GPU_PIXELS];
    int outcome;

    outcome = fs_gpu_carry_solve(SIXEL_SCAN_RASTER, 1.0e6,
        baseline);
    if (outcome != 0) return outcome;
    outcome = fs_gpu_carry_solve(
        live ? SIXEL_SCAN_RASTER : SIXEL_SCAN_SERPENTINE,
        live ? 0.0 : 1.0e6, actual);
    if (outcome != 0) return outcome;
    return live ? memcmp(actual, baseline, LSO2_GPU_PIXELS) == 0
                : memcmp(actual, baseline, LSO2_GPU_PIXELS) != 0;
}

/* The source and retained colors have RGB gaps of 13. Both use
 * palette entries exactly, yielding zero carry. The chromatic difference
 * (0, 0, 13) is below the Y' cutoff while the gray difference is above it. */
int
test_fs_gpu_spatial_threshold(int deferred)
{
    static unsigned char const gray[3] = {26, 26, 26};
    static unsigned char const tint[3] = {13, 13, 26};
    lso2_gpu_fixture_t f;
    sixel_palette_entries_request_t entries;
    sixel_wire_palette_request_t wire;
    sixel_wire_color_definition_t definitions[5] = {
        {0, 2, {0, 0, 0}}, {1, 2, {5, 5, 5}}, {2, 2, {10, 10, 10}},
        {3, 2, {5, 5, 10}}, {4, 2, {0, 0, 0}}
    };
    uint32_t activity[128];
    unsigned char levels[4096], *rgb, *valid;
    unsigned char colors[15] = {
        0, 0, 0, 13, 13, 13, 26, 26, 26, 13, 13, 26, 0, 0, 0
    };
    size_t i;
    int outcome;

    outcome = lso2_gpu_fixture_open_direct(&f, SIXEL_DIFFUSE_FS);
    if (outcome != 0) goto end;
    outcome = 1;
    for (i = 0; i < LSO2_GPU_PIXELS; ++i)
        memcpy(f.pixels + i * 3, i % 2 ? tint : gray, 3);
    entries.entries = colors;
    entries.colors = 5;
    entries.depth = 3;
    LSO2_GPU_CHECK(f.palette->vtbl->init_entries(f.palette, &entries)
        == SIXEL_OK);
    /* The GPU lookup palette is bound through the wire description. */
    memset(&wire, 0, sizeof(wire));
    wire.definitions = definitions;
    wire.count = 5;
    wire.colorspace = SIXEL_COLORSPACE_GAMMA;
    sixel_wire_palette_unref(f.wire);
    f.wire = NULL;
    LSO2_GPU_CHECK(sixel_wire_palette_new(&f.wire, &wire, f.allocator)
        == SIXEL_OK);
    f.prepare.output.palette = f.wire;
    LSO2_GPU_CHECK(lso2_gpu_fixture_baseline(&f, 0) == SIXEL_OK);
    sixel_retained_read_unref(f.read);
    f.read = NULL;
    LSO2_GPU_CHECK(sixel_retained_plane_legacy_map(f.plane, &rgb, &valid)
        == SIXEL_OK);
    memset(rgb, 13, LSO2_GPU_PIXELS * 3);
    sixel_retained_plane_legacy_unmap(f.plane);
    LSO2_GPU_CHECK(f.plane->vtbl->acquire_read(f.plane, &f.read) == SIXEL_OK);
    f.run.baseline = f.read;
    f.run.delta.threshold = 48;
    memset(activity, 255, sizeof(activity));
    /* 8 plus the middle-band flag. Only chroma-dominant pixels get T16. */
    memset(levels, 72, sizeof(levels));
    f.run.delta.selection.words = activity;
    f.run.delta.selection.word_count = 128;
    f.run.delta.selection.columns = 64;
    f.run.delta.selection.rows = 64;
    f.run.delta.selection.width = LSO2_GPU_WIDTH;
    f.run.delta.selection.height = LSO2_GPU_HEIGHT;
    f.run.delta.selection.tiled = 3;
    f.run.delta.selection.cell_thresholds = levels;
    f.run.delta.selection.cell_threshold_count = 4096;
    LSO2_GPU_CHECK(lso2_gpu_fixture_prepare(&f) == SIXEL_OK);
    LSO2_GPU_CHECK((deferred ? lso2_gpu_fixture_deferred(&f) :
        lso2_gpu_fixture_run(&f)) == SIXEL_OK);
    for (i = 0; i < LSO2_GPU_PIXELS; ++i) {
        LSO2_GPU_CHECK(f.image->indexes[i] == (i % 2 ? 4 : 2));
        LSO2_GPU_CHECK(f.image->skip[i] == (i % 2
            ? SIXEL_INDEXED_KEEP_APPROXIMATE : SIXEL_INDEXED_PAINT));
    }
    outcome = 0;
end:
    lso2_gpu_fixture_close(&f);
    return outcome;
}
