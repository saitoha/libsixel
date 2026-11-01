/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sixel.h>
#include "src/compat_stub.h"
#include "src/dither.h"
#include "src/gpu-palette.h"

/* Freeze the public coordinate equations with wider arithmetic, not the
 * production noise helpers. Direct lookup has an exact integer oracle. */
int
test_gpu_arithmetic(int xor_pattern, int mode)
{
    int const counts[] = {256, 64, 65, 16, 2, 1, 256};
    float const a_auto[] = {.15f, .25f, .25f, .45f,
        1.181370849898476f, 0, .15f};
    float const x_auto[] = {.10f, .15f, .15f, .25f,
        .615685424949238f, 0, .10f};
    unsigned char palette[768], source[65 * 17 * 3], saved[65 * 17 * 3];
    sixel_dither_t *dither;
    sixel_index_t *result;
    char const *env;
    unsigned long long phase;
    unsigned int distance, best_distance;
    float strength, noise;
    int method, pass, backend, i, d, x, y, k, ncolors, candidate[3];
    int delta, best, ok, passes;

    dither = NULL;
    result = NULL;
    ok = 0;
    method = xor_pattern ? SIXEL_DIFFUSE_X_DITHER : SIXEL_DIFFUSE_A_DITHER;
    env = xor_pattern ? "SIXEL_DITHER_X_DITHER_STRENGTH"
                      : "SIXEL_DITHER_A_DITHER_STRENGTH";
    if (sixel_compat_setenv(env, mode == 2 ? "0.5" : "") != 0 ||
            sixel_dither_new(&dither, 256, NULL) != SIXEL_OK) goto end;
    if (!sixel_gpu_palette_policy_claims_apply_stage(SIXEL_GPU_POLICY_AUTO,
            0, SIXEL_LUT_POLICY_NONE, method, SIXEL_SCAN_RASTER, 1105)) {
        goto end;
    }
    sixel_dither_set_pixelformat(dither, SIXEL_PIXELFORMAT_RGB888);
    sixel_dither_set_diffusion_type(dither, method);
    sixel_dither_set_lut_policy(dither, SIXEL_LUT_POLICY_NONE);
    sixel_dither_set_optimize_palette(dither, 0);
    dither->force_palette = 1;
    for (i = 0; i < (int)sizeof(source); ++i) {
        source[i] = (unsigned char)((i * 17 + i / 37) % 256);
    }
    memcpy(saved, source, sizeof(saved));
    passes = mode == 1 ? 7 : mode == 2 ? 3 : 2;
    for (pass = 0; pass < passes; ++pass) {
        ncolors = mode == 1 ? counts[pass] : 256;
        k = mode == 1 && pass == 2 ? 64 : ncolors;
        strength = mode == 1 ? (xor_pattern ? x_auto[pass] : a_auto[pass])
            : mode == 2 ? (pass == 0 ? .5f : pass == 1 ? .25f : 0.0f)
            : pass == 0 ? .5f : 0.0f;
        memset(palette, 0, sizeof(palette));
        for (i = 0; i < k; ++i) {
            memset(palette + i * 3, k == 1 ? 127 : i * 255 / (k - 1), 3);
        }
        dither->ncolors = ncolors;
        sixel_dither_set_palette(dither, palette);
        sixel_dither_set_transparent(dither, ncolors != k ? k : -1);
        dither->keycolor_reserved = ncolors != k;
        dither->a_dither_strength_override =
            dither->x_dither_strength_override =
                mode == 0 || (mode == 2 && pass != 0);
        dither->a_dither_strength = dither->x_dither_strength = strength;
        /* Both scan orders use the same absolute coordinate pattern. */
        sixel_dither_set_diffusion_scan(dither,
            pass % 2 ? SIXEL_SCAN_SERPENTINE : SIXEL_SCAN_RASTER);
        for (backend = 0; backend < 2; ++backend) {
            dither->gpu_policy = backend ? SIXEL_GPU_POLICY_FORCE
                                        : SIXEL_GPU_POLICY_OFF;
            result = sixel_dither_apply_palette(dither, source, 65, 17);
            if (result == NULL || memcmp(source, saved, sizeof(saved))) {
                goto end;
            }
            for (i = 0; i < 65 * 17; ++i) {
                x = i % 65; y = i / 65;
                for (d = 0; d < 3; ++d) {
                    if (xor_pattern) {
                        phase = (((unsigned long long)x + d * 29U) ^
                            ((unsigned long long)y * 149U)) * 1234U;
                        noise = (float)(phase % 512U) / 256.0f - 1.0f;
                    } else {
                        phase = ((unsigned long long)x + d * 67U +
                            (unsigned long long)y * 236U) * 119U;
                        noise = (float)(phase % 256U) / 128.0f - 1.0f;
                    }
                    candidate[d] = source[i * 3 + d] +
                        (int)((noise * strength) * 32.0f);
                    if (candidate[d] < 0) candidate[d] = 0;
                    if (candidate[d] > 255) candidate[d] = 255;
                }
                best = 0;
                best_distance = UINT_MAX;
                for (x = 0; x < k; ++x) {
                    distance = 0;
                    for (d = 0; d < 3; ++d) {
                        delta = candidate[d] - palette[x * 3 + d];
                        distance += (unsigned int)(delta * delta);
                    }
                    if (distance < best_distance) {
                        best_distance = distance;
                        best = x;
                    }
                }
                if (result[i] != best) {
                    fprintf(stderr, "pattern=%d mode=%d pass=%d gpu=%d "
                        "pixel=%d got=%d want=%d\n", xor_pattern, mode,
                        pass, backend, i, result[i], best);
                    goto end;
                }
            }
            sixel_allocator_free(dither->allocator, result);
            result = NULL;
        }
    }
    ok = 1;
end:
    if (dither != NULL) sixel_allocator_free(dither->allocator, result);
    sixel_dither_unref(dither);
    (void)sixel_compat_setenv(env, "");
    return ok ? 0 : 1;
}
