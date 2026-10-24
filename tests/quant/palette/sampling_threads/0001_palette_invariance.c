/* SPDX-License-Identifier: MIT */
/* Compare emitted RGB register definitions, not thread-dependent dither. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>

#include "src/frame.h"

#define SAMPLING_WIRE_CAPACITY (2u * 1024u * 1024u)

typedef struct sampling_wire_capture {
    char *bytes;
    size_t length;
} sampling_wire_capture_t;

static int
sampling_threads_write(char *data, int size, void *priv)
{
    sampling_wire_capture_t *capture;

    capture = priv;
    if (size < 0 || (size_t)size >=
            SAMPLING_WIRE_CAPACITY - capture->length) {
        return -1;
    }
    memcpy(capture->bytes + capture->length, data, (size_t)size);
    capture->length += (size_t)size;
    capture->bytes[capture->length] = '\0';
    return size;
}

static int
sampling_threads_encode(char const *policy, int quality, int geometry,
                        char const *threads, int palette[256][3])
{
    char threading[64];
    static struct {
        int flag;
        char const *value;
    } const quality_options[] = {
        { 'Q', "kmeans:seed=1" }, { 'F', "ward" },
        { 'W', "cielab" }, { 'X', "cielab" }, { 'd', "lso2" }
    };
    sixel_encoder_t *encoder;
    sixel_frame_t *frame;
    sixel_output_t *output;
    sampling_wire_capture_t capture;
    unsigned char pixels[256 * 192 * 3];
    char *cursor;
    int x;
    int y;
    int index;
    int red;
    int green;
    int blue;
    int consumed;
    int definitions;
    int ok;
    size_t option;

    (void)snprintf(threading, sizeof(threading), "balanced:B%s", threads);
    encoder = NULL;
    frame = NULL;
    output = NULL;
    capture.bytes = malloc(SAMPLING_WIRE_CAPACITY);
    capture.length = 0u;
    ok = 0;
    definitions = 0;
    memset(palette, 0xff, 256u * 3u * sizeof(int));
    /* A nonperiodic spatial color distribution exceeds the default target.
     * The crop also exceeds it; resizing creates interpolated colors. */
    for (y = 0; y < 192; ++y) {
        for (x = 0; x < 256; ++x) {
            index = (y * 256 + x) * 3;
            pixels[index] = (unsigned char)(x + y * 7 + x * y / 23);
            pixels[index + 1] = (unsigned char)(y + x * 3 + x * y / 41);
            pixels[index + 2] = (unsigned char)(x / 2 + y * 5);
        }
    }
    if (capture.bytes == NULL ||
            SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
            SIXEL_FAILED(sixel_frame_init_borrowed(frame, pixels,
                256, 192, SIXEL_PIXELFORMAT_RGB888, NULL, -1)) ||
            SIXEL_FAILED(sixel_output_new(&output, sampling_threads_write,
                &capture, NULL))) {
        goto end;
    }
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'G', "off")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '=', threading)) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '4', policy))) {
        goto end;
    }
    if (quality) {
        for (option = 0u; option < sizeof(quality_options) /
                sizeof(quality_options[0]); ++option) {
            if (SIXEL_FAILED(sixel_encoder_setopt(encoder,
                    quality_options[option].flag,
                    quality_options[option].value))) {
                goto end;
            }
        }
    }
    if ((geometry == 1 &&
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'c',
                "192x144+31+23"))) ||
            (geometry == 2 &&
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'w', "64"))) ||
            (geometry == 3 &&
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'w', "512"))) ||
            SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame, output))) {
        goto end;
    }
    cursor = capture.bytes;
    while ((cursor = strchr(cursor, '#')) != NULL) {
        consumed = 0;
        if (sscanf(cursor, "#%d;2;%d;%d;%d%n", &index, &red, &green,
                &blue, &consumed) == 4 && consumed > 0) {
            if (index < 0 || index >= 256 || red < 0 || red > 100 ||
                    green < 0 || green > 100 || blue < 0 || blue > 100) {
                goto end;
            }
            palette[index][0] = red;
            palette[index][1] = green;
            palette[index][2] = blue;
            ++definitions;
        }
        ++cursor;
    }
    ok = definitions > 0;
end:
    sixel_output_unref(output);
    sixel_frame_unref(frame);
    sixel_encoder_unref(encoder);
    free(capture.bytes);
    return ok;
}

int
test_palette_sampling_threads(int argc, char **argv)
{
    static char const *const budgets[] = { "1", "2", "3", "8" };
    int expected[256][3];
    int actual[256][3];
    size_t budget;

    if (argc != 4) {
        return EXIT_FAILURE;
    }
    for (budget = 0u; budget < sizeof(budgets) / sizeof(budgets[0]);
            ++budget) {
        if (!sampling_threads_encode(argv[1], atoi(argv[2]), atoi(argv[3]),
                budgets[budget], actual)) {
            fprintf(stderr, "sampling encode failed at threads=%s: %s\n",
                budgets[budget], sixel_helper_get_additional_message());
            return EXIT_FAILURE;
        }
        if (budget == 0u) {
            memcpy(expected, actual, sizeof(expected));
        } else if (memcmp(expected, actual, sizeof(expected)) != 0) {
            fprintf(stderr, "palette differs at threads=%s\n",
                budgets[budget]);
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}
