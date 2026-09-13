/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sixel.h>
#include "src/dither.h"
#include "src/dither-policy-fs.h"

static float captured_float;
static int calls_float;

static int
perturb_row_float_map(sixel_lookup_policy_interface_t const *self,
                       unsigned char const *pixel)
{
    (void)self;
    if (++calls_float == 2) {
        captured_float = ((float const *)(void const *)pixel)[0];
    }
    return 0;
}

int
test_perturb_0004_row_float(int argc, char **argv)
{
    sixel_allocator_t *allocator;
    sixel_dither_t *dither;
    sixel_dither_policy_interface_t *policy;
    void *object;
    sixel_dither_policy_prepare_request_t prep;
    sixel_dither_policy_apply_request_t request;
    sixel_lookup_policy_vtbl_t vtbl;
    sixel_lookup_policy_interface_t lookup;
    sixel_index_t result[2];
    unsigned char palette[6] = { 0, 0, 0, 255, 255, 255 };
    float pixels[6] = { 0, 0, 0, 0.25f, 0.25f, 0.25f };
    sixel_dither_perturb_t perturb;
    float weights[4];
    float expected;
    int failed;

    (void)argc;
    (void)argv;
    allocator = NULL;
    dither = NULL;
    policy = NULL;
    object = NULL;
    failed = 1;
    calls_float = 0;
    memset(&prep, 0, sizeof(prep));
    memset(&request, 0, sizeof(request));
    memset(&vtbl, 0, sizeof(vtbl));
    vtbl.map_pixel = perturb_row_float_map;
    lookup.vtbl = &vtbl;
    if (SIXEL_FAILED(sixel_allocator_new(&allocator,
                                        NULL, NULL, NULL, NULL))
            || SIXEL_FAILED(sixel_dither_new(&dither, 2, allocator))
            || SIXEL_FAILED(sixel_dither_policy_fs_float32_new(
                allocator, &object))) {
        goto end;
    }
    policy = (sixel_dither_policy_interface_t *)object;
    if (SIXEL_FAILED(sixel_dither_set_diffusion_perturb(dither, 0.5f))) {
        goto end;
    }
    prep.dither = dither;
    prep.depth = 3;
    prep.method_for_scan = SIXEL_SCAN_SERPENTINE;
    prep.pixelformat = SIXEL_PIXELFORMAT_RGBFLOAT32;
    if (SIXEL_FAILED(policy->vtbl->prepare(policy, &prep))) {
        goto end;
    }
    request.result = result;
    request.data = (unsigned char *)(void *)pixels;
    request.width = 2;
    request.height = 1;
    request.band_origin = 17;
    request.output_start = 17;
    request.depth = 3;
    request.palette = palette;
    request.lookup_policy = &lookup;
    request.dither = dither;
    request.pixelformat = prep.pixelformat;
    sixel_dither_perturb_init(&perturb, 0.5f, 0);
    sixel_dither_perturb_float(&perturb, 1, 17, weights);
    expected = 0.25f * weights[0];
    if (SIXEL_FAILED(policy->vtbl->apply(policy, &request))) {
        goto end;
    }
    failed = calls_float != 2 || captured_float != expected;
end:
    if (policy != NULL) {
        policy->vtbl->unref(policy);
    }
    sixel_dither_unref(dither);
    sixel_allocator_unref(allocator);
    return failed;
}
