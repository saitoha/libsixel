/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"
#include "src/delta-residual.h"
#include "src/bluenoise_64x64.h"

#define REQUIRE(x) do { if (!(x)) { \
    fprintf(stderr, "refresh %d line %d: %s (%s)\n", mode, __LINE__, #x, \
        sixel_helper_get_additional_message()); goto end; } } while (0)

typedef struct refresh_output {
    unsigned char bytes[65536];
    size_t size;
    size_t painted;
    int fail;
} refresh_output_t;

static int
refresh_write(char *data, int size, void *opaque)
{
    refresh_output_t *out;

    out = opaque;
    if (out->fail || size < 0 || (size_t)size > sizeof(out->bytes) - out->size)
        return -1;
    memcpy(out->bytes + out->size, data, (size_t)size);
    out->size += (size_t)size;
    return size;
}

static void
refresh_done(sixel_playback_result_t const *result, void *opaque)
{
    refresh_output_t *out;

    out = opaque;
    out->painted = result->painted_pixels;
}

/* Model the owner's successful-publication boundary for scheduler tests. */
static void
refresh_commit(sixel_delta_residual_t *state)
{
    state->width = state->mask.width;
    state->height = state->mask.height;
    state->refresh_epoch_us = state->refresh_next_epoch_us;
    state->refresh_last_us = state->refresh_next_us;
    state->refresh_valid = state->refresh_next_valid;
}

