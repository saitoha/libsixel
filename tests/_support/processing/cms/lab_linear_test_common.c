/* SPDX-License-Identifier: MIT */
/* Ordinary-library observations use independent physical Lab expectations.
 * Synthetic fault copies are isolated in lab_linear_fault_test_common.c. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/cms.h"
#include "src/frame-private.h"
#include "src/loader.h"
#include "src/loader-manager.h"
#include "tests/data/color-numeric/tiff-lab-linear.h"
#include "tests/data/color-numeric/profile-format.h"
#include "lab_linear_test_common.h"

typedef struct lab_linear_context {
    int engine;
    int words;
    int calls;
} lab_linear_context_t;

static int
lab_linear_values(float const *pixels, int engine, int words)
{
    double lo;
    double hi;
    double value;
    size_t pixel;
    size_t channel;
    int negative;
    int above_one;
    int ok;

    negative = 0;
    above_one = 0;
    ok = 1;
    for (pixel = 0u; pixel < 521u; ++pixel) {
        for (channel = 0u; channel < 3u; ++channel) {
            lo = tll_bounds[words][pixel][engine - 2][0][channel];
            hi = tll_bounds[words][pixel][engine - 2][1][channel];
            value = pixels[pixel * 3u + channel];
            if (!isfinite(value) || value < lo || value > hi) {
                fprintf(stderr, "TLIN|pixel=%zu|channel=%zu|value=%.17g|"
                    "lo=%.17g|hi=%.17g\n", pixel, channel, value, lo, hi);
                ok = 0;
            }
            /* Require ranges predicted by the oracle, not a rounding sign. */
            negative |= hi < 0.0 && value < 0.0;
            above_one |= lo > 1.0 && value > 1.0;
        }
    }
    return ok && negative && above_one;
}

static int
lab_linear_guards(unsigned char const *arena)
{
    size_t i;

    for (i = 0u; i < 32u; ++i) {
        if (arena[i] != 0xa5 ||
            arena[32u + sizeof(tll_source[0]) + i] != 0xa5) {
            return 0;
        }
    }
    return 1;
}

int
lab_linear_profile_test(int engine)
{
    int const wrong_profiles[4] = {0, 1, 2, 7};
    int const wrong_formats[2] = {SIXEL_PIXELFORMAT_RGBFLOAT32,
                                  SIXEL_PIXELFORMAT_LINEARRGBFLOAT32};
    sixel_cms_engine_t previous;
    sixel_cms_profile_t *lab;
    sixel_cms_profile_t *profile;
    unsigned char original[sizeof(tll_source[0]) + 64u];
    unsigned char *arena;
    size_t i;
    int applied;
    int ok;

    previous = sixel_cms_get_engine();
    sixel_cms_set_engine((sixel_cms_engine_t)engine);
    if ((int)sixel_cms_get_engine() != engine) {
        sixel_cms_set_engine(previous);
        return 77;
    }
    arena = malloc(sizeof(original));
    lab = sixel_cms_open_profile_from_mem(pf_profiles[6].bytes,
                                           pf_profiles[6].size);
    profile = NULL;
    ok = 0;
    if (arena == NULL || lab == NULL) { goto cleanup; }
    memset(original, 0xa5, sizeof(original));
    memcpy(original + 32u, tll_source[0], sizeof(tll_source[0]));
    memcpy(arena, original, sizeof(original));
    applied = sixel_cms_convert_profile_to_linearrgb(arena + 32u, 521, 1,
        SIXEL_PIXELFORMAT_CIELABFLOAT32, lab);
    ok = applied && lab_linear_guards(arena) &&
         lab_linear_values((float const *)(arena + 32u), engine, 0);
    fprintf(stderr, "TLINPROFILE|negative-begin\n");
    for (i = 0u; i < 4u; ++i) {
        profile = sixel_cms_open_profile_from_mem(
            pf_profiles[wrong_profiles[i]].bytes,
            pf_profiles[wrong_profiles[i]].size);
        /* XYZ is UNKNOWN to the facade. A backend may reject that profile
         * at open; the known RGB/Gray/CMYK profiles must remain openable. */
        if (profile == NULL && i != 3u) { ok = 0; }
        memcpy(arena, original, sizeof(original));
        applied = sixel_cms_convert_profile_to_linearrgb(arena + 32u, 521, 1,
            SIXEL_PIXELFORMAT_CIELABFLOAT32, profile);
        if (applied || memcmp(arena, original, sizeof(original)) != 0) {
            ok = 0;
        }
        sixel_cms_close_profile(profile);
        profile = NULL;
    }
    for (i = 0u; i < 2u; ++i) {
        memcpy(arena, original, sizeof(original));
        applied = sixel_cms_convert_profile_to_linearrgb(arena + 32u, 521, 1,
            wrong_formats[i], lab);
        if (applied || memcmp(arena, original, sizeof(original)) != 0) {
            ok = 0;
        }
    }
cleanup:
    sixel_cms_close_profile(profile);
    sixel_cms_close_profile(lab);
    free(arena);
    sixel_cms_set_engine(previous);
    fprintf(stderr, "TLINPROFILE|engine=%d|verified=%d\n", engine, ok);
    return ok ? 0 : 1;
}

static SIXELSTATUS
lab_linear_capture(sixel_frame_t *frame, void *context)
{
    lab_linear_context_t *state;

    state = context;
    ++state->calls;
    if (frame->width != 521 || frame->height != 1 ||
        frame->pixelformat != SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 ||
        frame->colorspace != SIXEL_COLORSPACE_LINEAR ||
        frame->ncolors != -1 || frame->transparent != -1 ||
        frame->transparent_mask != NULL ||
        frame->transparent_mask_size != 0u ||
        frame->alpha_zero_is_transparent != 0) {
        fprintf(stderr, "TLIN|metadata-failed\n");
        return SIXEL_BAD_INPUT;
    }
    return lab_linear_values((float const *)sixel_frame_get_pixels(frame),
                              state->engine, state->words)
         ? SIXEL_OK : SIXEL_BAD_INPUT;
}

int
lab_linear_frame_test(int argc, char **argv, int engine, int words)
{
    sixel_cms_engine_t previous;
    sixel_loader_t *loader;
    lab_linear_context_t context;
    SIXELSTATUS status;
    char order[128];
    int no;
    int ok;

    if (argc != 2) { return 2; }
    if (!loader_manager_entry_available("libtiff")) { return 77; }
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine((sixel_cms_engine_t)engine);
    if ((int)sixel_cms_get_engine() != engine) {
        sixel_cms_set_engine(previous);
        return 77;
    }
    context.engine = engine;
    context.words = words;
    context.calls = 0;
    loader = NULL;
    no = 0;
    ok = 0;
    snprintf(order, sizeof(order),
        "libtiff:cms_engine=%s:cms_target=linear:prefer_8bit=0!",
        sixel_cms_engine_to_string((sixel_cms_engine_t)engine));
    status = sixel_loader_new(&loader, NULL);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_CONTEXT,
                                  &context);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    sixel_loader_set_prefer_float32(loader, 1);
    status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_USE_PALETTE,
                                  &no);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_LOADER_ORDER,
                                  order);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    status = sixel_loader_load_file(loader, argv[1], lab_linear_capture);
    ok = SIXEL_SUCCEEDED(status) && context.calls == 1;
cleanup:
    sixel_loader_unref(loader);
    sixel_cms_set_engine(previous);
    fprintf(stderr, "TLINFRAME|engine=%d|words=%d|verified=%d\n",
        engine, words, ok);
    return ok ? 0 : 1;
}
