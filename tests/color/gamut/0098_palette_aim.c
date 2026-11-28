/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/output/gamut-mapping.md
 */
#include "../../_support/processing/gamut/gamut_test_common.h"
#include "src/pixelformat.h"
#include "src/target-color.h"

/*
 * Under clip, diffusion=aim measures palette entries as shown: an entry
 * outside sRGB becomes its wire-clamped color, while a representable
 * entry keeps its exact float bits.
 */
int
test_gamut_0098_palette_aim(int argc, char **argv)
{
    sixel_allocator_t *allocator = NULL;
    sixel_dither_t *dither = NULL;
    sixel_target_color_t *target = NULL;
    sixel_target_color_options_t options;
    sixel_palette_float32_entries_request_t request;
    sixel_palette_float32_entries_view_t view = {0};
    float colors[6] = {1.4f, -.1f, .15f, .25f, .5f, .75f};
    float const expected[6] = {1.0f, 0.0f, .15f, .25f, .5f, .75f};
    int ok;
    int c;

    (void)argc;
    (void)argv;
    options.aim = SIXEL_TARGET_COLOR_AIM_GAMUT;
    options.cluster = SIXEL_TARGET_COLOR_CLUSTER_SOURCE;
    options.method = SIXEL_TARGET_COLOR_METHOD_EXACT;
    options.lut_size = SIXEL_TARGET_COLOR_LUT_SIZE_DEFAULT;
    options.tolerance = SIXEL_TARGET_COLOR_TOLERANCE_DEFAULT;
    request.entries = colors;
    request.colors = 2;
    request.depth = 3 * sizeof(float);
    ok = SIXEL_SUCCEEDED(sixel_allocator_new(&allocator,
            NULL, NULL, NULL, NULL)) &&
        SIXEL_SUCCEEDED(sixel_target_color_new(&target, &options,
            SIXEL_PALETTE_OUTPUT_LEGACY, SIXEL_COLORSPACE_SRGB, NULL, NULL,
            allocator)) &&
        SIXEL_SUCCEEDED(sixel_dither_new(&dither, 2, allocator)) &&
        SIXEL_SUCCEEDED(dither->palette->vtbl->init_entries_float32(
            dither->palette, &request)) &&
        SIXEL_SUCCEEDED(sixel_palette_map_aim(dither->palette, target,
            SIXEL_COLORSPACE_SRGB_LINEAR,
            SIXEL_PIXELFORMAT_LINEARRGBFLOAT32)) &&
        SIXEL_SUCCEEDED(dither->palette->vtbl->get_entries_float32(
            dither->palette, &view)) &&
        view.entries != NULL;
    /* Linear sRGB output clamps the out-of-range channels only; the
     * in-range channel of the first entry passes through the gamma pair. */
    for (c = 0; ok && c < 3; ++c) {
        ok = isfinite(view.entries[c]) &&
            fabs(view.entries[c] - expected[c]) < 1e-6;
        if (!ok) {
            fprintf(stderr, "entry 0 channel %d: %.9g\n", c,
                (double)view.entries[c]);
        }
    }
    for (c = 3; ok && c < 6; ++c) {
        ok = view.entries[c] == expected[c];
        if (!ok) {
            fprintf(stderr, "entry 1 channel %d: %.9g\n", c,
                (double)view.entries[c]);
        }
    }
    sixel_dither_unref(dither);
    sixel_target_color_free(target);
    sixel_allocator_unref(allocator);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
