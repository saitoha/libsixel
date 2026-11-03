/* SPDX-License-Identifier: MIT */
#include <limits.h>
#include <string.h>
#include "src/dither.h"
#include "src/gpu-palette.h"

/* Small literal fixtures exercise the certificate boundary and overflow.
 * Each wrapper chooses one fixture. The oracle scans the original palette,
 * independently of cell bounds, candidate lists and packed GPU distances. */
int
certlut_gpu_fixture_test(int observation)
{
    static unsigned char const edges[8] = {
        0, 15, 16, 127, 128, 239, 240, 255
    };
    unsigned char palette[768];
    unsigned char pixels[512 * 3];
    sixel_index_t output[512];
    sixel_gpu_palette_request_t request;
    sixel_gpu_palette_test_hooks_t hooks;
    SIXELSTATUS status;
    int i, j, c, delta, distance, best, expected;

    memset(&request, 0, sizeof(request));
    memset(&hooks, 0, sizeof(hooks));
    memset(palette, 102, sizeof(palette));
    palette[0] = 112;
    palette[3] = 113;
    palette[6] = 90;
    memset(pixels, 102, sizeof(pixels));
    pixels[0] = 100;
    request.policy = SIXEL_GPU_POLICY_FORCE;
    request.lut_policy = SIXEL_LUT_POLICY_CERTLUT;
    request.dest = output;
    request.pixels = pixels;
    request.pixel_count = 1;
    request.width = request.height = 1;
    request.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    request.palette = palette;
    request.palette_size = sizeof(palette);
    request.palette_depth = 3;
    request.ncolors = 3;
    request.method_for_diffuse = SIXEL_DIFFUSE_NONE;
    request.method_for_scan = SIXEL_SCAN_RASTER;
    request.test_hooks = &hooks;
    if (observation == 1) {
        request.ncolors = 256;
        request.pixel_count = 256;
        request.width = request.height = 16;
        /* All 256 distinct colors lie in the same cell. Each wins at its
         * own coordinate, so every valid candidate bound must retain it. */
        for (i = 0; i < 256; ++i) {
            palette[i * 3] = (unsigned char)(i & 15);
            palette[i * 3 + 1] = (unsigned char)(i >> 4);
            palette[i * 3 + 2] = 7;
        }
        memcpy(pixels, palette, sizeof(palette));
    }
    if (observation == 2 || observation == 3) {
        memset(palette, 0, sizeof(palette));
        memset(pixels, 0, sizeof(pixels));
        request.ncolors = 2;
        for (c = 0; c < (observation == 2 ? 1 : 3); ++c) {
            palette[c] = 30;
            pixels[c] = 15;
        }
        /* The center winner is index 1. Index 0 only ties at the cell's
         * far boundary, where its lower index must prevent exclusion. */
    }
    if (observation == 4) {
        request.ncolors = 17;
        request.pixel_count = 512;
        request.width = 32;
        request.height = 16;
        for (i = 0; i < 51; ++i)
            palette[i] = (unsigned char)((i * 73 + 19) & 255);
        for (i = 0; i < 512; ++i) {
            pixels[i * 3] = edges[i >> 6];
            pixels[i * 3 + 1] = edges[(i >> 3) & 7];
            pixels[i * 3 + 2] = edges[i & 7];
        }
    }
    if (observation < 0) {
        hooks.fault = -observation;
        status = sixel_gpu_palette_apply(&request);
        if (status != (observation == -1 ? SIXEL_BAD_ALLOCATION
                : SIXEL_RUNTIME_ERROR) || !hooks.entered ||
                !hooks.rebuild || hooks.published) return 1;
        memset(&hooks, 0, sizeof(hooks));
    }
    status = sixel_gpu_palette_apply(&request);
    if (status != SIXEL_OK || hooks.kernel != 9 || !hooks.published ||
            (observation < 0 && !hooks.rebuild)) return 1;
    for (i = 0; i < (int)request.pixel_count; ++i) {
        best = INT_MAX;
        expected = 0;
        for (j = 0; j < request.ncolors; ++j) {
            distance = 0;
            for (c = 0; c < 3; ++c) {
                delta = pixels[i * 3 + c] - palette[j * 3 + c];
                distance += delta * delta;
            }
            if (distance < best) {
                best = distance;
                expected = j;
            }
        }
        if (output[i] != expected) return 1;
    }
    return 0;
}

/* Legacy direct application may fall back under AUTO after a GPU failure.
 * That fallback must retain CPU certlut's selected policy, even where its
 * approximate answer differs from the exact GPU contract. */
int
certlut_gpu_auto_fallback_test(void)
{
    unsigned char palette[9] = { 112, 102, 102, 113, 102, 102, 90, 102, 102 };
    unsigned char pixels[3] = { 100, 102, 102 };
    sixel_gpu_palette_test_hooks_t hooks;
    sixel_dither_t *dither;
    sixel_index_t *output;
    int ok;

    dither = NULL;
    output = NULL;
    ok = 0;
    memset(&hooks, 0, sizeof(hooks));
    if (sixel_dither_new(&dither, 3, NULL) != SIXEL_OK) goto end;
    sixel_dither_set_palette(dither, palette);
    sixel_dither_set_pixelformat(dither, SIXEL_PIXELFORMAT_RGB888);
    sixel_dither_set_diffusion_type(dither, SIXEL_DIFFUSE_NONE);
    sixel_dither_set_lut_policy(dither, SIXEL_LUT_POLICY_CERTLUT);
    sixel_dither_set_optimize_palette(dither, 0);
    dither->gpu_policy = SIXEL_GPU_POLICY_AUTO;
    dither->gpu_palette_threshold = 0;
    dither->gpu_test_hooks = &hooks;
    hooks.fault = 3;
    output = sixel_dither_apply_palette(dither, pixels, 1, 1);
    /* At this literal counterexample the CPU center winner is index 0;
     * exact nearest lookup would select index 2 instead. */
    ok = output != NULL && output[0] == 0 && hooks.entered &&
        !hooks.published && dither->lookup_policy != NULL &&
        dither->lookup_policy->vtbl->get_kind(dither->lookup_policy) ==
            SIXEL_LUT_POLICY_CERTLUT;
end:
    if (dither != NULL) sixel_allocator_free(dither->allocator, output);
    sixel_dither_unref(dither);
    return ok ? 0 : 1;
}
