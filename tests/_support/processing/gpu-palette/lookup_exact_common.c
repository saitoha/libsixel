/*
 * SPDX-License-Identifier: MIT
 *
 */
/*
 * GPU palette candidates must equal CPU exhaustive lookup after
 * dither.
 * Include partial rows, short rows, duplicate colors, ties and delta-kept
 * pixels. Cache reuse has its own invocation; invalidation is covered by
 * the policy-specific cache tests. The 129-color palette deliberately
 * repeats a single coordinate to exercise all equal-distance entries.
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
    GPUPAL_LOOKUP_WIDTH = 19,
    GPUPAL_LOOKUP_HEIGHT = 17,
    GPUPAL_LOOKUP_PIXELS = GPUPAL_LOOKUP_WIDTH * GPUPAL_LOOKUP_HEIGHT
};

static unsigned int
gpupal_lookup_next(unsigned int *state)
{
    *state = (unsigned int)((uint64_t)*state * 1103515245U
        + 12345U);
    return (*state >> 16) & 0x7fffU;
}

/* Random colors whose last entry repeats the first. */
static void
gpupal_lookup_palette(unsigned char *palette, int ncolors, unsigned int seed)
{
    unsigned int state;
    int index;

    state = seed;
    for (index = 0; index < ncolors * 3; ++index) {
        palette[index] = (unsigned char)(gpupal_lookup_next(&state) & 255U);
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
gpupal_lookup_pixels(unsigned char *pixels, unsigned char const *palette,
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
    for (y = 0; y < GPUPAL_LOOKUP_HEIGHT; ++y) {
        for (x = 0; x < GPUPAL_LOOKUP_WIDTH; ++x) {
            pixel = y * GPUPAL_LOOKUP_WIDTH + x;
            a = (int)(gpupal_lookup_next(&state) % (unsigned int)ncolors);
            b = (int)(gpupal_lookup_next(&state) % (unsigned int)ncolors);
            for (channel = 0; channel < 3; ++channel) {
                switch (y % 4) {
                case 0:
                    pixels[pixel * 3 + channel] = (unsigned char)
                        ((x * (channel + 1) + y * 2) & 255);
                    break;
                case 1:
                    pixels[pixel * 3 + channel] = (unsigned char)
                        (gpupal_lookup_next(&state) & 255U);
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
gpupal_lookup_oracle(unsigned char const *palette, int ncolors,
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
gpupal_lookup_apply(sixel_allocator_t *allocator,
                  unsigned char *palette,
                  int ncolors,
                  unsigned char *pixels,
                  int keep,
                  int diffusion,
                  unsigned char const *retained,
                  unsigned char const *valid,
                  int gpu_policy, int lookup,
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
    sixel_dither_set_lut_policy(dither, gpu_policy == SIXEL_GPU_POLICY_OFF &&
        lookup != SIXEL_LUT_POLICY_FHEDT ? SIXEL_LUT_POLICY_NONE : lookup);
    sixel_dither_set_optimize_palette(dither, 0);
    dither->gpu_policy = gpu_policy;
    if (keep) {
        sixel_dither_set_transparent(dither, ncolors);
        dither->keycolor_reserved = 1;
        sixel_dither_set_pipeline_accumulation_buffer_hint(dither,
            retained, (size_t)GPUPAL_LOOKUP_PIXELS * 3U,
            valid, (size_t)GPUPAL_LOOKUP_PIXELS,
            GPUPAL_LOOKUP_WIDTH, GPUPAL_LOOKUP_HEIGHT, 0, 0,
            GPUPAL_LOOKUP_WIDTH, GPUPAL_LOOKUP_HEIGHT, ncolors, 1, 8U,
            SIXEL_6DELTA_ERROR_DIFFUSE);
        sixel_dither_set_pipeline_accumulation_result_enabled(dither, 1);
    }
    indexes = sixel_dither_apply_palette(dither, pixels,
                                         GPUPAL_LOOKUP_WIDTH,
                                         GPUPAL_LOOKUP_HEIGHT);
    if (indexes != NULL && keep) {
        mask = sixel_dither_get_pipeline_accumulation_result_mask(
            dither, &mask_size);
        if (mask == NULL || mask_size != (size_t)GPUPAL_LOOKUP_PIXELS) {
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
gpu_lookup_exact_test(int argc, char **argv, int lookup)
{
    static unsigned char pixels[GPUPAL_LOOKUP_PIXELS * 3];
    static unsigned char retained[GPUPAL_LOOKUP_PIXELS * 3];
    static unsigned char valid[GPUPAL_LOOKUP_PIXELS];
    static unsigned char cpu_mask[GPUPAL_LOOKUP_PIXELS];
    static unsigned char gpu_mask[GPUPAL_LOOKUP_PIXELS];
    unsigned char palette[(SIXEL_PALETTE_MAX + 1) * 3];
    sixel_allocator_t *allocator;
    sixel_index_t *cpu;
    sixel_index_t *gpu;
    unsigned int state;
    static int const diffusions[] = {
        SIXEL_DIFFUSE_NONE, SIXEL_DIFFUSE_BLUENOISE_DITHER,
        SIXEL_DIFFUSE_X_DITHER, SIXEL_DIFFUSE_NONE,
        SIXEL_DIFFUSE_A_DITHER
    };
    int diffusion;
    int phase;
    int keep;
    int ncolors;
    int pixel;
    int ok;
    int reuse;

    if (argc != 4) return EXIT_FAILURE;
    diffusion = atoi(argv[1]);
    keep = atoi(argv[2]);
    ncolors = atoi(argv[3]);
    if (diffusion < 0 || diffusion > 4 || keep < 0 || keep > 1 ||
            ncolors < 1 || ncolors > (keep ? 255 : 256)) {
        return EXIT_FAILURE;
    }
    reuse = diffusion == 3;
    if (reuse) diffusion = 0;
    allocator = NULL;
    cpu = NULL;
    gpu = NULL;
    ok = 0;
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL,
                                         NULL))) {
        goto end;
    }
    /* Palette size is selected by one independently reported case. */
    gpupal_lookup_palette(palette, ncolors,
                        (unsigned int)(ncolors * 7 + 1));
    gpupal_lookup_pixels(pixels, palette, ncolors,
                       (unsigned int)(ncolors * 13 + 5));
    state = (unsigned int)ncolors;
    for (pixel = 0; pixel < GPUPAL_LOOKUP_PIXELS; ++pixel) {
        valid[pixel] = (unsigned char)
            (gpupal_lookup_next(&state) % 4U != 0U);
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
    for (phase = 0; phase < (reuse ? 2 : 1); ++phase) {
        /* A dedicated cache observation repeats exactly one palette. */
        cpu = gpupal_lookup_apply(allocator, palette, ncolors,
                                pixels,
                                keep, diffusions[diffusion],
                                retained, valid,
                                SIXEL_GPU_POLICY_OFF, lookup, cpu_mask);
        gpu = gpupal_lookup_apply(allocator, palette, ncolors,
                                pixels,
                                keep, diffusions[diffusion],
                                retained, valid,
                                SIXEL_GPU_POLICY_FORCE, lookup, gpu_mask);
        if (cpu == NULL || gpu == NULL) {
            fprintf(stderr, "apply failed: colors=%d keep=%d\n",
                    ncolors, keep);
            goto end;
        }
        for (pixel = 0; pixel < GPUPAL_LOOKUP_PIXELS; ++pixel) {
            if (cpu[pixel] != gpu[pixel] ||
                    (keep && cpu_mask[pixel] != gpu_mask[pixel]) ||
                    (!keep && diffusion == 0 && lookup != SIXEL_LUT_POLICY_FHEDT
                        && (int)gpu[pixel] != gpupal_lookup_oracle(
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
