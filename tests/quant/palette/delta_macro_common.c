/* SPDX-License-Identifier: MIT */
/* A shared display history cannot track arbitrary macro invocation bases. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"
#include "src/dither.h"

int
test_delta_macro_options(int mode)
{
    static unsigned char const retained[3] = { 51, 102, 153 };
    sixel_encoder_t *encoder;
    SIXELSTATUS status;
    unsigned int threshold;
    int error_mode;
    int use_macro;
    int number;
    int enabled;
    int flag;
    int macro_first;
    int ok;

    encoder = NULL;
    ok = 0;
    flag = mode == 2 || mode == 3 || mode == 5 || mode == 7 || mode == 8
        ? 'n' : 'u';
    macro_first = mode == 0 || mode == 2 || mode == 8;
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL))) {
        goto end;
    }
    if (mode == 4 || mode == 5) {
        /* The wrapper selects the environment base without a later -Z. */
        if (!encoder->sixdelta_enabled) {
            goto end;
        }
    } else if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'Z',
            macro_first ? "full" : "delta:T7:Eskip"))) {
        goto end;
    }
    if ((macro_first && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                flag, flag == 'n' ? "7" : NULL))) ||
            (mode == 8 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                'u', NULL))) ||
            SIXEL_FAILED(sixel_encoder_set_accumulation_buffer(encoder,
                retained, 1, 1, SIXEL_PIXELFORMAT_RGB888))) {
        goto end;
    }
    if (mode == 6 || mode == 7) {
        ok = SIXEL_SUCCEEDED(sixel_encoder_setopt(encoder, 'Z', "full")) &&
            SIXEL_SUCCEEDED(sixel_encoder_setopt(encoder, flag,
                flag == 'n' ? "7" : NULL)) &&
            !encoder->sixdelta_enabled && !encoder->accumulation_valid &&
            (flag == 'n' ? encoder->macro_number == 7 : encoder->fuse_macro);
        goto end;
    }
    threshold = encoder->sixdelta_threshold;
    error_mode = encoder->sixdelta_error_mode;
    use_macro = encoder->fuse_macro;
    number = encoder->macro_number;
    enabled = encoder->sixdelta_enabled;
    status = sixel_encoder_setopt(encoder, macro_first ? 'Z' : flag,
        macro_first ? "delta:T9:Ediffuse" : flag == 'n' ? "19" : NULL);
    /* A rejected delta must not alter threshold/error settings or the
     * existing macro selection; the inverse order has the same atomicity. */
    ok = status == SIXEL_BAD_ARGUMENT &&
        strstr(sixel_helper_get_additional_message(),
            "macro invocation has no tracked base surface") != NULL &&
        encoder->sixdelta_enabled == enabled &&
        encoder->fuse_macro == use_macro && encoder->macro_number == number &&
        encoder->sixdelta_threshold == threshold &&
        encoder->sixdelta_error_mode == error_mode &&
        encoder->accumulation_valid &&
        encoder->accumulation_valid_mask != NULL &&
        encoder->accumulation_valid_mask[0] != 0 &&
        encoder->accumulation_pixels != NULL &&
        memcmp(encoder->accumulation_pixels, retained, sizeof(retained)) == 0;
end:
    if (!ok) {
        fprintf(stderr, "macro delta option mode=%d: %s\n", mode,
            sixel_helper_get_additional_message());
    }
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
test_delta_macro_recovery(int mode)
{
    static struct { int flag; char const *value; } const options[] = {
        {'=', "balanced:B1"}, {'~', "none"}, {'d', "none"}, {'0', "none"},
        {'_', "none"}, {'W', "gamma"}, {'t', "rgb"}, {'E', "fast"},
        {'Z', "full"}, {'7', NULL}, {'b', "gray1"}
    };
    float pixels[3] = { 1.f, 1.f, 1.f };
    unsigned char bytes[3] = { 255, 255, 255 };
    sixel_encoder_t *encoder;
    sixel_frame_t *frame;
    sixel_dither_t *dither;
    SIXELSTATUS status;
    int i;
    int ok;

    encoder = NULL;
    frame = NULL;
    ok = 0;
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL))) {
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
            SIXEL_FAILED(sixel_encoder_setopt(encoder,
                mode == 1 ? 'n' : 'u', mode == 1 ? "7" : NULL)) ||
            (mode == 2 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                'n', "7"))) ||
            SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
            SIXEL_FAILED(sixel_frame_init_borrowed(frame,
                mode ? (unsigned char *)pixels : bytes, 1, 1,
                mode ? SIXEL_PIXELFORMAT_RGBFLOAT32 : SIXEL_PIXELFORMAT_RGB888,
                NULL, -1))) {
        goto end;
    }
    /* The TAP command substitution owns stdout's pipe. Use the actual
     * fd/hex transport, including outer definitions and invocations. Its
     * literal expected stream contains only the following full conversion:
     * any bytes from this rejected conversion fail that wire assertion. */
    encoder->sixdelta_enabled = 1;
    status = sixel_encoder_encode_frame(encoder, frame, NULL);
    if (status != SIXEL_BAD_ARGUMENT ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'Z', "full")) ||
            SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame, NULL))) {
        goto end;
    }
    dither = (sixel_dither_t *)encoder->diagnostic_dither;
    ok = dither != NULL && dither->use_rgb101_palette == (mode != 0) &&
        !encoder->accumulation_valid && pixels[0] == 1.f && bytes[0] == 255;
end:
    if (!ok) {
        fprintf(stderr, "macro delta recovery mode=%d: %s\n", mode,
            sixel_helper_get_additional_message());
    }
    sixel_frame_unref(frame);
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
