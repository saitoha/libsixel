/* SPDX-License-Identifier: MIT */
/* Check the original-input fallback after a mismatched native ICC layout. */
#ifndef TEST_COLORSYNC_CHANNEL_FRAME_COMMON_H
#define TEST_COLORSYNC_CHANNEL_FRAME_COMMON_H
#include <math.h>
#include <stdio.h>
#include "src/cms.h"
#include "src/frame-private.h"
#include "src/loader.h"
#include "src/loader-manager.h"
#include "tests/data/color-numeric/colorsync-channel-caller.h"

typedef struct colorsync_channel_frame_context {
    int depth;
    int calls;
} colorsync_channel_frame_context_t;

static SIXELSTATUS
colorsync_channel_frame_capture(sixel_frame_t *frame, void *context)
{
    colorsync_channel_frame_context_t *state;
    unsigned char const *bytes;
    float const *pixels;
    double const (*bounds)[3];
    size_t pixel;
    size_t channel;
    float value;

    state = context;
    ++state->calls;
    if (frame->width != 16 || frame->height != 16 ||
        frame->transparent_mask != NULL ||
        frame->transparent_mask_size != 0u) {
        return SIXEL_BAD_INPUT;
    }
    if (state->depth == 8) {
        /* The existing eight-bit fallback returns gamma RGB bytes. It is
         * not the linear-float path used by the sixteen-bit layer decoder. */
        if (sixel_frame_get_pixelformat(frame) != SIXEL_PIXELFORMAT_RGB888 ||
            frame->colorspace != SIXEL_COLORSPACE_GAMMA) {
            return SIXEL_BAD_INPUT;
        }
        bytes = sixel_frame_get_pixels(frame);
        for (pixel = 0u; pixel < 256u; ++pixel) {
            for (channel = 0u; channel < 3u; ++channel) {
                if (bytes[pixel * 3u + channel] !=
                    sc_psd8_rgb[pixel % 8u][channel]) {
                    return SIXEL_BAD_INPUT;
                }
            }
        }
    } else {
        if (sixel_frame_get_pixelformat(frame) !=
                SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 ||
            frame->colorspace != SIXEL_COLORSPACE_LINEAR) {
            return SIXEL_BAD_INPUT;
        }
        pixels = (float const *)sixel_frame_get_pixels(frame);
        for (pixel = 0u; pixel < 256u; ++pixel) {
            bounds = sc_psd16_bounds[pixel % 8u];
            for (channel = 0u; channel < 3u; ++channel) {
                value = pixels[pixel * 3u + channel];
                if (!isfinite(value) || value < bounds[0][channel] ||
                    value > bounds[1][channel]) {
                    return SIXEL_BAD_INPUT;
                }
            }
        }
    }
    return SIXEL_OK;
}

static int
colorsync_channel_frame(int argc, char **argv, int depth)
{
    colorsync_channel_frame_context_t context;
    sixel_cms_engine_t previous;
    sixel_loader_t *loader;
    SIXELSTATUS status;
    int file;
    int no;
    int ok;

    if (argc != 3) {
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
    context.depth = depth;
    no = 0;
    ok = 1;
    /* Validate the ICC-free control and the inconsistent-ICC specimen
     * against the same independently derived typed original-input result. */
    for (file = 1; ok && file < 3; ++file) {
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
                colorsync_channel_frame_capture);
        }
        ok = SIXEL_SUCCEEDED(status) && context.calls == 1;
        sixel_loader_unref(loader);
    }
    sixel_cms_set_engine(previous);
    if (ok) {
        fprintf(stderr, "CSCHFRAME|depth=%d|"
            "original-input-fallback-values-and-format-verified\n", depth);
    }
    return ok ? 0 : 1;
}
#endif
