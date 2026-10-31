/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/encoder.h"
#include "playback_test_common.h"
#include "reserved_palette_test_common.h"

static unsigned char const fixed_rgb[] = {
    255, 32, 32, 32, 255, 32, 255, 255, 255
};

typedef struct reserved_state {
    playback_test_buffer_t buffer;
    int completed;
    int failed;
    int legacy;
    int cancel;
} reserved_state_t;

/* Deliberately different geometries prove the source is not cropped to the
 * composite. Blue occurs only in the composite and is never reserved. */
static sixel_frame_t *
reserved_frame(int composite, int floating)
{
    sixel_frame_t *frame;
    unsigned char *pixels;
    unsigned char *p;
    int width, height, x, y, color;
    SIXELSTATUS status;

    frame = NULL;
    width = composite ? 32 : 17;
    height = composite ? 24 : 13;
    pixels = malloc((size_t)width * height * 3U);
    if (pixels == NULL) {
        return NULL;
    }
    memset(pixels, 128, (size_t)width * height * 3U);
    if (composite) {
        for (y = 0; y < height; ++y) {
            for (x = 0; x < width; ++x) {
                p = pixels + (y * width + x) * 3;
                color = x / 8;
                if (color < 3) {
                    memcpy(p, fixed_rgb + color * 3, 3U);
                } else {
                    p[0] = p[1] = 0;
                    p[2] = 255;
                }
            }
        }
    }
    status = sixel_frame_new(&frame, NULL);
    if (SIXEL_FAILED(status)) {
        free(pixels);
        return NULL;
    }
    status = sixel_frame_init(frame, pixels, width, height,
        SIXEL_PIXELFORMAT_RGB888, NULL, -1);
    if (SIXEL_SUCCEEDED(status) && floating) {
        status = sixel_frame_set_pixelformat(frame,
            SIXEL_PIXELFORMAT_LINEARRGBFLOAT32);
    }
    if (SIXEL_FAILED(status)) {
        sixel_frame_unref(frame);
        return NULL;
    }
    return frame;
}

/* Inspect actual wire colors and decoded pixels, not just configuration.
 * A missing reserved color or sampling the composite fails independently. */
static void
reserved_done(sixel_playback_result_t const *result, void *context)
{
    reserved_state_t *state;
    unsigned char *pixels, *palette;
    int width, height, count, i, channel, actual, expected;
    SIXELSTATUS status;

    state = context;
    ++state->completed;
    pixels = palette = NULL;
    if (state->cancel && result->status == SIXEL_INTERRUPTED) {
        state->buffer.size = 0;
        return;
    }
    status = result->status;
    if (status == SIXEL_OK) {
        status = sixel_decode_raw(state->buffer.bytes,
            (int)state->buffer.size, &pixels, &width, &height,
            &palette, &count, NULL);
    }
    if (status != SIXEL_OK) {
        fprintf(stderr, "reserved frame failed: %x %s\n", status,
            sixel_helper_get_additional_message());
        state->failed = 1;
        goto end;
    }
    if (width != 32 || height != 24 || count > 8 || count < 4) {
        fprintf(stderr, "reserved geometry/count: %d %d %d\n",
            width, height, count);
        state->failed = 1;
        goto end;
    }
    for (i = 0; i < 3; ++i) {
        for (channel = 0; channel < 3; ++channel) {
            expected = fixed_rgb[i * 3 + channel];
            actual = palette[i * 3 + channel];
            if (abs(actual - expected) > 2 ||
                    (!state->legacy &&
                     pixels[12 * width + i * 8 + 4] != i)) {
                fprintf(stderr, "reserved color %d channel %d: %d vs %d; "
                    "pixel=%d\n", i, channel, actual, expected,
                    pixels[12 * width + i * 8 + 4]);
                state->failed = 1;
            }
        }
    }
    if (!state->legacy) {
        /* The only generated color must be the independent gray source.
         * Ignore any unused register padding and the optional alpha key. */
        if (abs((int)palette[9] - 128) > 3 ||
                abs((int)palette[10] - 128) > 3 ||
                abs((int)palette[11] - 128) > 3) {
            fprintf(stderr, "composite leaked into generated palette: "
                "%u %u %u\n", palette[9], palette[10], palette[11]);
            state->failed = 1;
        }
    }
end:
    free(pixels);
    free(palette);
    state->buffer.size = 0;
}

