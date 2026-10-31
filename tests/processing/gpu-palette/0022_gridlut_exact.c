/*
 * SPDX-License-Identifier: MIT
 *
 * Policy: docs/encoder/palette-application/lookup-policy/gridlut.md
 */
/*
 * GPU cell candidates must equal CPU exhaustive lookup after dither.
 * Include partial rows, palette changes, cache reuse, overflow, duplicate
 * colors, ties and delta-kept pixels. The 129-color palette deliberately
 * overflows the candidate capacity so truncation would be detected.
 */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/dither.h"

enum {
    GPUPAL_GRID_WIDTH = 77,
    GPUPAL_GRID_HEIGHT = 53,
    GPUPAL_GRID_PIXELS = GPUPAL_GRID_WIDTH * GPUPAL_GRID_HEIGHT
};

static unsigned int
gpupal_grid_next(unsigned int *state)
{
    *state = *state * 1103515245U + 12345U;
    return (*state >> 16) & 0x7fffU;
}

/* Random colors whose last entry repeats the first. */
static void
gpupal_grid_palette(unsigned char *palette, int ncolors, unsigned int seed)
{
    unsigned int state;
    int index;

    state = seed;
    for (index = 0; index < ncolors * 3; ++index) {
        palette[index] = (unsigned char)(gpupal_grid_next(&state) & 255U);
    }
    if (ncolors > 2) {
        memcpy(palette + (ncolors - 1) * 3, palette, 3);
    }
    if (ncolors == 129) {
        memset(palette, 42, (size_t)ncolors * 3U);
    }
    memset(palette + ncolors * 3, 0, 3);
}

/* Rows cycle through a smooth ramp, random noise, palette hits and
 * equidistant midpoints of two palette colors. */
