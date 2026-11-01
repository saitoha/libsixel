/* SPDX-License-Identifier: MIT */
/* Independently fix the wire centroid and the next lookup's residual. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/dither.h"
#include "dither-policy-private.h"
#include "src/lookup-policy.h"
#include "src/palette.h"
#include "src/dither-policy-fs.h"
#include "src/dither-policy-atkinson.h"
#include "src/dither-policy-jajuni.h"
#include "src/dither-policy-stucki.h"
#include "src/dither-policy-burkes.h"
#include "src/dither-policy-sierra1.h"
#include "src/dither-policy-sierra2.h"
#include "src/dither-policy-sierra3.h"
#include "src/dither-policy-lso2.h"
#include "tests/data/color-numeric/p3-palette.h"
#include "tests/data/color-numeric/palette-space.h"

typedef struct spatial_method {
    char const *name;
    SIXELSTATUS (*create)(sixel_allocator_t *, void **);
    double right_weight;
} spatial_method_t;

/* Only the immediate right tap contributes on a two-pixel row. LSO2's
 * 1/8 error selects coefficient row 32: right 794, denominator 3004.
 * Stucki and Two-row Sierra use canonical first taps (8/42 and 4/16).
 * These literals are the expected contracts, not imported kernel tables. */
static spatial_method_t const spatial_methods[] = {
    { "atkinson", sixel_dither_policy_atkinson_float32_new, 1.0 / 8.0 },
    { "jajuni", sixel_dither_policy_jajuni_float32_new, 7.0 / 48.0 },
    { "stucki", sixel_dither_policy_stucki_float32_new, 8.0 / 42.0 },
    { "burkes", sixel_dither_policy_burkes_float32_new, 8.0 / 32.0 },
    { "sierra1", sixel_dither_policy_sierra1_float32_new, 2.0 / 4.0 },
    { "sierra2", sixel_dither_policy_sierra2_float32_new, 4.0 / 16.0 },
    { "sierra3", sixel_dither_policy_sierra3_float32_new, 5.0 / 32.0 },
    { "lso2", sixel_dither_policy_lso2_float32_new, 794.0 / 3004.0 },
    { "fs", sixel_dither_policy_fs_float32_new, 7.0 / 16.0 },
    { "auto", NULL, 0.0 }
};

typedef struct spatial_capture {
    char bytes[2048];
    size_t size;
} spatial_capture_t;

static int
spatial_write(char *data, int size, void *context)
{
    spatial_capture_t *capture;

    capture = context;
    if (size < 0 || (size_t)size >= sizeof(capture->bytes) - capture->size) {
        return -1;
    }
    memcpy(capture->bytes + capture->size, data, (size_t)size);
    capture->size += (size_t)size;
    capture->bytes[capture->size] = '\0';
    return size;
}

static double
spatial_gamma(double value)
{
    double magnitude;

    magnitude = fabs(value);
    magnitude = magnitude <= .0031308 ? 12.92 * magnitude
        : 1.055 * pow(magnitude, 1.0 / 2.4) - .055;
    return copysign(magnitude, value);
}

static int
spatial_encoder(spatial_method_t const *method, int p3, int linear)
{
    static struct {
        int flag;
        char const *value;
    } const options[] = {
        { '=', "greedy:B1" }, { 'p', "1" }, { '.', "auto" },
        { 'Q', "kmeans:seed=1" }, { 'X', "linear" }, { 'F', "none" },
        { 'a', "off" }, { '_', "none" }, { '~', "none" },
        { 't', "rgb" }, { '5', "none" }, { '4', "full-frame" },
        { '0', "soft:threshold=1.01" }
    };
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    spatial_capture_t capture;
    float pixels[6] = { 2.0f, .01f, .001f, -.5f, .01f, .001f };
    char const *expected;
    char const *option;
    size_t n;
    int ok;

    encoder = NULL;
    output = NULL;
    frame = NULL;
    ok = 0;
    option = method->name;
    if (strcmp(option, "sierra1") == 0) {
        option = "sierra:variant=1";
    } else if (strcmp(option, "sierra2") == 0) {
        option = "sierra:variant=2";
    } else if (strcmp(option, "sierra3") == 0) {
        option = "sierra:variant=3";
    }
    memset(&capture, 0, sizeof(capture));
    if (!linear) {
        for (n = 0U; n < 6U; ++n) {
            pixels[n] = (float)spatial_gamma(pixels[n]);
        }
    }
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, spatial_write,
                &capture, NULL))) {
        goto end;
    }
    for (n = 0U; n < sizeof(options) / sizeof(options[0]); ++n) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, options[n].flag,
                options[n].value))) {
            goto end;
        }
    }
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U',
            p3 ? "display-p3" : "gamma")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'W',
                linear ? "linear" : "gamma")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'd', option)) ||
            SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
            SIXEL_FAILED(sixel_frame_init_borrowed(frame, pixels, 2, 1,
                linear ? SIXEL_PIXELFORMAT_LINEARRGBFLOAT32
                       : SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, -1))) {
        goto end;
    }
    /* The unclipped linear centroid is (.75, .01, .001). Chromaticity
     * matrices derived from sRGB/P3 primaries and D65 give P3 linear
     * (.618621856849, .034563707150, .014446467376). The sRGB transfer
     * gives (81,20,13) in P3 or (88,10,1) in sRGB. Neither expectation
     * comes from a production transform. Margins exceed .05 RGB101 units.
     * One color fixes the centroid independently of clustering/scan order;
     * the companion residual tests, not these painted indexes, prove
     * that error is propagated using the unrounded working palette. */
    expected = p3 ? "#0;2;81;20;13#" : "#0;2;88;10;1#";
    ok = SIXEL_SUCCEEDED(sixel_encoder_encode_frame(encoder, frame, output))
        && strstr(capture.bytes, expected) != NULL;
