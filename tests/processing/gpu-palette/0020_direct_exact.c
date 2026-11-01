/*
 * SPDX-License-Identifier: MIT
 *
 * Policy: docs/encoder/palette-application/lookup-policy/README.md
 */
/*
 * Metal direct lookup must return the CPU exhaustive scan's index for every
 * pixel.  On Apple GPU family 7 and later this runs the matrix-product
 * kernel, whose exactness rests on byte coordinates keeping every score an
 * integer below 2^24.  The fixtures stress the parts that differ from the
 * per-thread scan: palette sizes that do not fill an 8-color tile, pixel
 * counts that leave partial simdgroups and threadgroups, equidistant colors
 * (lowest index must win), duplicate palette entries, and 6delta keeps that
 * finish lanes before the shared matrix product.
 */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/dither.h"
#include "src/gpu-palette.h"

enum {
    GPUPAL_DIRECT_WIDTH = 61,
    GPUPAL_DIRECT_HEIGHT = 37,
    GPUPAL_DIRECT_PIXELS = GPUPAL_DIRECT_WIDTH * GPUPAL_DIRECT_HEIGHT
};

static unsigned int
gpupal_direct_next(unsigned int *state)
{
    *state = *state * 1103515245U + 12345U;
    return (*state >> 16) & 0x7fffU;
}

/* Palette of ncolors entries; the last entry repeats the first so equal
 * colors meet the lowest-index rule.  Slot ncolors holds a black key that
 * only the keep fixtures reserve. */
static void
gpupal_direct_palette(unsigned char *palette, int ncolors,
                      unsigned int seed)
{
    unsigned int state;
    int index;

    state = seed;
    for (index = 0; index < ncolors * 3; ++index) {
        palette[index] = (unsigned char)(gpupal_direct_next(&state) & 255U);
    }
    if (ncolors > 2) {
        memcpy(palette + (ncolors - 1) * 3, palette, 3);
    }
    memset(palette + ncolors * 3, 0, 3);
}

/* A third of the pixels are random, a third reuse palette colors, and a
 * third sit on the even midpoint of two palette colors (an equidistant
 * pair whenever no third color is nearer). */
static void
gpupal_direct_pixels(unsigned char *pixels, unsigned char const *palette,
                     int ncolors, unsigned int seed)
{
    unsigned int state;
    int pixel;
    int channel;
    int a;
    int b;

    state = seed;
    for (pixel = 0; pixel < GPUPAL_DIRECT_PIXELS; ++pixel) {
        a = (int)(gpupal_direct_next(&state) % (unsigned int)ncolors);
        b = (int)(gpupal_direct_next(&state) % (unsigned int)ncolors);
        for (channel = 0; channel < 3; ++channel) {
            switch (pixel % 3) {
            case 0:
                pixels[pixel * 3 + channel] = (unsigned char)
                    (gpupal_direct_next(&state) & 255U);
                break;
            case 1:
                pixels[pixel * 3 + channel] = palette[a * 3 + channel];
                break;
            default:
                pixels[pixel * 3 + channel] = (unsigned char)
                    (((palette[a * 3 + channel] & 0xfe)
                      + (palette[b * 3 + channel] & 0xfe)) / 2);
                break;
            }
        }
    }
}

static sixel_index_t *
gpupal_direct_apply(sixel_allocator_t *allocator,
                    unsigned char *palette,
                    int ncolors,
                    unsigned char *pixels,
                    int keep,
                    unsigned char const *retained,
                    unsigned char const *valid,
                    int gpu_policy,
                    unsigned char *mask_copy)
{
    sixel_dither_t *dither;
    sixel_index_t *indexes;
    unsigned char const *mask;
    size_t mask_size;
    SIXELSTATUS status;
    sixel_gpu_palette_test_hooks_t hooks;

    memset(&hooks, 0, sizeof(hooks));
    dither = NULL;
    indexes = NULL;
    mask = NULL;
    mask_size = 0U;
    status = sixel_dither_new(&dither, keep ? ncolors + 1 : ncolors,
                              allocator);
    if (SIXEL_FAILED(status)) {
        return NULL;
    }
    sixel_dither_set_palette(dither, palette);
    sixel_dither_set_pixelformat(dither, SIXEL_PIXELFORMAT_RGB888);
    sixel_dither_set_diffusion_type(dither, SIXEL_DIFFUSE_NONE);
    sixel_dither_set_diffusion_scan(dither, SIXEL_SCAN_RASTER);
    sixel_dither_set_lut_policy(dither, SIXEL_LUT_POLICY_NONE);
    sixel_dither_set_optimize_palette(dither, 0);
    dither->gpu_policy = gpu_policy;
    dither->gpu_test_hooks = &hooks;
    if (keep) {
        sixel_dither_set_transparent(dither, ncolors);
        dither->keycolor_reserved = 1;
        sixel_dither_set_pipeline_accumulation_buffer_hint(dither,
            retained, (size_t)GPUPAL_DIRECT_PIXELS * 3U,
            valid, (size_t)GPUPAL_DIRECT_PIXELS,
            GPUPAL_DIRECT_WIDTH, GPUPAL_DIRECT_HEIGHT, 0, 0,
            GPUPAL_DIRECT_WIDTH, GPUPAL_DIRECT_HEIGHT, ncolors, 1, 8U,
            SIXEL_6DELTA_ERROR_DIFFUSE);
        sixel_dither_set_pipeline_accumulation_result_enabled(dither, 1);
    }
    indexes = sixel_dither_apply_palette(dither, pixels,
                                         GPUPAL_DIRECT_WIDTH,
                                         GPUPAL_DIRECT_HEIGHT);
    /* Capability is independent of compilation/dispatch success. A silent
     * direct fallback on a matrix-capable device must fail this test. */
    if (indexes != NULL && gpu_policy == SIXEL_GPU_POLICY_FORCE &&
            (!hooks.entered || hooks.kernel !=
                (hooks.matrix_supported ? 2 : 1))) {
        fprintf(stderr, "GPU kernel mismatch: matrix=%d kernel=%d\n",
            hooks.matrix_supported, hooks.kernel);
        sixel_allocator_free(allocator, indexes);
        indexes = NULL;
    }
    if (indexes != NULL && keep) {
        mask = sixel_dither_get_pipeline_accumulation_result_mask(
            dither, &mask_size);
        if (mask == NULL || mask_size != (size_t)GPUPAL_DIRECT_PIXELS) {
            sixel_allocator_free(allocator, indexes);
            indexes = NULL;
        } else {
            memcpy(mask_copy, mask, mask_size);
        }
    }
    sixel_dither_unref(dither);
    return indexes;
}

