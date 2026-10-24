/* SPDX-License-Identifier: MIT */
/* Distinct native colors must not trigger the few-color diffusion bypass. */
#include <stdlib.h>
#include <sixel.h>
#include "src/encoder.h"
#include "src/dither.h"

static int
heckbert_discard(char *data, int size, void *context)
{
    (void)data;
    (void)context;
    return size;
}

int
test_heckbert_native_0032(int argc, char **argv)
{
    static struct { int flag; char const *value; } const options[] = {
        { '=', "balanced:B1" }, { 'p', "2" }, { '.', "float32" },
        { 'Q', "heckbert" }, { 'F', "none" }, { 'a', "off" },
        { '_', "none" }, { '~', "none" }, { 'd', "fs" },
        { 'X', "gamma" }, { 'W', "gamma" }, { 'U', "gamma" },
        { 't', "rgb" }, { '4', "full-frame" }, { '5', "none" }
    };
    float pixels[9] = {
        .5001f, .5001f, .5001f, .5002f, .5002f, .5002f,
        .5003f, .5003f, .5003f
    };
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    sixel_dither_t *dither;
    size_t i;
    int ok;

    (void)argc;
    (void)argv;
    encoder = NULL;
    output = NULL;
    frame = NULL;
    ok = 0;
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, heckbert_discard,
                NULL, NULL))) {
        goto end;
    }
    for (i = 0U; i < sizeof(options) / sizeof(options[0]); ++i) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, options[i].flag,
                options[i].value))) {
            goto end;
        }
    }
    if (SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
            SIXEL_FAILED(sixel_frame_init_borrowed(frame, pixels, 3, 1,
                SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, -1)) ||
            SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame,
                output))) {
        goto end;
    }
    dither = (sixel_dither_t *)encoder->diagnostic_dither;
    ok = dither != NULL && dither->use_rgb101_palette != 0 &&
        dither->origcolors == 3 && dither->ncolors == 2 &&
        dither->method_for_diffuse == SIXEL_DIFFUSE_FS;
end:
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