end:
    if (!ok) {
        fprintf(stderr, "wire (%s, p3=%d, linear=%d): %s; %s\n",
            method->name, p3, linear, capture.bytes,
            sixel_helper_get_additional_message());
    }
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

typedef struct spatial_lookup {
    sixel_lookup_policy_interface_t base;
    float next[3];
    int calls;
} spatial_lookup_t;

static sixel_lookup_policy_result_t
spatial_lookup_pixel(sixel_lookup_policy_interface_t const *base,
                     unsigned char const *pixel)
{
    spatial_lookup_t *lookup;

    lookup = (spatial_lookup_t *)(void *)base;
    if (++lookup->calls == 2) {
        memcpy(lookup->next, pixel, sizeof(lookup->next));
    }
    return 0;
}

static int
spatial_lookup_pixel_hint(sixel_lookup_policy_interface_t const *self,
                          unsigned char const *pixel,
                          int hint)
{
    (void)hint;
    return spatial_lookup_pixel(self, pixel);
}

static int
spatial_residual(spatial_method_t const *method, int p3, int space)
{
    static sixel_lookup_policy_vtbl_t const vtbl = {
        NULL, NULL, NULL, spatial_lookup_pixel, spatial_lookup_pixel_hint,
        sixel_lookup_policy_map_pixels_default, NULL,
    NULL
};
    static int const colors[] = {
        SIXEL_COLORSPACE_GAMMA, SIXEL_COLORSPACE_LINEAR,
        SIXEL_COLORSPACE_OKLAB, SIXEL_COLORSPACE_CIELAB, SIXEL_COLORSPACE_DIN99D
    };
    static int const formats[] = {
        SIXEL_PIXELFORMAT_RGBFLOAT32, SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
        SIXEL_PIXELFORMAT_OKLABFLOAT32, SIXEL_PIXELFORMAT_CIELABFLOAT32,
        SIXEL_PIXELFORMAT_DIN99DFLOAT32
    };
    double const native[3] = { .312, .123, -.127 };
    sixel_dither_t *dither;
    sixel_dither_policy_interface_t *policy;
    sixel_dither_policy_prepare_request_t prepare;
    sixel_dither_policy_apply_request_t request;
    sixel_dither_execution_t request_execution;
    sixel_palette_entries_request_t br;
    sixel_palette_float32_entries_request_t fr;
    sixel_palette_entries_view_t bytes;
    spatial_lookup_t lookup;
    sixel_index_t result[2];
    unsigned char original_bytes[3] = { 0, 0, 0 };
    unsigned char const *wire;
    unsigned int count;
    float original[3];
    float pixels[6];
    double source[3] = { 1.0, -.03125, .125 };
    double center;
    double expected;
    void *object;
    int n;
    int ok;
    int colorspace;
    int linear;

    dither = NULL;
    policy = NULL;
    object = NULL;
    ok = 0;
    linear = space == 1;
    colorspace = colors[space];
    memset(&prepare, 0, sizeof(prepare));
    memset(&request, 0, sizeof(request));
    memset(&lookup, 0, sizeof(lookup));
    lookup.base.vtbl = &vtbl;
    for (n = 0; n < 3; ++n) {
        if (space >= 2) {
            center = native[n];
            original[n] = (float)center;
        } else if (p3) {
            center = color_p3_red_linear[n] * .8;
            if (!linear) {
                center = spatial_gamma(center);
            }
            original[n] = (float)center;
        } else {
            /* The .351 center emits .35, but snap none keeps the original
             * center for diffusion. A sent-color substitution must fail. */
            original[n] = linear
                ? (float)pow((.351 + .055) / 1.055, 2.4) : .351f;
            center = linear ? pow((.351 + .055) / 1.055, 2.4) : .351;
        }
        pixels[n] = (float)(center + .125);
        pixels[n + 3] = (float)source[n];
    }
    if (method->create == NULL ||
            SIXEL_FAILED(sixel_dither_new(&dither, 1, NULL)) ||
            SIXEL_FAILED(method->create(dither->allocator, &object))) {
        goto end;
    }
    policy = object;
    br.entries = original_bytes;
    br.colors = 1U;
    br.depth = 3;
    fr.entries = original;
    fr.colors = 1U;
    fr.depth = 3U * sizeof(float);
    if (SIXEL_FAILED(dither->palette->vtbl->init_entries(
            dither->palette, &br)) ||
            SIXEL_FAILED(dither->palette->vtbl->init_entries_float32(
                dither->palette, &fr)) ||
            SIXEL_FAILED(sixel_palette_prepare_rgb101(dither->palette,
                colorspace, p3 ? SIXEL_PALETTE_OUTPUT_DISPLAY_P3
                               : SIXEL_PALETTE_OUTPUT_LEGACY)) ||
            SIXEL_FAILED(sixel_palette_get_rgb101(dither->palette,
                &wire, &count)) || count != 1U ||
            SIXEL_FAILED(dither->palette->vtbl->get_entries(
                dither->palette, &bytes))) {
        goto end;
    }
    for (n = 0; n < 3; ++n) {
        if (wire[n] != (space >= 2
                ? color_palette_space[space].residual_wire[p3][n]
                : (p3 ? (n == 0 ? 91 : 0) : 35))) {
            goto end;
        }
    }
    dither->use_rgb101_palette = 1;
    if (SIXEL_FAILED(sixel_dither_set_clamp_policy(dither,
            SIXEL_DITHER_CLAMP_NONE, 1.01))) {
        goto end;
    }
    sixel_dither_policy_capture_options(dither, &prepare.options);
    prepare.method_for_scan = SIXEL_SCAN_RASTER;
    prepare.pixelformat = formats[space];
    if (SIXEL_FAILED(policy->vtbl->prepare(policy, &prepare))) {
        goto end;
    }
    sixel_dither_policy_capture_execution(dither, &request_execution);
    request.execution = &request_execution;
    request.width = 2;
    request.height = 1;
    request.depth = 3;
    request.data = (unsigned char *)(void *)pixels;
    request.palette = bytes.entries;
    request.result = result;
    request.lookup_policy = &lookup.base;
    if (SIXEL_FAILED(policy->vtbl->apply(policy, &request)) ||
            lookup.calls != 2 || result[0] != 0 || result[1] != 0) {
        goto end;
    }
    for (n = 0; n < 3; ++n) {
        /* LSO2 diffuses only lightness in perceptual working spaces;
         * chromatic axes retain the source, rather than an RGB error tap. */
        expected = source[n];
        if (space < 2 || n == 0 || strcmp(method->name, "lso2") != 0) {
            expected += .125 * method->right_weight;
        }
        /* Absolute tolerance covers binary32 palette, sample and residual
         * stores. It excludes both byte-projected palette errors and the
         * missing residual. Red stays extended; the negative green source
         * must not be clipped before receiving its correction. */
        if (!isfinite(lookup.next[n]) ||
                fabs((double)lookup.next[n] - expected) > 2e-7) {
            fprintf(stderr, "next[%d]: %.9g != %.9g\n", n,
                (double)lookup.next[n], expected);
            goto end;
        }
    }
    ok = 1;
end:
    if (!ok) {
        fprintf(stderr, "residual (%s, p3=%d, linear=%d): %s\n",
            method->name, p3, linear, sixel_helper_get_additional_message());
    }
    if (policy != NULL) {
        policy->vtbl->unref(policy);
    }
    sixel_dither_unref(dither);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
test_rgb101_spatial(int argc, char **argv)
{
    size_t n;
    int p3;
    int space;

    if (argc != 5) {
        return EXIT_FAILURE;
    }
    p3 = strcmp(argv[2], "p3") == 0;
    space = strcmp(argv[3], "linear") == 0 ? 1
        : strcmp(argv[3], "oklab") == 0 ? 2
        : strcmp(argv[3], "cielab") == 0 ? 3
        : strcmp(argv[3], "din99d") == 0 ? 4 : 0;
    for (n = 0U; n < sizeof(spatial_methods) / sizeof(spatial_methods[0]);
            ++n) {
        if (strcmp(argv[4], spatial_methods[n].name) == 0) {
            return strcmp(argv[1], "wire") == 0
                ? spatial_encoder(&spatial_methods[n], p3, space == 1)
                : spatial_residual(&spatial_methods[n], p3, space);
        }
    }
    return EXIT_FAILURE;
}
