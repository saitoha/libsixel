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

static void
auto_strength_palette(sixel_dither_t *dither, int setting)
{
    static int const counts[] = {256, 64, 65, 16, 2, 1, 256};
    unsigned char palette[768];
    int i, k;

    k = setting == 2 ? 64 : counts[setting];
    memset(palette, 0, sizeof(palette));
    for (i = 0; i < k * 3; ++i) {
        palette[i] = k == 1 ? 127 : (unsigned char)(i / 3 * 255 / (k - 1));
    }
    dither->ncolors = counts[setting];
    sixel_dither_set_palette(dither, palette);
    sixel_dither_set_transparent(dither, setting == 2 ? 64 : -1);
    dither->keycolor_reserved = setting == 2;
}

int
test_auto_strength_case(int setting)
{
    static float const expected[] = {
        .055f, .135f, .135f, .295f, .880096679918781f, 0, .055f
    };
    unsigned char pixels[64 * 64 * 3];
    sixel_dither_t *dither;
    sixel_index_t *gpu;
    sixel_index_t *cpu;
    int i, ok;

    dither = NULL;
    gpu = cpu = NULL;
    ok = 0;
    if (SIXEL_FAILED(sixel_dither_new(&dither, 256, NULL))) goto end;
    sixel_dither_set_pixelformat(dither, SIXEL_PIXELFORMAT_RGB888);
    sixel_dither_set_diffusion_type(dither, SIXEL_DIFFUSE_BLUENOISE_DITHER);
    sixel_dither_set_diffusion_scan(dither, SIXEL_SCAN_RASTER);
    sixel_dither_set_lut_policy(dither, SIXEL_LUT_POLICY_NONE);
    sixel_dither_set_optimize_palette(dither, 0);
    dither->bluenoise_phase_override = 1;
    dither->bluenoise_phase_x = dither->bluenoise_phase_y = 0;
    dither->bluenoise_channel_override = 1;
    dither->bluenoise_channel_rgb = 0;
    dither->gpu_policy = SIXEL_GPU_POLICY_FORCE;
    dither->bluenoise_strength_override = 0;
    for (i = 0; i < (int)sizeof(pixels); ++i) {
        pixels[i] = (unsigned char)(116 + (i / 3) % 24);
    }
    if (setting != 0) {
        /* Populate the cache with the preceding palette. Its output is
         * setup only; the assertion below observes the selected transition.
         * In particular, the last case restores 256 colors after one color. */
        auto_strength_palette(dither, setting - 1);
        gpu = sixel_dither_apply_palette(dither, pixels, 64, 64);
        if (gpu == NULL) goto end;
        sixel_allocator_free(dither->allocator, gpu);
        gpu = NULL;
    }
    auto_strength_palette(dither, setting);
    gpu = sixel_dither_apply_palette(dither, pixels, 64, 64);
    dither->bluenoise_strength_override = 1;
    dither->bluenoise_strength = expected[setting];
    dither->gpu_policy = SIXEL_GPU_POLICY_OFF;
    cpu = sixel_dither_apply_palette(dither, pixels, 64, 64);
    if (gpu == NULL || cpu == NULL ||
            memcmp(gpu, cpu, 4096 * sizeof(*gpu)) != 0) {
        fprintf(stderr, "GPU auto strength mismatch at setting %d\n",
            setting);
        goto end;
    }
    ok = 1;
    if (setting == 1) {
        /* Keep the negative control with the 64-color observation: a fixed
         * 256-color amplitude must not satisfy this fixture's oracle. */
        sixel_allocator_free(dither->allocator, cpu);
        dither->bluenoise_strength = .055f;
        cpu = sixel_dither_apply_palette(dither, pixels, 64, 64);
        ok = cpu != NULL && memcmp(gpu, cpu, 4096 * sizeof(*gpu)) != 0;
    }
end:
    if (dither != NULL) {
        sixel_allocator_free(dither->allocator, gpu);
        sixel_allocator_free(dither->allocator, cpu);
        sixel_dither_unref(dither);
    }
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
