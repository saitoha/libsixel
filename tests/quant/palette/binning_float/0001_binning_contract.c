/* SPDX-License-Identifier: MIT */
/* Independent weighted means and separation outside nominal bin bounds. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include <6cells.h>
#include "src/palette.h"
#include "src/palette-kmeans.h"

static int const bin_formats[] = {
    SIXEL_PIXELFORMAT_RGBFLOAT32, SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
    SIXEL_PIXELFORMAT_OKLABFLOAT32, SIXEL_PIXELFORMAT_CIELABFLOAT32,
    SIXEL_PIXELFORMAT_DIN99DFLOAT32
};
static int const bin_policies[] = {
    SIXEL_PALETTE_BINNING_EXACT, SIXEL_PALETTE_BINNING_HARD,
    SIXEL_PALETTE_BINNING_SOFT, SIXEL_PALETTE_BINNING_AUTO
};

static SIXELSTATUS
bin_build(sixel_palette_t *palette, float *samples, unsigned int count,
          unsigned int colors, int space, int policy, int extended)
{
    sixel_palette_generate_request_t request;
    sixel_palette_build_attempt_t attempt;
    SIXELSTATUS status;
    SIXELSTATUS finish;

    memset(&request, 0, sizeof(request));
    memset(&attempt, 0, sizeof(attempt));
    request.data = samples;
    request.length = count * 3u * sizeof(float);
    request.pixelformat = bin_formats[space];
    request.requested_colors = colors;
    request.prefer_float32 = 1;
    request.quantize_model = SIXEL_QUANTIZE_MODEL_KMEANS;
    request.final_merge_mode = SIXEL_FINAL_MERGE_NONE;
    request.lut_policy = SIXEL_LUT_POLICY_NONE;
    sixel_palette_binning_state_init(&attempt.binning, bin_policies[policy],
        policy == 3 ? SIXEL_PALETTE_POLICY_ORIGIN_AUTO
                    : SIXEL_PALETTE_POLICY_ORIGIN_EXPLICIT);
    status = sixel_palette_build_attempt_begin(palette, &attempt);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    status = extended ? sixel_palette_generate_extended(palette, &request)
        : palette->vtbl->generate(palette, &request);
    finish = sixel_palette_build_attempt_finish(palette, &attempt);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (SIXEL_FAILED(finish) ||
            attempt.binning.policy.effective !=
                (policy == 3 ? SIXEL_PALETTE_BINNING_HARD
                             : bin_policies[policy]) ||
            attempt.binning.policy.phase != SIXEL_PALETTE_POLICY_EXECUTED) {
        return SIXEL_LOGIC_ERROR;
    }
    return status;
}

static int
bin_mean(sixel_palette_t *palette, int space, int policy, int recover)
{
    float samples[12] = {
        2.0f, -2.0f, .5f, 2.0f, -2.0f, .5f, 2.0f, -2.0f, .5f,
        -.25f, 1.25f, 1.5f
    };
    double expected[3] = { 1.4375, -1.1875, .75 };
    sixel_palette_float32_entries_view_t view;
    SIXELSTATUS status;
    int n;

    if (recover) {
        /* Each failed attempt must leave the palette usable. Neither NaN
         * nor either infinity may be mistaken for an outlying finite bin. */
        for (n = 0; n < 3; ++n) {
            samples[0] = n == 0 ? NAN : n == 1 ? INFINITY : -INFINITY;
            status = bin_build(palette, samples, 4, 1, space, policy, 1);
            if (SIXEL_SUCCEEDED(status)) {
                fprintf(stderr, "nonfinite status %d\n", status);
                return 0;
            }
        }
        samples[0] = 2.0f;
    }
    if (SIXEL_FAILED(bin_build(palette, samples, 4, 1, space, policy, 1)) ||
            SIXEL_FAILED(palette->vtbl->get_entries_float32(palette, &view)) ||
            view.entry_count != 1 || view.entries == NULL) {
        return 0;
    }
    for (n = 0; n < 3; ++n) {
        /* The mean is rational and binary-exact. Allow several binary32
         * ULPs for fractional soft-bin accumulation and solver ordering. */
        if (!isfinite(view.entries[n]) ||
                fabs(view.entries[n] - expected[n]) > 1e-6) {
            fprintf(stderr, "mean[%d] %.9g != %.9g\n", n,
                (double)view.entries[n], expected[n]);
            return 0;
        }
    }
    return 1;
}

