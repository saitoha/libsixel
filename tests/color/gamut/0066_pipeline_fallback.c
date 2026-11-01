/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/output/gamut-mapping.md
 */
#include "../../_support/processing/gamut/gamut_filter_common.h"

int
test_gamut_0066_pipeline_fallback(int argc, char **argv)
{
    gamut_filter_fixture_t f = {0};
    sixel_thread_budget_scope_t scope;
    float *pixels;
    int ok;

    (void)argc;
    (void)argv;
    pixels = calloc(262144 * 3, sizeof(float));
    ok = pixels != NULL && gamut_filter_init(&f, pixels, 262144,
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, SIXEL_PIXELFORMAT_OKLABFLOAT32,
        SIXEL_GAMUT_RAY_TRACE) &&
        gamut_filter_scope(&scope, "balanced:global_budget=1");
    if (ok) {
        ok = sixel_filter_pipeline_run(f.color, f.gamut,
            f.allocator, NULL) == SIXEL_FALSE &&
            sixel_frame_get_pixelformat(f.frame) ==
                SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
        sixel_thread_budget_leave(&scope);
    }
    ok = ok && !sixel_filter_pipeline_eligible(
        SIXEL_PIXELFORMAT_RGB888, SIXEL_PIXELFORMAT_OKLABFLOAT32, 262144, 8);
    ok = ok && !sixel_filter_pipeline_eligible(
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
        SIXEL_PIXELFORMAT_OKLABFLOAT32, 100, 8);
    gamut_filter_free(&f);
    free(pixels);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
