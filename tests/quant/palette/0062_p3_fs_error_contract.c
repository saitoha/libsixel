/* SPDX-License-Identifier: MIT */
/* Observe nonzero FS error at the next real lookup, including extended RGB. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sixel.h>
#include "src/dither.h"
#include "src/palette.h"
#include "src/dither-policy-fs.h"
#include "src/lookup-policy-none.h"
#include "tests/data/color-numeric/p3-palette.h"

static sixel_lookup_policy_interface_t *p3_fs_real_lookup;
static float p3_fs_next[3];
static int p3_fs_calls;

static int
p3_fs_map(sixel_lookup_policy_interface_t const *self,
           unsigned char const *pixel)
{
    (void)self;
    if (++p3_fs_calls == 2) {
        memcpy(p3_fs_next, pixel, sizeof(p3_fs_next));
    }
    return p3_fs_real_lookup->vtbl->map_pixel(p3_fs_real_lookup, pixel);
}

int
test_palette_0062_p3_fs_error(int argc, char **argv)
{
    sixel_dither_t *dither;
    sixel_dither_policy_interface_t *policy;
    sixel_lookup_policy_interface_t capture;
    sixel_lookup_policy_vtbl_t capture_vtbl;
    sixel_dither_policy_prepare_request_t prep;
    sixel_dither_policy_apply_request_t request;
    sixel_palette_entries_request_t byte_request;
    sixel_palette_float32_entries_request_t float_request;
    sixel_palette_entries_view_t bytes;
    sixel_palette_float32_entries_view_t floats;
    sixel_lookup_policy_prepare_request_t lookup_request;
    sixel_index_t indices[2];
    unsigned char red[3] = {255, 0, 0};
    unsigned char const *codes;
    unsigned int count;
    float palette[3];
    float pixels[6];
    float error[3] = {0.125f, -0.125f, 0.0625f};
    float next[3] = {1.0f, -0.03125f, 0.125f};
    /* Each next component is source + error * 7/16. These exact dyadic
     * expectations do not call the production diffusion or weight helpers. */
    float expected[3] = {1.0546875f, -0.0859375f, 0.15234375f};
    void *object;
    SIXELSTATUS status;
    int channel;
    int failed;
    int legacy;

    dither = NULL;
    policy = NULL;
    p3_fs_real_lookup = NULL;
    object = NULL;
    failed = 1;
    p3_fs_calls = 0;
    memset(&prep, 0, sizeof(prep));
    memset(&request, 0, sizeof(request));
    memset(&lookup_request, 0, sizeof(lookup_request));
    memset(&capture_vtbl, 0, sizeof(capture_vtbl));
    if (argc != 2) {
        return 1;
    }
    legacy = strcmp(argv[1], "legacy") == 0;
    if (SIXEL_FAILED(sixel_dither_new(&dither, 1, NULL)) ||
            SIXEL_FAILED(sixel_dither_policy_fs_float32_new(
                dither->allocator, &object))) {
        goto end;
    }
    policy = object;
    object = NULL;
    if (SIXEL_FAILED(sixel_lookup_policy_none_float32_new(
            dither->allocator, &object))) {
        goto end;
    }
    p3_fs_real_lookup = object;
    for (channel = 0; channel < 3; ++channel) {
        palette[channel] = (float)color_p3_red_linear[channel];
        pixels[channel] = palette[channel] + error[channel];
        pixels[channel + 3] = next[channel];
    }
    byte_request.entries = red;
    byte_request.colors = 1;
    byte_request.depth = 3;
    float_request.entries = palette;
    float_request.colors = 1;
    float_request.depth = 3 * sizeof(float);
    if (SIXEL_FAILED(dither->palette->vtbl->init_entries(dither->palette,
            &byte_request)) ||
            SIXEL_FAILED(dither->palette->vtbl->init_entries_float32(
                dither->palette, &float_request)) ||
            SIXEL_FAILED(sixel_palette_prepare_rgb101(dither->palette,
                SIXEL_COLORSPACE_LINEAR)) ||
            SIXEL_FAILED(sixel_palette_get_rgb101(dither->palette,
                &codes, &count)) || count != 1 || codes[0] != 100 ||
                codes[1] != 0 || codes[2] != 0 ||
            SIXEL_FAILED(dither->palette->vtbl->get_entries(
                dither->palette, &bytes)) ||
            SIXEL_FAILED(dither->palette->vtbl->get_entries_float32(
                dither->palette, &floats))) {
        goto end;
    }
    lookup_request.palette = bytes.entries;
    lookup_request.palette_float = floats.entries;
    lookup_request.depth = 3;
    lookup_request.float_depth = 3 * sizeof(float);
    lookup_request.reqcolor = 1;
    if (SIXEL_FAILED(p3_fs_real_lookup->vtbl->prepare(p3_fs_real_lookup,
            &lookup_request))) {
        goto end;
    }
    capture_vtbl.map_pixel = p3_fs_map;
    capture.vtbl = &capture_vtbl;
    dither->output_color_definition = legacy
        ? SIXEL_PALETTE_OUTPUT_LEGACY : SIXEL_PALETTE_OUTPUT_DISPLAY_P3;
    prep.dither = dither;
    prep.depth = 3;
    prep.method_for_scan = SIXEL_SCAN_RASTER;
    prep.pixelformat = SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
    if (SIXEL_FAILED(policy->vtbl->prepare(policy, &prep))) {
        goto end;
    }
    request.result = indices;
    request.data = (unsigned char *)pixels;
    request.width = 2;
    request.height = 1;
    request.depth = 3;
    request.palette = bytes.entries;
    request.lookup_policy = &capture;
    request.dither = dither;
    request.pixelformat = prep.pixelformat;
    if (strcmp(argv[1], "finite") == 0) {
        pixels[0] = NAN;
        status = policy->vtbl->apply(policy, &request);
        if (status != SIXEL_BAD_INPUT || p3_fs_calls != 0) {
            goto end;
        }
        pixels[0] = palette[0] + error[0];
    }
    if (SIXEL_FAILED(policy->vtbl->apply(policy, &request)) ||
            p3_fs_calls != 2 || indices[0] != 0 || indices[1] != 0) {
        goto end;
    }
    if (legacy) {
        expected[0] = 1.0f;
        expected[1] = 0.0f;
    }
    for (channel = 0; channel < 3; ++channel) {
        /* Allow float32 storage rounding of the independently derived
         * canonical primary; never an sRGB clamp or a missing error. */
        if (fabsf(p3_fs_next[channel] - expected[channel]) > 0.0000002f) {
            fprintf(stderr, "FS next[%d]: %.9g != %.9g\n", channel,
                (double)p3_fs_next[channel], (double)expected[channel]);
            goto end;
        }
    }
    failed = 0;
end:
    if (policy != NULL) {
        policy->vtbl->unref(policy);
    }
    if (p3_fs_real_lookup != NULL) {
        p3_fs_real_lookup->vtbl->unref(p3_fs_real_lookup);
    }
    sixel_dither_unref(dither);
    return failed;
}
