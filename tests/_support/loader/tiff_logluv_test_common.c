/* SPDX-License-Identifier: MIT */
/* Public loader observations use independent storage and matrix expectations.
 */
#include "config.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "src/cms.h"
#include "src/frame-private.h"
#include "src/loader.h"
#include "src/loader-manager.h"
#include "tests/data/color-numeric/tiff-logluv.h"
#include "tiff_logluv_test_common.h"

#if HAVE_LIBTIFF
typedef struct tlog_context {
    int variant;
    int calls;
} tlog_context_t;

static SIXELSTATUS
tlog_capture(sixel_frame_t *frame, void *context)
{
    static int const formats[] = {
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, SIXEL_PIXELFORMAT_RGBFLOAT32,
        SIXEL_PIXELFORMAT_CIELABFLOAT32, SIXEL_PIXELFORMAT_OKLABFLOAT32,
        SIXEL_PIXELFORMAT_DIN99DFLOAT32
    };
    static int const spaces[] = {
        SIXEL_COLORSPACE_LINEAR, SIXEL_COLORSPACE_GAMMA,
        SIXEL_COLORSPACE_CIELAB, SIXEL_COLORSPACE_OKLAB,
        SIXEL_COLORSPACE_DIN99D
    };
    tlog_context_t *ctx;
    float const *pixels;
    unsigned char const *bytes;
    double value;
    double lo;
    double hi;
    size_t i;
    size_t c;
    size_t source;
    size_t x;
    size_t y;
    int bits;
    int target;
    int byte_view;
    int negative;
    int above;
    int ok;

    ctx = context;
    ++ctx->calls;
    bits = ctx->variant == 1;
    target = ctx->variant == 10 ? 1 :
             ctx->variant >= 12 && ctx->variant <= 14
             ? ctx->variant - 10 : 0;
    byte_view = ctx->variant == 11 || ctx->variant == 15;
    if (frame->width != 4 || frame->height != 2 || frame->ncolors != -1 ||
        frame->transparent != -1 || frame->transparent_mask != NULL ||
        frame->transparent_mask_size != 0u ||
        frame->alpha_zero_is_transparent != 0 ||
        frame->pixelformat != (byte_view ? SIXEL_PIXELFORMAT_RGB888
                                         : formats[target]) ||
        frame->colorspace != (byte_view ? SIXEL_COLORSPACE_GAMMA
                                        : spaces[target])) {
        fprintf(stderr, "TLOGLUV|metadata=0|format=%x|space=%d\n",
                frame->pixelformat, frame->colorspace);
        return SIXEL_BAD_INPUT;
    }
    pixels = (float const *)sixel_frame_get_pixels(frame);
    bytes = sixel_frame_get_pixels(frame);
    ok = 1;
    negative = 0;
    above = 0;
    for (i = 0u; i < 8u; ++i) {
        x = i % 4u;
        y = i / 4u;
        if (ctx->variant == 3 || ctx->variant == 4) { x = 3u - x; }
        if (ctx->variant == 4 || ctx->variant == 5) { y = 1u - y; }
        source = y * 4u + x;
        for (c = 0u; c < 3u; ++c) {
            if (byte_view) {
                ok = ok && bytes[i * 3u + c] ==
                    (ctx->variant == 15 ? tlog_legacy[bits][source][c]
                                        : tlog_bytes[bits][source][c]);
                continue;
            }
            value = pixels[i * 3u + c];
            lo = tlog_bounds[bits][target][source][0][c];
            hi = tlog_bounds[bits][target][source][1][c];
            if (!isfinite(value) || value < lo || value > hi) {
                fprintf(stderr, "TLOGLUV|pixel=%zu|channel=%zu|value=%.17g|"
                    "lo=%.17g|hi=%.17g\n", i, c, value, lo, hi);
                ok = 0;
            }
            negative |= hi < 0.0 && value < 0.0;
            above |= lo > 1.0 && value > 1.0;
        }
    }
    if (!byte_view && target == 0) { ok = ok && negative && above; }
    return ok ? SIXEL_OK : SIXEL_BAD_INPUT;
}
#endif

int
tiff_logluv_values_test(int argc, char **argv, int variant)
{
#if HAVE_LIBTIFF
    sixel_loader_t *loader;
    sixel_cms_engine_t previous;
    sixel_cms_engine_t requested;
    tlog_context_t context;
    SIXELSTATUS status;
    float control[3] = {0.2f, 0.4f, 0.6f};
    char order[160];
    char const *target;
    int byte_view;
    int control_ok;
    int result;

    if (argc != 2) { return 2; }
    previous = sixel_cms_get_engine();
    requested = variant == 6 ? SIXEL_CMS_ENGINE_NONE :
                variant == 16 ? SIXEL_CMS_ENGINE_LCMS2 :
                variant == 17 ? SIXEL_CMS_ENGINE_COLORSYNC :
                SIXEL_CMS_ENGINE_BUILTIN;
    sixel_cms_set_engine(requested);
    if (sixel_cms_get_engine() != requested) {
        sixel_cms_set_engine(previous);
        return 77;
    }
    /* A real apply before the marker proves the trace is live. No apply may
     * follow it while native XYZ is decoded or the RGB target is selected. */
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_BUILTIN);
    control_ok = sixel_cms_convert_to_linearrgb_with_profile_bytes(
        (unsigned char *)control, 1, 1, SIXEL_PIXELFORMAT_RGBFLOAT32,
        tlog_icc, sizeof(tlog_icc));
    fprintf(stderr, "TLOGLUV|native-begin\n");
    target = variant == 10 ? "gamma" : variant == 12 ? "cielab" :
             variant == 13 ? "oklab" : variant == 14 ? "din99d" : "linear";
    byte_view = variant == 11;
    snprintf(order, sizeof(order),
        "libtiff:cms_engine=%s:cms_target=%s:prefer_8bit=%d!",
        sixel_cms_engine_to_string(requested), target, byte_view);
    loader = NULL;
    context.variant = variant;
    context.calls = 0;
    status = sixel_loader_new(&loader, NULL);
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_CONTEXT,
                                     &context);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_LOADER_ORDER,
                                     order);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_loader_load_file(loader, argv[1], tlog_capture);
    }
    result = control_ok && status == SIXEL_OK && context.calls == 1;
    fprintf(stderr, "TLOGLUV|control=%d|status=%x|callbacks=%d\n",
        control_ok, status, context.calls);
    if (loader != NULL) { sixel_loader_unref(loader); }
    sixel_cms_set_engine(previous);
    return result ? 0 : 1;
#else
    (void)argc;
    (void)argv;
    (void)variant;
    return 77;
#endif
}