static int
reserved_validation(void)
{
    sixel_encoder_t *encoder;
    unsigned char copy[sizeof(fixed_rgb)];
    unsigned char duplicate[6] = {1, 2, 3, 1, 2, 3};
    int failed;

    encoder = NULL;
    failed = 1;
    memcpy(copy, fixed_rgb, sizeof(copy));
    if (sixel_encoder_new(&encoder, NULL) != SIXEL_OK ||
            sixel_encoder_setopt(encoder, 'p', "8") != SIXEL_OK ||
            sixel_encoder_set_reserved_palette(encoder, copy, 3) != SIXEL_OK) {
        goto end;
    }
    memset(copy, 0, sizeof(copy));
    if (memcmp(encoder->reserved_palette, fixed_rgb, sizeof(copy)) != 0 ||
            sixel_encoder_set_reserved_palette(encoder, NULL, 1)
                != SIXEL_BAD_ARGUMENT ||
            sixel_encoder_set_reserved_palette(encoder, fixed_rgb, 256)
                != SIXEL_BAD_ARGUMENT ||
            sixel_encoder_set_reserved_palette(encoder, duplicate, 2)
                != SIXEL_BAD_ARGUMENT ||
            sixel_encoder_setopt(encoder, 'p', "3") != SIXEL_BAD_ARGUMENT ||
            sixel_encoder_setopt(encoder, 'b', "xterm256")
                != SIXEL_BAD_ARGUMENT ||
            encoder->reqcolors != 8 || encoder->reserved_palette_count != 3 ||
            memcmp(encoder->reserved_palette, fixed_rgb, sizeof(copy)) != 0) {
        goto end;
    }
    if (sixel_encoder_setopt(encoder, 'p', "4") != SIXEL_OK ||
            sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_UPDATE_POLICY,
                "delta") != SIXEL_BAD_ARGUMENT ||
            sixel_encoder_set_reserved_palette(encoder, NULL, 0)
                != SIXEL_OK || encoder->reserved_palette_count != 0 ||
            sixel_encoder_setopt(encoder, 'p', "3") != SIXEL_OK) {
        goto end;
    }
    sixel_encoder_unref(encoder);
    encoder = NULL;
    if (sixel_encoder_new(&encoder, NULL) != SIXEL_OK ||
            sixel_encoder_setopt(encoder, 'b', "xterm256") != SIXEL_OK ||
            sixel_encoder_set_reserved_palette(encoder, fixed_rgb, 3)
                != SIXEL_BAD_ARGUMENT) {
        goto end;
    }
    failed = 0;
end:
    if (failed) {
        fprintf(stderr, "reserved validation: %s\n",
            sixel_helper_get_additional_message());
    }
    sixel_encoder_unref(encoder);
    return failed;
}

