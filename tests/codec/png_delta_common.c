/* SPDX-License-Identifier: MIT */
/* Standalone PNG snapshots have no receiver's retained destination plane. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"
#include "src/dither.h"
#include "src/sixel_decode_pixels.h"

static int
png_delta_message(void)
{
    return strstr(sixel_helper_get_additional_message(),
        "snapshot decoding has no retained base surface") != NULL;
}

int
test_png_delta_options(int mode)
{
    static unsigned char const retained[3] = { 51, 102, 153 };
    static char const *const targets[] = {
        "png:-", "png:unused-snapshot.png", "unused-snapshot.png"
    };
    sixel_encoder_t *encoder;
    char *png_path;
    char *sixel_path;
    SIXELSTATUS status;
    unsigned int threshold;
    int error_mode;
    int enabled;
    int png;
    int to_stdout;
    int fd;
    int png_first;
    int ok;

    encoder = NULL;
    ok = 0;
    png_first = mode < 6 && (mode & 1) == 0;
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL))) {
        goto end;
    }
    if (mode == 6) {
        /* Only the TAP wrapper selects the initial environment policy. */
        if (!encoder->sixdelta_enabled) {
            goto end;
        }
    } else if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'Z',
            png_first || mode == 8 ? "full" : "delta:T7:Eskip"))) {
        goto end;
    }
    if ((png_first && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                'o', targets[mode / 2]))) ||
            SIXEL_FAILED(sixel_encoder_set_accumulation_buffer(encoder,
                retained, 1, 1, SIXEL_PIXELFORMAT_RGB888))) {
        goto end;
    }
    if (mode == 7) {
        ok = SIXEL_SUCCEEDED(sixel_encoder_setopt(encoder, 'Z', "full")) &&
            SIXEL_SUCCEEDED(sixel_encoder_setopt(encoder, 'o', "png:-")) &&
            !encoder->sixdelta_enabled && !encoder->accumulation_valid &&
            encoder->output_is_png && encoder->output_png_to_stdout;
        goto end;
    }
    if (mode == 8) {
        /* Switching away from a snapshot permits ordinary SIXEL delta. */
        ok = SIXEL_SUCCEEDED(sixel_encoder_setopt(encoder, 'o', targets[1])) &&
            SIXEL_SUCCEEDED(sixel_encoder_setopt(encoder, 'o', "-")) &&
            SIXEL_SUCCEEDED(sixel_encoder_setopt(encoder, 'Z', "delta")) &&
            encoder->sixdelta_enabled && !encoder->output_is_png &&
            !encoder->output_png_to_stdout && encoder->png_output_path == NULL;
        goto end;
    }
    threshold = encoder->sixdelta_threshold;
    error_mode = encoder->sixdelta_error_mode;
    enabled = encoder->sixdelta_enabled;
    png = encoder->output_is_png;
    to_stdout = encoder->output_png_to_stdout;
    png_path = encoder->png_output_path;
    sixel_path = encoder->sixel_output_path;
    fd = encoder->outfd;
    status = sixel_encoder_setopt(encoder, png_first ? 'Z' : 'o',
        png_first ? "delta:T9:Ediffuse" : targets[mode < 6 ? mode / 2 : 0]);
    /* Both directions must reject before changing either output ownership
     * or policy settings. A supplied history remains reusable. */
    ok = status == SIXEL_BAD_ARGUMENT && png_delta_message() &&
        encoder->sixdelta_enabled == enabled &&
        encoder->sixdelta_threshold == threshold &&
        encoder->sixdelta_error_mode == error_mode &&
        encoder->output_is_png == png &&
        encoder->output_png_to_stdout == to_stdout &&
        encoder->png_output_path == png_path &&
        encoder->sixel_output_path == sixel_path && encoder->outfd == fd &&
        (png_path == NULL || strcmp(png_path, "unused-snapshot.png") == 0) &&
        encoder->accumulation_valid &&
        encoder->accumulation_valid_mask != NULL &&
        encoder->accumulation_valid_mask[0] != 0 &&
        encoder->accumulation_pixels != NULL &&
        memcmp(encoder->accumulation_pixels, retained, sizeof(retained)) == 0;
end:
    if (!ok) {
        fprintf(stderr, "PNG delta option mode=%d: %s\n", mode,
            sixel_helper_get_additional_message());
    }
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

typedef struct png_delta_stream {
    char data[4096];
    size_t size;
} png_delta_stream_t;