static void
gpupal_grid_pixels(unsigned char *pixels, unsigned char const *palette,
                   int ncolors, unsigned int seed)
{
    unsigned int state;
    int x;
    int y;
    int pixel;
    int channel;
    int a;
    int b;

    state = seed;
    for (y = 0; y < GPUPAL_GRID_HEIGHT; ++y) {
        for (x = 0; x < GPUPAL_GRID_WIDTH; ++x) {
            pixel = y * GPUPAL_GRID_WIDTH + x;
            a = (int)(gpupal_grid_next(&state) % (unsigned int)ncolors);
            b = (int)(gpupal_grid_next(&state) % (unsigned int)ncolors);
            for (channel = 0; channel < 3; ++channel) {
                switch ((y / 9) % 4) {
                case 0:
                    pixels[pixel * 3 + channel] = (unsigned char)
                        ((x * (channel + 1) + y * 2) & 255);
                    break;
                case 1:
                    pixels[pixel * 3 + channel] = (unsigned char)
                        (gpupal_grid_next(&state) & 255U);
                    break;
                case 2:
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
}

static int
gpupal_grid_oracle(unsigned char const *palette, int ncolors,
                   unsigned char const *pixel)
{
    int best;
    int minimum;
    int distance;
    int d;
    int i;
    int c;

    best = 0;
    minimum = INT_MAX;
    for (i = 0; i < ncolors; ++i) {
        distance = 0;
        for (c = 0; c < 3; ++c) {
            d = (int)pixel[c] - (int)palette[i * 3 + c];
            distance += d * d;
        }
        if (distance < minimum) {
            minimum = distance;
            best = i;
        }
    }
    return best;
}

static sixel_index_t *
gpupal_grid_apply(sixel_allocator_t *allocator,
                  unsigned char *palette,
                  int ncolors,
                  unsigned char *pixels,
                  int keep,
    int diffusion,
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
    sixel_dither_set_diffusion_type(dither, diffusion);
    sixel_dither_set_diffusion_scan(dither, SIXEL_SCAN_RASTER);
    sixel_dither_set_lut_policy(dither, gpu_policy == SIXEL_GPU_POLICY_OFF
        ? SIXEL_LUT_POLICY_NONE : SIXEL_LUT_POLICY_GRIDLUT);
    sixel_dither_set_optimize_palette(dither, 0);
    dither->gpu_policy = gpu_policy;
    if (keep) {
        sixel_dither_set_transparent(dither, ncolors);
        dither->keycolor_reserved = 1;
        sixel_dither_set_pipeline_accumulation_buffer_hint(dither,
            retained, (size_t)GPUPAL_GRID_PIXELS * 3U,
            valid, (size_t)GPUPAL_GRID_PIXELS,
            GPUPAL_GRID_WIDTH, GPUPAL_GRID_HEIGHT, 0, 0,
            GPUPAL_GRID_WIDTH, GPUPAL_GRID_HEIGHT, ncolors, 1, 8U,
            SIXEL_6DELTA_ERROR_DIFFUSE);
        sixel_dither_set_pipeline_accumulation_result_enabled(dither, 1);
    }
    indexes = sixel_dither_apply_palette(dither, pixels,
                                         GPUPAL_GRID_WIDTH,
                                         GPUPAL_GRID_HEIGHT);
    if (indexes != NULL && keep) {
        mask = sixel_dither_get_pipeline_accumulation_result_mask(
            dither, &mask_size);
        if (mask == NULL || mask_size != (size_t)GPUPAL_GRID_PIXELS) {
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
test_gpupal_0022_gridlut_exact(int argc, char **argv)
{
    static int const sizes[] = {2, 33, 64, 129, 200, 255};
    static unsigned char pixels[GPUPAL_GRID_PIXELS * 3];
    static unsigned char retained[GPUPAL_GRID_PIXELS * 3];
    static unsigned char valid[GPUPAL_GRID_PIXELS];
    static unsigned char cpu_mask[GPUPAL_GRID_PIXELS];
    static unsigned char gpu_mask[GPUPAL_GRID_PIXELS];
    unsigned char palette[(SIXEL_PALETTE_MAX + 1) * 3];
    sixel_allocator_t *allocator;
    sixel_index_t *cpu;
    sixel_index_t *gpu;
    unsigned int state;
    static int const diffusions[] = {
        SIXEL_DIFFUSE_NONE, SIXEL_DIFFUSE_BLUENOISE_DITHER,
        SIXEL_DIFFUSE_X_DITHER
    };
    int diffusion;
    int phase;
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
    for (diffusion = 0; diffusion < 3; ++diffusion) {
        for (size = 0; size < (int)(sizeof(sizes) / sizeof(sizes[0])); ++size) {
            ncolors = sizes[size];
            for (keep = 0; keep < 2; ++keep) {
                gpupal_grid_palette(palette, ncolors,
                                    (unsigned int)(ncolors * 7 + 1));
                gpupal_grid_pixels(pixels, palette, ncolors,
                                   (unsigned int)(ncolors * 13 + 5));
                state = (unsigned int)ncolors;
                for (pixel = 0; pixel < GPUPAL_GRID_PIXELS; ++pixel) {
                    valid[pixel] = (unsigned char)
                        (gpupal_grid_next(&state) % 4U != 0U);
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
                for (phase = 0; phase < 3; ++phase) {
                    /* Same table, then a different palette of equal size. */
                    if (phase == 2) {
                        palette[0] ^= 255U;
                    }
                    cpu = gpupal_grid_apply(allocator, palette, ncolors,
                                            pixels,
                                            keep, diffusions[diffusion],
                                            retained, valid,
                                            SIXEL_GPU_POLICY_OFF, cpu_mask);
                    gpu = gpupal_grid_apply(allocator, palette, ncolors,
                                            pixels,
                                            keep, diffusions[diffusion],
                                            retained, valid,
                                            SIXEL_GPU_POLICY_FORCE, gpu_mask);
                    if (cpu == NULL || gpu == NULL) {
                        fprintf(stderr, "apply failed: colors=%d keep=%d\n",
                                ncolors, keep);
                        goto end;
                    }
                    for (pixel = 0; pixel < GPUPAL_GRID_PIXELS; ++pixel) {
                        if (cpu[pixel] != gpu[pixel] ||
                                (keep && cpu_mask[pixel] != gpu_mask[pixel]) ||
                                (!keep && diffusion == 0
                                    && (int)gpu[pixel] != gpupal_grid_oracle(
                                    palette, ncolors, pixels + pixel * 3))) {
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
