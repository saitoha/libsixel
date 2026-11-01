/* SPDX-License-Identifier: MIT */
/* Observe the real factory/caller against fixed independent Lab values. */
#include <math.h>
#include <stdio.h>
#include "src/cms.h"
#include "src/frame-private.h"
#include "src/loader.h"
#include "src/loader-manager.h"
#include "tests/data/color-numeric/colorsync-lab.h"

static SIXELSTATUS
colorsync_lab_capture(sixel_frame_t *frame, void *context)
{
    float const *pixels;
    int *calls;
    size_t pixel;
    size_t channel;

    calls = context;
    ++*calls;
    if (frame->width != 16 || frame->height != 16 ||
        sixel_frame_get_pixelformat(frame) !=
            SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 ||
        frame->colorspace != SIXEL_COLORSPACE_LINEAR) {
        return SIXEL_BAD_INPUT;
    }
    pixels = (float const *)sixel_frame_get_pixels(frame);
    for (pixel = 0u; pixel < 256u; ++pixel) {
        for (channel = 0u; channel < 3u; ++channel) {
            /* These bounds come from the original strip and independent
             * equations. The factory under test cannot change the oracle. */
            if (!isfinite(pixels[pixel * 3u + channel]) ||
                pixels[pixel * 3u + channel] <
                    cs_lab_frame_bounds[pixel][0][channel] ||
                pixels[pixel * 3u + channel] >
                    cs_lab_frame_bounds[pixel][1][channel]) {
                fprintf(stderr, "ColorSync Lab frame pixel=%zu channel=%zu "
                    "value=%.9g interval=[%.9g,%.9g]\n", pixel, channel,
                    pixels[pixel * 3u + channel],
                    cs_lab_frame_bounds[pixel][0][channel],
                    cs_lab_frame_bounds[pixel][1][channel]);
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

int
test_loader_0173_lab(int argc, char **argv)
{
    sixel_cms_engine_t previous;
    sixel_loader_t *loader;
    SIXELSTATUS status;
    int calls;
    int no;
    int ok;

    if (argc != 2) {
        return 1;
    }
    if (!loader_manager_entry_available("libtiff")) {
        return 77;
    }
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_COLORSYNC);
    if (sixel_cms_get_engine() != SIXEL_CMS_ENGINE_COLORSYNC) {
        sixel_cms_set_engine(previous);
        return 77;
    }
    loader = NULL;
    calls = 0;
    no = 0;
    ok = 0;
    status = sixel_loader_new(&loader, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_CONTEXT, &calls);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    sixel_loader_set_prefer_float32(loader, 1);
    status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_USE_PALETTE, &no);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_LOADER_ORDER,
        "libtiff:cms_engine=colorsync:cms_target=linear:prefer_8bit=0!");
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_loader_load_file(loader, argv[1], colorsync_lab_capture);
    ok = SIXEL_SUCCEEDED(status) && calls == 1;
end:
    sixel_loader_unref(loader);
    sixel_cms_set_engine(previous);
    if (ok) {
        fprintf(stderr, "CSLABFRAME|values-verified\n");
    }
    return ok ? 0 : 1;
}
