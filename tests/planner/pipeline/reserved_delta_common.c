/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/encoder.h"
#include "src/sixel_decode_pixels.h"
#include "playback_test_common.h"

#define UI_WIDTH 77
#define UI_HEIGHT 53
#define UI_PIXELS (UI_WIDTH * UI_HEIGHT)
#define UI_CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "reserved delta gpu=%d latest=%d T=%d step=%d " \
        "line=%d: %s (%s)\n", gpu, latest, threshold, step, __LINE__, \
        #x, sixel_helper_get_additional_message()); goto end; } } while (0)

/* A gray video source stays fixed while an overlay appears and disappears.
 * The gray UI is within T48 of the background, so only deadline refresh
 * guarantees its final appearance/removal. Red/white exercise reserved keys. */
static int
reserved_delta_render(int gpu, int latest, int threshold, int record,
    unsigned char reference[3][UI_PIXELS * 3])
{
    static unsigned char const reserved[] = {
        128, 128, 128, 255, 0, 0, 255, 255, 255
    };
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_playback_t *playback;
    sixel_playback_config_t config;
    sixel_playback_options_t options;
    sixel_frame_t *frame, *source;
    playback_test_buffer_t stream;
    unsigned char pixels[UI_PIXELS * 3], background[17 * 13 * 3];
    unsigned char canvas[UI_PIXELS * 3];
    unsigned char *decoded, *paint, *palette;
    unsigned int flags;
    int step, i, c, slot, width, height, colors, ok;
    int hidden_keeps;

    encoder = NULL;
    output = NULL;
    playback = NULL;
    frame = source = NULL;
    decoded = paint = palette = NULL;
    memset(&stream, 0, sizeof(stream));
    memset(canvas, 0, sizeof(canvas));
    memset(background, 102, sizeof(background));
    ok = hidden_keeps = 0;
    step = 0;
    UI_CHECK(sixel_encoder_new(&encoder, NULL) == SIXEL_OK);
    UI_CHECK(sixel_encoder_setopt(encoder, 'p', "8") == SIXEL_OK);
    UI_CHECK(sixel_encoder_setopt(encoder, 'd', "bluenoise") == SIXEL_OK);
    UI_CHECK(sixel_encoder_setopt(encoder, '~', "none") == SIXEL_OK);
    UI_CHECK(sixel_encoder_setopt(encoder, '.', "8bit") == SIXEL_OK);
    UI_CHECK(sixel_encoder_setopt(encoder, 'X', "gamma") == SIXEL_OK);
    UI_CHECK(sixel_encoder_setopt(encoder, 'W', "gamma") == SIXEL_OK);
    UI_CHECK(sixel_encoder_setopt(encoder, 'G', gpu ? "force" : "off")
        == SIXEL_OK);
    UI_CHECK(sixel_encoder_setopt(encoder, '=', "greedy:B4:prepare=serial")
        == SIXEL_OK);
    UI_CHECK(sixel_encoder_set_reserved_palette(encoder, reserved, 3)
        == SIXEL_OK);
    if (!record) {
        UI_CHECK(sixel_encoder_setopt(encoder, 'Z', threshold
            ? "delta:T48:refresh_ms=80" : "delta:T0:refresh_ms=80")
            == SIXEL_OK);
    }
    UI_CHECK(sixel_output_new(&output, playback_test_write, &stream, NULL)
        == SIXEL_OK);
    UI_CHECK(sixel_playback_config_init(&config, sizeof(config)) == SIXEL_OK);
    config.palette_mode = latest ? SIXEL_PLAYBACK_PALETTE_LATEST
                                : SIXEL_PLAYBACK_PALETTE_PER_FRAME;
    UI_CHECK(sixel_playback_new(&playback, encoder, output, &config, NULL)
        == SIXEL_OK);
    UI_CHECK(sixel_frame_new(&frame, NULL) == SIXEL_OK);
    UI_CHECK(sixel_frame_new(&source, NULL) == SIXEL_OK);
    UI_CHECK(sixel_frame_init_borrowed(source, background, 17, 13,
        SIXEL_PIXELFORMAT_RGB888, NULL, 0) == SIXEL_OK);
    UI_CHECK(sixel_playback_options_init(&options, sizeof(options))
        == SIXEL_OK);
    options.palette_source = source;
    options.flags = SIXEL_PLAYBACK_OPTION_REFRESH_TIME;
    for (step = 0; step <= 10; ++step) {
        memset(pixels, 102, sizeof(pixels));
        if (step >= 1 && step <= 5) {
            for (i = 0; i < UI_PIXELS; ++i) {
                if (i % UI_WIDTH < 48) {
                    slot = (i % UI_WIDTH) / 16;
                    memcpy(pixels + i * 3, reserved + slot * 3, 3);
                }
            }
        }
        UI_CHECK(sixel_frame_init_borrowed(frame, pixels, UI_WIDTH,
            UI_HEIGHT, SIXEL_PIXELFORMAT_RGB888, NULL, 0) == SIXEL_OK);
        stream.size = 0;
        options.refresh_time_us = (uint64_t)step * 16000U;
        UI_CHECK(sixel_playback_submit(playback, frame, SIXEL_PLAYBACK_WAIT,
            2000, &options) == SIXEL_OK);
        UI_CHECK(sixel_playback_drain(playback, 5000) == SIXEL_OK);
        if (stream.size != 0) {
            UI_CHECK(sixel_decode_raw_with_options_mask(stream.bytes,
                (int)stream.size, 0, &decoded, &paint, &width, &height,
                &palette, &colors, &flags, encoder->allocator) == SIXEL_OK);
            UI_CHECK(width == UI_WIDTH && height == UI_HEIGHT);
            for (i = 0; i < UI_PIXELS; ++i) {
                if (paint[i]) {
                    UI_CHECK(decoded[i] < colors);
                    memcpy(canvas + i * 3, palette + decoded[i] * 3, 3);
                } else if (step == 6 && i % UI_WIDTH < 16) {
                    ++hidden_keeps;
                }
            }
        }
        slot = step >= 1 && step <= 5 ? 1 : step > 5 ? 2 : 0;
        if (record) {
            /* Inspect source sampling and reserved register placement
             * independently of the retained delta canvas comparison. */
            UI_CHECK(colors >= 4);
            for (i = 0; i < 12; ++i) {
                c = i < 9 ? reserved[i] : 102;
                UI_CHECK(abs((int)palette[i] - c) <= 3);
            }
            memcpy(reference[slot], canvas, sizeof(canvas));
        } else {
            UI_CHECK(encoder->accumulation_valid &&
                encoder->accumulation_pixels_size == sizeof(canvas) &&
                !memcmp(canvas, encoder->accumulation_pixels,
                    sizeof(canvas)));
            /* Compare with full frames, including intentional blue-noise
             * texture. Input RGB equality would wrongly reject dithering. */
            if (!threshold || step == 0 || step == 5 || step == 10) {
                UI_CHECK(!memcmp(canvas, reference[slot], sizeof(canvas)));
            }
        }
        sixel_allocator_free(encoder->allocator, decoded);
        sixel_allocator_free(encoder->allocator, paint);
        sixel_allocator_free(encoder->allocator, palette);
        decoded = paint = palette = NULL;
    }
    UI_CHECK(record || !threshold || hidden_keeps > 0);
    ok = 1;
end:
    if (encoder != NULL) {
        sixel_allocator_free(encoder->allocator, decoded);
        sixel_allocator_free(encoder->allocator, paint);
        sixel_allocator_free(encoder->allocator, palette);
    }
    sixel_playback_unref(playback);
    sixel_frame_unref(frame);
    sixel_frame_unref(source);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    free(stream.bytes);
    return ok ? 0 : 1;
}

int
reserved_delta_test(int gpu, int latest, int threshold)
{
    unsigned char reference[3][UI_PIXELS * 3];

    if (reserved_delta_render(0, 0, 0, 1, reference) != 0) return 1;
    return reserved_delta_render(gpu, latest, threshold, 0, reference);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
