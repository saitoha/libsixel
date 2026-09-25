/* SPDX-License-Identifier: MIT */
/* Compare concurrent bands with the same bands independently reconstructed. */
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/dither.h"

int
test_clamp_0043(int argc, char **argv)
{
    sixel_dither_t *parallel;
    sixel_dither_t *serial;
    sixel_index_t *actual;
    sixel_index_t *band;
    unsigned char palette[6] = { 0, 0, 0, 255, 255, 255 };
    float input[8 * 8 * 3];
    float working[8 * 8 * 3];
    int n;
    int y;
    int start;
    int rows;
    int ok;

    (void)argc;
    (void)argv;
    parallel = NULL;
    serial = NULL;
    actual = NULL;
    band = NULL;
    ok = 0;
    if (SIXEL_FAILED(sixel_dither_new(&parallel, 2, NULL)) ||
            SIXEL_FAILED(sixel_dither_new(&serial, 2, NULL))) {
        goto end;
    }
    sixel_dither_set_palette(parallel, palette);
    sixel_dither_set_palette(serial, palette);
    sixel_dither_set_diffusion_type(parallel, SIXEL_DIFFUSE_FS);
    sixel_dither_set_diffusion_type(serial, SIXEL_DIFFUSE_FS);
    sixel_dither_set_lut_policy(parallel, SIXEL_LUT_POLICY_NONE);
    sixel_dither_set_lut_policy(serial, SIXEL_LUT_POLICY_NONE);
    sixel_dither_set_pixelformat(parallel, SIXEL_PIXELFORMAT_RGBFLOAT32);
    sixel_dither_set_pixelformat(serial, SIXEL_PIXELFORMAT_RGBFLOAT32);
    parallel->force_palette = serial->force_palette = 1;
    parallel->prefer_float32 = serial->prefer_float32 = 1;
    if (SIXEL_FAILED(sixel_dither_set_clamp_policy(parallel,
            SIXEL_DITHER_CLAMP_SOFT, 1.01)) ||
            SIXEL_FAILED(sixel_dither_set_clamp_policy(serial,
                SIXEL_DITHER_CLAMP_SOFT, 1.01))) {
        goto end;
    }
    parallel->pipeline_parallel_active = 1;
    parallel->pipeline_dither_threads = 2;
    parallel->pipeline_band_height = 2;
    parallel->pipeline_band_overlap = 1;
    for (n = 0; n < 8 * 8 * 3; ++n) {
        input[n] = (float)((n * 17 + 13) % 151 - 25) / 100.0f;
    }
    memcpy(working, input, sizeof(input));
    actual = sixel_dither_apply_palette(parallel,
        (unsigned char *)working, 8, 8);
    if (actual == NULL) {
        goto end;
    }
    for (y = 0; y < 8; y += 2) {
        start = y == 0 ? 0 : y - 1;
        rows = y + 2 - start;
        memcpy(working, input + start * 8 * 3,
               (size_t)rows * 8 * 3 * sizeof(float));
        band = sixel_dither_apply_palette(serial,
            (unsigned char *)working, 8, rows);
        if (band == NULL || memcmp(actual + y * 8,
                band + (y - start) * 8, 16 * sizeof(*band)) != 0) {
            goto end;
        }
        sixel_allocator_free(serial->allocator, band);
        band = NULL;
    }
    ok = 1;
end:
    if (parallel != NULL) {
        sixel_allocator_free(parallel->allocator, actual);
    }
    if (serial != NULL) {
        sixel_allocator_free(serial->allocator, band);
    }
    sixel_dither_unref(parallel);
    sixel_dither_unref(serial);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
