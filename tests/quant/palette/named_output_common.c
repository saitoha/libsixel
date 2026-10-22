/* SPDX-License-Identifier: MIT */
/* Named destinations share CMS arithmetic, palette ownership and wire caches.
 * Decimal oracles start from stored linear float32 samples. Workspace setup
 * is a roundtrip integration check; named XYZ has an independent oracle. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/cms.h"
#include "src/colorspace.h"
#include "src/encoder.h"
#include "src/dither.h"
#include "src/palette.h"
#include "tests/data/color-numeric/named-output.h"

static char const *const named_output_names[] = { "adobe-rgb", "dci-p3" };
static int const named_output_spaces[] = {
    SIXEL_COLORSPACE_GAMMA, SIXEL_COLORSPACE_LINEAR,
    SIXEL_COLORSPACE_OKLAB, SIXEL_COLORSPACE_CIELAB, SIXEL_COLORSPACE_DIN99D
};
static int const named_output_formats[] = {
    SIXEL_PIXELFORMAT_RGBFLOAT32, SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
    SIXEL_PIXELFORMAT_OKLABFLOAT32, SIXEL_PIXELFORMAT_CIELABFLOAT32,
    SIXEL_PIXELFORMAT_DIN99DFLOAT32
};
static char const *const named_output_work[] = {
    "gamma", "linear", "oklab", "cielab", "din99d"
};

typedef struct named_output_capture {
    unsigned char bytes[8192];
    size_t size;
} named_output_capture_t;

static int
named_output_write(char *bytes, int size, void *context)
{
    named_output_capture_t *capture;

    capture = context;
    if (size < 0 || (size_t)size > sizeof(capture->bytes) - capture->size) {
        return -1;
    }
    memcpy(capture->bytes + capture->size, bytes, (size_t)size);
    capture->size += (size_t)size;
    return size;
}

int
test_named_definition(int kind, int export_profile)
{
    double const rgb[3] = {.23, .47, .81};
    sixel_color_def_t *definition;
    sixel_color_def_t *copy;
    sixel_allocator_t *allocator;
    unsigned char *bytes;
    size_t length;
    double xyz[3];
    double tolerance;
    int c;
    int ok;

    allocator = NULL;
    copy = NULL;
    bytes = NULL;
    length = 0;
    ok = 0;
    definition = sixel_cdef_name(named_output_names[kind]);
    if (definition == NULL) {
        goto end;
    }
    tolerance = 2e-10;
    if (export_profile) {
        if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL,
                NULL, NULL)) || SIXEL_FAILED(sixel_cdef_export_icc(
                    definition, &bytes, &length, allocator))) {
            goto end;
        }
        copy = sixel_cdef_icc(bytes, length);
        sixel_cdef_free(definition);
        definition = copy;
        /* ICC coefficients/TRCs use signed 15.16 storage; the bound covers
         * coefficient quantization and the power curve over this sample. */
        tolerance = 4e-5;
    }
    if (!sixel_cdef_to_xyz(definition, rgb, xyz, 1)) {
        goto end;
    }
    for (c = 0; c < 3; ++c) {
        if (!isfinite(xyz[c]) ||
                fabs(xyz[c] - named_xyz[kind][c]) > tolerance) {
            fprintf(stderr, "named XYZ %d/%d: %.17g != %.17g\n",
                kind, c, xyz[c], named_xyz[kind][c]);
            goto end;
        }
    }
    ok = 1;
