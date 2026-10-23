/* SPDX-License-Identifier: MIT */
/* Observe candidates before lookup: palette quality cannot prove this seam. */
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/dither.h"
#include "src/lookup-policy.h"
#include "src/encoder.h"
#include "src/palette.h"
#include "src/dither-policy-a-dither.h"
#include "src/dither-policy-x-dither.h"
#include "src/dither-policy-bluenoise.h"
#include "positional_reference.h"

static double
positional_noise(int method, int x, int y, int channel, int table, int row)
{
    uint64_t phase;

    /* Wide unsigned reference arithmetic implements the published residues;
     * it neither invokes nor copies the production-width noise helper. */
    if (method == 0) {
        phase = ((uint64_t)x + 67 * (uint64_t)channel +
                 236 * (uint64_t)y) * 119;
        return (double)(phase % 256) / 128.0 - 1.0;
    }
    if (method == 1) {
        phase = (((uint64_t)x + 29 * (uint64_t)channel) ^
                 (149 * (uint64_t)y)) * 1234;
        return (double)(phase % 512) / 256.0 - 1.0;
    }
    return positional_bn_sum[table][row][x * 3 + channel] / 255.0;
}

typedef struct positional_observer {
    sixel_lookup_policy_interface_t base;
    float const *source;
    unsigned char const *bytes;
    int method;
    int mode;
    int origin;
    int serpentine;
    int calls;
    int failed;
    double strength;
} positional_observer_t;

static sixel_lookup_policy_result_t
positional_observe(sixel_lookup_policy_interface_t const *base,
                   unsigned char const *pixel)
{
    positional_observer_t *observer;
    float const *values;
    double expected;
    double noise;
    double actual;
    double tolerance;
    int x;
    int y;
    int d;
    int pos;
    int byte_value;

    observer = (positional_observer_t *)(void *)base;
    values = (float const *)(void const *)pixel;
    y = observer->calls / 3;
    x = observer->calls % 3;
    if (observer->serpentine && ((observer->origin + y) & 1)) {
        x = 2 - x;
    }
    pos = y * 9 + x * 3;
    for (d = 0; d < 3; ++d) {
        noise = positional_noise(observer->method, x, observer->origin + y,
            d, observer->mode == 4 ? 2 : observer->origin ? 1 : 0, y);
        if (observer->mode == 6 && x == 1) {
            noise = 0.0;
        }
        if ((observer->mode == 2 || observer->mode == 7 ||
                observer->mode == 9)) {
            byte_value = observer->bytes[pos + d] +
                (int)(noise * observer->strength * 32.0);
            expected = byte_value < 0 ? 0
                : byte_value > 255 ? 255 : byte_value;
            actual = pixel[d];
            tolerance = 0.0;
        } else {
            expected = observer->source[pos + d] +
                noise * observer->strength * (32.0 / 255.0);
            actual = values[d];
            /* Stored source is exact in the reference. A few rounded float
             * operations need an absolute noise budget plus source-scale
             * rounding; neither tolerance permits clipping or byte stores. */
            tolerance = 2e-6 + 2e-7 * fabs(expected);
        }
        if (!isfinite(actual) || fabs(actual - expected) > tolerance) {
            fprintf(stderr, "query %d,%d,%d: %.12g != %.12g\n",
                    x, observer->origin + y, d, actual, expected);
            observer->failed = 1;
        }
    }
    ++observer->calls;
    return 1;
}

static int
positional_observe_hint(sixel_lookup_policy_interface_t const *self,
                        unsigned char const *pixel,
                        int hint)
{
    (void)hint;
    return positional_observe(self, pixel);
}

static sixel_lookup_policy_vtbl_t const positional_observer_vtbl = {
    NULL, NULL, NULL, positional_observe, positional_observe_hint,
    sixel_lookup_policy_map_pixels_default, NULL
};

static void
positional_config(sixel_dither_t *dither, float strength, int large)
{
    dither->a_dither_strength_override = 1;
    dither->a_dither_strength = strength;
    dither->x_dither_strength_override = 1;
    dither->x_dither_strength = strength;
    dither->bluenoise_strength_override = 1;
    dither->bluenoise_strength = strength;
    dither->bluenoise_phase_override = 1;
    dither->bluenoise_phase_x = large ? INT_MAX : 0;
    dither->bluenoise_phase_y = large ? -INT_MAX : 0;
    dither->bluenoise_channel_override = 1;
    dither->bluenoise_channel_rgb = 1;
}

