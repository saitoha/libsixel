/* SPDX-License-Identifier: MIT */
/* Verify signed TIFF CIELAB16 through the public loader frame. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "src/cms.h"
#include "src/frame-private.h"
#include "src/loader.h"
#include "src/loader-manager.h"
#include "tests/data/color-numeric/tiff-lab16-values.h"
#include "tests/data/color-numeric/tiff-lab16-mid-values.h"

typedef struct lab16_context {
    int mode;
    int calls;
    int misses;
} lab16_context_t;

static SIXELSTATUS
lab16_capture(sixel_frame_t *frame, void *context)
{
    lab16_context_t *state;
    float const *pixels;
    double lo;
    double hi;
    size_t pixel;
    size_t channel;
    size_t count;
    int format;
    int colorspace;

    state = context;
    ++state->calls;
    count = state->mode == 4 ? 4u : 15u;
    format = state->mode == 0 || state->mode == 4 ? SIXEL_PIXELFORMAT_RGBFLOAT32
                             : SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
    colorspace = state->mode == 0 || state->mode == 4 ? SIXEL_COLORSPACE_GAMMA
                                 : SIXEL_COLORSPACE_LINEAR;
    if (frame->width != (int)count || frame->height != 1 ||
        frame->pixelformat != format || frame->colorspace != colorspace ||
        frame->ncolors != -1 || frame->transparent != -1 ||
        frame->transparent_mask != NULL ||
        frame->transparent_mask_size != 0u ||
        frame->alpha_zero_is_transparent != 0) {
        fprintf(stderr, "LAB16|metadata-failed\n");
        return SIXEL_BAD_INPUT;
    }
    pixels = (float const *)sixel_frame_get_pixels(frame);
    for (pixel = 0u; pixel < count; ++pixel) {
        for (channel = 0u; channel < 3u; ++channel) {
            if (!isfinite(pixels[pixel * 3u + channel])) {
                return SIXEL_BAD_INPUT;
            }
            /* All signed extrema are checked on the no-ICC route. The
             * source ICC's existing numeric contract covers the interior. */
            if (state->mode > 0 && state->mode < 4 &&
                !lab16_icc_check[pixel]) {
                continue;
            }
            lo = state->mode == 4
               ? lab16_mid_bounds[pixel][0][channel]
               : lab16_bounds[state->mode][pixel][0][channel];
            hi = state->mode == 4
               ? lab16_mid_bounds[pixel][1][channel]
               : lab16_bounds[state->mode][pixel][1][channel];
            if (pixels[pixel * 3u + channel] < lo ||
                pixels[pixel * 3u + channel] > hi) {
                ++state->misses;
                fprintf(stderr,
                    "LAB16|pixel=%zu|channel=%zu|value=%.17g"
                    "|lo=%.17g|hi=%.17g\n", pixel, channel,
                    (double)pixels[pixel * 3u + channel], lo, hi);
            }
        }
    }
    return state->misses == 0 ? SIXEL_OK : SIXEL_BAD_INPUT;
}

int
test_loader_0174_lab16(int argc, char **argv)
{
    sixel_loader_t *loader;
    sixel_cms_engine_t previous;
    sixel_cms_engine_t engine;
    lab16_context_t context;
    SIXELSTATUS status;
    char const *name;
    char order[128];
    int mode;
    int no;
    int ok;

    if (argc != 3) {
        return 2;
    }
    mode = atoi(argv[1]);
    if (mode < 0 || mode > 4) {
        return 2;
    }
    if (!loader_manager_entry_available("libtiff")) {
        return 77;
    }
    engine = mode <= 1 || mode == 4 ? SIXEL_CMS_ENGINE_BUILTIN
                      : mode == 2 ? SIXEL_CMS_ENGINE_LCMS2
                                  : SIXEL_CMS_ENGINE_COLORSYNC;
    name = mode <= 1 || mode == 4 ? "builtin"
                                : mode == 2 ? "lcms2" : "colorsync";
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(engine);
    if (sixel_cms_get_engine() != engine) {
        sixel_cms_set_engine(previous);
        return 77;
    }
    context.mode = mode;
    context.calls = 0;
    context.misses = 0;
    loader = NULL;
    no = 0;
    ok = 0;
    snprintf(order, sizeof(order),
        "libtiff:cms_engine=%s:cms_target=linear:prefer_8bit=0!", name);
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
                                  order);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_loader_load_file(loader, argv[2], lab16_capture);
    ok = SIXEL_SUCCEEDED(status) && context.calls == 1 && context.misses == 0;
end:
    sixel_loader_unref(loader);
    sixel_cms_set_engine(previous);
    fprintf(stderr, "LAB16|mode=%d|calls=%d|misses=%d|verified=%d\n",
        mode, context.calls, context.misses, ok);
    return ok ? 0 : 1;
}
