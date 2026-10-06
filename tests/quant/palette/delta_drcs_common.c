/* SPDX-License-Identifier: MIT */
/* Glyph definitions cannot represent retained destination RGB omissions. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#if HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"
#include "src/dither.h"
#include "src/sixel_decode_pixels.h"
#include "compat_stub.h"

#if defined(TIOCGWINSZ) && !defined(__EMSCRIPTEN__)
typedef struct dd_stream {
    char data[4096];
    size_t size;
} dd_stream_t;

static int
dd_write(char *data, int size, void *context)
{
    dd_stream_t *stream;

    stream = context;
    if (size < 0 || (size_t)size >= sizeof(stream->data) - stream->size) {
        return -1;
    }
    memcpy(stream->data + stream->size, data, (size_t)size);
    stream->size += (size_t)size;
    stream->data[stream->size] = '\0';
    return size;
}
#endif

int
test_delta_drcs_options(int mode)
{
    static unsigned char const retained[3] = { 51, 102, 153 };
    sixel_encoder_t *encoder;
    SIXELSTATUS status;
    unsigned int threshold;
    int error_mode;
    int drcs;
    int tile_fd;
    int mmv;
    unsigned short charset;
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
            '@', "2:1"))) ||
            SIXEL_FAILED(sixel_encoder_set_accumulation_buffer(encoder,
                retained, 1, 1, SIXEL_PIXELFORMAT_RGB888))) {
        goto end;
    }
    if (mode == 3) {
        /* Disabling delta before selecting DRCS is a supported recovery. */
        ok = SIXEL_SUCCEEDED(sixel_encoder_setopt(encoder, 'Z', "full")) &&
            SIXEL_SUCCEEDED(sixel_encoder_setopt(encoder, '@', "2:1")) &&
            !encoder->sixdelta_enabled && !encoder->accumulation_valid &&
            encoder->fdrcs == 1;
        goto end;
    }
    threshold = encoder->sixdelta_threshold;
    error_mode = encoder->sixdelta_error_mode;
    drcs = encoder->fdrcs;
    tile_fd = encoder->tile_outfd;
    mmv = encoder->drcs_mmv;
    charset = encoder->drcs_charset_no;
    enabled = encoder->sixdelta_enabled;
    status = sixel_encoder_setopt(encoder, mode == 0 ? 'Z' : '@',
        mode == 0 ? "delta:T9:Eskip" : "3:2:-");
    /* Rejection must not leave half of the conflicting option installed. */
    ok = status == SIXEL_BAD_ARGUMENT &&
        strstr(sixel_helper_get_additional_message(),
            "glyph definitions do not preserve destination pixels") != NULL &&
        encoder->sixdelta_enabled == enabled &&
        encoder->fdrcs == drcs && encoder->tile_outfd == tile_fd &&
        encoder->drcs_mmv == mmv && encoder->drcs_charset_no == charset &&
        encoder->sixdelta_threshold == threshold &&
        encoder->sixdelta_error_mode == error_mode &&
        encoder->accumulation_valid &&
        encoder->accumulation_valid_mask != NULL &&
        encoder->accumulation_valid_mask[0] != 0 &&
        encoder->accumulation_pixels != NULL &&
        memcmp(encoder->accumulation_pixels, retained, sizeof(retained)) == 0;
end:
    if (!ok) {
        fprintf(stderr, "DRCS delta option mode=%d: %s\n", mode,
            sixel_helper_get_additional_message());
    }
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
test_delta_drcs_recovery(int mode)
{
#if defined(TIOCGWINSZ) && !defined(__EMSCRIPTEN__)
    static struct { int flag; char const *value; } const options[] = {
        {'=', "1"}, {'~', "none"}, {'d', "none"}, {'0', "none"},
        {'_', "none"}, {'W', "gamma"}, {'t', "rgb"},
        {'Z', "full"}, {'7', NULL}, {'b', "gray1"}, {'@', "2:1"}
    };
    sixel_encoder_t *encoder;
    sixel_frame_t *frame;
    sixel_output_t *output;
    sixel_dither_t *dither;
    static char const prefix[] = "\033[?8800h\033P1;0;0;2;1;3;6;0{ 0";
    dd_stream_t stream;
    char normal[4096];
    size_t body_size;
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
            SIXEL_FAILED(sixel_output_new(&output, dd_write, &stream, NULL))) {
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
    /* Keep geometry cached even when testing a missing validation guard:
     * neither the passing test nor its mutation may open the user's tty. */
    encoder->cell_width = 2;
    encoder->cell_height = 6;
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
                    encoder->diagnostic_dither != NULL ||
                    SIXEL_FAILED(sixel_encoder_setopt(encoder, 'Z', "full"))) {
                goto end;
            }
        }
        if (SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame, output)) ||
                strncmp(stream.data, prefix, sizeof(prefix) - 1) != 0 ||
                stream.size < sizeof(prefix) + 1 ||
                memcmp(stream.data + stream.size - 2, "\033\\", 2) != 0) {
            goto end;
        }
        /* Check the actual glyph envelope, then decode only its SIXEL
         * body. This is a body oracle, not a DRCS receiver simulation. */
        body_size = stream.size - (sizeof(prefix) - 1) - 2;
        memcpy(normal, "\033Pq", 3);
        memcpy(normal + 3, stream.data + sizeof(prefix) - 1, body_size);
        memcpy(normal + 3 + body_size, "\033\\", 2);
        if (SIXEL_FAILED(sixel_decode_raw_with_options_mask(
                    (unsigned char *)normal, (int)body_size + 5, 0,
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
        /* Full glyph bodies contain both literal black/white endpoints.
         * These endpoints are exact under both tested color definitions. */
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
        fprintf(stderr, "DRCS delta mode=%d pass=%d: %s\n", mode, pass,
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
#else
    (void)mode;
    return 77;
#endif
}

int
test_delta_drcs_tile_file(char const *path)
{
    static char const marker[] = "existing tile output\n";
    sixel_encoder_t *encoder;
    FILE *file;
    char argument[4096];
    char actual[sizeof(marker)];
    SIXELSTATUS status;
    int length;
    int ok;

    encoder = NULL;
    file = NULL;
    ok = 0;
    length = snprintf(argument, sizeof(argument), "3:2:%s", path);
    if (length < 0 || (size_t)length >= sizeof(argument)) {
        goto end;
    }
    /* This file is the observable side effect: a rejected selection must
     * not truncate its contents. Do not create incidental log artifacts. */
    file = sixel_compat_fopen(path, "wb");
    if (file == NULL || fwrite(marker, 1, sizeof(marker), file) !=
            sizeof(marker)) {
        goto end;
    }
    fclose(file);
    file = NULL;
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'Z', "delta"))) {
        goto end;
    }
    status = sixel_encoder_setopt(encoder, '@', argument);
    file = sixel_compat_fopen(path, "rb");
    ok = status == SIXEL_BAD_ARGUMENT && encoder->fdrcs == 0 &&
        encoder->tile_outfd == -1 && file != NULL &&
        fread(actual, 1, sizeof(actual), file) == sizeof(actual) &&
        memcmp(actual, marker, sizeof(marker)) == 0 && fgetc(file) == EOF;
end:
    if (file != NULL) {
        fclose(file);
    }
    if (!ok) {
        fprintf(stderr, "DRCS rejected tile file was changed: %s\n", path);
    }
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
