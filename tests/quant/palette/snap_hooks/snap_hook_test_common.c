/* SPDX-License-Identifier: MIT */
/* Shared setup and independent coordinate oracles for snap boundaries. */
#include "snap_hook_test_common.h"
#include "tests/data/color-numeric/snap-native.h"
#include "src/dither.h"
#include "src/palette.h"
#include "src/palette-kmeans.h"

void
snap_hook_settings(int policy, int timing, double rate)
{
    sixel_palette_snap_options_t options;

    memset(&options, 0, sizeof(options));
    options.policy_override = options.timing_override = 1;
    options.approach_rate_override = options.channel_factor_l_override = 1;
    options.policy = policy;
    options.timing = timing;
    options.approach_rate = rate;
    options.channel_factor_l = .85;
    sixel_set_palette_snap_override(&options);
}

int
snap_hook_targets(int policy, int final)
{
    int const formats[5] = {SIXEL_PIXELFORMAT_RGBFLOAT32,
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, SIXEL_PIXELFORMAT_OKLABFLOAT32,
        SIXEL_PIXELFORMAT_CIELABFLOAT32, SIXEL_PIXELFORMAT_DIN99DFLOAT32};
    double value[3];
    float entries[3];
    double expected;
    int space;
    int channel;
    int ok;

    ok = 1;
    snap_hook_settings(policy, SIXEL_PALETTE_SNAP_TIMING_ALL, .25);
    for (space = 0; space < 5; ++space) {
        for (channel = 0; channel < 3; ++channel) {
            value[channel] = snap_native_fixture[0][space][0][channel];
            entries[channel] = (float)value[channel];
        }
        if (final) {
            sixel_palette_reversible_palette_float(entries, 1, formats[space]);
            for (channel = 0; channel < 3; ++channel) {
                value[channel] = entries[channel];
            }
        } else {
            sixel_palette_snap_triple(value, 1, formats[space],
                SIXEL_PALETTE_SNAP_STAGE_QUANTIZER_ITER);
        }
        for (channel = 0; channel < 3; ++channel) {
            /* The oracle is derived independently from stored float inputs.
             * 2e-5 covers matrix/TRC inverses and final float storage. */
            expected = .75 * snap_native_fixture[0][space][0][channel]
                + .25 * snap_native_fixture[0][space][policy][channel];
            ok = ok && isfinite(value[channel]) &&
                fabs(value[channel] - expected) <= 2e-5;
        }
    }
    sixel_set_palette_snap_override(NULL);
    return ok;
}

int
snap_hook_quantizer(int timing)
{
    float pixels[12] = {98.f / 255, 98.f / 255, 98.f / 255,
        98.25f / 255, 98.25f / 255, 98.25f / 255,
        200.f / 255, 200.f / 255, 200.f / 255,
        200.25f / 255, 200.25f / 255, 200.25f / 255};
    sixel_palette_build_attempt_t attempt;
    sixel_palette_float32_entries_view_t view;
    sixel_dither_t *dither;
    SIXELSTATUS status;
    double expected;
    double target;
    unsigned int first;
    unsigned int row;
    unsigned int channel;
    int ok;

    dither = NULL;
    ok = 0;
    snap_hook_settings(SIXEL_PALETTE_SNAP_POLICY_NEAREST, timing, .25);
    sixel_set_kmeans_seed_override(1, 1);
    sixel_set_kmeans_iter_override(1, 1);
    sixel_set_kmeans_miniter_override(1, 1);
    sixel_set_kmeans_polish_iter_override(1, 0);
    sixel_set_kmeans_feedback_mode_override(1,
        SIXEL_PALETTE_KMEANS_FEEDBACK_OFF);
    memset(&attempt, 0, sizeof(attempt));
    memset(&view, 0, sizeof(view));
    sixel_palette_binning_state_init(&attempt.binning,
        SIXEL_PALETTE_BINNING_NONE, SIXEL_PALETTE_POLICY_ORIGIN_EXPLICIT);
    if (SIXEL_FAILED(sixel_dither_new(&dither, 2, NULL))) {
        goto end;
    }
    dither->quantize_model = SIXEL_QUANTIZE_MODEL_KMEANS;
    sixel_dither_set_lut_policy(dither, SIXEL_LUT_POLICY_NONE);
    sixel_dither_set_final_merge(dither, SIXEL_FINAL_MERGE_NONE);
    status = sixel_dither_initialize_with_palette_attempt(dither,
        (unsigned char *)pixels,
        4, 1, SIXEL_PIXELFORMAT_RGBFLOAT32, SIXEL_LARGE_AUTO, SIXEL_REP_AUTO,
        SIXEL_QUALITY_FULL, &attempt);
    if (SIXEL_FAILED(status) || attempt.quantizer_retry_count != 0 ||
            attempt.quantize_model != SIXEL_QUANTIZE_MODEL_KMEANS ||
            dither->use_rgb101_palette ||
            SIXEL_FAILED(dither->palette->vtbl->get_entries_float32(
                dither->palette, &view)) || view.entries == NULL ||
            view.entry_count != 2 || view.depth != 3 * (int)sizeof(float)) {
        goto end;
    }
    first = view.entries[0] < view.entries[3] ? 0u : 1u;
    ok = 1;
    for (row = 0; row < 2; ++row) {
        /* Four source points reduce to two disjoint means. One Lloyd update
         * reaches those means regardless of initial safe-tone seed movement.
         * ALL adds one quarter-rate update before the final quarter-rate pass;
         * ONCE leaves the continuous mean untouched until the final pass. */
        expected = ((double)pixels[row * 6] + pixels[row * 6 + 3]) / 2;
        target = (row == 0 ? 99.0 : 201.0) / 255;
        if (timing == SIXEL_PALETTE_SNAP_TIMING_ALL) {
            expected = .75 * expected + .25 * target;
        }
        expected = .75 * expected + .25 * target;
        for (channel = 0; channel < 3; ++channel) {
            ok = ok && isfinite(view.entries[((first + row) % 2) * 3 + channel])
                && fabs(view.entries[((first + row) % 2) * 3 + channel]
                    - expected) <= 2e-7;
        }
    }
end:
    sixel_dither_unref(dither);
    sixel_set_palette_snap_override(NULL);
    sixel_set_kmeans_seed_override(0, 0);
    sixel_set_kmeans_iter_override(0, 0);
    sixel_set_kmeans_miniter_override(0, 0);
    sixel_set_kmeans_polish_iter_override(0, 0);
    sixel_set_kmeans_feedback_mode_override(0,
        SIXEL_PALETTE_KMEANS_FEEDBACK_OFF);
    return ok;
}
