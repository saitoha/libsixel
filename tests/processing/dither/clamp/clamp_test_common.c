/* SPDX-License-Identifier: MIT */
/* Analytic contracts use fixed values, independently of perceptual metrics. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/dither.h"
#include "src/lookup-policy.h"
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
#include "src/dither-interframe-method.h"
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
    float *seen;
    int *calls;
} clamp_lookup_t;

static sixel_lookup_policy_result_t
clamp_lookup_pixel(sixel_lookup_policy_interface_t const *base,
                   unsigned char const *pixel)
{
    clamp_lookup_t const *lookup;

    lookup = (clamp_lookup_t const *)base;
    memcpy(lookup->last, pixel, 3U * sizeof(float));
    if (lookup->seen != NULL && *lookup->calls < 4) {
        memcpy(lookup->seen + *lookup->calls * 3, pixel, 3U * sizeof(float));
    }
    ++*lookup->calls;
    return 0;
}

static int
clamp_lookup_pixel_hint(sixel_lookup_policy_interface_t const *self,
                        unsigned char const *pixel,
                        int hint)
{
    (void)hint;
    return clamp_lookup_pixel(self, pixel);
}

static sixel_lookup_policy_vtbl_t const clamp_lookup_vtbl = {
    NULL, NULL, NULL, clamp_lookup_pixel, clamp_lookup_pixel_hint,
    sixel_lookup_policy_map_pixels_default, NULL
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
    float pixels[12] = { .99f, .99f, .99f, .99f, .99f, .99f };
    float seen[12];
    double grid[4] = { .95, 1.1, .603125, .7669921875 };
    double expected[3] = { 1.1, 1.1, 1.1 };
    int pixelformat;
    float last[3] = { 0.0f, 0.0f, 0.0f };
    unsigned char palette[6] = { 0, 0, 0, 255, 255, 255 };
    sixel_index_t result[4];
    int calls;
    int ok;
    int n;
    int temporal;
    int reset;
    int spatial;
    int serpentine;

    pixelformat = SIXEL_PIXELFORMAT_RGBFLOAT32;
    temporal = method == 13 || method == 14;
    reset = method == 14;
    spatial = method >= 15;
    serpentine = method == 16;
    if (spatial) {
        method = 0;
        for (n = 0; n < 12; ++n) {
            pixels[n] = n < 6 ? .95f : .1f;
        }
        if (serpentine) {
            grid[2] = .503125;
            grid[3] = .8232421875;
        }
        for (n = 0; n < 3; ++n) {
            expected[n] = grid[3];
        }
    }
    if (temporal) {
        method = 9;
    }
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
    lookup.seen = spatial ? seen : NULL;
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
    status = sixel_dither_set_clamp_policy(dither,
        SIXEL_DITHER_CLAMP_SOFT, 1.1);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (temporal) {
        /* Kernel-level invocation supplies the consuming-apply contract
         * normally established by PaletteApply, but generates real history. */
        dither->interframe_state.last_apply_consumed = 1;
        dither->interframe_strategy_override = 1;
        dither->interframe_strategy_token =
            SIXEL_INTERFRAME_STRATEGY_TOKEN_DIFFUSION;
        dither->interframe_spatial_diffuse_override = 1;
        dither->interframe_spatial_diffuse = SIXEL_DIFFUSE_NONE;
    }
    status = create[method](allocator, &object);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    policy = object;
    prepare.dither = dither;
    prepare.depth = 3;
    prepare.pixelformat = pixelformat;
    prepare.method_for_scan = serpentine ? SIXEL_SCAN_SERPENTINE
                                        : SIXEL_SCAN_RASTER;
    status = policy->vtbl->prepare(policy, &prepare);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    request.dither = dither;
    request.width = temporal ? 1 : 2;
    request.height = spatial ? 2 : 1;
    request.depth = 3;
    request.pixelformat = prepare.pixelformat;
    request.method_for_scan = prepare.method_for_scan;
    request.data = (unsigned char *)(void *)pixels;
    request.palette = palette;
    request.result = result;
    request.lookup_policy = &lookup.base;
    status = policy->vtbl->apply(policy, &request);
    if (temporal) {
        /* .99 rounds to byte 252. A black target retains 252/255 of
         * temporal correction, independently of the limiter formula. */
        if (SIXEL_FAILED(status) || calls != 1 ||
                dither->interframe_state.error_frame == NULL ||
                ((int32_t *)dither->interframe_state.error_frame)[0] !=
                    252 * SIXEL_INTERFRAME_VARERR_SCALE) {
            goto end;
        }
        if (reset) {
            sixel_dither_clear_frame_context(dither);
            dither->interframe_state.last_apply_consumed = 1;
        }
        for (n = 0; n < 3; ++n) {
            pixels[n] = .99f;
            expected[n] = reset ? .99 : 1.1;
        }
        status = policy->vtbl->apply(policy, &request);
        if (SIXEL_FAILED(status) ||
                dither->interframe_state.error_frame == NULL ||
                ((int32_t *)dither->interframe_state.error_frame)[0] !=
                    (reset ? 252 : 255) * SIXEL_INTERFRAME_VARERR_SCALE) {
            goto end;
        }
    }
    ok = SIXEL_SUCCEEDED(status) && calls == (spatial ? 4 : 2);
    if (spatial) {
        /* Analytic FS taps: 7/16 right, 3/16 down-left, 5/16 down,
         * 1/16 down-right. Pixel 1 clips to 1.1; propagate that residual. */
        for (n = 0; n < 12; ++n) {
            if (!isfinite(seen[n]) ||
                    fabs(seen[n] - grid[n / 3]) > .000001) {
                ok = 0;
            }
        }
    }
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
clamp_test_api(int scenario)
{
    sixel_dither_t *a;
    sixel_dither_t *b;
    sixel_encoder_t *ea;
    sixel_encoder_t *eb;
    SIXELSTATUS status;
    int ok;

    a = NULL;
    b = NULL;
    ea = NULL;
    eb = NULL;
    ok = 0;
    if (scenario == 2) {
        if (SIXEL_FAILED(sixel_encoder_new(&ea, NULL)) ||
                SIXEL_FAILED(sixel_encoder_new(&eb, NULL))) {
            goto end;
        }
        if (SIXEL_FAILED(sixel_encoder_setopt(ea,
                SIXEL_OPTFLAG_CLAMP_POLICY, "soft:T1.2")) ||
                SIXEL_FAILED(sixel_encoder_setopt(eb,
                SIXEL_OPTFLAG_CLAMP_POLICY, "hard:T1.5")) ||
                SIXEL_FAILED(sixel_encoder_setopt(ea,
                SIXEL_OPTFLAG_CLAMP_POLICY, "none:T1.3"))) {
            goto end;
        }
        sixel_encoder_unref(ea);
        ea = NULL;
        ok = eb->clamp_policy == SIXEL_DITHER_CLAMP_HARD &&
            eb->clamp_threshold == 1.5;
        goto end;
    }
    if (SIXEL_FAILED(sixel_dither_new(&a, 2, NULL)) ||
            SIXEL_FAILED(sixel_dither_new(&b, 2, NULL))) {
        goto end;
    }
    status = sixel_dither_set_clamp_policy(a, SIXEL_DITHER_CLAMP_SOFT, 1.2);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (scenario == 0) {
        /* All rejected updates preserve the complete last valid pair. */
        ok = sixel_dither_set_clamp_policy(NULL,
                 SIXEL_DITHER_CLAMP_HARD, 1.0) == SIXEL_BAD_ARGUMENT &&
            sixel_dither_set_clamp_policy(a, -1, 1.0) ==
                 SIXEL_BAD_ARGUMENT &&
            sixel_dither_set_clamp_policy(a, 4, 1.0) ==
                 SIXEL_BAD_ARGUMENT &&
            sixel_dither_set_clamp_policy(a, SIXEL_DITHER_CLAMP_HARD,
                 NAN) == SIXEL_BAD_ARGUMENT &&
            sixel_dither_set_clamp_policy(a, SIXEL_DITHER_CLAMP_NONE,
                 INFINITY) == SIXEL_BAD_ARGUMENT &&
            sixel_dither_set_clamp_policy(a, SIXEL_DITHER_CLAMP_AUTO,
                 .99) == SIXEL_BAD_ARGUMENT &&
            sixel_dither_set_clamp_policy(a, SIXEL_DITHER_CLAMP_HARD,
                 16.1) == SIXEL_BAD_ARGUMENT &&
            a->clamp_policy == SIXEL_DITHER_CLAMP_SOFT &&
            a->clamp_threshold == 1.2;
    } else {
        if (b->clamp_policy != SIXEL_DITHER_CLAMP_AUTO ||
                b->clamp_threshold != 1.01 ||
                SIXEL_FAILED(sixel_dither_set_clamp_policy(b,
                    SIXEL_DITHER_CLAMP_HARD, 16.0)) ||
                SIXEL_FAILED(sixel_dither_set_clamp_policy(a,
                    SIXEL_DITHER_CLAMP_NONE, 1.0))) {
            goto end;
        }
        sixel_dither_unref(a);
        a = NULL;
        ok = b->clamp_policy == SIXEL_DITHER_CLAMP_HARD &&
            b->clamp_threshold == 16.0;
    }
end:
    sixel_dither_unref(a);
    sixel_dither_unref(b);
    sixel_encoder_unref(ea);
    sixel_encoder_unref(eb);
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
