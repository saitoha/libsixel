/* SPDX-License-Identifier: MIT */
/* Inspect original PNG samples after the requested loader and CMS path. */
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
#include "src/loader-manager.h"
#include "src/loader-common.h"
#include "tests/data/color-numeric/png-samples.h"

typedef struct {
    int depth;
    int rgba;
    int managed;
    int chrm;
    int default_srgb;
    int calls;
    int oriented;
    int background;
    int policy;
    int target;
    int native_target;
    int reference_stage;
    double tolerance;
    double native_linear[24];
} png_numeric_t;

static SIXELSTATUS
png_numeric_capture(sixel_frame_t *frame, void *context)
{
    png_numeric_t *probe;
    unsigned char const *bytes;
    float const *pixels;
    double actual;
    double expected;
    double peak;
    double alpha;
    double background;
    double magnitude;
    int format;
    int channels;
    int p;
    int c;
    int hidden;
    int source_pixel;
    int expected_hidden;
    int expected_format;
    int expected_space;

    probe = (png_numeric_t *)context;
    ++probe->calls;
    format = sixel_frame_get_pixelformat(frame);
    channels = sixel_helper_compute_depth(format);
    bytes = sixel_frame_get_pixels(frame);
    pixels = (float const *)bytes;
    expected_format = probe->target == 2 ? SIXEL_PIXELFORMAT_RGB888 :
        probe->target == 1 ? SIXEL_PIXELFORMAT_RGBFLOAT32 :
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
    expected_space = probe->target ? SIXEL_COLORSPACE_GAMMA :
        SIXEL_COLORSPACE_LINEAR;
    if (probe->default_srgb && probe->managed) {
        /* Builtin requires gAMA before using isolated cHRM. Its documented
         * fallback keeps opaque bytes/words in their original sRGB transfer. */
        expected_format = probe->rgba && probe->background
            ? SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 :
            probe->depth ? SIXEL_PIXELFORMAT_RGBFLOAT32 :
            SIXEL_PIXELFORMAT_RGB888;
        expected_space = probe->rgba && probe->background
            ? SIXEL_COLORSPACE_LINEAR :
            SIXEL_COLORSPACE_GAMMA;
    }
    if (frame->width != (probe->oriented ? 2 : 4) ||
        frame->height != (probe->oriented ? 4 : 2) ||
        (probe->managed &&
         (format != expected_format || frame->colorspace != expected_space))) {
        fprintf(stderr, "PNG frame %dx%d format=%x coordinate=%d\n",
            frame->width, frame->height, format, frame->colorspace);
        return SIXEL_BAD_INPUT;
    }
    if (format != SIXEL_PIXELFORMAT_RGB888 &&
        format != SIXEL_PIXELFORMAT_RGBA8888 &&
        format != SIXEL_PIXELFORMAT_RGBFLOAT32 &&
        format != SIXEL_PIXELFORMAT_LINEARRGBFLOAT32) {
        return SIXEL_BAD_INPUT;
    }
    peak = probe->depth ? 65535.0 : 255.0;
    for (p = 0; p < 8; ++p) {
        /* EXIF 6 rotates clockwise: destination (x,y) reads (y,1-x).
         * Source alpha and hidden RGB must follow the same permutation. */
        source_pixel = probe->oriented ? (1 - p % 2) * 4 + p / 2 : p;
        hidden = 0;
        if (frame->transparent_mask != NULL) {
            if (frame->transparent_mask_size != 8u) {
                return SIXEL_BAD_INPUT;
            }
            hidden = frame->transparent_mask[p] != 0;
        } else if (format == SIXEL_PIXELFORMAT_RGBA8888) {
            hidden = bytes[p * 4 + 3] == 0;
            /* Alpha is a source sample, never a color-transfer channel. */
            if (bytes[p * 4 + 3] != color_png_source[0][source_pixel][3]) {
                return SIXEL_BAD_INPUT;
            }
        }
        expected_hidden = probe->rgba && source_pixel == 2 &&
            (!probe->background ||
             probe->policy != SIXEL_ALPHA_POLICY_COMPOSITE);
        if (hidden != expected_hidden) {
            fprintf(stderr, "PNG mask pixel=%d hidden=%d\n", p, hidden);
            return SIXEL_BAD_INPUT;
        }
        for (c = 0; c < 3; ++c) {
            if (SIXEL_PIXELFORMAT_IS_FLOAT32(format)) {
                actual = pixels[p * 3 + c];
            } else {
                actual = bytes[p * channels + c] / 255.0;
            }
            if (probe->managed && probe->chrm) {
                expected =
                    color_png_wide_chrm_linear[probe->depth][source_pixel][c];
            } else if (probe->managed && !probe->default_srgb) {
                expected = color_png_icc_linear[probe->depth][source_pixel][c];
            } else if (frame->colorspace == SIXEL_COLORSPACE_LINEAR) {
                expected = color_png_srgb_linear[probe->depth][source_pixel][c];
            } else {
                expected =
                    color_png_source[probe->depth][source_pixel][c] / peak;
            }
            if (probe->background && probe->rgba && !expected_hidden) {
                alpha = color_png_source[probe->depth][source_pixel][3] / peak;
                if (probe->background == 1) {
                    /* File bKGD is the original source green, including ICC. */
                    background = probe->managed
                        ? color_png_icc_linear[probe->depth][1][c]
                        : color_png_srgb_linear[probe->depth][1][c];
                } else {
                    /* Explicit RGB bytes use their declared working transfer,
                     * independently of the file's Display-P3 source profile. */
                    background = (64.0 * (c + 1)) / 255.0;
                    if (probe->background == 2) {
                        background = pow((background + 0.055) / 1.055, 2.4);
                    }
                }
                expected = expected * alpha + background * (1.0 - alpha);
            }
            if (probe->managed && probe->target) {
                if (probe->native_target) {
                    /* Native linear accuracy was independently checked in
                     * the first load. Now isolate the requested view from
                     * that native matrix error, with exact byte rounding. */
                    expected = probe->native_linear[p * 3 + c];
                }
                /* Derive the requested view after linear-light composition.
                 * Float gamma keeps sign and gamut; byte export bounds and
                 * rounds only this final view. No production TRC is called. */
                magnitude = fabs(expected);
                expected = copysign(magnitude <= 0.0031308
                    ? magnitude * 12.92
                    : 1.055 * pow(magnitude, 1.0 / 2.4) - 0.055, expected);
                if (probe->target == 2) {
                    expected = floor(fmax(0.0, fmin(1.0, expected)) *
                        255.0 + 0.5) / 255.0;
                }
            }
            /* The oracle is Decimal matrix/TRC math from original integer
             * samples. This bound includes float normalization and storage,
             * but cannot hide clipping or a wrong extended transfer rule. */
            if (!isfinite(actual) ||
                fabs(actual - expected) > probe->tolerance) {
                fprintf(stderr, "PNG pixel=%d channel=%d %.10g != %.10g\n",
                    p, c, actual, expected);
                return SIXEL_BAD_INPUT;
            }
            if (probe->reference_stage) {
                probe->native_linear[p * 3 + c] = actual;
            }
        }
    }
    return SIXEL_OK;
}

