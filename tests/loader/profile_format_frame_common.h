/* SPDX-License-Identifier: MIT */
/* Public PSD frames must recover from original inks after ICC rejection. */
#ifndef TEST_PROFILE_FORMAT_FRAME_COMMON_H
#define TEST_PROFILE_FORMAT_FRAME_COMMON_H
#include <math.h>
#include <stdio.h>
#include "src/cms.h"
#include "src/frame-private.h"
#include "src/loader.h"
#include "src/loader-manager.h"
#include "tests/data/color-numeric/profile-format-caller.h"

typedef struct profile_format_frame_context {
    int mode;
    int valid;
    int calls;
} profile_format_frame_context_t;

static SIXELSTATUS
profile_format_frame_capture(sixel_frame_t *frame, void *context)
{
    profile_format_frame_context_t *state;
    float const *pixels;
    double const (*bounds)[3];
    float value;
    size_t pixel;
    size_t channel;

    state = context;
    ++state->calls;
    if (frame->width != 16 || frame->height != 16 ||
        sixel_frame_get_pixelformat(frame) !=
            SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 ||
        frame->colorspace != SIXEL_COLORSPACE_LINEAR) {
        return SIXEL_BAD_INPUT;
    }
    pixels = (float const *)sixel_frame_get_pixels(frame);
    for (pixel = 0u; pixel < 256u; ++pixel) {
        bounds = state->valid ? pf_psd_valid[state->mode][pixel]
                                 : pf_psd_fallback[state->mode][pixel];
        for (channel = 0u; channel < 3u; ++channel) {
            value = pixels[pixel * 3u + channel];
            if (!isfinite(value) || value < bounds[0][channel] ||
                value > bounds[1][channel]) {
                fprintf(stderr, "Profile caller mode=%d valid=%d "
                    "pixel=%zu channel=%zu value=%.9g "
                    "interval=[%.17g,%.17g]\n", state->mode,
                    state->valid, pixel, channel, value,
                    bounds[0][channel], bounds[1][channel]);
                return SIXEL_BAD_INPUT;
            }
        }
        if (frame->transparent_mask != NULL &&
            frame->transparent_mask[pixel] != 0u) {
            return SIXEL_BAD_INPUT;
        }
    }
    return SIXEL_OK;
}

static int
profile_format_frame(int argc, char **argv, int mode)
{
    profile_format_frame_context_t context;
    sixel_cms_engine_t previous;
    sixel_loader_t *loader;
    SIXELSTATUS status;
    int file;
    int no;
    int ok;

    if (argc != 4) {
        return 1;
    }
    if (!loader_manager_entry_available("builtin")) {
        return 77;
    }
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_COLORSYNC);
    if (sixel_cms_get_engine() != SIXEL_CMS_ENGINE_COLORSYNC) {
        sixel_cms_set_engine(previous);
        return 77;
    }
    context.mode = mode;
    no = 0;
    ok = 1;
    /* Correct-profile, no-profile, and inconsistent-profile specimens
     * share original stored inks. Check every component against the
     * independent model appropriate to that path, including layer alpha. */
    for (file = 1; ok && file < 4; ++file) {
        context.valid = file == 1;
        context.calls = 0;
        loader = NULL;
        status = sixel_loader_new(&loader, NULL);
        if (SIXEL_SUCCEEDED(status)) {
            status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_CONTEXT,
                                          &context);
        }
        if (SIXEL_SUCCEEDED(status)) {
            status = sixel_loader_setopt(loader,
                SIXEL_LOADER_OPTION_USE_PALETTE, &no);
        }
        if (SIXEL_SUCCEEDED(status)) {
            status = sixel_loader_setopt(loader,
                SIXEL_LOADER_OPTION_LOADER_ORDER,
                "builtin:cms_engine=colorsync:cms_target=linear:"
                "prefer_8bit=0!");
        }
        if (SIXEL_SUCCEEDED(status)) {
            status = sixel_loader_load_file(loader, argv[file],
                profile_format_frame_capture);
        }
        ok = SIXEL_SUCCEEDED(status) && context.calls == 1;
        sixel_loader_unref(loader);
    }
    sixel_cms_set_engine(previous);
    if (ok) {
        fprintf(stderr, "PFFRAME|mode=%d|correct-profile-and-original-"
            "input-fallback-verified\n", mode);
    }
    return ok ? 0 : 1;
}
#endif