end:
    sixel_cdef_free(definition);
    if (allocator != NULL) {
        sixel_allocator_free(allocator, bytes);
        sixel_allocator_unref(allocator);
    }
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
test_named_output(int kind, int space, int model, int mode)
{
    static float const original[6] = {.4f, .1f, .03f, -.18f, .8f, .012f};
    static char const *const models[] = {
        "kmeans:seed=1", "center:seed=1", "medoids:seed=1", "heckbert"
    };
    static struct { int flag; char const *value; } const options[] = {
        {'=', "1"}, {'F', "none"}, {'_', "none"}, {'~', "none"},
        {'d', "none"}, {'t', "rgb"}, {'4', "full-frame"}, {'5', "none"},
        {'.', "auto"}, {'p', "2"}, {'a', "off"}
    };
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    named_output_capture_t capture;
    unsigned char *decoded;
    double source[3];
    double target[3];
    float pixels[6];
    float saved[6];
    SIXELSTATUS status;
    int width;
    int height;
    int i;
    int c;
    int pass;
    int selected;
    int expected;
    int ok;

    encoder = NULL;
    output = NULL;
    frame = NULL;
    decoded = NULL;
    ok = 0;
    status = SIXEL_OK;
    memset(&capture, 0, sizeof(capture));
    for (i = 0; i < 2; ++i) {
        for (c = 0; c < 3; ++c) {
            source[c] = original[i * 3 + c];
        }
        if (!sixel_color_convert(target, source, SIXEL_COLORSPACE_LINEAR,
                named_output_spaces[space])) {
            goto end;
        }
        for (c = 0; c < 3; ++c) {
            pixels[i * 3 + c] = (float)target[c];
        }
    }
    memcpy(saved, pixels, sizeof(saved));
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, named_output_write,
                &capture, NULL)) ||
            SIXEL_FAILED(sixel_frame_new(&frame, NULL))) {
        goto end;
    }
    for (i = 0; i < (int)(sizeof(options) / sizeof(options[0])); ++i) {
        if (mode == 1 && options[i].flag == 'p') {
            continue;
        }
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, options[i].flag,
                options[i].value))) {
            goto end;
        }
    }
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'Q', models[model])) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'X',
                named_output_work[space])) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'W',
                named_output_work[space])) ||
            (mode == 1 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                'I', NULL))) ||
            SIXEL_FAILED(sixel_frame_init_borrowed(frame, pixels, 2, 1,
                named_output_formats[space], NULL, -1))) {
        goto end;
    }
    if ((mode == 5 &&
            (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'p', "3")) ||
             SIXEL_FAILED(sixel_encoder_setopt(encoder, 'Z',
                "delta:threshold=0")))) ||
            (mode == 6 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                't', "hls")))) {
        goto end;
    }
    for (pass = 0; pass < (mode == 2 || mode == 5 ? 3 : 1); ++pass) {
        selected = mode == 2 || mode == 5 ? (kind + pass) % 2 : kind;
        capture.size = 0;
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U',
                named_output_names[selected]))) {
            goto end;
        }
        if (mode == 3 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                '_', "nearest:rate=0"))) {
            goto end;
        }
        if (mode == 4) {
            status = sixel_encoder_setopt(encoder, 'U', "not-a-color");
            if (status != SIXEL_BAD_ARGUMENT) {
                goto end;
            }
        }
        if (mode == 7) {
            pixels[0] = NAN;
            status = sixel_encoder_encode_frame(encoder, frame, output);
            memcpy(pixels, saved, sizeof(saved));
            if (SIXEL_SUCCEEDED(status) || capture.size != 0) {
                goto end;
            }
        }
        status = sixel_encoder_encode_frame(encoder, frame, output);
        if (SIXEL_FAILED(status) ||
                memcmp(saved, pixels, sizeof(saved)) != 0 ||
                !encoder->prefer_float32) {
            goto end;
        }
        status = sixel_decode_direct(capture.bytes, (int)capture.size,
            &decoded, &width, &height, NULL);
        if (SIXEL_FAILED(status) || width != 2 || height != 1) {
            goto end;
        }
        /* Independent destination codes have >= .035 percentage units
         * to the closest tie; the conversion budget is below .002 units. */
        for (i = 0; i < 2; ++i) {
            for (c = 0; c < 3; ++c) {
                expected = ((mode == 6 ? named_hls_wire[selected][i][c]
                    : named_wire[selected][i][c]) * 255 + 50) / 100;
                if (decoded[i * 4 + c] != expected) {
                    fprintf(stderr, "named RGB %d/%d: %d != %d\n", i, c,
                        decoded[i * 4 + c], expected);
                    goto end;
                }
            }
            if (decoded[i * 4 + 3] != 255) {
                goto end;
            }
        }
        free(decoded);
        decoded = NULL;
    }
    ok = 1;
end:
    if (!ok) {
        fprintf(stderr, "named output %d/%d/%d/%d: %x %s\n",
            kind, space, model, mode, status,
            sixel_helper_get_additional_message());
    }
    free(decoded);
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* CMS loading and destination conversion are distinct boundaries. The input
 * carries P3 ICC metadata; the independent oracle uses its stored matrix/TRC
 * and the source integers. A raw-byte or sRGB input interpretation fails. */
int
test_named_loaded(int kind)
{
    unsigned char input[8192];
    unsigned char *pixels;
    size_t length;
    SIXELSTATUS status;
    int width;
    int height;
    int i;
    int c;
    int expected;
    int ok;

    pixels = NULL;
    length = fread(input, 1u, sizeof(input), stdin);
    if (length == 0u || length == sizeof(input)) {
        return EXIT_FAILURE;
    }
    status = sixel_decode_direct(input, (int)length, &pixels,
        &width, &height, NULL);
    ok = SIXEL_SUCCEEDED(status) && width == 2 && height == 1;
    for (i = 0; i < 2 && ok; ++i) {
        for (c = 0; c < 3; ++c) {
            expected = (named_loaded_wire[kind][i][c] * 255 + 50) / 100;
            if (pixels[i * 4 + c] != expected) {
                fprintf(stderr, "loaded destination %d/%d: %d != %d\n",
                    i, c, pixels[i * 4 + c], expected);
                ok = 0;
            }
        }
        ok &= pixels[i * 4 + 3] == 255u;
    }
    free(pixels);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
