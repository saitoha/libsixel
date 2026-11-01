/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "playback_test_common.h"

enum { PLAYBACK_TEST_WIDTH = 96, PLAYBACK_TEST_HEIGHT = 48 };

int
playback_test_write(char *data, int size, void *priv)
{
    playback_test_buffer_t *buffer;
    unsigned char *resized;
    size_t required;

    buffer = (playback_test_buffer_t *)priv;
    required = buffer->size + (size_t)size;
    if (required > buffer->capacity) {
        resized = (unsigned char *)realloc(buffer->bytes, required * 2u);
        if (resized == NULL) {
            return -1;
        }
        buffer->bytes = resized;
        buffer->capacity = required * 2u;
    }
    memcpy(buffer->bytes + buffer->size, data, (size_t)size);
    buffer->size = required;
    return size;
}

int
playback_test_write_fail(char *data, int size, void *priv)
{
    (void)data;
    (void)size;
    (void)priv;
    return -1;
}

static sixel_frame_t *
playback_frame_sized(int index, int width, int height)
{
    sixel_frame_t *frame;
    unsigned char *pixels;
    unsigned char *pixel;
    int x;
    int y;
    int inside;
    int sx;
    int sy;

    frame = NULL;
    pixels = (unsigned char *)malloc((size_t)width * height * 3u);
    if (pixels == NULL) {
        return NULL;
    }
    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            pixel = pixels + ((size_t)y * width + x) * 3u;
            /* Scale coordinates, retaining the bar and gradient even in
             * the small scheduling fixture. Existing large callers keep
             * their original pixel values. */
            sx = x * PLAYBACK_TEST_WIDTH / width;
            sy = y * PLAYBACK_TEST_HEIGHT / height;
            inside = (sx - index * 7 + 120) % 120 < 24 && sy > 12 && sy < 36;
            pixel[0] = inside ? 230 : (unsigned char)(sx * 255
                                                      / PLAYBACK_TEST_WIDTH);
            pixel[1] = inside ? 40 : (unsigned char)(sy * 255
                                                     / PLAYBACK_TEST_HEIGHT);
            pixel[2] = (unsigned char)((sx + sy + index * 3) & 255);
        }
    }
    if (SIXEL_FAILED(sixel_frame_new(&frame, NULL))) {
        free(pixels);
        return NULL;
    }
    if (SIXEL_FAILED(sixel_frame_init(frame, pixels, width, height,
                                      SIXEL_PIXELFORMAT_RGB888,
                                      NULL, -1))) {
        sixel_frame_unref(frame);
        return NULL;
    }
    return frame;
}

sixel_frame_t *
playback_test_frame(int index)
{
    return playback_frame_sized(index, PLAYBACK_TEST_WIDTH,
        PLAYBACK_TEST_HEIGHT);
}

sixel_frame_t *
playback_test_small_frame(int index)
{
    return playback_frame_sized(index, 16, 16);
}

sixel_encoder_t *
playback_test_encoder(int delta, char const *threads)
{
    char policy[64];
    sixel_encoder_t *encoder;
    SIXELSTATUS status;

    encoder = NULL;
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL))) {
        return NULL;
    }
    (void)snprintf(policy, sizeof(policy), "greedy:B%s", threads);
    status = sixel_encoder_setopt(encoder,
                                  SIXEL_OPTFLAG_THREADING_POLICY, policy);
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_DIFFUSION, "fs");
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_COLORS, "16");
    }
    if (SIXEL_SUCCEEDED(status) && delta) {
        status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_UPDATE_POLICY,
                                      "delta");
    }
    if (SIXEL_FAILED(status)) {
        sixel_encoder_unref(encoder);
        return NULL;
    }
    return encoder;
}
