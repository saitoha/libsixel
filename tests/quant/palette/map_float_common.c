/* SPDX-License-Identifier: MIT */
/* Fixed-map entry, source precision, wire and pixel-application contracts. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"
#include "src/dither.h"
#include "src/palette.h"
#include "tests/data/color-numeric/p3-palette.h"

/* Independent forward equations from generate_color_fixtures.py.
 * Ideal sRGB8 input; allow float storage and matrix coefficient error. */
static double const map_expected[5][4][3] = {
    {
        { 0.345098039216, 0.345098039216, 0.345098039216 },
        { 0.36862745098, 0.36862745098, 0.36862745098 },
        { 0.345098039216, 0.345098039216, 0.345098039216 },
        { 1, 0, 0 },
    },
    {
        { 0.0975873471419, 0.0975873471419, 0.0975873471419 },
        { 0.111932427837, 0.111932427837, 0.111932427837 },
        { 0.0975873471419, 0.0975873471419, 0.0975873471419 },
        { 1, 0, 0 },
    },
    {
        { 0.460395603649, 3.72704393857e-11, 1.71607433054e-08 },
        { 0.481931490589, 3.90138356355e-11, 1.79634699707e-08 },
        { 0.460395603649, 3.72704393857e-11, 1.71607433054e-08 },
        { 0.627955360615, 0.224863061066, 0.125846298531 },
    },
    {
        { 0.37405892152, -5.99473426181e-08, 2.39789370472e-08 },
        { 0.399040551366, -6.27514944883e-08, 2.51005977953e-08 },
        { 0.37405892152, -5.99473426181e-08, 2.39789370472e-08 },
        { 0.532407941413, 0.625722340597, 0.52502497278 },
    },
    {
        { 0.410863807252, -1.14955796687e-07, 5.09734840867e-08 },
        { 0.436539303296, -1.20333072875e-07, 5.33578658237e-08 },
        { 0.410863807252, -1.14955796687e-07, 5.09734840867e-08 },
        { 0.570283185307, 0.395572187808, 0.255974559087 },
    },
};
static double const map_p3_red[5][3] = {
    { 1.09306636244, -0.22674197357, -0.150134580937 },
    { 1.22494017628, -0.0420569547097, -0.0196375545903 },
    { 0.648574071941, 0.262041750726, 0.145001928275 },
    { 0.549709528341, 0.735109145425, 0.740355728975 },
    { 0.58723742461, 0.400233055087, 0.323143851324 },
};

typedef struct map_capture {
    char data[8192];
    size_t size;
} map_capture_t;