int
test_loader_0185_png_source_numeric(int argc, char **argv)
{
    png_numeric_t probe;
    sixel_cms_engine_t engine;
    sixel_cms_engine_t previous;
    sixel_loader_t *loader;
    SIXELSTATUS status;
    char order[224];
    char reference_order[224];
    unsigned char bgcolor[3] = {64, 128, 192};
    int previous_policy;
    int explicit_first;
    int no;
    int prefer;
    int length;
    int target;

    if ((argc != 7 && argc != 9 && argc != 10) ||
        !sixel_cms_engine_from_string(argv[2], &engine)) {
        return 1;
    }
    if (!loader_manager_entry_available(argv[1])) {
        return 77;
    }
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(engine);
    if (sixel_cms_get_engine() != engine) {
        sixel_cms_set_engine(previous);
        return 77;
    }
    memset(&probe, 0, sizeof(probe));
    probe.depth = strcmp(argv[3], "16") == 0;
    probe.rgba = strcmp(argv[4], "rgba") == 0;
    probe.managed = engine != SIXEL_CMS_ENGINE_NONE;
    probe.tolerance = engine == SIXEL_CMS_ENGINE_COLORSYNC
        ? 0.0001 : 0.0000005;
    probe.chrm = argc >= 9 && strcmp(argv[7], "chrm") == 0;
    probe.default_srgb = argc >= 9 && strcmp(argv[7], "chrm-default") == 0;
    probe.oriented = argc >= 9 && strcmp(argv[7], "orientation") == 0;
    probe.background = argc < 9 ? 0 :
        strcmp(argv[7], "file") == 0 ? 1 :
        strcmp(argv[7], "gamma") == 0 ? 2 :
        strcmp(argv[7], "linear") == 0 ? 3 : 0;
    probe.policy = argc >= 9 ? atoi(argv[8]) : SIXEL_ALPHA_POLICY_COMPOSITE;
    probe.target = argc == 10 ? atoi(argv[9]) : 0;
    probe.native_target = engine == SIXEL_CMS_ENGINE_COLORSYNC &&
        probe.target != 0;
    previous_policy = loader_transparent_policy();
    explicit_first = SIXEL_BACKGROUND_POLICY_EXPLICIT_FIRST;
    no = 0;
    prefer = atoi(argv[5]);
    loader = NULL;
    length = snprintf(order, sizeof(order),
                       "%s:cms_engine=%s:background_colorspace=%s:"
                       "cms_target=%s:prefer_8bit=%d!",
                       argv[1], argv[2],
                       probe.background == 3 ? "linear" : "gamma",
                       probe.target ? "gamma" : "linear", probe.target == 2);
    if (length < 0 || (size_t)length >= sizeof(order)) {
        sixel_cms_set_engine(previous);
        return 1;
    }
    status = sixel_loader_new(&loader, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_CONTEXT,
                                 &probe);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    sixel_loader_set_prefer_float32(loader, prefer);
    status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_USE_PALETTE, &no);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_LOADER_ORDER,
                                 order);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (probe.background >= 2) {
        status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_BGCOLOR,
                                     bgcolor);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        status = sixel_loader_setopt(loader,
            SIXEL_LOADER_OPTION_BACKGROUND_POLICY, &explicit_first);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }
    sixel_helper_set_loader_transparent_policy(probe.policy);
    if (probe.native_target) {
        /* A linear-domain CMS error bound cannot be reused after a nonlinear
         * transfer, especially at zero or a byte threshold. Validate native
         * canonical samples first, then use independent scalar transfer math
         * to check the target. Both loads must deliver the requested frame. */
        length = snprintf(reference_order, sizeof(reference_order),
            "%s:cms_engine=%s:background_colorspace=%s:"
            "cms_target=linear:prefer_8bit=0!", argv[1], argv[2],
            probe.background == 3 ? "linear" : "gamma");
        if (length < 0 || (size_t)length >= sizeof(reference_order)) {
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_LOADER_ORDER,
                                     reference_order);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        target = probe.target;
        probe.target = 0;
        probe.reference_stage = 1;
        status = sixel_loader_load_file(loader, argv[6], png_numeric_capture);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        probe.target = target;
        probe.reference_stage = 0;
        probe.tolerance = 0.0000005;
        status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_LOADER_ORDER,
                                     order);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }
    status = sixel_loader_load_file(loader, argv[6], png_numeric_capture);
end:
    sixel_helper_set_loader_transparent_policy(previous_policy);
    sixel_loader_unref(loader);
    sixel_cms_set_engine(previous);
    return SIXEL_FAILED(status) || probe.calls != (probe.native_target ? 2 : 1);
}