int
positional_query_test(int method, int mode, int space)
{
    static SIXELSTATUS (*const create[2][3])(
        sixel_allocator_t *, void **) = {
        { sixel_dither_policy_a_dither_float32_new,
          sixel_dither_policy_x_dither_float32_new,
          sixel_dither_policy_bluenoise_float32_new },
        { sixel_dither_policy_a_dither_8bit_new,
          sixel_dither_policy_x_dither_8bit_new,
          sixel_dither_policy_bluenoise_8bit_new }
    };
    static int const formats[] = {
        SIXEL_PIXELFORMAT_RGBFLOAT32, SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
        SIXEL_PIXELFORMAT_OKLABFLOAT32, SIXEL_PIXELFORMAT_CIELABFLOAT32,
        SIXEL_PIXELFORMAT_DIN99DFLOAT32
    };
    /* Independent contract values for 256, 64, reserved+64, 16, 2, 1,
     * then 256 again. Reuse must not retain the preceding palette amplitude. */
    static int const counts[] = {256, 64, 65, 16, 2, 1, 256};
    static double const amplitudes[3][7] = {
        {.15, .25, .25, .45, 1.181370849898476, 0, .15},
        {.10, .15, .15, .25, .615685424949238, 0, .10},
        {.055, .135, .135, .295, .880096679918781, 0, .055}
    };
    float source[162];
    float original[162];
    unsigned char bytes[162];
    unsigned char palette[768] = { 0, 0, 0, 255, 255, 255 };
    unsigned char *gradient;
    sixel_index_t result[54];
    sixel_dither_t *dither;
    sixel_dither_policy_interface_t *policy;
    sixel_dither_policy_prepare_request_t prepare;
    sixel_dither_policy_apply_request_t request;
    positional_observer_t observer;
    void *object;
    int pass;
    int n;
    int ok;

    dither = NULL;
    policy = NULL;
    object = NULL;
    ok = 0;
    memset(&prepare, 0, sizeof(prepare));
    memset(&request, 0, sizeof(request));
    memset(&observer, 0, sizeof(observer));
    for (n = 0; n < 162; ++n) {
        source[n] = n % 3 == 0 ? -.25f : n % 3 == 1 ? 1.250123f : .500321f;
        if (space >= 2 && n % 3 != 0) {
            source[n] = n % 3 == 1 ? 300.125f : -300.25f;
        }
        bytes[n] = n % 3 == 0 ? 0 : n % 3 == 1 ? 255 : 128;
    }
    memcpy(original, source, sizeof(source));
    if (SIXEL_FAILED(sixel_dither_new(&dither, mode >= 8 ? 256 : 2, NULL)) ||
            SIXEL_FAILED(create[mode == 2 || mode == 7 || mode == 9][method](
                dither->allocator, &object))) {
        goto end;
    }
    policy = object;
    sixel_dither_set_palette(dither, palette);
    observer.base.vtbl = &positional_observer_vtbl;
    observer.source = original;
    observer.bytes = bytes;
    observer.method = method;
    observer.mode = mode;
    observer.origin = mode == 4 ? INT_MAX - 17
        : (mode == 3 || mode == 7) ? 6 : 0;
    request.dither = prepare.dither = dither;
    request.depth = prepare.depth = 3;
    request.width = 3;
    request.height = 18;
    request.band_origin = observer.origin;
    request.output_start = observer.origin + (mode == 3 || mode == 7 ? 2 : 0);
    request.data = (mode == 2 || mode == 7 || mode == 9) ? bytes
        : (unsigned char *)(void *)source;
    request.palette = palette;
    request.result = result;
    request.lookup_policy = &observer.base;
    if (mode == 6) {
        /* The owned full-image map has exact attenuation endpoints. */
        gradient = sixel_allocator_malloc(dither->allocator, 54);
        if (gradient == NULL) {
            goto end;
        }
        for (n = 0; n < 54; ++n) {
            gradient[n] = n % 3 == 1 ? 255 : 0;
        }
        dither->bluenoise_gradient_map = gradient;
        dither->bluenoise_gradient_map_size = 54;
        dither->bluenoise_gradient_width = 3;
        dither->bluenoise_gradient_height = 18;
        dither->bluenoise_gradient_factor_override = 1;
        dither->bluenoise_gradient_factor = 1.0f;
    }
    for (pass = 0; pass < (mode >= 8 ? 7 : mode == 5 ? 2 : 1); ++pass) {
        observer.calls = observer.failed = 0;
        observer.strength = mode == 0 || pass ? 0.0 : .5;
        observer.serpentine = mode == 3 || mode == 4 || mode == 7 || pass;
        positional_config(dither, (float)observer.strength, mode == 4);
        if (mode >= 8) {
            dither->ncolors = counts[pass];
            dither->keycolor_reserved = pass == 2;
            dither->keycolor = pass == 2 ? 64 : -1;
            observer.strength = mode <= 9 ? amplitudes[method][pass]
                : mode == 10 ? .5 : pass == 1 ? 0.0 : .25;
            positional_config(dither, (float)observer.strength, 0);
            if (mode != 11) {
                dither->a_dither_strength_override = 0;
                dither->x_dither_strength_override = 0;
                dither->bluenoise_strength_override = 0;
            }
        }
        /* Reprepare must update scan/config, independently of the residual
         * clamp setting. Position-only kernels have no error feedback. */
        if (SIXEL_FAILED(sixel_dither_set_clamp_policy(dither,
                pass ? SIXEL_DITHER_CLAMP_NONE : SIXEL_DITHER_CLAMP_HARD,
                1.01))) {
            goto end;
        }
        prepare.pixelformat = (mode == 2 || mode == 7 || mode == 9)
            ? SIXEL_PIXELFORMAT_RGB888 : formats[space];
        prepare.method_for_scan = observer.serpentine
            ? SIXEL_SCAN_SERPENTINE : SIXEL_SCAN_RASTER;
        for (n = 0; n < 54; ++n) {
            result[n] = 123;
        }
        if (SIXEL_FAILED(policy->vtbl->prepare(policy, &prepare)) ||
                SIXEL_FAILED(policy->vtbl->apply(policy, &request)) ||
                observer.failed || observer.calls != 54 ||
                memcmp(source, original, sizeof(source)) != 0) {
            goto end;
        }
        for (n = 0; n < 54; ++n) {
            if (result[n] != ((mode == 3 || mode == 7) && n < 6 ? 123 : 1)) {
                goto end;
            }
        }
    }
    ok = 1;
end:
    if (policy != NULL) {
        policy->vtbl->unref(policy);
    }
    sixel_dither_unref(dither);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

typedef struct positional_capture {
    char data[8192];
    size_t size;
} positional_capture_t;

static int
positional_write(char *data, int size, void *context)
{
    positional_capture_t *capture;

    capture = context;
    if (size < 0 || (size_t)size >= sizeof(capture->data) - capture->size) {
        return -1;
    }
    memcpy(capture->data + capture->size, data, (size_t)size);
    capture->size += (size_t)size;
    capture->data[capture->size] = '\0';
    return size;
}

int
positional_encoder_test(int method, int mode, int space)
{
    static char const *const methods[] = {
        "a_dither:strength=0.5", "x_dither:strength=0.5",
        "bluenoise:strength=0.5:channel=rgb"
    };
    static char const *const spaces[] = {
        "gamma", "linear", "oklab", "cielab", "din99d"
    };
    static struct { int flag; char const *value; } const options[] = {
        { '=', "1" }, { 'p', "1" }, { '.', "auto" }, { 'U', "gamma" },
        { 'Q', "kmeans:seed=1" }, { 'X', "linear" }, { 'F', "none" },
        { 'a', "off" }, { '_', "none" }, { '~', "none" }, { 't', "rgb" },
        { '5', "none" }, { '4', "full-frame" }, { '0', "hard" }
    };
    float pixels[162] = { 2.0f, .01f, .001f, -.5f, .01f, .001f };
    unsigned char bytes[6] = { 128, 128, 128, 128, 128, 128 };
    sixel_encoder_t *encoder;
    sixel_frame_t *frame;
    sixel_output_t *output;
    sixel_dither_t *dither;
    sixel_palette_float32_entries_view_t view;
    positional_capture_t capture;
    unsigned char const *wire;
    unsigned char *decoded;
    unsigned char *palette;
    unsigned int colors;
    double distance[2];
    double candidate;
    double delta;
    int width;
    int height;
    int ncolors;
    int pass;
    int n;
    int c;
    int p;
    int selected;
    int ok;

#if defined(__PCC__) || defined(__TINYC__)
    /* These compilers explicitly disable parallel PaletteApply in dither.c. */
    if (mode == 3) {
        fputs("positional: compiler disables parallel apply\n", stderr);
        return EXIT_SUCCESS;
    }
#endif
    encoder = NULL;
    frame = NULL;
    output = NULL;
    decoded = palette = NULL;
    memset(&capture, 0, sizeof(capture));
    ok = 0;
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, positional_write,
                &capture, NULL))) {
        goto end;
    }
    for (n = 0; n < (int)(sizeof(options) / sizeof(options[0])); ++n) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, options[n].flag,
                options[n].value))) {
            goto end;
        }
    }
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'd', methods[method])) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'W', spaces[space])) ||
            (mode == 1 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                'U', "display-p3")))) {
        goto end;
    }
    if (mode >= 2 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
            'X', "gamma"))) {
        goto end;
    }
    if (mode == 3) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'p', "2"))) {
            goto end;
        }
        encoder->dither_parallel_band_width_override = 1;
        encoder->dither_parallel_band_width = 6;
        encoder->dither_parallel_band_overwrap_override = 1;
        encoder->dither_parallel_band_overwrap = 2;
        for (n = 0; n < 162; ++n) {
            pixels[n] = n < 72 ? 0.0f : n < 144 ? 1.0f
                : (float)((n * 7) % 13 + 494) / 1000.0f;
        }
    }
    for (pass = 0; pass < (mode == 3 ? 2 : 1); ++pass) {
        memset(&capture, 0, sizeof(capture));
        /* Mode 3 caps the dither stage through the threading policy. */
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, '=',
                mode == 3 ? (pass ? "4:dither=2" : "1:dither=2")
                          : (pass ? "4" : "1"))) ||
                SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
                SIXEL_FAILED(sixel_frame_init_borrowed(frame,
                    mode == 2 ? (void *)bytes : (void *)pixels,
                    mode == 3 ? 3 : 2, mode == 3 ? 18 : 1,
                    mode == 2 ? SIXEL_PIXELFORMAT_RGB888 : mode == 3
                        ? SIXEL_PIXELFORMAT_RGBFLOAT32
                        : SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, NULL, -1)) ||
                SIXEL_FAILED(sixel_encoder_encode_frame(encoder,
                    frame, output))) {
            goto end;
        }
        dither = (sixel_dither_t *)encoder->diagnostic_dither;
        if (dither == NULL || dither->use_rgb101_palette != (mode != 2)) {
            goto end;
        }
        if (mode < 2) {
            if (SIXEL_FAILED(dither->palette->vtbl->get_entries_float32(
                    dither->palette, &view)) || view.entry_count != 1) {
                goto end;
            }
            for (c = 0; c < 3; ++c) {
                /* Independent Decimal equations for the linear mean
                 * (.75, .01, .001); 3e-5 allows transform/store rounding. */
                if (!isfinite(view.entries[c]) ||
                        fabs(view.entries[c] -
                            positional_centroid[space][c]) > 3e-5) {
                    goto end;
                }
            }
            /* Both wire triples are >.03 RGB101 unit from a rounding tie. */
            if (strstr(capture.data, mode == 1
                    ? POSITIONAL_P3_WIRE : "#0;2;88;10;1") == NULL) {
                goto end;
            }
        } else if (mode == 2) {
            if (strstr(capture.data, "#0;2;50;50;50") == NULL) {
                goto end;
            }
        } else {
            /* Prove that this was an actual parallel application, with
             * three six-row bands and nonzero overlap, not serial fallback. */
            if (pass && (dither->pipeline_last_dither_threads != 2 ||
                    dither->pipeline_last_band_height != 6 ||
                    dither->pipeline_last_band_overlap != 2)) {
                goto end;
            }
            if (SIXEL_FAILED(dither->palette->vtbl->get_entries_float32(
                    dither->palette, &view)) || view.entry_count != 2 ||
                    SIXEL_FAILED(sixel_palette_get_rgb101(dither->palette,
                        &wire, &colors)) || colors != 2 ||
                    SIXEL_FAILED(sixel_decode_raw(
                        (unsigned char *)capture.data, (int)capture.size,
                        &decoded, &width, &height, &palette, &ncolors,
                        NULL)) || width != 3 || height != 18) {
                goto end;
            }
            for (n = 0; n < 54; ++n) {
                distance[0] = distance[1] = 0.0;
                for (c = 0; c < 3; ++c) {
                    candidate = pixels[3 * n + c] + positional_noise(
                        method, n % 3, n / 3, c, 0, n / 3) * 16.0 / 255.0;
                    for (p = 0; p < 2; ++p) {
                        delta = candidate - view.entries[p * 3 + c];
                        distance[p] += delta * delta;
                    }
                }
                /* These candidates stay away from nearest-color ties. */
                if (fabs(distance[0] - distance[1]) < 1e-5) {
                    goto end;
                }
                selected = distance[1] < distance[0];
                for (c = 0; c < 3; ++c) {
                    if (decoded[n] >= ncolors ||
                            palette[decoded[n] * 3 + c] !=
                            (wire[selected * 3 + c] * 255 + 50) / 100) {
                        fprintf(stderr, "band pixel %d channel %d\n", n, c);
                        goto end;
                    }
                }
            }
            free(decoded);
            free(palette);
            decoded = palette = NULL;
        }
        sixel_frame_unref(frame);
        frame = NULL;
    }
    ok = 1;
end:
    if (!ok) {
        fprintf(stderr, "positional encoder %d/%d/%d: %s: %s\n",
                method, mode, space, capture.data,
                sixel_helper_get_additional_message());
    }
    free(decoded);
    free(palette);
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
