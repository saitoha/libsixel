/* SPDX-License-Identifier: MIT */
/* GPU auto amplitude must match independently specified CPU amplitudes. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/dither.h"

int
test_gpupal_0004_auto_strength(int argc, char **argv)
{
    int const counts[] = {256, 64, 65, 16, 2, 1, 256};
    float const expected[] = {
        .055f, .135f, .135f, .295f, .880096679918781f, 0, .055f
    };
    unsigned char palette[768];
    unsigned char pixels[64 * 64 * 3];
    sixel_dither_t *dither;
    sixel_index_t *gpu;
    sixel_index_t *cpu;
    int pass;
    int i;
    int k;
    int ok;
    int difference;

    (void)argc;
    (void)argv;
    dither = NULL;
    gpu = cpu = NULL;
    ok = difference = 0;
    if (SIXEL_FAILED(sixel_dither_new(&dither, 256, NULL))) {
        goto end;
    }
    sixel_dither_set_pixelformat(dither, SIXEL_PIXELFORMAT_RGB888);
    sixel_dither_set_diffusion_type(dither, SIXEL_DIFFUSE_BLUENOISE_DITHER);
    sixel_dither_set_diffusion_scan(dither, SIXEL_SCAN_RASTER);
    sixel_dither_set_lut_policy(dither, SIXEL_LUT_POLICY_NONE);
    sixel_dither_set_optimize_palette(dither, 0);
    dither->bluenoise_phase_override = 1;
    dither->bluenoise_phase_x = dither->bluenoise_phase_y = 0;
    dither->bluenoise_channel_override = 1;
    dither->bluenoise_channel_rgb = 0;
    for (i = 0; i < (int)sizeof(pixels); ++i) {
        pixels[i] = (unsigned char)(116 + (i / 3) % 24);
    }
    for (pass = 0; pass < 7; ++pass) {
        k = pass == 2 ? 64 : counts[pass];
        memset(palette, 0, sizeof(palette));
        for (i = 0; i < k * 3; ++i) {
            palette[i] = k == 1 ? 127 : (unsigned char)(i / 3 * 255 / (k - 1));
        }
        dither->ncolors = counts[pass];
        sixel_dither_set_palette(dither, palette);
        sixel_dither_set_transparent(dither, pass == 2 ? 64 : -1);
        dither->keycolor_reserved = pass == 2;
        dither->bluenoise_strength_override = 0;
        dither->gpu_policy = SIXEL_GPU_POLICY_FORCE;
        gpu = sixel_dither_apply_palette(dither, pixels, 64, 64);
        dither->bluenoise_strength_override = 1;
        dither->bluenoise_strength = expected[pass];
        dither->gpu_policy = SIXEL_GPU_POLICY_OFF;
        cpu = sixel_dither_apply_palette(dither, pixels, 64, 64);
        if (gpu == NULL || cpu == NULL ||
                memcmp(gpu, cpu, 4096 * sizeof(*gpu)) != 0) {
            fprintf(stderr, "GPU auto strength mismatch at %d colors\n", k);
            goto end;
        }
        sixel_allocator_free(dither->allocator, cpu);
        cpu = NULL;
        if (pass == 1) {
            /* Ensure this fixture detects leaving the GPU at its fixed
             * 256-color default, not merely CPU/GPU parity on easy pixels. */
            dither->bluenoise_strength = .055f;
            cpu = sixel_dither_apply_palette(dither, pixels, 64, 64);
            if (cpu != NULL) {
                difference = memcmp(gpu, cpu, 4096 * sizeof(*gpu)) != 0;
            }
            sixel_allocator_free(dither->allocator, cpu);
            cpu = NULL;
        }
        sixel_allocator_free(dither->allocator, gpu);
        gpu = NULL;
    }
    ok = difference;
end:
    if (dither != NULL) {
        sixel_allocator_free(dither->allocator, gpu);
        sixel_allocator_free(dither->allocator, cpu);
        sixel_dither_unref(dither);
    }
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
