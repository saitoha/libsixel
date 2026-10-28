/* SPDX-License-Identifier: MIT
 * Interleaved encoder builds must not leak strength across worker requests.
 * Compare complete SIXEL streams, including a reset to the default setting.
 */
#include <stdlib.h>
#include <string.h>
#include <sixel.h>

struct strength_wire {
    char data[65536];
    size_t size;
};

static int
strength_capture(char *data, int size, void *context)
{
    struct strength_wire *wire;

    wire = context;
    if (size < 0 || (size_t)size > sizeof(wire->data) - wire->size) {
        return -1;
    }
    memcpy(wire->data + wire->size, data, (size_t)size);
    wire->size += (size_t)size;
    return size;
}

int
test_0015_range_encoder_isolation(int argc, char **argv)
{
    char const *models[4] = {
        "heckbert", "kmeans:seed=1", "medoids:seed=1", "center:seed=1"
    };
    char const *settings[5] = {
        "auto:strength=0", "auto:strength=100", "auto:strength=0",
        "auto:strength=100", "auto"
    };
    sixel_encoder_t *encoder[2];
    sixel_frame_t *frame;
    sixel_output_t *output;
    struct strength_wire wire[5];
    unsigned char pixels[64 * 64 * 3];
    unsigned int model;
    unsigned int run;
    unsigned int i;
    unsigned int value;
    int result;

    (void)argc;
    (void)argv;
    encoder[0] = encoder[1] = NULL;
    frame = NULL;
    output = NULL;
    result = EXIT_FAILURE;
    for (i = 0u; i < 64u * 64u; ++i) {
        value = i % 64u >= 61u ? 255u : (i + i / 64u) % 24u;
        pixels[i * 3u] = pixels[i * 3u + 1u] = pixels[i * 3u + 2u] =
            (unsigned char)value;
    }
    for (model = 0u; model < 4u; ++model) {
        for (i = 0u; i < 2u; ++i) {
            if (SIXEL_FAILED(sixel_encoder_new(&encoder[i], NULL)) ||
                    SIXEL_FAILED(sixel_encoder_setopt(encoder[i], 'Q',
                        models[model])) ||
                    SIXEL_FAILED(sixel_encoder_setopt(encoder[i], '=',
                        "greedy:B4"))) {
                goto end;
            }
        }
        for (run = 0u; run < 5u; ++run) {
            wire[run].size = 0u;
            if (SIXEL_FAILED(sixel_encoder_setopt(encoder[run % 2u], '5',
                    settings[run])) ||
                    SIXEL_FAILED(sixel_output_new(&output,
                        strength_capture, &wire[run], NULL)) ||
                    SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
                    SIXEL_FAILED(sixel_frame_init_borrowed(frame, pixels,
                        64, 64, SIXEL_PIXELFORMAT_RGB888, NULL, -1)) ||
                    SIXEL_FAILED(sixel_encoder_encode_frame(
                        encoder[run % 2u], frame, output))) {
                goto end;
            }
            sixel_frame_unref(frame);
            frame = NULL;
            sixel_output_unref(output);
            output = NULL;
        }
        if (wire[0].size != wire[2].size ||
                memcmp(wire[0].data, wire[2].data, wire[0].size) != 0 ||
                wire[1].size != wire[3].size ||
                memcmp(wire[1].data, wire[3].data, wire[1].size) != 0 ||
                wire[1].size != wire[4].size ||
                memcmp(wire[1].data, wire[4].data, wire[1].size) != 0 ||
                (wire[0].size == wire[1].size &&
                 memcmp(wire[0].data, wire[1].data, wire[0].size) == 0)) {
            goto end;
        }
        sixel_encoder_unref(encoder[0]);
        sixel_encoder_unref(encoder[1]);
        encoder[0] = encoder[1] = NULL;
    }
    result = EXIT_SUCCESS;
end:
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder[0]);
    sixel_encoder_unref(encoder[1]);
    return result;
}
