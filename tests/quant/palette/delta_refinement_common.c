/* SPDX-License-Identifier: MIT */
/* A positive threshold may hide black behind emitted gray 5 (2% RGB101).
 * Re-evaluate at zero without losing history or changing the configured max. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"
#include "src/dither.h"

static int
refinement_write(char *data, int size, void *priv)
{
    (void)data;
    (void)priv;
    return size;
}

int
test_delta_refinement(int mode)
{
    static struct { int flag; char const *value; } const options[] = {
        {'=', "1"}, {'~', "none"}, {'d', "none"}, {'0', "hard"},
        {'_', "none"}, {'W', "gamma"}, {'U', "gamma"}, {'t', "rgb"},
        {'Q', "heckbert"}, {'a', "off"}, {'F', "none"}, {'7', NULL},
        {'Z', "delta:T8:Eskip"}, {'G', "off"}, {'.', "8bit"}
    };
    unsigned char source[72];
    unsigned char history[72];
    unsigned char *mask;
    sixel_frame_transparency_t transparency;
    sixel_frame_interface_t *iface;
    float floats[72];
    sixel_encoder_t *encoder;
    sixel_encoder_t *other;
    sixel_frame_t *frame;
    sixel_output_t *output;
    sixel_dither_t *dither;
    int i;
    int ok;

    encoder = other = NULL;
    frame = NULL;
    output = NULL;
    ok = 0;
    memset(source, 0, sizeof(source));
    memset(floats, 0, sizeof(floats));
    memset(history, mode == 3 ? 0 : 5, sizeof(history));
    mask = NULL;
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_encoder_new(&other, NULL)) ||
            SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, refinement_write,
                NULL, NULL))) {
        goto end;
    }
    for (i = 0; i < (int)(sizeof(options) / sizeof(options[0])); ++i) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, options[i].flag,
                options[i].value))) {
        goto end;
        }
    }
    if (mode == 1 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
            '.', "float32"))) {
        goto end;
    }
    if (mode == 2 && (SIXEL_FAILED(sixel_encoder_setopt(encoder,
            'G', "force")) || SIXEL_FAILED(sixel_encoder_setopt(encoder,
            'd', "bluenoise:strength=0.5")))) {
        goto end;
    }
    if (mode == 4) {
        source[0] = source[1] = source[2] = 255;
    }
    if (SIXEL_FAILED(sixel_frame_init_borrowed(frame,
            mode == 1 ? (unsigned char *)floats : source, 2, 12,
            mode == 1 ? SIXEL_PIXELFORMAT_RGBFLOAT32 : SIXEL_PIXELFORMAT_RGB888,
            NULL, -1)) ||
            SIXEL_FAILED(sixel_encoder_set_accumulation_buffer(encoder,
                history, 2, 12, SIXEL_PIXELFORMAT_RGB888))) {
        goto end;
    }
    if (mode == 5) {
        mask = malloc(24);
        if (mask == NULL) {
        goto end;
        }
        memset(mask, 255, 24);
        memset(&transparency, 0, sizeof(transparency));
        transparency.transparent = -1;
        transparency.alpha_zero_is_transparent = 1;
        transparency.transparent_mask = mask;
        transparency.transparent_mask_size = 24;
        iface = (sixel_frame_interface_t *)frame;
        if (SIXEL_FAILED(iface->vtbl->set_transparency(iface,
                &transparency))) {
        goto end;
        }
        mask = NULL;
    }
    if (SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame, output))) {
        goto end;
    }
    if (mode >= 3) {
        ok = !sixel_encoder_needs_6delta_refinement(encoder);
        if (mode == 4) {
            ok = ok && encoder->accumulation_pixels[0] == 255 &&
                encoder->accumulation_pixels[3] == 5;
        }
        goto end;
    }
    dither = (sixel_dither_t *)encoder->diagnostic_dither;
    if (!sixel_encoder_needs_6delta_refinement(encoder) ||
            memcmp(encoder->accumulation_pixels, history, sizeof(history)) ||
            dither == NULL || (mode == 1 && !dither->use_rgb101_palette) ||
            sixel_encoder_needs_6delta_refinement(other)) {
        goto end;
    }
    if (SIXEL_FAILED(sixel_encoder_set_6delta_threshold_limit(encoder, 0)) ||
            sixel_encoder_set_6delta_threshold_limit(encoder, 256) !=
                SIXEL_BAD_ARGUMENT || encoder->sixdelta_threshold_limit != 0 ||
            other->sixdelta_threshold_limit != 255 ||
            SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame, output)) ||
            sixel_encoder_needs_6delta_refinement(encoder)) {
        goto end;
    }
    for (i = 0; i < 72; ++i) {
        if (encoder->accumulation_pixels[i] != 0) {
            goto end;
        }
    }
    /* The ordinary maximum is restored without discarding the black history.
     * Exact idle frames must not request another timer-driven refinement. */
    ok = encoder->sixdelta_threshold == 8 &&
        SIXEL_SUCCEEDED(sixel_encoder_set_6delta_threshold_limit(encoder,
            255)) &&
        SIXEL_SUCCEEDED(sixel_encoder_encode_frame(encoder, frame, output)) &&
        !sixel_encoder_needs_6delta_refinement(encoder);
end:
    if (!ok) {
        fprintf(stderr, "delta refinement mode %d: %s\n", mode,
            sixel_helper_get_additional_message());
    }
    free(mask);
    sixel_output_unref(output);
    sixel_frame_unref(frame);
    sixel_encoder_unref(encoder);
    sixel_encoder_unref(other);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
