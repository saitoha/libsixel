/* SPDX-License-Identifier: MIT */
/* Failed color conversion precedes any mapping failure, even across tiles. */
#include "../../_support/processing/gamut/gamut_filter_common.h"

int
test_gamut_0064_pipeline_failure(int argc, char **argv)
{
    gamut_filter_fixture_t a = {0}, b = {0};
    sixel_thread_budget_scope_t scope;
    SIXELSTATUS left_status, right_status;
    float *left, *right;
    size_t count, i;
    int ok, target;
    char detail[256];

    (void)argc;
    (void)argv;
    count = 262145;
    left = malloc(count * 3 * sizeof(float));
    right = malloc(count * 3 * sizeof(float));
    ok = left != NULL && right != NULL;
    for (target = 0; ok && target < 2; ++target) {
        for (i = 0; i < count * 3; ++i) {
            left[i] = .4f;
        }
        left[0] = 1.2f;
        left[(count - 1) * 3] = NAN;
        memcpy(right, left, count * 3 * sizeof(float));
        ok = gamut_filter_init(&a, left, count,
            SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, target
                ? SIXEL_PIXELFORMAT_OKLABFLOAT32
                : SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
            SIXEL_GAMUT_RAY_TRACE) &&
            gamut_filter_init(&b, right, count,
                SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, target
                    ? SIXEL_PIXELFORMAT_OKLABFLOAT32
                    : SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
                SIXEL_GAMUT_RAY_TRACE) &&
            gamut_filter_scope(&scope, "balanced:global_budget=2");
        if (ok) {
            left_status = gamut_filter_serial(&a);
            snprintf(detail, sizeof(detail), "%s",
                sixel_helper_get_additional_message());
            right_status = sixel_filter_pipeline_run(b.color, b.gamut,
                b.allocator, NULL);
            ok = SIXEL_FAILED(left_status) && left_status == right_status &&
                !memcmp(left, right, count * 3 * sizeof(float)) &&
                !strcmp(detail, sixel_helper_get_additional_message()) &&
                sixel_frame_get_pixelformat(a.frame) ==
                    sixel_frame_get_pixelformat(b.frame);
            sixel_thread_budget_leave(&scope);
        }
        gamut_filter_free(&a);
        gamut_filter_free(&b);
    }
    free(left);
    free(right);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
