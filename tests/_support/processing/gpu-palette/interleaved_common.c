/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <string.h>
#include "src/lookup-policy-gpu.h"
#include "src/dither-policy-gpu.h"
#include "src/gpu-palette.h"
#include "src/palette.h"

/* Keep two palettes and four consumers alive together, then interleave a
 * third lookup. Results must retain the original palette and dither identity
 * after the source palette and the backend's active program have changed. */
int
gpu_lookup_interleaved(int policy, int scalar)
{
    static int const methods[] = { SIXEL_DIFFUSE_NONE, SIXEL_DIFFUSE_A_DITHER,
        SIXEL_DIFFUSE_X_DITHER, SIXEL_DIFFUSE_BLUENOISE_DITHER };
    sixel_allocator_t *allocator;
    sixel_palette_t *palette;
    sixel_palette_entries_request_t entries;
    sixel_gpu_device_interface_t *device;
    sixel_gpu_lookup_preparer_interface_t *provider;
    sixel_gpu_lookup_plan_interface_t *plans[3];
    sixel_gpu_lookup_program_view_t views[3];
    sixel_gpu_program_t *programs[3][4];
    sixel_lookup_policy_configuration_t configuration;
    sixel_lookup_execution_request_t execution;
    sixel_gpu_palette_request_t request;
    sixel_gpu_palette_test_hooks_t hooks;
    unsigned char colors[65 * 3], pixels[19 * 17 * 3];
    sixel_index_t baseline[2][4][19 * 17], output[19 * 17];
    SIXELSTATUS status;
    void *object;
    int i, j, pass, phase, outcome, different;

    allocator = NULL;
    palette = NULL;
    device = NULL;
    provider = NULL;
    memset(plans, 0, sizeof(plans));
    memset(programs, 0, sizeof(programs));
    memset(&configuration, 0, sizeof(configuration));
    memset(&execution, 0, sizeof(execution));
    memset(&request, 0, sizeof(request));
    outcome = 1;
    status = sixel_gpu_device_acquire(&device);
    if (status == SIXEL_FEATURE_ERROR) return 77;
    if (status != SIXEL_OK) goto end;
    if (scalar && strcmp(device->vtbl->program_format(device),
            "metal.lookup-rgb8.v1") != 0) {
        outcome = 77;
        goto end;
    }
    if (sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL) != SIXEL_OK)
        goto end;
    object = NULL;
    if (sixel_palette_factory_new(allocator, &object) != SIXEL_OK) goto end;
    palette = object;
    configuration.kind = policy;
    configuration.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    configuration.fhedt_resolution = 64;
    configuration.fhedt_refine = 1;
    configuration.fhedt_weights[0] = configuration.fhedt_weights[1] =
        configuration.fhedt_weights[2] = 1.0;
    execution.working_palette = palette;
    execution.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    execution.colorspace = SIXEL_COLORSPACE_GAMMA;
    execution.allow_batch = 1;
    if (sixel_gpu_lookup_select(allocator, &configuration, &provider)
            != SIXEL_OK) goto end;
    for (i = 0; i < 3; ++i) {
        if (i == 2) {
            provider->vtbl->unref(provider);
            provider = NULL;
            configuration.kind = policy == SIXEL_LUT_POLICY_5BIT
                ? SIXEL_LUT_POLICY_6BIT : SIXEL_LUT_POLICY_5BIT;
            if (sixel_gpu_lookup_select(allocator, &configuration, &provider)
                    != SIXEL_OK) goto end;
        }
        for (j = 0; j < (int)sizeof(colors); ++j)
            colors[j] = (unsigned char)(j * 73 + i * 101 + 19);
        entries.entries = colors;
        entries.colors = execution.ncolors = i == 1 ? 17 : 65;
        entries.depth = 3;
        if (palette->vtbl->init_entries(palette, &entries) != SIXEL_OK ||
                provider->vtbl->prepare(provider, &execution, device,
                    &plans[i]) != SIXEL_OK) goto end;
        if (plans[i]->vtbl->describe(plans[i], &views[i]) != SIXEL_OK)
            goto end;
        for (j = 0; j < 4; ++j)
            if (sixel_gpu_program_prepare(device, plans[i],
                    sixel_gpu_dither_byte_program(methods[j]),
                    &programs[i][j]) != SIXEL_OK) goto end;
    }
    for (j = 0; j < (int)sizeof(pixels); ++j)
        pixels[j] = (unsigned char)(j * 31 + j / 7);
    request.policy = SIXEL_GPU_POLICY_FORCE;
    request.dest = output;
    request.pixels = pixels;
    request.pixel_count = 19 * 17;
    request.width = 19;
    request.height = 17;
    request.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    request.palette = colors;
    request.palette_size = sizeof(colors);
    request.palette_depth = 3;
    request.ncolors = 65;
    request.method_for_scan = SIXEL_SCAN_RASTER;
    request.positional_strength = 1.0f;
    request.bluenoise_strength = 0.65f;
    request.test_hooks = &hooks;
    /* In the scalar observation the baseline uses real matrix hardware;
     * the second phase forces the same policy's non-matrix implementation. */
    for (phase = 0; phase < 2; ++phase) {
        for (pass = 0; pass < 3; ++pass) {
            i = phase == 0 ? pass : 2 - pass;
            for (j = 0; j < 4; ++j) {
                memset(&hooks, 0, sizeof(hooks));
                hooks.disable_matrix = scalar && phase == 1;
                request.program = programs[i][j];
                request.palette = views[i].palette;
                request.palette_size = views[i].palette_size;
                request.ncolors = (int)views[i].ncolors;
                request.dither_program = sixel_gpu_dither_byte_program(
                    methods[j]);
                request.method_for_diffuse = methods[j];
                status = sixel_gpu_palette_apply(&request);
                if (status != SIXEL_OK || !hooks.entered || !hooks.published)
                    goto end;
                if (scalar && phase == 1 && i < 2 && hooks.kernel !=
                        (policy == SIXEL_LUT_POLICY_NONE ? 1 : 3)) goto end;
                if (i == 2) continue;
                if (phase == 0) memcpy(baseline[i][j], output, sizeof(output));
                else if (memcmp(baseline[i][j], output, sizeof(output)))
                    goto end;
            }
        }
    }
    /* A constant result would make cache isolation vacuous. Both palette
     * order and the positional query must be visible in this fixture. */
    different = 0;
    for (j = 1; j < 4; ++j)
        different |= memcmp(baseline[0][0], baseline[0][j], sizeof(output));
    if (!different || !memcmp(baseline[0], baseline[1], sizeof(baseline[0])))
        goto end;
    outcome = 0;
end:
    if (outcome == 1)
        fprintf(stderr, "interleaved GPU lookup %d: %s\n", policy,
            sixel_helper_get_additional_message());
    for (i = 0; i < 3; ++i) {
        for (j = 0; j < 4; ++j) sixel_gpu_program_destroy(programs[i][j]);
        if (plans[i] != NULL) plans[i]->vtbl->unref(plans[i]);
    }
    if (provider != NULL) provider->vtbl->unref(provider);
    if (palette != NULL) palette->vtbl->unref(palette);
    if (device != NULL) device->vtbl->unref(device);
    if (allocator != NULL) sixel_allocator_unref(allocator);
    return outcome;
}