int
reserved_palette_test(int scenario)
{
    reserved_state_t state;
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_playback_t *playback;
    sixel_playback_config_t config;
    sixel_playback_options_t options;
    sixel_frame_t *frame, *source;
    sixel_frame_t *retained[4];
    float original[17 * 13 * 3];
    SIXELSTATUS status;
    size_t old_size;
    int i, failed;

    if (scenario == 4) {
        return reserved_validation();
    }
    memset(&state, 0, sizeof(state));
    state.legacy = scenario == 5;
    state.cancel = scenario == 7;
    encoder = NULL;
    output = NULL;
    playback = NULL;
    frame = source = NULL;
    memset(retained, 0, sizeof(retained));
    failed = 1;
    status = sixel_encoder_new(&encoder, NULL);
    if (status != SIXEL_OK ||
            sixel_encoder_setopt(encoder, 'p', "8") != SIXEL_OK ||
            sixel_encoder_setopt(encoder, 'd', "none") != SIXEL_OK ||
            sixel_encoder_setopt(encoder, 'W', "auto") != SIXEL_OK ||
            sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_GPU_POLICY, "off")
                != SIXEL_OK ||
            sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_THREADING_POLICY,
                scenario == 0 ? "greedy:B1" : "greedy:B4") != SIXEL_OK ||
            sixel_encoder_set_reserved_palette(encoder, fixed_rgb, 3)
                != SIXEL_OK) {
        goto end;
    }
    if (scenario == 6 &&
            (sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_SAMPLING_POLICY,
                "full-frame") != SIXEL_OK ||
             sixel_encoder_setopt(encoder, 'X', "oklab") != SIXEL_OK)) {
        goto end;
    }
    status = sixel_output_new(&output, playback_test_write, &state.buffer,
        NULL);
    if (status != SIXEL_OK) {
        goto end;
    }
    (void)sixel_playback_config_init(&config, sizeof(config));
    config.capacity = 4;
    config.palette_mode = scenario == 2 ? SIXEL_PLAYBACK_PALETTE_LATEST :
        scenario == 3 ? SIXEL_PLAYBACK_PALETTE_AUTO :
                        SIXEL_PLAYBACK_PALETTE_PER_FRAME;
    config.done = reserved_done;
    config.done_context = &state;
    if (sixel_playback_new(&playback, encoder, output, &config, NULL)
            != SIXEL_OK) {
        goto end;
    }
    for (i = 0; i < 4; ++i) {
        frame = reserved_frame(1, scenario != 0);
        source = reserved_frame(0, scenario != 0);
        if (frame == NULL || source == NULL) {
            goto end;
        }
        if (scenario == 6) {
            /* Full-source construction may convert its private sample to
             * Oklab, but must leave the caller's linear source untouched. */
            retained[i] = source;
            sixel_frame_ref(source);
            memcpy(original, sixel_frame_get_pixels(source), sizeof(original));
        }
        (void)sixel_playback_options_init(&options, sizeof(options));
        if (state.legacy) {
            old_size = offsetof(sixel_playback_options_t, palette_source);
            /* An old binary has no trailing field. Initialization must not
             * touch that memory, and submission must not read the poison. */
            options.palette_source = (sixel_frame_t *)(void *)&state;
            status = sixel_playback_options_init(&options, old_size);
            if (status != SIXEL_OK || options.struct_size != old_size ||
                    options.palette_source != (sixel_frame_t *)(void *)&state) {
                goto end;
            }
        } else {
            options.palette_source = source;
        }
        status = sixel_playback_submit(playback, frame, SIXEL_PLAYBACK_WAIT,
            -1, &options);
        /* Playback must retain both objects after the caller lets go. */
        sixel_frame_unref(frame);
        sixel_frame_unref(source);
        frame = source = NULL;
        if (status != SIXEL_OK) {
            goto end;
        }
    }
    if (state.cancel) {
        (void)sixel_playback_cancel(playback);
    }
    if (sixel_playback_drain(playback, -1) == SIXEL_OK &&
            state.completed == 4 && !state.failed) {
        failed = 0;
    }
    for (i = 0; i < 4 && scenario == 6; ++i) {
        if (sixel_frame_get_pixelformat(retained[i]) !=
                SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 ||
                memcmp(original, sixel_frame_get_pixels(retained[i]),
                    sizeof(original)) != 0) {
            fprintf(stderr, "full sampling mutated the source\n");
            failed = 1;
        }
    }
end:
    if (failed) {
        fprintf(stderr, "reserved scenario %d: %s, completed=%d failed=%d\n",
            scenario, sixel_helper_get_additional_message(),
            state.completed, state.failed);
    }
    sixel_playback_unref(playback);
    for (i = 0; i < 4; ++i) {
        sixel_frame_unref(retained[i]);
    }
    sixel_frame_unref(frame);
    sixel_frame_unref(source);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    free(state.buffer.bytes);
    return failed;
}
