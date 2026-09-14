/* SPDX-License-Identifier: MIT */
/* Inspect canonical samples and source-alpha composition independently. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/cms.h"
#include "src/frame-private.h"
#include "src/loader.h"
#include "src/loader-common.h"
#include "tests/data/color-numeric/png-samples.h"

typedef struct {
    int raw;
    int depth;
    int rgba;
    int prefer;
    int background;
    int policy;
    int reference;
    int calls;
    double linear[24];
} cg_png_probe_t;

static SIXELSTATUS
cg_png_capture(sixel_frame_t *frame, void *context)
{
    cg_png_probe_t *probe;
    double expected;
    double actual;
    double alpha;
    double background;
    double tolerance;
    int byte_path;
    int hidden;
    int preserve;
    int p;
    int c;

    probe = (cg_png_probe_t *)context;
    ++probe->calls;
    byte_path = probe->raw && !probe->depth && !probe->rgba &&
        !probe->prefer && !probe->background;
    preserve = probe->rgba && (!probe->background || probe->reference ||
        probe->policy != SIXEL_ALPHA_POLICY_COMPOSITE);
    if (frame->width != 4 || frame->height != 2 ||
        frame->pixelformat != (byte_path ? SIXEL_PIXELFORMAT_RGB888 :
            SIXEL_PIXELFORMAT_LINEARRGBFLOAT32) ||
        frame->colorspace != (byte_path ? SIXEL_COLORSPACE_GAMMA :
            SIXEL_COLORSPACE_LINEAR) || !frame->owns_pixels ||
        frame->transparent != -1 || frame->ncolors != -1 ||
        frame->alpha_zero_is_transparent != preserve ||
        (frame->transparent_mask != NULL) != preserve ||
        frame->transparent_mask_size != (preserve ? 8u : 0u)) {
        fprintf(stderr, "CG PNG frame or coverage contract differs\n");
        return SIXEL_BAD_INPUT;
    }
    for (p = 0; p < 8; ++p) {
        hidden = preserve && p == 2;
        if (preserve && frame->transparent_mask[p] != hidden) {
            fprintf(stderr, "CG PNG mask pixel=%d\n", p);
            return SIXEL_BAD_INPUT;
        }
        for (c = 0; c < 3; ++c) {
            expected = probe->raw ?
                color_png_srgb_linear[probe->depth][p][c] :
                color_png_icc_linear[probe->depth][p][c];
            tolerance = 0.0001;
            if (byte_path) {
                expected = color_png_source[0][p][c];
                actual = frame->pixels.u8ptr[p * 3 + c];
                tolerance = 0.0;
            } else {
                actual = frame->pixels.f32ptr[p * 3 + c];
            }
            if (probe->background && !probe->reference) {
                /* The first load validates native linear accuracy against
                 * the original profile. This second check isolates original
                 * alpha and background arithmetic from that native error. */
                expected = probe->linear[p * 3 + c];
                tolerance = 0.0000005;
                if (probe->rgba && !hidden) {
                    alpha = color_png_source[probe->depth][p][3] /
                        (probe->depth ? 65535.0 : 255.0);
                    background = 64.0 * (c + 1) / 255.0;
                    if (probe->background == 1) {
                        background = pow((background + 0.055) / 1.055, 2.4);
                    }
                    expected = expected * alpha +
                        background * (1.0 - alpha);
                }
            }
            if (!isfinite(actual) || fabs(actual - expected) > tolerance) {
                fprintf(stderr, "CG PNG pixel=%d channel=%d %.12g != %.12g\n",
                    p, c, actual, expected);
                return SIXEL_BAD_INPUT;
            }
            if (probe->reference) {
                probe->linear[p * 3 + c] = actual;
            }
        }
    }
    return SIXEL_OK;
}

int
test_loader_0187_coregraphics_png(int argc, char **argv)
{
    cg_png_probe_t probe;
    sixel_loader_t *loader;
    SIXELSTATUS status;
    sixel_cms_engine_t engine;
    sixel_cms_engine_t previous_engine;
    unsigned char bgcolor[3] = {64, 128, 192};
    char order[160];
    int no;
    int previous_policy;

#if !HAVE_COREGRAPHICS
    return 77;
#endif
    --argc;
    ++argv;
    if (argc != 8 || !sixel_cms_engine_from_string(argv[6], &engine)) {
        return 1;
    }
    memset(&probe, 0, sizeof(probe));
    probe.raw = strcmp(argv[0], "raw") == 0;
    probe.depth = atoi(argv[1]) == 16;
    probe.rgba = strcmp(argv[2], "rgba") == 0;
    probe.prefer = atoi(argv[3]);
    probe.background = atoi(argv[4]);
    probe.policy = atoi(argv[5]);
    if (snprintf(order, sizeof(order),
        "coregraphics:background_colorspace=%s!",
        probe.background == 2 ? "linear" : "gamma") >=
        (int)sizeof(order)) {
        return 1;
    }
    no = 0;
    loader = NULL;
    previous_policy = loader_transparent_policy();
    previous_engine = sixel_cms_get_engine();
    sixel_cms_set_engine(engine);
    sixel_helper_set_loader_transparent_policy(probe.policy);
    status = sixel_loader_new(&loader, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_CONTEXT, &probe);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_USE_PALETTE, &no);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_LOADER_ORDER,
                                 order);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    sixel_loader_set_prefer_float32(loader, probe.prefer);
    if (probe.background) {
        probe.reference = 1;
        /* Force a canonical reference even for an otherwise byte-only input.
         * The tested preference is restored before the background load. */
        sixel_loader_set_prefer_float32(loader, 1);
        status = sixel_loader_load_file(loader, argv[7], cg_png_capture);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        probe.reference = 0;
        sixel_loader_set_prefer_float32(loader, probe.prefer);
        status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_BGCOLOR,
                                     bgcolor);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }
    status = sixel_loader_load_file(loader, argv[7], cg_png_capture);
end:
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "CG PNG status=%x: %s\n", status,
            sixel_helper_get_additional_message());
    }
    sixel_loader_unref(loader);
    sixel_helper_set_loader_transparent_policy(previous_policy);
    sixel_cms_set_engine(previous_engine);
    return SIXEL_FAILED(status) ||
        probe.calls != (probe.background ? 2 : 1);
}
