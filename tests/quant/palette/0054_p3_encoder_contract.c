/* SPDX-License-Identifier: MIT */
/* Exercise output-definition reset and rejection before the writer starts. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "tests/data/color-numeric/p3-palette.h"

typedef struct p3_capture {
    char bytes[2048];
    size_t size;
} p3_capture_t;

static int
p3_write(char *data, int size, void *context)
{
    p3_capture_t *capture;

    capture = context;
    if (size < 0 || (size_t)size >= sizeof(capture->bytes) - capture->size) {
        return -1;
    }
    memcpy(capture->bytes + capture->size, data, (size_t)size);
    capture->size += (size_t)size;
    capture->bytes[capture->size] = '\0';
    return size;
}

int
test_palette_0054_p3_encoder(int argc, char **argv)
{
    static struct {
        int flag;
        char const *value;
    } const options[] = {
        {'=', "1"}, {'.', "float"}, {'j', "scalar:resize_precision=float"},
        {'Q', "kmeans:seed=1"}, {'X', "linear"}, {'W', "linear"},
        {'U', "display-p3"}, {'F', "none"}, {'a', "off"}, {'_', "none"},
        {'~', "none"}, {'d', "none"}, {'t', "rgb"}, {'5', "none"},
        {'4', "full-frame"}
    };
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    p3_capture_t capture;
    char first[2048];
    float source[3];
    SIXELSTATUS status;
    size_t index;
    int phase;
    int result;
    int flag;
    int palette_auto;
    char const *value;

    encoder = NULL;
    output = NULL;
    frame = NULL;
    result = EXIT_FAILURE;
    memset(&capture, 0, sizeof(capture));
    if (argc != 2) {
        return EXIT_FAILURE;
    }
    palette_auto = strcmp(argv[1], "palette-auto") == 0;
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, p3_write, &capture, NULL))) {
        goto end;
    }
    for (index = 0; index < sizeof(options) / sizeof(options[0]); ++index) {
        if (palette_auto && options[index].flag == 't') {
            continue;
        }
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, options[index].flag,
                options[index].value))) {
            goto end;
        }
    }
    if (!palette_auto && strcmp(argv[1], "reuse") != 0) {
        flag = 0;
        value = NULL;
        if (strcmp(argv[1], "diffusion") == 0) {
            flag = 'd';
            value = "interframe";
        } else if (strcmp(argv[1], "working") == 0) {
            flag = 'W';
            value = "gamma";
        }
        if (flag == 0 ||
                SIXEL_FAILED(sixel_encoder_setopt(encoder, flag, value))) {
            goto end;
        }
    }
    for (phase = 0; phase < 3; ++phase) {
        capture.size = 0;
        capture.bytes[0] = '\0';
        source[0] = 1.0f;
        source[1] = 0.0f;
        source[2] = 0.0f;
        if (strcmp(argv[1], "working") == 0) {
            for (index = 0; index < 3; ++index) {
                source[index] = (float)color_p3_red_linear[index];
            }
        }
        if (SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
                SIXEL_FAILED(sixel_frame_init_borrowed(frame, source, 1, 1,
                    SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, NULL, -1))) {
            goto end;
        }
        if (palette_auto) {
            /* Default AUTO, explicit HLS rejection, then explicit AUTO on
             * the same encoder/output. A failed request emits no bytes and
             * must not poison the next job's RGB serialization. */
            if (phase != 0 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                    't', phase == 1 ? "hls" : "auto"))) {
                goto end;
            }
            status = sixel_encoder_encode_frame(encoder, frame, output);
            if (phase == 1) {
                if (status != SIXEL_BAD_ARGUMENT || capture.size != 0) {
                    goto end;
                }
            } else {
                if (SIXEL_FAILED(status) || strstr(capture.bytes,
                        "#0;2;92;20;14") == NULL) {
                    goto end;
                }
                if (phase == 0) {
                    memcpy(first, capture.bytes, capture.size + 1U);
                } else if (strcmp(first, capture.bytes) != 0) {
                    goto end;
                }
            }
            sixel_frame_unref(frame);
            frame = NULL;
            continue;
        }
        if (strcmp(argv[1], "working") == 0) {
            status = sixel_encoder_encode_frame(encoder, frame, output);
            if (SIXEL_FAILED(status) ||
                    strstr(capture.bytes, "#0;2;100;0;0") == NULL) {
                goto end;
            }
            result = EXIT_SUCCESS;
            goto end;
        }
        if (strcmp(argv[1], "reuse") != 0) {
            status = sixel_encoder_encode_frame(encoder, frame, output);
            if (status != SIXEL_BAD_ARGUMENT || capture.size != 0) {
                goto end;
            }
            result = EXIT_SUCCESS;
            goto end;
        }
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U',
                phase == 1 ? "gamma" : "display-p3")) ||
                SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame,
                    output))) {
            goto end;
        }
        if (strstr(capture.bytes, phase == 1 ? "#0;2;100;0;0" :
                "#0;2;92;20;14") == NULL) {
            goto end;
        }
        if (phase == 0) {
            memcpy(first, capture.bytes, capture.size + 1U);
        } else if (phase == 2 && strcmp(first, capture.bytes) != 0) {
            goto end;
        }
        sixel_frame_unref(frame);
        frame = NULL;
    }
    result = EXIT_SUCCESS;
end:
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    if (result != EXIT_SUCCESS) {
        fprintf(stderr, "P3 encoder contract failed: %s: %s\n", argv[1],
                sixel_helper_get_additional_message());
    }
    return result;
}
