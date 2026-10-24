/* SPDX-License-Identifier: MIT */
/* Separate requested precision from the output's effective float format;
 * precision-policy=8bit rejects the named P3 destination. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"
#include "src/options.h"
#include "tests/data/color-numeric/p3-palette.h"

static char p3_precision_output[2048];
static size_t p3_precision_size;

static int
p3_precision_write(char *data, int size, void *context)
{
    (void)context;
    if (size < 0 || (size_t)size >=
            sizeof(p3_precision_output) - p3_precision_size) {
        return -1;
    }
    memcpy(p3_precision_output + p3_precision_size, data, (size_t)size);
    p3_precision_size += (size_t)size;
    p3_precision_output[p3_precision_size] = '\0';
    return size;
}

int
test_palette_0058_p3_precision(int argc, char **argv)
{
    static struct {
        int flag;
        char const *value;
    } const options[] = {
        {'=', "greedy:B1"}, {'p', "1"},
        {'j', "scalar:resize_precision=float"},
        {'Q', "kmeans:seed=1"}, {'X', "linear"}, {'W', "gamma"},
        {'U', "display-p3"}, {'F', "none"}, {'a', "off"}, {'_', "none"},
        {'~', "none"}, {'d', "none"}, {'t', "rgb"}, {'5', "none"},
        {'4', "full-frame"}
    };
    static struct {
        char const *precision;
        char const *output;
        int requested;
        int effective;
    } const phases[] = {
        {"float", "gamma", SIXEL_OPTION_PRECISION_FLOAT32, 1},
        {"auto", "gamma", SIXEL_OPTION_PRECISION_AUTO, 0},
        {"8bit", "gamma", SIXEL_OPTION_PRECISION_8BIT, 0},
        {"auto", "display-p3", SIXEL_OPTION_PRECISION_AUTO, 1},
        {"float", "gamma", SIXEL_OPTION_PRECISION_FLOAT32, 1},
        {"auto", "gamma", SIXEL_OPTION_PRECISION_AUTO, 0},
        {"auto", "display-p3", SIXEL_OPTION_PRECISION_AUTO, 1},
        {"8bit", "gamma", SIXEL_OPTION_PRECISION_8BIT, 0},
        {"auto", "display-p3", SIXEL_OPTION_PRECISION_AUTO, 1}
    };
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    float pixels[9];
    SIXELSTATUS status;
    size_t index;
    size_t phase;
    size_t phase_count;
    int channel;
    int failed;
    int byte_request;
    int pixelformat;
    int width;
    char const *expected_wire;

    encoder = NULL;
    output = NULL;
    frame = NULL;
    failed = 1;
    if (argc != 2 ||
            SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, p3_precision_write,
                NULL, NULL))) {
        goto end;
    }
    for (index = 0; index < sizeof(options) / sizeof(options[0]); ++index) {
        /* The first reuse pair must never have selected P3. */
        if (strcmp(argv[1], "reuse") == 0 && options[index].flag == 'U') {
            continue;
        }
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, options[index].flag,
                options[index].value))) {
            goto end;
        }
    }
    phase_count = strcmp(argv[1], "reuse") == 0
        ? sizeof(phases) / sizeof(phases[0]) : 1;
    for (phase = 0; phase < phase_count; ++phase) {
        p3_precision_size = 0;
        p3_precision_output[0] = '\0';
        byte_request = strcmp(argv[1], "8bit") == 0;
        if (strcmp(argv[1], "reuse") == 0) {
            if (SIXEL_FAILED(sixel_encoder_setopt(encoder, '.',
                    phases[phase].precision)) ||
                    SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U',
                        phases[phase].output)) ||
                    encoder->precision_requested != phases[phase].requested ||
                    encoder->prefer_float32 != phases[phase].effective) {
                goto end;
            }
        } else if (byte_request) {
            if (SIXEL_FAILED(sixel_encoder_setopt(encoder, '.', "8bit"))) {
                goto end;
            }
        } else if (strcmp(argv[1], "auto") != 0 ||
                encoder->precision_requested != SIXEL_OPTION_PRECISION_AUTO ||
                encoder->prefer_float32 != 1 ||
                encoder->precision_prefer_float32 != 0) {
            goto end;
        }
        pixelformat = SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
        width = 1;
        expected_wire = "#0;2;100;0;0";
        for (channel = 0; channel < 3; ++channel) {
            pixels[channel] = (float)color_p3_red_linear[channel];
        }
        if (strcmp(argv[1], "reuse") == 0 && phase < 2) {
            /* Two black samples and one white sample have linear mean
             * 1/3. The sRGB transfer and RGB101 rounding give 61% in both
             * modes; requested/effective state distinguishes the modes. */
            pixelformat = SIXEL_PIXELFORMAT_RGBFLOAT32;
            width = 3;
            expected_wire = "#0;2;61;61;61";
            for (channel = 0; channel < 9; ++channel) {
                pixels[channel] = channel < 6 ? 0.0f : 1.0f;
            }
        }
        if (SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
                SIXEL_FAILED(sixel_frame_init_borrowed(frame, pixels, width, 1,
                    pixelformat, NULL, -1))) {
            goto end;
        }
        status = sixel_encoder_encode_frame(encoder, frame, output);
        /* A byte palette cannot carry a named P3 destination. */
        if (byte_request) {
            if (!SIXEL_FAILED(status) || p3_precision_size != 0) {
                goto end;
            }
        } else if (SIXEL_FAILED(status) ||
                strstr(p3_precision_output, expected_wire) == NULL) {
            goto end;
        }
        sixel_frame_unref(frame);
        frame = NULL;
    }
    failed = 0;
end:
    if (failed) {
        fprintf(stderr, "P3 requested/effective precision failed: %s\n",
                p3_precision_output);
    }
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return failed;
}
