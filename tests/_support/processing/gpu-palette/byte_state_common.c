/* SPDX-License-Identifier: MIT */
#include <limits.h>
#include <string.h>
#include "src/gpu-palette.h"

/* Each wrapper fixes one policy and one observation. State transitions use
 * one two-frame sequence; the oracle is a separate integer palette scan. */
int
gpu_byte_state_test(int policy, int observation)
{
    static unsigned char const edges[8] = { 0, 7, 8, 127, 128, 247, 248, 255 };
    unsigned char palette[256 * 3];
    unsigned char pixels[512 * 3];
    sixel_index_t output[512];
    sixel_gpu_palette_request_t request;
    sixel_gpu_palette_test_hooks_t hooks;
    SIXELSTATUS status;
    int pass, i, j, c, delta, distance, best, expected, count;

    memset(palette, 0, sizeof(palette));
    memset(pixels, 255, sizeof(pixels));
    memset(&request, 0, sizeof(request));
    memset(palette + 3, 255, 3);
    request.policy = SIXEL_GPU_POLICY_FORCE;
    request.lut_policy = policy;
    request.dest = output;
    request.pixels = pixels;
    request.pixel_count = 1;
    request.width = request.height = 1;
    request.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    request.palette = palette;
    request.palette_size = sizeof(palette);
    request.palette_depth = 3;
    request.ncolors = 2;
    request.method_for_diffuse = SIXEL_DIFFUSE_NONE;
    request.method_for_scan = SIXEL_SCAN_RASTER;
    request.test_hooks = &hooks;
    if (observation == 2) {
        memset(palette, 255, 3);
        memset(palette + 3, 0, 3);
        memset(pixels, 0, 3);
        request.ncolors = 1;
    }
    if (observation == 4) {
        memset(palette, 0, sizeof(palette));
        memset(palette + 255 * 3, 255, 3);
        request.ncolors = 256;
    }
    if (observation == 5) {
        memset(palette, 0, sizeof(palette));
        palette[3] = 2;
        memset(pixels, 0, 3);
        pixels[0] = 1;
    }
    if (observation == 6) {
        request.ncolors = 17;
        request.pixel_count = 512;
        request.width = 32;
        request.height = 16;
        for (i = 0; i < 51; ++i) {
            palette[i] = (unsigned char)(i * 73 + 19);
        }
        for (i = 0; i < 512; ++i) {
            pixels[i * 3] = edges[i >> 6];
            pixels[i * 3 + 1] = edges[(i >> 3) & 7];
            pixels[i * 3 + 2] = edges[i & 7];
        }
    }
    for (pass = 0; pass < (observation < 4 ? 2 : 1); ++pass) {
        if (pass == 1 && observation == 1) {
            memset(palette, 255, 3);
            memset(palette + 3, 0, 3);
        }
        if (pass == 1 && observation == 2) request.ncolors = 2;
        memset(&hooks, 0, sizeof(hooks));
        if (pass == 0 && observation == 3) hooks.fault = 3;
        status = sixel_gpu_palette_apply(&request);
        if (pass == 0 && observation == 3) {
            if (status != SIXEL_RUNTIME_ERROR || hooks.published) return 1;
            continue;
        }
        /* Output equality alone must not pass via the legacy direct scan. */
        if (status != SIXEL_OK || hooks.kernel != 9 || !hooks.published) {
            return 1;
        }
        if (pass == 1 && hooks.rebuild != (observation != 0)) return 1;
        count = (int)request.pixel_count;
        for (i = 0; i < count; ++i) {
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
    }
    return 0;
}
