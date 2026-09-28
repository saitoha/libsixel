/* SPDX-License-Identifier: MIT */
/* Independent PNG metadata samples, frame representation and coverage. */
#ifndef LOADER_BUILTIN_PNG_METADATA_TEST_COMMON_H
#define LOADER_BUILTIN_PNG_METADATA_TEST_COMMON_H

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "src/cms.h"
#include "src/loader-common.h"
#include "tests/data/color-numeric/png-metadata.h"
#include "loader_builtin_memory_test_common.h"

static int
png_metadata_check(int fixture, int engine, int background)
{
    color_png_metadata_t const *source;
    edge_loader_options_t options;
    edge_frame_probe_t probe;
    SIXELSTATUS status;
    float const *expected;
    float value;
    double peak;
    double tolerance;
    int format;
    int colorspace;
    int has_alpha;
    int apply_count;
    int direct;
    int p;
    int c;
    int result;
    int expected_index;
    unsigned char const *byte_rgb;
    char const *evaluator;

    if (fixture < 0 || fixture >= 40 || background < 0 || background > 2) {
        return 1;
    }
#if !HAVE_LCMS2
    if (engine == SIXEL_CMS_ENGINE_LCMS2) {
        return 77;
    }
#endif
#if !(defined(__APPLE__) && HAVE_COREGRAPHICS)
    if (engine == SIXEL_CMS_ENGINE_COLORSYNC) {
        return 77;
    }
#endif
    source = &color_png_metadata[fixture];
    edge_loader_options_init(&options);
    options.require_static = fixture != 39;
    options.use_palette = 1;
    options.cms_engine = engine;
    options.set_prefer_float32 = 1;
    options.prefer_float32 = 1;
    if (background != 0) {
        options.set_bgcolor = 1;
        options.bgcolor[0] = 200;
        options.bgcolor[1] = 80;
        options.bgcolor[2] = 30;
    }
    sixel_helper_set_loader_background_colorspace(background == 2
        ? SIXEL_COLORSPACE_LINEAR : SIXEL_COLORSPACE_GAMMA);
    result = edge_load_buffer_options("PNG metadata", source->bytes,
        source->length, &options, &probe, &status);
    sixel_helper_set_loader_background_colorspace(-1);
    if (result != 0 || status != SIXEL_OK || probe.callback_count != 1 ||
        probe.width[0] != 8 || probe.height[0] != 1) {
        fprintf(stderr, "PNG metadata: load=%d status=%x calls=%d\n",
            result, status, probe.callback_count);
        return 1;
    }
    has_alpha = source->type != 2;
    expected = source->linear;
    format = SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
    colorspace = SIXEL_COLORSPACE_LINEAR;
    if (background != 0) {
        expected = background == 2 ? source->explicit_linear
                                  : source->explicit_gamma;
    }
    if (engine == SIXEL_CMS_ENGINE_NONE || !source->transfer) {
        expected = has_alpha && background != 0 ? source->srgb
                                               : source->source;
        if (!has_alpha || background == 0) {
            /* A zero mask alone does not change source RGB coordinates.
             * UINT16 retains its original precision in gamma float. */
            format = source->depth == 8 ? SIXEL_PIXELFORMAT_RGB888
                                       : SIXEL_PIXELFORMAT_RGBFLOAT32;
            colorspace = SIXEL_COLORSPACE_GAMMA;
        }
    }
    if (fixture == 38) {
        format = SIXEL_PIXELFORMAT_PAL8;
        colorspace = SIXEL_COLORSPACE_GAMMA;
    }
    if (probe.pixelformat[0] != format ||
        probe.colorspace[0] != colorspace) {
        fprintf(stderr, "PNG metadata: format=%x/%x space=%d/%d\n",
            probe.pixelformat[0], format, probe.colorspace[0], colorspace);
        return 1;
    }
    /* A retained palette remaps only the two alpha-zero indices. APNG and
     * canonical frames keep a separate exact coverage mask instead. */
    if (fixture == 38) {
        if (probe.ncolors[0] != 8 || probe.transparent[0] != 0 ||
            probe.alpha_zero_is_transparent[0] != 0 ||
            probe.mask_size[0] != 0 || probe.rgb_size[0] != 8 ||
            probe.palette_size[0] != 24) {
            return 1;
        }
        for (p = 0; p < 8; ++p) {
            expected_index = source->rgba[p * 4 + 3] == 0 ? 0 : p;
            if (probe.rgb[0][p] != expected_index) {
                return 1;
            }
        }
    } else {
        if (probe.transparent[0] != -1 ||
            probe.alpha_zero_is_transparent[0] != has_alpha ||
            probe.mask_size[0] != (has_alpha ? 8u : 0u)) {
            return 1;
        }
        for (p = 0; has_alpha && p < 8; ++p) {
            if (probe.mask[0][p] != (source->rgba[p * 4 + 3] == 0)) {
                return 1;
            }
        }
    }
    if (fixture == 38) {
        byte_rgb = probe.palette[0];
        for (c = 0; c < 24; ++c) {
            /* Native RGB8 packing has its own integer quantization boundary.
             * The two candidates come from continuous source-derived color,
             * not a tolerance fitted to these observed native output bytes. */
            if (engine == SIXEL_CMS_ENGINE_COLORSYNC) {
                if (byte_rgb[c] < source->native_byte_low[c] ||
                    byte_rgb[c] > source->native_byte_high[c]) {
                    return 1;
                }
            } else if (byte_rgb[c] != source->byte_rgb[c]) {
                return 1;
            }
        }
    } else if (format == SIXEL_PIXELFORMAT_RGB888) {
        if (probe.rgb_size[0] != 24) {
            return 1;
        }
        for (p = 0; p < 8; ++p) {
            for (c = 0; c < 3; ++c) {
                if (probe.rgb[0][p * 3 + c] != source->rgba[p * 4 + c]) {
                    return 1;
                }
            }
        }
    } else {
        if (probe.rgb_size[0] != 24 * sizeof(float)) {
            return 1;
        }
        direct = engine == SIXEL_CMS_ENGINE_NONE || !source->transfer ||
            !source->usable_profile;
        /* Preserve the source-derived error budgets; arbitrary multiples
         * of machine epsilon do not justify widening these bounds. */
        tolerance = engine == SIXEL_CMS_ENGINE_COLORSYNC
            ? 0.0003238444581717203 : 2.0e-7;
        for (p = 0; p < 8; ++p) {
            peak = 0.0;
            for (c = 0; c < 3; ++c) {
                peak = fmax(peak, fabs(expected[p * 3 + c]));
            }
            for (c = 0; c < 3; ++c) {
                memcpy(&value, probe.rgb[0] +
                    (p * 3 + c) * sizeof(float), sizeof(value));
                /* Direct evaluation has one final F32 rounding. A relative
                 * native budget must not hide an extra TRC F32 store here. */
                if (!isfinite(value) ||
                    (direct && value != expected[p * 3 + c]) ||
                    (!direct && fabs(value - expected[p * 3 + c]) >
                        tolerance * peak)) {
                    fprintf(stderr, "PNG metadata %d p=%d c=%d: %a/%a\n",
                        fixture, p, c, (double)value,
                        (double)expected[p * 3 + c]);
                    return 1;
                }
            }
        }
    }
    /* TAP checks actual event=apply records against this selected path.
     * Source-profile-adapter and parser diagnostics never count as applies. */
    apply_count = 0;
    if (engine != SIXEL_CMS_ENGINE_NONE && source->transfer &&
        source->usable_profile) {
        apply_count = source->background && has_alpha ? 2 : 1;
    }
    evaluator = apply_count ? sixel_cms_engine_to_string(engine) : "direct";
    printf("PNGMETA|evaluator=%s|applies=%d\n", evaluator, apply_count);
    return 0;
}
#endif
