/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"
#include "src/frame.h"

int
test_progressive_0001(int argc, char **argv)
{
    sixel_encoder_t *encoder;
    sixel_frame_t *frame;
    sixel_frame_t *prepared;
    unsigned char *pixels;
    unsigned char *domain;
    float const *values;
    int ok;
    SIXELSTATUS status;

    (void)argc;
    (void)argv;
    encoder = NULL;
    frame = NULL;
    prepared = NULL;
    domain = NULL;
    pixels = NULL;
    ok = 0;
    if (sixel_encoder_new(&encoder, NULL) != SIXEL_OK ||
            sixel_frame_new(&frame, NULL) != SIXEL_OK) {
        goto end;
    }
    pixels = malloc(9);
    if (pixels == NULL) {
        goto end;
    }
    memcpy(pixels, "\0\0\0\xff\0\0\xff\xff\xff", 9);
    status = sixel_frame_init(frame, pixels, 3, 1,
                              SIXEL_PIXELFORMAT_RGB888, NULL, 0);
    if (status != SIXEL_OK) {
        goto end;
    }
    pixels = NULL;
    if (sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_CROP,
                             "2x1+1+0") != SIXEL_OK) {
        goto end;
    }
    status = sixel_encoder_prepare_repair(encoder, frame, &prepared, &domain);
    if (status != SIXEL_OK) {
        goto end;
    }
    values = (float const *)sixel_frame_get_pixels(prepared);
    ok = sixel_frame_get_width(frame) == 3 &&
        sixel_frame_get_width(prepared) == 2 &&
        sixel_frame_get_height(prepared) == 1 &&
        sixel_frame_get_pixelformat(prepared) == SIXEL_PIXELFORMAT_RGBFLOAT32 &&
        domain[0] == 1 && domain[1] == 1 &&
        values[0] == 1.0f && values[1] == 0.0f && values[2] == 0.0f &&
        values[3] == 1.0f && values[4] == 1.0f && values[5] == 1.0f;
    /* Mutating caller storage after bind preparation cannot change target. */
    memset(sixel_frame_get_pixels(frame), 0, 9);
    ok = ok && values[0] == 1.0f && values[3] == 1.0f;
end:
    free(pixels);
    if (encoder != NULL) {
        sixel_allocator_free(encoder->allocator, domain);
    }
    sixel_frame_unref(prepared);
    sixel_frame_unref(frame);
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
