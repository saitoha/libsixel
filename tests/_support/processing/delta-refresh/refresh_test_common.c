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

/* Keep assertions local when test support units are amalgamated. */
#define DR_REQUIRE(x) do { if (!(x)) { \
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
    state->refresh_frame = state->refresh_next_frame;
    state->refresh_origin_x = state->refresh_next_origin_x;
    state->refresh_origin_y = state->refresh_next_origin_y;
    state->refresh_valid = state->refresh_next_valid;
}

/* Encode three slightly changing frames with one update policy. */
static int
refresh_encode_sequence(char const *diffuse, char const *gpu,
    char const *policy, refresh_output_t *out)
{
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    unsigned char pixels[64 * 48 * 3];
    int frame_no, x, y, change, value, channel, ok;

    ok = 0;
    encoder = NULL;
    output = NULL;
    frame = NULL;
    if (sixel_encoder_new(&encoder, NULL) != SIXEL_OK ||
            sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_DIFFUSION, diffuse)
                != SIXEL_OK ||
            sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_GPU_POLICY, gpu)
                != SIXEL_OK ||
            sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_UPDATE_POLICY, policy)
                != SIXEL_OK) {
        fprintf(stderr, "refresh dither %s/%s/%s rejected: %s\n", diffuse,
            gpu, policy, sixel_helper_get_additional_message());
        goto end;
    }
    for (frame_no = 0; frame_no < 3; ++frame_no) {
        /* Changes stay within T8 so that only refresh can repaint them. */
        for (y = 0; y < 48; ++y) {
            for (x = 0; x < 64; ++x) {
                change = frame_no ? (x * 7 + y * 13 + frame_no * 5) % 7 - 3
                    : 0;
                for (channel = 0; channel < 3; ++channel) {
                    value = (channel == 0 ? x * 4 : channel == 1 ? y * 5
                        : (x + y) * 2) + (channel == 1 ? -change : change);
                    pixels[(y * 64 + x) * 3 + channel] = (unsigned char)
                        (value < 0 ? 0 : value > 255 ? 255 : value);
                }
            }
        }
        out[frame_no].size = 0;
        if (sixel_output_new(&output, refresh_write, &out[frame_no], NULL)
                != SIXEL_OK ||
                sixel_frame_new(&frame, NULL) != SIXEL_OK ||
                sixel_frame_init_borrowed(frame, pixels, 64, 48,
                    SIXEL_PIXELFORMAT_RGB888, NULL, 0) != SIXEL_OK ||
                sixel_encoder_encode_frame(encoder, frame, output)
                    != SIXEL_OK) {
            fprintf(stderr, "refresh dither %s/%s/%s frame %d: %s\n",
                diffuse, gpu, policy, frame_no,
                sixel_helper_get_additional_message());
            goto end;
        }
        sixel_frame_unref(frame);
        frame = NULL;
        sixel_output_unref(output);
        output = NULL;
    }
    ok = 1;
end:
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok;
}

/* refresh=1 selects every pixel on every frame, so each early-keep gate
 * must then behave exactly as T0. Any application path that ignored the
 * refresh mask would keep T8 approximations and change the bytes. */
