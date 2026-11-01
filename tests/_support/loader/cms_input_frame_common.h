/* SPDX-License-Identifier: MIT */
/* Independent public-frame observation of a concrete LCMS input caller. */
#ifndef TEST_CMS_INPUT_FRAME_COMMON_H
#define TEST_CMS_INPUT_FRAME_COMMON_H
#include <math.h>
#include <stdio.h>
#include "src/cms.h"
#include "src/frame-private.h"
#include "src/loader.h"
#include "src/loader-manager.h"
#include "tests/data/color-numeric/lcms-input-units.h"

typedef struct cms_input_frame_context {
    int mode;
    int calls;
} cms_input_frame_context_t;

static SIXELSTATUS
cms_input_frame_capture(sixel_frame_t *frame, void *context)
{
    cms_input_frame_context_t *state;
    float const *pixels;
    double const (*bounds)[3];
    double const (*top)[3];
    double lo;
    double hi;
    double alpha;
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
    alpha = (float)(128.0f / 255.0f);
    for (pixel = 0u; pixel < 256u; ++pixel) {
        bounds = state->mode == 3 ? cms_units_lab_bounds[pixel][0]
            : cms_units_cmyk_bounds[8u + pixel % 8u][0];
        top = cms_units_cmyk_bounds[8u + (pixel + 3u) % 8u][0];
        for (channel = 0u; channel < 3u; ++channel) {
            lo = bounds[0][channel];
            hi = bounds[1][channel];
            if (state->mode == 2) {
                lo = top[0][channel] * alpha + lo * (1.0 - alpha);
                hi = top[1][channel] * alpha + hi * (1.0 - alpha);
            }
            if (!isfinite(pixels[pixel * 3u + channel]) ||
                pixels[pixel * 3u + channel] < lo ||
                pixels[pixel * 3u + channel] > hi) {
                fprintf(stderr, "LCMS frame mode=%d pixel=%zu channel=%zu "
                    "value=%.9g interval=[%.9g,%.9g]\n", state->mode,
                    pixel, channel, pixels[pixel * 3u + channel], lo, hi);
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
cms_input_frame(int argc, char **argv, int mode)
{
    cms_input_frame_context_t context;
    sixel_cms_engine_t previous;
    sixel_loader_t *loader;
    SIXELSTATUS status;
    int no;
    int ok;

    if (argc != 2) {
        return 1;
    }
    if (!loader_manager_entry_available(mode == 3 ? "libtiff" : "builtin")) {
        return 77;
    }
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_LCMS2);
    if (sixel_cms_get_engine() != SIXEL_CMS_ENGINE_LCMS2) {
        sixel_cms_set_engine(previous);
        return 77;
    }
    context.mode = mode;
    context.calls = 0;
    loader = NULL;
    no = 0;
    ok = 0;
    status = sixel_loader_new(&loader, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_CONTEXT,
                                  &context);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    sixel_loader_set_prefer_float32(loader, 1);
    status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_USE_PALETTE, &no);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_LOADER_ORDER,
        mode == 3
        ? "libtiff:cms_engine=lcms2:cms_target=linear:prefer_8bit=0!"
        : "builtin:cms_engine=lcms2:cms_target=linear:prefer_8bit=0!");
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_loader_load_file(loader, argv[1], cms_input_frame_capture);
    ok = SIXEL_SUCCEEDED(status) && context.calls == 1;
end:
    sixel_loader_unref(loader);
    sixel_cms_set_engine(previous);
    if (ok) {
        fprintf(stderr, "LCMSFRAME|mode=%d|values-verified\n", mode);
    }
    return ok ? 0 : 1;
}
#endif
