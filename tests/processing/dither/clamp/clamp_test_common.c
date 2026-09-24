/* SPDX-License-Identifier: MIT */
/* Analytic contracts use fixed values, independently of perceptual metrics. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/dither.h"
#include "src/encoder.h"
#include "src/dither-policy-fs.h"
#include "src/dither-policy-atkinson.h"
#include "src/dither-policy-jajuni.h"
#include "src/dither-policy-stucki.h"
#include "src/dither-policy-burkes.h"
#include "src/dither-policy-sierra1.h"
#include "src/dither-policy-sierra2.h"
#include "src/dither-policy-sierra3.h"
#include "src/dither-policy-lso2.h"
#include "src/dither-policy-interframe.h"
#include "clamp_test_common.h"

int
clamp_test_numeric(int scenario)
{
    sixel_allocator_t *allocator;
    sixel_dither_clamp_t clamp;
    SIXELSTATUS status;
    float source[3] = { 0.8f, 0.2f, 0.5f };
    float pixel[3] = { 1.4f, 0.5f, 0.6f };
    double expected[3] = { 1.1, 0.35, 0.55 };
    double threshold;
    int policy;
    int ok;
    int n;

    allocator = NULL;
    policy = SIXEL_DITHER_CLAMP_SOFT;
    threshold = 1.1;
    switch (scenario) {
    case 1:
        policy = SIXEL_DITHER_CLAMP_AUTO;
        break;
    case 2:
        policy = SIXEL_DITHER_CLAMP_HARD;
        expected[0] = 1.0;
        expected[1] = 0.5;
        expected[2] = 0.6;
        break;
    case 3:
        policy = SIXEL_DITHER_CLAMP_NONE;
        expected[0] = 1.4;
        expected[1] = 0.5;
        expected[2] = 0.6;
        break;
    case 4:
        source[0] = pixel[0] = 1.5f;
        source[1] = pixel[1] = -0.4f;
        source[2] = pixel[2] = 0.3f;
        expected[0] = 1.5;
        expected[1] = -0.4;
        expected[2] = 0.3;
        break;
    case 5:
        pixel[0] = -0.4f;
        pixel[1] = -0.1f;
        pixel[2] = 0.2f;
        expected[0] = -0.1;
        expected[1] = -0.025;
        expected[2] = 0.275;
        break;
    case 6:
        threshold = 1.0;
        expected[0] = 1.0;
        expected[1] = 0.3;
        expected[2] = 8.0 / 15.0;
        break;
    default:
        break;
    }
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        return EXIT_FAILURE;
    }
    status = sixel_dither_clamp_init(&clamp, source, 1, 1, 3,
        SIXEL_PIXELFORMAT_RGBFLOAT32, policy, threshold, allocator);
    ok = SIXEL_SUCCEEDED(status);
    if (ok) {
        status = sixel_dither_clamp_pixel(&clamp, 0U, pixel);
        ok = SIXEL_SUCCEEDED(status);
        for (n = 0; n < 3; ++n) {
            if (!isfinite(pixel[n]) ||
                    fabs((double)pixel[n] - expected[n]) > 0.000001) {
                fprintf(stderr, "channel %d: %.9g != %.9g\n",
                        n, (double)pixel[n], expected[n]);
                ok = 0;
            }
        }
        sixel_dither_clamp_finish(&clamp);
    }
    sixel_allocator_unref(allocator);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

typedef struct clamp_lookup {
    sixel_lookup_policy_interface_t base;
    float *last;
    int *calls;
} clamp_lookup_t;

static sixel_lookup_policy_result_t
clamp_lookup_pixel(sixel_lookup_policy_interface_t const *base,
                   unsigned char const *pixel)
{
    clamp_lookup_t const *lookup;

    lookup = (clamp_lookup_t const *)base;
    memcpy(lookup->last, pixel, 3U * sizeof(float));
    ++*lookup->calls;
    return 0;
}

static sixel_lookup_policy_vtbl_t const clamp_lookup_vtbl = {
    NULL, NULL, NULL, clamp_lookup_pixel
};

int
clamp_test_kernel(int method)
{
    static SIXELSTATUS (*const create[])(sixel_allocator_t *, void **) = {
        sixel_dither_policy_fs_float32_new,
        sixel_dither_policy_atkinson_float32_new,
        sixel_dither_policy_jajuni_float32_new,
        sixel_dither_policy_stucki_float32_new,
        sixel_dither_policy_burkes_float32_new,
        sixel_dither_policy_sierra1_float32_new,
        sixel_dither_policy_sierra2_float32_new,
        sixel_dither_policy_sierra3_float32_new,
        sixel_dither_policy_lso2_float32_new,
        sixel_dither_policy_interframe_float32_new
    };
    static int const lab_formats[] = {
        SIXEL_PIXELFORMAT_OKLABFLOAT32, SIXEL_PIXELFORMAT_CIELABFLOAT32,
        SIXEL_PIXELFORMAT_DIN99DFLOAT32
    };
    sixel_allocator_t *allocator;
    sixel_dither_t *dither;
    sixel_dither_policy_interface_t *policy;
    sixel_dither_policy_prepare_request_t prepare;
    sixel_dither_policy_apply_request_t request;
    clamp_lookup_t lookup;
    SIXELSTATUS status;
    void *object;
    float pixels[6] = { .99f, .99f, .99f, .99f, .99f, .99f };
    double expected[3] = { 1.1, 1.1, 1.1 };
    int pixelformat;
    float last[3] = { 0.0f, 0.0f, 0.0f };
    unsigned char palette[6] = { 0, 0, 0, 255, 255, 255 };
    sixel_index_t result[2];
    int calls;
    int ok;
    int n;

    pixelformat = SIXEL_PIXELFORMAT_RGBFLOAT32;
    if (method >= 10) {
        pixelformat = lab_formats[method - 10];
        method = 8;
        /* Extended source chroma must neither change nor reduce the L-only
         * correction. A black lookup target makes L exceed the soft bound. */
        pixels[1] = pixels[4] = 1000.0f;
        pixels[2] = pixels[5] = -1000.0f;
        expected[1] = 1000.0;
        expected[2] = -1000.0;
    }
    allocator = NULL;
    dither = NULL;
    policy = NULL;
    object = NULL;
    calls = 0;
    ok = 0;
    memset(&prepare, 0, sizeof(prepare));
    memset(&request, 0, sizeof(request));
    lookup.base.vtbl = &clamp_lookup_vtbl;
    lookup.last = last;
    lookup.calls = &calls;
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_dither_new(&dither, 2, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    sixel_dither_set_palette(dither, palette);
    dither->clamp_policy = SIXEL_DITHER_CLAMP_SOFT;
    dither->clamp_threshold = 1.1;
    status = create[method](allocator, &object);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    policy = object;
    prepare.dither = dither;
    prepare.depth = 3;
    prepare.pixelformat = pixelformat;
    prepare.method_for_scan = SIXEL_SCAN_RASTER;
    status = policy->vtbl->prepare(policy, &prepare);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    request.dither = dither;
    request.width = 2;
    request.height = 1;
    request.depth = 3;
    request.pixelformat = prepare.pixelformat;
    request.method_for_scan = prepare.method_for_scan;
    request.data = (unsigned char *)(void *)pixels;
    request.palette = palette;
    request.result = result;
    request.lookup_policy = &lookup.base;
    status = policy->vtbl->apply(policy, &request);
    ok = SIXEL_SUCCEEDED(status) && calls == 2;
    for (n = 0; n < 3; ++n) {
        if (!isfinite(last[n]) || fabs(last[n] - expected[n]) > 0.000001) {
            fprintf(stderr, "method %d channel %d: %.9g != %.9g\n",
                    method, n, (double)last[n], expected[n]);
            ok = 0;
        }
    }
end:
    if (policy != NULL) {
        policy->vtbl->unref(policy);
    }
    sixel_dither_unref(dither);
    sixel_allocator_unref(allocator);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
clamp_test_keep(int method)
{
    static int const methods[] = {
        SIXEL_DIFFUSE_FS, SIXEL_DIFFUSE_ATKINSON, SIXEL_DIFFUSE_JAJUNI,
        SIXEL_DIFFUSE_STUCKI, SIXEL_DIFFUSE_BURKES, SIXEL_DIFFUSE_SIERRA1,
        SIXEL_DIFFUSE_SIERRA2, SIXEL_DIFFUSE_SIERRA3, SIXEL_DIFFUSE_LSO2
    };
    sixel_allocator_t *allocator;
    sixel_dither_t *dither;
    sixel_index_t *indexes;
    unsigned char palette[6] = { 0, 0, 0, 255, 255, 255 };
    unsigned char retained[6] = { 255, 255, 255, 0, 0, 0 };
    unsigned char valid[2] = { 1, 0 };
    unsigned char pixels[6];
    SIXELSTATUS status;
    int mode;
    int ok;

    allocator = NULL;
    dither = NULL;
    ok = 1;
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        return EXIT_FAILURE;
    }
    status = sixel_dither_new(&dither, 2, allocator);
    if (SIXEL_FAILED(status)) {
        sixel_allocator_unref(allocator);
        return EXIT_FAILURE;
    }
    sixel_dither_set_palette(dither, palette);
    sixel_dither_set_pixelformat(dither, SIXEL_PIXELFORMAT_RGB888);
    sixel_dither_set_diffusion_type(dither, methods[method]);
    sixel_dither_set_diffusion_scan(dither, SIXEL_SCAN_RASTER);
    sixel_dither_set_lut_policy(dither, SIXEL_LUT_POLICY_NONE);
    sixel_dither_set_optimize_palette(dither, 0);
    sixel_dither_set_transparent(dither, 0);
    /* A retained white first pixel wins its lookup. Propagating its negative
     * residual must switch the next pixel from white to black. */
    for (mode = 0; mode < 2; ++mode) {
        memset(pixels, 130, sizeof(pixels));
        sixel_dither_set_pipeline_accumulation_buffer_hint(dither,
            retained, sizeof(retained), valid, sizeof(valid),
            2, 1, 0, 0, 2, 1, 0, 1, 0U,
            mode ? SIXEL_6DELTA_ERROR_DIFFUSE : SIXEL_6DELTA_ERROR_SKIP);
        indexes = sixel_dither_apply_palette(dither, pixels, 2, 1);
        if (indexes == NULL) {
            ok = 0;
        } else {
            if (indexes[0] != 0 || indexes[1] != (mode ? 0 : 1)) {
                fprintf(stderr, "keep method %d mode %d: %u, %u\n",
                    method, mode, indexes[0], indexes[1]);
                ok = 0;
            }
            sixel_allocator_free(allocator, indexes);
        }
    }
    sixel_dither_unref(dither);
    sixel_allocator_unref(allocator);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
clamp_test_options(int scenario)
{
    sixel_encoder_t *encoder;
    SIXELSTATUS status;
    int ok;

    encoder = NULL;
    status = sixel_encoder_new(&encoder, NULL);
    if (SIXEL_FAILED(status)) {
        return EXIT_FAILURE;
    }
    ok = 1;
    switch (scenario) {
    case 0:
        ok = encoder->clamp_policy == SIXEL_DITHER_CLAMP_AUTO &&
            encoder->clamp_threshold == 1.01;
        break;
    case 1:
        status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_CLAMP_POLICY,
                                      "soft:T1.2");
        ok = SIXEL_SUCCEEDED(status) &&
            encoder->clamp_policy == SIXEL_DITHER_CLAMP_SOFT &&
            encoder->clamp_threshold == 1.2;
        break;
    case 2:
        status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_CLAMP_POLICY,
                                      "soft:threshold=1.2");
        ok = SIXEL_SUCCEEDED(status);
        status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_CLAMP_POLICY,
                                      "hard");
        ok = ok && SIXEL_SUCCEEDED(status) &&
            encoder->clamp_policy == SIXEL_DITHER_CLAMP_HARD &&
            encoder->clamp_threshold == 1.01;
        break;
    case 3:
        status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_CLAMP_POLICY,
                                      "soft:threshold=nan");
        ok = SIXEL_FAILED(status) &&
            encoder->clamp_policy == SIXEL_DITHER_CLAMP_AUTO &&
            encoder->clamp_threshold == 1.01;
        break;
    case 4:
        status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_CLAMP_POLICY,
                                      "soft:threshold=0.9");
        ok = SIXEL_FAILED(status);
        break;
    case 5:
        status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_CLAMP_POLICY,
                                      "soft:threshold=16.1");
        ok = SIXEL_FAILED(status);
        break;
    default:
        ok = 0;
        break;
    }
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
