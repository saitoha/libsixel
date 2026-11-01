/* SPDX-License-Identifier: MIT */
/* A prepared policy owns values, not the prepare request's storage. */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sixel.h>
#include <6cells.h>

int
prepared_options_test(char const *classid, int method, int pixelformat)
{
    sixel_allocator_t *allocator;
    sixel_factory_t *factory;
    sixel_dither_policy_interface_t *policy;
    sixel_dither_policy_prepare_request_t request;
    sixel_dither_policy_options_t expected;
    sixel_dither_policy_prepare_request_t configuration;
    sixel_dither_policy_options_t actual;
    void *object;
    SIXELSTATUS status;
    int passed;

    allocator = NULL;
    policy = NULL;
    object = NULL;
    passed = 0;
    memset(&request, 0, sizeof(request));
    memset(&actual, 0, sizeof(actual));
    request.pixelformat = pixelformat;
    request.method_for_scan = SIXEL_SCAN_SERPENTINE;
    request.options.method_for_diffuse = method;
    request.options.ncolors = 17;
    request.options.keycolor_reserved = 1;
    request.options.clamp_policy = SIXEL_DITHER_CLAMP_HARD;
    request.options.clamp_threshold = 1.25;
    request.options.diffusion_deadzone = 0.125;
    request.options.diffusion_perturb = 0.25f;
    request.options.diffusion_perturb_seed = -7;
    request.options.a_dither_strength = 0.375f;
    request.options.x_dither_strength = 0.5f;
    request.options.bluenoise_strength = 0.625f;
    request.options.bluenoise_phase_x = 13;
    request.options.bluenoise_phase_y = 27;
    request.options.bluenoise_channel_rgb = 1;
    request.options.bluenoise_size = 64;
    request.options.bluenoise_gradient_factor = 0.75f;
    expected = request.options;
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) goto end;
    status = sixel_components_getservice("services/factory", &object);
    if (SIXEL_FAILED(status)) goto end;
    factory = object;
    object = NULL;
    status = factory->vtbl->create(factory, classid, allocator, &object);
    factory->vtbl->unref(factory);
    if (SIXEL_FAILED(status)) goto end;
    policy = object;
    status = policy->vtbl->prepare(policy, &request);
    if (SIXEL_FAILED(status)) goto end;
    memset(&request, 0, sizeof(request));
    status = policy->vtbl->get_configuration(policy, &configuration);
    if (SIXEL_FAILED(status)) goto end;
    actual = configuration.options;
    /* Compare fields, not padding: C assignment need not copy padding. */
    passed = configuration.pixelformat == pixelformat &&
        configuration.method_for_scan == SIXEL_SCAN_SERPENTINE &&
        actual.method_for_diffuse == expected.method_for_diffuse &&
        actual.ncolors == expected.ncolors &&
        actual.keycolor_reserved == expected.keycolor_reserved &&
        actual.use_rgb101_palette == expected.use_rgb101_palette &&
        actual.clamp_policy == expected.clamp_policy &&
        actual.clamp_threshold == expected.clamp_threshold &&
        actual.diffusion_deadzone == expected.diffusion_deadzone &&
        actual.diffusion_perturb == expected.diffusion_perturb &&
        actual.diffusion_perturb_seed == expected.diffusion_perturb_seed &&
        actual.a_dither_strength == expected.a_dither_strength &&
        actual.x_dither_strength == expected.x_dither_strength &&
        actual.bluenoise_strength == expected.bluenoise_strength &&
        actual.bluenoise_phase_x == expected.bluenoise_phase_x &&
        actual.bluenoise_phase_y == expected.bluenoise_phase_y &&
        actual.bluenoise_channel_rgb == expected.bluenoise_channel_rgb &&
        actual.bluenoise_size == expected.bluenoise_size &&
        actual.bluenoise_gradient_factor == expected.bluenoise_gradient_factor;
end:
    if (policy != NULL) policy->vtbl->unref(policy);
    sixel_allocator_unref(allocator);
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* Observe the input to nearest lookup, independently of the chosen index. */
typedef struct prepared_query_probe {
    sixel_lookup_policy_interface_t interface;
    int red;
    int calls;
} prepared_query_probe_t;