static int
png_delta_write(char *data, int size, void *context)
{
    png_delta_stream_t *stream;

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
test_png_delta_recovery(int mode)
{
    static struct { int flag; char const *value; } const options[] = {
        {'=', "balanced:B1"}, {'~', "none"}, {'d', "none"}, {'0', "none"},
        {'_', "none"}, {'W', "gamma"}, {'t', "rgb"}, {'Z', "full"},
        {'7', NULL}, {'b', "gray1"}, {'o', "png:-"}
    };
    float pixels[6] = { 1.f, 1.f, 1.f, 0.f, 0.f, 0.f };
    unsigned char bytes[6] = { 255, 255, 255, 0, 0, 0 };
    sixel_encoder_t *encoder;
    sixel_frame_t *frame;
    sixel_output_t *output;
    sixel_dither_t *dither;
    png_delta_stream_t stream;
    unsigned char *decoded;
    unsigned char *paint;
    unsigned char *palette;
    unsigned int flags;
    int width;
    int height;
    int ncolors;
    int i;
    int c;
    int ok;

    encoder = NULL;
    frame = NULL;
    output = NULL;
    decoded = paint = palette = NULL;
    memset(&stream, 0, sizeof(stream));
    ok = 0;
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, png_delta_write,
                &stream, NULL))) {
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
                mode == 2 ? "display-p3" : "gamma")) ||
            SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
            SIXEL_FAILED(sixel_frame_init_borrowed(frame,
                mode ? (unsigned char *)pixels : bytes, 2, 1,
                mode ? SIXEL_PIXELFORMAT_RGBFLOAT32 : SIXEL_PIXELFORMAT_RGB888,
                NULL, -1))) {
        goto end;
    }
    /* Bypass setters to exercise early frame validation. An in-memory
     * callback captures the staged SIXEL body, without a real terminal. */
    encoder->sixdelta_enabled = 1;
    if (sixel_encoder_encode_frame(encoder, frame, output) !=
            SIXEL_BAD_ARGUMENT || !png_delta_message() || stream.size != 0 ||
            encoder->diagnostic_dither != NULL ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'Z', "full")) ||
            SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame, output)) ||
            SIXEL_FAILED(sixel_decode_raw_with_options_mask(
                (unsigned char *)stream.data, (int)stream.size, 0,
                &decoded, &paint, &width, &height, &palette, &ncolors,
                &flags, encoder->allocator)) ||
            width != 2 || height != 1 || paint == NULL ||
            encoder->accumulation_valid || !encoder->output_is_png) {
        goto end;
    }
    dither = (sixel_dither_t *)encoder->diagnostic_dither;
    if (dither == NULL || dither->use_rgb101_palette != (mode != 0)) {
        goto end;
    }
    /* These independently fixed endpoints survive all supported RGB output
     * definitions exactly. A keep is not a painted black/white pixel. */
    for (i = 0; i < 2; ++i) {
        if (!paint[i] || decoded[i] >= ncolors) {
            goto end;
        }
        for (c = 0; c < 3; ++c) {
            if (palette[decoded[i] * 3 + c] != (i == 0 ? 255 : 0)) {
                goto end;
            }
        }
    }
    ok = 1;
end:
    if (!ok) {
        fprintf(stderr, "PNG delta recovery mode=%d: %s\n", mode,
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

int
test_png_delta_file_guard(int mode)
{
    static unsigned char const retained[3] = { 51, 102, 153 };
    sixel_encoder_t *encoder;
    char *path;
    SIXELSTATUS status;
    int fd;
    int ok;

    encoder = NULL;
    ok = 0;
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'Z', "full")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'o',
                mode ? "png:-" : "png:unused-snapshot.png")) ||
            SIXEL_FAILED(sixel_encoder_set_accumulation_buffer(encoder,
                retained, 1, 1, SIXEL_PIXELFORMAT_RGB888))) {
        goto end;
    }
    path = encoder->png_output_path;
    fd = encoder->outfd;
    encoder->sixdelta_enabled = 1;
    /* No input or staging file is needed: the conflict precedes all input
     * access and preserves the destination for a later full conversion. */
    status = sixel_encoder_encode(encoder, "unused-input-for-delta-png");
    ok = status == SIXEL_BAD_ARGUMENT && png_delta_message() &&
        encoder->png_output_path == path && encoder->outfd == fd &&
        encoder->output_is_png && encoder->output_png_to_stdout == mode &&
        encoder->accumulation_valid &&
        memcmp(encoder->accumulation_pixels, retained, sizeof(retained)) == 0;
    if (ok && path != NULL) {
        ok = strcmp(path, "unused-snapshot.png") == 0;
    }
end:
    if (!ok) {
        fprintf(stderr, "PNG delta file guard mode=%d: %s\n", mode,
            sixel_helper_get_additional_message());
    }
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