static int
map_write(char *data, int size, void *context)
{
    map_capture_t *capture;

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
test_map_solver(int mode, int space, char const *path,
                char const *quantizer, char const *binning,
                int output_p3, int solver)
{
    static struct { int flag; char const *value; } const options[] = {
        { '=', "1" }, { 'X', "linear" },
        { 'F', "none" }, { '_', "none" }, { '~', "5bit" },
        { 't', "rgb" }, { '4', "full-frame" },
        { '0', "none" }, { 'U', "gamma" }, { '.', "auto" }
    };
    static char const *const spaces[] = {
        "gamma", "linear", "oklab", "cielab", "din99d"
    };
    static unsigned char const export_rgb[12] = {
        88, 88, 88, 94, 94, 94, 88, 88, 88, 255, 0, 0
    };
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    sixel_dither_t *dither;
    sixel_palette_float32_entries_view_t view;
    map_capture_t capture;
    float source[6];
    unsigned char byte_source[6];
    unsigned char *decoded;
    unsigned char *decoded_palette;
    double expected;
    double tolerance;
    int width;
    int height;
    int colors;
    int byte_input;
    int byte_work;
    int image;
    int soft_image;
    int fs;
    int p3;
    int passes;
    int pass;
    int i;
    int j;
    int match;
    int selected;
    int requested_model;
    int ok;

    encoder = NULL;
    output = NULL;
    frame = NULL;
    decoded = NULL;
    decoded_palette = NULL;
    ok = 0;
    selected = -1;
    image = mode == 6 || mode == 9 || mode == 10;
    soft_image = image && binning != NULL && strcmp(binning, "soft") == 0;
    fs = mode == 1;
    passes = mode == 4 || mode == 5 || mode == 9 ? 3 : 1;
    memset(&capture, 0, sizeof(capture));
    if (space < 0 || space > 4 ||
            SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, map_write,
                &capture, NULL))) {
        goto end;
    }
    for (i = 0; i < (int)(sizeof(options) / sizeof(options[0])); ++i) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, options[i].flag,
                options[i].value))) {
            goto end;
        }
    }
    if ((quantizer != NULL && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                'Q', quantizer))) ||
            (binning != NULL && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                '5', binning))) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U',
                output_p3 ? "display-p3" : "gamma")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'W', spaces[space])) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'd',
                fs ? "fs" : "none")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'm', path)) ||
            (mode == 3 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                '.', "float32"))) ||
            (mode == 8 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                '_', "nearest:rate=0"))) ||
            (image && SIXEL_FAILED(sixel_encoder_setopt(encoder, 'L',
                "builtin:cms_engine=builtin:cms_target=linear:"
                "prefer_8bit=0!"))) ||
            (image && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                '.', "8bit"))) ||
            (mode == 7 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                'M', "gpl:-")))) {
        goto end;
    }
    requested_model = encoder->quantize_model;
    for (pass = 0; pass < passes; ++pass) {
        byte_input = mode == 2 || mode == 3 || (mode == 4 && pass == 1);
        byte_work = byte_input && mode != 3;
        p3 = output_p3 || (mode == 5 && pass == 1);
        if (mode == 9) {
            /* Resolve AUTO afresh after changing binning on one encoder. */
            solver = pass == 1 ? SIXEL_QUANTIZE_MODEL_KMEANS
                               : SIXEL_QUANTIZE_MODEL_MEDIANCUT;
            if (SIXEL_FAILED(sixel_encoder_setopt(encoder, '5',
                    pass == 1 ? "hard" : "none"))) {
                goto end;
            }
        }
        if (mode == 5 && SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U',
                p3 ? "display-p3" : "gamma"))) {
            goto end;
        }
        for (i = 0; i < 6; ++i) {
            source[i] = image ? (float)color_p3_red_linear[i % 3] : .359f;
            byte_source[i] = 92;
        }
        memset(&capture, 0, sizeof(capture));
        if (SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
                SIXEL_FAILED(sixel_frame_init_borrowed(frame,
                    byte_input ? (void *)byte_source : (void *)source,
                    fs ? 2 : 1, 1, byte_input ? SIXEL_PIXELFORMAT_RGB888
                        : image ? SIXEL_PIXELFORMAT_LINEARRGBFLOAT32
                                : SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, -1))) {
            goto end;
        }
        if (mode == 10) {
            /* Unsupported image-map binning must fail before output. */
            ok = SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame,
                output)) && capture.size == 0;
            goto end;
        }
        if (SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame,
                output)) || encoder->quantize_model != requested_model) {
            goto end;
        }
        dither = (sixel_dither_t *)encoder->diagnostic_dither;
        if (dither == NULL || dither->use_rgb101_palette != !byte_work ||
                (image && dither->quantize_model != solver)) {
            goto end;
        }
        if (!byte_work) {
            memset(&view, 0, sizeof(view));
            if (SIXEL_FAILED(dither->palette->vtbl->get_entries_float32(
                    dither->palette, &view)) || view.entries == NULL ||
                    view.entry_count == 0 || view.entry_count > 256 ||
                    (!soft_image && view.entry_count != 4)) {
                goto end;
            }
            if (image) {
                /* Ideal primaries differ slightly from the fixture's
                 * quantized ICC matrix. 3e-4 covers that profile error;
                 * clipping signed/out-of-sRGB coordinates cannot pass. */
                match = 0;
                for (i = 0; i < (int)view.entry_count; ++i) {
                    for (j = 0; j < 3; ++j) {
                        if (!isfinite(view.entries[i * 3 + j]) ||
                                fabs(view.entries[i * 3 + j]
                                    - map_p3_red[space][j]) > 3e-4) {
                            break;
                        }
                    }
                    if (j == 3) {
                        selected = i;
                        ++match;
                    }
                }
                if (match == 0 || (!soft_image && match != 1)) {
                    goto end;
                }
            } else {
                for (i = 0; i < 12; ++i) {
                    expected = map_expected[space][i / 3][i % 3];
                    tolerance = 2e-6 + fabs(expected) * 5e-6;
                    if (!isfinite(view.entries[i]) ||
                            fabs(view.entries[i] - expected) > tolerance) {
                        fprintf(stderr, "entry %d: %.9g != %.9g\n", i,
                            (double)view.entries[i], expected);
                        goto end;
                    }
                }
            }
        }
        if (!image &&
                (strstr(capture.data, "#0;2;35;35;35") == NULL ||
                 strstr(capture.data, "#1;2;37;37;37") == NULL ||
                 strstr(capture.data, "#2;2;35;35;35") == NULL ||
                 strstr(capture.data, p3 ? "#3;2;92;20;14"
                                         : "#3;2;100;0;0") == NULL)) {
            goto end;
        }
        if (SIXEL_FAILED(sixel_decode_raw((unsigned char *)capture.data,
                (int)capture.size, &decoded, &width, &height,
                &decoded_palette, &colors, NULL)) ||
                width != (fs ? 2 : 1) || height != 1) {
            goto end;
        }
        if (image) {
            if (soft_image) {
                /* Soft bins may retain nearly equal representatives.
                 * Require the applied entry to be the independent primary,
                 * without imposing a tie index or a distinct-entry count. */
                selected = decoded[0];
                if (selected >= (int)view.entry_count) {
                    goto end;
                }
                for (j = 0; j < 3; ++j) {
                    if (!isfinite(view.entries[selected * 3 + j]) ||
                            fabs(view.entries[selected * 3 + j]
                                - map_p3_red[space][j]) > 3e-4) {
                        goto end;
                    }
                }
            }
            if (decoded[0] != selected || selected >= colors ||
                    decoded_palette[selected * 3] != 255 ||
                    decoded_palette[selected * 3 + 1] != 0 ||
                    decoded_palette[selected * 3 + 2] != 0) {
                goto end;
            }
        } else if (decoded[0] != 1 || (fs && decoded[1] != 0)) {
            goto end;
        }
        if (mode == 7 && (encoder->capture_ncolors != 4 ||
                encoder->capture_palette_size != sizeof(export_rgb) ||
                memcmp(encoder->capture_palette, export_rgb,
                    sizeof(export_rgb)) != 0)) {
            goto end;
        }
        free(decoded);
        free(decoded_palette);
        decoded = NULL;
        decoded_palette = NULL;
        sixel_frame_unref(frame);
        frame = NULL;
    }
    ok = 1;
end:
    if (!ok) {
        fprintf(stderr, "map mode=%d space=%d: %s\n%s\n", mode, space,
            sixel_helper_get_additional_message(), capture.data);
    }
    free(decoded);
    free(decoded_palette);
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
test_map_float(int mode, int space, char const *path)
{
    return test_map_solver(mode, space, path, "kmeans:seed=1", "none", 0,
        SIXEL_QUANTIZE_MODEL_KMEANS);
}