static int
prepared_query_map(sixel_lookup_policy_interface_t const *self,
    unsigned char const *pixel)
{
    prepared_query_probe_t *probe;

    probe = (prepared_query_probe_t *)(void *)self;
    probe->red = pixel[0];
    ++probe->calls;
    return 0;
}

static int
prepared_query_hint(sixel_lookup_policy_interface_t const *self,
    unsigned char const *pixel, int hint)
{
    (void)hint;
    return prepared_query_map(self, pixel);
}

int
prepared_query_test(char const *classid, int method, int expected_red)
{
    sixel_allocator_t *allocator;
    sixel_factory_t *factory;
    sixel_dither_policy_interface_t *policy;
    sixel_palette_t *palette;
    sixel_palette_entries_request_t palette_request;
    sixel_dither_policy_prepare_request_t prepare;
    sixel_dither_policy_apply_request_t request;
    sixel_dither_execution_t execution;
    sixel_lookup_policy_vtbl_t lookup_vtbl;
    prepared_query_probe_t probe;
    unsigned char colors[6] = {0, 0, 0, 255, 255, 255};
    unsigned char pixel[3] = {128, 128, 128};
    sixel_index_t index;
    void *object;
    SIXELSTATUS status;
    int passed;

    allocator = NULL;
    policy = NULL;
    palette = NULL;
    object = NULL;
    passed = 0;
    memset(&prepare, 0, sizeof(prepare));
    memset(&request, 0, sizeof(request));
    memset(&execution, 0, sizeof(execution));
    memset(&palette_request, 0, sizeof(palette_request));
    memset(&probe, 0, sizeof(probe));
    memset(&lookup_vtbl, 0, sizeof(lookup_vtbl));
    lookup_vtbl.map_pixel = prepared_query_map;
    lookup_vtbl.map_pixel_hint = prepared_query_hint;
    probe.interface.vtbl = &lookup_vtbl;
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) goto end;
    status = sixel_components_getservice("services/factory", &object);
    if (SIXEL_FAILED(status)) goto end;
    factory = object;
    object = NULL;
    status = factory->vtbl->create(factory, "quant/palette", allocator,
        &object);
    if (SIXEL_SUCCEEDED(status)) {
        palette = object;
        object = NULL;
        status = factory->vtbl->create(factory, classid, allocator, &object);
        policy = object;
    }
    factory->vtbl->unref(factory);
    if (SIXEL_FAILED(status)) goto end;
    palette_request.entries = colors;
    palette_request.colors = 2;
    palette_request.depth = 3;
    status = palette->vtbl->init_entries(palette, &palette_request);
    if (SIXEL_FAILED(status)) goto end;
    prepare.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    prepare.method_for_scan = SIXEL_SCAN_RASTER;
    prepare.options.method_for_diffuse = method;
    prepare.options.ncolors = 2;
    prepare.options.clamp_threshold = 1;
    prepare.options.a_dither_strength = 1;
    prepare.options.x_dither_strength = 1;
    prepare.options.bluenoise_strength = 1;
    prepare.options.bluenoise_size = 64;
    prepare.options.bluenoise_channel_rgb = 1;
    status = policy->vtbl->prepare(policy, &prepare);
    if (SIXEL_FAILED(status)) goto end;
    /* An alias to prepare would now disable every positional perturbation. */
    memset(&prepare, 0, sizeof(prepare));
    execution.working_palette = palette;
    execution.allocator = allocator;
    execution.keycolor = -1;
    execution.delta_keycolor = -1;
    request.width = request.height = 1;
    request.depth = 3;
    request.data = pixel;
    request.result = &index;
    request.palette = colors;
    request.lookup_policy = &probe.interface;
    request.execution = &execution;
    status = policy->vtbl->apply(policy, &request);
    passed = status == SIXEL_OK && probe.calls == 1 &&
        probe.red == expected_red;
end:
    if (policy != NULL) policy->vtbl->unref(policy);
    if (palette != NULL) palette->vtbl->unref(palette);
    if (!passed) {
        fprintf(stderr, "status=%x calls=%d red=%d expected=%d\n",
            status, probe.calls, probe.red, expected_red);
    }
    sixel_allocator_unref(allocator);
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
