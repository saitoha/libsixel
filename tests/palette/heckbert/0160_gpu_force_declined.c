/* SPDX-License-Identifier: MIT */
/* Under the force policy a histogram the Vulkan Heckbert solver declines
 * stays on the CPU, so the palette equals the CPU-only palette. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <string.h>
#include <sixel.h>
#include "src/dither.h"
#if defined(HAVE_VULKAN) && !defined(HAVE_METAL)
#include "src/gpu-runtime-vulkan.h"
#include "src/palette-resident-vulkan.h"
#endif

enum { DECLINED_SIDE = 128 };

#if defined(HAVE_VULKAN) && !defined(HAVE_METAL)
/* 128x128 distinct colors exceed the 4096-entry Vulkan solver domain. */
static void
declined_pixels(unsigned char *pixels)
{
    int x;
    int y;
    unsigned char *p;

    p = pixels;
    for (y = 0; y < DECLINED_SIDE; ++y) {
        for (x = 0; x < DECLINED_SIDE; ++x) {
            *p++ = (unsigned char)(x * 2);
            *p++ = (unsigned char)(y * 2);
            *p++ = (unsigned char)((x * 7 + y * 13) % 256);
        }
    }
}

static int
declined_palette(unsigned char *pixels, int gpu_policy,
    unsigned char *palette, int *ncolors)
{
    sixel_dither_t *dither;
    unsigned char *entries;
    int ok;

    dither = NULL;
    ok = 0;
    if (SIXEL_FAILED(sixel_dither_new(&dither, 256, NULL))) goto end;
    dither->gpu_policy = gpu_policy;
    if (SIXEL_FAILED(sixel_dither_initialize(dither, pixels, DECLINED_SIDE,
            DECLINED_SIDE, SIXEL_PIXELFORMAT_RGB888, SIXEL_LARGE_NORM,
            SIXEL_REP_AUTO, SIXEL_QUALITY_AUTO)))
        goto end;
    *ncolors = sixel_dither_get_num_of_palette_colors(dither);
    entries = sixel_dither_get_palette(dither);
    if (entries == NULL || *ncolors <= 0 || *ncolors > 256) goto end;
    memcpy(palette, entries, (size_t)*ncolors * 3u);
    ok = 1;
end:
    sixel_dither_unref(dither);
    return ok;
}
#endif

int
test_gpu_force_declined(int argc, char **argv)
{
#if defined(HAVE_VULKAN) && !defined(HAVE_METAL)
    static unsigned char pixels[DECLINED_SIDE * DECLINED_SIDE * 3];
    unsigned char cpu[768];
    unsigned char forced[768];
    sixel_vk_runtime_t *runtime;
    int cpu_colors;
    int forced_colors;
    int ready;

    (void)argc;
    (void)argv;
    sixel_vk_lock();
    runtime = sixel_vk_runtime_get_locked();
    ready = runtime != NULL && sixel_vk_heckbert_threads(runtime) != 0u;
    sixel_vk_unlock();
    if (!ready) return 77;
    declined_pixels(pixels);
    if (!declined_palette(pixels, SIXEL_GPU_POLICY_OFF, cpu, &cpu_colors) ||
            !declined_palette(pixels, SIXEL_GPU_POLICY_FORCE, forced,
                &forced_colors))
        return 1;
    return cpu_colors == forced_colors &&
        memcmp(cpu, forced, (size_t)cpu_colors * 3u) == 0 ? 0 : 1;
#else
    (void)argc;
    (void)argv;
    return 77;
#endif
}
