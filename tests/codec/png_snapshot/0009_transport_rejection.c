/* SPDX-License-Identifier: MIT */
/* A PNG snapshot cannot interpret receiver-side transport/composition. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdlib.h>
#include <sixel.h>
#include "src/encoder.h"

static int
png_transport_write(char *data, int size, void *context)
{
    int *written;

    (void)data;
    written = context;
    *written += size;
    return size;
}

int
test_png_snapshot_0009(int argc, char **argv)
{
    static int const flags[] = {'n', '@', 'O'};
    static char const *const values[] = {"7", "2:1", NULL};
    float pixels[3] = {.5f, .2f, .8f};
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    SIXELSTATUS status;
    int written;
    int i;
    int ok;

    (void)argc;
    (void)argv;
    encoder = NULL;
    output = NULL;
    frame = NULL;
    written = 0;
    ok = 0;
    if (SIXEL_FAILED(sixel_output_new(&output, png_transport_write,
            &written, NULL)) || SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
            SIXEL_FAILED(sixel_frame_init_borrowed(frame, pixels, 1, 1,
                SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, -1))) {
        goto end;
    }
    for (i = 0; i < 3; ++i) {
        if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
                SIXEL_FAILED(sixel_encoder_setopt(encoder, 'o', "png:-")) ||
                SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U',
                    "display-p3")) ||
                SIXEL_FAILED(sixel_encoder_setopt(encoder,
                    flags[i], values[i]))) {
            goto end;
        }
        status = sixel_encoder_encode_frame(encoder, frame, output);
        if (status != SIXEL_BAD_ARGUMENT || written != 0) {
            goto end;
        }
        sixel_encoder_unref(encoder);
        encoder = NULL;
    }
    ok = 1;
end:
    sixel_encoder_unref(encoder);
    sixel_output_unref(output);
    sixel_frame_unref(frame);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
