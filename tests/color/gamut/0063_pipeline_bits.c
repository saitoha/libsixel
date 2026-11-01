/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/output/gamut-mapping.md
 */
#include "../../_support/processing/gamut/gamut_filter_common.h"

int
test_gamut_0063_pipeline_bits(int argc, char **argv)
{
    int const formats[] = {SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
        SIXEL_PIXELFORMAT_RGBFLOAT32, SIXEL_PIXELFORMAT_OKLABFLOAT32,
        SIXEL_PIXELFORMAT_CIELABFLOAT32, SIXEL_PIXELFORMAT_DIN99DFLOAT32};
    int const policies[] = {SIXEL_GAMUT_LOCAL_MINDE,
        SIXEL_GAMUT_EDGE_SEEKER, SIXEL_GAMUT_RAY_TRACE};
    gamut_filter_fixture_t a = {0}, b = {0};
    sixel_thread_budget_scope_t scope;
    float *left, *right;
    size_t i, count;
    int ok, policy, target, budget;
    char text[32];

    if (argc != 4) return EXIT_FAILURE;
    policy = atoi(argv[1]);
    target = atoi(argv[2]);
    budget = atoi(argv[3]);
    if (policy < 0 || policy >= 3 || target < 0 || target >= 5 ||
            (budget != 2 && budget != 4)) return EXIT_FAILURE;
    /* Two complete production tiles plus a tail force the pipeline path. */
    count = 262145;
    left = malloc(count * 3 * sizeof(float));
    right = malloc(count * 3 * sizeof(float));
    ok = left != NULL && right != NULL;
    if (ok) {
        for (i = 0; i < count; ++i) {
            left[i * 3] = i % 257 == 0 ? 1.2f : .3f;
            left[i * 3 + 1] = .4f;
            left[i * 3 + 2] = i % 257 == 0 ? -.1f : .7f;
        }
        memcpy(right, left, count * 3 * sizeof(float));
        ok = gamut_filter_init(&a, left, count,
            SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, formats[target],
            policies[policy]) && gamut_filter_init(&b, right, count,
            SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, formats[target],
            policies[policy]);
        snprintf(text, sizeof(text), "balanced:global_budget=%d",
            budget);
        ok = ok && gamut_filter_scope(&scope, text);
        if (ok) {
            ok = SIXEL_SUCCEEDED(gamut_filter_serial(&a)) &&
                sixel_filter_pipeline_run(b.color, b.gamut,
                    b.allocator, NULL) == SIXEL_OK &&
                !memcmp(left, right, count * 3 * sizeof(float)) &&
                sixel_frame_get_pixelformat(b.frame) ==
                    formats[target];
            sixel_thread_budget_leave(&scope);
        }
        gamut_filter_free(&a);
        gamut_filter_free(&b);
    }
    free(left);
    free(right);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