int
test_refresh_dither_equivalence(char const *method, char const *gpu)
{
    static refresh_output_t refreshed[3], exact[3];
    int frame_no;

    if (!refresh_encode_sequence(method, gpu,
            "delta:T8:refresh=1", refreshed) ||
            !refresh_encode_sequence(method, gpu, "delta:T0", exact)) {
        return 1;
    }
    for (frame_no = 0; frame_no < 3; ++frame_no) {
        if (refreshed[frame_no].size != exact[frame_no].size ||
                memcmp(refreshed[frame_no].bytes, exact[frame_no].bytes,
                    exact[frame_no].size) != 0) {
            fprintf(stderr, "refresh %s/%s frame %d differs from T0\n",
                method, gpu, frame_no);
            return 1;
        }
    }
    return 0;
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
    uint32_t cycle[4][128];
    unsigned char canary[sizeof(sixel_frame_t *)];
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
        /* A restart without valid history selects nothing; frame zero. */
        sixel_delta_refresh_prepare(&state, 128, 128, 0, 0, 4, 0);
        DR_REQUIRE(state.selected_pixels == 0U);
        DR_REQUIRE(state.refresh_next_frame == 0U);
        refresh_commit(&state);
        if (mode == 2) {
            for (tick = 1U; tick <= 4U; ++tick) {
                sixel_delta_refresh_prepare(&state, 128, 128, 0, 0, 4, 1);
                DR_REQUIRE(state.refresh_next_frame == tick);
                DR_REQUIRE(state.selected_pixels == 4096U);
                for (i = 0; i < 128U; ++i) {
                    DR_REQUIRE((seen[i] & state.mask.cells[i]) == 0U);
                    seen[i] |= state.mask.cells[i];
                }
                refresh_commit(&state);
            }
            for (i = 0; i < 128U; ++i) DR_REQUIRE(seen[i] == UINT32_MAX);
        } else if (mode == 3) {
            /* The second period repeats the first period's phase order. */
            for (tick = 1U; tick <= 8U; ++tick) {
                sixel_delta_refresh_prepare(&state, 128, 128, 0, 0, 4, 1);
                DR_REQUIRE(state.selected_pixels == 4096U);
                if (tick <= 4U)
                    memcpy(cycle[tick - 1U], state.mask.cells,
                        sizeof(cycle[0]));
                else
                    DR_REQUIRE(memcmp(cycle[tick - 5U], state.mask.cells,
                        sizeof(cycle[0])) == 0);
                refresh_commit(&state);
            }
        } else if (mode == 4) {
            /* An uncommitted frame does not advance the count. */
            sixel_delta_refresh_prepare(&state, 128, 128, 0, 0, 4, 1);
            memcpy(cycle[0], state.mask.cells, sizeof(cycle[0]));
            sixel_delta_refresh_prepare(&state, 128, 128, 0, 0, 4, 1);
            DR_REQUIRE(state.refresh_next_frame == 1U);
            DR_REQUIRE(state.selected_pixels == 4096U);
            DR_REQUIRE(memcmp(cycle[0], state.mask.cells,
                sizeof(cycle[0])) == 0);
        } else {
            sixel_delta_refresh_prepare(&state, 128, 128, 0, 0, 4, 1);
            refresh_commit(&state);
            /* Moving the rectangle restarts against valid history. */
            sixel_delta_refresh_prepare(&state, 128, 128, 1, 0, 4, 1);
            DR_REQUIRE(state.refresh_next_frame == 0U);
            DR_REQUIRE(state.selected_pixels == 16384U);
            refresh_commit(&state);
            sixel_delta_refresh_prepare(&state, 128, 128, 1, 0, 4, 1);
            DR_REQUIRE(state.selected_pixels == 4096U);
        }
        ok = 1;
        goto end;
    }
    DR_REQUIRE(sixel_encoder_new(&encoder, NULL) == SIXEL_OK);
    DR_REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_DIFFUSION,
        "bluenoise") == SIXEL_OK);
    DR_REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_LUT_POLICY,
        "none") == SIXEL_OK);
    DR_REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_GPU_POLICY,
        mode == 7 ? "force" : "off") == SIXEL_OK);
    if (mode == 1) {
        DR_REQUIRE(encoder->refresh_frames == 0U);
        DR_REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_UPDATE_POLICY,
            "delta:T8:refresh=4") == SIXEL_OK);
        DR_REQUIRE(encoder->refresh_frames == 4U);
        DR_REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_UPDATE_POLICY,
            "delta:T8:J4") == SIXEL_OK);
        DR_REQUIRE(encoder->refresh_frames == 4U);
        DR_REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_UPDATE_POLICY,
            "delta:T8:J10001") != SIXEL_OK);
        DR_REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_UPDATE_POLICY,
            "delta:T8:J4:R1") != SIXEL_OK);
        /* Refresh applies to every dither, including error diffusion. */
        DR_REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_DIFFUSION,
            "fs") == SIXEL_OK);
        DR_REQUIRE(encoder->refresh_frames == 4U);
        DR_REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_UPDATE_POLICY,
            "delta") == SIXEL_OK);
        DR_REQUIRE(encoder->refresh_frames == 0U);
        ok = 1;
        goto end;
    }
    DR_REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_UPDATE_POLICY,
        "delta:T8:J4") == SIXEL_OK);
    DR_REQUIRE(sixel_output_new(&output, refresh_write, &out, NULL)
        == SIXEL_OK);
    DR_REQUIRE(sixel_playback_config_init(&config, sizeof(config)) == SIXEL_OK);
    config.capacity = 2;
    config.done = refresh_done;
    config.done_context = &out;
    if (mode == 10) config.palette_mode = SIXEL_PLAYBACK_PALETTE_LATEST;
    DR_REQUIRE(sixel_playback_new(&playback, encoder, output, &config, NULL)
        == SIXEL_OK);
    DR_REQUIRE(sixel_frame_new(&frame, NULL) == SIXEL_OK);
    memset(pixels, 102, sizeof(pixels));
    DR_REQUIRE(sixel_frame_init_borrowed(frame, pixels, 130, 77,
        SIXEL_PIXELFORMAT_RGB888, NULL, 0) == SIXEL_OK);
    if (mode == 8) {
        /* The pre-palette-source size leaves the appended field alone. */
        legacy_size = offsetof(sixel_playback_options_t, palette_source);
        memset(&options, 0xa5, sizeof(options));
        memset(canary, 0xa5, sizeof(canary));
        DR_REQUIRE(sixel_playback_options_init(&options, legacy_size)
            == SIXEL_OK);
        DR_REQUIRE(options.struct_size == legacy_size);
        DR_REQUIRE(memcmp(&options.palette_source, canary,
            sizeof(canary)) == 0);
        DR_REQUIRE(sixel_playback_submit(playback, frame,
            SIXEL_PLAYBACK_WAIT, -1, &options) == SIXEL_OK);
        DR_REQUIRE(sixel_playback_drain(playback, -1) == SIXEL_OK);
        ok = 1;
        goto end;
    }
    DR_REQUIRE(sixel_playback_options_init(&options, sizeof(options))
        == SIXEL_OK);
    for (tick = 0U; tick <= 4U; ++tick) {
        memset(pixels, tick ? 108 : 102, sizeof(pixels));
        out.size = 0;
        if (mode == 9 && tick == 1U) out.fail = 1;
        status = sixel_playback_submit(playback, frame,
            SIXEL_PLAYBACK_WAIT, -1, &options);
        if (status == SIXEL_OK) status = sixel_playback_drain(playback, -1);
        if (mode == 9 && tick == 1U) {
            DR_REQUIRE(status != SIXEL_OK);
            DR_REQUIRE(!encoder->accumulation_valid);
            DR_REQUIRE(!encoder->residual_state.refresh_valid);
            ok = 1;
            goto end;
        }
        DR_REQUIRE(status == SIXEL_OK);
        expected = paint_counts[tick];
        DR_REQUIRE(out.painted == expected);
        DR_REQUIRE(encoder->residual_state.selected_pixels ==
            (tick ? expected : 0U));
        DR_REQUIRE(encoder->residual_state.refresh_frame == tick);
        changed = 0U;
        for (i = 0; i < 10010U; ++i) {
            /* Check positions as well as counts: a scaled tile has the same
             * histogram but leaves coherent blocks in a uniform fade. */
            rank = sixel_bn64[(((i / 130U) + 17U) & 63U) * 64U +
                              (((i % 130U) + 37U) & 63U)];
            expected = rank < tick * 64U ? 107U : 102U;
            DR_REQUIRE(encoder->accumulation_pixels[i * 3U] == expected);
            if (encoder->accumulation_pixels[i * 3U] != 102) {
                DR_REQUIRE(encoder->accumulation_pixels[i * 3U] == 107);
                ++changed;
            }
        }
        expected = 0U;
        for (i = 1U; i <= tick; ++i) expected += paint_counts[i];
        DR_REQUIRE(changed == expected);
    }
    ok = 1;
end:
    sixel_playback_unref(playback);
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok ? 0 : 1;
}

#undef DR_REQUIRE
