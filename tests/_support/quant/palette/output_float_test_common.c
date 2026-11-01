/* SPDX-License-Identifier: MIT */
/* Output conversion must not decide working-palette precision. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"
#include "src/dither.h"
#include "src/palette.h"
#include "tests/data/color-numeric/output-native.h"

static int const out_native_formats[] = {
    SIXEL_PIXELFORMAT_RGBFLOAT32, SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
    SIXEL_PIXELFORMAT_OKLABFLOAT32, SIXEL_PIXELFORMAT_CIELABFLOAT32,
    SIXEL_PIXELFORMAT_DIN99DFLOAT32
};
static char const *const out_native_spaces[] = {
    "gamma", "linear", "oklab", "cielab", "din99d"
};
static char const *const out_native_outputs[] = {
    "linear", "smpte-c", "gamma"
};

typedef struct out_native_capture {
    char data[4096];
    size_t size;
} out_native_capture_t;

static int
out_native_write(char *data, int size, void *context)
{
    out_native_capture_t *capture;

    capture = context;
    if (size < 0 || (size_t)size >= sizeof(capture->data) - capture->size) {
        return -1;
    }
    memcpy(capture->data + capture->size, data, (size_t)size);
    capture->size += (size_t)size;
    capture->data[capture->size] = '\0';
    return size;
}

static int
out_native_values(sixel_dither_t *dither, out_native_capture_t *capture,
                  int space, int output, int map)
{
    static unsigned char const map_rgb[12] = {
        88, 88, 88, 94, 94, 94, 88, 88, 88, 255, 0, 0
    };
    sixel_palette_float32_entries_view_t view;
    unsigned char const *wire;
    unsigned char const *expected_wire;
    unsigned int count;
    unsigned int entry;
    int channel;
    int sample;
    int match;
    int matches;
    double expected;
    char definition[64];

    if (dither == NULL || !dither->use_rgb101_palette ||
            dither->pixelformat != out_native_formats[space] ||
            SIXEL_FAILED(dither->palette->vtbl->get_entries_float32(
                dither->palette, &view)) || view.entries == NULL ||
            view.entry_count != (map ? 4U : 2U) ||
            SIXEL_FAILED(sixel_palette_get_rgb101(dither->palette,
                &wire, &count)) || count != view.entry_count) {
        return 0;
    }
    matches = 0;
    for (entry = 0; entry < count; ++entry) {
        match = -1;
        for (sample = 0; sample < (map ? 1 : 2); ++sample) {
            /* Stored independent inputs are the singleton centers. The
             * 2e-5 budget covers solver arithmetic/storage, not clipping
             * or projection through an 8-bit compatibility view. */
            for (channel = 0; channel < 3; ++channel) {
                expected = map ? map_rgb[entry * 3 + channel] / 255.0
                    : output_native[space].entries[sample][channel];
                if (!isfinite(view.entries[entry * 3 + channel]) ||
                        fabs(view.entries[entry * 3 + channel] - expected)
                            > 2e-5) {
                    break;
                }
            }
            if (channel == 3) {
                match = sample;
                break;
            }
        }
        if (match < 0) {
            return 0;
        }
        matches |= 1 << match;
        expected_wire = map ? output_map_wire[output][entry]
            : output_native[space].wire[output][match];
        /* Independent Decimal transfer/matrix equations start from stored
         * binary32 inputs. Every wire result is away from a rounding tie. */
        if (memcmp(wire + entry * 3, expected_wire, 3) != 0) {
            return 0;
        }
        snprintf(definition, sizeof(definition), "#%u;2;%u;%u;%u", entry,
            (unsigned)wire[entry * 3], (unsigned)wire[entry * 3 + 1],
            (unsigned)wire[entry * 3 + 2]);
        if (strstr(capture->data, definition) == NULL) {
            return 0;
        }
    }
    return map || matches == 3;
}