static int
bin_edges(sixel_palette_t *palette, int space, int policy)
{
    float const values[4] = { -4.0f, -2.0f, 2.0f, 4.0f };
    float samples[12];
    sixel_palette_float32_entries_view_t view;
    unsigned int found;
    unsigned int row;
    unsigned int expected;
    unsigned int axis;
    unsigned int channel;
    int match;

    /* Four separated colors must survive on each axis, including opponent
     * axes. A saturated address would collapse them to two edge cells even
     * at maximum bin depth. No palette ordering is assumed. */
    for (axis = 0; axis < 3; ++axis) {
        for (row = 0; row < 4; ++row) {
            for (channel = 0; channel < 3; ++channel) {
                samples[row * 3 + channel] = channel == axis
                    ? values[row] : .125f;
            }
        }
        if (SIXEL_FAILED(bin_build(palette, samples, 4, 4, space,
                policy, 1)) ||
                SIXEL_FAILED(palette->vtbl->get_entries_float32(palette,
                    &view)) || view.entry_count != 4 || view.entries == NULL) {
            return 0;
        }
        found = 0;
        for (row = 0; row < 4; ++row) {
            for (expected = 0; expected < 4; ++expected) {
                match = 1;
                for (channel = 0; channel < 3; ++channel) {
                    if (!isfinite(view.entries[row * 3 + channel]) ||
                            fabs(view.entries[row * 3 + channel] -
                                samples[expected * 3 + channel]) > 2e-6) {
                        match = 0;
                    }
                }
                if (match) {
                    found |= 1u << expected;
                }
            }
        }
        if (found != 15u) {
            fprintf(stderr, "edge axis %u preserved mask %u\n", axis, found);
            return 0;
        }
    }
    return 1;
}

static int
bin_nominal(sixel_palette_t *palette, int policy)
{
    float samples[18] = {
        .125f, .25f, .5f, .125f, .25f, .5f,
        .25f, .125f, .375f, .5f, .625f, .75f,
        .875f, .75f, .625f, .875f, .75f, .625f
    };
    sixel_palette_float32_entries_view_t view;
    float saved[6];
    int n;

    /* This comparative control protects unchanged nominal grid behavior;
     * the independent means/edges above establish arithmetic correctness. */
    if (SIXEL_FAILED(bin_build(palette, samples, 6, 2, 0, policy, 0)) ||
            SIXEL_FAILED(palette->vtbl->get_entries_float32(palette, &view)) ||
            view.entry_count != 2 || view.entries == NULL) {
        return 0;
    }
    memcpy(saved, view.entries, sizeof(saved));
    if (SIXEL_FAILED(bin_build(palette, samples, 6, 2, 0, policy, 1)) ||
            SIXEL_FAILED(palette->vtbl->get_entries_float32(palette, &view)) ||
            view.entry_count != 2 || view.entries == NULL) {
        return 0;
    }
    for (n = 0; n < 6; ++n) {
        if (!isfinite(saved[n]) || !isfinite(view.entries[n]) ||
                fabs(saved[n] - view.entries[n]) > 2e-6) {
            return 0;
        }
    }
    return 1;
}

int
test_palette_binning_float(int argc, char **argv)
{
    sixel_allocator_t *allocator;
    sixel_palette_t *palette;
    void *object;
    int space;
    int policy;
    int ok;

    if (argc != 5) {
        return EXIT_FAILURE;
    }
    space = atoi(argv[2]);
    policy = atoi(argv[3]);
    if (space < 0 || space >= 5 || policy < 0 || policy >= 4) {
        return EXIT_FAILURE;
    }
    allocator = NULL;
    palette = NULL;
    object = NULL;
    ok = 0;
    /* Single-axis fixtures need distinct deterministic seeds. The contract
     * is bin separation, not a stochastic solver reaching a global minimum. */
    sixel_set_kmeans_init_type_override(1, SIXEL_PALETTE_KMEANS_INIT_PCA);
    sixel_set_kmeans_seed_override(1, 1u);
    sixel_set_kmeans_binbits_override(1, 4u);
    sixel_set_kmeans_mapping_mode_override(1, atoi(argv[4])
        ? SIXEL_PALETTE_KMEANS_MAPPING_SRGB
        : SIXEL_PALETTE_KMEANS_MAPPING_UNIFORM);
    if (SIXEL_SUCCEEDED(sixel_allocator_new(&allocator, NULL, NULL,
                NULL, NULL)) &&
            SIXEL_SUCCEEDED(sixel_palette_factory_new(allocator, &object))) {
        palette = object;
        if (strcmp(argv[1], "edge") == 0) {
            ok = bin_edges(palette, space, policy);
        } else if (strcmp(argv[1], "nominal") == 0) {
            ok = bin_nominal(palette, policy);
        } else {
            ok = bin_mean(palette, space, policy,
                strcmp(argv[1], "recover") == 0);
        }
    }
    if (!ok) {
        fprintf(stderr, "%s space=%d policy=%d: %s\n", argv[1], space,
            policy, sixel_helper_get_additional_message());
    }
    if (palette != NULL) {
        palette->vtbl->unref(palette);
    }
    sixel_allocator_unref(allocator);
    sixel_set_kmeans_init_type_override(0, SIXEL_PALETTE_KMEANS_INIT_AUTO);
    sixel_set_kmeans_seed_override(0, 0u);
    sixel_set_kmeans_binbits_override(0, 0u);
    sixel_set_kmeans_mapping_mode_override(0,
        SIXEL_PALETTE_KMEANS_MAPPING_UNIFORM);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