int
test_delta_refresh(int mode)
{
    sixel_delta_residual_t state;
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_playback_t *playback;
    sixel_playback_config_t config;
    sixel_playback_options_t options;
    sixel_frame_t *frame;
    refresh_output_t out;
    unsigned char pixels[130 * 77 * 3];
    size_t const paint_counts[] = {10010U, 2497U, 2502U, 2493U, 2518U};
    uint32_t seen[128];
    uint64_t timestamp;
    size_t expected, changed, i, legacy_size;
    unsigned int tick, rank;
    int ok;
    SIXELSTATUS status;

    ok = 0;
    encoder = NULL;
    output = NULL;
    playback = NULL;
    frame = NULL;
    memset(&state, 0, sizeof(state));
    memset(&out, 0, sizeof(out));
    memset(seen, 0, sizeof(seen));
    if (mode >= 2 && mode <= 5) {
        sixel_delta_refresh_prepare(&state, 128, 128, 64, 1000000U, 0);
        REQUIRE(state.selected_pixels == 0U);
        refresh_commit(&state);
        if (mode == 2) {
            for (tick = 1U; tick <= 4U; ++tick) {
                sixel_delta_refresh_prepare(&state, 128, 128, 64,
                    1000000U + tick * 16000U, 1);
                REQUIRE(state.selected_pixels == 4096U);
                for (i = 0; i < 128U; ++i) {
                    REQUIRE((seen[i] & state.mask.cells[i]) == 0U);
                    seen[i] |= state.mask.cells[i];
                }
                refresh_commit(&state);
            }
            for (i = 0; i < 128U; ++i) REQUIRE(seen[i] == UINT32_MAX);
        } else if (mode == 3) {
            sixel_delta_refresh_prepare(&state, 128, 128, 64, 1090000U, 1);
            REQUIRE(state.selected_pixels == 16384U);
        } else if (mode == 4) {
            sixel_delta_refresh_prepare(&state, 128, 128, 64, 1016000U, 1);
            refresh_commit(&state);
            sixel_delta_refresh_prepare(&state, 128, 128, 64, 1016000U, 1);
            REQUIRE(state.selected_pixels == 0U);
        } else {
            sixel_delta_refresh_prepare(&state, 128, 128, 64, 999999U, 1);
            REQUIRE(state.selected_pixels == 16384U);
            refresh_commit(&state);
            sixel_delta_refresh_prepare(&state, 128, 128, 64, 1015999U, 1);
            REQUIRE(state.selected_pixels == 4096U);
        }
        ok = 1;
        goto end;
    }
    REQUIRE(sixel_encoder_new(&encoder, NULL) == SIXEL_OK);
    REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_DIFFUSION,
        "bluenoise") == SIXEL_OK);
    REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_LUT_POLICY,
        "none") == SIXEL_OK);
    REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_GPU_POLICY,
        mode == 7 ? "force" : "off") == SIXEL_OK);
    if (mode == 1) {
        REQUIRE(encoder->refresh_ms == 0U);
        REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_UPDATE_POLICY,
            "delta:T8:refresh_ms=64") == SIXEL_OK);
        REQUIRE(encoder->refresh_ms == 64U);
        REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_UPDATE_POLICY,
            "delta:T8:J64") == SIXEL_OK);
        REQUIRE(encoder->refresh_ms == 64U);
        REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_UPDATE_POLICY,
            "delta:T8:J10001") != SIXEL_OK);
        REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_UPDATE_POLICY,
            "delta:T8:J64:R1") != SIXEL_OK);
        REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_DIFFUSION,
            "fs") != SIXEL_OK);
        REQUIRE(encoder->refresh_ms == 64U);
        REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_UPDATE_POLICY,
            "delta") == SIXEL_OK);
        REQUIRE(encoder->refresh_ms == 0U);
        ok = 1;
        goto end;
    }
    REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_UPDATE_POLICY,
        "delta:T8:J64") == SIXEL_OK);
    REQUIRE(sixel_output_new(&output, refresh_write, &out, NULL) == SIXEL_OK);
    REQUIRE(sixel_playback_config_init(&config, sizeof(config)) == SIXEL_OK);
    config.capacity = 2;
    config.done = refresh_done;
    config.done_context = &out;
    if (mode == 10) config.palette_mode = SIXEL_PLAYBACK_PALETTE_LATEST;
    REQUIRE(sixel_playback_new(&playback, encoder, output, &config, NULL)
        == SIXEL_OK);
    REQUIRE(sixel_frame_new(&frame, NULL) == SIXEL_OK);
    memset(pixels, 102, sizeof(pixels));
    REQUIRE(sixel_frame_init_borrowed(frame, pixels, 130, 77,
        SIXEL_PIXELFORMAT_RGB888, NULL, 0) == SIXEL_OK);
    if (mode == 8) {
        legacy_size = offsetof(sixel_playback_options_t, refresh_time_us);
        memset(&options, 0xa5, sizeof(options));
        REQUIRE(sixel_playback_options_init(&options, legacy_size)
            == SIXEL_OK);
        REQUIRE(options.refresh_time_us == UINT64_C(0xa5a5a5a5a5a5a5a5));
        options.flags = SIXEL_PLAYBACK_OPTION_REFRESH_TIME;
        REQUIRE(sixel_playback_submit(playback, frame,
            SIXEL_PLAYBACK_WAIT, -1, &options)
            == SIXEL_BAD_ARGUMENT);
        options.flags = 0;
        REQUIRE(sixel_playback_submit(playback, frame,
            SIXEL_PLAYBACK_WAIT, -1, &options) == SIXEL_OK);
        REQUIRE(sixel_playback_drain(playback, -1) == SIXEL_OK);
        ok = 1;
        goto end;
    }
    REQUIRE(sixel_playback_options_init(&options, sizeof(options))
        == SIXEL_OK);
    options.flags = SIXEL_PLAYBACK_OPTION_REFRESH_TIME;
    for (tick = 0U; tick <= 4U; ++tick) {
        timestamp = tick * 16000U;
        options.refresh_time_us = timestamp;
        memset(pixels, tick ? 108 : 102, sizeof(pixels));
        out.size = 0;
        if (mode == 9 && tick == 1U) out.fail = 1;
        status = sixel_playback_submit(playback, frame,
            SIXEL_PLAYBACK_WAIT, -1, &options);
        if (status == SIXEL_OK) status = sixel_playback_drain(playback, -1);
        if (mode == 9 && tick == 1U) {
            REQUIRE(status != SIXEL_OK);
            REQUIRE(!encoder->accumulation_valid);
            REQUIRE(!encoder->residual_state.refresh_valid);
            ok = 1;
            goto end;
        }
        REQUIRE(status == SIXEL_OK);
        expected = paint_counts[tick];
        REQUIRE(out.painted == expected);
        REQUIRE(encoder->residual_state.selected_pixels ==
            (tick ? expected : 0U));
        REQUIRE(encoder->residual_state.refresh_last_us == timestamp);
        changed = 0U;
        for (i = 0; i < 10010U; ++i) {
            /* Check positions as well as counts: a scaled tile has the same
             * histogram but leaves coherent blocks in a uniform fade. */
            rank = sixel_bn64[(((i / 130U) + 17U) & 63U) * 64U +
                              (((i % 130U) + 37U) & 63U)];
            expected = rank < tick * 64U ? 107U : 102U;
            REQUIRE(encoder->accumulation_pixels[i * 3U] == expected);
            if (encoder->accumulation_pixels[i * 3U] != 102) {
                REQUIRE(encoder->accumulation_pixels[i * 3U] == 107);
                ++changed;
            }
        }
        expected = 0U;
        for (i = 1U; i <= tick; ++i) expected += paint_counts[i];
        REQUIRE(changed == expected);
    }
    ok = 1;
end:
    sixel_playback_unref(playback);
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok ? 0 : 1;
}
