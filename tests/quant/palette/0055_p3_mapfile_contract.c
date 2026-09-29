/* SPDX-License-Identifier: MIT */
/* Protect fixed-palette interpretation through the public encoder boundary.
 * Literal wire codes and independent gray transfer equations are the oracle;
 * decoding observes which entries were actually used by real pixels. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "tests/data/color-numeric/p3-palette.h"

typedef struct p3_map_capture {
    char data[8192];
    size_t size;
} p3_map_capture_t;

static int
p3_map_write(char *data, int size, void *context)
{
    p3_map_capture_t *capture;

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
test_palette_0055_p3_mapfile(int argc, char **argv)
{
    static struct {
        int flag;
        char const *value;
    } const options[] = {
        {'=', "1"}, {'.', "8bit"}, {'U', "display-p3"},
        {'Q', "kmeans:seed=1"}, {'X', "linear"}, {'F', "none"},
        {'_', "none"}, {'~', "none"}, {'t', "rgb"}, {'5', "none"},
        {'4', "full-frame"}, {'G', "off"}, {'0', "none"}
    };
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    p3_map_capture_t capture;
    unsigned char *pixels;
    unsigned char *palette;
    float source[6];
    double value;
    SIXELSTATUS status;
    size_t n;
    int linear;
    int fs;
    int image;
    int reuse;
    int phase;
    int width;
    int height;
    int colors;
    int result;
    int expected;

    encoder = NULL;
    output = NULL;
    frame = NULL;
    pixels = NULL;
    palette = NULL;
    result = EXIT_FAILURE;
    memset(&capture, 0, sizeof(capture));
    if (argc != 3) {
        return EXIT_FAILURE;
    }
    linear = strstr(argv[1], "gamma") == NULL;
    fs = strstr(argv[1], "fs") != NULL;
    image = strstr(argv[1], "image") != NULL;
    reuse = strcmp(argv[1], "reuse") == 0;
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, p3_map_write, &capture,
                NULL))) {
        goto end;
    }
    for (n = 0; n < sizeof(options) / sizeof(options[0]); ++n) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, options[n].flag,
                options[n].value))) {
            goto end;
        }
    }
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'W',
            linear ? "linear" : "gamma")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'd',
                fs ? "fs" : "none")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'm', argv[2]))) {
        goto end;
    }
    if (image && SIXEL_FAILED(sixel_encoder_setopt(encoder, 'L',
            "builtin:cms_engine=builtin:cms_target=linear:prefer_8bit=0!"))) {
        goto end;
    }
    for (phase = 0; phase < (reuse ? 3 : 1); ++phase) {
        if (reuse && SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U',
                phase == 1 ? "gamma" : "display-p3"))) {
            goto end;
        }
        memset(&capture, 0, sizeof(capture));
        /* With snapping bypassed, .359 selects the original 94/255 entry.
         * FS then sends a negative error (7/16) to the next pixel, which
         * selects 88/255. Wire quantization must not move these boundaries. */
        value = linear ? pow((.359 + .055) / 1.055, 2.4) : .359;
        for (n = 0; n < 6; ++n) {
            source[n] = (float)value;
        }
        if (image) {
            for (n = 0; n < 3; ++n) {
                source[n] = (float)color_p3_red_linear[n];
            }
        }
        if (SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
                SIXEL_FAILED(sixel_frame_init_borrowed(frame, source,
                    fs ? 2 : 1, 1, linear || image
                        ? SIXEL_PIXELFORMAT_LINEARRGBFLOAT32
                        : SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, -1))) {
            goto end;
        }
        status = sixel_encoder_encode_frame(encoder, frame, output);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        if (!image &&
                (strstr(capture.data, "#0;2;35;35;35") == NULL ||
                 strstr(capture.data, "#1;2;37;37;37") == NULL ||
                 strstr(capture.data, "#2;2;35;35;35") == NULL ||
                 strstr(capture.data, phase == 1 ? "#3;2;100;0;0"
                                               : "#3;2;92;20;14") == NULL)) {
            goto end;
        }
        if (SIXEL_FAILED(sixel_decode_raw((unsigned char *)capture.data,
                (int)capture.size, &pixels, &width, &height, &palette,
                &colors, NULL)) || width != (fs ? 2 : 1) || height != 1) {
            goto end;
        }
        if (image) {
            expected = pixels[0];
            if (expected >= colors || palette[expected * 3] != 255 ||
                    palette[expected * 3 + 1] != 0 ||
                    palette[expected * 3 + 2] != 0) {
                goto end;
            }
        } else if (pixels[0] != 1 ||
                   (fs && pixels[1] != 0)) {
            goto end;
        }
        free(pixels);
        free(palette);
        pixels = NULL;
        palette = NULL;
        sixel_frame_unref(frame);
        frame = NULL;
    }
    result = EXIT_SUCCESS;
end:
    if (result != EXIT_SUCCESS) {
        fprintf(stderr, "P3 mapfile %s: %s\n%s\n", argv[1],
                sixel_helper_get_additional_message(), capture.data);
    }
    free(pixels);
    free(palette);
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return result;
}
