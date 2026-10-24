/* SPDX-License-Identifier: MIT */
/* Palette-free foreground masks cannot erase under delta preservation. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"
#include "src/sixel_decode_pixels.h"

typedef struct dm_stream {
    char data[4096];
    size_t size;
} dm_stream_t;

static int
dm_write(char *data, int size, void *context)
{
    dm_stream_t *stream;

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
test_delta_mono_options(int mode)
{
    static unsigned char const retained[3] = { 51, 102, 153 };
    sixel_encoder_t *encoder;
    SIXELSTATUS status;
    unsigned int threshold;
    int error_mode;
    int color_option;
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
            'e', NULL))) ||
            SIXEL_FAILED(sixel_encoder_set_accumulation_buffer(encoder,
                retained, 1, 1, SIXEL_PIXELFORMAT_RGB888))) {
        goto end;
    }
    if (mode == 3) {
        /* Disabling delta before selecting mono is a supported recovery. */
        ok = SIXEL_SUCCEEDED(sixel_encoder_setopt(encoder, 'Z', "full")) &&
            SIXEL_SUCCEEDED(sixel_encoder_setopt(encoder, 'e', NULL)) &&
            !encoder->sixdelta_enabled && !encoder->accumulation_valid &&
            encoder->color_option == SIXEL_COLOR_OPTION_MONOCHROME;
        goto end;
    }
    threshold = encoder->sixdelta_threshold;
    error_mode = encoder->sixdelta_error_mode;
    color_option = encoder->color_option;
    enabled = encoder->sixdelta_enabled;
    status = sixel_encoder_setopt(encoder, mode == 0 ? 'Z' : 'e',
        mode == 0 ? "delta:T9:Eskip" : NULL);
    /* Rejection must not leave half of the conflicting option installed. */
    ok = status == SIXEL_BAD_ARGUMENT &&
        strstr(sixel_helper_get_additional_message(),
            "transparent background cannot erase previous pixels") != NULL &&
        encoder->sixdelta_enabled == enabled &&
        encoder->color_option == color_option &&
        encoder->sixdelta_threshold == threshold &&
        encoder->sixdelta_error_mode == error_mode &&
        encoder->accumulation_valid &&
        encoder->accumulation_valid_mask != NULL &&
        encoder->accumulation_valid_mask[0] != 0 &&
        encoder->accumulation_pixels != NULL &&
        memcmp(encoder->accumulation_pixels, retained, sizeof(retained)) == 0;
end:
    if (!ok) {
        fprintf(stderr, "mono delta option mode=%d: %s\n", mode,
            sixel_helper_get_additional_message());
    }
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
test_delta_mono_runtime(int unused)
{
    unsigned char pixels[6] = { 0, 0, 0, 255, 255, 255 };
    sixel_encoder_t *encoder;
    sixel_frame_t *frame;
    sixel_output_t *output;
    dm_stream_t stream;
    int ok;

    (void)unused;
    encoder = NULL;
    frame = NULL;
    output = NULL;
    ok = 0;
    memset(&stream, 0, sizeof(stream));
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'Z', "full")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'e', NULL)) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'd', "none")) ||
            SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
            SIXEL_FAILED(sixel_frame_init_borrowed(frame, pixels, 2, 1,
                SIXEL_PIXELFORMAT_RGB888, NULL, -1)) ||
            SIXEL_FAILED(sixel_output_new(&output, dm_write, &stream, NULL))) {
        goto end;
    }
    /* Bypass the option guards to exercise final output validation. */
    encoder->sixdelta_enabled = 1;
    if (sixel_encoder_encode_frame(encoder, frame, output) !=
            SIXEL_BAD_ARGUMENT || stream.size != 0 ||
            strstr(sixel_helper_get_additional_message(),
                "transparent background cannot erase previous pixels") ==
                NULL) {
        goto end;
    }
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'Z', "full")) ||
            SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame, output))) {
        goto end;
    }
    /* Ordinary mono still emits a palette-free foreground mask. */
    ok = stream.size != 0 && strchr(stream.data, '#') == NULL &&
        strchr(stream.data, '@') != NULL && !encoder->accumulation_valid;
end:
    if (!ok) {
        fprintf(stderr, "mono delta output: %s\n",
            sixel_helper_get_additional_message());
    }
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
test_delta_bw_frames(int unused)
{
    static struct { int flag; char const *value; } const options[] = {
        {'=', "balanced:B1"}, {'~', "none"}, {'d', "none"}, {'0', "none"},
        {'_', "none"}, {'.', "float32"}, {'W', "gamma"}, {'U', "gamma"},
        {'t', "rgb"}, {'Z', "delta"}, {'7', NULL}, {'b', "gray1"}
    };
    sixel_encoder_t *encoder;
    sixel_frame_t *frame;
    sixel_output_t *output;
    dm_stream_t stream;
    float pixels[6];
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

    (void)unused;
    encoder = NULL;
    frame = NULL;
    output = NULL;
    decoded = paint = palette = NULL;
    ok = 0;
    pass = 0;
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, dm_write, &stream, NULL))) {
        goto end;
    }
    for (i = 0; i < (int)(sizeof(options) / sizeof(options[0])); ++i) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, options[i].flag,
                options[i].value))) {
            goto end;
        }
    }
    /* Opaque black/white is a different contract from -e. Both endpoints
     * can overwrite each other; repeating the last frame can omit both. */
    for (pass = 0; pass < 3; ++pass) {
        memset(&stream, 0, sizeof(stream));
        for (i = 0; i < 6; ++i) {
            pixels[i] = (float)((i / 3) ^ (pass != 0));
        }
        if (SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
                SIXEL_FAILED(sixel_frame_init_borrowed(frame,
                    (unsigned char *)pixels, 2, 1,
                    SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, -1)) ||
                SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame,
                    output)) ||
                SIXEL_FAILED(sixel_decode_raw_with_options_mask(
                    (unsigned char *)stream.data, (int)stream.size, 0,
                    &decoded, &paint, &width, &height, &palette, &ncolors,
                    &flags, encoder->allocator)) ||
                width != 2 || height != 1 || paint == NULL ||
                !encoder->accumulation_valid) {
            goto end;
        }
        for (i = 0; i < 2; ++i) {
            value = (i ^ (pass != 0)) * 255;
            if ((paint[i] != 0) != (pass != 2) ||
                    !encoder->accumulation_valid_mask[i]) {
                goto end;
            }
            for (c = 0; c < 3; ++c) {
                /* Literal endpoints are exact, with no rounding boundary. */
                if (encoder->accumulation_pixels[i * 3 + c] != value ||
                        (pass != 2 &&
                         palette[decoded[i] * 3 + c] != value)) {
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
        fprintf(stderr, "opaque BW delta pass=%d: %s\n", pass,
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
