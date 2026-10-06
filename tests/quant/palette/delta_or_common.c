/* SPDX-License-Identifier: MIT */
/* OR index planes cannot represent retained destination RGB omissions. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"
#include "src/dither.h"
#include "src/sixel_decode_pixels.h"

typedef struct do_stream {
    char data[4096];
    size_t size;
} do_stream_t;

static int
do_write(char *data, int size, void *context)
{
    do_stream_t *stream;

    stream = context;
    if (size < 0 || (size_t)size >= sizeof(stream->data) - stream->size) {
        return -1;
    }
    memcpy(stream->data + stream->size, data, (size_t)size);
    stream->size += (size_t)size;
    stream->data[stream->size] = '\0';
    return size;
}

int
test_delta_or_options(int mode)
{
    static unsigned char const retained[3] = { 51, 102, 153 };
    sixel_encoder_t *encoder;
    SIXELSTATUS status;
    unsigned int threshold;
    int error_mode;
    int ormode;
    int enabled;
    int ok;

    encoder = NULL;
    ok = 0;
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL))) {
        goto end;
    }
    if (mode == 2) {
        /* The TAP wrapper sets only the initial base policy. */
        if (!encoder->sixdelta_enabled) {
            goto end;
        }
    } else if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'Z',
            mode == 0 ? "full" : "delta:T7:Eskip"))) {
        goto end;
    }
    if ((mode == 0 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
            'O', NULL))) ||
            SIXEL_FAILED(sixel_encoder_set_accumulation_buffer(encoder,
                retained, 1, 1, SIXEL_PIXELFORMAT_RGB888))) {
        goto end;
    }
    if (mode == 3) {
        /* Disabling delta before selecting OR is a supported recovery. */
        ok = SIXEL_SUCCEEDED(sixel_encoder_setopt(encoder, 'Z', "full")) &&
            SIXEL_SUCCEEDED(sixel_encoder_setopt(encoder, 'O', NULL)) &&
            !encoder->sixdelta_enabled && !encoder->accumulation_valid &&
            encoder->ormode == 1;
        goto end;
    }
    threshold = encoder->sixdelta_threshold;
    error_mode = encoder->sixdelta_error_mode;
    ormode = encoder->ormode;
    enabled = encoder->sixdelta_enabled;
    status = sixel_encoder_setopt(encoder, mode == 0 ? 'Z' : 'O',
        mode == 0 ? "delta:T9:Eskip" : NULL);
    /* Rejection must not leave half of the conflicting option installed. */
    ok = status == SIXEL_BAD_ARGUMENT &&
        strstr(sixel_helper_get_additional_message(),
            "P2=5 does not preserve omitted destination pixels") != NULL &&
        encoder->sixdelta_enabled == enabled &&
        encoder->ormode == ormode &&
        encoder->sixdelta_threshold == threshold &&
        encoder->sixdelta_error_mode == error_mode &&
        encoder->accumulation_valid &&
        encoder->accumulation_valid_mask != NULL &&
        encoder->accumulation_valid_mask[0] != 0 &&
        encoder->accumulation_pixels != NULL &&
        memcmp(encoder->accumulation_pixels, retained, sizeof(retained)) == 0;
end:
    if (!ok) {
        fprintf(stderr, "OR delta option mode=%d: %s\n", mode,
            sixel_helper_get_additional_message());
    }
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
test_delta_or_recovery(int mode)
{
    static struct { int flag; char const *value; } const options[] = {
        {'=', "1"}, {'~', "none"}, {'d', "none"}, {'0', "none"},
        {'_', "none"}, {'W', "gamma"}, {'t', "rgb"},
        {'Z', "full"}, {'7', NULL}, {'b', "gray1"}, {'O', NULL}
    };
    sixel_encoder_t *encoder;
    sixel_frame_t *frame;
    sixel_output_t *output;
    sixel_dither_t *dither;
    do_stream_t stream;
    float pixels[6];
    unsigned char bytes[6];
    unsigned char *decoded;
    unsigned char *paint;
    unsigned char *palette;
    int width;
    int height;
    int ncolors;
    unsigned int flags;
    int pass;
    int i;
    int c;
    int value;
    int ok;

    encoder = NULL;
    frame = NULL;
    output = NULL;
    decoded = paint = palette = NULL;
    ok = 0;
    pass = 0;
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, do_write, &stream, NULL))) {
        goto end;
    }
    for (i = 0; i < (int)(sizeof(options) / sizeof(options[0])); ++i) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, options[i].flag,
                options[i].value))) {
            goto end;
        }
    }
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder, '.',
            mode ? "float32" : "8bit")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U',
                mode == 2 ? "display-p3" : "gamma"))) {
        goto end;
    }
    for (pass = 0; pass < 2; ++pass) {
        memset(&stream, 0, sizeof(stream));
        for (i = 0; i < 6; ++i) {
            value = (i / 3) ^ pass;
            pixels[i] = (float)value;
            bytes[i] = (unsigned char)(value * 255);
        }
        if (SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
                SIXEL_FAILED(sixel_frame_init_borrowed(frame,
                    mode ? (unsigned char *)pixels : bytes, 2, 1,
                    mode ? SIXEL_PIXELFORMAT_RGBFLOAT32 :
                        SIXEL_PIXELFORMAT_RGB888, NULL, -1))) {
            goto end;
        }
        if (pass == 0) {
            /* Bypass setters to exercise output-time validation. A supplied
             * callback captures all bytes without a terminal or a file. */
            encoder->sixdelta_enabled = 1;
            if (sixel_encoder_encode_frame(encoder, frame, output) !=
                    SIXEL_BAD_ARGUMENT || stream.size != 0 ||
                    SIXEL_FAILED(sixel_encoder_setopt(encoder, 'Z', "full"))) {
                goto end;
            }
        }
        if (SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame, output)) ||
                strstr(stream.data, ";5q") == NULL ||
                SIXEL_FAILED(sixel_decode_raw_with_options_mask(
                    (unsigned char *)stream.data, (int)stream.size, 0,
                    &decoded, &paint, &width, &height, &palette, &ncolors,
                    &flags, encoder->allocator)) ||
                width != 2 || height != 1 || paint == NULL ||
                encoder->accumulation_valid) {
            goto end;
        }
        dither = (sixel_dither_t *)encoder->diagnostic_dither;
        if (dither == NULL || dither->use_rgb101_palette != (mode != 0)) {
            goto end;
        }
        /* OR index zero is painted, never a keep marker. Literal endpoint
         * RGB values are exact for both output definitions in this test. */
        for (i = 0; i < 2; ++i) {
            value = (i ^ pass) * 255;
            if (!paint[i]) {
                goto end;
            }
            for (c = 0; c < 3; ++c) {
                if (palette[decoded[i] * 3 + c] != value) {
                    goto end;
                }
            }
        }
        sixel_allocator_free(encoder->allocator, decoded);
        sixel_allocator_free(encoder->allocator, paint);
        sixel_allocator_free(encoder->allocator, palette);
        decoded = paint = palette = NULL;
        sixel_frame_unref(frame);
        frame = NULL;
    }
    ok = 1;
end:
    if (!ok) {
        fprintf(stderr, "OR delta mode=%d pass=%d: %s\n", mode, pass,
            sixel_helper_get_additional_message());
    }
    if (encoder != NULL) {
        sixel_allocator_free(encoder->allocator, decoded);
        sixel_allocator_free(encoder->allocator, paint);
        sixel_allocator_free(encoder->allocator, palette);
    }
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