int
test_output_native(int space, int output_kind, int model, int mode,
                   char const *map_path)
{
    static struct { int flag; char const *value; } const options[] = {
        { '=', "greedy:B1" }, { 'F', "none" }, { '_', "none" },
        { '~', "none" }, { 'd', "none" }, { 't', "rgb" },
        { '4', "full-frame" }, { '5', "none" }, { '.', "auto" }
    };
    static char const *const models[] = {
        "kmeans:seed=1", "center:seed=1", "medoids:seed=1", "heckbert", "auto"
    };
    static int const model_codes[] = {
        SIXEL_QUANTIZE_MODEL_KMEANS, SIXEL_QUANTIZE_MODEL_KCENTER,
        SIXEL_QUANTIZE_MODEL_KMEDOIDS, SIXEL_QUANTIZE_MODEL_MEDIANCUT,
        SIXEL_QUANTIZE_MODEL_MEDIANCUT
    };
    float pixels[6];
    float saved[6];
    unsigned char bytes[6] = { 90, 136, 182, 191, 64, 32 };
    unsigned char saved_bytes[6];
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    sixel_dither_t *dither;
    out_native_capture_t capture;
    SIXELSTATUS status;
    unsigned int i;
    int pass;
    int selected_output;
    int byte;
    int ok;

    encoder = NULL;
    output = NULL;
    frame = NULL;
    ok = 0;
    byte = mode == 1;
    status = SIXEL_OK;
    memset(&capture, 0, sizeof(capture));
    for (i = 0; i < 6; ++i) {
        pixels[i] = (float)output_native[space].entries[i / 3][i % 3];
    }
    memcpy(saved, pixels, sizeof(saved));
    memcpy(saved_bytes, bytes, sizeof(bytes));
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, out_native_write,
                &capture, NULL)) ||
            SIXEL_FAILED(sixel_frame_new(&frame, NULL))) {
        goto end;
    }
    for (i = 0; i < sizeof(options) / sizeof(options[0]); ++i) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, options[i].flag,
                options[i].value))) {
            goto end;
        }
    }
    if ((mode != 5 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                'p', "2"))) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'Q', models[model])) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'X',
                out_native_spaces[space])) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'W',
                out_native_spaces[space])) ||
            (mode == 2 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                '_', "nearest:rate=0"))) ||
            ((byte || mode == 4) && SIXEL_FAILED(sixel_encoder_setopt(
                encoder, '.', "8bit"))) ||
            (mode == 5 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                'm', map_path))) ||
            (mode != 5 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                'a', "off"))) ||
            SIXEL_FAILED(sixel_frame_init_borrowed(frame,
                byte ? (void *)bytes : (void *)pixels, 2, 1,
                byte ? SIXEL_PIXELFORMAT_RGB888 : out_native_formats[space],
                NULL, -1))) {
        goto end;
    }
    for (pass = 0; pass < (mode == 3 ? 3 : 1); ++pass) {
        selected_output = mode == 3 ? pass : output_kind;
        memset(&capture, 0, sizeof(capture));
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U',
                out_native_outputs[selected_output]))) {
            goto end;
        }
        status = sixel_encoder_encode_frame(encoder, frame, output);
        dither = (sixel_dither_t *)encoder->diagnostic_dither;
        if (SIXEL_FAILED(status) || dither == NULL ||
                memcmp(saved, pixels, sizeof(saved)) != 0 ||
                memcmp(saved_bytes, bytes, sizeof(bytes)) != 0) {
            goto end;
        }
        /* precision-policy=8bit also applies a float source as bytes. */
        if (byte || mode == 4) {
            ok = !dither->use_rgb101_palette &&
                dither->pixelformat == SIXEL_PIXELFORMAT_RGB888;
            if (!ok) {
                fprintf(stderr, "byte control: rgb101=%d format=%x\n",
                    dither->use_rgb101_palette, dither->pixelformat);
            }
            goto end;
        }
        if (!out_native_values(dither, &capture, space, selected_output,
                mode == 5) || (mode != 5 &&
                dither->quantize_model != model_codes[model])) {
            goto end;
        }
    }
    ok = 1;
end:
    if (!ok) {
        fprintf(stderr, "native output W=%d U=%d Q=%d mode=%d status=%x: "
            "%s; %s\n", space, output_kind, model, mode, status,
            sixel_helper_get_additional_message(), capture.data);
    }
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
