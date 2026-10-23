/* SPDX-License-Identifier: MIT */
/* Independent FS indexes exercise real work-band geometry and soft limits. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/dither.h"
#include "src/palette.h"
#include "tests/data/color-numeric/clamp-bands.h"

int
test_clamp_0040(int argc, char **argv)
{
    static int const heights[4] = { 6, 6, 2, 7 };
    static int const overlaps[4] = { 2, 1, 2, 99 };
    unsigned char bytes[9] = { 0, 32, 223, 128, 128, 128, 255, 223, 32 };
    float values[9] = { -.25f, .125f, .875f, .5f, .5f, .5f,
                       1.25f, .875f, .125f };
    sixel_dither_t *parallel;
    sixel_index_t *actual;
    sixel_palette_float32_entries_request_t request;
    sixel_palette_float32_entries_view_t view;
    float input[8 * 17 * 3];
    float working[8 * 17 * 3];
    int scenario;
    int pass;
    int n;
    int ok;

#if defined(__PCC__) || defined(__TINYC__)
    /* Match the deliberate compiler exclusion in dither.c. */
    fputs("clamp: compiler disables parallel apply\n", stderr);
    return EXIT_SUCCESS;
#endif
    scenario = argc > 1 ? atoi(argv[1]) : 0;
    if (scenario < 0 || scenario > 3) {
        return EXIT_FAILURE;
    }
    parallel = NULL;
    actual = NULL;
    ok = 0;
    if (SIXEL_FAILED(sixel_dither_new(&parallel, 3, NULL))) {
        goto end;
    }
    sixel_dither_set_palette(parallel, bytes);
    sixel_dither_set_diffusion_type(parallel, SIXEL_DIFFUSE_FS);
    sixel_dither_set_lut_policy(parallel, SIXEL_LUT_POLICY_NONE);
    sixel_dither_set_pixelformat(parallel, SIXEL_PIXELFORMAT_RGBFLOAT32);
    parallel->method_for_scan = scenario == 1 || scenario == 3
        ? SIXEL_SCAN_SERPENTINE : SIXEL_SCAN_RASTER;
    parallel->force_palette = 1;
    parallel->prefer_float32 = 1;
    parallel->use_rgb101_palette = 1;
    memset(&request, 0, sizeof(request));
    request.entries = values;
    request.colors = 3;
    request.depth = 3 * sizeof(float);
    if (SIXEL_FAILED(parallel->palette->vtbl->init_entries_float32(
            parallel->palette, &request)) ||
            SIXEL_FAILED(sixel_palette_prepare_rgb101(parallel->palette,
                SIXEL_COLORSPACE_GAMMA, SIXEL_PALETTE_OUTPUT_LEGACY)) ||
            SIXEL_FAILED(sixel_dither_set_clamp_policy(parallel,
                SIXEL_DITHER_CLAMP_SOFT, 1.01))) {
        goto end;
    }
    for (n = 0; n < 8 * 17 * 3; ++n) {
        input[n] = (float)((n * 37 + 79) % 193 - 32) / 128.0f;
    }
    /* The oracle uses the effective 6/12-row band heights, with overlap
     * capped at half the height and a short final band. Each warm-up starts
     * from original pixels; serpentine parity uses the absolute row.
     * Its 50-digit and binary32-rounded recurrences agree on every index,
     * with >0.0017 squared-distance margin at each lookup. We assert the
     * discrete choices, not bit-identical intermediate floating arithmetic.
     * Whole-image FS, lost overlap, omitted limiting and byte projection
     * all disagree with this oracle, so a serial fallback cannot pass. */
    for (pass = 0; pass < 2; ++pass) {
        /* Pipeline hints are consumed/reset by each completed apply. */
        parallel->pipeline_parallel_active = 1;
        parallel->pipeline_band_height = heights[scenario];
        parallel->pipeline_band_overlap = overlaps[scenario];
        parallel->pipeline_dither_threads = pass + 2;
        memcpy(working, input, sizeof(input));
        actual = sixel_dither_apply_palette(parallel,
            (unsigned char *)working, 8, 17);
        if (actual == NULL) {
            goto end;
        }
        for (n = 0; n < 8 * 17; ++n) {
            if (actual[n] != clamp_band_indexes[scenario][n] - '0') {
                fprintf(stderr, "scenario=%d workers=%d pixel=%d: %u/%c\n",
                    scenario, pass + 2, n, (unsigned int)actual[n],
                    clamp_band_indexes[scenario][n]);
                goto end;
            }
        }
        /* Overlap workers own their source/limiter copies. Applying the
         * palette must preserve both the shared source and native entries. */
        if (memcmp(working, input, sizeof(input)) != 0 ||
                SIXEL_FAILED(parallel->palette->vtbl->get_entries_float32(
                    parallel->palette, &view)) || view.entries == NULL ||
                view.entry_count != 3 ||
                memcmp(view.entries, values, sizeof(values)) != 0) {
            goto end;
        }
        sixel_allocator_free(parallel->allocator, actual);
        actual = NULL;
    }
    ok = 1;
end:
    if (!ok) {
        fprintf(stderr, "parallel clamp: %s\n",
            sixel_helper_get_additional_message());
    }
    if (parallel != NULL) {
        sixel_allocator_free(parallel->allocator, actual);
    }
    sixel_dither_unref(parallel);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
