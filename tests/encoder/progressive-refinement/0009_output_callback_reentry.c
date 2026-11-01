/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"

typedef struct reentry_probe {
    sixel_encoder_t *encoder;
    sixel_encoder_t *other;
    int calls;
    int valid;
} reentry_probe_t;

static int
reentry_writer(char *data, int size, void *context)
{
    reentry_probe_t *probe;

    (void)data;
    (void)size;
    probe = context;
    ++probe->calls;
    probe->valid = probe->valid &&
        sixel_encoder_set_6delta_threshold_limit(probe->encoder, 3)
            == SIXEL_RUNTIME_ERROR &&
        sixel_encoder_set_6delta_threshold_limit(probe->other, 17)
            == SIXEL_OK;
    return 0;
}

int
test_progressive_0009(int argc, char **argv)
{
    reentry_probe_t probe;
    sixel_frame_t *frame;
    sixel_output_t *output;
    unsigned char *pixels;
    int ok;

    (void)argc;
    (void)argv;
    memset(&probe, 0, sizeof(probe));
    frame = NULL;
    output = NULL;
    pixels = NULL;
    ok = 0;
    probe.valid = 1;
    if (sixel_encoder_new(&probe.encoder, NULL) != SIXEL_OK ||
            sixel_encoder_new(&probe.other, NULL) != SIXEL_OK ||
            sixel_frame_new(&frame, NULL) != SIXEL_OK ||
            sixel_output_new(&output, reentry_writer, &probe, NULL)
                != SIXEL_OK) {
        goto end;
    }
    pixels = calloc(3, 1);
    if (pixels == NULL || sixel_frame_init(frame, pixels, 1, 1,
            SIXEL_PIXELFORMAT_RGB888, NULL, 0) != SIXEL_OK) {
        goto end;
    }
    pixels = NULL;
    ok = sixel_encoder_encode_frame(probe.encoder, frame, output)
        == SIXEL_OK;
    ok = ok && probe.calls > 0 && probe.valid &&
        probe.encoder->sixdelta_threshold_limit == 255 &&
        probe.other->sixdelta_threshold_limit == 17;
end:
    free(pixels);
    sixel_output_unref(output);
    sixel_frame_unref(frame);
    sixel_encoder_unref(probe.encoder);
    sixel_encoder_unref(probe.other);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