int
test_gpupal_0020_direct_exact(int argc, char **argv)
{
    static int const sizes[] = {2, 7, 8, 9, 33, 64, 200, 255};
    static unsigned char pixels[GPUPAL_DIRECT_PIXELS * 3];
    static unsigned char retained[GPUPAL_DIRECT_PIXELS * 3];
    static unsigned char valid[GPUPAL_DIRECT_PIXELS];
    static unsigned char cpu_mask[GPUPAL_DIRECT_PIXELS];
    static unsigned char gpu_mask[GPUPAL_DIRECT_PIXELS];
    unsigned char palette[(SIXEL_PALETTE_MAX + 1) * 3];
    sixel_allocator_t *allocator;
    sixel_index_t *cpu;
    sixel_index_t *gpu;
    unsigned int state;
    int size;
    int keep;
    int ncolors;
    int pixel;
    int ok;

    (void)argc;
    (void)argv;
    allocator = NULL;
    cpu = NULL;
    gpu = NULL;
    ok = 0;
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL,
                                         NULL))) {
        goto end;
    }
    for (size = 0; size < (int)(sizeof(sizes) / sizeof(sizes[0])); ++size) {
        ncolors = sizes[size];
        for (keep = 0; keep < 2; ++keep) {
            gpupal_direct_palette(palette, ncolors,
                                  (unsigned int)(ncolors * 7 + 1));
            gpupal_direct_pixels(pixels, palette, ncolors,
                                 (unsigned int)(ncolors * 13 + 5));
            /* Every third pixel retains its own color, a keep wherever its
             * history is valid; the rest retain a distant color and need a
             * palette search. */
            state = (unsigned int)ncolors;
            for (pixel = 0; pixel < GPUPAL_DIRECT_PIXELS; ++pixel) {
                valid[pixel] = (unsigned char)
                    (gpupal_direct_next(&state) % 4U != 0U);
                if (pixel % 3 == 0) {
                    memcpy(retained + pixel * 3, pixels + pixel * 3, 3);
                } else {
                    retained[pixel * 3 + 0] =
                        (unsigned char)(pixels[pixel * 3 + 0] ^ 0x80);
                    retained[pixel * 3 + 1] =
                        (unsigned char)(pixels[pixel * 3 + 1] ^ 0x80);
                    retained[pixel * 3 + 2] =
                        (unsigned char)(pixels[pixel * 3 + 2] ^ 0x80);
                }
            }
            cpu = gpupal_direct_apply(allocator, palette, ncolors, pixels,
                                      keep, retained, valid,
                                      SIXEL_GPU_POLICY_OFF, cpu_mask);
            gpu = gpupal_direct_apply(allocator, palette, ncolors, pixels,
                                      keep, retained, valid,
                                      SIXEL_GPU_POLICY_FORCE, gpu_mask);
            if (cpu == NULL || gpu == NULL) {
                fprintf(stderr, "apply failed: colors=%d keep=%d\n",
                        ncolors, keep);
                goto end;
            }
            for (pixel = 0; pixel < GPUPAL_DIRECT_PIXELS; ++pixel) {
                if (cpu[pixel] != gpu[pixel] ||
                        (keep && cpu_mask[pixel] != gpu_mask[pixel])) {
                    fprintf(stderr,
                            "mismatch: colors=%d keep=%d pixel=%d "
                            "cpu=%d gpu=%d\n",
                            ncolors, keep, pixel, (int)cpu[pixel],
                            (int)gpu[pixel]);
                    goto end;
                }
            }
            sixel_allocator_free(allocator, cpu);
            sixel_allocator_free(allocator, gpu);
            cpu = NULL;
            gpu = NULL;
        }
    }
    ok = 1;

end:
    if (allocator != NULL) {
        sixel_allocator_free(allocator, cpu);
        sixel_allocator_free(allocator, gpu);
        sixel_allocator_unref(allocator);
    }
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
