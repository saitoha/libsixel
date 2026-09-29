/* SPDX-License-Identifier: MIT */
/* Native Heckbert observations with literal independent expectations. */
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/palette.h"
#include "src/palette-common-merge.h"

static int const heckbert_formats[] = {
    SIXEL_PIXELFORMAT_RGBFLOAT32, SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
    SIXEL_PIXELFORMAT_OKLABFLOAT32, SIXEL_PIXELFORMAT_CIELABFLOAT32,
    SIXEL_PIXELFORMAT_DIN99DFLOAT32
};

/* Fail one allocation, then permit later calls: a hidden legacy retry would
 * otherwise turn the intentionally failed native build into success. */
static int heckbert_allocations;
static int heckbert_fail_at;

static void *
heckbert_malloc(size_t size)
{
    if (++heckbert_allocations == heckbert_fail_at) {
        return NULL;
    }
    return malloc(size);
}

int
test_heckbert_native(int argc, char **argv)
{
    float pixels[18] = {
        -.5f, -.5f, -.5f, -.5f, -.5f, -.5f,
        -.5f, -.5f, -.5f, -.5f, -.5f, -.5f,
        .25f, .25f, .25f, 1.5f, 1.5f, 1.5f
    };
    double expected[18] = { 0.5, 0.5, 0.5 };
    sixel_allocator_t *allocator;
    sixel_palette_t *palette;
    void *object;
    sixel_palette_generate_request_t request;
    sixel_palette_build_attempt_t attempt;
    sixel_palette_float32_entries_view_t view;
    SIXELSTATUS status;
    SIXELSTATUS finish;
    int space;
    int mode;
    unsigned int count;
    unsigned int colors;
    unsigned int i;
    unsigned int j;
    unsigned int channel;
    int matched;
    int ok;
    double tolerance;

    if (argc != 3) {
        return EXIT_FAILURE;
    }
    mode = atoi(argv[1]);
    space = atoi(argv[2]);
    if (space < 0 || space > 4) {
        return EXIT_FAILURE;
    }
    allocator = NULL;
    palette = NULL;
    object = NULL;
    count = 6U;
    colors = 1U;
    ok = 0;
    memset(&request, 0, sizeof(request));
    memset(&view, 0, sizeof(view));
    request.method_for_largest = SIXEL_LARGE_NORM;
    request.method_for_rep = SIXEL_REP_CENTER_BOX;
    request.final_merge_mode = SIXEL_FINAL_MERGE_NONE;
    if (mode == 1 || mode == 2) {
        /* Same three distinct colors, with populations 4:1:1. */
        request.method_for_rep = mode == 1 ? SIXEL_REP_AVERAGE_COLORS
                                          : SIXEL_REP_AVERAGE_PIXELS;
        for (channel = 0U; channel < 3U; ++channel) {
            expected[channel] = mode == 1 ? 5.0 / 12.0 : -1.0 / 24.0;
        }
    } else if (mode == 3 || mode == 4 || mode == 5 || mode == 6) {
        colors = 2U;
        count = mode == 6 ? 4U : 2U;
        for (i = 0U; i < count; ++i) {
            for (channel = 0U; channel < 3U; ++channel) {
                pixels[i * 3U + channel] = mode == 3
                    ? (i ? 1.25f : -.25f) : mode == 4
                    ? (i ? .5002f : .5001f) : mode == 5
                    ? (i ? 1e-20f : -1e-20f)
                    : (i < 2U ? -FLT_MAX : FLT_MAX);
                if (i < colors) {
                    expected[i * 3U + channel] = pixels[i * 3U + channel];
                }
            }
        }
        if (mode == 6) {
            for (channel = 0U; channel < 3U; ++channel) {
                expected[3U + channel] = FLT_MAX;
            }
        }
    } else if (mode == 7 || mode == 8) {
        /* A 2 by 1.25 rectangle selects red by range, green by luminance. */
        count = 4U;
        colors = 2U;
        request.method_for_largest = mode == 7 ? SIXEL_LARGE_NORM
                                               : SIXEL_LARGE_LUM;
        for (i = 0U; i < count; ++i) {
            pixels[i * 3U] = i / 2U ? 2.0f : 0.0f;
            pixels[i * 3U + 1U] = i % 2U ? 1.25f : 0.0f;
            pixels[i * 3U + 2U] = .125f;
        }
        for (i = 0U; i < colors; ++i) {
            expected[i * 3U] = mode == 7 ? i * 2.0 : 1.0;
            expected[i * 3U + 1U] = mode == 7 ? .625 : i * 1.25;
            expected[i * 3U + 2U] = .125;
        }
    } else if (mode == 9) {
        /* Correlation changes the median partition from the range split.
         * PCA groups (0,0) with (1.1,-1), leaving (1,2) with (3,2). */
        count = 4U;
        colors = 2U;
        request.method_for_largest = SIXEL_LARGE_PCA;
        pixels[0] = 0.0f;
        pixels[1] = 0.0f;
        pixels[2] = .125f;
        pixels[3] = 1.0f;
        pixels[4] = 2.0f;
        pixels[5] = .125f;
        pixels[6] = 1.1f;
        pixels[7] = -1.0f;
        pixels[8] = .125f;
        pixels[9] = 3.0f;
        pixels[10] = 2.0f;
        pixels[11] = .125f;
        expected[0] = .55;
        expected[1] = -.5;
        expected[2] = .125;
        expected[3] = 2.0;
        expected[4] = 2.0;
        expected[5] = .125;
    } else if (mode == 10 || mode == 11) {
        count = 4U;
        colors = 2U;
        request.method_for_largest = SIXEL_LARGE_PCA;
        request.final_merge_mode = SIXEL_FINAL_MERGE_WARD;
        for (i = 0U; i < count; ++i) {
            for (channel = 0U; channel < 3U; ++channel) {
                pixels[i * 3U + channel] = i < 2U
                    ? -.5f + i * .25f : 1.25f + (i - 2U) * .25f;
            }
        }
        for (i = 0U; i < colors; ++i) {
            for (channel = 0U; channel < 3U; ++channel) {
                expected[i * 3U + channel] = i ? 1.375 : -.375;
            }
        }
    } else if (mode == 12 || mode == 13) {
        pixels[0] = mode == 12 ? NAN : INFINITY;
    }
    if (SIXEL_FAILED(sixel_allocator_new(&allocator,
            heckbert_malloc, NULL, NULL, NULL)) ||
            SIXEL_FAILED(sixel_palette_factory_new(allocator, &object))) {
        goto end;
    }
    palette = object;
    request.data = pixels;
    request.length = count * 3U * sizeof(float);
    request.pixelformat = heckbert_formats[space];
    request.requested_colors = colors;
    request.prefer_float32 = 1;
    request.quantize_model = SIXEL_QUANTIZE_MODEL_MEDIANCUT;
    request.lut_policy = SIXEL_LUT_POLICY_NONE;
    memset(&attempt, 0, sizeof(attempt));
    sixel_palette_binning_state_init(&attempt.binning,
        SIXEL_PALETTE_BINNING_NONE, SIXEL_PALETTE_POLICY_ORIGIN_EXPLICIT);
    if (SIXEL_FAILED(sixel_palette_build_attempt_begin(palette, &attempt))) {
        goto end;
    }
    sixel_set_final_merge_target_factor_override(1, 2.0);
    sixel_set_final_merge_lloyd_iterations_override(1, mode == 11 ? 3U : 0U);
    heckbert_allocations = 0;
    heckbert_fail_at = mode == 14 ? 1 : mode == 15 ? 4 : 0;
    status = sixel_palette_generate_extended(palette, &request);
    heckbert_fail_at = 0;
    finish = sixel_palette_build_attempt_finish(palette, &attempt);
    if (mode >= 12 && mode <= 15) {
        ok = SIXEL_FAILED(status);
        goto end;
    }
    if (SIXEL_FAILED(status) || SIXEL_FAILED(finish) ||
            SIXEL_FAILED(palette->vtbl->get_entries_float32(palette, &view)) ||
            view.entries == NULL || view.entry_count != colors) {
        goto end;
    }
    /* Palette order is not a color contract. Require every literal expected
     * triple once; storage arithmetic gets a small relative binary32 budget.
     * The tiny-coordinate case has no absolute floor that could hide zero. */
    for (i = 0U; i < colors; ++i) {
        matched = 0;
        for (j = 0U; j < colors; ++j) {
            for (channel = 0U; channel < 3U; ++channel) {
                tolerance = fabs(expected[i * 3U + channel]) * 5e-7;
                if (mode != 5 && tolerance < 1e-7) {
                    tolerance = 1e-7;
                }
                if (!isfinite(view.entries[j * 3U + channel]) ||
                        fabs(view.entries[j * 3U + channel]
                            - expected[i * 3U + channel]) > tolerance) {
                    break;
                }
            }
            if (channel == 3U) {
                ++matched;
            }
        }
        if (matched != 1) {
            goto end;
        }
    }
    ok = 1;
end:
    if (!ok) {
        fprintf(stderr, "Heckbert mode=%d space=%d: %s\n", mode, space,
            sixel_helper_get_additional_message());
        for (i = 0U; view.entries != NULL && i < view.entry_count; ++i) {
            fprintf(stderr, "  %g %g %g\n", (double)view.entries[i * 3U],
                (double)view.entries[i * 3U + 1U],
                (double)view.entries[i * 3U + 2U]);
        }
    }
    sixel_set_final_merge_target_factor_override(0, 1.81);
    sixel_set_final_merge_lloyd_iterations_override(0, 3U);
    if (palette != NULL) {
        palette->vtbl->unref(palette);
    }
    sixel_allocator_unref(allocator);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
